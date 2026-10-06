/* Главный цикл (банк A): ScummEngine::scummLoop, resetScumm, processInput /
 * ScummEngine_v2::processKeyboard (scumm.cpp, input.cpp) для v2. Ввод и вывод на
 * экран — платформенные функции (input_poll, gfx_*). */
#include "actor.h"
#include "gfx.h"

#ifndef HOST
#define TMARK(i) do { uint16_t t_ = *(volatile uint16_t *)0177710; dbg_acc[i] += (uint16_t)(t_last - t_); t_last = t_; } while (0)
#else
#define TMARK(i) do { } while (0)
#endif
extern uint16_t dbg_acc[8];
static uint16_t t_last;

/* platform: клавиша (ASCII, 0 — нет) и щелчки; ESC — пропуск катсцены */
#define KEY_ESC 27
extern uint16_t input_poll(int16_t *mx, int16_t *my, uint8_t *click);


static uint8_t completeScreenRedraw;

/* scumm.cpp: resetScumm (часть v2) + scumm.cpp: init */
void scumm_init(void)
{
    res_init();
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
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > 319) mouse_x = 319;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > 199) mouse_y = 199;
    virtualMouse_x = (int16_t)(mouse_x + screenStartStrip * 8);
    virtualMouse_y = (int16_t)(mouse_y - 16);
    if (virtualMouse_y < 0 || virtualMouse_y >= 128) virtualMouse_y = -1;
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
    talkDelay -= delta;
    if (talkDelay < 0) talkDelay = 0;
    int16_t oldEgo = VAR(VAR_EGO);
#ifndef HOST
    t_last = *(volatile uint16_t *)0177710;
#endif
    cursor_hide();
    CHARSET_1();
    processInput();
    /* scummLoop_updateScummVars */
    VAR(VAR_CAMERA_POS_X) = camera.cur_x >> 3;
    VAR(VAR_HAVE_MSG) = haveMsg;
    VAR(VAR_VIRT_MOUSE_X) = virtualMouse_x >> 3;
    VAR(VAR_VIRT_MOUSE_Y) = virtualMouse_y >> 1;
    VAR(VAR_MUSIC_TIMER) = (int16_t)sound_music_timer();
    if (completeScreenRedraw)
    {
        /* фон мира в PG_BITMAP цел — затёрт только экран: без fullRedraw (декодирования) */
        redrawVerbs();
        redrawV2Inventory();
        handleMouseOver(0);
        completeScreenRedraw = 0;
    }
    TMARK(0);
    runAllScripts();
    checkExecVerbs();
    checkAndRunSentenceScript();
    TMARK(1);
    if (currentRoom == 0)
        drawDirtyScreenParts();
    else
    {
        walkActors();
        moveCamera();
        updateObjectStates();
        TMARK(2);
        /* scummLoop_handleDrawing */
        if (camera.cur_x != camera.last_x || bgNeedsRedraw || fullRedraw) redrawBGAreas();
        processDrawQue();
        TMARK(3);
        /* scummLoop_handleActors (v5 и ниже) */
        processActors();
        TMARK(4);
        fullRedraw = 0;
        handleMouseOver(oldEgo != VAR(VAR_EGO));
        drawDirtyScreenParts();
        playActorSounds();
        TMARK(5);
    }
    if (screen_trashed)
    {
        /* чтение файла затёрло экран: перерисовать всё (как _completeScreenRedraw) */
        screen_trashed = 0;
        gfx_clear_all();
        completeScreenRedraw = 1;
        drawDirtyScreenParts();
    }
    cursor_show();
    TMARK(6);
    sound_process();
    camera.last_x = camera.cur_x;
}

/* scumm.cpp: ScummEngine::go — шаг цикла; ожидание delta тиков 60 Гц — у платформы */
int16_t scumm_delta(void)
{
    int16_t delta = VAR(VAR_TIMER_NEXT);
    if (delta < 1) delta = 1;
    return delta;
}
