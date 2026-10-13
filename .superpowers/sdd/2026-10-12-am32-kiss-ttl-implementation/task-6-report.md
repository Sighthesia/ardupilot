# Task 6 Report: Cross-Repository Verification and Contract Close

## Status

`DONE_WITH_CONCERNS` — all runnable host/build checks pass; two
environment/toolchain blockers (pre-existing, identical to Task 3/4/5
reports) prevent the focused C++ test binary and the Infineon board
configuration. No product code was modified by this task; this report is
the only file added.

## Step 1: AM32 host and build checks (all PASS)

Repo: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`
(worktree clean, HEAD `1eb9ebc`).

- `python3 tools/verify-esc-protocol.py` → PASS
  (`OK: frame fields/CRC/slots/counter, streaming recovery, address routing`),
  exit 0.
- `python3 tools/verify-esc-build-contract.py --selftest` → PASS
  (`profile selftest ok: 21 mutations, all rejected`, plus the protocol
  mutation rejections: 9-byte length, 1..2 guard, shared-throttle input,
  non-zero shared throttle, old XOR/CRC offsets, old slot index),
  exit 0.
- `tools/build-app.sh all` → PASS for all six targets (`AM32`, `AM32_B`,
  `AM32_C`, `AM32_D`, `AM32_E`, `AM32_F`); each reports
  `0 Error(s), 37 Warning(s)` and generates its HEX
  (`AM32_AI8051U{,_b,_c,_d,_e,_f}.hex`, 30273 bytes each), exit 0.
  Note: warning count is 37 this run vs 36 recorded in the Task 2 report;
  still 0 errors / 0 C-compiler warnings per the build-contract
  classification (31 L16 + 5 L30 linker warnings + 1 additional linker
  note line counted by the build script output).

## Step 2: ArduPilot focused and target builds

Repo: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/Infineon/ardupilot`
(HEAD `f6dd7eb39d`; worktree clean except untracked `docs/ref/`, left
untouched per instruction).

- `python3 Tools/scripts/check_serial_protocol_gates.py` → PASS
  (`OK: every SerialProtocol is gated or explicitly registered`;
  52 enumerators, 35 gated, 17 ungated of which 13 are pre-existing
  TODO-to-gate entries unrelated to this work), exit 0.
- `git diff --check` → PASS (no output), exit 0.
- `./waf configure --board sitl` → PASS
  (`'configure' finished successfully`), exit 0.
- `./waf --targets tests/test_am32_kiss` → BLOCKED, exit 1, exact message:
  `you need to install empy with 'python3 -m pip install empy==3.3.4'`.
  Same pre-existing environment blocker as Task 3/4 reports; binary not
  generated, `./build/sitl/tests/test_am32_kiss` not run.
- `./waf configure --board UluluDriveDayun` → BLOCKED, exit 2, exact error:
  `Tools/ardupilotwaf/infineon.py line 49, in configure:
  cfg.srcnode.find_dir('modules/infineon/TARGET_CYT4BB7/config/GeneratedSource').abspath(),
  AttributeError: 'NoneType' object has no attribute 'abspath'`.
  The checkout lacks `modules/infineon/TARGET_CYT4BB7/config/GeneratedSource`
  (empty submodule path); same pre-existing blocker as the Task 5 report.
  `./waf copter` for the target board was therefore not attempted — claiming
  it would be hardware verification without the toolchain is out of scope.
- Generated hwdef inspection for the Infineon board: unavailable, same as
  Task 5 (configuration did not complete). The existing
  `build/sitl/hwdef.h` is SITL-only and was not used as board evidence.

## Step 3: Forbidden-path and bus-usage audit (all PASS)

- `libraries/AP_AM32Kiss`: grep for
  `FLOW_CONTROL_RTS_DE|OPTION_HDPLEX|CAN|RS485|set_direction|DE_PIN|RE_PIN`
  → no matches. The only flow-control reference in the driver is
  `AP_AM32Kiss.cpp:67`
  `_uart->set_flow_control(AP_HAL::UARTDriver::FLOW_CONTROL_DISABLE);`.
  No `set_options(OPTION_HDPLEX)`, no blocking reads, no CAN/DroneCAN/
  PiccoloCAN/RS485 direction control.
- `libraries/AP_SerialManager`: the `SerialProtocol_AM32Kiss` init case
  (`AP_SerialManager.cpp:529-534`) calls
  `uart->begin(baud, AM32KISS_BUFSIZE_RX 16, AM32KISS_BUFSIZE_TX 64)` and
  `set_flow_control(FLOW_CONTROL_DISABLE)` only. Enum value
  `SerialProtocol_AM32Kiss = 51` sits immediately before
  `SerialProtocol_NumProtocols` (`AP_SerialManager.h:90-91`); all existing
  protocol values unchanged.
- `libraries/SRV_Channel`: `AP_AM32Kiss am32kiss;` member
  (`SRV_Channel.h:669` under `#if AP_AM32KISS_ENABLED`), subgroup index 45
  (`SRV_Channels.cpp:205`), `am32kiss.update()` immediately after
  `hal.rcout->push()` (`SRV_Channels.cpp:496-497`); Robotis/FETtec/CAN
  blocks preserved.
- `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/hwdef.dat`: diff of the
  Task 5 commit (`b17f2e744a`) against its parent shows exactly two added
  keys (`DEFAULT_SERIAL3_PROTOCOL`, `DEFAULT_SERIAL3_BAUD`) and a trailing
  newline; UART id 3 remains `SCB 3 / tx [13,1] / rx [13,0]` (= P13.1 TX /
  P13.0 RX) with DMA `[1,23]` RX / `[1,22]` TX. No new UART, SCB, DMA,
  CAN, DE/RE, or U16 control symbol was added.

## Step 4: Contract evidence mapping (report-only; spec checkboxes untouched)

- C-01/C-03/AC-01/AC-05/AC-09/AC-10: already `[x]`; re-confirmed by the
  Step 3 audit (no RS485/CAN/DE control anywhere in the new driver path,
  single-direction broadcast, 11-byte/6-slot/slot[address-1]/zero-throttle
  contract passing host tests on both repos).
- AC-03 (no CAN/RS485-DE in the ESC link): code evidence complete per the
  Step 3 audit; board-compile evidence still pending on the GeneratedSource
  blocker.
- AC-02/AC-07 (hwdef generation / target-board compile): still blocked on
  the missing `modules/infineon/.../GeneratedSource`; explicitly pending,
  not claimed.
- AC-04/AC-06 (6-channel frame send, motor mapping/safety): ArduPilot-side
  frame packing is covered by `test_am32_kiss.cpp` assertions but the binary
  could not be built/run here (empy blocker); live motor-order, arming,
  failsafe, and single-wire loading behavior require the real board.
- AC-08 (board README for unused CAN/RS485): present in
  `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/README.md` (Task 5
  commits `b17f2e744a`, `f6dd7eb39d`).
- Hardware-only boundary (unchanged): idle voltage, `115200 8N1` on wire,
  shared-bus loading, U16-unpopulated assembly status, and ESC spin
  verification require oscilloscope/target-board runs and are not covered
  by this report.
- Per instruction priority (report-only verification), no spec/ledger
  checkbox was modified in this task.

## Scope

- Added: only this report file
  (`.superpowers/sdd/2026-10-12-am32-kiss-ttl-implementation/task-6-report.md`).
- `docs/ref/` (user untracked) was never staged, modified, or committed.
- No product-code files modified; no commit to the AM32 repo.
