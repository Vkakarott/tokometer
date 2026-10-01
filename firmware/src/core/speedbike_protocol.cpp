#include "speedbike_protocol.h"
uint8_t speedbikeCrc(const uint8_t *bytes,size_t length){uint16_t sum=0;for(size_t i=0;i<length;++i)sum+=bytes[i];return static_cast<uint8_t>(0x100-(sum&0xff));}
size_t buildFrame(uint8_t op,uint8_t command,const uint8_t *payload,uint8_t length,uint8_t *out,size_t capacity){const size_t total=6u+length;if(capacity<total)return 0;out[0]=0x54;out[1]=0x46;out[2]=op;out[3]=command;out[4]=length;for(uint8_t i=0;i<length;++i)out[5+i]=payload[i];out[5+length]=speedbikeCrc(out,5+length);return total;}
size_t buildSessionNotice(uint8_t *out,size_t capacity){const uint8_t payload[]={0x0a};return buildFrame(0x57,0x58,payload,sizeof payload,out,capacity);}
size_t buildPasswordFrame(uint8_t *out,size_t capacity){const uint8_t payload[]={0x0b,1,1,1,1,1,1};return buildFrame(0x57,0x58,payload,sizeof payload,out,capacity);}
