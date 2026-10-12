/* Реплики и текст (банк A): ScummEngine_v2::decodeParseString (script_v2.cpp),
 * actorTalk / stopTalk (actor.cpp), CHARSET_1 / handleNextCharsetCode / newLine /
 * convertMessageToString (string.cpp) для v2. Отрисовка знаков — charset_*. */
#include "vm.h"

#define SCREEN_W 320
#define FONT_H 8          /* CharsetRendererV2: 8x8 */
#define FONT_W 8
#define DEFAULT_TALK_DELAY 3

uint8_t string0_color;    /* _string[0].color */
static uint8_t actorToPrintStrFor;
static uint8_t charsetBuffer[256];
/* реплика на экране, выведенная целиком (CHARSET_1): скрипт, повторяющий её каждый
 * шаг (тентакл у лестницы: «You can't pass…», пока герой рядом), не перерисовывает
 * её — ScummVM рисует кадр разом и стирание с выводом не видно, а на БК это мигание
 * и ~15 мс на шаг */
static uint8_t shown_msg[256], shown_actor, shown_pos, shown_ok;
static int16_t shown_delay, shown_count;
/* откуда в байт-коде (decodeParseString) взята показанная реплика: тот же print —
 * без разбора строки, только сдвиг sp_pc (у тентакла ~4% на шаг). Годится, пока
 * не сменилось поколение кэша байт-кода (ресурс мог переехать) и в реплике нет
 * подстановок переменных */
static uint8_t shown_page, pend_page;
static uint16_t shown_at, shown_len, pend_at, pend_len, shown_gen, pend_gen;
static uint8_t charsetBufPos;
static uint8_t charsetColor;
static uint8_t keepText, useTalkAnims, hasMask;
static int16_t msgCount;
static int16_t nextLeft, nextTop;


/* --- string.cpp: convertMessageToString и подстановки 4..7 --- */

static uint8_t *dst_p, *dst_end;

static void put(uint8_t c)
{
    if (dst_p >= dst_end) fatal("MSG OVERFLOW", 0);
    *dst_p++ = c;
}

static void convertFar(uint8_t page, uint16_t addr);

/* ScummEngine_v2::readVar: переменные 14..16 — косвенные */
static int16_t read_var(uint16_t var)
{
    return scummVars[var >= 14 && var <= 16 ? (uint16_t)scummVars[var] : var];
}

static void convertIntMessage(uint16_t var)
{
    int16_t num = read_var(var);
    char tmp[7];
    uint8_t n = 0;
    uint16_t u = (uint16_t)(num < 0 ? -num : num);
    do tmp[n++] = (char)('0' + u % 10); while (u /= 10);
    if (num < 0) put('-');
    while (n) put((uint8_t)tmp[--n]);
}

static void convertVerbMessage(uint16_t var)
{
    int16_t num = read_var(var);
    if (!num) return;
    uint8_t slot = getVerbSlot((uint8_t)num, 0);
    uint16_t a;
    uint8_t pg = slot ? verb_name(slot, &a) : 0;
    if (pg) convertFar(pg, a);
}

static void convertNameMessage(uint16_t var)
{
    int16_t num = read_var(var);
    if (!num) return;
    uint16_t a;
    uint8_t pg = getObjOrActorName((uint16_t)num, &a);
    if (pg) convertFar(pg, a);
}

static void convertStringMessage(uint16_t var)
{
    uint8_t chr;
    while ((chr = (uint8_t)scummVars[var++]))
        if (chr != '@') put(chr);
}

/* одна строка в формате сообщения: из буфера src (len байт) */
static void convertMessage(const uint8_t *src)
{
    uint16_t num = 0;
    for (;;)
    {
        uint8_t chr = src[num++];
        if (chr == 0) break;
        if (chr == 0xFF)
        {
            chr = src[num++];
            if (chr == 1 || chr == 2 || chr == 3 || chr == 8)
            {
                put(0xFF);
                put(chr);
            }
            else
            {
                uint16_t val = (uint16_t)(src[num] | (src[num + 1] << 8));
                switch (chr)
                {
                case 4: convertIntMessage(val); break;
                case 5: convertVerbMessage(val); break;
                case 6: convertNameMessage(val); break;
                case 7: convertStringMessage(val); break;
                default:
                    put(0xFF);
                    put(chr);
                    put(src[num]);
                    put(src[num + 1]);
                }
                num += 2;
            }
        }
        else if (chr != '@')
            put(chr);
    }
}

