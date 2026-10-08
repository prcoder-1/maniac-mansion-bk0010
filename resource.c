/* Ресурсы: индекс 00.LFL и кэш в страницах СМК.
 * resource_v2.cpp: readIndexFile / readEnhancedIndexFile / readGlobalObjects,
 * resource.cpp: ensureResourceLoaded / loadResource / getResourceAddress.
 *
 * В ScummVM ресурс читается из файла комнаты по (комната, смещение) в кучу. Здесь
 * файл NN.LFL читается целиком (EMT 36 ANDOS иначе не умеет) в страницу кэша:
 * блок комнаты лежит с её начала. Все глобальные ресурсы этого файла (скрипты,
 * костюмы, звуки) сразу переносятся в кучу и больше не вытесняются — повторное
 * чтение файла стоит секунды. Страницы комнат вытесняются по давности (LRU). */
#include "scumm.h"
#include "con.h"

#define RES_NONE 0      /* ещё не загружен */
#define RES_BAD  0377   /* битая запись индекса: смещение или длина вне файла */

static uint8_t num_res[3];                     /* костюмы, скрипты, звуки */
static uint8_t room_of[3][NUM_SCRIPTS];        /* файл (комната) ресурса */
static uint16_t offs_of[3][NUM_SCRIPTS];       /* смещение в файле, 0xFFFF — нет */
static uint8_t cpage[3][NUM_SCRIPTS];          /* где лежит: страница / RES_* */
static uint16_t caddr[3][NUM_SCRIPTS];

static uint8_t file_page[NUM_ROOMS];           /* страница файла комнаты, 0 — нет */
static uint16_t file_len[NUM_ROOMS];

/* страницы кэша PG_CACHE..PG_LAST: 0 свободна, 0377 куча, иначе комната+1 */
#define PG_FREE 0
#define PG_HEAP 0377
static uint8_t pg_use[16];
static uint16_t pg_stamp[16];
static uint16_t stamp;
static uint8_t locked_room;
static uint8_t busy_page;   /* файл, который сейчас разбирается */

static uint8_t tix(uint8_t type)
{
    if (type == rtCostume) return 0;
    if (type == rtScript) return 1;
    return 2;
}

uint8_t res_num(uint8_t type)
{
    return num_res[tix(type)];
}

/* resource_v2.cpp: readEnhancedIndexFile + readResTypeList(v3old) + readGlobalObjects.
 * 00.LFL кладётся во временную страницу и разбирается словами через far_*. */
static uint16_t ip_page, ip;

static uint8_t ib(void)
{
    return far_rb(ip_page, ip++);
}

static uint16_t iw(void)
{
    uint16_t v = far_rw(ip_page, ip);
    ip += 2;
    return v;
}

static void read_list(uint8_t t, uint8_t n)
{
    if (n > NUM_SCRIPTS) fatal("INDEX", n);
    num_res[t] = n;
    for (uint8_t i = 0; i < n; ++i) room_of[t][i] = ib();
    for (uint8_t i = 0; i < n; ++i) offs_of[t][i] = iw();
}

static uint8_t take_page(void);
static void load_file(uint8_t room);

/* чтение файла: буфер загрузчика (02400..067777) перекрывает экран, видна лишь
 * его нижняя четверть (070000..077777, «малый экран») — там надпись; после
 * чтения экран сразу чистый (до перерисовки — без мусора буфера) */
static int16_t ld_screen(const struct ld_req *q)
{
#ifndef HOST
    volatile uint16_t *v = (volatile uint16_t *)070000;
    for (uint16_t n = 04000; n; --n) *v++ = 0;   /* 070000..077777 */
    con_puts(27, 12, "LOADING", 2);
    con_puts(29, 12, q->name, 2);
#endif
    int16_t len = ld_file(q);
#ifndef HOST
    gfx_clear_all();
    con_puts(13, 12, "LOADING", 2);   /* до перерисовки (декодирование комнаты — секунды) */
    con_puts(15, 12, q->name, 2);
#endif
    screen_trashed = 1;
    return len;
}

