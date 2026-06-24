#include "hw_adc.h"
#include "eload_config.h"

/** ADC DMA 原始缓冲区；DMA 按普通序列顺序写入 PA0、PA3 两个半字。 */
static volatile uint16_t s_adc_dma_buffer[HW_ADC_DMA_CHANNEL_COUNT];

void hw_adc_init(void)
{
  uint8_t i;

  /** 上电先清零采样值，避免控制层在 DMA 首次转换前读到随机值。 */
  for(i = 0U; i < HW_ADC_DMA_CHANNEL_COUNT; i++)
  {
    s_adc_dma_buffer[i] = 0U;
  }
}

void hw_adc_start_conversion(void)
{
  /** 软件触发一次双通道普通序列转换，结果由 DMA 写入 s_adc_dma_buffer。 */
  adc_ordinary_software_trigger_enable(ADC1, TRUE);
}

uint32_t hw_adc_dma_buffer_address(void)
{
  return (uint32_t)s_adc_dma_buffer;
}

uint16_t hw_adc_dma_buffer_size(void)
{
  return (uint16_t)HW_ADC_DMA_CHANNEL_COUNT;
}

uint16_t hw_adc_get_raw_current(void)
{
  return s_adc_dma_buffer[HW_ADC_INDEX_CURRENT];
}

uint16_t hw_adc_get_raw_voltage(void)
{
  return s_adc_dma_buffer[HW_ADC_INDEX_VOLTAGE];
}

uint32_t hw_adc_raw_to_mv(uint16_t raw)
{
  /** ADC 电压换算：raw / 4095 * 3300mV，使用整数四舍五入。 */
  return (((uint32_t)raw * ELOAD_ADC_REF_MV) + (ELOAD_ADC_FULL_SCALE / 2UL)) / ELOAD_ADC_FULL_SCALE;
}

uint32_t hw_adc_get_current_ma(void)
{
  uint32_t sense_mv;
  uint32_t denom;

  /** INA180A1 + 50mR：Vadc(mV)=I(mA)*R(mOhm)*Gain/1000。 */
  sense_mv = hw_adc_raw_to_mv(hw_adc_get_raw_current());
  denom = ELOAD_SHUNT_MOHM * ELOAD_INA_GAIN;
  if(denom == 0UL)
  {
    return 0UL;
  }
  return (sense_mv * 1000UL) / denom;
}

uint32_t hw_adc_get_load_voltage_mv(void)
{
  uint32_t adc_mv;

  /** 负载电压分压：Vload = Vadc * (R21 + R22) / R22。 */
  adc_mv = hw_adc_raw_to_mv(hw_adc_get_raw_voltage());
  return (adc_mv * (ELOAD_VOLT_DIV_HIGH_KOHM + ELOAD_VOLT_DIV_LOW_KOHM)) / ELOAD_VOLT_DIV_LOW_KOHM;
}
