# AM32 KISS TTL ESC Broadcast Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 ArduPilot 与 AM32 电调固件统一到 115200 8N1、6 槽、11 字节 KISS TTL 广播协议，并在 CYT4BB7 目标板上通过网表确认的 `U1.75/U1.74` 复用现有 `SCB3` 串口控制 6 个逻辑电机。

**Architecture:** AM32 端把节点地址 `1..6` 编译进六个 Keil target，每个节点从 11 字节广播帧的 `slot[address-1]` 取实际油门；ArduPilot 端新增独立 `AP_AM32Kiss` 驱动，在 `SRV_Channels::push()` 中读取六个逻辑电机输出并发送一帧。目标板复用既有 `UART id 3 / SCB3 / P13.1 TX / P13.0 RX`，不新增 UART、DMA、CAN 或 RS485 方向控制。

**Tech Stack:** ArduPilot C++/Waf/AP_Param/SRV_Channels/UARTDriver，Infineon CYT4BB7 hwdef JSON，Keil C251 AM32 C 固件，Python host-side protocol tests，GoogleTest。

## Global Constraints

- 电调通信不得依赖 RS485 协议栈、RS485 收发器方向控制或 CAN/DroneCAN/PiccoloCAN 电调链路。
- `U16 SIT3088ETK` 不装配；`A/B` 由 `U1.75/U1.74` 直接作为 TTL 信号使用。
- 地址 `1~6` 分别映射到 `slot[0]~slot[5]`，总帧长度为 11 字节。
- AM32 节点使用自身地址对应的 slot 作为实际油门；公共 `throttle` 字段固定为 `0`，仅参与 XOR/CRC 校验。
- 控制帧为 `0x0F + slot[0..5] + throttle + counter + XOR + CRC8`；CRC8 为 MSB-first、poly `0x07`、init `0x00`、无反射、无 xorout，覆盖字节 `0..9`。
- 当前控制链路仅单向广播，不读取回包、不启用半双工方向切换，不添加 RS485 DE/RE 控制。
- 共享 HAL 代码只能通过 ArduPilot HAL 接口访问平台功能；不得修改 `modules/`。
- 不改变既有 `SerialProtocol_*` 数值或既有参数索引；新协议使用数值 `51`，原 `SerialProtocol_NumProtocols` 顺延。
- 外部 AM32 工程位于 `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`，其改动必须在该仓库单独提交。

---

### Task 1: Extend AM32 to the Six-Slot Wire Contract

**Files:**
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/Inc/esc_serial.h:6`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/Inc/targets.h:16-20`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/Src/esc_serial.c:13-118`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/docs/port/esc-serial-protocol.md:1-36,91-103`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/docs/port/am32-deviation-matrix.md` entries describing the 9-byte parser and slot indexing
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/tools/verify-esc-build-contract.py` protocol assertions for frame length, slot indexing, and CRC offsets
- Test: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/tools/verify-esc-protocol.py`

**Interfaces:**
- Consumes: existing `uartRxPop()`, `uartRxOverflowTake()`, `signalSetInput()`, and `get_crc8()`.
- Produces: `ESC_SERIAL_THROTTLE_LEN == 11`; valid node addresses `1..6`; `signalSetInput(1000 + slot * 1000 / 255)` using `throttle_frame[ESC_NODE_ADDRESS - 1]`.

- [ ] **Step 1: Write the failing host tests.** Change the reference encoder and decoder from four slots/9 bytes to six slots/11 bytes. Assert the common field is zero, addresses 1 and 6 select slots 0 and 5, and the CRC covers bytes 0 through 9.

```python
def make_throttle_frame(slots, counter):
    if len(slots) != 6:
        raise ValueError("exactly six slots are required")
    throttle = 0
    frame = bytearray([0x0F, *slots, throttle, counter & 0xFF,
                       throttle ^ (counter & 0xFF)])
    frame.append(crc8(frame))
    return bytes(frame)

assert len(make_throttle_frame([1, 2, 3, 4, 5, 6], 7)) == 11
assert Decoder(1).frame(make_throttle_frame([11, 22, 33, 44, 55, 66], 7)).input_us == 1043
assert Decoder(6).frame(make_throttle_frame([11, 22, 33, 44, 55, 66], 7)).input_us == 1258
```

- [ ] **Step 2: Run the protocol test to verify it fails.**

Run: `python3 tools/verify-esc-protocol.py`

Expected: FAIL because the current decoder requires a 9-byte frame and reads the shared byte at offset 5.

- [ ] **Step 3: Implement the AM32 parser change.** Use these exact offsets and preserve the existing overflow/counter atomicity:

```c
#define ESC_SERIAL_THROTTLE_LEN 11U

