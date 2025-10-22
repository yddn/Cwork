
#include "main.h"
#include <stdio.h>
#include <string.h>
extern SPI_HandleTypeDef hspi1;
unsigned char cmd[8];
unsigned char ReceiveData;
uint8_t tmc_spi_rv = 0;
// 写32位寄存器值
// 从 TMC 5160 读取数据或向 TMC 5160 写入数据时，返回高位八位包含 SPI
// 状态，SPI_STATUS， 对应八个选定的状态位。
/*
  如果前一次访问是读访问，则传输回来的数据是前一个数据报一起传输的地址对应的读取值。
  如果前一次访问是写访问，则数据回读为以前接收的镜像写数据。
  因此，读访问和写访问之间的区别在于，读访问不将数据传输到寻址寄存器，
  仅传输地址，因此它的 32
  个数据位是虚拟的，而且，随后的读访问或写访问将发回前一个读周期中传输
  的地址对应的数据。
  每当从 TMC 5160 读取数据或向 TMC 5160 写入数据时，返回高位八位包含 SPI
  状态，SPI_STATUS， 对应八个选定的状态位。
动作                    发送给 TMC5160 的数据        TMC5160 返回的数据
读 XACTUAL              ->  0x2100000000            <- 0xSS & unused data 
读 XACTUAL              ->  0x2100000000            <- 0xSS & XACTUAL 
写 VMAX:= 0x00ABCDEF    ->  0xA700ABCDEF            <- 0xSS & XACTUAL 
写 VMAX:= 0x00123456    ->  0xA700123456            <- 0xSS 00ABCDEF

7 status_stop_r RAMP_STAT[1] – 1:       仅限运动控制器模式下，右参考开关停止标志
6 status_stop_l RAMP_STAT[0] – 1:       仅限运动控制器模式下，左参考开关停止标志
5 position_reached RAMP_STAT[9] – 1:    仅限运动控制器模式下，目标位置到达标志 
4 velocity_reached RAMP_STAT[8] – 1:    仅限运动控制器模式下，目标速度到达标志
3 standstill DRV_STATUS[31] – 1:        电机静止标志
2 sg2 DRV_STATUS[24] – 1:               stallGuard 标志有效标志
1 driver_error GSTAT[1] – 1:            驱动器错误指示 (通过 读 GSTAT 清标志) 
0 reset_flag GSTAT[0] – 1:              复位发生指示(通过 读 GSTAT 清标志)

*/

// SPI1发送一个字节，返回接收到的字节
uint8_t SPI1_ReadWriteByte(uint8_t TxData) {
  uint8_t RxData = 0;
  // 发送并接收一个字节，阻塞模式，超时时间设为10ms
  if (HAL_SPI_TransmitReceive(&hspi1, &TxData, &RxData, 1, 100) != HAL_OK) {
    RxData = 0xFF; // 返回错误标志
    printf("SPI ERR\r\n");
  }
  // else printf("spi ok\r\n");
  return RxData;
}

void TMC_WriteData(uint8_t address, uint32_t datagram) {
  TMC_S;
  tmc_spi_rv =  SPI1_ReadWriteByte(address | 0x80); // address|0x80
  if(tmc_spi_rv != 0xFF)  {
    SPI1_ReadWriteByte((datagram >> 24) & 0xff);
    SPI1_ReadWriteByte((datagram >> 16) & 0xff);
    SPI1_ReadWriteByte((datagram >> 8) & 0xff);
    SPI1_ReadWriteByte(datagram & 0xff);
  }
  TMC_U; // SPI_CS片选拉高
}

