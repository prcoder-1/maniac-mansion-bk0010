/* Сохранение и загрузка игры (банк C). Как ScummEngine::saveState/loadState, но
 * снимком памяти, а не списком полей: всё состояние игры (state.c), данные
 * банков A и B (скрипты, глаголы, инвентарь, строки, актёры, боксы) и блоки
 * кучи (инвентарь, имена) — в файл MMSAVEn через EMT 36. Банк C (кэш ресурсов,
 * экран, звук) не сохраняется: после загрузки звук остановлен, комната
 * перечитана, экран перерисован целиком. Образ собирается в PG_BITMAP — фон
 * комнаты всё равно декодируется заново. Заголовок сверяется со сборкой. */
#include "scumm.h"

#define SV_MAGIC 0x564D   /* "MV" */
#define SV_VER 1
#define SV_PAGE PG_BITMAP

extern uint8_t gstate_start[], gstate_end[];
extern uint8_t kbd_buf[16];
extern volatile uint16_t kbd_head, kbd_tail;

struct SvHead { uint16_t magic, ver, gs, a_start, a_len, b_start, b_len; };

static void head_now(struct SvHead *h)
{
    h->magic = SV_MAGIC;
    h->ver = SV_VER;
    h->gs = (uint16_t)(gstate_end - gstate_start);
    h->a_start = save_dataA();
    h->a_len = res_addr;
    h->b_start = save_dataB();
    h->b_len = res_addr;
}

static uint16_t wait_key(void)
{
    while (kbd_head == kbd_tail) ;
    uint16_t k = kbd_buf[kbd_tail] & 0177;
    kbd_tail = (kbd_tail + 1) & 15;
    return k;
}

static int16_t sv_file(uint8_t slot, uint16_t flags, uint16_t len)
{
    char name[8] = "MMSAVE0";
    name[6] = (char)('0' + slot);
    struct ld_req q = { name, flags, SV_PAGE, PAGE_ADDR, len, PG_MASK, OVF_ADDR };
    int16_t r = ld_file(&q);
    /* фон комнаты в PG_BITMAP затёрт, экран — тоже */
    gfx_room_changed();
    fullRedraw = 1;
    screen_trashed = 1;
    return r;
}

/* 0 — удачно; иначе код ошибки (1 образ не помещается, 2 запись, 3 чтение,
 * 4 чужая сборка) */
static uint8_t do_save(uint8_t slot)
{
    struct SvHead h;
    head_now(&h);
    uint16_t off = PAGE_ADDR;
    far_wr(SV_PAGE, off, &h, sizeof h);
    off += sizeof h;
    far_wr(SV_PAGE, off, gstate_start, h.gs);
    off += h.gs;
    far_cp(SV_PAGE, off, PG_BANKA, h.a_start, h.a_len);
    off += h.a_len;
    far_cp(SV_PAGE, off, PG_BANKB, h.b_start, h.b_len);
    off += h.b_len;
    sv_page = SV_PAGE;
    sv_off = off;
    heapA(1);
    heapV(1);
    heapB(1);
    if (!sv_off) return 1;   /* не поместилось в страницу */
    return sv_file(slot, LD_WRITE, sv_off - PAGE_ADDR) >= 0 ? 0 : 2;
}

static uint8_t do_load(uint8_t slot)
{
    struct SvHead h, now;
    if (sv_file(slot, 0, PAGE_DATA_SIZE) < (int16_t)sizeof h) return 3;
    far_rd(&h, SV_PAGE, PAGE_ADDR, sizeof h);
    head_now(&now);
    if (h.magic != now.magic || h.ver != now.ver || h.gs != now.gs || h.a_start != now.a_start ||
        h.a_len != now.a_len || h.b_start != now.b_start || h.b_len != now.b_len)
        return 4;
    stopAllSounds();
    heapA(0);   /* блоки кучи текущей игры */
    heapV(0);
    heapB(0);
    uint16_t off = PAGE_ADDR + sizeof h;
    far_rd(gstate_start, SV_PAGE, off, h.gs);
    off += h.gs;
    far_cp(PG_BANKA, h.a_start, SV_PAGE, off, h.a_len);
    off += h.a_len;
    far_cp(PG_BANKB, h.b_start, SV_PAGE, off, h.b_len);
    off += h.b_len;
    sv_page = SV_PAGE;
    sv_off = off;
    heapA(2);
    heapV(2);
    heapB(2);
    /* loadState: комната, экран */
    vm_flush();
    if (currentRoom)
    {
        room_page = res_get(rtRoom, roomResource);
        room_addr = res_addr;
        res_lock_room(roomResource);
    }
    actors_redraw_all();
    camera.last_x = camera.cur_x;
    gfx_room_changed();
    fullRedraw = 1;
    screen_trashed = 1;
    return 0;
}

/* клавиша СБР: строка меню в области текста; 1 — игра загружена. Ошибка —
 * «ERROR код ответ_драйвера» до нажатия клавиши */
uint8_t saveload_menu(void)
{
    static const uint8_t prompt[] = "1-3: SAVE   4-6: LOAD";
    charset_clear();
    charset_drawString(0, 0, 15, prompt);
    uint16_t k = wait_key();
    uint8_t e = 0, loaded = 0;
    if (k >= '1' && k <= '3')
        e = do_save((uint8_t)(k - '0'));
    else if (k >= '4' && k <= '6')
    {
        e = do_load((uint8_t)(k - '3'));
        loaded = !e;
    }
    charset_clear();
    if (e)
    {
        uint8_t msg[] = "ERROR 0 000";
        msg[6] = (uint8_t)('0' + e);
        msg[8] = (uint8_t)('0' + ((ld_resp >> 6) & 7));
        msg[9] = (uint8_t)('0' + ((ld_resp >> 3) & 7));
        msg[10] = (uint8_t)('0' + (ld_resp & 7));
        charset_drawString(0, 0, 4, msg);
        wait_key();
        charset_clear();
    }
    return loaded;
}