#if ESC_NODE_ADDRESS < 1 || ESC_NODE_ADDRESS > 6
#error "ESC_NODE_ADDRESS must be in the range 1..6"
#endif

if (throttle_frame[9] != (uint8_t)(throttle_frame[7] ^ throttle_frame[8]) ||
    get_crc8(throttle_frame, 10U) != throttle_frame[10] ||
    (counter_valid && !counterIsForward(throttle_frame[8]))) {
    esc_serial_frames_bad++;
    return;
}
input_us = (uint16_t)(1000UL +
    ((uint32_t)throttle_frame[ESC_NODE_ADDRESS - 1U] * 1000UL) / 255UL);
e_rpm = throttle_frame[ESC_NODE_ADDRESS - 1U];
signalSetInput(input_us);
last_counter = throttle_frame[8];
```

The parser must still consume exactly ten bytes after `0x0F`, treat all bytes
inside that candidate as payload, and reset only after a failed candidate. The
shared byte at offset 7 is validated but never used as the node throttle.

- [ ] **Step 4: Update protocol documentation and run tests.** Replace every 9-byte/four-slot/`throttle_frame[ESC_NODE_ADDRESS]` statement with the 11-byte/six-slot/`ESC_NODE_ADDRESS - 1` contract. Run:

```bash
python3 tools/verify-esc-protocol.py
python3 tools/verify-esc-build-contract.py --selftest
```

Update the build-contract verifier's source mutations so it rejects a restored
9-byte length, a four-slot encoder, a shared-byte `signalSetInput()` call, or
CRC/XOR checks at the old offsets. Expected: both commands PASS; the verifier
must reject a mutation that restores `signalSetInput()` from the shared byte.

- [ ] **Step 5: Commit the AM32 parser contract.**

```bash
git add app/Inc/esc_serial.h app/Inc/targets.h app/Src/esc_serial.c \
  docs/port/esc-serial-protocol.md docs/port/am32-deviation-matrix.md \
  tools/verify-esc-protocol.py tools/verify-esc-build-contract.py
git commit -m "feat: extend AM32 KISS control to six slots"
```

---

### Task 2: Add AM32 Node Targets 3 Through 6

**Files:**
- Create: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/target/C/esc_node.h`
- Create: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/target/D/esc_node.h`
- Create: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/target/E/esc_node.h`
- Create: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/target/F/esc_node.h`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/app/AM32_AI8051U.uvproj` target definitions after `AM32_B`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/tools/build-app.sh:6-39,91-100`
- Modify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/AGENTS.md` and build documentation that state only nodes 1/2 exist
- Test: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC/tools/verify-esc-protocol.py`

**Interfaces:**
- Consumes: the existing Keil target layout where `IncludePath` selects `target/A/esc_node.h` or `target/B/esc_node.h`.
- Produces: six buildable target names `AM32`, `AM32_B`, `AM32_C`, `AM32_D`, `AM32_E`, `AM32_F`, selecting node addresses `1..6`.

- [ ] **Step 1: Add node headers.** Each new header must preserve the existing include guard style and define its unique address:

```c
#define ESC_NODE_ADDRESS  3U  /* target C; use 4U/5U/6U for D/E/F */
#define ESC_TARGET_NAME   "AM32_AI8051U_c"  /* use the matching lowercase suffix */
```

- [ ] **Step 2: Duplicate the two existing Keil target configurations.** Keep compiler/linker settings identical to `AM32_B`; change only `TargetName`, `OutputName`, and the first `IncludePath` entry to `.\target\C`, `.\target\D`, `.\target\E`, and `.\target\F`. Do not alter source membership, memory layout, or production profile defines.

- [ ] **Step 3: Extend the build helper.** Accept individual names and build all six by default. Report the node number and expected HEX path for each image; reject unknown names before invoking Wine.

- [ ] **Step 4: Run the source contract and build checks.**

Run: `python3 tools/verify-esc-build-contract.py --selftest`

Run: `tools/build-app.sh all`

Expected: the static verifier passes. If the local Keil/Wine toolchain is unavailable, record that exact prerequisite failure and still verify all six target names and include paths with the repository's host-side checks.

- [ ] **Step 5: Commit the six-node build matrix.**

```bash
git add app/target/C app/target/D app/target/E app/target/F \
  app/AM32_AI8051U.uvproj tools/build-app.sh AGENTS.md
