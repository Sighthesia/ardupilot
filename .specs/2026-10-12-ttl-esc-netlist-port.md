# Spec Contract: Netlist TTL ESC Broadcast Port

- **Created**: 2026-10-12
- **Status**: In Progress
- **Target Files/Modules**: `docs/ref/Netlist_2026-10-11.txt`, `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/hwdef.dat`, `libraries/AP_HAL_Infineon/`, `libraries/AP_SerialManager/`, AM32/KISS ESC protocol integration, board defaults and focused tests; protocol source: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`

## 1. Core Objective

移植当前 ArduPilot 工程，使目标 CYT4BB7CEBQ0AESGS 板按网表运行，并让电调通过 TTL UART 广播通信；RS485 和 CAN 不作为电调通信链路启用。

## 2. Hard Constraints & Negative Rules (Non-negotiables)

- [x] **C-01**: 电调通信不得依赖 RS485 协议栈、RS485 收发器方向控制或 CAN/DroneCAN/PiccoloCAN 电调链路。
- [ ] **C-02**: 必须根据 `docs/ref/Netlist_2026-10-11.txt` 验证 MCU 引脚、连接器和电源关系，不能凭空替换引脚。
- [x] **C-03**: `U16 SIT3088ETK` 不装配；`A/B` 可由 `U1.75/U1.74` 直接作为 TTL 信号使用，且不得启用任何 U16 方向控制路径。
- [ ] **C-04**: 共享 HAL 代码必须使用 ArduPilot HAL 接口，不得引入平台外设调用到共享库。
- [ ] **C-05**: 现有参数索引、已有公共协议行为和无关车辆功能不得被破坏。
- [x] **C-06**: 电调固件使用 115200 8N1 的 KISS 控制串口，扩展控制帧为 `0x0F + 6 个槽位 + throttle + counter + XOR + CRC8`。
- [x] **C-07**: 地址 `1~6` 分别映射到 `slot[0]~slot[5]`，总帧长度为 11 字节；AM32 与 ArduPilot 同步采用该定义。
- [x] **C-08**: AM32 节点使用自身地址对应的 `slot[address-1]` 作为实际油门；公共 `throttle` 字段保留并固定由 ArduPilot 发送为 `0`，仅参与 XOR/CRC 校验，不参与电机控制。

## 3. Atomized Acceptance Criteria (Checklist)

- [x] **AC-01**: ESC TTL 广播复用 `A/B` 网络；`A=U1.75`、`B=U1.74`，ESC1/2/3 并联，U16 不装配；GND 连接见网表 `GND` 网络。
- [ ] **AC-02**: 目标板 hwdef 能为确认后的 TTL UART 生成唯一、无冲突的串口定义、引脚复用、DMA 和默认串口协议配置。
- [ ] **AC-03**: 构建配置不启用 CAN 作为该电调链路，并且 RS485 收发器/方向控制不被该链路使用。
- [ ] **AC-04**: TTL 广播帧使用 11 字节 AM32/KISS 格式，能够发送 6 路目标电调命令，并正确处理无应答广播场景。
- [x] **AC-05**: 当前控制链路仅单向广播，不读取回包、不启用半双工方向切换，不添加 RS485 DE/RE 控制。
- [ ] **AC-06**: 电机输出映射与目标板实际 ESC1、ESC2、ESC3 顺序一致，启动、解锁、失联和停止状态有明确安全行为。
- [ ] **AC-07**: 至少通过 Infineon 目标板 hwdef 生成/编译检查，以及协议帧或串口驱动的 focused test；无法在真实硬件验证的部分必须明确标注。
- [ ] **AC-08**: 网表中仍存在但不使用的 CAN/RS485 接口有板级注释或配置说明，避免后续误接线。
- [x] **AC-09**: 地址 `1~6` 扩展为 6 槽帧；AM32 `ESC_NODE_ADDRESS` 放宽为 `1..6`，解析长度为 11 字节，CRC 覆盖前 10 字节。
- [x] **AC-10**: AM32 使用 `signalSetInput(slot[ESC_NODE_ADDRESS-1])`，不再使用公共 `throttle` 字段驱动电机；ArduPilot 发送的公共字段为 `0`，旧 9 字节协议不作为兼容模式保留。

## 4. Edge Cases & Boundary Conditions

- 当前网表将 ESC 连接器接在 `A/B` 网络；用户确认目标是复用 `A/B` 且 U16 不装配，因此固件不得生成或启用 U16 的 RS485 方向控制。
- TTL 广播可能是单线半双工，也可能是独立 TX 广播；两者对 RX 引脚、DE/RE、上拉和 UART 驱动实现要求不同。
- 多个 ESC 共享广播线时，广播命令通常没有单个 ESC 应答；读取遥测或参数需要另行定义寻址/轮询策略。
- `DEBUG_RX/TX`、RC 串口和 `LC302` 已占用 UART 资源，新增或复用串口不得造成 SCB/引脚/DMA 冲突。
- 未经原理图确认，不能依据 `RS485_GH1.26` 连接器名称判断其电气类型；网表中 `CANH/CANL` 与该连接器存在独立连接。

## 5. Execution & Verification Log

- [x] Initial repository and netlist exploration
- [x] Physical TTL interface clarification
- [x] AM32/KISS protocol source identified
- [ ] Design and implementation
- [ ] Automated build/tests and hardware-boundary verification
