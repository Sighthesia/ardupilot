# Task 3 Report: AP_AM32Kiss Protocol Library

## Status

Implemented the AP_AM32Kiss pure protocol helpers, unit-test target, normal
vehicle-library registration, and a compile-guarded lifecycle stub for the
 아직-unregistered `SerialProtocol_AM32Kiss` interface.

## Modified Files

- `libraries/AP_AM32Kiss/AP_AM32Kiss_config.h`
- `libraries/AP_AM32Kiss/AP_AM32Kiss.h`
- `libraries/AP_AM32Kiss/AP_AM32Kiss.cpp`
- `libraries/AP_AM32Kiss/tests/test_am32_kiss.cpp`
- `libraries/AP_AM32Kiss/tests/wscript`
- `Tools/ardupilotwaf/ardupilotwaf.py`

The lifecycle code is guarded by `AP_AM32KISS_SERIAL_PROTOCOL_ENABLED`, which
defaults to disabled until Task 4 adds the serial-manager enum and registration.
This keeps generic and SITL builds from depending on an enum that does not yet
exist. The pure helpers remain available in all builds.

## Implementation

- Maps clamped `1000..2000us` PWM to `0..255` with integer rounding.
- Packs the strict 11-byte frame: header, six slots, zero common throttle,
  counter, XOR, and CRC8 over bytes `0..9`.
- Implements the AM32 MSB-first CRC8 polynomial `0x07`.
- Provides guarded UART lifecycle behavior using `find_serial()`, 115200 baud
  fallback, 16-byte RX and 64-byte TX buffers, nonblocking TX-space checks,
  safe-state zeroing, and counter advancement only after a full write.
- Does not configure RS485 direction control, half-duplex mode, or read UART
  responses.

## Commit

`1099ba384e feat: add AM32 KISS protocol helpers`

## Tests and Verification

- `./waf configure --board sitl`: passed.
- `./waf --targets tests/test_am32_kiss`: blocked before compilation because
  the environment lacks the required Python package `empy==3.3.4`.
- `./build/sitl/tests/test_am32_kiss`: not run because the target was not
  generated.
- `git diff --check`: passed.
- `python3 -m py_compile Tools/ardupilotwaf/ardupilotwaf.py libraries/AP_AM32Kiss/tests/wscript`:
  passed.
- `SRV_Channels` API was checked against `SRV_Channel/SRV_Channel.h`; the
  required `get_output_pwm(Function, uint16_t&)` and `get_motor_function()`
  interfaces exist.

## Concerns

- The focused C++ test result remains unverified until `empy==3.3.4` is
  available in the build environment.
- Task 4 must enable `AP_AM32KISS_SERIAL_PROTOCOL_ENABLED` only after adding
  and registering `SerialProtocol_AM32Kiss`; no enum value was guessed here.
- UART integration and board-specific availability remain intentionally
  compile-guarded and are not active in the current SITL build.
