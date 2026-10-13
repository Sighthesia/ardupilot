#pragma once

#include <AP_HAL/AP_HAL.h>
#include <AP_Param/AP_Param.h>

#include "AP_AM32Kiss_config.h"

class AP_AM32Kiss {
public:
    static constexpr uint8_t SLOT_COUNT = 6;
    static constexpr uint8_t FRAME_LENGTH = 11;
    static constexpr uint8_t HEADER = 0x0F;
    static constexpr uint32_t DEFAULT_BAUDRATE = 115200U;

    AP_AM32Kiss();

    CLASS_NO_COPY(AP_AM32Kiss);

    static const struct AP_Param::GroupInfo var_info[];

    /// Periodically called from SRV_Channels::push().
    void update();

    static uint8_t throttle_to_byte(uint16_t pwm);
    static void pack_frame(const uint8_t slots[SLOT_COUNT], uint8_t counter, uint8_t frame[FRAME_LENGTH]);
    static uint8_t crc8(const uint8_t *data, uint8_t length);

private:
#if AP_AM32KISS_ENABLED && AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
    void init_uart();

    AP_HAL::UARTDriver *_uart;
#endif

    uint8_t _counter;
};