// 读32位寄存器值
uint32_t TMC_ReadData(uint8_t address) {
  uint8_t rxdata[4] = {0};
  uint32_t datagram = 0;

  TMC_S;

  tmc_spi_rv = SPI1_ReadWriteByte(address & 0x7F);
  rxdata[0] =  SPI1_ReadWriteByte(0x00); 
  rxdata[1] =  SPI1_ReadWriteByte(0x00); 
  rxdata[2] =  SPI1_ReadWriteByte(0x00); 
  rxdata[3] =  SPI1_ReadWriteByte(0x00);

  TMC_U; // SPI_CS片选拉高

  TMC_S; // SPI_CS片选拉低

  tmc_spi_rv = SPI1_ReadWriteByte(address & 0x7F);
  rxdata[0] =  SPI1_ReadWriteByte(0x00); 
  rxdata[1] =  SPI1_ReadWriteByte(0x00); 
  rxdata[2] =  SPI1_ReadWriteByte(0x00); 
  rxdata[3] =  SPI1_ReadWriteByte(0x00);

  datagram =
      (rxdata[0] << 24) | (rxdata[1] << 16) | (rxdata[2] << 8) | rxdata[3];

  TMC_U; // SPI_CS片选拉高

  return datagram;
}

/*
coolStep 是一种基于电机机械负载的步进电机智能调节能耗，使其“绿色
SEMIN   阈值下限。4 位无符号整数。如果 SG 低于此阈值，
        coolStep 会增加两个线圈的电流。4 位 SEMIN 乘以  =0 禁用 coolStep
        32，对应 10 位 SG 值范围的下半部分。           1...15 阈值为 SEMIN*32
SEMAX   阈值上限，4 位无符号整数。如果 SG 被采样到等于
        或高于该阈值，coolStep 将降低到两个线圈的电流。 上限为( SEMIN + SEMAX + 1 ) * 32

coolStep 根据负载调节电机电流     五个参数控制 coolStep，返回一个状态值

SEUP  SEDN SEIMIN  TCOOLTHRS THIGH  状态字: CSACTUAL

dcStep 只需要几个设置。它直接将电机运动反馈给斜坡发生器，从而即使电机相对于目标速度过载，
它也能无缝集成到运动斜坡中。dcStep 在斜坡发生器目标速度 VACTUAL 下以全步模式运行电机，如果电
机过载，则以降低的速度运行电机。它需要设置最小操作速度 VDCMIN。VDCMIN 应为 dcStep 可对电机
运行进行可靠检测的最低运行速度。除非制动到低于 VDCMIN 的速度，否则电机不会堵转。如果速度低
于这个值，一旦负载被释放，电机将重新启动，除非堵转检测被启用(设置 sg_stop)。stallguard 2 实现堵
转检测功能。


设置 GCONF 标志 stop_enable 使能此
选项。每当 ENCA_DCIN 被拉高，电机将根据设置的静止 IHOLD、IHOLDDELAY 和 stealthChop 进入电流降
低状态。ENN 禁用驱动器将需要三个时钟周期来安全地关闭驱动器。
*/

