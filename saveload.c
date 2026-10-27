/* Сохранение и загрузка игры (банк C). Как ScummEngine::saveState/loadState, но
 * снимком памяти, а не списком полей: всё состояние игры (state.c), данные
 * банков A и B (скрипты, глаголы, инвентарь, строки, актёры) и блоки кучи
 * (инвентарь, имена) — в файл MMSAVEn. Банки C и D (кэш ресурсов, экран, звук,
 * комната) не сохраняются: после загрузки звук остановлен, комната перечитана,
 * экран перерисован целиком. Образ собирается в PG_BITMAP — фон комнаты всё равно
 * декодируется заново. Заголовок сверяется со сборкой: размеры и места данных и
 * метка sv_build — CRC32 ядра и банков A, B (их данные и код, разбирающий их,
 * в образе), её пишет tools/ftab.py в BANKC на дискете.
 *
 * Файлы MMSAVE1, MMSAVE2 (по 20 Кбайт) создаёт сборка образа; читаются и пишутся они
 * по блокам драйвером ПЗУ КНГМД (romio.s), минуя ANDOS. Формат диска ANDOS —
 * FAT12: сектор 512 байт, кластер — 4 сектора, FAT — блоки 1..2, каталог —
 * 5..11, данные — с блока 12. */
#include "scumm.h"
#include "snddrv.h"

#define SV_MAGIC 0x564D   /* "MV" */
#define SV_VER 3
#define SV_PAGE PG_BITMAP

extern uint8_t gstate_start[], gstate_end[];
extern uint8_t boxes[BOXES_SIZE];   /* room.c / sys.ld */
extern uint8_t kbd_buf[16];
extern uint8_t band[1024], bounce[256];   /* sys.ld */
extern uint8_t romio_code[], romio_end[];
extern volatile uint16_t kbd_head, kbd_tail;

struct SvHead { uint16_t magic, ver, gs, a_start, a_len, b_start, b_len, build[2]; };

uint16_t sv_build[2] = { 1, 1 };   /* не нули: в .data (в файле) */

static void head_now(struct SvHead *h)
{
    h->magic = SV_MAGIC;
    h->ver = SV_VER;
    h->gs = (uint16_t)(gstate_end - gstate_start);
    h->a_start = save_dataA();
    h->a_len = res_addr;
    h->b_start = save_dataB();
    h->b_len = res_addr;
    h->build[0] = sv_build[0];
    h->build[1] = sv_build[1];
}

static uint16_t wait_key(void)
{
    while (kbd_head == kbd_tail) ;
    uint16_t k = kbd_buf[kbd_tail] & 0177;
    kbd_tail = (kbd_tail + 1) & 15;
    return k;
}

#define SV_CLUSTERS 10   /* MMSAVEn: 20 Кбайт (на дискете места на два) */

/* блок диска драйвером ПЗУ (romio.s из _bounce): words < 0 — запись; 0 — удачно */
static uint8_t blk_io(uint16_t blk, int16_t words)
{
    for (uint8_t *s = romio_code, *d = bounce; s < romio_end;) *d++ = *s++;
    ld_resp = ((uint16_t (*)(uint16_t, int16_t, void *))bounce)(blk, words, band);
    return ld_resp != 0;
}

/* кластеры файла MMSAVEn по каталогу и FAT; 0 — нет файла или ошибка диска */
static uint8_t sv_clusters(uint8_t slot, uint16_t clu[SV_CLUSTERS])
{
    static const char name[11] = "MMSAVE0    ";
    uint16_t c = 0;
    for (uint16_t b = 5; b < 12 && !c; b++)
    {
        if (blk_io(b, 256)) return 0;
        for (uint16_t e = 0; e < 512; e += 32)
        {
            uint8_t k = 0;
            while (k < 11 && band[e + k] == (k == 6 ? '0' + slot : (uint8_t)name[k])) k++;
            if (k == 11)
            {
                c = band[e + 26] | (uint16_t)band[e + 27] << 8;
                break;
            }
        }
    }
    if (!c || blk_io(1, 512)) return 0;   /* FAT: блоки 1..2 -> band */
    for (uint8_t i = 0; i < SV_CLUSTERS; i++)
    {
        if (c < 2 || c >= 0xFF8) return 0;   /* файл короче 24 Кбайт */
        clu[i] = c;
        uint16_t o = c + (c >> 1), v = band[o] | (uint16_t)band[o + 1] << 8;
        c = c & 1 ? v >> 4 : v & 0xFFF;
    }
    return 1;
}

