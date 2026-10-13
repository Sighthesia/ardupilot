#pragma once

#include <AP_HAL/AP_HAL_Boards.h>

// The AM32 KISS TTL link is currently provided by the Infineon target.
#ifndef AP_AM32KISS_ENABLED
#define AP_AM32KISS_ENABLED (CONFIG_HAL_BOARD == HAL_BOARD_INFINEON)
#endif

// Task 4 will enable this after registering SerialProtocol_AM32Kiss.
#ifndef AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
#define AP_AM32KISS_SERIAL_PROTOCOL_ENABLED 0
#endif
