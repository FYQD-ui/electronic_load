#ifndef __HW_LED_H
#define __HW_LED_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PB3 控制的 LED 为 MCU 输出高电平点亮：PB3 -> R23 -> LED2_A，LED2_C -> GND。 */
#define HW_LED_ACTIVE_HIGH                 (1U)

void hw_led_init(void);
void hw_led_set(uint8_t on);
void hw_led_on(void);
void hw_led_off(void);
void hw_led_toggle(void);
uint8_t hw_led_get_state(void);

#ifdef __cplusplus
}
#endif

#endif
