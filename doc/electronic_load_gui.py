"""电子负载 PySide6 串口控制工具。

依赖安装：
    pip install PySide6

固件串口参数：115200, 8N1，无流控。
"""

from __future__ import annotations

import re
import sys
import time
from dataclasses import dataclass

from PySide6.QtCore import QIODevice, QTimer, Qt
from PySide6.QtGui import QFont
from PySide6.QtSerialPort import QSerialPort, QSerialPortInfo
from eload_binary_protocol import (
    CMD_CLEAR_FAULT,
    CMD_INFO,
    CMD_PING,
    CMD_SET_CURRENT,
    CMD_SET_OUTPUT,
    CMD_STATUS,
    COMMAND_NAMES,
    RESPONSE_MASK,
    STATUS_NAMES,
    STATUS_OK,
    Frame,
    FrameParser,
    decode_info,
    decode_status,
    encode_frame,
    pack_u16,
)
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSizePolicy,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)


FAULT_NAMES = {
    0: "硬件反接/低压",
    1: "硬件过压",
    2: "软件过压",
    3: "软件过流",
    4: "功率超限",
    5: "过温",
    6: "温度传感器异常",
    7: "DAC通信异常",
}


APP_STYLE = """
QMainWindow {
    background: #E9EEF3;
}
QWidget {
    color: #1F2A36;
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI";
    font-size: 10pt;
}
QGroupBox {
    background: #F7FAFC;
    border: 1px solid #B8C6D4;
    border-radius: 8px;
    margin-top: 13px;
    padding: 12px 10px 10px 10px;
    font-weight: 600;
    color: #245C8E;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 12px;
    padding: 0 6px;
    color: #1F6097;
    background: #E9EEF3;
}
QLabel#titleLabel {
    color: #153B5C;
    font-size: 18pt;
    font-weight: 800;
}
QLabel#subtitleLabel {
    color: #5B7186;
    font-size: 9pt;
}
QLabel#connectionLabel {
    border-radius: 10px;
    padding: 4px 10px;
    background: #FFE8DD;
    color: #A04018;
    border: 1px solid #E2A58B;
    font-weight: 700;
}
QLabel#connectionLabel[connected="true"] {
    background: #DFF5E5;
    color: #1E7B3E;
    border: 1px solid #8DC79D;
}
QLabel#fieldValue {
    color: #24384A;
    font-weight: 700;
}
QLabel#faultLabel {
    color: #9A6500;
    font-weight: 700;
}
QComboBox, QSpinBox {
    background: #FFFFFF;
    border: 1px solid #AAB7C4;
    border-radius: 6px;
    padding: 5px 8px;
    color: #1F2A36;
    min-height: 22px;
}
QComboBox:focus, QSpinBox:focus {
    border: 1px solid #2A7DB8;
}
QComboBox:disabled, QSpinBox:disabled {
    color: #8A97A4;
    border-color: #CCD4DC;
    background: #F1F4F7;
}
QPushButton {
    background: #EDF3F8;
    border: 1px solid #9FB2C4;
    border-radius: 6px;
    padding: 6px 12px;
    color: #17324A;
    font-weight: 700;
}
QPushButton:hover {
    background: #E0ECF5;
    border-color: #5C91BD;
}
QPushButton:pressed {
    background: #D4E3EE;
}
QPushButton:disabled {
    background: #F1F3F5;
    color: #9AA5AF;
    border-color: #D1D7DD;
}
QPushButton#connectButton {
    background: #2878B5;
    border-color: #1E6394;
    color: #FFFFFF;
}
QPushButton#connectButton:hover {
    background: #3288C8;
}
QPushButton#outputButton {
    background: #FFF3E3;
    border-color: #D98A36;
    color: #9A4F00;
}
QPushButton#outputButton:checked {
    background: #E3F6E8;
    border-color: #36A764;
    color: #19713A;
}
QPushButton#clearFaultButton {
    background: #FFE9EC;
    border-color: #D66A78;
    color: #A52E3F;
}
QPlainTextEdit {
    background: #FFFFFF;
    border: 1px solid #B8C6D4;
    border-radius: 6px;
    color: #1E4E2F;
    font-family: "Consolas", "Cascadia Mono", monospace;
    font-size: 9pt;
    padding: 6px;
}
"""


