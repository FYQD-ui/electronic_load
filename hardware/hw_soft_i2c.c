#include "hw_soft_i2c.h"
#include "wk_system.h"

/** 软件 I2C 半周期，单位 us；5us 对应约 100kHz 总线速率。 */
#define SOFT_I2C_HALF_PERIOD_US            (5U)

/** 等待 SCL 被释放为高电平的超时次数，用于兼容从机时钟拉伸。 */
#define SOFT_I2C_SCL_RELEASE_TIMEOUT       (200U)

static void soft_i2c_delay(void)
{
  wk_delay_us(SOFT_I2C_HALF_PERIOD_US);
}

static void soft_i2c_scl_low(void)
{
  gpio_bits_reset(SW_SCL_GPIO_PORT, SW_SCL_PIN);
}

static void soft_i2c_scl_release(void)
{
  /** 开漏输出写 1 表示释放总线，由板上的上拉电阻将 SCL 拉高。 */
  gpio_bits_set(SW_SCL_GPIO_PORT, SW_SCL_PIN);
}

static void soft_i2c_sda_low(void)
{
  gpio_bits_reset(SW_SDA_GPIO_PORT, SW_SDA_PIN);
}

static void soft_i2c_sda_release(void)
{
  /** 开漏输出写 1 表示释放总线，由板上的上拉电阻将 SDA 拉高。 */
  gpio_bits_set(SW_SDA_GPIO_PORT, SW_SDA_PIN);
}

static uint8_t soft_i2c_sda_read(void)
{
  return (gpio_input_data_bit_read(SW_SDA_GPIO_PORT, SW_SDA_PIN) == SET) ? 1U : 0U;
}

static error_status soft_i2c_wait_scl_high(void)
{
  uint16_t timeout;

  soft_i2c_scl_release();
  timeout = SOFT_I2C_SCL_RELEASE_TIMEOUT;
  while(gpio_input_data_bit_read(SW_SCL_GPIO_PORT, SW_SCL_PIN) == RESET)
  {
    if(timeout-- == 0U)
    {
      return ERROR;
    }
    wk_delay_us(1U);
  }
  return SUCCESS;
}

static error_status soft_i2c_start(void)
{
  soft_i2c_sda_release();
  if(soft_i2c_wait_scl_high() != SUCCESS)
  {
    return ERROR;
  }
  soft_i2c_delay();
  soft_i2c_sda_low();
  soft_i2c_delay();
  soft_i2c_scl_low();
  return SUCCESS;
}

static void soft_i2c_stop(void)
{
  soft_i2c_sda_low();
  soft_i2c_delay();
  (void)soft_i2c_wait_scl_high();
  soft_i2c_delay();
  soft_i2c_sda_release();
  soft_i2c_delay();
}

static error_status soft_i2c_write_byte(uint8_t value)
{
  uint8_t bit_index;
  uint8_t ack;

  for(bit_index = 0U; bit_index < 8U; bit_index++)
  {
    if((value & 0x80U) != 0U)
    {
      soft_i2c_sda_release();
    }
    else
    {
      soft_i2c_sda_low();
    }

    soft_i2c_delay();
    if(soft_i2c_wait_scl_high() != SUCCESS)
    {
      return ERROR;
    }
    soft_i2c_delay();
    soft_i2c_scl_low();
    value <<= 1;
  }

  /** 第九个时钟读取从机 ACK；ACK 为低电平。 */
  soft_i2c_sda_release();
  soft_i2c_delay();
  if(soft_i2c_wait_scl_high() != SUCCESS)
  {
    return ERROR;
  }
  ack = soft_i2c_sda_read();
  soft_i2c_delay();
  soft_i2c_scl_low();
  return (ack == 0U) ? SUCCESS : ERROR;
}