git commit -m "build: add AM32 KISS node targets three to six"
```

---

### Task 3: Create the AP_AM32Kiss Protocol Library and Unit Tests

**Files:**
- Create: `libraries/AP_AM32Kiss/AP_AM32Kiss_config.h`
- Create: `libraries/AP_AM32Kiss/AP_AM32Kiss.h`
- Create: `libraries/AP_AM32Kiss/AP_AM32Kiss.cpp`
- Create: `libraries/AP_AM32Kiss/tests/test_am32_kiss.cpp`
- Create: `libraries/AP_AM32Kiss/tests/wscript`
- Modify: `Tools/ardupilotwaf/ardupilotwaf.py:27-134`

**Interfaces:**
- Consumes: `AP_HAL::UARTDriver`, `AP_SerialManager`, `SRV_Channels::get_motor_function()`, `SRV_Channels::get_output_pwm()`, and `hal.util->get_soft_armed()`.
- Produces: `AP_AM32Kiss::update()`, `AP_AM32Kiss::pack_frame(const uint8_t slots[6], uint8_t counter, uint8_t frame[11])`, `AP_AM32Kiss::throttle_to_byte(uint16_t pwm)`, and `AP_AM32Kiss::crc8(const uint8_t *data, uint8_t length)`.

- [ ] **Step 1: Write failing pure-protocol tests.** Test the public static helpers without a live UART:

```cpp
TEST(AM32Kiss, throttle_mapping)
{
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(900), 0);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(1000), 0);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(1500), 128);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(2000), 255);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(2100), 255);
}

TEST(AM32Kiss, packs_six_slots_and_crc)
{
    const uint8_t slots[6] {1, 2, 3, 4, 5, 6};
    uint8_t frame[AP_AM32Kiss::FRAME_LENGTH] {};
    AP_AM32Kiss::pack_frame(slots, 0xFF, frame);
    EXPECT_EQ(frame[0], 0x0F);
    EXPECT_EQ(frame[1], 1);
    EXPECT_EQ(frame[6], 6);
    EXPECT_EQ(frame[7], 0);
    EXPECT_EQ(frame[8], 0xFF);
    EXPECT_EQ(frame[9], 0xFF);
    EXPECT_EQ(frame[10], AP_AM32Kiss::crc8(frame, 10));
}
```

- [ ] **Step 2: Run the focused test to verify it fails.**

Run: `./waf configure --board sitl && ./waf --targets tests/test_am32_kiss`

Expected: FAIL because `AP_AM32Kiss` and its test target do not exist.

- [ ] **Step 3: Implement the pure protocol helpers.** Define these constants and formulas:

```cpp
static constexpr uint8_t SLOT_COUNT = 6;
static constexpr uint8_t FRAME_LENGTH = 11;
static constexpr uint8_t HEADER = 0x0F;