@dataclass
class PendingRequest:
    command: int
    description: str
    sent_at: float


class ValuePanel(QGroupBox):
    """电子仪表风格的大号数值显示面板。"""

    def __init__(self, title: str, value: str = "--", unit: str = "") -> None:
        super().__init__(title)
        self.unit = unit
        self.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self.setMinimumHeight(92)

        self.value_label = QLabel(value)
        self.value_label.setObjectName("meterValue")
        value_font = QFont("Consolas")
        value_font.setPointSize(22)
        value_font.setBold(True)
        self.value_label.setFont(value_font)
        self.value_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        self.unit_label = QLabel(unit)
        self.unit_label.setObjectName("meterUnit")
        unit_font = QFont("Microsoft YaHei UI")
        unit_font.setPointSize(10)
        unit_font.setBold(True)
        self.unit_label.setFont(unit_font)
        self.unit_label.setAlignment(Qt.AlignmentFlag.AlignCenter)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 7, 8, 7)
        layout.setSpacing(0)
        layout.addStretch(1)
        layout.addWidget(self.value_label)
        layout.addWidget(self.unit_label)
        layout.addStretch(1)

        self.setStyleSheet(
            """
            ValuePanel {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                            stop:0 #FFFFFF, stop:1 #EDF5FB);
                border: 1px solid #9DB8CF;
                border-radius: 9px;
            }
            QLabel#meterValue {
                color: #126B45;
                letter-spacing: 1px;
            }
            QLabel#meterUnit {
                color: #60798E;
            }
            """
        )

    def set_value(self, value: str) -> None:
        # 新界面中单位单独显示，所以这里兼容旧调用，把字符串中的单位剥掉。
        if self.unit and value.endswith(self.unit):
            value = value[: -len(self.unit)].strip()
        self.value_label.setText(value)


class ElectronicLoadWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("电子负载控制器 3A / 12V")
        self.resize(900, 580)
        self.setMinimumSize(820, 520)

        self.serial = QSerialPort(self)
        self.serial.readyRead.connect(self._on_ready_read)
        self.serial.errorOccurred.connect(self._on_serial_error)

        self.poll_timer = QTimer(self)
        self.poll_timer.setInterval(250)
        self.poll_timer.timeout.connect(self._poll_status)

        self.timeout_timer = QTimer(self)
        self.timeout_timer.setInterval(500)
        self.timeout_timer.timeout.connect(self._check_timeouts)
        self.timeout_timer.start()

        self.frame_parser = FrameParser()
        self.request_counter = 0
        self.pending: dict[int, PendingRequest] = {}
        self.status_request_id: int | None = None

        self._build_ui()
        self._refresh_ports()
        self._set_connected(False)

    def _build_ui(self) -> None:
        central = QWidget()
        central.setObjectName("centralWidget")
        root = QVBoxLayout(central)
        root.setContentsMargins(14, 10, 14, 10)
        root.setSpacing(7)

        header = QHBoxLayout()
        title_box = QVBoxLayout()
        title_box.setSpacing(2)
        title_label = QLabel("电子负载控制器")
        title_label.setObjectName("titleLabel")
        subtitle_label = QLabel("3A / 12V · 115200 8N1")
        subtitle_label.setObjectName("subtitleLabel")
        title_box.addWidget(title_label)
        title_box.addWidget(subtitle_label)
        header.addLayout(title_box, 1)
        self.connection_label = QLabel("未连接")
        self.connection_label.setObjectName("connectionLabel")
        self.connection_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        header.addWidget(self.connection_label)
        root.addLayout(header)

        connection_group = QGroupBox("串口连接")
        connection_layout = QHBoxLayout(connection_group)
        connection_layout.setContentsMargins(10, 10, 10, 10)
        connection_layout.setSpacing(8)
        self.port_combo = QComboBox()
        self.refresh_button = QPushButton("刷新")
        self.connect_button = QPushButton("连接")
        self.connect_button.setObjectName("connectButton")
        connection_layout.addWidget(QLabel("端口"))
        connection_layout.addWidget(self.port_combo, 1)
        connection_layout.addWidget(self.refresh_button)
        connection_layout.addWidget(self.connect_button)
        root.addWidget(connection_group)

        readings = QGridLayout()
        readings.setHorizontalSpacing(8)
        readings.setVerticalSpacing(8)
        self.voltage_panel = ValuePanel("负载电压", "--", "V")
        self.current_panel = ValuePanel("负载电流", "--", "A")
        self.power_panel = ValuePanel("负载功率", "--", "W")
        self.temperature_panel = ValuePanel("MOS 区域温度", "--", "°C")
        readings.addWidget(self.voltage_panel, 0, 0)
        readings.addWidget(self.current_panel, 0, 1)
        readings.addWidget(self.power_panel, 0, 2)
        readings.addWidget(self.temperature_panel, 0, 3)
        root.addLayout(readings)

        main_area = QHBoxLayout()
        main_area.setSpacing(8)

        left_column = QVBoxLayout()
        left_column.setSpacing(8)

        control_group = QGroupBox("输出控制")
        control_layout = QGridLayout(control_group)
        control_layout.setContentsMargins(10, 12, 10, 10)
        control_layout.setHorizontalSpacing(8)
        control_layout.setVerticalSpacing(8)
        self.current_spin = QSpinBox()
        self.current_spin.setRange(0, 3000)
        self.current_spin.setSuffix(" mA")
        self.current_spin.setSingleStep(10)
        self.current_spin.setMinimumWidth(130)
        self.set_current_button = QPushButton("设置")
        self.output_button = QPushButton("输出关闭")
        self.output_button.setObjectName("outputButton")
        self.output_button.setCheckable(True)
        self.clear_fault_button = QPushButton("清除故障")
        self.clear_fault_button.setObjectName("clearFaultButton")
        control_layout.addWidget(QLabel("目标电流"), 0, 0)
        control_layout.addWidget(self.current_spin, 0, 1)
        control_layout.addWidget(self.set_current_button, 0, 2)
        control_layout.addWidget(self.output_button, 1, 0, 1, 2)
        control_layout.addWidget(self.clear_fault_button, 1, 2)
        left_column.addWidget(control_group)

        state_group = QGroupBox("设备状态")
        state_layout = QFormLayout(state_group)
        state_layout.setContentsMargins(10, 12, 10, 10)
        state_layout.setSpacing(7)
        self.state_label = QLabel("--")
        self.setpoint_label = QLabel("--")
        self.fault_label = QLabel("--")
        self.device_info_label = QLabel("--")
        for label in (self.state_label, self.setpoint_label, self.device_info_label):
            label.setObjectName("fieldValue")
        self.fault_label.setObjectName("faultLabel")
        self.fault_label.setWordWrap(True)
        state_layout.addRow("运行状态", self.state_label)
        state_layout.addRow("设备设定", self.setpoint_label)
        state_layout.addRow("故障信息", self.fault_label)
        state_layout.addRow("设备信息", self.device_info_label)
        left_column.addWidget(state_group)
        left_column.addStretch(1)

        log_group = QGroupBox("通信日志")
        log_layout = QVBoxLayout(log_group)
        log_layout.setContentsMargins(10, 12, 10, 10)
        self.log_edit = QPlainTextEdit()
        self.log_edit.setReadOnly(True)
        self.log_edit.document().setMaximumBlockCount(500)
        log_layout.addWidget(self.log_edit)

        main_area.addLayout(left_column, 0)
        main_area.addWidget(log_group, 1)
        root.addLayout(main_area, 1)

        self.refresh_button.clicked.connect(self._refresh_ports)
        self.connect_button.clicked.connect(self._toggle_connection)
        self.set_current_button.clicked.connect(self._set_current)
        self.output_button.clicked.connect(self._set_output)
        self.clear_fault_button.clicked.connect(lambda: self._send_command(CMD_CLEAR_FAULT, description="CLEAR_FAULT"))

        self.setCentralWidget(central)

    def _refresh_ports(self) -> None:
        selected = self.port_combo.currentData()
        self.port_combo.clear()
        for info in QSerialPortInfo.availablePorts():
            description = info.description() or "未知设备"
            self.port_combo.addItem(f"{info.portName()} - {description}", info.portName())
        if selected:
            index = self.port_combo.findData(selected)
            if index >= 0:
                self.port_combo.setCurrentIndex(index)

    def _toggle_connection(self) -> None:
        if self.serial.isOpen():
            self.poll_timer.stop()
            self.serial.close()
            self.pending.clear()
            self.status_request_id = None
            self._set_connected(False)
            self._log("串口已断开")
            return

        port_name = self.port_combo.currentData()
        if not port_name:
            QMessageBox.warning(self, "串口", "未找到可用串口。")
            return

        self.serial.setPortName(port_name)
        self.serial.setBaudRate(115200)
        self.serial.setDataBits(QSerialPort.DataBits.Data8)
        self.serial.setParity(QSerialPort.Parity.NoParity)
        self.serial.setStopBits(QSerialPort.StopBits.OneStop)
        self.serial.setFlowControl(QSerialPort.FlowControl.NoFlowControl)
        if not self.serial.open(QIODevice.OpenModeFlag.ReadWrite):
            QMessageBox.critical(self, "连接失败", self.serial.errorString())
            return

        self.frame_parser.reset()
        self.pending.clear()
        self._set_connected(True)
        self._log(f"已连接 {port_name} @ 115200")
        self._send_command(CMD_PING)
        self._send_command(CMD_INFO)
        self.poll_timer.start()

    def _set_connected(self, connected: bool) -> None:
        self.connect_button.setText("断开" if connected else "连接")
        self.connection_label.setText("已连接" if connected else "未连接")
        self.connection_label.setProperty("connected", "true" if connected else "false")
        self.connection_label.style().unpolish(self.connection_label)
        self.connection_label.style().polish(self.connection_label)
        self.port_combo.setEnabled(not connected)
        self.refresh_button.setEnabled(not connected)
        self.set_current_button.setEnabled(connected)
        self.output_button.setEnabled(connected)
        self.clear_fault_button.setEnabled(connected)

    def _next_request_id(self) -> int:
        for _ in range(255):
            self.request_counter = (self.request_counter % 255) + 1
            if self.request_counter not in self.pending:
                return self.request_counter
        raise RuntimeError("没有可用的协议序号")

    def _send_command(
        self, command: int, payload: bytes = b"", description: str | None = None
    ) -> int | None:
        if not self.serial.isOpen():
            return None
        sequence = self._next_request_id()
        frame = encode_frame(sequence, command, payload)
        name = description or COMMAND_NAMES.get(command, f"CMD_0x{command:02X}")
        self.pending[sequence] = PendingRequest(
            command=command, description=name, sent_at=time.monotonic()
        )
        self.serial.write(frame)
        self._log(f"> {name} [{frame.hex(' ').upper()}]")
        return sequence

    def _set_current(self) -> None:
        value = self.current_spin.value()
        self._send_command(
            CMD_SET_CURRENT, pack_u16(value), description=f"SET_CURRENT {value}mA"
        )

    def _set_output(self, checked: bool) -> None:
        self.output_button.setText("输出开启" if checked else "输出关闭")
        self._send_command(
            CMD_SET_OUTPUT,
            bytes((1 if checked else 0,)),
            description=f"SET_OUTPUT {1 if checked else 0}",
        )

    def _poll_status(self) -> None:
        if not self.serial.isOpen() or self.status_request_id is not None:
            return
        self.status_request_id = self._send_command(CMD_STATUS, description="STATUS")

    def _on_ready_read(self) -> None:
        data = bytes(self.serial.readAll())
        if not data:
            return
        for frame in self.frame_parser.feed(data):
            self._handle_frame(frame)

    def _handle_frame(self, frame: Frame) -> None:
        self._log(f"< [{frame.raw.hex(' ').upper()}]")
        pending = self.pending.pop(frame.sequence, None)
        if self.status_request_id == frame.sequence:
            self.status_request_id = None

        if pending is None:
            self._log(f"未匹配响应 SEQ={frame.sequence}")
            return

        expected_command = pending.command | RESPONSE_MASK
        if frame.command != expected_command:
            self._log(
                f"响应命令不匹配: 期望 0x{expected_command:02X}, "
                f"收到 0x{frame.command:02X}"
            )
            return
        if not frame.payload:
            self._log(f"{pending.description}: 空响应")
            return

        status = frame.payload[0]
        if status != STATUS_OK:
            self._log(
                f"设备拒绝 {pending.description}: "
                f"{STATUS_NAMES.get(status, f'0x{status:02X}')}"
            )
            if pending.command == CMD_SET_OUTPUT:
                self._poll_status()
            return

        try:
            if pending.command == CMD_INFO:
                info = decode_info(frame.payload)
                self.device_info_label.setText(
                    f"FW {info['firmware']} / 协议 {info['protocol']} / "
                    f"{info['max_current_ma']}mA / {info['max_voltage_mv']}mV"
                )
            elif pending.command == CMD_STATUS:
                self._update_status(decode_status(frame.payload))
            elif pending.command == CMD_SET_CURRENT:
                value = int.from_bytes(frame.payload[1:3], "little")
                self.setpoint_label.setText(f"{value} mA")
        except (ValueError, IndexError) as error:
            self._log(f"{pending.description} 响应解析失败: {error}")

    def _update_status(self, status: dict[str, int | str | bool]) -> None:
        voltage_mv = int(status["voltage_mv"])
        current_ma = int(status["current_ma"])
        power_mw = int(status["power_mw"])
        temperature_mc = int(status["temperature_mc"])
        temperature_valid = bool(status["temperature_valid"])
        set_ma = int(status["set_current_ma"])
        output_enabled = bool(status["output_enabled"])
        fault_bitmap = int(status["fault_bitmap"])

        self.voltage_panel.set_value(f"{voltage_mv / 1000:.3f}")
        self.current_panel.set_value(f"{current_ma / 1000:.3f}")
        self.power_panel.set_value(f"{power_mw / 1000:.2f}")
        self.temperature_panel.set_value(
            f"{temperature_mc / 1000:.1f}" if temperature_valid else "无效"
        )
        self.state_label.setText(str(status["state"]))
        self.setpoint_label.setText(f"{set_ma} mA")

        fault_names = [
            name for bit, name in FAULT_NAMES.items() if fault_bitmap & (1 << bit)
        ]
        self.fault_label.setText("无" if not fault_names else "、".join(fault_names))

        rx_overflow = int(status["rx_overflow_count"])
        if rx_overflow:
            self._log(f"MCU USART 接收缓冲溢出累计: {rx_overflow}")

        self.output_button.blockSignals(True)
        self.output_button.setChecked(output_enabled)
        self.output_button.setText("输出开启" if output_enabled else "输出关闭")
        self.output_button.blockSignals(False)

    def _check_timeouts(self) -> None:
        now = time.monotonic()
        expired = [
            sequence
            for sequence, item in self.pending.items()
            if now - item.sent_at > 2.0
        ]
        for sequence in expired:
            item = self.pending.pop(sequence)
            self._log(f"请求超时 SEQ={sequence}: {item.description}")
            if self.status_request_id == sequence:
                self.status_request_id = None

    def _on_serial_error(self, error: QSerialPort.SerialPortError) -> None:
        if error in (
            QSerialPort.SerialPortError.NoError,
            QSerialPort.SerialPortError.NotOpenError,
        ):
            return
        self._log(f"串口错误: {self.serial.errorString()}")
        if error == QSerialPort.SerialPortError.ResourceError:
            self.poll_timer.stop()
            self.serial.close()
            self._set_connected(False)

    def _log(self, text: str) -> None:
        self.log_edit.appendPlainText(text)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyleSheet(APP_STYLE)
    window = ElectronicLoadWindow()
    window.show()
    sys.exit(app.exec())
