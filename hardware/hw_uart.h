#ifndef __HW_UART_H
#define __HW_UART_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void hw_uart_init(void);
void hw_uart_irq_handler(void);
void hw_uart_putc(uint8_t value);
void hw_uart_write(const uint8_t *data, uint16_t length);
uint8_t hw_uart_getc_nonblock(uint8_t *value);
uint32_t hw_uart_get_rx_overflow_count(void);

#ifdef __cplusplus
}
#endif

#endif
