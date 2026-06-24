#include "hw_tmp112.h"
#include "hw_soft_i2c.h"

error_status hw_tmp112_init(void)
{
  /** TMP112 上电默认连续转换，无需写配置寄存器；这里只初始化并恢复软件 I2C 总线。 */
  return hw_soft_i2c_init();
}

error_status hw_tmp112_read_temperature_mc(int32_t *temperature_mc)
{
  uint8_t pointer;
  uint8_t data[2];
  int16_t raw12;
  int32_t value;

  if(temperature_mc == 0)
  {
    return ERROR;
  }

  pointer = TMP112_REG_TEMPERATURE;
  if(hw_soft_i2c_write_then_read(TMP112_ADDR7, &pointer, 1U, data, 2U) != SUCCESS)
  {
    return ERROR;
  }

  /** 默认模式温度为左对齐 12bit 二补码，最低有效位等于 0.0625°C。 */
  raw12 = (int16_t)(((uint16_t)data[0] << 8) | data[1]);
  raw12 >>= 4;

  /** 0.0625°C = 62.5m°C；乘 625 后除 10，保持整数运算。 */
  value = ((int32_t)raw12 * 625L) / 10L;
  *temperature_mc = value;
  return SUCCESS;
}
