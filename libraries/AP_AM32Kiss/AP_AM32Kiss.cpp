#include "AP_AM32Kiss.h"

#include <AP_Math/AP_Math.h>
#include <AP_SerialManager/AP_SerialManager.h>
#include <SRV_Channel/SRV_Channel.h>

extern const AP_HAL::HAL& hal;

const AP_Param::GroupInfo AP_AM32Kiss::var_info[] = {
    AP_GROUPEND
};

AP_AM32Kiss::AP_AM32Kiss()
    :
#if AP_AM32KISS_ENABLED && AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
      _uart(nullptr),
#endif
      _counter(0)
{
}

uint8_t AP_AM32Kiss::throttle_to_byte(uint16_t pwm)
{
    const uint32_t clamped = constrain_int32(pwm, 1000, 2000) - 1000;
    return uint8_t((clamped * 255U + 500U) / 1000U);
}

uint8_t AP_AM32Kiss::crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80U) ? uint8_t((crc << 1) ^ 0x07U) : uint8_t(crc << 1);
        }
    }

    return crc;
}

void AP_AM32Kiss::pack_frame(const uint8_t slots[SLOT_COUNT], uint8_t counter, uint8_t frame[FRAME_LENGTH])
{
    frame[0] = HEADER;
    for (uint8_t i = 0; i < SLOT_COUNT; i++) {
        frame[i + 1] = slots[i];
    }
    frame[7] = 0;
    frame[8] = counter;
    frame[9] = frame[7] ^ frame[8];
    frame[10] = crc8(frame, 10);
}

#if AP_AM32KISS_ENABLED && AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
void AP_AM32Kiss::init_uart()
{
    if (_uart != nullptr) {
        return;
    }

    const AP_SerialManager& serial_manager = AP::serialmanager();
    _uart = serial_manager.find_serial(AP_SerialManager::SerialProtocol_AM32Kiss, 0);
    if (_uart == nullptr) {
        return;
    }

    _uart->set_flow_control(AP_HAL::UARTDriver::FLOW_CONTROL_DISABLE);

    const uint32_t baudrate = serial_manager.find_baudrate(AP_SerialManager::SerialProtocol_AM32Kiss, 0);
    _uart->begin(baudrate == 0 ? DEFAULT_BAUDRATE : baudrate, 16, 64);
}
#endif

void AP_AM32Kiss::update()
{
#if AP_AM32KISS_ENABLED && AP_AM32KISS_SERIAL_PROTOCOL_ENABLED
    init_uart();
    if (_uart == nullptr) {
        return;
    }

    if (_uart->txspace() < FRAME_LENGTH) {
        return;
    }

    uint8_t slots[SLOT_COUNT] {};
    if (hal.util->get_soft_armed() && !SRV_Channels::get_emergency_stop()) {
        for (uint8_t i = 0; i < SLOT_COUNT; i++) {
            uint16_t pwm;
            if (SRV_Channels::get_output_pwm(SRV_Channels::get_motor_function(i), pwm)) {
                slots[i] = throttle_to_byte(pwm);
            }
        }
    }

    uint8_t frame[FRAME_LENGTH];
    pack_frame(slots, _counter, frame);
    if (_uart->write(frame, FRAME_LENGTH) == FRAME_LENGTH) {
        _counter++;
    }
#endif
}