/* имя из ресурса (страница) — через буфер на стеке */
static void convertFar(uint8_t page, uint16_t addr)
{
    uint8_t buf[64];
    uint8_t n = 0;
    do buf[n] = far_rb(page, addr + n); while (buf[n] && ++n < sizeof buf - 1);
    buf[sizeof buf - 1] = 0;
    convertMessage(buf);
}

/* --- реплика --- */

/* экран затёрт целиком (загрузка игры, чтение большого файла): реплики на нём нет —
 * та же снова выводится заново (кэш показанного восстановлен из сохранения) */
void string_forget(void)
{
    shown_ok = 0;
    hasMask = 0;
}

/* actor.cpp: stopTalk */
void stopTalk(void)
{
    shown_ok = 0;
    haveMsg = 0;
    talkDelay = 0;
    uint8_t act = talkingActor;
    if (act && act < 0x80)
    {
        if ((uint8_t)actor_get(act, AF_ROOM) == currentRoom && useTalkAnims)
        {
            runActorTalkScript(act, ACTOR_TALKSTOP);
            useTalkAnims = 0;
        }
        talkingActor = 0xFF;
    }
    keepText = 0;
    /* restoreCharsetBg */
    nextLeft = 0;
    nextTop = 0;
    if (hasMask)
    {
        hasMask = 0;
        charset_clear();
    }
}

/* string.cpp: newLine */
static void newLine(void)
{
    nextLeft = 0;
    nextTop += FONT_H;
}

/* string.cpp: handleNextCharsetCode */
static uint8_t handleNextCharsetCode(uint8_t *code)
{
    uint8_t c = 0;
    uint8_t *buffer = charsetBuffer + charsetBufPos;
    for (;;)
    {
        c = *buffer++;
        if (c != 0xFF) break;
        c = *buffer++;
        uint8_t endLoop = 0;
        switch (c)
        {
        case 1:
            c = 13;
            msgCount = SCREEN_W;
            endLoop = 1;
            break;
        case 2:
            haveMsg = 0;
            keepText = 1;
            endLoop = 1;
            break;
        case 3:
            haveMsg = 0xFF;
            keepText = 0;
            msgCount = 0;
            endLoop = 1;
            break;
        case 8:
            break;
        case 9:
        case 10:
        case 12:
        case 13:
        case 14:
            buffer += 2;   /* в v2 не встречаются */
            break;
        default:
            fatal("CHARSET CODE", c);
        }
        if (endLoop) break;
    }
    charsetBufPos = (uint8_t)(buffer - charsetBuffer);
    *code = c;
    return c != 2 && c != 3;
}

/* string.cpp: CHARSET_1 (v2: надписи без привязки к актёру, шрифт 8x8) */
void CHARSET_1(void)
{
    if (!haveMsg) return;
    uint8_t a = talkingActor;
    if (a == 0xFF) a = 0;
    if (talkDelay) return;
    if (haveMsg == 1)
    {
        stopTalk();   /* (_sound->_sfxMode & 2) == 0: речи нет */
        return;
    }
    if (a)
    {
        runActorTalkScript(a, ACTOR_TALKSTART);
        useTalkAnims = 1;
    }
    talkDelay = 60;   /* VAR_DEFAULT_TALK_DELAY нет */
    uint8_t whole = !keepText && charsetBufPos == 0;
    shown_ok = 0;
    if (!keepText)
    {
        nextLeft = 0;   /* restoreCharsetBg */
        nextTop = 0;
        if (hasMask)
        {
            hasMask = 0;
            charset_clear();
        }
        msgCount = 0;
    }
    else
        talkDelay += (int16_t)(msgCount * DEFAULT_TALK_DELAY);
    uint8_t c = 0;
    while (handleNextCharsetCode(&c))
    {
        if (c == 0)
        {
            haveMsg = 1;
            keepText = 0;
            msgCount = 0;
            if (whole)
            {
                uint8_t i = 0;
                while ((shown_msg[i] = charsetBuffer[i]) != 0 && ++i) ;
                shown_actor = talkingActor;
                shown_pos = charsetBufPos;
                shown_delay = talkDelay;
                shown_count = VAR(VAR_CHARCOUNT);
                shown_page = pend_page;
                shown_at = pend_at;
                shown_len = pend_len;
                shown_gen = pend_gen;
                shown_ok = 1;
            }
            break;
        }
        if (c == 13)
        {
            newLine();
            continue;
        }
        if (nextLeft >= SCREEN_W) newLine();
        charset_printChar(nextLeft, nextTop, c, charsetColor);
        hasMask = 1;
        msgCount += 1;
        nextLeft += FONT_W;
        talkDelay += DEFAULT_TALK_DELAY;
        VAR(VAR_CHARCOUNT)++;
    }
}