void res_init(void)
{
    ip_page = take_page();
    struct ld_req q = { "00.LFL", LD_XOR, ip_page, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR };
    if (ld_screen(&q) < 0) fatal("00.LFL", ld_resp);
    ip = PAGE_ADDR;
    if (iw() != 0x0100) fatal("NOT MM V2", 0);
    uint16_t n = iw();
    if (n > NUM_GLOBAL_OBJECTS) fatal("OBJECTS", n);
    numGlobalObjects = n;
    for (uint16_t i = 0; i < n; ++i)
    {
        uint8_t b = ib();
        objectOwnerTable[i] = b & 0x0F;   /* OF_OWNER_MASK */
        objectStateTable[i] = b >> 4;     /* OF_STATE_SHL */
    }
    uint8_t nr = ib();                    /* комнаты: номер = индекс, смещения не нужны */
    if (nr > NUM_ROOMS) fatal("ROOMS", nr);
    ip += (uint16_t)nr * 3;
    read_list(0, ib());
    read_list(1, ib());
    read_list(2, ib());
    pg_use[ip_page] = PG_FREE;
}

/* --- страницы кэша --- */

static uint8_t take_page(void)
{
    uint8_t best = 0;
    for (uint8_t p = PG_CACHE; p <= PG_LAST; ++p)
    {
        if (pg_use[p] == PG_FREE)
        {
            best = p;
            break;
        }
        if (pg_use[p] == PG_HEAP) continue;
        if (pg_use[p] == (uint8_t)(locked_room + 1) || p == busy_page) continue;
        if (!best || (uint16_t)(stamp - pg_stamp[p]) > (uint16_t)(stamp - pg_stamp[best])) best = p;
    }
    if (!best) fatal("NO PAGES", 0);
    if (pg_use[best] != PG_FREE)
    {
        uint8_t room = (uint8_t)(pg_use[best] - 1);   /* вытесняется файл комнаты */
        file_page[room] = 0;
    }
    pg_use[best] = 0376;   /* занята (уточнит вызывающий) */
    pg_stamp[best] = ++stamp;
    return best;
}

/* --- куча: блоки [слово: длина с заголовком | 1 — занят][данные] --- */

static void heap_page(uint8_t p)
{
    pg_use[p] = PG_HEAP;
    uint16_t h = PAGE_DATA_SIZE;
    far_wr(p, PAGE_ADDR, &h, 2);
}

uint16_t res_alloc(uint16_t size)
{
    uint16_t need = (uint16_t)((size + 3) & ~1u);
    for (uint8_t pass = 0; pass < 2; ++pass)
    {
        for (uint8_t p = PG_CACHE; p <= PG_LAST; ++p)
        {
            if (pg_use[p] != PG_HEAP) continue;
            uint16_t a = PAGE_ADDR, end = PAGE_ADDR + PAGE_DATA_SIZE;
            while (a < end)
            {
                uint16_t h = far_rw(p, a);
                uint16_t len = h & ~1u;
                if (!(h & 1))
                {
                    /* слить со следующими свободными */
                    while (a + len < end)
                    {
                        uint16_t h2 = far_rw(p, a + len);
                        if (h2 & 1) break;
                        len += h2;
                    }
                    if (len >= need)
                    {
                        uint16_t w;
                        if (len - need >= 8)
                        {
                            w = len - need;
                            far_wr(p, a + need, &w, 2);
                            len = need;
                        }
                        w = len | 1;
                        far_wr(p, a, &w, 2);
                        res_addr = a + 2;
                        return p;
                    }
                    if (len != (h & ~1u)) far_wr(p, a, &len, 2);
                }
                a += len;
            }
        }
        heap_page(take_page());
    }
    fatal("HEAP", size);
}

void res_free(uint16_t page, uint16_t addr)
{
    uint16_t h = far_rw(page, addr - 2) & ~1u;
    far_wr(page, addr - 2, &h, 2);
}

/* --- файлы комнат --- */

