#include <unity.h>
#include "core/byte_ring.h"
#include "core/speedbike_protocol.h"
void test_fifo(){ByteRing ring;const uint8_t in[]={1,2};uint8_t out=0;ring.push(in,sizeof in);TEST_ASSERT_TRUE(ring.pop(out));TEST_ASSERT_EQUAL_UINT8(1,out);TEST_ASSERT_TRUE(ring.pop(out));TEST_ASSERT_EQUAL_UINT8(2,out);}
void test_overflow_resync(){ByteRing ring;uint8_t burst[300]{};for(uint16_t i=0;i<293;++i)burst[i]=static_cast<uint8_t>(i);const uint8_t frame[]={0x54,0x46,0x52,0x58,1,1,0xba};for(uint8_t i=0;i<7;++i)burst[293+i]=frame[i];ring.push(burst,sizeof burst);FrameDecoder decoder;uint8_t byte=0;while(ring.pop(byte))decoder.push(&byte,1);DecodedFrame decoded{};TEST_ASSERT_TRUE(decoder.next(decoded));TEST_ASSERT_EQUAL_HEX8(0x52,decoded.op);}
int main(int,char**){UNITY_BEGIN();RUN_TEST(test_fifo);RUN_TEST(test_overflow_resync);return UNITY_END();}
