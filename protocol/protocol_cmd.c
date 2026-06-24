#include "protocol_cmd.h"
#include "eload_config.h"
#include "eload_control.h"
#include "hw_uart.h"

/** 协议固件版本号。 */
#define PROTOCOL_FW_MAJOR                   (1U)
#define PROTOCOL_FW_MINOR                   (3U)
#define PROTOCOL_FW_PATCH                   (0U)

/** 请求 payload 最大长度；当前命令最多只需要 2 字节，预留扩展空间。 */
#define PROTOCOL_MAX_PAYLOAD                (48U)

/** LEN 最小包含 SEQ 和 CMD 两字节。 */
#define PROTOCOL_MIN_BODY_LENGTH            (2U)
#define PROTOCOL_MAX_BODY_LENGTH            (PROTOCOL_MIN_BODY_LENGTH + PROTOCOL_MAX_PAYLOAD)
#define PROTOCOL_MAX_TX_FRAME               (2U + 2U + PROTOCOL_MAX_BODY_LENGTH + 2U)

/** 接收状态机。 */
typedef enum
{
  RX_WAIT_HEADER_1 = 0,
  RX_WAIT_HEADER_2,
  RX_WAIT_LENGTH_L,
  RX_WAIT_LENGTH_H,
  RX_WAIT_BODY,
  RX_WAIT_CRC_L,
  RX_WAIT_CRC_H
} protocol_rx_state_t;

static protocol_rx_state_t s_rx_state;
static uint16_t s_rx_length;
static uint16_t s_rx_index;
static uint16_t s_rx_crc_calc;
static uint16_t s_rx_crc_received;
static uint8_t s_rx_body[PROTOCOL_MAX_BODY_LENGTH];

static uint16_t protocol_crc16_update(uint16_t crc, uint8_t data)
{
  uint8_t bit;

  crc ^= data;
  for(bit = 0U; bit < 8U; bit++)
  {
    if((crc & 0x0001U) != 0U)
    {
      crc = (uint16_t)((crc >> 1) ^ 0xA001U);
    }
    else
    {
      crc >>= 1;
    }
  }
  return crc;
}

static uint16_t protocol_crc16(const uint8_t *data, uint16_t length)
{
  uint16_t crc;
  uint16_t index;

  crc = 0xFFFFU;
  for(index = 0U; index < length; index++)
  {
    crc = protocol_crc16_update(crc, data[index]);
  }
  return crc;
}

static void protocol_put_u16_le(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value & 0xFFU);
  data[1] = (uint8_t)(value >> 8);
}

static void protocol_put_u32_le(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)(value & 0xFFUL);
  data[1] = (uint8_t)((value >> 8) & 0xFFUL);
  data[2] = (uint8_t)((value >> 16) & 0xFFUL);
  data[3] = (uint8_t)((value >> 24) & 0xFFUL);
}

static uint16_t protocol_get_u16_le(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

static void protocol_send_frame(uint8_t sequence,
                                uint8_t command,
                                const uint8_t *payload,
                                uint16_t payload_length)
{
  uint8_t frame[PROTOCOL_MAX_TX_FRAME];
  uint16_t body_length;
  uint16_t frame_length;
  uint16_t crc;
  uint16_t index;

  if(payload_length > PROTOCOL_MAX_PAYLOAD)
  {
    return;
  }

  body_length = (uint16_t)(PROTOCOL_MIN_BODY_LENGTH + payload_length);
  frame[0] = PROTOCOL_HEADER_1;
  frame[1] = PROTOCOL_HEADER_2;
  protocol_put_u16_le(&frame[2], body_length);
  frame[4] = sequence;
  frame[5] = command;
  for(index = 0U; index < payload_length; index++)
  {
    frame[6U + index] = payload[index];
  }

  /** CRC 覆盖 LEN_L、LEN_H、SEQ、CMD 和 payload，不包含帧头及 CRC 自身。 */
  crc = protocol_crc16(&frame[2], (uint16_t)(2U + body_length));
  frame[6U + payload_length] = (uint8_t)(crc & 0xFFU);
  frame[7U + payload_length] = (uint8_t)(crc >> 8);
  frame_length = (uint16_t)(8U + payload_length);
  hw_uart_write(frame, frame_length);
}

static void protocol_send_status_only(uint8_t sequence, uint8_t command, protocol_status_t status)
{
  uint8_t payload[1];

  payload[0] = (uint8_t)status;
  protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), payload, 1U);
}

