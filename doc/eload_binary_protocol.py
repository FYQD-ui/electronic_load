"""电子负载二进制串口协议。

帧格式：
    55 AA | LEN_L LEN_H | SEQ | CMD | PAYLOAD | CRC_L CRC_H

LEN 为 SEQ、CMD 和 PAYLOAD 的总字节数。CRC16/MODBUS 的计算范围从
LEN_L 开始，到 PAYLOAD 末尾结束。所有多字节数值均为小端。
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

HEADER = b"\x55\xAA"
PROTOCOL_VERSION = 3
MAX_BODY_LENGTH = 50

CMD_PING = 0x01
CMD_INFO = 0x02
CMD_STATUS = 0x03
CMD_SET_CURRENT = 0x10
CMD_SET_OUTPUT = 0x11
CMD_CLEAR_FAULT = 0x12
RESPONSE_MASK = 0x80

STATUS_OK = 0x00
STATUS_NAMES = {
    0x00: "OK",
    0x01: "BAD_COMMAND",
    0x02: "BAD_LENGTH",
    0x03: "RANGE",
    0x04: "FAULT_LATCHED",
    0x05: "TEMP_INVALID",
    0x06: "FAULT_ACTIVE",
    0x07: "IO_ERROR",
}

COMMAND_NAMES = {
    CMD_PING: "PING",
    CMD_INFO: "INFO",
    CMD_STATUS: "STATUS",
    CMD_SET_CURRENT: "SET_CURRENT",
    CMD_SET_OUTPUT: "SET_OUTPUT",
    CMD_CLEAR_FAULT: "CLEAR_FAULT",
}

STATE_NAMES = {
    0: "OFF",
    1: "IDLE",
    2: "CC",
    3: "FAULT",
}


@dataclass(frozen=True)
class Frame:
    sequence: int
    command: int
    payload: bytes
    raw: bytes


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc & 0xFFFF


def encode_frame(sequence: int, command: int, payload: bytes = b"") -> bytes:
    if not 0 <= sequence <= 0xFF:
        raise ValueError("sequence must fit in one byte")
    if not 0 <= command <= 0xFF:
        raise ValueError("command must fit in one byte")
    body = bytes((sequence, command)) + payload
    if not 2 <= len(body) <= MAX_BODY_LENGTH:
        raise ValueError("body length out of range")
    length = struct.pack("<H", len(body))
    crc = crc16_modbus(length + body)
    return HEADER + length + body + struct.pack("<H", crc)


class FrameParser:
    def __init__(self) -> None:
        self.buffer = bytearray()
        self.crc_error_count = 0
        self.length_error_count = 0

    def reset(self) -> None:
        self.buffer.clear()

    def feed(self, data: bytes) -> list[Frame]:
        self.buffer.extend(data)
        frames: list[Frame] = []

        while True:
            header_index = self.buffer.find(HEADER)
            if header_index < 0:
                if self.buffer[-1:] == HEADER[:1]:
                    self.buffer[:] = self.buffer[-1:]
                else:
                    self.buffer.clear()
                break
            if header_index:
                del self.buffer[:header_index]
            if len(self.buffer) < 4:
                break

            body_length = struct.unpack_from("<H", self.buffer, 2)[0]
            if not 2 <= body_length <= MAX_BODY_LENGTH:
                self.length_error_count += 1
                del self.buffer[0]
                continue

            frame_length = 2 + 2 + body_length + 2
            if len(self.buffer) < frame_length:
                break

            raw = bytes(self.buffer[:frame_length])
            crc_received = struct.unpack_from("<H", raw, frame_length - 2)[0]
            crc_calculated = crc16_modbus(raw[2:-2])
            if crc_received != crc_calculated:
                self.crc_error_count += 1
                del self.buffer[0]
                continue

            del self.buffer[:frame_length]
            frames.append(Frame(raw[4], raw[5], raw[6:-2], raw))

        return frames


def pack_u16(value: int) -> bytes:
    return struct.pack("<H", value)


def decode_info(payload: bytes) -> dict[str, int | str]:
    if len(payload) != 25:
        raise ValueError(f"INFO payload length {len(payload)} != 25")
    values = struct.unpack("<BBBBBHHIIii", payload)
    status, proto, fw_major, fw_minor, fw_patch, max_i, max_v, cont_p, abs_p, trip_t, clear_t = values
    return {
        "status": status,
        "protocol": proto,
        "firmware": f"{fw_major}.{fw_minor}.{fw_patch}",
        "max_current_ma": max_i,
        "max_voltage_mv": max_v,
        "continuous_power_mw": cont_p,
        "absolute_power_mw": abs_p,
        "trip_temperature_mc": trip_t,
        "clear_temperature_mc": clear_t,
    }


def decode_status(payload: bytes) -> dict[str, int | str | bool]:
    if len(payload) != 27:
        raise ValueError(f"STATUS payload length {len(payload)} != 27")
    values = struct.unpack("<BBBBBHHHIiII", payload)
    status, state, output, temp_valid, fault_latched, set_ma, current_ma, voltage_mv, power_mw, temp_mc, fault, rx_overflow = values
    return {
        "status": status,
        "state": STATE_NAMES.get(state, f"UNKNOWN({state})"),
        "state_code": state,
        "output_enabled": bool(output),
        "temperature_valid": bool(temp_valid),
        "fault_latched": bool(fault_latched),
        "set_current_ma": set_ma,
        "current_ma": current_ma,
        "voltage_mv": voltage_mv,
        "power_mw": power_mw,
        "temperature_mc": temp_mc,
        "fault_bitmap": fault,
        "rx_overflow_count": rx_overflow,
    }