uint8_t AP_AM32Kiss::throttle_to_byte(uint16_t pwm)
{
    const uint32_t clamped = constrain_int32(pwm, 1000, 2000) - 1000;
    return uint8_t((clamped * 255U + 500U) / 1000U);
}
```

Implement the CRC loop explicitly as MSB-first `0x07`; `pack_frame()` must set
`frame[7] = 0`, `frame[8] = counter`, `frame[9] = frame[7] ^ frame[8]`, and
`frame[10] = crc8(frame, 10)`.

- [ ] **Step 4: Add the feature configuration and UART/output lifecycle.** Define `AP_AM32KISS_ENABLED` as `1` for the Infineon board and as `0` when the board does not provide the feature, while keeping the library compilable in generic builds. On the first `update()`, find `SerialProtocol_AM32Kiss` instance 0 and call `begin(baud, 16, 64)`. If no UART is configured, return without marking digital outputs. Once initialized, fill `slots[i]` from `SRV_Channels::get_output_pwm(SRV_Channels::get_motor_function(i), pwm)`; missing channels become zero. If `!hal.util->get_soft_armed()` or `SRV_Channels::get_emergency_stop()`, force all six slots to zero. Send only when `txspace() >= 11` and increment the counter only when `write(frame, 11) == 11`. Never call `set_flow_control(FLOW_CONTROL_RTS_DE)`, `set_options(OPTION_HDPLEX)`, or a blocking read.

- [ ] **Step 5: Register the library in the normal build.** Add `AP_AM32Kiss` to `COMMON_VEHICLE_DEPENDENT_LIBRARIES` and add the test `wscript` using the same `bld.ap_find_tests(use='ap')` pattern as `AP_Math/tests/wscript`.

- [ ] **Step 6: Run the focused tests and commit.**

```bash
./waf configure --board sitl
./waf --targets tests/test_am32_kiss
./build/sitl/tests/test_am32_kiss
git add libraries/AP_AM32Kiss Tools/ardupilotwaf/ardupilotwaf.py
git commit -m "feat: add AM32 KISS protocol helpers"
```

---

### Task 4: Register the Serial Protocol and Connect SRV_Channels

**Files:**
- Modify: `libraries/AP_SerialManager/AP_SerialManager.h:37-91`
- Modify: `libraries/AP_SerialManager/AP_SerialManager_config.h:121-123`
- Modify: `libraries/AP_SerialManager/AP_SerialManager.cpp:180-190,514-588`
- Modify: `libraries/AP_SerialManager/AP_SerialManager_default_protocol_check.cpp:39-114`
- Modify: `libraries/SRV_Channel/SRV_Channel.h:20-28,650-670`
- Modify: `libraries/SRV_Channel/SRV_Channels.cpp:198-205,486-513`
- Test: `libraries/AP_SerialManager/AP_SerialManager_default_protocol_check.cpp` compile-time gate and `tests/test_am32_kiss`

**Interfaces:**
- Consumes: `AP_AM32Kiss` from Task 3 and existing `SRV_Channels::push()` ordering.
- Produces: `SerialProtocol_AM32Kiss = 51`, default protocol value `SerialProtocol_AM32Kiss`, serial initialization with flow control disabled, and one `am32kiss.update()` call per `SRV_Channels::push()`.

- [ ] **Step 1: Add the new protocol without renumbering existing values.** Put `SerialProtocol_AM32Kiss = 51` immediately before `SerialProtocol_NumProtocols`. Add `AP_SERIALMANAGER_AM32KISS_BUFSIZE_RX 16` and `..._TX 64`. Extend the parameter `@Values` documentation with `51:AM32Kiss`.

- [ ] **Step 2: Add serial initialization and default-protocol gating.** Add a `SerialProtocol_AM32Kiss` switch case that calls `uart->begin(state[i].baudrate(), 16, 64)` and `uart->set_flow_control(FLOW_CONTROL_DISABLE)`. Add `AP_AM32Kiss_config.h` to the default protocol checker and the condition `(p != SerialProtocol_AM32Kiss || (AP_AM32KISS_ENABLED))`.

- [ ] **Step 3: Wire the driver into `SRV_Channels`.** Include `AP_AM32Kiss.h`, add an `AP_AM32Kiss am32kiss;` member, and invoke `am32kiss.update()` immediately after `hal.rcout->push()` under `#if AP_AM32KISS_ENABLED`. Keep existing Robotis, FETtec, and CAN update blocks unchanged.

- [ ] **Step 4: Verify protocol registration and output safety.** Confirm a build with no `DEFAULT_SERIALn_PROTOCOL` remains unchanged, a build with `SerialProtocol_AM32Kiss` compiles the gate, and no call to `FLOW_CONTROL_RTS_DE` or `OPTION_HDPLEX` exists in the new driver. Run:

```bash
python3 Tools/scripts/check_serial_protocol_gates.py
./waf configure --board sitl
./waf --targets tests/test_am32_kiss
```

- [ ] **Step 5: Commit the ArduPilot integration.**

```bash
git add libraries/AP_SerialManager libraries/SRV_Channel
git commit -m "feat: register AM32 KISS serial output"
```

---

### Task 5: Bind the Existing SCB3 UART to the Netlist and Document Unused Buses

**Files:**
- Modify: `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/hwdef.dat:6-118`
- Create: `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/README.md`
- Test: generated hwdef output under the build directory and `docs/ref/Netlist_2026-10-11.txt`

**Interfaces:**
- Consumes: existing `UART id 3 / SCB3 / TX [13,1] / RX [13,0]` and Task 4's protocol enum.
- Produces: `SERIAL3` defaulting to AM32 KISS at 115200, with the direct-TTL netlist mapping documented as `P13.1/U1.75=A/TX` and `P13.0/U1.74=B/RX`.

