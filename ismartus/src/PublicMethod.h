
#ifndef _PUBLIC_METHOD_
#define _PUBLIC_METHOD_

unsigned short byteToUShort(unsigned char dataH,unsigned char dataL);
void uShortToByte(unsigned short sdata,unsigned char *cdata);
int getCheckSum(const unsigned char* buffer, int length, int crc);
unsigned char compareChar(unsigned char *a,unsigned char *b,int length);
void exchangeData(unsigned char *a,unsigned char *b,int lenght);

#endif
