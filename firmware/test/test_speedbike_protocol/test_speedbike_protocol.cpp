#include <unity.h>

#include "core/speedbike_protocol.h"

void test_builders_append_expected_crc() {
    uint8_t bytes[16]{};
    TEST_ASSERT_EQUAL_UINT(7, buildSessionNotice(bytes, sizeof bytes));
    TEST_ASSERT_EQUAL_HEX8(0xac, bytes[6]);
    TEST_ASSERT_EQUAL_UINT(13, buildPasswordFrame(bytes, sizeof bytes));
    TEST_ASSERT_EQUAL_HEX8(0x9f, bytes[12]);
}

void test_decoder_skips_leading_garbage() {
    FrameDecoder decoder;
    DecodedFrame frame{};
    const uint8_t bytes[] = {0x00, 0xff, 0x54, 0x46, 0x52, 0x58, 1, 1, 0xba};
    decoder.push(bytes, sizeof bytes);
    TEST_ASSERT_TRUE(decoder.next(frame));
    TEST_ASSERT_EQUAL_HEX8(0x52, frame.op);
}

void test_decoder_drops_bad_crc_then_recovers() {
    FrameDecoder decoder;
    DecodedFrame frame{};
    const uint8_t bytes[] = {0x54,0x46,0x41,0x5a,1,1,0x00,0x54,0x46,0x52,0x58,1,1,0xba};
    decoder.push(bytes, sizeof bytes);
    TEST_ASSERT_TRUE(decoder.next(frame));
    TEST_ASSERT_EQUAL_HEX8(0x52, frame.op);
}

void test_decoder_password_request_split() {
    FrameDecoder decoder;
    DecodedFrame frame{};
    const uint8_t first[] = {0x54, 0x46, 0x52};
    const uint8_t second[] = {0x58, 1, 1, 0xba};
    decoder.push(first, sizeof first);
    TEST_ASSERT_FALSE(decoder.next(frame));
    decoder.push(second, sizeof second);
    TEST_ASSERT_TRUE(decoder.next(frame));
}

void test_decoder_reassembles_dashboard_from_three_chunks() {
    FrameDecoder decoder;
    DecodedFrame frame{};
    uint8_t bytes[26]{};
    bytes[0] = 0x54;
    bytes[1] = 0x46;
    bytes[2] = 0x41;
    bytes[3] = 0x44;
    bytes[4] = 0x14;
    bytes[7] = 0x0b;
    bytes[8] = 0xb8;
    bytes[25] = 0x0a;
    decoder.push(bytes, 4);
    decoder.push(bytes + 4, 11);
    decoder.push(bytes + 15, 11);
    TEST_ASSERT_TRUE(decoder.next(frame));
    TEST_ASSERT_EQUAL_UINT8(20, frame.length);
    TEST_ASSERT_EQUAL_HEX8(0x0b, frame.payload[2]);
    TEST_ASSERT_EQUAL_HEX8(0xb8, frame.payload[3]);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_builders_append_expected_crc);
    RUN_TEST(test_decoder_skips_leading_garbage);
    RUN_TEST(test_decoder_drops_bad_crc_then_recovers);
    RUN_TEST(test_decoder_password_request_split);
    RUN_TEST(test_decoder_reassembles_dashboard_from_three_chunks);
    return UNITY_END();
}
