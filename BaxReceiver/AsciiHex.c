// Karim Ladha 2014
// Utils for converting between binary and ascii hex
#ifndef _ASCII_HEX_H_
#define _ASCII_HEX_H_

unsigned short WriteBinaryToHex(char* dest, void* source, unsigned short len, unsigned char littleEndian)
{
	unsigned short ret = (len*2);
	unsigned char* ptr = source;
	char temp;

	if(littleEndian) ptr += len-1; // Start at MSB

	for(;len>0;len--)
	{
		temp = '0' + (*ptr >> 4);
		if(temp>'9')temp += ('A' - '9' - 1);  
		*dest++ = temp;
		temp = '0' + (*ptr & 0xf);
		if(temp>'9')temp += ('A' - '9' - 1); 
		*dest++ = temp;

		if(littleEndian)ptr--;
		else ptr++;
	}
	*dest = '\0';
	return ret;
}

unsigned short ReadHexToBinary(unsigned char* dest, const char* source, unsigned short maxLen)
{
	unsigned short read = 0;

	char hex1, hex2;
	for(;maxLen>0;maxLen--)
	{
		// First char
		if		(*source >= '0' && *source <= '9') hex1 = *source - '0';
		else if	(*source >= 'a' && *source <= 'f') hex1 = *source - 'a' + 0x0A;
		else if	(*source >= 'A' && *source <= 'F') hex1 = *source - 'A' + 0x0A;
		else break;

		source++;

		// Second char
		if		(*source >= '0' && *source <= '9') hex2 = *source - '0';
		else if	(*source >= 'a' && *source <= 'f') hex2 = *source - 'a' + 0x0A;
		else if	(*source >= 'A' && *source <= 'F') hex2 = *source - 'A' + 0x0A;
		else break;

		source++;

		// Regardless of endianess, pairs are assembled LSB on right
		*dest = (unsigned char)hex2 | (hex1<<4);	// hex1 is the msb

		// Increment count and dest
		read++;
		dest++;
	}

	return read;
}

#endif
