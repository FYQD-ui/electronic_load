#ifndef __PROTOCOL_CMD_H
#define __PROTOCOL_CMD_H

#include "at32f421_wk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 二进制协议帧头。 */
#define PROTOCOL_HEADER_1                   (0x55U)
#define PROTOCOL_HEADER_2                   (0xAAU)

/** 二进制协议版本。 */
#define PROTOCOL_VERSION                    (3U)

/** 请求命令。 */
#define PROTOCOL_CMD_PING                   (0x01U)
#define PROTOCOL_CMD_INFO                   (0x02U)
#define PROTOCOL_CMD_STATUS                 (0x03U)
#define PROTOCOL_CMD_SET_CURRENT            (0x10U)
#define PROTOCOL_CMD_SET_OUTPUT             (0x11U)
#define PROTOCOL_CMD_CLEAR_FAULT            (0x12U)

/** 响应命令在请求命令基础上设置 bit7。 */
#define PROTOCOL_CMD_RESPONSE_MASK          (0x80U)

/** 统一响应状态码，位于响应 payload 的第一个字节。 */
typedef enum
{
  PROTOCOL_STATUS_OK = 0x00,
  PROTOCOL_STATUS_BAD_COMMAND = 0x01,
  PROTOCOL_STATUS_BAD_LENGTH = 0x02,
  PROTOCOL_STATUS_RANGE = 0x03,
  PROTOCOL_STATUS_FAULT_LATCHED = 0x04,
  PROTOCOL_STATUS_TEMP_INVALID = 0x05,
  PROTOCOL_STATUS_FAULT_ACTIVE = 0x06,
  PROTOCOL_STATUS_IO_ERROR = 0x07
} protocol_status_t;

void protocol_cmd_init(void);
void protocol_cmd_poll(void);

#ifdef __cplusplus
}
#endif

#endif
