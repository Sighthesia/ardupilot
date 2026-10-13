# Final Fix Report: AM32 KISS TTL Review Wave

## Scope

Applied the two Important fixes and the requested low-risk Minor fixes from
`/tmp/opencode/am32-final-review.md` without changing hwdef structure, protocol
enum values, or existing parameter indexes.

## Fixes

- Reordered `AP_AM32Kiss::init_uart()` so `begin()` uses
  `AP_SERIALMANAGER_AM32KISS_BUFSIZE_RX/TX` first, followed by
  `FLOW_CONTROL_DISABLE`.
- After the AM32 UART is found and initialized, aggregated the physical output
  mask for logical motor functions `i = 0..5` through
  `SRV_Channels::get_output_channel_mask(SRV_Channels::get_motor_function(i))`
  and marked that mask digital with `SRV_Channels::set_digital_outputs(mask, 0)`.
  The existing `_uart != nullptr` initialization guard makes this run once.
- Kept the AM32 path guarded by `AP_AM32KISS_ENABLED` and only active after
  `find_serial()` returns a UART. No RTS/DE or half-duplex configuration was
  added.
- Replaced the test's zero-length array with `nullptr, 0` and restored the CRC
  vector declaration after the partial review edit.
- Updated the design document to state that the implementation reuses the
  existing UART id 3 / SCB3 / P13.1 / P13.0 / DMA 23/22 mapping rather than
  adding a dedicated UART entry.

## Feature-gate decision

`AP_AM32KISS_ENABLED` remains enabled for the existing Infineon HAL design.
The current Infineon board header defines `CONFIG_HAL_BOARD_SUBTYPE` as
`HAL_BOARD_SUBTYPE_NONE`; there is no existing `UluluDriveDayun` subtype symbol
to reference safely. Narrowing the gate would therefore require inventing a
board define or changing unrelated board infrastructure, which is outside this
review-fix scope.

## Verification

- `git diff --check` — passed.
- `python3 Tools/scripts/check_serial_protocol_gates.py` — passed.
- `./waf configure --board sitl` — passed.
- Focused `tests/test_am32_kiss` build/run — blocked by the existing missing
  `empy==3.3.4` dependency; the binary was not generated.
- Infineon board configure — remains blocked by the existing missing
  `modules/infineon/TARGET_CYT4BB7/config/GeneratedSource` submodule content.

## Commit

Final-fix changes are committed with the English conventional commit message:
`fix: close AM32 KISS final review findings`.
