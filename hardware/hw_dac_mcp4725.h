#ifndef __HW_DAC_MCP4725_H
#define __HW_DAC_MCP4725_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** MCP4725 A0 接 GND，7bit 地址为 0x60。 */
#define MCP4725_ADDR7                       (0x60U)

error_status hw_dac_mcp4725_set_raw(uint16_t raw);
error_status hw_dac_mcp4725_set_voltage_mv(uint32_t mv);
error_status hw_dac_mcp4725_set_current_ma(uint32_t current_ma);
void hw_dac_mcp4725_output_zero(void);
uint16_t hw_dac_mcp4725_current_to_raw(uint32_t current_ma);
void hw_dac_mcp4725_test(void);

#ifdef __cplusplus
}
#endif

#endif
