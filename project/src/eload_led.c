#include "eload_led.h"
#include "eload_control.h"
#include "hw_led.h"

/** 空闲/关闭状态慢闪周期，单位为 10ms tick；50 表示 500ms 翻转一次。 */
#define ELOAD_LED_IDLE_TOGGLE_TICKS        (50U)

/** 故障状态快闪周期，单位为 10ms tick；10 表示 100ms 翻转一次。 */
#define ELOAD_LED_FAULT_TOGGLE_TICKS       (10U)

/** LED 状态机计数器；只在 10ms 周期任务里更新。 */
static uint16_t s_led_tick;

void eload_led_init(void)
{
  /** LED 上电默认关闭，避免误认为电子负载已经开始输出。 */
  s_led_tick = 0U;
  hw_led_init();
}

void eload_led_task_10ms(void)
{
  const eload_status_t *status;
  uint16_t toggle_ticks;

  status = eload_control_get_status();

  if(status->state == ELOAD_STATE_CC)
  {
    /** 恒流输出中：LED 常亮，表示电子负载正在吸收电流。 */
    s_led_tick = 0U;
    hw_led_on();
    return;
  }

  if(status->state == ELOAD_STATE_FAULT)
  {
    /** 故障锁存：LED 快闪，提示必须排除故障并执行 CLR 后才能再次输出。 */
    toggle_ticks = ELOAD_LED_FAULT_TOGGLE_TICKS;
  }
  else
  {
    /** 关闭或空闲：LED 慢闪，表示 MCU 固件运行正常但未输出。 */
    toggle_ticks = ELOAD_LED_IDLE_TOGGLE_TICKS;
  }

  s_led_tick++;
  if(s_led_tick >= toggle_ticks)
  {
    s_led_tick = 0U;
    hw_led_toggle();
  }
}
