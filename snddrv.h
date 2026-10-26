/* Звуковой драйвер AY (snddrv.c, drvhdr.s): страница 1 ОЗУ БК-0011М в окне 040000.
 * Из игры — через snd_call ядра (irq.s): страница драйвера в окне, прерывания
 * закрыты; такты — из кадрового прерывания. */
#pragma once
#include <stdint.h>

#define DRV_PAGE11 1          /* страница ОЗУ 11М */
#define DRV_ADDR 040000u
#define DRV_END  057400u      /* код и данные драйвера — до слотов звуков */
/* слоты данных звуков: большой (звуки 55 и 58 — по 5876 байт) и малый (остальные
 * не длиннее 2491) */
#define DRV_SLOT0 057400u
#define DRV_SLOT0_N 5888u
#define DRV_SLOT1 073000u
#define DRV_SLOT1_N 2560u

/* входы драйвера (таблица переходов в drvhdr.s, по 4 байта) */
enum { DRV_IRQ, DRV_INIT, DRV_START, DRV_STOP, DRV_STOPALL, DRV_MODE };

uint16_t snd_call(uint16_t entry, uint16_t a, uint16_t b, uint16_t c);
void w1_copy(uint16_t page11, uint16_t smk_page, uint16_t n);
void ay_mute(void);

/* почтовый ящик в ядре (irq.s): пишет драйвер */
extern volatile uint16_t snd_on, snd_cur, snd_next, snd_mtimer, ay_dirty, snd_pcjr;
