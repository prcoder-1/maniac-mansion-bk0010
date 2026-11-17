/* Модуль AZБК изнутри (az.c, az_ui.c, az_act.c, v4pix.s): регистры, страницы памяти AZ,
 * окна 12/13, пакеты блиттера. Снаружи модуля — только az.h. */
#pragma once
#include <stdint.h>
#include "gfx.h"

#define REG(a) (*(volatile uint16_t *)(a))

#define AZ_VMODE 076136u   /* 4 бит, 3 слоя, строка 256 слов, x2 -> 512, x3 -> 256 строк, страницы по кадру, рулон 65536 */
#define AZ_ROW0 28         /* строка экрана y = 0 MM */
#define AZ_X0 96           /* точка экрана x = 0 MM (вид — по центру 512) */

/* страницы памяти AZ по 4 Кбайт (адрес слова = страница * 2048), выше 1 Мбайт */
#define AZ_L0 01000u       /* слой 0: 32 страницы (64К слов) */
#define AZ_L1 01040u
#define AZ_L2 01100u
#define AZ_STORE 01140u    /* склад фона: полоса — 256 слов, 120 полос */
#define AZ_CMDS 01160u     /* пакет шага (2 страницы, до 510 команд) */
#define AZ_CMDI 01162u     /* пакет «сейчас» */
#define AZ_SENT 01164u     /* маяк конца пакета */
#define AZ_CURS 01165u     /* картинки курсора: 8 страниц */
#define AZ_CANV 01200u     /* холсты актёров: по странице, до 64 на шаг */
#define AZ_CANV_N 64
/* SRC блиттера (что он читает) — с адреса, кратного 32 словам: плата берёт начало с
 * ближайшего такого вниз (фон под актёрами «проседал», курсор при мигании рассыпался) */
#define AZ_HI(p) ((uint16_t)(p) >> 5)              /* биты 23..16 адреса слова */
#define AZ_LO(p) ((uint16_t)(((p) & 037) << 11))   /* биты 15..0 */

extern uint16_t az_w12, az_w13, az_nq, az_ni, az_canv;
extern uint8_t az_vs, az_lmode;
extern int16_t az_lx0, az_lx1, az_ly0, az_ly1;   /* фонарик: точки вида, строки комнаты */
void win12(uint16_t p);
void win13(uint16_t p);
void win_back(void);
void az_smk(uint16_t page);   /* azsmk.s: страница СМК, _cur_page прежний */
void az_back(void);
void smk_to(uint16_t page);   /* az.c: то же и окна 12, 13 — снова от СМК */
void smk_back(void);
void blt_idle(void);
void blt_now(void);
void bq(uint16_t hi, uint16_t slo, uint16_t dlo, uint16_t op, uint16_t w, uint16_t h, uint16_t pitch, uint16_t y, uint16_t k);
void bi(uint16_t hi, uint16_t slo, uint16_t dlo, uint16_t op, uint16_t w, uint16_t h, uint16_t pitch, uint16_t y, uint16_t k);

uint16_t v4_pix(struct v2p *p);   /* v4pix.s */
void az_cur_init(void);           /* az_ui.c */
void az_cur_lost(void);
