#ifndef __HW_SOFT_I2C_H
#define __HW_SOFT_I2C_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

error_status hw_soft_i2c_init(void);
error_status hw_soft_i2c_write_then_read(uint8_t addr7,
                                         const uint8_t *write_data,
                                         uint16_t write_len,
                                         uint8_t *read_data,
                                         uint16_t read_len);

#ifdef __cplusplus
}
#endif

#endif
