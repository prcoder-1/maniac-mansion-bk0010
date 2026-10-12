/* Ресурсы: индекс 00.LFL и кэш в страницах СМК.
 * resource_v2.cpp: readIndexFile / readEnhancedIndexFile / readGlobalObjects,
 * resource.cpp: ensureResourceLoaded / loadResource / getResourceAddress.
 *
 * В ScummVM ресурс читается из файла комнаты по (комната, смещение) в кучу. Здесь
 * файл NN.LFL читается целиком в страницу кэша: блок комнаты лежит с её начала.
 * Глобальный ресурс (скрипт, костюм, звук) переносится из файла в кучу при первом
 * обращении. Куча — над кодом банка D и не больше HEAP_MAX страниц кэша; когда
 * она полна, ненужные сейчас ресурсы освобождаются (expireResources: не заперты
 * скриптом, не выполняются, не у актёров комнаты, не звучат) и при обращении
 * переносятся снова. Страницы комнат вытесняются по давности (LRU). */
#include "scumm.h"
#include "con.h"

#define RES_NONE 0      /* ещё не загружен */
#define RES_BAD  0377   /* битая запись индекса: смещение или длина вне файла */

/* resource_v2.cpp: readClassicIndexFile (MM v1 DOS): число ресурсов задано
 * (readMAXS), не в файле */
#define V1_OBJECTS 800
#define V1_ROOMS 55
#define V1_COSTUMES 35
#define V1_SCRIPTS 200
#define V1_SOUNDS 100
/* таблицы ресурсов — одна на все типы: костюмы, скрипты, звуки подряд (res_base) */
#define NRES (V1_COSTUMES + V1_SCRIPTS + V1_SOUNDS)
static const uint16_t res_base[3] = { 0, V1_COSTUMES, V1_COSTUMES + V1_SCRIPTS };
static uint8_t num_res[3];                     /* костюмы, скрипты, звуки */
static uint8_t room_of[NRES];                  /* файл (комната) ресурса */
static uint16_t offs_of[NRES];                 /* смещение в файле, 0xFFFF — нет */
static uint8_t cpage[NRES];                    /* где лежит: страница / RES_* */
static uint16_t caddr[NRES];

static uint8_t file_page[NUM_ROOMS];           /* страница файла комнаты, 0 — нет */
static uint16_t file_len[NUM_ROOMS];

/* страницы кэша PG_CACHE..PG_LAST: 0 свободна, 0377 куча, иначе комната+1 */
#define PG_FREE 0
#define PG_HEAP 0377
static uint8_t pg_use[16];
static uint16_t pg_stamp[16];
static uint16_t stamp;
static uint8_t locked_room;
static uint8_t busy_page;   /* файл, из которого сейчас копируется ресурс */
#define HEAP_MAX 3          /* страниц кэша под кучей: остальные три — файлам комнат */
static uint8_t heap_pages;
static uint8_t res_locked[(NRES + 7) / 8];   /* запер скрипт (o2_resourceRoutines) */

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

/* 00.LFL кладётся во временную страницу и разбирается через far_* */
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
    num_res[t] = n;
    uint16_t b = res_base[t];
    for (uint8_t i = 0; i < n; ++i) room_of[b + i] = ib();
    for (uint8_t i = 0; i < n; ++i) offs_of[b + i] = iw();
}

static uint8_t take_page(void);
static void load_file(uint8_t room);
static void heap_page(uint8_t p);

/* чтение .LFL: все короче 034600 байт — буфер загрузчика (03200..037777) до экрана
 * не доходит, и загрузчик его не трогает (LD_LOW). Надпись — всегда в строке 29,
 * ниже игровых 200 строк: ни глаголов, ни комнаты не задевает и стирается в конце
 * шага (screen_trashed 1). Длиннее (не бывает) — экран в мусоре: всё заново (2) */
static int16_t ld_screen(const struct ld_req *q)
{
#ifndef HOST
    con_puts(29, 9, "LOADING", 1);
    con_puts(29, 17, q->name, 1);
#endif
    int16_t len = ld_file(q);
    if ((uint16_t)len <= 040000u - 03200u)
    {
        if (screen_trashed < 1) screen_trashed = 1;
        return len;
    }
#ifndef HOST
    gfx_clear_all();
    con_puts(13, 12, "LOADING", 1);   /* до перерисовки (декодирование комнаты — секунды) */
    con_puts(15, 12, q->name, 1);
#endif
    screen_trashed = 2;
    return len;
}