void TMC5160_RegInit(uint32_t mode) {
  TMC_WriteData(0x00, 0x00000004); // GCONF:
  // sendData(0x00,0x0000000C);//GCONF://静音打开
  TMC_WriteData(0x09, 0x00010606); //
  TMC_WriteData(0x0A, 0x00080400); //
  TMC_WriteData(0x10, 0x00020503); // IHOLD_IRUN: IHOLD=3, IRUN=9 (max.current),
                                   // IHOLDDELAY=60x00071C01
  TMC_WriteData(0x11, 0x0000000A); //
                                   // sendData(0x35,0x00001080);//
  TMC_WriteData(
      0x6C,
      0x04410153); // CHOPCONF: TOFF=3, HSTRT=5, HEND=2, TBL=2, CHM=0,TPFD=4

  switch (mode) {
  case 0: // Position mode>>

    TMC_WriteData(0x20, 0x00000000);
    // sendData(0x24,0x0000FFFF);//A1:65535
    TMC_WriteData(0x25, 0);      // V1:1000000
    TMC_WriteData(0x26, 20000);  // AMAX:65535
    TMC_WriteData(0x27, 200000); // VMAX:1000000
    TMC_WriteData(0x28, 20000);  // DMAX:65535
    TMC_WriteData(0x2A, 1);      // D1:65535
    TMC_WriteData(0x2B, 500);    // VSTOP=10

    TMC_WriteData(0x2D, 0); // XTARGET=101200
    TMC_WriteData(0x21, 0); // XTARGET=101200
    break;
  case 1:                            // Velocity mode>>
    TMC_WriteData(0x24, 20000);      // A1:100
    TMC_WriteData(0x25, 0);          // V1:500000
    TMC_WriteData(0x26, 100000);     // AMAX:110
    TMC_WriteData(0x27, 200000);     // VMAX
    TMC_WriteData(0x28, 100000);     // DMAX:65535
    TMC_WriteData(0x20, 0x00000001); // RAMPMODE=1
    TMC_WriteData(0x2A, 1);          // D1:65535
    TMC_WriteData(0x13, 0xFFFFFFFF); // TPWMTHRS:96
    break;
  }

  //	delay_us(10);
  HAL_Delay(5);
  //	ENABLE_CONTROL();  //DRV_ENN拉低使能
  TMC_WriteData(0x2D, -200000); // 收回指令
  // wait_pos_ok();
  TMC_WriteData(0x2D, 0); // XTARGET=101200
  TMC_WriteData(0x21, 0); // XTARGET=101200

  uint32_t gcon = TMC_ReadData(0x00);
  printf("GConf_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x09);
  printf("New_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x0A);
  printf("0x0A_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x00);
  printf("GConf_STATUS = 0x%08X\n", gcon);
  gcon = TMC_ReadData(0x6C);
  printf("0x6C = 0x%08X\n", gcon);
}

// 周期调用检测复位并初始化
void TMC5160_PeriodicCheck(void) {
  
    
    uint32_t gstat = TMC_ReadData(TMC5160_REG_GSTAT);
    if (gstat & 0x01)         TMC5160_RegInit(1);
    
}

// 解析所有报警状态
void TMC5160_ParseDRVStatus(uint32_t drv_status_reg,
                            TMC5160_DRV_STATUS_t *status) {
  status->reset = (drv_status_reg & (1U << 0)) != 0;
  status->drv_err = (drv_status_reg & (1U << 1)) != 0;
  status->uv_cp = (drv_status_reg & (1U << 2)) != 0;
  status->stallguard = (drv_status_reg & (1U << 3)) != 0;
  status->ot = (drv_status_reg & (1U << 4)) != 0;
  status->otpw = (drv_status_reg & (1U << 5)) != 0;
  status->s2ga = (drv_status_reg & (1U << 6)) != 0;
  status->s2gb = (drv_status_reg & (1U << 7)) != 0;
  status->s2vsa = (drv_status_reg & (1U << 8)) != 0;
  status->s2vsb = (drv_status_reg & (1U << 9)) != 0;
  status->otsd = (drv_status_reg & (1U << 10)) != 0;

  // 9位实际电流值，bit11-19
  uint16_t cs_low = (drv_status_reg >> 11) & 0x1F;  // 5位低位
  uint16_t cs_high = (drv_status_reg >> 16) & 0x0F; // 4位高位
  status->cs_actual = (cs_high << 5) | cs_low;

  // 8位堵转检测值，bit20-27
  status->stallguard_val = (drv_status_reg >> 20) & 0xFF;
}

// 根据状态位返回报警等级
static TMC_AlarmLevel_t TMC_GetAlarmLevel(TMC_AlarmType_t alarm) {
  switch (alarm) {
  case TMC_ALARM_RESET:
    return TMC_ALARM_WARNING;
  case TMC_ALARM_DRV_ERR:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_UV_CP:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_STALLGUARD:
    return TMC_ALARM_WARNING;
  case TMC_ALARM_OT:
    return TMC_ALARM_WARNING;
  case TMC_ALARM_OTPW:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_OTSD:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_S2GA:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_S2GB:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_S2VSA:
    return TMC_ALARM_CRITICAL;
  case TMC_ALARM_S2VSB:
    return TMC_ALARM_CRITICAL;
  default:
    return TMC_ALARM_WARNING;
  }
}

// 4.报警检测函数，支持多报警
void TMC_CheckAlarms(const TMC5160_DRV_STATUS_t *status,
                     TMC_AlarmStatus_t *alarms) {

  memset(alarms, 0, sizeof(TMC_AlarmStatus_t)); // 清空之前报警

  // 检测各报警
  if (status->reset) {
    alarms->active[TMC_ALARM_RESET] = true;
    alarms->level[TMC_ALARM_RESET] = TMC_GetAlarmLevel(TMC_ALARM_RESET);
  }
  if (status->drv_err) {
    alarms->active[TMC_ALARM_DRV_ERR] = true;
    alarms->level[TMC_ALARM_DRV_ERR] = TMC_GetAlarmLevel(TMC_ALARM_DRV_ERR);
  }
  if (status->uv_cp) {
    alarms->active[TMC_ALARM_UV_CP] = true;
    alarms->level[TMC_ALARM_UV_CP] = TMC_GetAlarmLevel(TMC_ALARM_UV_CP);
  }
  if (status->stallguard) {
    alarms->active[TMC_ALARM_STALLGUARD] = true;
    alarms->level[TMC_ALARM_STALLGUARD] =
        TMC_GetAlarmLevel(TMC_ALARM_STALLGUARD);
  }
  if (status->ot) {
    alarms->active[TMC_ALARM_OT] = true;
    alarms->level[TMC_ALARM_OT] = TMC_GetAlarmLevel(TMC_ALARM_OT);
  }
  if (status->otpw) {
    alarms->active[TMC_ALARM_OTPW] = true;
    alarms->level[TMC_ALARM_OTPW] = TMC_GetAlarmLevel(TMC_ALARM_OTPW);
  }
  if (status->otsd) {
    alarms->active[TMC_ALARM_OTSD] = true;
    alarms->level[TMC_ALARM_OTSD] = TMC_GetAlarmLevel(TMC_ALARM_OTSD);
  }
  if (status->s2ga) {
    alarms->active[TMC_ALARM_S2GA] = true;
    alarms->level[TMC_ALARM_S2GA] = TMC_GetAlarmLevel(TMC_ALARM_S2GA);
  }
  if (status->s2gb) {
    alarms->active[TMC_ALARM_S2GB] = true;
    alarms->level[TMC_ALARM_S2GB] = TMC_GetAlarmLevel(TMC_ALARM_S2GB);
  }
  if (status->s2vsa) {
    alarms->active[TMC_ALARM_S2VSA] = true;
    alarms->level[TMC_ALARM_S2VSA] = TMC_GetAlarmLevel(TMC_ALARM_S2VSA);
  }
  if (status->s2vsb) {
    alarms->active[TMC_ALARM_S2VSB] = true;
    alarms->level[TMC_ALARM_S2VSB] = TMC_GetAlarmLevel(TMC_ALARM_S2VSB);
  }
}

void TMC_HandleAlarms(const TMC_AlarmStatus_t *alarms) {
  for (int i = 0; i < TMC_ALARM_MAX; i++) {
    if (alarms->active[i]) {
      switch (i) {
      case TMC_ALARM_RESET:
        printf("Warning: Chip reset detected\n");
        break;
      case TMC_ALARM_DRV_ERR:
        printf("Critical: Driver error detected\n");
        break;
      case TMC_ALARM_UV_CP:
        printf("Critical: Undervoltage protection triggered\n");
        break;
      case TMC_ALARM_STALLGUARD:
        printf("Warning: Stall guard triggered\n");
        break;
      case TMC_ALARM_OT:
        printf("Warning: Overtemperature warning\n");
        break;
      case TMC_ALARM_OTPW:
        printf("Critical: Overtemperature shutdown\n");
        break;
      case TMC_ALARM_OTSD:
        printf("Critical: Overtemperature shutdown state\n");
        break;
      case TMC_ALARM_S2GA:
        printf("Critical: Overcurrent protection A triggered\n");
        break;
      case TMC_ALARM_S2GB:
        printf("Critical: Overcurrent protection B triggered\n");
        break;
      case TMC_ALARM_S2VSA:
        printf("Critical: Overvoltage protection A triggered\n");
        break;
      case TMC_ALARM_S2VSB:
        printf("Critical: Overvoltage protection B triggered\n");
        break;
      default:
        printf("Unknown alarm detected:%d\n", i);
        break;
      }
    }
  }
}
// 用户回调接口设计
typedef void (*TMC_AlarmCallback_t)(TMC_AlarmType_t alarm,
                                    TMC_AlarmLevel_t level);

static TMC_AlarmCallback_t user_alarm_callback = NULL;

void TMC_RegisterAlarmCallback(TMC_AlarmCallback_t cb) {
  user_alarm_callback = cb;
}

void TMC_InvokeAlarmCallback(const TMC_AlarmStatus_t *alarms) {
  if (user_alarm_callback == NULL)
    return;

  for (int i = 0; i < TMC_ALARM_MAX; i++) {
    if (alarms->active[i]) {
      user_alarm_callback((TMC_AlarmType_t)i, alarms->level[i]);
    }
  }
}

void TMC_ProcessStatus(uint32_t drv_status_reg) {
  TMC5160_DRV_STATUS_t status;
  TMC_AlarmStatus_t alarms;

  TMC5160_ParseDRVStatus(drv_status_reg, &status);      // 解析寄存器

  TMC_CheckAlarms(&status, &alarms);            // 检测报警

  TMC_HandleAlarms(&alarms);                         // 处理报警（打印、控制等）

  TMC_InvokeAlarmCallback(&alarms);                  // 调用用户回调（如果注册了）

  for (int i = 0; i < TMC_ALARM_MAX; i++) {                 // 根据严重报警决定是否停止电机等
    if (alarms.active[i] && alarms.level[i] == TMC_ALARM_CRITICAL) {
      // 这里可以调用停止电机函数
      // Motor_Stop();
      break;
    }
  }
}


/*

void TMC_init(void) {
  TMC_WriteData(0x6C, 0x000100C3); // CHOPCONF: TOFF=3, HSTRT=4, HEND=1, TBL=2,
                                   // CHM=0 (spreadCycle)
  TMC_WriteData(
      0x10,
      0x00061F0A); // IHOLD_IRUN: IHOLD=10, IRUN=31 (最大电流 ), IHOLDDELAY=6
  TMC_WriteData(0x11, 0x0000000A); // TPOWERDOWN=10:
                                   // 电机静止到电流减小之间的延时
  TMC_WriteData(
      0x00, 0x00000004); // EN_PWM_MODE=1 enables stealthChop (缺省 PWM_CONF 值)
  TMC_WriteData(0x13,
                0x000001F4); // TPWM_THRS=500 对应切换速度 35000 = ca. 30RPM

  TMC_WriteData(0x24, 0x000003E8);  // A1:= 1 000 第一阶段加速度
  TMC_WriteData(0x25, 0x00000C350); // V1:= 50 000 加速度阈值速度 V1
  TMC_WriteData(0x26, 0x000001F4);  // AMAX = 500 大于 V1 的加速度
  TMC_WriteData(0x27, 0x00030D40);  // VMAX = 200 000
  TMC_WriteData(0x28, 0x000002BC);  // DMAX = 700 大于 V1 的减速度
  TMC_WriteData(0x2A, 0x00000578);  // D1 = 1400 小于 V1 的减速度
  TMC_WriteData(0x2B, 0x0000000A);  // VSTOP = 10 停止速度(接近于 0)
  TMC_WriteData(0x20, 0x00000000);  // RAMPMODE = 0 (目标位置运动)
  TMC_WriteData(0x2D,
                0xFFFF3800); // XTARGET = -51200 (向左运动一圈 (200*256 微步)

  // TMC_WriteData(0x13,0x000001F4);//TPWM_THRS=500 对应切换速度 35000 = ca.
  // 30RPM
  uint32_t gcon = TMC_ReadData(0x00);
  printf("GConf_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x09);
  printf("New_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x0A);
  printf("0x0A_STATUS = 0x%08X\n", gcon);
  HAL_Delay(1);
  gcon = TMC_ReadData(0x00);
  printf("GConf_STATUS = 0x%08X\n", gcon);
  gcon = TMC_ReadData(0x6C);
  printf("0x6C = 0x%08X\n", gcon);
}
void TMC5160_WriteReg(uint8_t addr, uint32_t data) {
  uint8_t i;
  TMC_S; // SPI_CS片选拉低

  SPI1_ReadWriteByte(addr & 0x80); // 写操作，最高位清0

  // 发送4字节数据 MSB先行
  for (i = 0; i < 4; i++) {
    SPI1_ReadWriteByte((data >> (8 * (3 - i))) & 0xFF);
  }

  TMC_U; // SPI_CS片选拉高
}
uint32_t TMC5160_ReadReg(uint8_t addr) {
  uint8_t i;
  uint32_t data = 0;

  TMC_S; // SPI_CS片选拉低

  SPI1_ReadWriteByte(addr | 0x7F); // 读操作，最高位置1

  for (i = 0; i < 4; i++) {
    data <<= 8;
    data |= SPI1_ReadWriteByte(0x00);
  }

  TMC_U; // SPI_CS片选拉高

  return data;
}
void TMC5160_Init(void) {
  TMC5160_WriteReg(0x00, 0x00000004);
  // 使能内部电流控制和电机使能

  // 寄存器地址定义
#define TMC5160_GCONF 0x00
#define TMC5160_IHOLD_IRUN 0x10
#define TMC5160_CHOPCONF 0x6C
#define TMC5160_PWMCONF 0x70
#define TMC5160_RAMPMODE 0x20
#define TMC5160_XTARGET 0x24
#define TMC5160_XACTUAL 0x21

#define GCONF 0x00
#define GSTAT 0x01
#define IHOLD_IRUN 0x10
#define CHOPCONF 0x6C
#define DRV_STATUS 0x6F
#define VSTART 0x40
#define A1 0x41
#define V1 0x42
#define AMAX 0x43
#define ENCM_CTRL 0x20

  TMC5160_WriteReg(0x00, 0x00000004); // GCONF: bit0和bit2置1
  uint32_t gcon = TMC5160_ReadReg(0x00);
  printf("GConf_STATUS = 0x%08X\n", gcon);
  // 设置电流
  uint32_t ihold = 5;
  uint32_t irun = 15;
  uint32_t iholddelay = 6;
  uint32_t ihold_irun_val = (iholddelay << 16) | (irun << 8) | ihold;
  TMC5160_WriteReg(0x10, ihold_irun_val);
  gcon = TMC5160_ReadReg(0x10);
  printf("iHold_STATUS = 0x%08X\n", gcon);

  // CHOPCONF配置
  uint32_t chopconf_val = 0;
  chopconf_val |= (3 << 0);  // TOFF
  chopconf_val |= (4 << 4);  // HSTRT
  chopconf_val |= (2 << 8);  // HEND
  chopconf_val |= (1 << 12); // TBL
  chopconf_val |= (4 << 24); // MRES=16细分
  TMC5160_WriteReg(0x6C, chopconf_val);
  gcon = TMC5160_ReadReg(0x6c);
  printf("CHOPCONF = 0x%08X\n", gcon);
  // 速度和加速度配置
  TMC5160_WriteReg(0x20, 1000 << 16); // VMAX
  TMC5160_WriteReg(0x22, 1000 << 16); // AMAX

  // 目标速度设置为0，待启动
  TMC5160_WriteReg(0x23, 0);

  HAL_Delay(10);*/
  /*return ;

   // 1. GCONF - 全局配置，启用内部电流控制等


  // 2. IHOLD_IRUN - 电流设置
  // IHOLD=1, IRUN=3, IHOLDDELAY=6
  uint32_t ihold = 1;
  uint32_t irun = 3;
  uint32_t iholddelay = 6;
  uint32_t ihold_irun_val = (iholddelay << 16) | (irun << 8) | ihold;
  TMC5160_WriteReg(0x10, ihold_irun_val);

  // 3. CHOPCONF - 细分和斩波配置
  // MRES=4 (16细分), 其他参数使用默认或示例值
  // CHOPCONF寄存器位说明：
  // bit3~0 MRES=4
  // bit14 TOFF=3 (斩波时间)
  // bit15 HEND=2 (斩波结束阈值)
  // bit16 HSTRT=4 (斩波开始阈值)
  // 这里给出一个示例配置：
  uint32_t chopconf_val = (3 << 0)    // TOFF=3
                        | (4 << 4)    // HSTRT=4
                        | (2 << 8)    // HEND=2
                        | (4 << 12)   // TBL=4 (斩波时间基准)
                        | (4 << 16)   // MRES=4 (16细分)
                        ;
  // 注意：具体位位置请参考手册，这里仅示例，实际请核对手册位定义

  // 由于MRES是bit3~bit0，上面写错了，正确是bit3~bit0
  // 重新写CHOPCONF，参考手册：
  // TOFF: bit3~0
  // HSTRT: bit7~4
  // HEND: bit11~8
  // TBL: bit13~12
  // MRES: bit15~12
  // 但MRES是bit15~12，和TBL冲突，手册显示MRES是bit3~0，TBL是bit13~12

  // 正确配置如下：
  chopconf_val = 0;
  chopconf_val |= (3 << 0);   // TOFF=3
  chopconf_val |= (4 << 4);   // HSTRT=4
  chopconf_val |= (2 << 8);   // HEND=2
  chopconf_val |= (1 << 12);  // TBL=1 (32 MHz)
  chopconf_val |= (4 << 16);  // MRES=4 (16细分)
  这里有误，MRES是bit3~0，不能放16位
  // 正确是MRES在bit3~0，所以：
  chopconf_val &= ~(0xF << 0);
  chopconf_val |= (4 << 0);   // MRES=4

  // 重新整理：
  chopconf_val = 0;
  chopconf_val |= (3 << 4);   // TOFF=3，bit6~4
  chopconf_val |= (4 << 0);   // MRES=4，bit3~0
  chopconf_val |= (2 << 8);   // HEND=2，bit11~8
  chopconf_val |= (1 << 12);  // TBL=1，bit13~12
  chopconf_val |= (4 << 16);  // HSTRT=4，bit19~16

  // 但手册中HSTRT是bit16~19，TOFF是bit0~3，MRES是bit24~27
  // 为避免混淆，建议查阅最新手册，以下为正确位定义（参考TMC5160 datasheet）：

  // TMC5160 CHOPCONF寄存器位定义：
  // TOFF: bit0~3
  // HSTRT: bit4~7
  // HEND: bit8~11
  // TBL: bit12~13
  // MRES: bit24~27

  chopconf_val = 0;
  chopconf_val |= (3 << 0);    // TOFF=3
  chopconf_val |= (4 << 4);    // HSTRT=4
  chopconf_val |= (2 << 8);    // HEND=2
  chopconf_val |= (1 << 12);   // TBL=1 (32 MHz)
  chopconf_val |= (4 << 24);   // MRES=4 (16细分)

  TMC5160_WriteReg(0x6C, chopconf_val);*/
  /*uint32_t gstat = TMC5160_ReadReg(0x01);
  printf("GSTAT = 0x%08X\n", gstat);
  if (gstat & 0x01)
    printf("Reset detected\n");
  if (gstat & 0x02)
    printf("Driver error detected\n");
  if (gstat & 0x04)
    printf("Undervoltage or overtemperature detected\n");

  uint32_t scon = TMC5160_ReadReg(0x6C);
  printf("Conf_STATUS = 0x%08X\n", scon);
  // 4. 其他寄存器可按需配置
}*/

/*
void SPI_SendByte(char data) {
  cmd[0] = data;
  if (HAL_SPI_Transmit(&hspi1, cmd, 1, 1000) == HAL_OK) {
    if (HAL_SPI_Receive(&hspi1, cmd, 1, 1000) == HAL_OK) {
      ReceiveData = cmd[0];
    } else
      ;
  } else
    ;
}

char SPI_ReceiveByte(void) {
  cmd[0] = 0;
  if (HAL_SPI_Transmit(&hspi1, cmd, 1, 1000) == HAL_OK) {
    if (HAL_SPI_Receive(&hspi1, cmd, 1, 1000) == HAL_OK) {
      // return cmd[0];
    } else
      ;
  } else
    ;
  return cmd[0];
}*/
// TMC5160 takes 40 bit data: 8 address and 32 data
/*void sendData(unsigned long address, long datagram) {
  unsigned char i;
  cmd[0] = address;
  cmd[1] = (datagram >> 24) & 0xff;
  cmd[2] = (datagram >> 16) & 0xff;
  cmd[3] = (datagram >> 8) & 0xff;
  cmd[4] = datagram & 0xff;

  TMC_S; // SPI_CS片选拉低
  //	SPI_SendByte(address);
  //	SPI_SendByte((datagram >> 24) & 0xff);
  //	SPI_SendByte((datagram >> 16) & 0xff);
  //	SPI_SendByte((datagram >> 8) & 0xff);
  //	SPI_SendByte(datagram & 0xff);
  for (i = 0; i < 5; i++) {
    if (HAL_SPI_Transmit(&hspi1, &cmd[i], 1, 100) == HAL_OK) {
    } else
      printf("spi1 err\r\n");
  }
  TMC_U; // SPI_CS片选拉高
}
unsigned long ReadData(long address) {
  char data[4] = {0, 0, 0, 0};
  unsigned long datagram = 0;

  TMC_S; // SPI_CS片选拉低

  SPI_SendByte(address);
  data[0] = SPI_ReceiveByte(); // SPI_ReceiveByte((datagram >> 24) & 0xff);
  data[1] = SPI_ReceiveByte(); // SPI_ReceiveByte((datagram >> 16) & 0xff);
  data[2] = SPI_ReceiveByte(); // SPI_ReceiveByte((datagram >> 8) & 0xff);
  data[3] = SPI_ReceiveByte(); // SPI_ReceiveByte(datagram & 0xff);
  TMC_U;                       // SPI_CS片选拉高

  datagram = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  return datagram;
}*/
/*void TMC_test(void) {

  //		if(PositionData==0)
  //		{
  //			if(DirFlag)
  //			{
  //				DirFlag=0;
  //				PositionData=51200*2;
  //			}
  //			else
  //			{
  //				DirFlag=1;
  //				PositionData=-51200*2;
  //			}
  //			sendData(0xAD,PositionData);
  ////PAGE36:XTARGET=51200*2(顺时针旋转2圈(1圈：200*256微步))
  //			sendData(0x21,0x00000000);
  ////PAGE35:
  //查询XACTUAL，下一个读操作返回XACTUAL，实际电机位置，该值通常只应在归零时修改。在位置模式下修改，修改内容将启动一个运动
  //			HAL_Delay(1000);
  //		}
  //		else
  //		{
  //			//read data
  //			HAL_Delay(500);
  //			sendData(0x21,0x00000000);
  ////PAGE35:
  //查询XACTUAL，下一个读操作返回XACTUAL，实际电机位置，该值通常只应在归零时修改。在位置模式下修改，修改内容将启动一个运动
  //			register_value = ReadData(0x21);
  ////PAGE35:读XACTUAL 			if(register_value==0)
  //			{
  //				PositionData=0;
  //			}
  //			else;
  sendData(0xAD,
           51200 *
               2); // PAGE36:XTARGET=51200*2(顺时针旋转2圈(1圈：200*256微步))
  sendData(
      0x21,
      0x00000000); // PAGE35:
                   // 查询XACTUAL，下一个读操作返回XACTUAL，实际电机位置，该值通常只应在归零时修改。在位置模式下修改，修改内容将启动一个运动
  HAL_Delay(6000);

  sendData(0xAD,
           -51200 *
               2); // PAGE36:XTARGET=51200*2(顺时针旋转2圈(1圈：200*256微步))
  sendData(
      0x21,
      0x00000000); // PAGE35:
                   // 查询XACTUAL，下一个读操作返回XACTUAL，实际电机位置，该值通常只应在归零时修改。在位置模式下修改，修改内容将启动一个运动
  HAL_Delay(6000);
}*/

/*

// 设置目标位置，启动运动
void TMC5160_MoveTo(int32_t position) {
  // 目标位置寄存器：0x7A (XTARGET)
  TMC5160_WriteReg(0x7A, (uint32_t)position);

  // 使能运动，写入XTARGET后，驱动自动开始运动
}

// 读取当前位置
int32_t TMC5160_GetPosition(void) {
  return (int32_t)TMC5160_ReadReg(0x21); // XACTUAL寄存器
}
*/