- [ ] **Step 1: Add the board default protocol.** Add the hwdef key:

```json
"DEFAULT_SERIAL3_PROTOCOL" : "SerialProtocol_AM32Kiss",
"DEFAULT_SERIAL3_BAUD" : 115200,
```

Do not add a new UART entry, SCB, DMA channel, CAN interface, RS485 DE pin, or U16 control symbol. The existing UART 3 entry already uses the unique DMA pair 23/22 and SCB3 triggers.

- [ ] **Step 2: Add board-level wiring notes.** In the new README record the evidence and boundaries:

```text
ESC TTL broadcast: A = U1.75 = P13.1 = SCB3_TX = SERIAL3 TX
                  B = U1.74 = P13.0 = SCB3_RX = SERIAL3 RX
ESC1/2/3 share A/B and GND; U16 SIT3088ETK is not populated.
CANH/CANL through U12 is a separate unused-for-ESC network.
```

Reference the Infineon CYT4BB/4BB datasheet package pin list and alternate
function table in the README rather than treating the net names as RS485.

- [ ] **Step 3: Generate and inspect the hwdef.**

Run: `./waf configure --board UluluDriveDayun`

Inspect the generated header and assert that it contains one `UART id 3`,
`SCB3`, `P13.1_SCB3_UART_TX`, `P13.0_SCB3_UART_RX`, and the expected PDMA
triggers, with no `DEFAULT_SERIAL3_PROTOCOL` numeric value and no new SCB/DMA
collision.

- [ ] **Step 4: Commit the board binding.**

```bash
git add libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun
git commit -m "feat: bind Infineon SCB3 to AM32 TTL ESC bus"
```

---

### Task 6: Run Cross-Repository Verification and Close the Contract

**Files:**
- Modify: `.specs/2026-10-12-ttl-esc-netlist-port.md:13-47`
- Modify: `docs/superpowers/plans/2026-10-12-am32-kiss-ttl-implementation.md` task checkboxes during execution only
- Verify: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`
- Verify: `docs/ref/Netlist_2026-10-11.txt`

**Interfaces:**
- Consumes: all previous task commits and the approved six-slot wire contract.
- Produces: reproducible host/build verification and an explicit hardware-only verification boundary.

- [ ] **Step 1: Run AM32 host and build checks.**

```bash
cd /home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC
python3 tools/verify-esc-protocol.py
python3 tools/verify-esc-build-contract.py --selftest
tools/build-app.sh all
```

Expected: protocol, source-contract, and all six target builds pass. A missing
Keil/Wine installation is a documented environment blocker, not evidence of a
protocol pass.

- [ ] **Step 2: Run ArduPilot focused and target builds.**

```bash
cd /home/Sighthesia/0_Files/Producing/Hardware/MCU/Infineon/ardupilot
./waf configure --board sitl
./waf --targets tests/test_am32_kiss
./build/sitl/tests/test_am32_kiss
./waf configure --board UluluDriveDayun
./waf copter
```

Also run `python3 Tools/scripts/check_serial_protocol_gates.py` and
`git diff --check`. Record any target-board compiler/toolchain blocker without
claiming hardware verification.

- [ ] **Step 3: Audit the final diff for forbidden paths and bus usage.**

Run:

```bash
git diff --check
rg -n "FLOW_CONTROL_RTS_DE|OPTION_HDPLEX|SIT3088|DroneCAN|PiccoloCAN|CAN.*ESC" \
  libraries/AP_AM32Kiss libraries/AP_SerialManager libraries/SRV_Channel \
  libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun
```

The AM32 driver must not contain RS485 direction, half-duplex, or CAN control.
Existing unrelated CAN/RS485 references elsewhere in the repository are not
part of this audit and must not be removed.

- [ ] **Step 4: Update the task contract with evidence.** Mark AC-02 through AC-08 complete only with the corresponding generated hwdef, test, build, or documentation evidence. Leave real-board checks explicitly pending until an oscilloscope or target board verifies idle voltage, `115200 8N1`, shared-bus loading, and that U16 is unpopulated.

- [ ] **Step 5: Commit the final verification record in the ArduPilot repo.**

```bash
git add .specs/2026-10-12-ttl-esc-netlist-port.md
git commit -m "test: verify AM32 KISS TTL ESC integration"
```
