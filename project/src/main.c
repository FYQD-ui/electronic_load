/* add user code begin Header */
/**
  **************************************************************************
  * @file     main.c
  * @brief    main program
  **************************************************************************
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */
/* add user code end Header */

/* Includes ------------------------------------------------------------------*/
#include "at32f421_wk_config.h"
#include "wk_adc.h"
#include "wk_i2c.h"
#include "wk_usart.h"
#include "wk_dma.h"
#include "wk_gpio.h"
#include "wk_system.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "eload_config.h"
#include "eload_control.h"
#include "eload_led.h"
#include "hw_adc.h"
#include "hw_dac_mcp4725.h"
#include "hw_uart.h"
#include "protocol_cmd.h"
/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief main function.
  * @param  none
  * @retval none
  */
int main(void)
{
  /* add user code begin 1 */

  /* add user code end 1 */

  /* system clock config. */
  wk_system_clock_config();

  /* config periph clock. */
  wk_periph_clock_config();

  /* nvic config. */
  wk_nvic_config();

  /* timebase config for
     void wk_delay_us(uint32_t delay);
     void wk_delay_ms(uint32_t delay); */
  wk_timebase_init();

  /* init gpio function. */
  wk_gpio_config();

  /* init adc1 function. */
  wk_adc1_init();

  /* init usart1 function. */
  wk_usart1_init();
  /** USART1 使用接收中断和环形缓冲，保证二进制帧不会因 10ms 主循环周期丢字节。 */
  hw_uart_init();

  /* init i2c1 function. */
  wk_i2c1_init();

  /* init i2c2 function. */
  wk_i2c2_init();

  /* init dma1 channel1 */
  wk_dma1_channel1_init();
  /* config dma channel transfer parameter */
  /* user need to modify define values DMAx_CHANNELy_XXX_BASE_ADDR 
     and DMAx_CHANNELy_BUFFER_SIZE in at32xxx_wk_config.h */
  /* add user code begin dma config */
  /** ADC DMA 配置：外设源为 ADC1->odt，内存目标为 hw_adc.c 中的双通道缓冲区。 */
  hw_adc_init();
  wk_dma_channel_config(DMA1_CHANNEL1,
                        (uint32_t)&ADC1->odt,
                        hw_adc_dma_buffer_address(),
                        hw_adc_dma_buffer_size());
  dma_channel_enable(DMA1_CHANNEL1, TRUE);
  /* add user code end dma config */

  /* add user code begin 2 */
#if ELOAD_DAC_TEST_ENABLE
  /** DAC 专用测试模式：固定输出 1.65V，不启动会自动归零的业务控制层。 */
  hw_dac_mcp4725_test();
#else
  /** 业务初始化顺序：先让电子负载 DAC 输出 0，再打开串口命令接口。 */
  eload_control_init();
  eload_led_init();
  protocol_cmd_init();
#endif
  /* add user code end 2 */

  while(1)
  {
    /* add user code begin 3 */
#if ELOAD_DAC_TEST_ENABLE
    /** 周期重写固定测试电压，器件瞬时复位后也能自动恢复 1.65V。 */
    hw_dac_mcp4725_test();
    wk_delay_ms(500U);
#else
    /** 10ms 周期任务：触发采样、执行保护、处理串口命令。 */
    hw_adc_start_conversion();
    eload_control_task_10ms();
    eload_led_task_10ms();
    protocol_cmd_poll();
    wk_delay_ms(ELOAD_TASK_PERIOD_MS);
#endif
    /* add user code end 3 */
  }
}

  /* add user code begin 4 */

  /* add user code end 4 */
