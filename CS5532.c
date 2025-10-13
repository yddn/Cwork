 #include <stdint.h>
#include <stdbool.h>

#define SYNC0              0xFE  // 串行口初始化序列结束命令
#define SYNC1              0xFF  // 串行口初始化序列命令之一
#define NULL_BYTE          0x00  // 清除端口标志位，保持连续转换模式

// 寄存器地址
#define REG_OFFSET         0x01  // 偏移寄存器
#define REG_GAIN           0x02  // 增益寄存器
#define REG_CONFIG         0x03  // 配置寄存器

// 偏移寄存器命令
#define WRITE_OFFSET       (CMD_WRITE + REG_OFFSET)
#define READ_OFFSET        (CMD_READ + REG_OFFSET)

// 增益寄存器命令
#define WRITE_GAIN         (CMD_WRITE + REG_GAIN)
#define READ_GAIN          (CMD_READ + REG_GAIN)

// 配置寄存器命令
#define WRITE_CONFIG       (CMD_WRITE + REG_CONFIG)
#define READ_CONFIG        (CMD_READ + REG_CONFIG)

// 转换命令
#define START_SINGLE       0x80  // 单次转换
#define START_CONTINUOUS   0xC0  // 连续转换

#define NORMAL_CONVERSION  (0x80 + 0x00)
#define SYSTEM_OFFSET_CAL  (0x80 + 0x05)
#define SYSTEM_GAIN_CAL    (0x80 + 0x06)

// 配置寄存器位定义（示例）
#define CR_STANDBY_MODE    (0x00UL << 31)
#define CR_SLEEP_MODE      (0x01UL << 31)
#define CR_POWER_SAVE      (0x01UL << 30)
#define CR_SYSTEM_RESET    (0x01UL << 29)
// ... 其他配置位定义
// CS5532命令定义（根据手册调整）
#define CMD_RESET               0x06
#define CMD_SYNC                0xFC
#define CMD_START_SINGLE        0x08
#define CMD_START_CONT          0xC0
#define CMD_STOP_CONT           0x0A
#define CMD_READ_DATA           0x12
#define CMD_WRITE_CONFIG        0x40  // 写配置寄存器起始地址（示例）
#define CMD_OFFSET_CAL          0x1A  // 零点校准
#define CMD_GAIN_CAL            0x16  // 增益校准

// CS5532寄存器地址定义
#define CS5532_REG_STATUS      0x00
#define CS5532_REG_CONFIG      0x01
#define CS5532_REG_DATA        0x02
#define CS5532_REG_OFFSET_CAL  0x04
#define CS5532_REG_GAIN_CAL    0x05
#define CS5532_REG_CALIBRATION 0x06


