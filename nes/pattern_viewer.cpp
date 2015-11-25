#define _CRT_SECURE_NO_WARNINGS

#include "cpu.h" 
#include <stdio.h>
#include <stdlib.h>

void show_patterns(const char *fname)
{
    FILE *f = fopen(fname, "rb");
    if (!f) return;

    INESHeader header;
    fread(&header, sizeof(header), 1, f);
    
    int chrRomBytes = header.chrRomSize * 8192;
    unsigned char *buffer = new unsigned char[chrRomBytes];
    
    fseek(f, header.prgRomSize * 16384, SEEK_CUR);

    int y = fread(buffer, 1, chrRomBytes, f);

    char p = buffer[0];
    
    int tile = 0;
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
        {
            //show_tile(buffer, tile++, x * 8, y * 8);
        }
    }
    
    fclose(f);
}