static error_status soft_i2c_read_byte(uint8_t *value, uint8_t send_ack)
{
  uint8_t bit_index;
  uint8_t data;

  if(value == 0)
  {
    return ERROR;
  }

  data = 0U;
  soft_i2c_sda_release();
  for(bit_index = 0U; bit_index < 8U; bit_index++)
  {
    data <<= 1;
    soft_i2c_delay();
    if(soft_i2c_wait_scl_high() != SUCCESS)
    {
      return ERROR;
    }
    if(soft_i2c_sda_read() != 0U)
    {
      data |= 1U;
    }
    soft_i2c_delay();
    soft_i2c_scl_low();
  }

  /** 读取非最后一个字节时发送 ACK，最后一个字节发送 NACK。 */
  if(send_ack != 0U)
  {
    soft_i2c_sda_low();
  }
  else
  {
    soft_i2c_sda_release();
  }
  soft_i2c_delay();
  if(soft_i2c_wait_scl_high() != SUCCESS)
  {
    return ERROR;
  }
  soft_i2c_delay();
  soft_i2c_scl_low();
  soft_i2c_sda_release();
  *value = data;
  return SUCCESS;
}

static error_status soft_i2c_recover_bus(void)
{
  uint8_t pulse;

  soft_i2c_sda_release();
  soft_i2c_scl_release();
  soft_i2c_delay();

  if(soft_i2c_sda_read() != 0U)
  {
    return SUCCESS;
  }

  /** SDA 被从机占用时输出最多 9 个 SCL 脉冲，使从机退出未完成字节。 */
  for(pulse = 0U; pulse < 9U; pulse++)
  {
    soft_i2c_scl_low();
    soft_i2c_delay();
    if(soft_i2c_wait_scl_high() != SUCCESS)
    {
      return ERROR;
    }
    soft_i2c_delay();
    if(soft_i2c_sda_read() != 0U)
    {
      break;
    }
  }

  soft_i2c_stop();
  return (soft_i2c_sda_read() != 0U) ? SUCCESS : ERROR;
}

error_status hw_soft_i2c_init(void)
{
  /** GPIO 已由 wk_gpio_config() 配成开漏输出，这里将两根线释放为空闲高电平。 */
  soft_i2c_scl_release();
  soft_i2c_sda_release();
  return soft_i2c_recover_bus();
}

error_status hw_soft_i2c_write_then_read(uint8_t addr7,
                                         const uint8_t *write_data,
                                         uint16_t write_len,
                                         uint8_t *read_data,
                                         uint16_t read_len)
{
  uint16_t index;

  if(((write_len != 0U) && (write_data == 0)) ||
     ((read_len != 0U) && (read_data == 0)) ||
     ((write_len == 0U) && (read_len == 0U)))
  {
    return ERROR;
  }

  if(soft_i2c_start() != SUCCESS)
  {
    soft_i2c_stop();
    return ERROR;
  }

  if(write_len != 0U)
  {
    if(soft_i2c_write_byte((uint8_t)(addr7 << 1)) != SUCCESS)
    {
      soft_i2c_stop();
      return ERROR;
    }

    for(index = 0U; index < write_len; index++)
    {
      if(soft_i2c_write_byte(write_data[index]) != SUCCESS)
      {
        soft_i2c_stop();
        return ERROR;
      }
    }
  }

  if(read_len != 0U)
  {
    /** 写完寄存器指针后使用重复 START 切换到读方向。 */
    if(write_len != 0U)
    {
      if(soft_i2c_start() != SUCCESS)
      {
        soft_i2c_stop();
        return ERROR;
      }
    }

    if(soft_i2c_write_byte((uint8_t)((addr7 << 1) | 0x01U)) != SUCCESS)
    {
      soft_i2c_stop();
      return ERROR;
    }

    for(index = 0U; index < read_len; index++)
    {
      if(soft_i2c_read_byte(&read_data[index], (index + 1U < read_len) ? 1U : 0U) != SUCCESS)
      {
        soft_i2c_stop();
        return ERROR;
      }
    }
  }

  soft_i2c_stop();
  return SUCCESS;
}
