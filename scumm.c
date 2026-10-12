/* Главный цикл (банк A): ScummEngine::scummLoop, resetScumm, processInput /
 * ScummEngine_v2::processKeyboard (scumm.cpp, input.cpp) для v2. Ввод и вывод на
 * экран — платформенные функции (input_poll, gfx_*). */
#include "actor.h"
#include "gfx.h"
#include "con.h"

/* отладка (make TIMING=1): время частей шага в dbg_acc (отсчёты таймера) */
#if defined(TIMING) && !defined(HOST)
#define TMARK(i) do { uint16_t t_ = *(volatile uint16_t *)0177710; dbg_acc[i] += (uint16_t)(t_last - t_); t_last = t_; } while (0)
extern uint16_t dbg_acc[8];
static uint16_t t_last;
#else
#define TMARK(i) do { } while (0)
#endif

/* platform: клавиша (ASCII, 0 — нет) и щелчки; ESC — пропуск катсцены */
#define KEY_ESC 27
#define KEY_MENU 014   /* СБР — сохранение и загрузка */
extern uint16_t input_poll(int16_t *mx, int16_t *my, uint8_t *click);


static uint8_t completeScreenRedraw;
static uint8_t loaded;   /* игра только что загружена: остаток шага — только перерисовка */

/* scumm.cpp: resetScumm (часть v2) + scumm.cpp: init */
void scumm_init(void)
{
    res_init();
    cost_info_reset();
    gfx_init_pal();
    actors_init();
    vm_reset();
    verbs_reset();
    camera.mode = 0;
    camera.follows = 0;
    mouse_x = 104;
    mouse_y = 56;
    ENCD_offs = 0;
    for (uint8_t i = 0; i < 128; i++) resourceMapper[i] = 0;
}

/* input.cpp: processInput + ScummEngine_v2::processKeyboard */
static void processInput(void)
{
    uint8_t click = 0;
    uint16_t key = input_poll(&mouse_x, &mouse_y, &click);
    /* джойстик (joy.c, банк B): дальний вызов — только когда порт не пуст сейчас или
     * был на прошлом шаге (отпускание кнопки) */
    static uint16_t joy_last;
#ifndef HOST
    uint16_t jv = *(volatile uint16_t *)0177714;
#else
    uint16_t jv = 0;
#endif
    if (jv | joy_last) click |= joy_poll(&mouse_x, &mouse_y);
    joy_last = jv;
    if (key == KEY_MENU)
    {
        key = 0;
        loaded = saveload_menu();
    }
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > 319) mouse_x = 319;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > 199) mouse_y = 199;
    virtualMouse_x = (int16_t)(mouse_x + screenStartStrip * 8);
    virtualMouse_y = (int16_t)(mouse_y - 16);
    if (virtualMouse_y < 0 || virtualMouse_y >= 136) virtualMouse_y = -1;
    mouseAndKeyboardStat = 0;
    if (click & 2)
        key = KEY_ESC;   /* правая кнопка в v0-v3 — как ESC */
    else if (click & 1)
        mouseAndKeyboardStat = 0x8000;   /* MBS_LEFT_CLICK */
    if (key == KEY_ESC)
    {
        if (VAR(VAR_CUTSCENEEXIT_KEY) != 0)
        {
            abortCutscene();
            mouseAndKeyboardStat = (uint16_t)VAR(VAR_CUTSCENEEXIT_KEY);
        }
    }
    else if (key)
        mouseAndKeyboardStat = key;
    if (mouseAndKeyboardStat && mouseAndKeyboardStat < 0x200)
    {
        if (315 <= mouseAndKeyboardStat && mouseAndKeyboardStat <= 323)
            VAR(VAR_KEYPRESS) = (int16_t)(mouseAndKeyboardStat - 314);
        else
            VAR(VAR_KEYPRESS) = (int16_t)mouseAndKeyboardStat;
    }
}

