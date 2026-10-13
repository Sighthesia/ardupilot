#include <AP_gtest.h>

#include <AP_AM32Kiss/AP_AM32Kiss.h>

TEST(AM32Kiss, throttle_mapping)
{
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(900), 0);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(1000), 0);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(1500), 128);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(2000), 255);
    EXPECT_EQ(AP_AM32Kiss::throttle_to_byte(2100), 255);
}

TEST(AM32Kiss, packs_six_slots_and_crc)
{
    const uint8_t slots[AP_AM32Kiss::SLOT_COUNT] {1, 2, 3, 4, 5, 6};
    uint8_t frame[AP_AM32Kiss::FRAME_LENGTH] {};

    AP_AM32Kiss::pack_frame(slots, 0xFF, frame);

    EXPECT_EQ(frame[0], 0x0F);
    EXPECT_EQ(frame[1], 1);
    EXPECT_EQ(frame[6], 6);
    EXPECT_EQ(frame[7], 0);
    EXPECT_EQ(frame[8], 0xFF);
    EXPECT_EQ(frame[9], 0xFF);
    EXPECT_EQ(frame[10], AP_AM32Kiss::crc8(frame, 10));
    EXPECT_EQ(frame[10], 0x76);
}

TEST(AM32Kiss, crc8_vectors)
{
    const uint8_t vector[] {0x0F, 1, 2, 3, 4, 5, 6, 0, 0xFF, 0xFF};

    EXPECT_EQ(AP_AM32Kiss::crc8(nullptr, 0), 0);
    EXPECT_EQ(AP_AM32Kiss::crc8(vector, sizeof(vector)), 0x76);
}

TEST(AM32Kiss, counter_wrap_frame_fields)
{
    const uint8_t slots[AP_AM32Kiss::SLOT_COUNT] {};
    uint8_t frame[AP_AM32Kiss::FRAME_LENGTH] {};

    AP_AM32Kiss::pack_frame(slots, 0xFF, frame);
    EXPECT_EQ(frame[8], 0xFF);
    EXPECT_EQ(frame[9], 0xFF);

    AP_AM32Kiss::pack_frame(slots, 0, frame);
    EXPECT_EQ(frame[8], 0);
    EXPECT_EQ(frame[9], 0);
}

AP_GTEST_MAIN()
