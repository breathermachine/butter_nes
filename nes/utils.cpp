#include "utils.h"

unsigned char FlipByte(unsigned char in)
{
	return (unsigned char)(((in * 0x80200802ULL) & 0x0884422110ULL) * 0x0101010101ULL >> 32);
}