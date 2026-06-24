#ifndef __HW_ADC_H
#define __HW_ADC_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ADC DMA 通道数量；序列 1 为 PA0 电流，序列 2 为 PA3 负载电压。 */
#define HW_ADC_DMA_CHANNEL_COUNT           (2U)

/** ADC DMA 缓冲区索引：PA0/INA180 输出，单位为原始 ADC 码值。 */
#define HW_ADC_INDEX_CURRENT               (0U)

/** ADC DMA 缓冲区索引：PA3/LOAD_VOLT_ADC，单位为原始 ADC 码值。 */
#define HW_ADC_INDEX_VOLTAGE               (1U)

void hw_adc_init(void);
void hw_adc_start_conversion(void);
uint32_t hw_adc_dma_buffer_address(void);
uint16_t hw_adc_dma_buffer_size(void);
uint16_t hw_adc_get_raw_current(void);
uint16_t hw_adc_get_raw_voltage(void);
uint32_t hw_adc_raw_to_mv(uint16_t raw);
uint32_t hw_adc_get_current_ma(void);
uint32_t hw_adc_get_load_voltage_mv(void);

#ifdef __cplusplus
}
#endif

#endif