/* блок, длина, первое слово NN.LFL на диске — пишет tools/ftab.py в образ (0 —
 * через ДОС) */
uint16_t lfl_ft[NUM_ROOMS][3] = { { 0, 0, 1 } };   /* не нули: в .data (в файле) */

void res_init(void)
{
    ip_page = take_page();
    struct ld_req q = { "00.LFL", LD_XOR | LD_LOW, ip_page, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR,
                        lfl_ft[0][0], lfl_ft[0][1], lfl_ft[0][2] };
    if (ld_screen(&q) < 0) fatal("00.LFL", ld_resp);
    ip = PAGE_ADDR;
    if (iw() != 0x0A31) fatal("NOT MM V1", 0);
    numGlobalObjects = V1_OBJECTS;
    for (uint16_t i = 0; i < V1_OBJECTS; ++i)
    {
        uint8_t b = ib();
        objectOwnerTable[i] = b & 0x0F;   /* OF_OWNER_MASK */
        objectStateTable[i] = b >> 4;     /* OF_STATE_SHL */
    }
    ip += V1_ROOMS * 3;                   /* комнаты: номер = индекс, смещения не нужны */
    read_list(0, V1_COSTUMES);
    read_list(1, V1_SCRIPTS);
    read_list(2, V1_SOUNDS);
    pg_use[ip_page] = PG_FREE;
    heap_page(PG_BANKD);
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

/* в странице банка D куча — над его кодом и данными (bank.ld) */
#ifdef HOST
#define HEAP_D (PAGE_ADDR + 030000u)
#else
extern uint8_t bdata_end[];
#define HEAP_D ((uint16_t)bdata_end)
#endif
static uint16_t heap_start(uint8_t p)
{
    return p == PG_BANKD ? HEAP_D : PAGE_ADDR;
}

static void heap_page(uint8_t p)
{
    pg_use[p] = PG_HEAP;
    uint16_t a = heap_start(p), h = PAGE_ADDR + PAGE_DATA_SIZE - a;
    far_wr(p, a, &h, 2);
}

static void res_expire(void);

uint16_t res_alloc(uint16_t size)
{
    uint16_t need = (uint16_t)((size + 3) & ~1u);
    for (uint8_t pass = 0; pass < 3; ++pass)
    {
        for (uint8_t p = PG_CACHE; p <= PG_BANKD; ++p)
        {
            if (pg_use[p] != PG_HEAP) continue;
            uint16_t a = heap_start(p), end = PAGE_ADDR + PAGE_DATA_SIZE;
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
                        /* блок заполнит вызывающий (скрипт, OBCD предмета): строки кэша
                         * байт-кода могли захватить это место, пока оно было свободным */
                        vm_flush();
                        return p;
                    }
                    if (len != (h & ~1u)) far_wr(p, a, &len, 2);
                }
                a += len;
            }
        }
        if (pass == 0 && heap_pages >= HEAP_MAX)
            res_expire();
        else
        {
            heap_page(take_page());   /* после освобождения не хватило — ещё страница */
            heap_pages++;
        }
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
    struct ld_req q = { name, LD_XOR | LD_LOW, pg, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR,
                        lfl_ft[room][0], lfl_ft[room][1], lfl_ft[room][2] };
    int16_t len = ld_screen(&q);
    vm_flush();
    if (len < 0) fatal(name, ld_resp);
    /* хвост сверх страницы (PG_MASK:OVF_ADDR) жив лишь до следующего чтения, а
     * ресурсы переносятся позже — таких файлов у MM нет (самый длинный — 14.6 Кбайт) */
    if ((uint16_t)len > PAGE_DATA_SIZE) fatal(name, (uint16_t)len);
    pg_use[pg] = (uint8_t)(room + 1);
    file_page[room] = pg;
    file_len[room] = (uint16_t)len;
}

/* ресурс j (индекс таблиц как одномерных) из файла комнаты в кэше — в кучу
 * (loadResource: длина — первое слово) */
