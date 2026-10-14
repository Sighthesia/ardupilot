# AM32 KISS TTL 移植变更记录

更新时间：2026-10-14

本文记录当前 ArduPilot 分支中为 CYT4BB7CEBQ0AESGS / UluluDriveDayun
目标板增加 AM32 KISS TTL 电调广播所做的实际修改。内容以当前 Git
提交和工作区代码为准，不把设计计划或未完成验证写成已完成代码。

## 1. 变更范围

相对 AM32 KISS TTL 设计基线 `6764d07429`，当前 ArduPilot 分支包含以下
功能提交：

| 提交 | 内容 |
| --- | --- |
| `36b1625ea3` | 新增 `AP_AM32Kiss` 协议辅助库 |
| `c883a36d26` | 注册 `SerialProtocol_AM32Kiss` 并接入输出周期 |
| `b17f2e744a` | 将 UluluDriveDayun 的现有 SCB3 绑定为 AM32 TTL 总线 |
| `f6dd7eb39d` | 澄清 U16 未装配状态的证据边界 |
| `1fc241b9cd` | 修正 UART 初始化、PWM/数字输出冲突和设计文档 |
| `3c7f81effc` | 添加跨仓库验证报告 |

此外，提交 `c2836a6927` 添加了参考网表文件
`docs/ref/Netlist_2026-10-11.txt`。该文件是硬件参考资料，不是 AM32
驱动实现的一部分。

## 2. AM32 KISS 帧协议

ArduPilot 端在 `libraries/AP_AM32Kiss/` 实现了 11 字节、6 槽位的控制帧：

```text
byte 0    : 0x0F header
byte 1..6 : slot[0..5]
byte 7    : shared throttle，固定为 0
byte 8    : rolling counter
byte 9    : XOR = byte7 ^ byte8
byte 10   : CRC8
```

协议细节：

- 电调地址 `1..6` 对应 ArduPilot 的 `slot[address - 1]`。
- PWM `1000..2000` 映射为单字节 `0..255`，超出范围先钳位。
- CRC8 为 MSB-first、初值 `0x00`、多项式 `0x07`，覆盖 byte `0..9`。
- 每次完整写入 11 字节后 counter 才递增。
- 未解锁、急停或没有有效电机输出时，6 个槽位均发送零。
- UART TX 空间不足一个完整帧时丢弃本周期发送，不发送半帧。

对应实现和测试：

- `libraries/AP_AM32Kiss/AP_AM32Kiss.h`
- `libraries/AP_AM32Kiss/AP_AM32Kiss.cpp`
- `libraries/AP_AM32Kiss/tests/test_am32_kiss.cpp`
- `libraries/AP_AM32Kiss/tests/wscript`

## 3. ArduPilot 串口协议注册

在 `libraries/AP_SerialManager/` 中新增：

- `SerialProtocol_AM32Kiss = 51`，追加在现有 `SerialProtocol_IOMCU = 50`
  之后，未改变已有枚举值。
- AM32 KISS 串口 buffer 配置：RX `16`、TX `64`。
- 串口初始化分支使用配置波特率和 `FLOW_CONTROL_DISABLE`。
- 默认协议 gate 检查已包含 `AP_AM32Kiss_config.h`。
- `AP_AM32KISS_ENABLED` 当前默认由 `HAL_BOARD_INFINEON` 控制。
- `AP_AM32KISS_SERIAL_PROTOCOL_ENABLED` 默认启用协议生命周期。

涉及文件：

- `libraries/AP_SerialManager/AP_SerialManager.h`
- `libraries/AP_SerialManager/AP_SerialManager.cpp`
- `libraries/AP_SerialManager/AP_SerialManager_config.h`
- `libraries/AP_SerialManager/AP_SerialManager_default_protocol_check.cpp`
- `libraries/AP_AM32Kiss/AP_AM32Kiss_config.h`

## 4. 输出生命周期和物理 PWM 屏蔽

`SRV_Channels::push()` 在底层 `hal.rcout->push()` 后调用：

```cpp
am32kiss.update();
```

