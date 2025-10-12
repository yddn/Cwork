#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 假设串口读写接口
int Serial_Write(const uint8_t *buf, int len);
int Serial_Read(uint8_t *buf, int maxlen, int timeout_ms);

// MODBUS相关参数
#define MODBUS_ADDR 0x01
#define FUNC_WRITE_BLOCK 0x10
#define FUNC_ACK 0x06
#define FUNC_NACK 0x15
#define FUNC_COMPLETE 0x20

#define BLOCK_SIZE 200
#define FILE_SIZE (100*1024)  // 100KB

// CRC16计算函数（MODBUS标准CRC16）
uint16_t Modbus_CRC16(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }
    return crc;
}

// 组装MODBUS RTU帧
// 帧格式: [Addr][Func][BlockNum_H][BlockNum_L][DataLen_H][DataLen_L][Data...][CRC_L][CRC_H]
int Build_Modbus_Frame(uint8_t *frame, uint16_t block_num, const uint8_t *data, uint16_t data_len)
{
    frame[0] = MODBUS_ADDR;
    frame[1] = FUNC_WRITE_BLOCK;
    frame[2] = (block_num >> 8) & 0xFF;
    frame[3] = block_num & 0xFF;
    frame[4] = (data_len >> 8) & 0xFF;
    frame[5] = data_len & 0xFF;
    memcpy(&frame[6], data, data_len);

    uint16_t crc = Modbus_CRC16(frame, 6 + data_len);
    frame[6 + data_len] = crc & 0xFF;
    frame[7 + data_len] = (crc >> 8) & 0xFF;

    return 8 + data_len; // 总长度
}

// 解析STM32回复帧，判断ACK/NACK/Complete
// 简单假设回复格式: [Addr][Func][BlockNum_H][BlockNum_L][CRC_L][CRC_H]
int Parse_Reply(const uint8_t *buf, int len, uint16_t *block_num, uint8_t *func)
{
    if (len < 6) return -1;
    uint16_t crc_calc = Modbus_CRC16(buf, len - 2);
    uint16_t crc_recv = buf[len - 2] | (buf[len - 1] << 8);
    if (crc_calc != crc_recv) return -2; // CRC错误

    if (buf[0] != MODBUS_ADDR) return -3; // 地址错误

    *func = buf[1];
    *block_num = (buf[2] << 8) | buf[3];
    return 0;
}

// 计算文件整体CRC32（示例用简单算法，实际可用标准库）
uint32_t crc32_table[256];
void Init_CRC32_Table()
{
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = polynomial ^ (c >> 1);
            else c >>= 1;
        }
        crc32_table[i] = c;
    }
}

uint32_t CRC32(const uint8_t *buf, size_t len)
{
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}

// 发送完整性校验帧
// 格式: [Addr][FUNC_COMPLETE][CRC32_H][CRC32_L][CRC32_HH][CRC32_LL][CRC_L][CRC_H]
int Build_Complete_Frame(uint8_t *frame, uint32_t crc32)
{
    frame[0] = MODBUS_ADDR;
    frame[1] = FUNC_COMPLETE;
    frame[2] = (crc32 >> 24) & 0xFF;
    frame[3] = (crc32 >> 16) & 0xFF;
    frame[4] = (crc32 >> 8) & 0xFF;
    frame[5] = crc32 & 0xFF;

    uint16_t crc = Modbus_CRC16(frame, 6);
    frame[6] = crc & 0xFF;
    frame[7] = (crc >> 8) & 0xFF;

    return 8;
}

