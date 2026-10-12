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
void far_wr2d(uint16_t page, uint16_t addr, uint16_t ps, const void *buf, uint16_t bs, uint16_t w, uint16_t h)
{
    for (uint16_t y = 0; y < h; y++) far_wr(page, (uint16_t)(addr + y * ps), (const uint8_t *)buf + y * bs, w);
}
void far_rd2d(uint16_t page, uint16_t addr, uint16_t ps, void *buf, uint16_t bs, uint16_t w, uint16_t h)
{
    for (uint16_t y = 0; y < h; y++) far_rd((uint8_t *)buf + y * bs, page, (uint16_t)(addr + y * ps), w);
}
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

/* переменные ядра БК (platform.c): кэш байт-кода и место курсора */
#include "vm.h"
uint8_t vm_c_page, *vm_c_line;
uint16_t vm_c_addr, vm_c_gen;
uint8_t vm_lines[VM_LINES][VM_CACHE];
uint8_t vm_l_page[VM_LINES];
uint16_t vm_l_addr[VM_LINES], vm_l_gen[VM_LINES];
uint8_t vm_sb_num[VM_SB], vm_sb_page[VM_SB];
uint16_t vm_sb_addr[VM_SB], vm_sb_gen[VM_SB];
uint8_t cur_vis;
int16_t cur_x, cur_y;
uint8_t lt_mode;
FILE *host_trace;

/* тесты без VM (res, gfx, strips): вызовы модулей, которых в них нет; в vmtest и
 * sndtest — настоящие функции */
__attribute__((weak)) uint8_t isSoundRunning(uint8_t s) { (void)s; return 0; }
__attribute__((weak)) uint8_t isScriptInUse(uint16_t s) { (void)s; return 0; }
__attribute__((weak)) uint8_t costume_in_use(uint8_t c) { (void)c; return 0; }
__attribute__((weak)) void cost_info_reset(void) {}
__attribute__((weak)) void c64_cache_clear(void) {}
__attribute__((weak)) void drawRoomObjectsRange(uint8_t s0, uint8_t s1) { (void)s0; (void)s1; }

void fatal(const char *msg, uint16_t v)
{
    fprintf(stderr, "FATAL: %s %06o (%u)\n", msg, v, v);
    exit(2);
}
