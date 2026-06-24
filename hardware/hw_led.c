#include "hw_led.h"

/** 软件缓存 LED 亮灭状态，避免依赖读取 GPIO 输出寄存器的芯片差异。 */
static uint8_t s_hw_led_on;

void hw_led_init(void)
{
  /** 初始化后先关闭 LED，后续由 eload_led_task_10ms() 按状态显示。 */
  s_hw_led_on = 0U;
  hw_led_off();
}

void hw_led_set(uint8_t on)
{
  s_hw_led_on = (on == 0U) ? 0U : 1U;

#if (HW_LED_ACTIVE_HIGH != 0U)
  if(s_hw_led_on != 0U)
  {
    gpio_bits_set(LED_GPIO_PORT, LED_PIN);
  }
  else
  {
    gpio_bits_reset(LED_GPIO_PORT, LED_PIN);
  }
#else
  if(s_hw_led_on != 0U)
  {
    gpio_bits_reset(LED_GPIO_PORT, LED_PIN);
  }
  else
  {
    gpio_bits_set(LED_GPIO_PORT, LED_PIN);
  }
#endif
}

void hw_led_on(void)
{
  hw_led_set(1U);
}

void hw_led_off(void)
{
  hw_led_set(0U);
}

void hw_led_toggle(void)
{
  hw_led_set((s_hw_led_on == 0U) ? 1U : 0U);
}

uint8_t hw_led_get_state(void)
{
  return s_hw_led_on;
}