// SPI命令格式（高3位为命令，低5位为寄存器地址）
#define CMD_READ  0x20  // 001xxxxx
#define CMD_WRITE 0x40  // 010xxxxx

 prsv->wtv.csgain[0] = 0x0045c1a6;   //预置校准值
 prsv->wtv.csgain[1] = 0x0045c1a6;
 prsv->wtv.csoffSet[0] = 0x06df7700;
 prsv->wtv.csoffSet[1] = 0x06df7700;

 void SPI_SelectCS(uint8_t cs_index)
  {
    // 先全部拉高，取消所有片选
    HAL_GPIO_WritePin(SCS1_Port, SCS1, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SCS2_Port, SCS2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MAX1_GPIO_Port, MAX1_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(MAX2_GPIO_Port, MAX2_CS_Pin, GPIO_PIN_SET);

    // 根据传入的cs_index拉低对应片选
    switch(cs_index)
    {
      case 0:
        HAL_GPIO_WritePin(SCS1_Port, SCS1, GPIO_PIN_RESET);
        break;
      case 1:
        HAL_GPIO_WritePin(SCS2_Port, SCS2, GPIO_PIN_RESET);
        break;
      case 2:
        HAL_GPIO_WritePin(MAX1_GPIO_Port, MAX1_CS_Pin, GPIO_PIN_RESET);
        break;
      case 3:
        HAL_GPIO_WritePin(MAX2_GPIO_Port, MAX2_CS_Pin, GPIO_PIN_RESET);
        break;
    }
  }

  // SPI发送并接收一个字节，返回接收到的数据
  static uint8_t SPI1_TransmitReceiveByte(uint8_t i, uint8_t txData)
  {
    SPI_SelectCS(i);
    uint8_t rxData = 0;
      // HAL_SPI_TransmitReceive阻塞模式，超时时间10ms
    if(HAL_SPI_TransmitReceive(&hspi1, &txData, &rxData, 1, 10) != HAL_OK)
        rxData = 0xFF;
    SPI_SelectCS(5);    //取消片选 
    return rxData;
    
  }

  // CS5532写寄存器函数
  // addr: 寄存器地址，data: 指向写入数据缓冲区，len: 数据长度（字节）
  void CS5532_WriteRegister(uint8_t i ,uint8_t addr, uint8_t *data, uint8_t len)
  {
    // 发送写命令，CS5532写寄存器命令格式一般是：0x40 | addr
    // 具体命令格式请参考CS5532手册，这里假设是0x40+addr
    uint8_t cmd = 0x40 | (addr & 0x3F);
    SPI1_TransmitReceiveByte(i,cmd);

    for(uint8_t idx  = 0; idx  < len; idx ++)
    {
        SPI1_TransmitReceiveByte(i,data[idx]);
    }

  }
  // addr: 寄存器地址，data: 指向接收缓冲区，len: 读取字节数
  void CS5532_ReadRegister(uint8_t i ,uint8_t addr, uint8_t *data, uint8_t len)
  {
      SPI_SelectCS(i);

      // 发送读命令，CS5532读寄存器命令格式一般是：0x00 | addr
      // 具体命令格式请参考CS5532手册，这里假设是0x00+addr
      uint8_t cmd = addr & 0x3F;
      SPI1_TransmitReceiveByte(i,cmd);

      // 读取数据，发送0xFF作为占位
      for(uint8_t idx  = 0; idx  < len; idx ++)
      {
          data[idx ] = SPI1_TransmitReceiveByte(i,0xFF);
      }

      SPI_SelectCS(5);
  }

  void CS5532_WriteData24(uint8_t i, uint8_t addr, uint32_t val)
  {
      uint8_t buf[3];
      buf[0] = (val >> 16) & 0xFF;
      buf[1] = (val >> 8) & 0xFF;
      buf[2] = val & 0xFF;
      CS5532_WriteRegister(i, addr, buf, 3);
  }
  

  // 写配置寄存器示例，配置采样率、滤波器、增益等
  void CS5532_Config(uint8_t i)   //上电直接配置参数
    {   //7:CH_SEL = 0 AIN1
      // 配置寄存器数据示例，3字节，具体根据CS5532手册调整
      SPI1_TransmitReceiveByte(i, CMD_READ_DATA);
      //uint8_t config_d = 0x54;  // 这里填入实际配置值

      //CS5532_WriteRegister(i,CS5532_REG_CONFIG, &config_d, 1); 
      uint8_t config_d[3] = {0x00, 0x00, 0x00}; 
      CS5532_WriteRegister(i,CS5532_REG_CONFIG, config_d, 3); 
      HAL_Delay(10);
  }
  
  void CS5532_Sync(uint8_t i)              // 2.同步命令
  {
    SPI1_TransmitReceiveByte(i,CMD_SYNC);
    HAL_Delay(1);
  }

  void CS5532_Init(void)       // 1.系统复位-上电一次即可 + 同步
  {
    for(uint8_t i = 0;i < SYS_HXT;i++)
    {  
      SPI1_TransmitReceiveByte(i,CMD_RESET);
    }
    HAL_Delay(10);                        // 等待复位完成
    for(uint8_t i = 0; i < SYS_HXT; i++)
    {
        CS5532_Sync(i);
        CS5532_Config(i);
        CS5532_WriteData24(i, CS5532_REG_OFFSET_CAL, prsv->wtv.csoffSet[i]);
        CS5532_WriteData24(i, CS5532_REG_GAIN_CAL, prsv->wtv.csgain[i]);
        SPI1_TransmitReceiveByte(i,CMD_START_CONT);    //连续转换
    }    
  }
  
  void CS5532_OffsetCalibration(uint8_t i)  //执行零点校准 -串口手动配置
  {
      SPI1_TransmitReceiveByte(i,CMD_OFFSET_CAL); 
      // 等待校准完成，时间根据芯片手册，一般10~20ms
      HAL_Delay(20);
  }

  void Get_Weight(uint8_t iHx)		//称读取重量
  {
//      uint8_t buf[3] = {0};
//      CS5532_ReadRegister(iHx, CS5532_REG_DATA, buf, 3);
//      // CS5532数据高字节在前，拼接成32位无符号数
//      uint32_t val = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
     uint8_t buf[3] = {0};
    SPI_SelectCS(iHx);
    SPI1_TransmitReceiveByte(iHx, CMD_READ_DATA); // 发送读数据命令
    // 连续读取3字节数据
    buf[0] = SPI1_TransmitReceiveByte(iHx, 0xFF);
    buf[1] = SPI1_TransmitReceiveByte(iHx, 0xFF);
    buf[2] = SPI1_TransmitReceiveByte(iHx, 0xFF);
    //SPI1_TransmitReceiveByte(iHx,CMD_SYNC);
     
    SPI_SelectCS(5); // 释放片选
    uint32_t val = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];  
    
    printf("CS5532[%d] Data: %u\r\n", iHx, val);
      //return val;
  }