/* перенести ресурс файла (страница pg, хвост сверх 24 Кбайт — в PG_MASK:OVF_ADDR) в кучу */
static void file_copy(uint8_t dpage, uint16_t daddr, uint8_t pg, uint16_t off, uint16_t n)
{
    if (off < PAGE_DATA_SIZE)
    {
        uint16_t k = n;
        if (off + k > PAGE_DATA_SIZE) k = PAGE_DATA_SIZE - off;
        far_cp(dpage, daddr, pg, PAGE_ADDR + off, k);
        daddr += k;
        off += k;
        n -= k;
    }
    if (n) far_cp(dpage, daddr, PG_MASK, OVF_ADDR + (off - PAGE_DATA_SIZE), n);
}

static uint16_t file_word(uint8_t pg, uint16_t off)
{
    if ((uint16_t)(off + 1) < PAGE_DATA_SIZE) return far_rw(pg, PAGE_ADDR + off);
    uint16_t lo = off < PAGE_DATA_SIZE ? far_rb(pg, PAGE_ADDR + off)
                                       : far_rb(PG_MASK, OVF_ADDR + (off - PAGE_DATA_SIZE));
    ++off;
    uint16_t hi = off < PAGE_DATA_SIZE ? far_rb(pg, PAGE_ADDR + off)
                                       : far_rb(PG_MASK, OVF_ADDR + (off - PAGE_DATA_SIZE));
    return (uint16_t)(lo | (hi << 8));
}

static void load_file(uint8_t room)
{
    char name[7] = "00.LFL";
    name[0] = (char)('0' + room / 10);
    name[1] = (char)('0' + room % 10);
    uint8_t pg = take_page();
    struct ld_req q = { name, LD_XOR, pg, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR };
    int16_t len = ld_screen(&q);
    vm_flush();
    if (len < 0) fatal(name, ld_resp);
    pg_use[pg] = (uint8_t)(room + 1);
    file_page[room] = pg;
    busy_page = pg;
    file_len[room] = (uint16_t)len;
    /* глобальные ресурсы файла — в кучу (loadResource: длина — первое слово) */
    for (uint8_t t = 0; t < 3; ++t)
        for (uint8_t i = 0; i < num_res[t]; ++i)
        {
            if (room_of[t][i] != room || cpage[t][i] != RES_NONE) continue;
            uint16_t off = offs_of[t][i];
            uint16_t size = off < (uint16_t)len - 1 ? file_word(pg, off) : 0;
            if (off == 0xFFFF || size < 4 || (uint16_t)(off + size) > (uint16_t)len || off + size < off)
            {
                cpage[t][i] = RES_BAD;
                continue;
            }
            uint8_t hp = (uint8_t)res_alloc(size);
            uint16_t a = res_addr;
            file_copy(hp, a, pg, off, size);
            cpage[t][i] = hp;
            caddr[t][i] = a;
        }
    busy_page = 0;
}

/* getResourceAddress: страница ресурса (адрес — в *addr) или 0, если ресурса нет */
uint8_t res_get(uint8_t type, uint16_t idx)
{
    if (type == rtRoom)
    {
        if (idx == 0 || idx >= NUM_ROOMS) return 0;
        if (!file_page[idx]) load_file((uint8_t)idx);
        pg_stamp[file_page[idx]] = ++stamp;
        res_addr = PAGE_ADDR;
        return file_page[idx];
    }
    uint8_t t = tix(type);
    if (idx == 0 || idx >= num_res[t]) return 0;   /* ensureResourceLoaded: idx 0 — нет */
    if (cpage[t][idx] == RES_NONE)
    {
        uint8_t room = room_of[t][idx];
        if (room == 0 || room >= NUM_ROOMS || offs_of[t][idx] == 0xFFFF)
            cpage[t][idx] = RES_BAD;
        else
            load_file(room);   /* файл мог быть в кэше, но ресурс вытеснен не бывает */
    }
    if (cpage[t][idx] == RES_BAD) return 0;
    res_addr = caddr[t][idx];
    return cpage[t][idx];
}

/* текущая комната не вытесняется */
void res_lock_room(uint8_t room)
{
    locked_room = room;
}