int main()
{
    FILE *fp = fopen("file.bin", "rb");
    if (!fp) {
        printf("打开文件失败\n");
        return -1;
    }

    // 初始化CRC32表
    Init_CRC32_Table();

    // 读取文件到内存（100KB）
    uint8_t *file_buf = malloc(FILE_SIZE);
    if (!file_buf) {
        printf("内存分配失败\n");
        fclose(fp);
        return -1;
    }
    size_t read_len = fread(file_buf, 1, FILE_SIZE, fp);
    fclose(fp);
    if (read_len != FILE_SIZE) {
        printf("文件读取大小不符\n");
        free(file_buf);
        return -1;
    }

    uint16_t total_blocks = (FILE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint16_t current_block = 0;

    uint8_t send_frame[BLOCK_SIZE + 10];
    uint8_t recv_buf[64];

    while (current_block < total_blocks) {
        uint16_t this_block_size = BLOCK_SIZE;
        if (current_block == total_blocks - 1) {
            this_block_size = FILE_SIZE - current_block * BLOCK_SIZE;
        }

        int frame_len = Build_Modbus_Frame(send_frame, current_block, &file_buf[current_block * BLOCK_SIZE], this_block_size);

        // 发送数据块
        if (Serial_Write(send_frame, frame_len) != frame_len) {
            printf("发送失败\n");
            free(file_buf);
            return -1;
        }

        // 等待回复，超时重发
        int retry = 0;
        int max_retry = 5;
        int recv_len;
        uint16_t reply_block;
        uint8_t reply_func;
        while (retry < max_retry) {
            recv_len = Serial_Read(recv_buf, sizeof(recv_buf), 1000); // 1秒超时
            if (recv_len > 0) {
                int ret = Parse_Reply(recv_buf, recv_len, &reply_block, &reply_func);
                if (ret == 0 && reply_block == current_block) {
                    if (reply_func == FUNC_ACK) {
                        // 块传输成功，发送下一块
                        current_block++;
                        break;
                    } else if (reply_func == FUNC_NACK) {
                        // 重发当前块
                        printf("块 %d 校验失败，重发\n", current_block);
                        Serial_Write(send_frame, frame_len);
                        retry++;
                    } else {
                        // 其他回复忽略
                    }
                }
            } else {
                // 超时，重发
                printf("块 %d 回复超时，重发\n", current_block);
                Serial_Write(send_frame, frame_len);
                retry++;
            }
        }

        if (retry >= max_retry) {
            printf("块 %d 多次重发失败，终止传输\n", current_block);
            free(file_buf);
            return -1;
        }
    }

    // 发送完整性校验帧
    uint32_t file_crc = CRC32(file_buf, FILE_SIZE);
    int complete_len = Build_Complete_Frame(send_frame, file_crc);
    Serial_Write(send_frame, complete_len);

    // 等待STM32确认完整性
    int retry = 0;
    while (retry < 5) {
        int recv_len = Serial_Read(recv_buf, sizeof(recv_buf), 2000);
        if (recv_len > 0) {
            uint16_t reply_block;
            uint8_t reply_func;
            int ret = Parse_Reply(recv_buf, recv_len, &reply_block, &reply_func);
            if (ret == 0 && reply_func == FUNC_COMPLETE) {
                printf("文件传输完成且校验通过\n");
                break;
            } else if (ret == 0 && reply_func == FUNC_NACK) {
                printf("文件校验失败，重新发送完整性校验帧\n");
                Serial_Write(send_frame, complete_len);
                retry++;
            }
        } else {
            printf("等待完整性确认超时，重发完整性校验帧\n");
            Serial_Write(send_frame, complete_len);
            retry++;
        }
    }

    if (retry >= 5) {
        printf("完整性校验失败，传输终止\n");
        free(file_buf);
        return -1;
    }

    free(file_buf);
    return 0;
}


//------STM32 RCV----------------
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// 假设串口收发接口
int Serial_Write(const uint8_t *buf, int len);
int Serial_Read(uint8_t *buf, int maxlen, int timeout_ms);

// MODBUS相关定义
#define MODBUS_ADDR 0x01
#define FUNC_WRITE_BLOCK 0x10
#define FUNC_ACK 0x06
#define FUNC_NACK 0x15
#define FUNC_COMPLETE 0x20

#define BLOCK_SIZE 200
#define FILE_SIZE (100*1024)  // 100KB
#define TOTAL_BLOCKS ((FILE_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE)

// CRC16计算函数（MODBUS标准CRC16）
uint16_t Modbus_CRC16(const uint8_t *buf, int len)
{
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < len; pos++) {
        crc ^= (uint16_t)buf[pos];
        for (int i = 8; i != 0; i--) {
            if ((crc & 0x0001) != 0) {
                crc >>= 1;
                crc ^= 0xA001;
            }
            else
                crc >>= 1;
        }
    }
    return crc;
}

// CRC32计算（与上位机一致）
uint32_t crc32_table[256];
void Init_CRC32_Table()
{
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = polynomial ^ (c >> 1);
            else c >>= 1;
        }
        crc32_table[i] = c;
    }
}

uint32_t CRC32(const uint8_t *buf, size_t len)
{
    uint32_t c = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        c = crc32_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}

// 发送回复帧
// 格式: [Addr][Func][BlockNum_H][BlockNum_L][CRC_L][CRC_H]
void Send_Reply(uint8_t func, uint16_t block_num)
{
    uint8_t buf[6];
    buf[0] = MODBUS_ADDR;
    buf[1] = func;
    buf[2] = (block_num >> 8) & 0xFF;
    buf[3] = block_num & 0xFF;
    uint16_t crc = Modbus_CRC16(buf, 4);
    buf[4] = crc & 0xFF;
    buf[5] = (crc >> 8) & 0xFF;
    Serial_Write(buf, 6);
}

