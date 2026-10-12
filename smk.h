/* Память: СМК-512 в режиме ОЗУ10, страницы, загрузчик (loader.s) и дальние вызовы.
 *
 * Базовое ОЗУ: 01000..02777 загрузчик, 03000..03177 дальний стек и _cur_page,
 * 03200..037777 ядро и состояние (на время чтения файла уходит в SWAP), стек Си
 * вверху. Страница СМК: 0100000..0157777 данные (24 Кбайт), 0160000..0176777 —
 * общая копия кода (REPL), одинаковая во всех страницах 2..15. */
#pragma once
#include <stdint.h>

#define PG_BANKA 2   /* код: VM, объекты, глаголы, реплики, главный цикл */
#define PG_BANKB 3   /* код: актёры, костюмы, камера */
#define PG_BITMAP 4  /* фон комнаты (БК 2bpp, сжатие 4:5) */
#define PG_MASK  5   /* z-маска, StripTable, шрифт, хвост файла */
#define PG_SWAP  6   /* копия базового ОЗУ на время чтения файла */
/* PG_SWAP за копией базового ОЗУ (0100000..0134577): символы фона v1 и таблицы
 * сборки строк (v1cell.s, gfx_v1.c) */
#define V1_CHR    0134600u   /* charMap, 2048 байт */
#define V1_MCH    0140600u   /* maskChar, до 2048 байт */
#define V1_GPE    0144600u   /* [11][256] */
#define V1_GPT    0152200u   /* [2][256] */
#define V1_GPM3   0153200u   /* [256] */
#define V1_C3T    0153600u   /* [5][8][2][8] (до 0155000; дальше — рабочее v1cell.s) */
/* кэш перекодированных кадров актёров — в PG_MASK за маской (110 x 136 до 0135160)
 * и таблицами proc3.s (0136000..0137777) */
#define C64_PAGE  PG_MASK
#define C64_CACHE 0140000u   /* 24 слота по 128 байт (до 0146000) */
#define SPR_SCR   0146000u   /* PG_MASK: построение спрайта (до 512 байт) */
#define PG_BANKC 7   /* код: экран */
#define PG_CACHE 8   /* 8..13 — кэш ресурсов */
#define PG_LAST  13
#define PG_BANKD 14  /* код: ресурсы, звук, комната и боксы; выше кода — куча ресурсов */
#define PG_SPR   15  /* кэш спрайтов актёров v1 (costume_r.c, sprblit.s) */

#define PAGE_ADDR 0100000u        /* адреса окна страницы — целые, не указатели */
#define PAGE_DATA_SIZE 060000u
#define REPL_ADDR 0160000u
#define OVF_ADDR  0152000u        /* хвост файла длиннее 24 Кбайт — в PG_MASK */

#define LD_XOR 1     /* инвертировать байты (XOR 0xFF файлов LFL) */
#define LD_ALL 2     /* во все страницы 2..15 (общая копия кода) */
#define LD_LOW 010   /* файл короче 034600 байт (буфер ниже экрана): «малый экран» не нужен */

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

int16_t ld_file(const struct ld_req *q);
void smk_map(uint16_t page);
void smk_code(uint16_t code);
uint16_t smk_probe(void);
extern uint16_t smk_home, ld_resp, cur_page, far_sp;
