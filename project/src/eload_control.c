#include "eload_control.h"
#include "eload_config.h"
#include "hw_adc.h"
#include "hw_dac_mcp4725.h"
#include "hw_tmp112.h"

/** TMP112 采样周期换算为 10ms 任务 tick。 */
#define ELOAD_TEMP_SAMPLE_TICKS            (ELOAD_TEMP_SAMPLE_PERIOD_MS / ELOAD_TASK_PERIOD_MS)

/** 连续功率超限时间换算为 10ms 任务 tick。 */
#define ELOAD_CONT_POWER_TRIP_TICKS        (ELOAD_CONT_POWER_TRIP_TIME_MS / ELOAD_TASK_PERIOD_MS)

/** 电子负载运行状态；协议层只读此结构，控制层集中修改。 */
static eload_status_t s_eload;

/** TMP112 周期读取计数器。 */
static uint16_t s_temperature_tick;

/** 连续超过 25W 的累计时间计数器。 */
static uint16_t s_cont_power_tick;

static void eload_control_enter_fault(uint32_t reason)
{
  /** 故障优先动作：立即 DAC=0，避免 MOS 继续线性耗散。 */
  hw_dac_mcp4725_output_zero();
  s_eload.output_enabled = 0U;
  s_eload.fault_latched = 1U;
  s_eload.fault_bitmap |= reason;
  s_eload.state = ELOAD_STATE_FAULT;
}

static void eload_control_read_temperature(void)
{
  int32_t temperature_mc;

  if(hw_tmp112_read_temperature_mc(&temperature_mc) == SUCCESS)
  {
    s_eload.temperature_mc = temperature_mc;
    s_eload.temperature_valid = 1U;
    s_eload.temperature_fail_count = 0U;
  }
  else
  {
    if(s_eload.temperature_fail_count < 0xFFU)
    {
      s_eload.temperature_fail_count++;
    }
    if(s_eload.temperature_fail_count >= ELOAD_TEMP_FAIL_LIMIT)
    {
      s_eload.temperature_valid = 0U;
    }
  }
}

static uint32_t eload_control_active_faults(void)
{
  uint32_t active;

  active = 0UL;
  if(s_eload.fault.rev_fault != 0U)
  {
    active |= ELOAD_FAULT_REV_HW;
  }
  if(s_eload.fault.ov_fault != 0U)
  {
    active |= ELOAD_FAULT_OV_HW;
  }
  if(s_eload.measured_voltage_mv > ELOAD_LIMIT_VOLTAGE_MV)
  {
    active |= ELOAD_FAULT_OV_SW;
  }
  if(s_eload.measured_current_ma > (ELOAD_LIMIT_CURRENT_MA + 100UL))
  {
    active |= ELOAD_FAULT_OC_SW;
  }
  if(s_eload.measured_power_mw > ELOAD_LIMIT_POWER_ABS_MW)
  {
    active |= ELOAD_FAULT_POWER;
  }
  if((s_eload.temperature_valid != 0U) && (s_eload.temperature_mc >= ELOAD_TEMP_TRIP_MC))
  {
    active |= ELOAD_FAULT_OVERTEMP;
  }
  if((s_eload.temperature_valid == 0U) &&
     (s_eload.temperature_fail_count >= ELOAD_TEMP_FAIL_LIMIT))
  {
    active |= ELOAD_FAULT_TEMP_SENSOR;
  }
  return active;
}

void eload_control_init(void)
{
  /** 上电默认关闭输出，必须收到 OUT 1 命令后才进入恒流状态。 */
  s_eload.state = ELOAD_STATE_OFF;
  s_eload.set_current_ma = 0UL;
  s_eload.measured_current_ma = 0UL;
  s_eload.measured_voltage_mv = 0UL;
  s_eload.measured_power_mw = 0UL;
  s_eload.temperature_mc = 0L;
  s_eload.fault_bitmap = 0UL;
  s_eload.temperature_valid = 0U;
  s_eload.temperature_fail_count = 0U;
  s_eload.output_enabled = 0U;
  s_eload.fault_latched = 0U;
  s_eload.fault.rev_fault = 0U;
  s_eload.fault.ov_fault = 0U;
  s_temperature_tick = 0U;
  s_cont_power_tick = 0U;

  hw_dac_mcp4725_output_zero();
  if(hw_tmp112_init() == SUCCESS)
  {
    eload_control_read_temperature();
  }
  else
  {
    s_eload.temperature_fail_count = ELOAD_TEMP_FAIL_LIMIT;
    s_eload.temperature_valid = 0U;
    eload_control_enter_fault(ELOAD_FAULT_TEMP_SENSOR);
  }
}

