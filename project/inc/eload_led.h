#ifndef __ELOAD_LED_H
#define __ELOAD_LED_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void eload_led_init(void);
void eload_led_task_10ms(void);

#ifdef __cplusplus
}
#endif

#endif
