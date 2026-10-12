/* Внутреннее состояние VM (банк A): слоты скриптов, указатель скрипта, катсцены.
 * script.cpp: ScriptSlot / NestedScript / vm.* */
#pragma once
#include "scumm.h"

/* 16 байт: индекс массива — сдвигом */
struct ScriptSlot
{
    uint16_t offs;
    int32_t delay;
    uint16_t number;
    uint8_t status, where, freezeResistant, recursive, freezeCount, cutsceneOverride;
    uint8_t pad[2];
};

struct NestedScript
{
    uint16_t number;
    uint8_t where, slot;
};

extern struct ScriptSlot slots[NUM_SCRIPT_SLOT];
extern uint16_t cutScenePtr[5];
extern uint8_t cutSceneScript[5];
extern int16_t cutSceneData[5];
extern uint8_t cutSceneStackPointer;

/* текущий скрипт: страница и адрес начала (_scriptOrgPointer), смещение (_scriptPointer) */
extern uint8_t sp_page;
extern uint16_t sp_base, sp_pc;
extern uint8_t opcode;
extern uint16_t resultVarNumber;

uint8_t fetchScriptByte(void);
uint16_t fetchScriptWord(void);
void updateScriptPtr(void);
void runScriptNested(uint8_t script);
/* строка из скрипта в кучу: страница, адрес — в res_addr */
uint8_t loadPtrToResource(void);

/* строки кэша байт-кода в базовом ОЗУ (platform.c): far_rd пишет только туда.
 * Восемь блоков по 32 байта (VM_CACHE — степень двойки, script.c): циклы
 * скриптов, крутящихся каждый шаг (страж-тентакл — два скрипта по 70 и 180 байт),
 * не перечитываются из страницы */
#define VM_LINES 8
#define VM_CACHE 32
extern uint8_t vm_lines[VM_LINES][VM_CACHE];
extern uint8_t vm_l_page[VM_LINES];               /* теги строк: страница, адрес, поколение */
extern uint16_t vm_l_addr[VM_LINES], vm_l_gen[VM_LINES];
