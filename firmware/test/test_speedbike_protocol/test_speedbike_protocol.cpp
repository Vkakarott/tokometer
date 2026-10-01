#include <unity.h>
#include "core/speedbike_protocol.h"
void test_crc(){const uint8_t a[]={0x54,0x46,0x52,0x58,1,1};const uint8_t b[]={0x54,0x46,0x41,0x5a,1,1};TEST_ASSERT_EQUAL_HEX8(0xba,speedbikeCrc(a,sizeof a));TEST_ASSERT_EQUAL_HEX8(0xc9,speedbikeCrc(b,sizeof b));}
void test_frames(){uint8_t out[16]{};const uint8_t n[]={0x54,0x46,0x57,0x58,1,0x0a,0xac};const uint8_t p[]={0x54,0x46,0x57,0x58,7,0x0b,1,1,1,1,1,1,0x9f};TEST_ASSERT_EQUAL_UINT(sizeof n,buildSessionNotice(out,sizeof out));TEST_ASSERT_EQUAL_UINT8_ARRAY(n,out,sizeof n);TEST_ASSERT_EQUAL_UINT(sizeof p,buildPasswordFrame(out,sizeof out));TEST_ASSERT_EQUAL_UINT8_ARRAY(p,out,sizeof p);}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_crc);RUN_TEST(test_frames);return UNITY_END();}
