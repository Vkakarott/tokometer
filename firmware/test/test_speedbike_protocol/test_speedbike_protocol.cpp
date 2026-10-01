#include <unity.h>

#include "core/speedbike_protocol.h"

void test_builders_append_expected_crc() {
    uint8_t bytes[16]{};
    TEST_ASSERT_EQUAL_UINT(7, buildSessionNotice(bytes, sizeof bytes));
    TEST_ASSERT_EQUAL_HEX8(0xac, bytes[6]);
    TEST_ASSERT_EQUAL_UINT(13, buildPasswordFrame(bytes, sizeof bytes));
    TEST_ASSERT_EQUAL_HEX8(0x9f, bytes[12]);
}

void test_known_crc_values() {
    const uint8_t request[] = {0x54, 0x46, 0x52, 0x58, 1, 1};
    const uint8_t answer[] = {0x54, 0x46, 0x41, 0x5a, 1, 1};
    TEST_ASSERT_EQUAL_HEX8(0xba, speedbikeCrc(request, sizeof request));
    TEST_ASSERT_EQUAL_HEX8(0xc9, speedbikeCrc(answer, sizeof answer));
}

void test_classifies_password_request_and_session_ack() {
    const DecodedFrame request{0x52, 0x58, 1, {1}};
    const DecodedFrame acknowledgement{0x41, 0x58, 0, {}};
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FrameKind::PasswordRequest), static_cast<int>(classifyFrame(request)));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(FrameKind::SessionAck), static_cast<int>(classifyFrame(acknowledgement)));
}

void test_parses_dashboard_rpm() {
    const DecodedFrame dashboard{0x41, 0x44, 20, {0, 0, 0x0b, 0xb8}};
    uint16_t rpm = 0;
    TEST_ASSERT_TRUE(parseDashboardRpm(dashboard, rpm));
    TEST_ASSERT_EQUAL_UINT16(3000, rpm);
}

void test_rejects_non_dashboard_lengths() {
    const DecodedFrame shortDashboard{0x41, 0x44, 19, {0, 0, 0x0b, 0xb8}};
    uint16_t rpm = 0;
    TEST_ASSERT_FALSE(parseDashboardRpm(shortDashboard, rpm));
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
    RUN_TEST(test_known_crc_values);
    RUN_TEST(test_classifies_password_request_and_session_ack);
    RUN_TEST(test_parses_dashboard_rpm);
    RUN_TEST(test_rejects_non_dashboard_lengths);
    RUN_TEST(test_decoder_skips_leading_garbage);
    RUN_TEST(test_decoder_drops_bad_crc_then_recovers);
    RUN_TEST(test_decoder_password_request_split);
    RUN_TEST(test_decoder_reassembles_dashboard_from_three_chunks);
    return UNITY_END();
}
