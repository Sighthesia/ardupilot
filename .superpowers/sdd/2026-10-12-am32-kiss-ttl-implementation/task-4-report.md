# Task 4 Report: Register the AM32 KISS Serial Protocol

## Status

Implemented the AM32 KISS serial protocol registration and connected the
driver to the `SRV_Channels::push()` output lifecycle.

## Implementation

- Added `SerialProtocol_AM32Kiss = 51` immediately before
  `SerialProtocol_NumProtocols`; existing protocol values remain unchanged.
- Added the AM32 RX/TX buffer-size macros (`16`/`64`) and documented
  `51:AM32Kiss` in the serial protocol parameter values.
- Added the serial-manager initialization case with the configured baud rate,
  the AM32 buffer sizes, and `FLOW_CONTROL_DISABLE`.
- Added `AP_AM32Kiss_config.h` to the default-protocol checker and gated the
  protocol on `AP_AM32KISS_ENABLED`.
- Enabled the AM32 serial lifecycle macro after protocol registration.
- Added the `AP_AM32Kiss` SRV_Channels subgroup at a new index (`45`), without
  changing existing subgroup indexes, and called `am32kiss.update()` directly
  after `hal.rcout->push()`.
- Preserved the existing Robotis, FETtec, and CAN update blocks.
- The AM32 driver contains no `FLOW_CONTROL_RTS_DE` or `OPTION_HDPLEX` use.

## Verification

- `python3 Tools/scripts/check_serial_protocol_gates.py`: passed. The checker
  reports all serial protocols as gated or explicitly registered.
- `./waf configure --board sitl`: passed.
- `./waf --targets tests/test_am32_kiss`: blocked before compilation because
  the environment lacks `empy==3.3.4`; Waf requested
  `python3 -m pip install empy==3.3.4`.
- `git diff --check`: passed.
- No hwdef files were modified by Task 4.

## Concerns

- The focused AM32 test binary was not generated or run because of the missing
  `empy==3.3.4` dependency.
- The board-specific UART/default protocol mapping remains Task 5 work.
