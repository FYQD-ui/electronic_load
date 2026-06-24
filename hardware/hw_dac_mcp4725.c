#include "hw_dac_mcp4725.h"
#include "hw_i2c.h"
#include "hw_uart.h"
#include "wk_system.h"
#include "eload_config.h"

uint16_t hw_dac_mcp4725_current_to_raw(uint32_t current_ma)
{
  uint64_t numerator;
  uint64_t denominator;
  uint64_t raw;

  if(current_ma > ELOAD_LIMIT_CURRENT_MA)
  {
    current_ma = ELOAD_LIMIT_CURRENT_MA;
  }

  /**
   * 一次完成电流到 DAC 码值的有理数换算，避免先计算整数 mV 导致 20mA 步进：
   * raw = I(mA) * Rshunt(mOhm) / 1000
   *       * (Rhigh + Rlow) / Rlow
   *       * 4096 / VDD(mV)。
   */
  numerator = (uint64_t)current_ma * ELOAD_SHUNT_MOHM;
  numerator *= (ELOAD_DAC_TO_ISET_HIGH_OHM + ELOAD_DAC_TO_ISET_LOW_OHM);
  numerator *= (ELOAD_DAC_FULL_SCALE + 1UL);
  denominator = 1000ULL * ELOAD_DAC_TO_ISET_LOW_OHM * ELOAD_DAC_REF_MV;
  raw = (numerator + (denominator / 2ULL)) / denominator;
  if(raw > ELOAD_DAC_FULL_SCALE)
  {
    raw = ELOAD_DAC_FULL_SCALE;
  }
  return (uint16_t)raw;
}

error_status hw_dac_mcp4725_set_raw(uint16_t raw)
{
  uint8_t frame[3];

  if(raw > ELOAD_DAC_FULL_SCALE)
  {
    raw = (uint16_t)ELOAD_DAC_FULL_SCALE;
  }

  /** MCP4725 Write DAC Register 命令：C2=0,C1=1,C0=0,PD=00。 */
  frame[0] = (uint8_t)0x40U;
  frame[1] = (uint8_t)(raw >> 4);
  frame[2] = (uint8_t)((raw & 0x0FU) << 4);
  return hw_i2c_master_write(I2C1, MCP4725_ADDR7, frame, 3U);
}

error_status hw_dac_mcp4725_set_voltage_mv(uint32_t mv)
{
  uint32_t raw;

  if(mv > ELOAD_DAC_REF_MV)
  {
    mv = ELOAD_DAC_REF_MV;
  }
  /** MCP4725 传输函数为 VOUT=VDD*code/4096；加半分母实现四舍五入。 */
  raw = ((mv * (ELOAD_DAC_FULL_SCALE + 1UL)) + (ELOAD_DAC_REF_MV / 2UL)) / ELOAD_DAC_REF_MV;
  if(raw > ELOAD_DAC_FULL_SCALE)
  {
    raw = ELOAD_DAC_FULL_SCALE;
  }
  return hw_dac_mcp4725_set_raw((uint16_t)raw);
}

error_status hw_dac_mcp4725_set_current_ma(uint32_t current_ma)
{
  return hw_dac_mcp4725_set_raw(hw_dac_mcp4725_current_to_raw(current_ma));
}

void hw_dac_mcp4725_output_zero(void)
{
  /** 关断电子负载时必须先把 DAC 输出拉到 0，LM358 会关闭 MOS 栅极驱动。 */
  (void)hw_dac_mcp4725_set_raw(0U);
}

static const char *test_error_msg(hw_i2c_error_step_t step)
{
  switch(step)
  {
    case HW_I2C_ERR_NONE:              return "OK";
    case HW_I2C_ERR_BUSY_TIMEOUT:      return "BUSY timeout";
    case HW_I2C_ERR_START_TIMEOUT:     return "START timeout";
    case HW_I2C_ERR_ADDR_NACK:         return "ADDR NACK";
    case HW_I2C_ERR_DATA_NACK:         return "DATA NACK";
    case HW_I2C_ERR_BUS:               return "BUS error";
    case HW_I2C_ERR_ARBITRATION_LOST:  return "ARBITRATION lost";
    case HW_I2C_ERR_DATA_BYTE0_TIMEOUT: return "DATA[0] timeout";
    case HW_I2C_ERR_DATA_BYTE1_TIMEOUT: return "DATA[1] timeout";
    case HW_I2C_ERR_DATA_BYTE2_TIMEOUT: return "DATA[2] timeout";
    case HW_I2C_ERR_TDC_TIMEOUT:       return "TDC timeout";
    case HW_I2C_ERR_NULL_PARAM:        return "NULL param";
    default:                           return "UNKNOWN";
  }
}

static void test_print_error(void)
{
  const char *msg;
  uint8_t i;

  msg = test_error_msg(hw_i2c_get_last_error_step());
  hw_uart_write((const uint8_t *)"[DAC_TEST] I2C error: ", 22);
  for(i = 0U; msg[i] != '\0'; i++) { hw_uart_putc((uint8_t)msg[i]); }
  hw_uart_putc((uint8_t)'\r');
  hw_uart_putc((uint8_t)'\n');
}

void hw_dac_mcp4725_test(void)
{
  error_status ret;

  /** 专用测试模式持续保持固定电压，便于万用表和示波器稳定测量。 */
  ret = hw_dac_mcp4725_set_voltage_mv(ELOAD_DAC_TEST_VOLTAGE_MV);
  if(ret != SUCCESS) { test_print_error(); }
}
