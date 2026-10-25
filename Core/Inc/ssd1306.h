#ifndef INC_SSD1306_H_
#define INC_SSD1306_H_

#include "main.h"

#define SSD1306_I2C_ADDR  (0x3C << 1)   /* some modules use 0x3D */
#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT    64
#define SSD1306_PAGES     (SSD1306_HEIGHT / 8)

typedef enum {
    SSD1306_OK = 0,
    SSD1306_ERR_I2C
} SSD1306_Status;

SSD1306_Status SSD1306_Init(I2C_HandleTypeDef *hi2c);

/* Drawing functions only modify the RAM framebuffer. */
void SSD1306_Clear(void);
void SSD1306_DrawPixel(uint8_t x, uint8_t y, uint8_t on);
void SSD1306_SetCursor(uint8_t x, uint8_t y);   /* in pixels; y in steps of 8 gives text rows */
void SSD1306_WriteChar(char c);
void SSD1306_WriteString(const char *s);
void SSD1306_Printf(const char *fmt, ...);      /* max 31 characters per call */

/* Push the framebuffer to the display over I2C (~100 ms at 100 kHz). */
SSD1306_Status SSD1306_Update(void);

#endif /* INC_SSD1306_H_ */