/* scumm.cpp: ScummEngine::scummLoop (v2) */
void scummLoop(int16_t delta)
{
    if (delta > 15) delta = 15;
    decreaseScriptDelay(delta);
    SND_POLL();
    talkDelay -= delta;
    if (talkDelay < 0) talkDelay = 0;
    int16_t oldEgo = VAR(VAR_EGO);
#if defined(TIMING) && !defined(HOST)
    t_last = *(volatile uint16_t *)0177710;
#endif
    CHARSET_1();   /* курсор прячут сами функции вывода (gfx.c) */
    SND_POLL();
    processInput();
    SND_POLL();
    if (loaded)
    {
        loaded = 0;
        goto redraw;
    }
    /* scummLoop_updateScummVars */
    VAR(VAR_CAMERA_POS_X) = camera.cur_x >> 3;
    VAR(VAR_HAVE_MSG) = haveMsg;
    VAR(VAR_VIRT_MOUSE_X) = virtualMouse_x >> 3;
    VAR(VAR_VIRT_MOUSE_Y) = virtualMouse_y >> 1;
    VAR(VAR_MUSIC_TIMER) = 0;   /* Player_V1::getMusicTimer: в v1 таймера музыки нет */
    SND_POLL();
    if (completeScreenRedraw)
    {
        /* фон мира в PG_BITMAP цел — затёрт только экран: без fullRedraw (декодирования) */
        verbs_forget();
        string_forget();
        redrawVerbs();
        redrawV2Inventory();
        handleMouseOver(0);
        drawSentence();   /* строка предложения тоже затёрта (o2_drawSentence — по смене) */
        completeScreenRedraw = 0;
    }
    TMARK(0);
    runAllScripts();
    checkExecVerbs();
    checkAndRunSentenceScript();
    TMARK(1);
    if (currentRoom == 0)
    {
        if (draw_frame) drawDirtyScreenParts();
    }
    else
    {
        walkActors();
        moveCamera();
        updateObjectStates();
        SND_POLL();
        TMARK(2);
        if (draw_frame)
        {
            /* scummLoop_handleDrawing */
            /* темно или было темно: режим света и фонарик у курсора (gfx_lights) */
            if (!(VAR(VAR_CURRENT_LIGHTS) & 2) || lt_mode) gfx_lights();
            if (camera.cur_x != camera.last_x || bgNeedsRedraw || fullRedraw) redrawBGAreas();
            processDrawQue();
            TMARK(3);
            /* scummLoop_handleActors (v5 и ниже) */
            processActors();
            TMARK(4);
            fullRedraw = 0;
        }
        else
            animate_only();   /* шаг без отрисовки: кадры костюмов идут, рисунок — в следующем */
        handleMouseOver(oldEgo != VAR(VAR_EGO));
        if (draw_frame) drawDirtyScreenParts();
        gfx_prefetch();   /* группа фона рядом с видимой частью (широкая комната) */
        playActorSounds();
        TMARK(5);
    }
redraw:
    if (draw_frame && screen_trashed)
    {
        /* чтение файла (resource.c ld_screen): 1 — только надпись LOADING в строке
         * 29; 2 — затёрт весь экран: комната, актёры, глаголы, инвентарь и строка
         * предложения — заново (как _completeScreenRedraw) */
        if (screen_trashed == 2)
        {
            gfx_clear_all();
            actors_redraw_all();
            drawDirtyScreenParts();
            completeScreenRedraw = 1;
        }
        else
            con_puts(29, 9, "              ", 2);
        screen_trashed = 0;
    }
    cursor_show();
    TMARK(6);
    sound_process();
    if (draw_frame) camera.last_x = camera.cur_x;   /* сдвиг камеры перерисует ближайший кадр */
}

/* scumm.cpp: ScummEngine::go — шаг цикла; ожидание delta тиков 60 Гц — у платформы */
int16_t scumm_delta(void)
{
    int16_t delta = VAR(VAR_TIMER_NEXT);
    if (delta < 1) delta = 1;
    /* WORKAROUND ScummVM (ScummEngine::go): пока идёт скрипт 137 (кирпич открыл дверь
     * темницы), шаг — 6 джиффи, как медленный цикл оригинала: иначе дверь не успевает
     * закрыться и один герой убегает */
    if (isScriptRunning(137)) delta = 6;
    return delta;
}
