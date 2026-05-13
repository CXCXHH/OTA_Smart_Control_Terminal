#include "oled_boot.h"
#include "bsp_iic.h"
#include "delay.h"

#define OLED_ADDR 0x78

static const uint8_t Font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x7C,0x12,0x11,0x12,0x7C}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
};

static uint8_t FontIndex(char ch)
{
    if(ch >= 'a' && ch <= 'z')
        ch = (char)(ch - 32);
    if(ch == ' ')
        return 0;
    if(ch >= '0' && ch <= '9')
        return (uint8_t)(1 + ch - '0');
    if(ch >= 'A' && ch <= 'Z')
        return (uint8_t)(11 + ch - 'A');
    if(ch == ':')
        return 37;
    if(ch == '-')
        return 38;
    return 0;
}

static void OLED_Write(uint8_t control, uint8_t data)
{
    IIC_Start();
    IIC_Send_Byte(OLED_ADDR);
    if(IIC_wait_Ack(100) != 0) goto out;
    IIC_Send_Byte(control);
    if(IIC_wait_Ack(100) != 0) goto out;
    IIC_Send_Byte(data);
    (void)IIC_wait_Ack(100);
out:
    IIC_Stop();
}

static void OLED_Cmd(uint8_t cmd)
{
    OLED_Write(0x00, cmd);
}

static void OLED_Data(uint8_t data)
{
    OLED_Write(0x40, data);
}

void OLED_Boot_Init(void)
{
    Delay_ms(20);
    OLED_Cmd(0xAE); OLED_Cmd(0x20); OLED_Cmd(0x10);
    OLED_Cmd(0xB0); OLED_Cmd(0xC8); OLED_Cmd(0x00);
    OLED_Cmd(0x10); OLED_Cmd(0x40); OLED_Cmd(0x81);
    OLED_Cmd(0xFF); OLED_Cmd(0xA1); OLED_Cmd(0xA6);
    OLED_Cmd(0xA8); OLED_Cmd(0x3F); OLED_Cmd(0xA4);
    OLED_Cmd(0xD3); OLED_Cmd(0x00); OLED_Cmd(0xD5);
    OLED_Cmd(0xF0); OLED_Cmd(0xD9); OLED_Cmd(0x22);
    OLED_Cmd(0xDA); OLED_Cmd(0x12); OLED_Cmd(0xDB);
    OLED_Cmd(0x20); OLED_Cmd(0x8D); OLED_Cmd(0x14);
    OLED_Cmd(0xAF);
    OLED_Boot_Clear();
}

void OLED_Boot_Clear(void)
{
    uint8_t page;
    uint8_t col;

    for(page = 0; page < 8; page++)
    {
        OLED_Cmd((uint8_t)(0xB0 + page));
        OLED_Cmd(0x00);
        OLED_Cmd(0x10);
        for(col = 0; col < 128; col++)
            OLED_Data(0x00);
    }
}

void OLED_Boot_ShowLine(uint8_t line, const char *str)
{
    uint8_t i;
    uint8_t col;

    if(line > 7)
        return;

    OLED_Cmd((uint8_t)(0xB0 + line));
    OLED_Cmd(0x00);
    OLED_Cmd(0x10);

    for(col = 0; col < 128; col++)
        OLED_Data(0x00);

    OLED_Cmd((uint8_t)(0xB0 + line));
    OLED_Cmd(0x00);
    OLED_Cmd(0x10);

    col = 0;
    while(*str && col < 126)
    {
        const uint8_t *glyph = Font5x7[FontIndex(*str++)];
        for(i = 0; i < 5; i++)
            OLED_Data(glyph[i]);
        OLED_Data(0x00);
        col += 6;
    }
}

static void OLED_Boot_ClearLine(uint8_t line)
{
    uint8_t col;

    OLED_Cmd((uint8_t)(0xB0 + line));
    OLED_Cmd(0x00);
    OLED_Cmd(0x10);

    for(col = 0; col < 128; col++)
        OLED_Data(0x00);
}

void OLED_Boot_ShowLine2x(uint8_t line, const char *str)
{
    uint8_t i;
    uint8_t repeat;
    uint8_t col;
    uint8_t row;

    if(line > 6)
        return;

    OLED_Boot_ClearLine(line);
    OLED_Boot_ClearLine((uint8_t)(line + 1U));

    for(row = 0; row < 2; row++)
    {
        const char *p = str;

        OLED_Cmd((uint8_t)(0xB0 + line + row));
        OLED_Cmd(0x00);
        OLED_Cmd(0x10);

        col = 0;
        while(*p && col < 126)
        {
            const uint8_t *glyph = Font5x7[FontIndex(*p++)];
            for(i = 0; i < 5; i++)
            {
                uint8_t src = glyph[i];
                uint8_t out = 0;
                uint8_t bit;
                uint8_t base = (uint8_t)(row * 4U);

                for(bit = 0; bit < 4; bit++)
                {
                    if(src & (uint8_t)(1U << (base + bit)))
                        out |= (uint8_t)(3U << (bit * 2U));
                }

                for(repeat = 0; repeat < 2; repeat++)
                    OLED_Data(out);
                col += 2;
            }
            OLED_Data(0x00);
            OLED_Data(0x00);
            col += 2;
        }
    }
}
