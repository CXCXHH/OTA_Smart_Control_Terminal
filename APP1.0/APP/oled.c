#include "oled.h"
#include "bsp_iic.h"
#include "delay.h"

static void WriteCmd(uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};  /* Co=0, D/C#=0 (command) */

    IIC_WriteBuf(OLED_ADDR, buf, 2);
}

static void WriteDat(uint8_t dat)
{
    uint8_t buf[2] = {0x40, dat};  /* Co=0, D/C#=1 (data) */

    IIC_WriteBuf(OLED_ADDR, buf, 2);
}

void OLED_Init(void)
{
    Delay_ms(100);
    WriteCmd(0xAE); WriteCmd(0x20); WriteCmd(0x10);
    WriteCmd(0xB0); WriteCmd(0xC8); WriteCmd(0x00);
    WriteCmd(0x10); WriteCmd(0x40); WriteCmd(0x81);
    WriteCmd(0xFF); WriteCmd(0xA1); WriteCmd(0xA6);
    WriteCmd(0xA8); WriteCmd(0x3F); WriteCmd(0xA4);
    WriteCmd(0xD3); WriteCmd(0x00); WriteCmd(0xD5);
    WriteCmd(0xF0); WriteCmd(0xD9); WriteCmd(0x22);
    WriteCmd(0xDA); WriteCmd(0x12); WriteCmd(0xDB);
    WriteCmd(0x20); WriteCmd(0x8D); WriteCmd(0x14);
    WriteCmd(0xAF);
}

void OLED_Clear(void)
{
    uint8_t m, n;
    for (m = 0; m < 8; m++) {
        WriteCmd(0xB0 + m);
        WriteCmd(0x00);
        WriteCmd(0x10);
        for (n = 0; n < 128; n++)
            WriteDat(0x00);
    }
}

static void OLED_SetPos(uint8_t x, uint8_t y)
{
    WriteCmd(0xB0 + y);
    WriteCmd(((x & 0xF0) >> 4) | 0x10);
    WriteCmd((x & 0x0F) | 0x01);
}


void OLED_ShowStr(uint8_t x, uint8_t y, const char ch[], uint8_t size)
{
    uint8_t c, i;
    while (*ch)
    {
        c = *ch - 32;
        if (c > 94) c = 0;

        if (size == 0)  /* 6x8 font */
        {
            if (x > 126) { x = 0; y++; }
            OLED_SetPos(x, y);
            for (i = 0; i < 6; i++)
                WriteDat(F6x8[c][i]);
            x += 6;
        }
        else  /* 8x16 font */
        {
            if (x > 120) { x = 0; y += 2; }
            OLED_SetPos(x, y);
            for (i = 0; i < 8; i++)
                WriteDat(F8x16[c][i]);
            OLED_SetPos(x, y + 1);
            for (i = 0; i < 8; i++)
                WriteDat(F8x16[c][8 + i]);
            x += 8;
        }
        ch++;
    }
}
