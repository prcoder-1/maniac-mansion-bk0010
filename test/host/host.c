/* Хостовая модель платформы: страницы СМК-512, загрузчик, служебные функции ядра.
 * Страница — массив 0100000 байт окна 0100000..0177777; far_* обращаются к нему
 * напрямую, как общая копия кода на БК — к подключённой странице. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scumm.h"

uint8_t smk_mem[16][0100000];
uint16_t smk_home = 0160, ld_resp, cur_page, far_sp;
const char *host_data_dir = ".";
unsigned host_loads, host_load_bytes;

static uint8_t *at(uint16_t page, uint16_t addr)
{
    if (page < 2 || page > 15 || addr < 0100000u || addr >= 0160000u)
    {
        fprintf(stderr, "far: адрес вне окна данных: страница %u адрес %06o\n", page, addr);
        abort();
    }
    return &smk_mem[page][addr - 0100000u];
}

uint8_t far_rb(uint16_t page, uint16_t addr) { return *at(page, addr); }
uint16_t far_rw(uint16_t page, uint16_t addr) { return (uint16_t)(at(page, addr)[0] | (*at(page, addr + 1) << 8)); }
void far_wb(uint16_t page, uint16_t addr, uint16_t v) { *at(page, addr) = (uint8_t)v; }
void far_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n) { if (n) { at(page, addr + n - 1); memcpy(dst, at(page, addr), n); } }
void far_wr(uint16_t page, uint16_t addr, const void *src, uint16_t n) { if (n) { at(page, addr + n - 1); memcpy(at(page, addr), src, n); } }
void far_fill(uint16_t page, uint16_t addr, uint16_t v, uint16_t n) { if (n) { at(page, addr + n - 1); memset(at(page, addr), v, n); } }
void far_cp(uint16_t dp, uint16_t da, uint16_t sp, uint16_t sa, uint16_t n)
{
    if (!n) return;
    at(dp, da + n - 1);
    at(sp, sa + n - 1);
    memmove(at(dp, da), at(sp, sa), n);
}

void smk_map(uint16_t page) { cur_page = page; }
void smk_code(uint16_t code) { cur_page = code; }

int16_t ld_file(const struct ld_req *q)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", host_data_dir, q->name);
    FILE *f = fopen(path, "rb");
    if (!f) { ld_resp = 1; return -1; }
    static uint8_t buf[070000 - 03200];
    size_t len = fread(buf, 1, sizeof buf, f);
    int more = fgetc(f) != EOF;
    fclose(f);
    if (more) { ld_resp = 2; return -1; }
    if (q->flags & LD_XOR) for (size_t i = 0; i < len; ++i) buf[i] ^= 0xFF;
    host_loads++;
    host_load_bytes += (unsigned)len;
    if (q->flags & LD_ALL)
    {
        for (int p = 2; p < 16; ++p) memcpy(&smk_mem[p][q->dest - 0100000u], buf, len);
        return (int16_t)len;
    }
    size_t k = len < q->split ? len : q->split;
    memcpy(&smk_mem[q->page][q->dest - 0100000u], buf, k);
    if (len > k) memcpy(&smk_mem[q->ovf_page][q->ovf_dest - 0100000u], buf + k, len - k);
    return (int16_t)len;
}

void fatal(const char *msg, uint16_t v)
{
    fprintf(stderr, "FATAL: %s %06o (%u)\n", msg, v, v);
    exit(2);
}
