#include "hw_uart.h"

/** USART1 中断接收环形缓冲区大小；必须为 2 的整数次幂。 */
#define HW_UART_RX_BUFFER_SIZE             (128U)
#define HW_UART_RX_BUFFER_MASK             (HW_UART_RX_BUFFER_SIZE - 1U)

/** USART1 接收环形缓冲区，由中断写入、主循环读取。 */
static volatile uint8_t s_rx_buffer[HW_UART_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint32_t s_rx_overflow_count;

void hw_uart_init(void)
{
  /** 清空软件接收缓冲，并启用 USART1 接收中断。 */
  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_rx_overflow_count = 0UL;
  nvic_irq_enable(USART1_IRQn, 2U, 0U);
  usart_interrupt_enable(USART1, USART_RDBF_INT, TRUE);
}

void hw_uart_irq_handler(void)
{
  uint8_t value;
  uint16_t next_head;

  if(usart_flag_get(USART1, USART_RDBF_FLAG) == SET)
  {
    value = (uint8_t)(usart_data_receive(USART1) & 0xFFU);
    next_head = (uint16_t)((s_rx_head + 1U) & HW_UART_RX_BUFFER_MASK);
    if(next_head != s_rx_tail)
    {
      s_rx_buffer[s_rx_head] = value;
      s_rx_head = next_head;
    }
    else
    {
      /** 缓冲区已满时丢弃新字节并累计计数，避免破坏尚未解析的数据。 */
      s_rx_overflow_count++;
    }
  }
}

void hw_uart_putc(uint8_t value)
{
  while(usart_flag_get(USART1, USART_TDBE_FLAG) == RESET)
  {
  }
  usart_data_transmit(USART1, value);
}

void hw_uart_write(const uint8_t *data, uint16_t length)
{
  uint16_t index;

  if(data == 0)
  {
    return;
  }

  for(index = 0U; index < length; index++)
  {
    hw_uart_putc(data[index]);
  }
}

uint8_t hw_uart_getc_nonblock(uint8_t *value)
{
  if((value == 0) || (s_rx_head == s_rx_tail))
  {
    return 0U;
  }

  *value = s_rx_buffer[s_rx_tail];
  s_rx_tail = (uint16_t)((s_rx_tail + 1U) & HW_UART_RX_BUFFER_MASK);
  return 1U;
}

uint32_t hw_uart_get_rx_overflow_count(void)
{
  return s_rx_overflow_count;
}
