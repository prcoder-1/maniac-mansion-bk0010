/* Память: СМК-512 в режиме All (smk.inc), страницы, загрузчик (loader.s) и дальние вызовы.
 *
 * Базовое ОЗУ: 01000..02777 загрузчик, 03000..03177 дальний стек и _cur_page,
 * 03200..037777 ядро и состояние (на время чтения файла уходит в SWAP), стек Си
 * вверху. Страница СМК: 0100000..0157777 данные (24 Кбайт), 0160000..0176777 —
 * общая копия кода (REPL), одинаковая во всех страницах 2..15. */
#pragma once
#include <stdint.h>

#define PG_BANKA 2   /* код: VM, объекты, глаголы */
#define PG_BANKB 3   /* код: цикл, актёры, графика, ресурсы */
#define PG_BITMAP 4  /* фон комнаты (БК 2bpp, сжатие 4:5) */
#define PG_MASK  5   /* z-маска, StripTable, шрифт, хвост файла */
#define PG_SWAP  6   /* копия базового ОЗУ на время чтения файла */
#define PG_BANKC 7   /* код: ресурсы, экран, звук */
#define PG_CACHE 8   /* 8..14 — кэш ресурсов */
#define PG_LAST  14
#define PG_BANKD 15  /* код: комната и боксы */

#define PAGE_ADDR 0100000u        /* адреса окна страницы — целые, не указатели */
#define PAGE_DATA_SIZE 060000u
#define REPL_ADDR 0160000u
#define OVF_ADDR  0152000u        /* хвост файла длиннее 24 Кбайт — в PG_MASK */

#define LD_XOR 1     /* инвертировать байты (XOR 0xFF файлов LFL) */
#define LD_ALL 2     /* во все страницы 2..15 (общая копия кода) */

struct ld_req
{
    const char *name;
    uint16_t flags;
    uint16_t page;
    uint16_t dest;
    uint16_t split;      /* сколько байт положить в page:dest, остальное — в хвост */
    uint16_t ovf_page;
    uint16_t ovf_dest;
    uint16_t fblk, flen, fchk;   /* файл подряд с блока fblk (ftab.py), 0 — через ДОС */
};
extern uint16_t ld_fast;   /* последний файл прочитан драйвером ПЗУ: экран цел */

int16_t ld_file(const struct ld_req *q);
void smk_map(uint16_t page);
void smk_code(uint16_t code);
uint16_t smk_probe(void);
void smk_twins(void);

/* БК-0011М (irq.s): тени регистров 0177662 и 0177716, счётчики кадрового прерывания */
#define VCTL_IRQ_OFF 040000u   /* 0177662: бит 14 — кадровое прерывание запрещено */
#define VCTL_PAGE6   0100000u  /* 0177662: бит 15 — на экране страница 6 */
#define VCTL_PAL(n)  ((uint16_t)(n) << 8)
void vctl_set(uint16_t v);
void w1_map(uint16_t page11);   /* страница ОЗУ 11М в окне 040000 */
void scr_copy(uint16_t src11, uint16_t dst11, uint16_t row0, uint16_t nrows);
void scr_fill(uint16_t page11);
/* две экранные страницы (platform.c): видимая 5 или 6; полная перерисовка вида — в
 * скрытой (gfx.c scr_begin), смена — в конце шага (scr_flip) в кадровом прерывании */
extern uint8_t scr_shown, flip_pending;
extern uint16_t ui_gen, pg_ui_gen[2], vctl_next;
void scr_flip(void);
void vctl_flip(uint16_t v);
extern volatile uint16_t vctl, vctl_req, mem11, jiffies, frames;
extern uint16_t smk_home, ld_resp, cur_page, far_sp;