static protocol_status_t protocol_output_error_status(void)
{
  const eload_status_t *status;

  status = eload_control_get_status();
  if(status->fault_latched != 0U)
  {
    return PROTOCOL_STATUS_FAULT_LATCHED;
  }
  if(status->temperature_valid == 0U)
  {
    return PROTOCOL_STATUS_TEMP_INVALID;
  }
  return PROTOCOL_STATUS_IO_ERROR;
}

static void protocol_handle_command(uint8_t sequence,
                                    uint8_t command,
                                    const uint8_t *payload,
                                    uint16_t payload_length)
{
  uint8_t response[PROTOCOL_MAX_PAYLOAD];
  uint16_t current_ma;
  const eload_status_t *status;

  switch(command)
  {
    case PROTOCOL_CMD_PING:
      if(payload_length != 0U)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      response[0] = PROTOCOL_STATUS_OK;
      response[1] = PROTOCOL_VERSION;
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 2U);
      break;

    case PROTOCOL_CMD_INFO:
      if(payload_length != 0U)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      response[0] = PROTOCOL_STATUS_OK;
      response[1] = PROTOCOL_VERSION;
      response[2] = PROTOCOL_FW_MAJOR;
      response[3] = PROTOCOL_FW_MINOR;
      response[4] = PROTOCOL_FW_PATCH;
      protocol_put_u16_le(&response[5], ELOAD_LIMIT_CURRENT_MA);
      protocol_put_u16_le(&response[7], ELOAD_LIMIT_VOLTAGE_MV);
      protocol_put_u32_le(&response[9], ELOAD_LIMIT_POWER_CONT_MW);
      protocol_put_u32_le(&response[13], ELOAD_LIMIT_POWER_ABS_MW);
      protocol_put_u32_le(&response[17], (uint32_t)ELOAD_TEMP_TRIP_MC);
      protocol_put_u32_le(&response[21], (uint32_t)ELOAD_TEMP_CLEAR_MC);
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 25U);
      break;

    case PROTOCOL_CMD_STATUS:
      if(payload_length != 0U)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      status = eload_control_get_status();
      response[0] = PROTOCOL_STATUS_OK;
      response[1] = (uint8_t)status->state;
      response[2] = status->output_enabled;
      response[3] = status->temperature_valid;
      response[4] = status->fault_latched;
      protocol_put_u16_le(&response[5], (uint16_t)status->set_current_ma);
      protocol_put_u16_le(&response[7], (uint16_t)status->measured_current_ma);
      protocol_put_u16_le(&response[9], (uint16_t)status->measured_voltage_mv);
      protocol_put_u32_le(&response[11], status->measured_power_mw);
      protocol_put_u32_le(&response[15], (uint32_t)status->temperature_mc);
      protocol_put_u32_le(&response[19], status->fault_bitmap);
      protocol_put_u32_le(&response[23], hw_uart_get_rx_overflow_count());
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 27U);
      break;

    case PROTOCOL_CMD_SET_CURRENT:
      if(payload_length != 2U)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      current_ma = protocol_get_u16_le(payload);
      if(current_ma > ELOAD_LIMIT_CURRENT_MA)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_RANGE);
        return;
      }
      if(eload_control_set_current_ma(current_ma) != SUCCESS)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_IO_ERROR);
        return;
      }
      response[0] = PROTOCOL_STATUS_OK;
      protocol_put_u16_le(&response[1], current_ma);
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 3U);
      break;

    case PROTOCOL_CMD_SET_OUTPUT:
      if((payload_length != 1U) || (payload[0] > 1U))
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      if(eload_control_output_enable(payload[0]) != SUCCESS)
      {
        protocol_send_status_only(sequence, command, protocol_output_error_status());
        return;
      }
      response[0] = PROTOCOL_STATUS_OK;
      response[1] = payload[0];
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 2U);
      break;

    case PROTOCOL_CMD_CLEAR_FAULT:
      if(payload_length != 0U)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_LENGTH);
        return;
      }
      if(eload_control_clear_fault() != SUCCESS)
      {
        protocol_send_status_only(sequence, command, PROTOCOL_STATUS_FAULT_ACTIVE);
        return;
      }
      response[0] = PROTOCOL_STATUS_OK;
      protocol_put_u32_le(&response[1], 0UL);
      protocol_send_frame(sequence, (uint8_t)(command | PROTOCOL_CMD_RESPONSE_MASK), response, 5U);
      break;

    default:
      protocol_send_status_only(sequence, command, PROTOCOL_STATUS_BAD_COMMAND);
      break;
  }
}

