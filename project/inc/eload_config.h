#ifndef __ELOAD_CONFIG_H
#define __ELOAD_CONFIG_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 电子负载硬件设计最大电流，单位 mA；所有设定电流必须被限制在 0~3000mA。 */
#define ELOAD_LIMIT_CURRENT_MA             (3000U)

/** 电子负载硬件设计最大输入电压，单位 mV；超过此值进入软件保护。 */
#define ELOAD_LIMIT_VOLTAGE_MV             (12000U)

/** 连续功耗建议上限，单位 mW；按当前 25W 连续散热能力做默认保护。 */
#define ELOAD_LIMIT_POWER_CONT_MW          (25000UL)

/** 短时功耗绝对上限，单位 mW；达到 36W 立即关断。 */
#define ELOAD_LIMIT_POWER_ABS_MW           (36000UL)

/** ADC 参考电压，单位 mV；当前模拟/数字电源为 3.3V。 */
#define ELOAD_ADC_REF_MV                   (3300UL)

/** ADC 满量程码值；AT32F421 ADC 为 12bit。 */
#define ELOAD_ADC_FULL_SCALE               (4095UL)

/** 电流采样电阻，单位 mOhm；原理图 R9 = 50mR。 */
#define ELOAD_SHUNT_MOHM                   (50UL)

/** 电流检测放大器增益；原理图 U5 = INA180A1，增益为 20V/V。 */
#define ELOAD_INA_GAIN                     (20UL)

/** 负载电压上分压电阻，单位 kOhm；原理图 R21 = 100K。 */
#define ELOAD_VOLT_DIV_HIGH_KOHM           (100UL)

/** 负载电压下分压电阻，单位 kOhm；原理图 R22 = 27K。 */
#define ELOAD_VOLT_DIV_LOW_KOHM            (27UL)

/** DAC 分辨率；MCP4725 为 12bit。 */
#define ELOAD_DAC_FULL_SCALE               (4095UL)

/** DAC 参考/供电电压，单位 mV；MCP4725 由 3V3 供电。 */
#define ELOAD_DAC_REF_MV                   (3300UL)

/** MCP4725 专用测试开关：1=持续输出固定电压并绕过正常控制逻辑，0=正常业务。 */
#define ELOAD_DAC_TEST_ENABLE              (0U)

/** MCP4725 专用测试输出电压，单位 mV。 */
#define ELOAD_DAC_TEST_VOLTAGE_MV          (1650UL)

/** DAC 到 I_SET 的上电阻，单位 Ohm；原理图 R17 = 100K。 */
#define ELOAD_DAC_TO_ISET_HIGH_OHM         (100000UL)

/** I_SET 到 GND 的下电阻，单位 Ohm；原理图 R16 = 4.7K。 */
#define ELOAD_DAC_TO_ISET_LOW_OHM          (4700UL)

/** 反接/异常比较器软件判定极性；0 表示低电平为故障，实测相反时只改此宏。 */
#define ELOAD_REV_FAULT_ACTIVE_LEVEL       (1U)

/** 过压比较器软件判定极性；1 表示高电平为故障，实测相反时只改此宏。 */
#define ELOAD_OV_FAULT_ACTIVE_LEVEL        (1U)

/** TMP112 温度采样周期，单位 ms；默认转换速率 4Hz，对应 250ms。 */
#define ELOAD_TEMP_SAMPLE_PERIOD_MS         (250U)

/** 过温关断阈值，单位 m°C；80°C 时立即关闭电子负载并锁存故障。 */
#define ELOAD_TEMP_TRIP_MC                  (80000L)

/** 过温故障允许清除阈值，单位 m°C；降到 70°C 以下才允许 CLR。 */
#define ELOAD_TEMP_CLEAR_MC                 (70000L)

/** TMP112 连续通信失败次数上限；达到后关闭输出并报告传感器故障。 */
#define ELOAD_TEMP_FAIL_LIMIT               (3U)

/** 连续功耗超过 25W 的允许时间，单位 ms；超过该时间后进入功率故障。 */
#define ELOAD_CONT_POWER_TRIP_TIME_MS       (1000U)

/** 串口命令最大长度，包含结尾 0；当前 ASCII 命令保持短帧便于调试。 */
#define ELOAD_CMD_LINE_SIZE                (96U)

/** 主循环任务周期，单位 ms；采样、保护、协议轮询都按此周期推进。 */
#define ELOAD_TASK_PERIOD_MS               (10U)

#ifdef __cplusplus
}
#endif

#endif