// 发送完整性确认帧
// 格式: [Addr][FUNC_COMPLETE][CRC32_H][CRC32_L][CRC32_HH][CRC32_LL][CRC_L][CRC_H]
void Send_Complete_Reply()
{
    uint8_t buf[6];
    buf[0] = MODBUS_ADDR;
    buf[1] = FUNC_COMPLETE;
    // 这里不带数据，只表示完成
    uint16_t crc = Modbus_CRC16(buf, 2);
    buf[2] = crc & 0xFF;
    buf[3] = (crc >> 8) & 0xFF;
    Serial_Write(buf, 4);
}

// 文件缓存
uint8_t file_buf[FILE_SIZE];
bool block_received[TOTAL_BLOCKS];

// 处理接收到的数据块
void Process_Data_Block(uint16_t block_num, const uint8_t *data, uint16_t data_len)
{
    if (block_num >= TOTAL_BLOCKS) {
        // 块号错误，回复NACK
        Send_Reply(FUNC_NACK, block_num);
        return;
    }
    // 校验数据长度合理
    uint16_t expected_len = BLOCK_SIZE;
    if (block_num == TOTAL_BLOCKS - 1) {
        expected_len = FILE_SIZE - block_num * BLOCK_SIZE;
    }
    if (data_len != expected_len) {
        Send_Reply(FUNC_NACK, block_num);
        return;
    }

    // 这里可以增加数据块CRC校验（上位机已用MODBUS CRC16校验帧头和数据）

    // 写入缓存
    memcpy(&file_buf[block_num * BLOCK_SIZE], data, data_len);
    block_received[block_num] = true;

    // 回复ACK
    Send_Reply(FUNC_ACK, block_num);
}

// 处理完整性校验请求
void Process_Complete_Frame(const uint8_t *data, uint16_t data_len)
{
    if (data_len != 4) {
        // 长度错误，回复NACK
        Send_Reply(FUNC_NACK, 0);
        return;
    }
    // 解析上位机发送的文件CRC32
    uint32_t file_crc = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];

    // 检查是否所有块都已接收
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (!block_received[i]) {
            // 有块未接收，回复NACK
            Send_Reply(FUNC_NACK, 0);
            return;
        }
    }

    // 计算本地文件CRC32
    uint32_t local_crc = CRC32(file_buf, FILE_SIZE);

    if (local_crc == file_crc) {
        // 校验通过，回复完成
        Send_Reply(FUNC_COMPLETE, 0);
    } else {
        // 校验失败，回复NACK
        Send_Reply(FUNC_NACK, 0);
    }
}

// 解析接收到的MODBUS帧
void Parse_Modbus_Frame(const uint8_t *buf, int len)
{
    if (len < 8) return; // 最小帧长

    // 校验地址
    if (buf[0] != MODBUS_ADDR) return;

    // 校验CRC16
    uint16_t crc_calc = Modbus_CRC16(buf, len - 2);
    uint16_t crc_recv = buf[len - 2] | (buf[len - 1] << 8);
    if (crc_calc != crc_recv) return;

    uint8_t func = buf[1];

    if (func == FUNC_WRITE_BLOCK) {
        // 解析块号和数据长度
        uint16_t block_num = (buf[2] << 8) | buf[3];
        uint16_t data_len = (buf[4] << 8) | buf[5];
        if (len != 8 + data_len) return; // 长度不符

        const uint8_t *data = &buf[6];
        Process_Data_Block(block_num, data, data_len);
    }
    else if (func == FUNC_COMPLETE) {
        // 完整性校验帧，数据长度应为4
        uint16_t data_len = len - 6; // 除去地址、功能码、CRC
        const uint8_t *data = &buf[2];
        Process_Complete_Frame(data, data_len);
    }
    // 其他功能码可扩展
}

// 主循环示例，接收串口数据并解析
void Main_Loop()
{
    uint8_t rx_buf[512];
    int rx_len = 0;

    // 初始化接收状态
    memset(block_received, 0, sizeof(block_received));
    Init_CRC32_Table();

    while (1) {
        // 这里假设串口接收函数阻塞或带超时
        int len = Serial_Read(rx_buf, sizeof(rx_buf), 1000);
        if (len > 0) {
            // 解析MODBUS帧
            Parse_Modbus_Frame(rx_buf, len);
        }
        // 其他任务处理...
    }
}
