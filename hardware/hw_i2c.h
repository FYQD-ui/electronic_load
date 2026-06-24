#ifndef __HW_I2C_H
#define __HW_I2C_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** I2C 写操作失败时的具体阶段，用于硬件调试定位。 */
typedef enum
{
  HW_I2C_ERR_NONE = 0,
  HW_I2C_ERR_BUSY_TIMEOUT,
  HW_I2C_ERR_START_TIMEOUT,
  HW_I2C_ERR_ADDR_NACK,
  HW_I2C_ERR_DATA_NACK,
  HW_I2C_ERR_BUS,
  HW_I2C_ERR_ARBITRATION_LOST,
  HW_I2C_ERR_DATA_BYTE0_TIMEOUT,
  HW_I2C_ERR_DATA_BYTE1_TIMEOUT,
  HW_I2C_ERR_DATA_BYTE2_TIMEOUT,
  HW_I2C_ERR_TDC_TIMEOUT,
  HW_I2C_ERR_NULL_PARAM
} hw_i2c_error_step_t;

error_status hw_i2c_master_write(i2c_type *i2c_x, uint8_t addr7, const uint8_t *data, uint16_t len);
hw_i2c_error_step_t hw_i2c_get_last_error_step(void);

#ifdef __cplusplus
}
#endif

#endif
