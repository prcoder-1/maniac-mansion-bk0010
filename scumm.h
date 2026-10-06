/* Общие определения порта SCUMM v2 (подмножество engines/scumm ScummVM).
 *
 * Правило памяти: код банка не держит указателей в страницы СМК — данные ресурсов
 * читаются функциями far_* по паре (страница, адрес окна 0100000..0157777).
 * Указатели, передаваемые между банками, должны указывать в базовое ОЗУ (стек,
 * состояние ядра), но не в bss банка. */
#pragma once
#include <stdint.h>
#include "smk.h"

typedef uint8_t byte;

/* scumm.h: типы ресурсов */
enum
{
    rtRoom = 1, rtScript = 2, rtCostume = 3, rtSound = 4, rtInventory = 5,
    rtVerb = 8, rtActorName = 9, rtObjectName = 16,
};

/* resource_v4.cpp readMAXS / scumm.cpp: размеры таблиц MM v2 */
#define NUM_VARIABLES 800
#define NUM_GLOBAL_OBJECTS 800
#define NUM_ROOMS 61
#define NUM_COSTUMES 40
#define NUM_SCRIPTS 200
#define NUM_SOUNDS 120

/* far_*: smk.s (общая копия кода) */
uint8_t far_rb(uint16_t page, uint16_t addr);
uint16_t far_rw(uint16_t page, uint16_t addr);
void far_wb(uint16_t page, uint16_t addr, uint16_t v);
void far_rd(void *dst, uint16_t page, uint16_t addr, uint16_t n);
void far_wr(uint16_t page, uint16_t addr, const void *src, uint16_t n);
void far_cp(uint16_t dpage, uint16_t daddr, uint16_t spage, uint16_t saddr, uint16_t n);
void far_fill(uint16_t page, uint16_t addr, uint16_t v, uint16_t n);

/* ядро */
void fatal(const char *msg, uint16_t v) __attribute__((noreturn));
extern uint8_t screen_trashed;   /* буфер чтения файла затёр экран */

/* состояние (state.c, базовое ОЗУ) */
extern int16_t scummVars[NUM_VARIABLES];
extern uint8_t objectOwnerTable[NUM_GLOBAL_OBJECTS];
extern uint8_t objectStateTable[NUM_GLOBAL_OBJECTS];
extern uint16_t numGlobalObjects;

/* resource.c (банк B) */
void res_init(void);
uint8_t res_get(uint8_t type, uint16_t idx, uint16_t *addr);
uint16_t res_alloc(uint16_t size, uint16_t *addr);
void res_free(uint16_t page, uint16_t addr);
void res_lock_room(uint8_t room);
uint8_t res_num(uint8_t type);