static void protocol_reset_rx(void)
{
  s_rx_state = RX_WAIT_HEADER_1;
  s_rx_length = 0U;
  s_rx_index = 0U;
  s_rx_crc_calc = 0xFFFFU;
  s_rx_crc_received = 0U;
}

static void protocol_feed_byte(uint8_t value)
{
  switch(s_rx_state)
  {
    case RX_WAIT_HEADER_1:
      if(value == PROTOCOL_HEADER_1)
      {
        s_rx_state = RX_WAIT_HEADER_2;
      }
      break;

    case RX_WAIT_HEADER_2:
      if(value == PROTOCOL_HEADER_2)
      {
        s_rx_state = RX_WAIT_LENGTH_L;
        s_rx_crc_calc = 0xFFFFU;
      }
      else if(value != PROTOCOL_HEADER_1)
      {
        s_rx_state = RX_WAIT_HEADER_1;
      }
      break;

    case RX_WAIT_LENGTH_L:
      s_rx_length = value;
      s_rx_crc_calc = protocol_crc16_update(s_rx_crc_calc, value);
      s_rx_state = RX_WAIT_LENGTH_H;
      break;

    case RX_WAIT_LENGTH_H:
      s_rx_length |= (uint16_t)((uint16_t)value << 8);
      s_rx_crc_calc = protocol_crc16_update(s_rx_crc_calc, value);
      if((s_rx_length < PROTOCOL_MIN_BODY_LENGTH) ||
         (s_rx_length > PROTOCOL_MAX_BODY_LENGTH))
      {
        protocol_reset_rx();
      }
      else
      {
        s_rx_index = 0U;
        s_rx_state = RX_WAIT_BODY;
      }
      break;

    case RX_WAIT_BODY:
      s_rx_body[s_rx_index++] = value;
      s_rx_crc_calc = protocol_crc16_update(s_rx_crc_calc, value);
      if(s_rx_index >= s_rx_length)
      {
        s_rx_state = RX_WAIT_CRC_L;
      }
      break;

    case RX_WAIT_CRC_L:
      s_rx_crc_received = value;
      s_rx_state = RX_WAIT_CRC_H;
      break;

    case RX_WAIT_CRC_H:
      s_rx_crc_received |= (uint16_t)((uint16_t)value << 8);
      if(s_rx_crc_received == s_rx_crc_calc)
      {
        protocol_handle_command(s_rx_body[0],
                                s_rx_body[1],
                                &s_rx_body[2],
                                (uint16_t)(s_rx_length - PROTOCOL_MIN_BODY_LENGTH));
      }
      protocol_reset_rx();
      break;

    default:
      protocol_reset_rx();
      break;
  }
}

void protocol_cmd_init(void)
{
  /** 二进制协议启动时不发送 ASCII 文本，避免污染上位机帧解析器。 */
  protocol_reset_rx();
}

void protocol_cmd_poll(void)
{
  uint8_t value;

  while(hw_uart_getc_nonblock(&value) != 0U)
  {
    protocol_feed_byte(value);
  }
}
