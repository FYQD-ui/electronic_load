# Electronic Load Debug Log

## 2026-06-24：负载无法启动，Q4 被错误的反接保护信号持续导通

### 故障现象

- 上位机控制、状态显示和输入电压 ADC 采样正常。
- MCP4725 软件驱动修正后能够正常设置 DAC。
- 被测电源接入后，功率 MOSFET Q1/Q2 不导通，负载电流始终为 0。

### 现场测量

在已设置电流并打开输出的状态下，以板上 GND 为参考测得：

| 测量点 | 电压 |
| --- | ---: |
| LM358 U1 pin 1，`GATE_CTRL` | 63 mV |
| LM358 U1 pin 2，`ISENSE` | 0 V |
| LM358 U1 pin 3，`I_SET` | 49 mV |
| LM358 U1 pin 4，GND | 0 V |
| LM358 U1 pin 8，`+12V` | 12 V |
| Q4 pin 1，`FAULT_KILL` | 2.63 V |
| Q1 pin 1，Gate | 62 mV |
| Q1 pin 2，Drain/`LOAD+` | 5.5 V |
| Q1 pin 3，Source/`ISENSE` | 0 V |

LM358 供电正常，且 `I_SET=49mV > ISENSE=0V`，恒流环本应提高 U1 pin 1 电压并驱动 Q1/Q2。但是 Q4 的栅极为 2.63V，2N7002 已导通，将 `GATE_CTRL` 强制钳位到约 0V，导致功率 MOSFET 无法开启。

### 根本原因

LM393 U13 第一通道原连接关系为：

```text
pin 3（V+）= LOAD_CMP_SENSE
pin 2（V-）= 约 33mV 参考
```

正常接入正电压时，`LOAD_CMP_SENSE > 33mV`，LM393 开集电极输出晶体管关闭，`REV_FAULT`被上拉为高电平。

保护链路采用高有效关断逻辑：

```text
REV_FAULT 高
    -> D4
    -> FAULT_KILL 高
    -> Q4 导通
    -> GATE_CTRL 被拉低
    -> Q1/Q2 关闭
```

因此第一路比较器输出的定义是“正常为高、故障为低”，而 D4、D6、Q4 组成的硬件关断链要求“故障为高”。两部分有效极性相反，造成正常输入时反而持续触发硬件关断。

实测 `FAULT_KILL=2.63V`与`REV_FAULT`上拉电压经过 D4 正向压降后的结果一致。

### 解决办法

交换 LM393 U13 第一通道的正、负输入：

```text
pin 3（V+）= 约 33mV 参考
pin 2（V-）= LOAD_CMP_SENSE
```

修改后：

- 正常正电压输入：`LOAD_CMP_SENSE > 33mV`，`REV_FAULT`输出低，Q4关闭。
- 输入过低、掉线或反向异常：`LOAD_CMP_SENSE < 33mV`，`REV_FAULT`输出高，Q4导通并关断功率 MOSFET。
- `REV_FAULT`与`OV_FAULT`统一为高电平故障有效，可以继续通过 D4、D6进行硬件“或”关断。

软件同步修改：

```c
#define ELOAD_REV_FAULT_ACTIVE_LEVEL       (1U)
```

配置位置：`project/inc/eload_config.h`。

### 验证结果

完成 LM393 第一通道输入交换并将软件故障有效电平改为高有效后：

- Q4不再在正常输入状态下钳位`GATE_CTRL`。
- LM358能够驱动Q1/Q2。
- 采样电阻R9产生电流反馈电压。
- 电子负载恒流功能恢复正常。

### 后续设计注意事项

- LM393为开集电极输出，分析逻辑时必须同时考虑输入极性、外部上拉和后级有效电平。
- 接入二极管“或”保护链的所有故障信号必须统一为高有效。
- 调试“软件正常但功率级不工作”时，优先沿以下链路逐点测量：

```text
DAC_OUT -> I_SET -> LM358输入 -> GATE_CTRL
        -> Q4硬件钳位 -> MOS Gate -> ISENSE -> R9
```
