/* Память: СМК-512 в режиме ОЗУ10, страницы, загрузчик (loader.s) и дальние вызовы.
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
/* PG_SWAP за копией базового ОЗУ (0100000..0134577): таблицы символов фона v1 и
 * кэш перекодированных кадров актёров */
#define V1_CHR    0134600u   /* charMap, 2048 байт */
#define V1_MCH    0140600u   /* maskChar, до 2048 байт */
#define C64_CACHE 0144600u   /* 32 слота по 128 байт (до 0154600) */
#define PG_BANKC 7   /* код: ресурсы, экран, звук */
#define PG_CACHE 8   /* 8..15 — кэш ресурсов */
#define PG_LAST  15

#define PAGE_ADDR 0100000u        /* адреса окна страницы — целые, не указатели */
#define PAGE_DATA_SIZE 060000u
#define REPL_ADDR 0160000u
#define OVF_ADDR  0152000u        /* хвост файла длиннее 24 Кбайт — в PG_MASK */

#define LD_XOR 1     /* инвертировать байты (XOR 0xFF файлов LFL) */
#define LD_ALL 2     /* во все страницы 2..15 (общая копия кода) */
#define LD_WRITE 4   /* запись: split байт из page:dest в файл */

struct ld_req
{
    const char *name;
    uint16_t flags;
    uint16_t page;
    uint16_t dest;
    uint16_t split;      /* сколько байт положить в page:dest, остальное — в хвост */
    uint16_t ovf_page;
    uint16_t ovf_dest;
};

int16_t ld_file(const struct ld_req *q);
void smk_map(uint16_t page);
void smk_code(uint16_t code);
uint16_t smk_probe(void);
extern uint16_t smk_home, ld_resp, cur_page, far_sp;
