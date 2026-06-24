#include "hw_i2c.h"

/** I2C 等待循环上限；避免器件未焊接或总线被拉低时死等。 */
#define HW_I2C_TIMEOUT                    (50000UL)

static hw_i2c_error_step_t s_last_error_step = HW_I2C_ERR_NONE;

static void hw_i2c_clear_error_flags(i2c_type *i2c_x)
{
  i2c_flag_clear(i2c_x, I2C_ACKFAIL_FLAG | I2C_BUSERR_FLAG | I2C_ARLOST_FLAG);
}

static void hw_i2c_abort(i2c_type *i2c_x)
{
  i2c_stop_generate(i2c_x);
  hw_i2c_clear_error_flags(i2c_x);
}

hw_i2c_error_step_t hw_i2c_get_last_error_step(void)
{
  return s_last_error_step;
}

static error_status hw_i2c_wait_flag(i2c_type *i2c_x,
                                     uint32_t flag,
                                     flag_status state,
                                     hw_i2c_error_step_t timeout_step,
                                     hw_i2c_error_step_t nack_step)
{
  uint32_t timeout;

  timeout = HW_I2C_TIMEOUT;
  while(i2c_flag_get(i2c_x, flag) != state)
  {
    if(i2c_flag_get(i2c_x, I2C_ACKFAIL_FLAG) == SET)
    {
      s_last_error_step = nack_step;
      return ERROR;
    }
    if(i2c_flag_get(i2c_x, I2C_BUSERR_FLAG) == SET)
    {
      s_last_error_step = HW_I2C_ERR_BUS;
      return ERROR;
    }
    if(i2c_flag_get(i2c_x, I2C_ARLOST_FLAG) == SET)
    {
      s_last_error_step = HW_I2C_ERR_ARBITRATION_LOST;
      return ERROR;
    }
    if(timeout-- == 0UL)
    {
      s_last_error_step = timeout_step;
      return ERROR;
    }
  }
  return SUCCESS;
}

error_status hw_i2c_master_write(i2c_type *i2c_x, uint8_t addr7, const uint8_t *data, uint16_t len)
{
  uint16_t i;
  volatile uint32_t dummy;

  s_last_error_step = HW_I2C_ERR_NONE;

  if((data == 0) || (len == 0U))
  {
    s_last_error_step = HW_I2C_ERR_NULL_PARAM;
    return ERROR;
  }

  /** 清除上一笔异常传输遗留的错误标志，保证本次等待条件有效。 */
  hw_i2c_clear_error_flags(i2c_x);

  /** 等待总线空闲，防止上一笔传输未释放 STOP 时直接发起 START。 */
  if(hw_i2c_wait_flag(i2c_x,
                      I2C_BUSYF_FLAG,
                      RESET,
                      HW_I2C_ERR_BUSY_TIMEOUT,
                      HW_I2C_ERR_ADDR_NACK) != SUCCESS)
  {
    hw_i2c_clear_error_flags(i2c_x);
    return ERROR;
  }

  i2c_start_generate(i2c_x);
  if(hw_i2c_wait_flag(i2c_x,
                      I2C_STARTF_FLAG,
                      SET,
                      HW_I2C_ERR_START_TIMEOUT,
                      HW_I2C_ERR_ADDR_NACK) != SUCCESS)
  {
    hw_i2c_abort(i2c_x);
    return ERROR;
  }

  i2c_7bit_address_send(i2c_x, (uint8_t)(addr7 << 1), I2C_DIRECTION_TRANSMIT);
  if(hw_i2c_wait_flag(i2c_x,
                      I2C_ADDR7F_FLAG,
                      SET,
                      HW_I2C_ERR_ADDR_NACK,
                      HW_I2C_ERR_ADDR_NACK) != SUCCESS)
  {
    hw_i2c_abort(i2c_x);
    return ERROR;
  }

  /** 读 STS2 清除地址匹配状态，这是 AT32/I2C 主机发送流程的必要步骤。 */
  dummy = i2c_x->sts2;
  (void)dummy;

  for(i = 0U; i < len; i++)
  {
    if(hw_i2c_wait_flag(i2c_x,
                        I2C_TDBE_FLAG,
                        SET,
                        (hw_i2c_error_step_t)((uint8_t)HW_I2C_ERR_DATA_BYTE0_TIMEOUT + i),
                        HW_I2C_ERR_DATA_NACK) != SUCCESS)
    {
      hw_i2c_abort(i2c_x);
      return ERROR;
    }
    i2c_data_send(i2c_x, data[i]);
  }

  if(hw_i2c_wait_flag(i2c_x,
                      I2C_TDC_FLAG,
                      SET,
                      HW_I2C_ERR_TDC_TIMEOUT,
                      HW_I2C_ERR_DATA_NACK) != SUCCESS)
  {
    hw_i2c_abort(i2c_x);
    return ERROR;
  }

  i2c_stop_generate(i2c_x);
  return SUCCESS;
}