/* actor.cpp: actorTalk */
static uint8_t talk_same(void)
{
    if (!(shown_ok && hasMask && haveMsg == 1 && actorToPrintStrFor == shown_actor)) return 0;
    charsetBufPos = shown_pos;
    talkDelay = shown_delay;
    VAR(VAR_HAVE_MSG) = 0xFF;
    VAR(VAR_CHARCOUNT) = shown_count;
    return 1;
}

static void actorTalk(const uint8_t *msg)
{
    dst_p = charsetBuffer;
    dst_end = charsetBuffer + sizeof charsetBuffer - 1;
    convertMessage(msg);
    *dst_p = 0;
    {
        uint8_t i = 0;
        while (charsetBuffer[i] == shown_msg[i] && charsetBuffer[i] && ++i) ;
        if (charsetBuffer[i] == shown_msg[i] && talk_same()) return;   /* та же реплика на экране: только срок */
    }
    if (actorToPrintStrFor == 0xFF)
    {
        if (!keepText) stopTalk();
        talkingActor = 0xFF;
    }
    else
    {
        uint8_t oldact;
        if ((uint8_t)actor_get(actorToPrintStrFor, AF_ROOM) != currentRoom)
            oldact = 0xFF;
        else
        {
            if (!keepText) stopTalk();
            talkingActor = actorToPrintStrFor;
            runActorTalkScript(actorToPrintStrFor, ACTOR_TALKSTART);
            useTalkAnims = 1;
            oldact = talkingActor;
        }
        if (oldact >= 0x80) return;
    }
    uint8_t ta = talkingActor;
    if (ta > 0x7F) charsetColor = string0_color;
    else charsetColor = (uint8_t)actor_get(ta, AF_TALKCOLOR);
    charsetBufPos = 0;
    talkDelay = 0;
    haveMsg = 0xFF;
    VAR(VAR_HAVE_MSG) = 0xFF;
    VAR(VAR_CHARCOUNT) = 0;
    CHARSET_1();
}

/* script_v2.cpp: ScummEngine_v2::decodeParseString */
void decodeParseString(uint8_t actor)
{
    uint8_t buffer[256];
    uint8_t *ptr = buffer, c, subst = 0;
    actorToPrintStrFor = actor;
    uint16_t at = sp_base + sp_pc, pc0 = sp_pc;
    if (sp_page == shown_page && at == shown_at && shown_gen == vm_c_gen && talk_same())
    {
        sp_pc += shown_len;
        return;
    }
    while ((c = fetchScriptByte()))
    {
        uint8_t insertSpace = (c & 0x80) != 0;
        c &= 0x7f;
        if (c < 8)
        {
            *ptr++ = 0xFF;
            *ptr++ = c;
            if (c > 3)
            {
                subst = 1;
                *ptr++ = fetchScriptByte();
                *ptr++ = 0;
            }
        }
        else
            *ptr++ = c;
        if (insertSpace) *ptr++ = ' ';
        if (ptr > buffer + sizeof buffer - 6) fatal("PRINT", 0);
    }
    *ptr = 0;
    pend_page = subst ? 0 : sp_page;
    pend_at = at;
    pend_len = sp_pc - pc0;
    pend_gen = vm_c_gen;
    actorTalk(buffer);
    pend_page = 0;
}
