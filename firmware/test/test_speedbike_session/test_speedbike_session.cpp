#include <unity.h>

#include "core/speedbike_session.h"

DecodedFrame dashboardFrame() { return {0x41, 0x44, 20, {0, 0, 0x0b, 0xb8}}; }
DecodedFrame passwordRequest() { return {0x52, 0x58, 1, {1}}; }
DecodedFrame sessionAck() { return {0x41, 0x58, 0, {}}; }

void enterAwaiting(SpeedBikeSession &session) { session.tick(0); session.takeOutput(); session.deviceFound(1); session.takeOutput(); session.linkConnected(2); session.takeOutput(); }

void test_boot_starts_scan() { SpeedBikeSession s; s.tick(0); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Scanning), static_cast<int>(s.state())); TEST_ASSERT_TRUE(s.takeOutput().startScan); }
void test_device_found_connects() { SpeedBikeSession s; s.tick(0); s.takeOutput(); s.deviceFound(1); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Connecting), static_cast<int>(s.state())); TEST_ASSERT_TRUE(s.takeOutput().connect); }
void test_link_connected_sends_notice() { SpeedBikeSession s; s.tick(0); s.takeOutput(); s.deviceFound(1); s.takeOutput(); s.linkConnected(2); TEST_ASSERT_TRUE(s.takeOutput().sendNotice); }
void test_request_sends_password() { SpeedBikeSession s; enterAwaiting(s); s.frame(passwordRequest(), 3); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Authenticating), static_cast<int>(s.state())); TEST_ASSERT_TRUE(s.takeOutput().sendPassword); }
void test_ack_starts_streaming() { SpeedBikeSession s; enterAwaiting(s); s.frame(passwordRequest(), 3); s.takeOutput(); s.frame(sessionAck(), 4); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Streaming), static_cast<int>(s.state())); }
void test_ack_without_dashboard_reports_no_data() { SpeedBikeSession s; enterAwaiting(s); s.frame(passwordRequest(), 3); s.takeOutput(); s.frame(sessionAck(), 4); s.takeOutput(); s.tick(3004); TEST_ASSERT_TRUE(s.takeOutput().noData); }
void test_authentication_timeout_reconnects() { SpeedBikeSession s; enterAwaiting(s); s.frame(passwordRequest(), 3); s.takeOutput(); s.tick(10003); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::ReconnectWait), static_cast<int>(s.state())); }
void test_dashboard_without_request_starts_streaming() { SpeedBikeSession s; enterAwaiting(s); s.frame(dashboardFrame(), 3); SessionOutput o=s.takeOutput(); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Streaming), static_cast<int>(s.state())); TEST_ASSERT_TRUE(o.hasRpm); TEST_ASSERT_EQUAL_UINT16(3000,o.rpm); }
void test_password_timeout_reconnects() { SpeedBikeSession s; enterAwaiting(s); s.tick(15002); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::ReconnectWait), static_cast<int>(s.state())); }
void test_disconnect_authenticating_reconnects() { SpeedBikeSession s; enterAwaiting(s); s.frame(passwordRequest(),3); s.takeOutput(); s.linkDisconnected(4); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::ReconnectWait), static_cast<int>(s.state())); }
void test_reconnect_wait_starts_scan() { SpeedBikeSession s; enterAwaiting(s); s.linkDisconnected(10); s.tick(2010); TEST_ASSERT_TRUE(s.takeOutput().startScan); }
void test_wraparound_timeout() { SpeedBikeSession s; s.tick(0); s.takeOutput(); s.deviceFound(1); s.takeOutput(); s.linkConnected(0xfffffff0u); s.takeOutput(); s.tick(0x3a80u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::AwaitingPasswordRequest),static_cast<int>(s.state())); s.tick(0x3a90u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::ReconnectWait),static_cast<int>(s.state())); s.tick(0x4260u); TEST_ASSERT_EQUAL_INT(static_cast<int>(SessionState::Scanning),static_cast<int>(s.state())); }
void test_no_data_sets_and_clears() { SpeedBikeSession s; enterAwaiting(s); s.frame(dashboardFrame(),10); s.takeOutput(); s.tick(3010); TEST_ASSERT_TRUE(s.takeOutput().noData); s.frame(dashboardFrame(),3011); TEST_ASSERT_FALSE(s.takeOutput().noData); }
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_boot_starts_scan);RUN_TEST(test_device_found_connects);RUN_TEST(test_link_connected_sends_notice);RUN_TEST(test_request_sends_password);RUN_TEST(test_ack_starts_streaming);RUN_TEST(test_ack_without_dashboard_reports_no_data);RUN_TEST(test_authentication_timeout_reconnects);RUN_TEST(test_dashboard_without_request_starts_streaming);RUN_TEST(test_password_timeout_reconnects);RUN_TEST(test_disconnect_authenticating_reconnects);RUN_TEST(test_reconnect_wait_starts_scan);RUN_TEST(test_wraparound_timeout);RUN_TEST(test_no_data_sets_and_clears);return UNITY_END();}
