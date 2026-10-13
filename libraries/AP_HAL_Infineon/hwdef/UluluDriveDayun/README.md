# UluluDriveDayun AM32 TTL Wiring

`SERIAL3` reuses the existing `UART id 3` peripheral: SCB3 at 115200 baud,
with no additional UART, DMA, CAN, or RS485 direction-control hardware.

## ESC bus

The netlist evidence is `docs/ref/Netlist_2026-10-11.txt`:

```text
ESC TTL broadcast: A = U1.75 = P13.1 = SCB3_TX = SERIAL3 TX
                   B = U1.74 = P13.0 = SCB3_RX = SERIAL3 RX
ESC1/2/3 share A/B and GND; U16 SIT3088ETK is not populated.
CANH/CANL through U12 is a separate unused-for-ESC network.
```

The package-pin mapping and UART alternate functions are taken from the
Infineon CYT4BB/4BB datasheet package pin list and HSIOM alternate-function
table: P13.1 provides `SCB3_UART_TX` and P13.0 provides `SCB3_UART_RX`.
The net names `A` and `B` are direct TTL connections here, not RS485 signals.

No U16 DE/RE control is configured, and the separate U12 CANH/CANL network is
not part of the AM32 ESC control path.
