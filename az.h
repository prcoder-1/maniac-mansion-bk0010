/* AZБК: VGA 512x256 в 16 цветах, аппаратная прокрутка, блиттер (модуль azmod — azmod.ld).
 *
 * Модуль лежит в странице 6 ОЗУ БК-0011М (вторая экранная, на AZБК не нужна) и
 * вызывается через azc (farcall.s): страница — в окне 040000 на время вызова, СМК — как
 * была. Плату находит az_boot (azboot.c, банк D) при пуске; флаг az_on — в базовом ОЗУ. */
#pragma once
#include <stdint.h>

#define AZ_PAGE11 6                     /* страница ОЗУ 11М модуля */
#define AZ_MOD_ADDR 040000u
#define AZ_MOD_END 0100000u

extern volatile uint8_t az_on;          /* sys.ld: плата найдена, модуль загружен */
uint16_t azc(uint16_t fn, ...);         /* farcall.s: функция модуля по номеру (azhdr.s) */
extern const uint8_t font8[];           /* font8.s (банк C): шрифт 8x8 ScummVM */
void az_boot(void);                     /* azboot.c (банк D): проба, загрузка, режим */

/* номера функций модуля — порядок таблицы azhdr.s */
enum
{
    AZ_INIT,        /* () -> версия ПЛИС: видеорежим, палитра, слои */
    AZ_FLIP,        /* () пакет блиттера шага; прокрутка — в гашении */
    AZ_VIEW,        /* (vs, dirty, d0, d1, lt) -> 0/1/2 как gfx_view_flush */
    AZ_STRIPS,      /* (struct v2p *) -> адрес за картинкой: полосы фона -> склад */
    AZ_CHAR,        /* (x, y, glyph, color) знак 8x8 (8 байт, бит 7 — левая точка) в слой интерфейса */
    AZ_CLEAR,       /* (x0, y0, x1, y1) прямоугольник интерфейса — чёрным */
    AZ_RECOLOR,     /* (x0, y0, x1, y1, from, to) */
    AZ_CURSOR,      /* (x, y, color) курсор; color 0 — убрать */
    AZ_CLEARALL,    /* () все слои — чёрным */
    AZ_LABEL,       /* (name, font8) надпись LOADING под экраном игры; name 0 — стереть */
    AZ_LIGHTS,      /* (mode, x0, y0, x1, y1) свет и фонарик */
    AZ_ACTORS,      /* (const struct az_actx *, flushed) актёры (из банка B) */
    AZ_ACTRESET,    /* () актёры на экране забыты (экран заново) */
};

/* актёры (az_act.c): данные и функции банка B — модуль вызывается из него, банк B подключён */
struct Actor;
struct CostInfo;
struct az_actx
{
    struct Actor *actors;
    uint8_t *ract, *nract;
    uint8_t *strip_usage;                  /* [41][4]: gfxUsageBits по полосам от xstart / 8 */
    int16_t *xstart;
    void (*ract_build)(void);
    void (*animateCostume)(struct Actor *);
    void (*animate_only)(void);
    const struct CostInfo *(*cost_info)(uint8_t id);
};
