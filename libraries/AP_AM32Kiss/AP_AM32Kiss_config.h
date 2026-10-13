#pragma once

#include <AP_HAL/AP_HAL_Boards.h>

// The AM32 KISS TTL link is currently provided by the Infineon target.
#ifndef AP_AM32KISS_ENABLED
#define AP_AM32KISS_ENABLED (CONFIG_HAL_BOARD == HAL_BOARD_INFINEON)
#endif

#ifndef AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
#define AP_AM32KISS_SERIAL_PROTOCOL_ENABLED 1
#endif
