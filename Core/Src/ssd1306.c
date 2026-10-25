#include <ssd1306.h>
#include <ssd1306_font.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* I2C control bytes: 0x00 = following bytes are commands, 0x40 = following bytes are display data. */
#define SSD1306_CTRL_CMD   0x00
#define SSD1306_CTRL_DATA  0x40

static I2C_HandleTypeDef *oled_hi2c;

/* One bit per pixel. Byte index = page * 128 + column, bit 0 = top pixel of the page. */
static uint8_t framebuffer[SSD1306_WIDTH * SSD1306_PAGES];
static uint8_t cursor_x, cursor_y;

static HAL_StatusTypeDef oled_write_cmds(const uint8_t *cmds, uint16_t len)
{
    return HAL_I2C_Mem_Write(oled_hi2c, SSD1306_I2C_ADDR, SSD1306_CTRL_CMD,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)cmds, len, 100);
}

SSD1306_Status SSD1306_Init(I2C_HandleTypeDef *hi2c)
{
    static const uint8_t init_seq[] = {
        0xAE,        /* display off */
        0xD5, 0x80,  /* clock divide ratio / oscillator frequency */
        0xA8, 0x3F,  /* multiplex ratio: 64 rows */
        0xD3, 0x00,  /* display offset: 0 */
        0x40,        /* start line: 0 */
        0x8D, 0x14,  /* enable the internal charge pump */
        0x20, 0x00,  /* horizontal addressing mode */
        0xA1,        /* segment remap (mirror horizontally) */
        0xC8,        /* COM scan direction remapped (mirror vertically) */
        0xDA, 0x12,  /* COM pins configuration for 128x64 */
        0x81, 0xCF,  /* contrast */
        0xD9, 0xF1,  /* pre-charge period */
        0xDB, 0x40,  /* VCOMH deselect level */
        0xA4,        /* output follows RAM content */
        0xA6         /* normal (not inverted) display */
    };
    static const uint8_t display_on = 0xAF;

    oled_hi2c = hi2c;
    HAL_Delay(100);  /* power-up time of the controller */

    if (oled_write_cmds(init_seq, sizeof(init_seq)) != HAL_OK) {
        return SSD1306_ERR_I2C;
    }

    /* Clear the display RAM first so random power-up content is never shown. */
    SSD1306_Clear();
    if (SSD1306_Update() != SSD1306_OK) {
        return SSD1306_ERR_I2C;
    }

    if (oled_write_cmds(&display_on, 1) != HAL_OK) {
        return SSD1306_ERR_I2C;
    }
    return SSD1306_OK;
}

void SSD1306_Clear(void)
{
    memset(framebuffer, 0, sizeof(framebuffer));
    cursor_x = 0;
    cursor_y = 0;
}

void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) {
        return;
    }
    uint16_t idx = x + (uint16_t)(y / 8) * SSD1306_WIDTH;
    if (on) {
        framebuffer[idx] |= (uint8_t)(1U << (y % 8));
    } else {
        framebuffer[idx] &= (uint8_t)~(1U << (y % 8));
    }
}

void SSD1306_SetCursor(uint8_t x, uint8_t y)
{
    cursor_x = x;
    cursor_y = y;
}

void SSD1306_WriteChar(char c)
{
    if (c == '\n') {
        cursor_x = 0;
        cursor_y += 8;
        return;
    }
    if (c == '\r') {
        cursor_x = 0;
        return;
    }
    if (c < 32 || c > 126) {
        c = '?';
    }
    if (cursor_x + 6 > SSD1306_WIDTH) {  /* wrap to the next text row */
        cursor_x = 0;
        cursor_y += 8;
    }

    const uint8_t *glyph = &font5x7[(c - 32) * 5];

    /* A character cell is 6x8 pixels: 5x7 glyph plus one blank column and one blank row. */
    for (uint8_t col = 0; col < 6; col++) {
        for (uint8_t row = 0; row < 8; row++) {
            uint8_t on = (col < 5 && row < 7) ? ((glyph[col] >> row) & 1U) : 0U;
            SSD1306_DrawPixel(cursor_x + col, cursor_y + row, on);
        }
    }
    cursor_x += 6;
}

void SSD1306_WriteString(const char *s)
{
    while (*s) {
        SSD1306_WriteChar(*s++);
    }
}

void SSD1306_Printf(const char *fmt, ...)
{
    char buf[32];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    SSD1306_WriteString(buf);
}

SSD1306_Status SSD1306_Update(void)
{
    for (uint8_t page = 0; page < SSD1306_PAGES; page++) {
        /* Set the column window to the full width and the page window to this single page. */
        uint8_t window[] = { 0x21, 0x00, SSD1306_WIDTH - 1, 0x22, page, page };
        if (oled_write_cmds(window, sizeof(window)) != HAL_OK) {
            return SSD1306_ERR_I2C;
        }
        if (HAL_I2C_Mem_Write(oled_hi2c, SSD1306_I2C_ADDR, SSD1306_CTRL_DATA,
                              I2C_MEMADD_SIZE_8BIT,
                              &framebuffer[page * SSD1306_WIDTH],
                              SSD1306_WIDTH, 100) != HAL_OK) {
            return SSD1306_ERR_I2C;
        }
    }
    return SSD1306_OK;
}
