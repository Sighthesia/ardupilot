# AM32 KISS TTL Broadcast ESC Design

## Context

The target board uses a CYT4BB7CEBQ0AESGS MCU. The netlist connects the three
ESC connectors to the shared `A/B` nets:

- `A`: `ESC1/2/3` pin 2 and MCU `U1.75`
- `B`: `ESC1/2/3` pin 1 and MCU `U1.74`
- `GND`: `ESC1/2/3` pins 4/5

The `SIT3088ETK` at `U16` is not populated. Therefore the firmware treats the
MCU pins as the direct TTL UART connection and must not configure RS485 driver
enable, receiver enable, or CAN for the ESC link. The separate `CANH/CANL`
network and its `SIT1050TK` transceiver are outside this control path.

The AM32 source project is
`/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`.
Its current FC-to-ESC frame is a 9-byte KISS-like fixed frame with four
throttle slots. The approved extension changes it to six slots so ESC node
addresses 1 through 6 can share one broadcast frame.

## Goals

1. Send six AM32 throttle values through one direct TTL UART broadcast frame.
2. Keep the protocol implementation independent from Robotis Dynamixel and
   FETtec OneWire implementations.
3. Preserve the existing ArduPilot output pipeline and safety behavior.
4. Make the board UART, DMA, pin mapping, and default serial protocol explicit.
5. Leave RS485 and CAN unavailable to this ESC driver.

## Non-goals

- AM32 telemetry, configuration, firmware flashing, or addressed replies.
- RS485 direction control or half-duplex support for this link.
- CAN/DroneCAN/PiccoloCAN ESC control.
- Changing existing serial protocol numeric assignments or parameter indexes.
- Treating the netlist's `RS485_GH1.26` connector name as evidence that the
  ESC `A/B` nets are RS485.

## Protocol

The link is `115200 8N1`, transmitted from the flight controller to all ESCs.
The 11-byte frame is:

| Offset | Field | Meaning |
| ---: | --- | --- |
| 0 | Header | `0x0F` |
| 1..6 | `slot[0..5]` | throttle for AM32 addresses 1..6 |
| 7 | Common throttle | fixed `0` from ArduPilot; retained for XOR/CRC compatibility and ignored for node motor input |
| 8 | Counter | modulo-256 sequence number |
| 9 | XOR | `throttle ^ counter` |
| 10 | CRC8 | CRC over bytes `0..9` |

The CRC is the source project's MSB-first CRC8 with polynomial `0x07`, initial
value `0x00`, no reflection, and no final XOR. Each AM32 image is built with
an `ESC_NODE_ADDRESS` in `1..6` and selects `throttle_frame[address - 1]` as
its motor input slot. The common throttle byte is fixed to zero by ArduPilot;
it is validated by XOR/CRC but is not used as the node's motor input. The frame
has no runtime address field and no reply expectation.

The AM32 parser remains fixed-length after seeing `0x0F`: it consumes exactly
ten following bytes. The XOR and CRC are checked before committing the input.
The existing forward counter rule is preserved. A rejected, duplicated, or
ambiguous frame does not change the motor input or timeout state.

## ArduPilot Architecture

### `AP_AM32Kiss`

Create a new library `libraries/AP_AM32Kiss/` with a small driver class that:

- obtains its UART through `AP_SerialManager::SerialProtocol_AM32Kiss`;
- starts the UART at the configured baud, defaulting to 115200;
- reads six motor values from the configured ArduPilot servo functions;
- converts each value from the normal `1000..2000us` range to `0..255` with
  clamping and integer rounding defined by unit tests;
- writes one 11-byte frame per output update, with byte 7 fixed to zero;
- increments the frame counter modulo 256;
- does not read or parse UART responses;
- drops a frame when the UART has insufficient transmit space instead of
  blocking the control loop;
- sends zero throttle when the output layer reports a stopped/safe state.

The class is called from `SRV_Channels::push()` after the normal RC output
update, matching the existing FETtec integration point. It uses
`SRV_Channels::srv_channel()` and `SRV_Channel::get_output_pwm()` for the
configured six output channels, rather than reaching into a vehicle-specific
motor class.

