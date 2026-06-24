#ifndef __HW_FAULT_H
#define __HW_FAULT_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  uint8_t rev_fault;
  uint8_t ov_fault;
} hw_fault_status_t;

void hw_fault_get(hw_fault_status_t *status);
uint8_t hw_fault_any_active(void);

#ifdef __cplusplus
}
#endif

#endif
