/* Запуск: СМК-512, общая копия кода и банки из файлов ANDOS; затем проверка M2:
 * фон комнат (по клавише — следующая), время декодирования в отсчётах 366 Гц */
#include "scumm.h"
#include "gfx.h"
#include "con.h"

void bm_blit(uint16_t src, uint16_t stride, uint16_t vram, uint16_t ncols, uint16_t rows);

#define TMR_LIMIT (*(volatile uint16_t *)0177706)
#define TMR_COUNT (*(volatile uint16_t *)0177710)
#define TMR_CSR   (*(volatile uint16_t *)0177712)
#define KBD_CSR   (*(volatile uint16_t *)0177660)
#define KBD_DATA  (*(volatile uint16_t *)0177662)

void fatal(const char *msg, uint16_t v)
{
    con_clear();
    con_puts(10, 0, msg, 3);
    con_oct(11, 0, v, 3);
    for (;;) ;
}

static void load_code(const char *name, uint16_t flags, uint16_t page, uint16_t dest)
{
    struct ld_req q = { name, flags, page, dest, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR };
    if (ld_file(&q) < 0) fatal(name, ld_resp);
}

int main(void)
{
    con_clear();
    smk_home = smk_probe();
    if (!smk_home) fatal("NEED SMK-512", 0);
    cur_page = smk_home;
    far_sp = 03174;
    load_code("REPL", LD_ALL, 0, REPL_ADDR);
    load_code("BANKA", 0, PG_BANKA, PAGE_ADDR);
    load_code("BANKB", 0, PG_BANKB, PAGE_ADDR);
    smk_map(PG_BANKA);
    TMR_LIMIT = 0177777;
    TMR_CSR = 0160;   /* счёт, делители 16 и 4: 366 Гц */

    res_init();
    gfx_init_pal();
    static const uint8_t rooms[] = { 1, 2, 44, 29, 12, 9 };
    for (uint8_t i = 0;; i = (uint8_t)((i + 1) % sizeof rooms))
    {
        uint16_t a;
        uint8_t pg = res_get(rtRoom, rooms[i], &a);
        uint16_t t0 = TMR_COUNT;
        room_decode(pg, a);
        uint16_t t = (uint16_t)(t0 - TMR_COUNT);
        con_clear();
        bm_blit(PAGE_ADDR, bm_stride, 040000 + 44 * 64, 64, 128);
        con_puts(0, 0, "ROOM", 2);
        con_oct(0, 5, rooms[i], 2);
        con_puts(0, 12, "T", 2);
        con_oct(0, 14, t, 2);
        uint16_t t1 = TMR_COUNT;
        bm_blit(PAGE_ADDR, bm_stride, 040000 + 44 * 64, 64, 128);
        con_oct(1, 14, (uint16_t)(t1 - TMR_COUNT), 2);
        while (!(KBD_CSR & 0200)) ;
        (void)KBD_DATA;
    }
}
