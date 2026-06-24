#ifndef __ELOAD_CONTROL_H
#define __ELOAD_CONTROL_H

#include "at32f421_wk_config.h"
#include "hw_fault.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 故障位：硬件反接/低压比较器触发。 */
#define ELOAD_FAULT_REV_HW                  (1UL << 0)

/** 故障位：硬件过压比较器触发。 */
#define ELOAD_FAULT_OV_HW                   (1UL << 1)

/** 故障位：MCU 测得负载电压超过 12V。 */
#define ELOAD_FAULT_OV_SW                   (1UL << 2)

/** 故障位：MCU 测得电流超过 3A 允许裕量。 */
#define ELOAD_FAULT_OC_SW                   (1UL << 3)

/** 故障位：绝对功率超限或连续 25W 超时。 */
#define ELOAD_FAULT_POWER                   (1UL << 4)

/** 故障位：TMP112 温度达到过温关断阈值。 */
#define ELOAD_FAULT_OVERTEMP                (1UL << 5)

/** 故障位：TMP112 连续通信失败，温度保护不可用。 */
#define ELOAD_FAULT_TEMP_SENSOR             (1UL << 6)

/** 故障位：MCP4725 通信失败，无法可靠控制负载电流。 */
#define ELOAD_FAULT_DAC_COMM                (1UL << 7)

typedef enum
{
  ELOAD_STATE_OFF = 0,
  ELOAD_STATE_IDLE,
  ELOAD_STATE_CC,
  ELOAD_STATE_FAULT
} eload_state_t;

typedef struct
{
  eload_state_t state;
  uint32_t set_current_ma;
  uint32_t measured_current_ma;
  uint32_t measured_voltage_mv;
  uint32_t measured_power_mw;
  int32_t temperature_mc;
  uint32_t fault_bitmap;
  uint8_t temperature_valid;
  uint8_t temperature_fail_count;
  uint8_t output_enabled;
  uint8_t fault_latched;
  hw_fault_status_t fault;
} eload_status_t;

void eload_control_init(void);
void eload_control_task_10ms(void);
error_status eload_control_set_current_ma(uint32_t current_ma);
error_status eload_control_output_enable(uint8_t enable);
error_status eload_control_clear_fault(void);
const eload_status_t *eload_control_get_status(void);
const char *eload_control_state_name(eload_state_t state);

#ifdef __cplusplus
}
#endif

#endif