`AP_AM32Kiss::update()` 首次找到配置为 `SerialProtocol_AM32Kiss` 的 UART
后执行一次初始化：

1. 使用 `AP_SerialManager` 查找 AM32 UART 和波特率。
2. 先调用 `begin()`，再禁用硬件流控。
3. 遍历 6 个逻辑电机函数，通过
   `SRV_Channels::get_output_channel_mask()` 汇总实际物理输出通道。
4. 调用 `SRV_Channels::set_digital_outputs(mask, 0)`，避免这些物理通道
   继续由 PWM 后端同时驱动。

这不是把逻辑电机编号直接当成物理通道，而是通过 `SERVOx_FUNCTION`
映射得到实际 channel mask。AM32 驱动未使用 RS485 DE/RE、半双工方向控制、
CAN 或 `FLOW_CONTROL_RTS_DE`。

涉及文件：

- `libraries/SRV_Channel/SRV_Channel.h`
- `libraries/SRV_Channel/SRV_Channels.cpp`
- `libraries/AP_AM32Kiss/AP_AM32Kiss.cpp`

## 5. UluluDriveDayun 板级配置

没有新增 UART、SCB 或 DMA。`libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/hwdef.dat`
复用原有 UART id 3：

| 项目 | 配置 |
| --- | --- |
| 默认协议 | `SerialProtocol_AM32Kiss` |
| 默认波特率 | `115200` |
| 外设 | `SCB3` |
| TX | `P13.1`，网表标号 `U1.75`，ESC 总线 `A` |
| RX | `P13.0`，网表标号 `U1.74`，ESC 总线 `B` |
| DMA RX/TX | 既有 DMA `23/22` |
| 电气链路 | 直接 TTL，不经过 RS485 收发器 |

板级说明位于：

- `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/README.md`
- `docs/ref/Netlist_2026-10-11.txt`

网表文本包含 U16 SIT3088ETK 的 footprint 和连接信息；“目标装配不安装
U16”来自实际装配/BOM 确认，不能声称由网表文本单独证明。CANH/CANL
仍是独立网络，没有用于本 AM32 ESC 链路。

## 6. 测试与验证状态

已通过：

- AM32 `python3 tools/verify-esc-protocol.py`
- AM32 `python3 tools/verify-esc-build-contract.py --selftest`
- AM32 `tools/build-app.sh all`，六个目标均构建成功，0 errors
- `python3 Tools/scripts/check_serial_protocol_gates.py`
- `./waf configure --board sitl`
- `git diff --check`
- 最终代码审查和最终修复 scoped re-review

当前环境阻塞：

- `./waf --targets tests/test_am32_kiss`：缺少 `empy==3.3.4`。
- `./waf configure --board UluluDriveDayun`：缺少
  `modules/infineon/TARGET_CYT4BB7/config/GeneratedSource`。

尚未完成的硬件验证：

- 实际板上 TTL 电平和 TX/RX 波形确认。
- 六个电调同时启动、停止、失联和复位测试。
- 高负载下的帧完整性、计数器和 CRC 验证。
- 真实电机输出与 ArduPilot 解锁/急停状态的联动验证。
- 目标板完整 Infineon 工具链构建和烧录验证。

详细验证记录：

- `.superpowers/sdd/2026-10-12-am32-kiss-ttl-implementation/task-6-report.md`
- `.superpowers/sdd/2026-10-12-am32-kiss-ttl-implementation/final-fix-report.md`

## 7. 当前不包含的功能

本次 ArduPilot 改动只覆盖 AM32 KISS TTL 电调广播，不自动增加以下设备
的驱动或板级分配：

- ELRS/CRSF 接收机
- Wi-Fi 网络模块
- 摄像头视频和图像处理
- 自定义上位机协议
- 网表中其他私有 UART、SPI、I2C、CAN 外设

这些模块后续需要按实际接口、电平、DMA、协议和安全需求分别配置。网络、
视频和 AI 类功能通常应放在伴随计算机，通过 MAVLink 与 ArduPilot 对接；
实时姿态、失控保护和电机安全逻辑仍应由 ArduPilot 负责。