static void res_copy(uint16_t j, uint8_t room)
{
    uint8_t pg = file_page[room];
    uint16_t len = file_len[room], off = offs_of[j];
    uint16_t size = off < len - 1 ? file_word(pg, off) : 0;
    if (size < 4 || (uint32_t)off + size > len)
    {
        cpage[j] = RES_BAD;
        return;
    }
    busy_page = pg;   /* новая страница кучи не вытеснит этот файл */
    uint8_t hp = (uint8_t)res_alloc(size);
    uint16_t a = res_addr;
    file_copy(hp, a, pg, off, size);
    busy_page = 0;
    cpage[j] = hp;
    caddr[j] = a;
}

/* resource.cpp: expireResources — куча полна: освободить всё, что сейчас не нужно */
static void res_expire(void)
{
    uint8_t cost = 0;
    for (uint8_t t = 0; t < 3; ++t)
        for (uint8_t i = 1; i < num_res[t]; ++i)
        {
            uint16_t j = res_base[t] + i;
            uint8_t p = cpage[j];
            if (p == RES_NONE || p == RES_BAD || (res_locked[j >> 3] & (1u << (j & 7)))) continue;
            if (t == 0 ? costume_in_use(i) : t == 1 ? isScriptInUse(i) : isSoundRunning(i)) continue;
            res_free(p, caddr[j]);
            cpage[j] = RES_NONE;
            if (t == 0) cost = 1;
            if (!(i & 7)) SND_POLL();
        }
    vm_flush();                  /* строки байт-кода могли быть из освобождённых скриптов */
    if (cost) cost_info_reset();   /* заголовки костюмов, кэш спрайтов — по адресам в куче */
}

/* o2_resourceRoutines: скрипт запирает ресурс (не освобождать) или отпирает */
void res_lock(uint8_t type, uint16_t idx, uint8_t on)
{
    if (type == rtRoom) return;
    uint8_t t = tix(type);
    if (idx >= num_res[t]) return;
    uint16_t j = res_base[t] + idx;
    uint8_t *b = &res_locked[j >> 3], m = (uint8_t)(1u << (j & 7));
    if (on) *b |= m;
    else *b &= (uint8_t)~m;
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
    uint16_t j = res_base[t] + idx;
    uint8_t *cp = &cpage[j];
    if (*cp == RES_NONE)
    {
        uint8_t room = room_of[j];
        if (room == 0 || room >= NUM_ROOMS || offs_of[j] == 0xFFFF)
            *cp = RES_BAD;
        else
        {
            if (!file_page[room]) load_file(room);
            pg_stamp[file_page[room]] = ++stamp;
            res_copy(j, room);
        }
    }
    if (*cp == RES_BAD) return 0;
    res_addr = caddr[j];
    return *cp;
}

/* текущая комната не вытесняется */
void res_lock_room(uint8_t room)
{
    locked_room = room;
}

/* o2_restart (ScummEngine::restart): всё заново, как после запуска с диска —
 * исходные данные ядра (состояние игры, кэши) из файла MANIAC (в странице кэша
 * спрайтов: её всё равно сбросит перезапуск), затем вход _start: main снова
 * читает REPL и банки и запускает загрузочный скрипт. Раскладка запуска (её ждёт
 * smk_probe) прячет банк и общую копию кода — последние команды в _bounce */
void restart(void)
{
    extern uint8_t gstate_start[], __bss_start[];
    extern void start(void);
    extern uint8_t bounce[];   /* sys.ld */
    *(volatile uint8_t *)023776 = 0;   /* _snd_active: snd_tick — сразу выход */
    *(volatile uint16_t *)0177716 = 0;
    struct ld_req q = { "MANIAC", LD_LOW, PG_SPR, PAGE_ADDR, PAGE_DATA_SIZE, PG_MASK, OVF_ADDR };
    if (ld_file(&q) < 0) fatal("MANIAC", ld_resp);
    /* файл автостарта — с 0760 (8 слов адреса запуска перед 01000) */
    far_rd(gstate_start, PG_SPR, PAGE_ADDR + (uint16_t)gstate_start - 0760u, (uint16_t)(__bss_start - gstate_start));
    volatile uint16_t *t = (volatile uint16_t *)bounce;
    t[0] = 012737; t[1] = 6; t[2] = 0177130;          /* mov #6, @#177130 */
    t[3] = 012737; t[4] = smk_home; t[5] = 0177130;   /* mov #код, @#177130 */
    t[6] = 012737; t[7] = 0; t[8] = 0177130;          /* mov #0, @#177130 */
    t[9] = 000137; t[10] = (uint16_t)start;           /* jmp @#_start */
    ((void (*)(void))bounce)();
    __builtin_unreachable();
}
