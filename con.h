/* Служебный текст 8x8 прямо в видеоОЗУ (сообщения загрузчика и тестов) */
#pragma once
#include <stdint.h>
#define VRAM ((volatile uint8_t *)040000)
void con_puts(uint8_t row, uint8_t col, const char *s, uint8_t color);
void con_oct(uint8_t row, uint8_t col, uint16_t v, uint8_t color);
void con_clear(void);
