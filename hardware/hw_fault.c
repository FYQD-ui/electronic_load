#include "hw_fault.h"
#include "eload_config.h"

static uint8_t hw_fault_level_to_active(uint8_t level, uint8_t active_level)
{
  return (level == active_level) ? 1U : 0U;
}

void hw_fault_get(hw_fault_status_t *status)
{
  uint8_t rev_level;
  uint8_t ov_level;

  if(status == 0)
  {
    return;
  }

  /** REV_FAULT/OV_FAULT 均来自 LM393 比较器输出，极性由 eload_config.h 集中配置。 */
  rev_level = (gpio_input_data_bit_read(REV_FAULT_GPIO_PORT, REV_FAULT_PIN) == SET) ? 1U : 0U;
  ov_level = (gpio_input_data_bit_read(OV_FAULT_GPIO_PORT, OV_FAULT_PIN) == SET) ? 1U : 0U;

  status->rev_fault = hw_fault_level_to_active(rev_level, ELOAD_REV_FAULT_ACTIVE_LEVEL);
  status->ov_fault = hw_fault_level_to_active(ov_level, ELOAD_OV_FAULT_ACTIVE_LEVEL);
}

uint8_t hw_fault_any_active(void)
{
  hw_fault_status_t status;

  hw_fault_get(&status);
  return (uint8_t)((status.rev_fault != 0U) || (status.ov_fault != 0U));
}