void eload_control_task_10ms(void)
{
  uint32_t active_faults;

  hw_fault_get(&s_eload.fault);
  s_eload.measured_current_ma = hw_adc_get_current_ma();
  s_eload.measured_voltage_mv = hw_adc_get_load_voltage_mv();
  s_eload.measured_power_mw = (s_eload.measured_current_ma * s_eload.measured_voltage_mv) / 1000UL;

  s_temperature_tick++;
  if(s_temperature_tick >= ELOAD_TEMP_SAMPLE_TICKS)
  {
    s_temperature_tick = 0U;
    eload_control_read_temperature();
  }

  /** 25W 以上允许短时工作，持续 1s 后按功率故障处理；降到 25W 以下立即清零计时。 */
  if(s_eload.measured_power_mw > ELOAD_LIMIT_POWER_CONT_MW)
  {
    if(s_cont_power_tick < ELOAD_CONT_POWER_TRIP_TICKS)
    {
      s_cont_power_tick++;
    }
  }
  else
  {
    s_cont_power_tick = 0U;
  }

  active_faults = eload_control_active_faults();
  if(s_cont_power_tick >= ELOAD_CONT_POWER_TRIP_TICKS)
  {
    active_faults |= ELOAD_FAULT_POWER;
  }

  if(active_faults != 0UL)
  {
    eload_control_enter_fault(active_faults);
    return;
  }

  if(s_eload.fault_latched != 0U)
  {
    s_eload.state = ELOAD_STATE_FAULT;
    hw_dac_mcp4725_output_zero();
    return;
  }

  if(s_eload.output_enabled == 0U)
  {
    s_eload.state = ELOAD_STATE_IDLE;
    hw_dac_mcp4725_output_zero();
    return;
  }

  /** 运行态只输出被限幅后的设定电流，真实闭环由 LM358 模拟环路完成。 */
  s_eload.state = ELOAD_STATE_CC;
  if(hw_dac_mcp4725_set_current_ma(s_eload.set_current_ma) != SUCCESS)
  {
    /** DAC 通信失败时保持关断，并记录独立的 DAC 通信故障位。 */
    eload_control_enter_fault(ELOAD_FAULT_DAC_COMM);
  }
}

error_status eload_control_set_current_ma(uint32_t current_ma)
{
  if(current_ma > ELOAD_LIMIT_CURRENT_MA)
  {
    return ERROR;
  }

  s_eload.set_current_ma = current_ma;
  if((s_eload.output_enabled != 0U) && (s_eload.fault_latched == 0U))
  {
    return hw_dac_mcp4725_set_current_ma(s_eload.set_current_ma);
  }
  return SUCCESS;
}

error_status eload_control_output_enable(uint8_t enable)
{
  if(enable == 0U)
  {
    s_eload.output_enabled = 0U;
    s_eload.state = ELOAD_STATE_IDLE;
    hw_dac_mcp4725_output_zero();
    return SUCCESS;
  }

  /** 温度无效、温度过高或有锁存故障时禁止打开功率输出。 */
  if((s_eload.fault_latched != 0U) ||
     (s_eload.temperature_valid == 0U) ||
     (s_eload.temperature_mc >= ELOAD_TEMP_TRIP_MC))
  {
    return ERROR;
  }

  s_eload.output_enabled = 1U;
  return hw_dac_mcp4725_set_current_ma(s_eload.set_current_ma);
}

error_status eload_control_clear_fault(void)
{
  uint32_t active_faults;

  hw_fault_get(&s_eload.fault);
  eload_control_read_temperature();
  active_faults = eload_control_active_faults();

  /** 过温故障必须降到恢复阈值以下，避免在临界温度反复启停。 */
  if((s_eload.temperature_valid == 0U) ||
     (s_eload.temperature_mc > ELOAD_TEMP_CLEAR_MC) ||
     (active_faults != 0UL))
  {
    return ERROR;
  }

  s_eload.fault_latched = 0U;
  s_eload.fault_bitmap = 0UL;
  s_eload.output_enabled = 0U;
  s_eload.state = ELOAD_STATE_IDLE;
  s_cont_power_tick = 0U;
  hw_dac_mcp4725_output_zero();
  return SUCCESS;
}

const eload_status_t *eload_control_get_status(void)
{
  return &s_eload;
}

const char *eload_control_state_name(eload_state_t state)
{
  switch(state)
  {
    case ELOAD_STATE_OFF:
      return "OFF";
    case ELOAD_STATE_IDLE:
      return "IDLE";
    case ELOAD_STATE_CC:
      return "CC";
    case ELOAD_STATE_FAULT:
      return "FAULT";
    default:
      return "UNKNOWN";
  }
}
