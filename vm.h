/* Внутреннее состояние VM (банк A): слоты скриптов, указатель скрипта, катсцены.
 * script.cpp: ScriptSlot / NestedScript / vm.* */
#pragma once
#include "scumm.h"

/* 16 байт: шаг массива — степень двойки (TOOLCHAIN-BUGS.md) */
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

/* кэш байт-кода в базовом ОЗУ (state.c): far_rd пишет только туда */
#define VM_CACHE 64
extern uint8_t vm_cache[VM_CACHE];
