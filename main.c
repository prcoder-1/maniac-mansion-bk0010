/* Запуск: СМК-512, общая копия кода и банки из файлов ANDOS; затем проверка M1 */
#include "scumm.h"
#include "con.h"
#include "test/m1ref.h"

int16_t a_test(int16_t x);

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
    far_sp = 02374;
    load_code("REPL", LD_ALL, 0, REPL_ADDR);
    load_code("BANKA", 0, PG_BANKA, PAGE_ADDR);
    load_code("BANKB", 0, PG_BANKB, PAGE_ADDR);
    smk_map(PG_BANKA);

    res_init();
    uint16_t got[8][2];
    uint8_t n = sizeof m1ref / sizeof m1ref[0];
    for (uint8_t i = 0; i < n; ++i)
    {
        uint16_t a, size = 0, sum = 0;
        uint8_t pg = res_get(m1ref[i].type, m1ref[i].idx, &a);
        if (pg)
        {
            size = far_rw(pg, a);
            for (uint16_t k = 0; k < size; ++k) sum += far_rb(pg, a + k);
        }
        got[i][0] = size;
        got[i][1] = sum;
    }
    int16_t r = a_test(5);
    con_clear();
    con_puts(0, 0, "MANIAC M1", 2);
    uint8_t bad = r != 11;
    for (uint8_t i = 0; i < n; ++i)
    {
        uint8_t ok = got[i][0] == m1ref[i].size && got[i][1] == m1ref[i].sum;
        con_oct((uint8_t)(2 + i), 0, m1ref[i].type, 1);
        con_oct((uint8_t)(2 + i), 7, m1ref[i].idx, 1);
        con_oct((uint8_t)(2 + i), 14, got[i][0], ok ? 2 : 3);
        con_oct((uint8_t)(2 + i), 21, got[i][1], ok ? 2 : 3);
        if (!ok) ++bad;
    }
    con_puts(12, 0, bad ? "M1 FAIL" : "M1 OK", bad ? 3 : 2);
    for (;;) ;
}
