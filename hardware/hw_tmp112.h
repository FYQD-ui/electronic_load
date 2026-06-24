#ifndef __HW_TMP112_H
#define __HW_TMP112_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** TMP112A SOT563 的 ADD0 接 GND，7bit I2C 地址为 0x48。 */
#define TMP112_ADDR7                         (0x48U)

/** TMP112 温度寄存器指针。 */
#define TMP112_REG_TEMPERATURE               (0x00U)

error_status hw_tmp112_init(void);
error_status hw_tmp112_read_temperature_mc(int32_t *temperature_mc);

#ifdef __cplusplus
}
#endif

#endif