The initial output mapping is six consecutive motor channels in logical
address order. The mapping must be represented by the existing servo function
configuration or a new AM32 motor mask parameter; it must not depend on
physical PWM pins because the control signal is the dedicated UART. The final
implementation will preserve the existing parameter-index rules and document
the selected mapping in the board defaults.

### Serial manager

Add a new protocol enum after the existing protocols without renumbering any
existing value. Add the protocol to the compiled-in protocol validation and
the serial initialization switch. The initialization path uses ordinary UART
TX/RX with flow control disabled and no `OPTION_HDPLEX`.

Because the current control path is broadcast-only, the driver does not enable
UART RX for protocol behavior and does not add an Infineon-specific direction
GPIO. The RX pin may remain configured by the UART backend for electrical
compatibility, but no response is expected or required.

### Infineon board definition

The netlist review confirmed that the ESC bus reuses the existing UART id 3
entry in `libraries/AP_HAL_Infineon/hwdef/UluluDriveDayun/hwdef.dat`: SCB3,
`P13.1/U1.75` for TX, `P13.0/U1.74` for RX, and the existing DMA pair
23/22. No dedicated UART entry, new SCB, or new DMA allocation is added. The
existing entry is selected for AM32 KISS by the board defaults and must retain:

- the confirmed `A` TX and `B` RX pins, or the confirmed electrical direction
  if the board's wiring labels use the inverse nomenclature;
- the existing unique RX/TX DMA channels and triggers;
- a default baud rate of 115200;
- explicit documentation that this UART is direct TTL and does not use `U16`.

The generator and generated board definitions must remain compatible with
boards that do not define this UART. No CAN definition is added for this
feature.

## AM32 Source Changes

Modify the source project in place:

- `app/Inc/esc_serial.h`: set the throttle frame length to 11.
- `app/Inc/targets.h`: accept `ESC_NODE_ADDRESS` values 1 through 6.
- `app/Src/esc_serial.c`: validate XOR at offset 9, CRC at offset 10 over
  bytes 0 through 9, select `throttle_frame[ESC_NODE_ADDRESS - 1]` as the
  node throttle, and pass that slot value to `signalSetInput()`.
- update comments and target headers so the six-slot wire contract is visible
  to each AM32 build target.

The existing 9-byte format is intentionally replaced rather than accepted as
a second ambiguous mode. This prevents a stale or misconfigured image from
silently treating one ESC's data as another ESC's slot.

## Safety and Failure Handling

- Before initialization, no AM32 frame is sent.
- If the UART is unavailable or has insufficient space, the driver records a
  failed send and returns without blocking.
- On disarm, emergency stop, or normal output zeroing, all six slots are zero.
- The frame counter advances only for a frame accepted into the UART output
  path, preventing counter gaps caused solely by a full TX buffer.
- No pre-arm success is inferred from a UART write; physical ESC arming and
  signal-loss handling remain AM32 responsibilities.
- The implementation must not call `set_flow_control(FLOW_CONTROL_RTS_DE)` or
  use `OPTION_HDPLEX` for this link.

## Verification

### Protocol tests

Add focused tests for:

1. all six slots, including address 1 and address 6;
2. header, throttle, counter, XOR, and CRC byte positions;
3. CRC vectors generated from the AM32 implementation;
4. throttle conversion at 1000us, 1500us, 2000us, and out-of-range values;
5. counter wrap from 255 to 0;
6. full-TX-buffer behavior without blocking;
7. AM32 acceptance/rejection of valid, duplicate, bad-XOR, and bad-CRC frames.

### Build checks

- Generate the Infineon hwdef and inspect the generated UART macros for SCB,
  HSIOM, DMA, and pin collisions.
- Build the focused AM32Kiss library or target vehicle for the Infineon board.
- Run the relevant ArduPilot unit tests and the AM32 host-side tests.
- Mark real-board electrical validation separately: verify idle level,
  `115200 8N1`, `A/B` voltage levels, shared-bus loading, and that the
  unpopulated U16 cannot drive the bus.

## References

- Netlist: `docs/ref/Netlist_2026-10-11.txt`.
- AM32 protocol source: `/home/Sighthesia/0_Files/Producing/Hardware/MCU/STC32/AI8051U_AM32_ESC`.
- Existing ArduPilot serial ESC integration: `libraries/AP_FETtecOneWire/`.
- Existing ArduPilot serial protocol registration: `libraries/AP_SerialManager/`.