/* n байт образа (с PAGE_ADDR) в файл (write) или из файла; 0 — удачно */
static uint8_t sv_file(uint8_t slot, uint8_t write, uint16_t n)
{
    uint16_t clu[SV_CLUSTERS];
    if (!sv_clusters(slot, clu)) return 1;
    uint16_t off = PAGE_ADDR;
    for (uint8_t i = 0; n && i < SV_CLUSTERS; i++)
        for (uint8_t h = 0; h < 2 && n; h++)   /* кластер — двумя кусками по 1 Кбайт через band */
        {
            uint16_t blk = 12 + ((clu[i] - 2) << 2) + (h << 1);
            if (write)
            {
                far_rd(band, SV_PAGE, off, 1024);
                if (blk_io(blk, -512)) return 1;
            }
            else
            {
                if (blk_io(blk, 512)) return 1;
                far_wr(SV_PAGE, off, band, 1024);
            }
            off += 1024;
            n = n > 1024 ? n - 1024 : 0;
        }
    return 0;
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
    far_wr(SV_PAGE, off, boxes, BOXES_SIZE);
    off += BOXES_SIZE;
    sv_page = SV_PAGE;
    sv_off = off;
    heapA(1);
    heapV(1);
    heapB(1);
    /* 1 — не поместилось в страницу или в файл */
    uint8_t e = !sv_off || sv_off - PAGE_ADDR > SV_CLUSTERS * 2048u ? 1 : sv_file(slot, 1, sv_off - PAGE_ADDR) ? 2 : 0;
    gfx_room_changed();   /* фон комнаты в PG_BITMAP затёрт образом */
    fullRedraw = 1;
    return e;
}

static uint8_t do_load(uint8_t slot)
{
    struct SvHead h, now;
    uint8_t e = sv_file(slot, 0, SV_CLUSTERS * 2048u);
    gfx_room_changed();   /* фон комнаты в PG_BITMAP затёрт файлом */
    fullRedraw = 1;
    if (e) return 3;
    far_rd(&h, SV_PAGE, PAGE_ADDR, sizeof h);
    head_now(&now);
    if (h.magic != now.magic || h.ver != now.ver || h.gs != now.gs || h.a_start != now.a_start ||
        h.a_len != now.a_len || h.b_start != now.b_start || h.b_len != now.b_len ||
        h.build[0] != now.build[0] || h.build[1] != now.build[1])
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
    far_rd(boxes, SV_PAGE, off, BOXES_SIZE);
    off += BOXES_SIZE;
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
    cost_info_reset();
    camera.last_x = camera.cur_x;
    gfx_room_changed();
    fullRedraw = 1;
    screen_trashed = 1;
    return 0;
}

/* надпись в строке меню на время записи или чтения файла */
static void sv_note(const char *what, uint8_t slot)
{
    uint8_t d[2] = { (uint8_t)('0' + slot), 0 };
    charset_clear();
    charset_drawString(charset_drawString(0, 0, 15, (const uint8_t *)what), 0, 15, d);
}

/* клавиша СБР: строка меню в области текста; 1 — игра загружена. Ошибка —
 * «ERROR код ответ_драйвера» до нажатия клавиши. 7 — звук PCjr / PC-спикер.
 * На время меню звук стоит (драйвер не зовётся, AY молчит). */
uint8_t saveload_menu(void)
{
    static const uint8_t prompt_pj[] = "1-2: SAVE  3-4: LOAD  7: SOUND PCJR";
    static const uint8_t prompt_spk[] = "1-2: SAVE  3-4: LOAD  7: SOUND SPEAKER";
    snd_on = 0;
    ay_mute();
    charset_clear();
    charset_drawString(0, 0, 15, snd_pcjr ? prompt_pj : prompt_spk);
    uint16_t k = wait_key();
    uint8_t e = 0, loaded = 0;
    if (k == '7')
        sound_mode((uint8_t)!snd_pcjr);
    else if (k >= '1' && k <= '2')
    {
        sv_note("Saving game ", (uint8_t)(k - '0'));
        e = do_save((uint8_t)(k - '0'));
    }
    else if (k >= '3' && k <= '4')
    {
        sv_note("Loading game ", (uint8_t)(k - '2'));
        e = do_load((uint8_t)(k - '2'));
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
    snd_on = 1;
    return loaded;
}
