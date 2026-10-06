/* Сгенерировано tools/gen_opcodes.py из ScummVM script_v2.cpp (setupOpcodes) — не править */
#pragma once
#include <stdint.h>

enum
{
    OP_o5_stopObjectCode,
    OP_o2_putActor,
    OP_o5_startMusic,
    OP_o5_getActorRoom,
    OP_o2_isGreaterEqual,
    OP_o2_drawObject,
    OP_o2_getActorElevation,
    OP_o2_setState08,
    OP_o5_isNotEqual,
    OP_o5_faceActor,
    OP_o2_assignVarWordIndirect,
    OP_o2_setObjPreposition,
    OP_o2_resourceRoutines,
    OP_o5_walkActorToActor,
    OP_o2_putActorAtObject,
    OP_o2_ifNotState08,
    OP_o5_getObjectOwner,
    OP_o5_animateActor,
    OP_o2_panCameraTo,
    OP_o2_actorOps,
    OP_o5_print,
    OP_o2_actorFromPos,
    OP_o5_getRandomNr,
    OP_o2_clearState02,
    OP_o5_jumpRelative,
    OP_o2_doSentence,
    OP_o5_move,
    OP_o2_setBitVar,
    OP_o5_startSound,
    OP_o2_ifClassOfIs,
    OP_o2_walkActorTo,
    OP_o2_ifState02,
    OP_o5_stopMusic,
    OP_o4_saveLoadGame,
    OP_o2_getActorY,
    OP_o2_loadRoomWithEgo,
    OP_o5_setVarRange,
    OP_o2_setState04,
    OP_o5_equalZero,
    OP_o2_setOwnerOf,
    OP_o2_addIndirect,
    OP_o5_delayVariable,
    OP_o2_assignVarByte,
    OP_o2_putActorInRoom,
    OP_o2_delay,
    OP_o2_ifNotState04,
    OP_o3_setBoxFlags,
    OP_o2_getBitVar,
    OP_o2_setCameraAt,
    OP_o2_roomOps,
    OP_o5_getDist,
    OP_o2_findObject,
    OP_o2_walkActorToObject,
    OP_o2_setState01,
    OP_o2_isLessEqual,
    OP_o2_subtract,
    OP_o2_waitForActor,
    OP_o5_stopSound,
    OP_o2_setActorElevation,
    OP_o2_ifNotState01,
    OP_o2_cutscene,
    OP_o2_startScript,
    OP_o2_getActorX,
    OP_o2_isLess,
    OP_o5_increment,
    OP_o2_clearState08,
    OP_o5_isEqual,
    OP_o2_chainScript,
    OP_o2_waitForSentence,
    OP_o2_ifState08,
    OP_o2_pickupObject,
    OP_o5_actorFollowCamera,
    OP_o5_setObjectName,
    OP_o5_getActorMoving,
    OP_o2_setState02,
    OP_o2_beginOverride,
    OP_o2_add,
    OP_o2_dummy,
    OP_o2_ifNotState02,
    OP_o2_cursorCommand,
    OP_o2_stopScript,
    OP_o5_getActorFacing,
    OP_o5_getClosestObjActor,
    OP_o2_clearState04,
    OP_o5_isScriptRunning,
    OP_o2_subIndirect,
    OP_o2_getObjPreposition,
    OP_o2_ifState04,
    OP_o2_lights,
    OP_o5_getActorCostume,
    OP_o5_loadRoom,
    OP_o2_clearState01,
    OP_o2_isGreater,
    OP_o2_verbOps,
    OP_o2_getActorWalkBox,
    OP_o5_isSoundRunning,
    OP_o2_ifState01,
    OP_o5_breakHere,
    OP_o2_restart,
    OP_o5_notEqualZero,
    OP_o2_switchCostumeSet,
    OP_o2_drawSentence,
    OP_o2_waitForMessage,
    OP_o2_endCutscene,
    OP_o5_decrement,
    OP_o5_pseudoRoom,
    OP_o5_printEgo,
};

static const uint8_t opcode_handler[256] =
{
    OP_o5_stopObjectCode, OP_o2_putActor, OP_o5_startMusic, OP_o5_getActorRoom,  /* 00 */
    OP_o2_isGreaterEqual, OP_o2_drawObject, OP_o2_getActorElevation, OP_o2_setState08,  /* 04 */
    OP_o5_isNotEqual, OP_o5_faceActor, OP_o2_assignVarWordIndirect, OP_o2_setObjPreposition,  /* 08 */
    OP_o2_resourceRoutines, OP_o5_walkActorToActor, OP_o2_putActorAtObject, OP_o2_ifNotState08,  /* 0C */
    OP_o5_getObjectOwner, OP_o5_animateActor, OP_o2_panCameraTo, OP_o2_actorOps,  /* 10 */
    OP_o5_print, OP_o2_actorFromPos, OP_o5_getRandomNr, OP_o2_clearState02,  /* 14 */
    OP_o5_jumpRelative, OP_o2_doSentence, OP_o5_move, OP_o2_setBitVar,  /* 18 */
    OP_o5_startSound, OP_o2_ifClassOfIs, OP_o2_walkActorTo, OP_o2_ifState02,  /* 1C */
    OP_o5_stopMusic, OP_o2_putActor, OP_o4_saveLoadGame, OP_o2_getActorY,  /* 20 */
    OP_o2_loadRoomWithEgo, OP_o2_drawObject, OP_o5_setVarRange, OP_o2_setState04,  /* 24 */
    OP_o5_equalZero, OP_o2_setOwnerOf, OP_o2_addIndirect, OP_o5_delayVariable,  /* 28 */
    OP_o2_assignVarByte, OP_o2_putActorInRoom, OP_o2_delay, OP_o2_ifNotState04,  /* 2C */
    OP_o3_setBoxFlags, OP_o2_getBitVar, OP_o2_setCameraAt, OP_o2_roomOps,  /* 30 */
    OP_o5_getDist, OP_o2_findObject, OP_o2_walkActorToObject, OP_o2_setState01,  /* 34 */
    OP_o2_isLessEqual, OP_o2_doSentence, OP_o2_subtract, OP_o2_waitForActor,  /* 38 */
    OP_o5_stopSound, OP_o2_setActorElevation, OP_o2_walkActorTo, OP_o2_ifNotState01,  /* 3C */
    OP_o2_cutscene, OP_o2_putActor, OP_o2_startScript, OP_o2_getActorX,  /* 40 */
    OP_o2_isLess, OP_o2_drawObject, OP_o5_increment, OP_o2_clearState08,  /* 44 */
    OP_o5_isEqual, OP_o5_faceActor, OP_o2_chainScript, OP_o2_setObjPreposition,  /* 48 */
    OP_o2_waitForSentence, OP_o5_walkActorToActor, OP_o2_putActorAtObject, OP_o2_ifState08,  /* 4C */
    OP_o2_pickupObject, OP_o5_animateActor, OP_o5_actorFollowCamera, OP_o2_actorOps,  /* 50 */
    OP_o5_setObjectName, OP_o2_actorFromPos, OP_o5_getActorMoving, OP_o2_setState02,  /* 54 */
    OP_o2_beginOverride, OP_o2_doSentence, OP_o2_add, OP_o2_setBitVar,  /* 58 */
    OP_o2_dummy, OP_o2_ifClassOfIs, OP_o2_walkActorTo, OP_o2_ifNotState02,  /* 5C */
    OP_o2_cursorCommand, OP_o2_putActor, OP_o2_stopScript, OP_o5_getActorFacing,  /* 60 */
    OP_o2_loadRoomWithEgo, OP_o2_drawObject, OP_o5_getClosestObjActor, OP_o2_clearState04,  /* 64 */
    OP_o5_isScriptRunning, OP_o2_setOwnerOf, OP_o2_subIndirect, OP_o2_dummy,  /* 68 */
    OP_o2_getObjPreposition, OP_o2_putActorInRoom, OP_o2_dummy, OP_o2_ifState04,  /* 6C */
    OP_o2_lights, OP_o5_getActorCostume, OP_o5_loadRoom, OP_o2_roomOps,  /* 70 */
    OP_o5_getDist, OP_o2_findObject, OP_o2_walkActorToObject, OP_o2_clearState01,  /* 74 */
    OP_o2_isGreater, OP_o2_doSentence, OP_o2_verbOps, OP_o2_getActorWalkBox,  /* 78 */
    OP_o5_isSoundRunning, OP_o2_setActorElevation, OP_o2_walkActorTo, OP_o2_ifState01,  /* 7C */
    OP_o5_breakHere, OP_o2_putActor, OP_o5_startMusic, OP_o5_getActorRoom,  /* 80 */
    OP_o2_isGreaterEqual, OP_o2_drawObject, OP_o2_getActorElevation, OP_o2_setState08,  /* 84 */
    OP_o5_isNotEqual, OP_o5_faceActor, OP_o2_assignVarWordIndirect, OP_o2_setObjPreposition,  /* 88 */
    OP_o2_resourceRoutines, OP_o5_walkActorToActor, OP_o2_putActorAtObject, OP_o2_ifNotState08,  /* 8C */
    OP_o5_getObjectOwner, OP_o5_animateActor, OP_o2_panCameraTo, OP_o2_actorOps,  /* 90 */
    OP_o5_print, OP_o2_actorFromPos, OP_o5_getRandomNr, OP_o2_clearState02,  /* 94 */
    OP_o2_restart, OP_o2_doSentence, OP_o5_move, OP_o2_setBitVar,  /* 98 */
    OP_o5_startSound, OP_o2_ifClassOfIs, OP_o2_walkActorTo, OP_o2_ifState02,  /* 9C */
    OP_o5_stopObjectCode, OP_o2_putActor, OP_o4_saveLoadGame, OP_o2_getActorY,  /* A0 */
    OP_o2_loadRoomWithEgo, OP_o2_drawObject, OP_o5_setVarRange, OP_o2_setState04,  /* A4 */
    OP_o5_notEqualZero, OP_o2_setOwnerOf, OP_o2_addIndirect, OP_o2_switchCostumeSet,  /* A8 */
    OP_o2_drawSentence, OP_o2_putActorInRoom, OP_o2_waitForMessage, OP_o2_ifNotState04,  /* AC */
    OP_o3_setBoxFlags, OP_o2_getBitVar, OP_o2_setCameraAt, OP_o2_roomOps,  /* B0 */
    OP_o5_getDist, OP_o2_findObject, OP_o2_walkActorToObject, OP_o2_setState01,  /* B4 */
    OP_o2_isLessEqual, OP_o2_doSentence, OP_o2_subtract, OP_o2_waitForActor,  /* B8 */
    OP_o5_stopSound, OP_o2_setActorElevation, OP_o2_walkActorTo, OP_o2_ifNotState01,  /* BC */
    OP_o2_endCutscene, OP_o2_putActor, OP_o2_startScript, OP_o2_getActorX,  /* C0 */
    OP_o2_isLess, OP_o2_drawObject, OP_o5_decrement, OP_o2_clearState08,  /* C4 */
    OP_o5_isEqual, OP_o5_faceActor, OP_o2_chainScript, OP_o2_setObjPreposition,  /* C8 */
    OP_o5_pseudoRoom, OP_o5_walkActorToActor, OP_o2_putActorAtObject, OP_o2_ifState08,  /* CC */
    OP_o2_pickupObject, OP_o5_animateActor, OP_o5_actorFollowCamera, OP_o2_actorOps,  /* D0 */
    OP_o5_setObjectName, OP_o2_actorFromPos, OP_o5_getActorMoving, OP_o2_setState02,  /* D4 */
    OP_o5_printEgo, OP_o2_doSentence, OP_o2_add, OP_o2_setBitVar,  /* D8 */
    OP_o2_dummy, OP_o2_ifClassOfIs, OP_o2_walkActorTo, OP_o2_ifNotState02,  /* DC */
    OP_o2_cursorCommand, OP_o2_putActor, OP_o2_stopScript, OP_o5_getActorFacing,  /* E0 */
    OP_o2_loadRoomWithEgo, OP_o2_drawObject, OP_o5_getClosestObjActor, OP_o2_clearState04,  /* E4 */
    OP_o5_isScriptRunning, OP_o2_setOwnerOf, OP_o2_subIndirect, OP_o2_dummy,  /* E8 */
    OP_o2_getObjPreposition, OP_o2_putActorInRoom, OP_o2_dummy, OP_o2_ifState04,  /* EC */
    OP_o2_lights, OP_o5_getActorCostume, OP_o5_loadRoom, OP_o2_roomOps,  /* F0 */
    OP_o5_getDist, OP_o2_findObject, OP_o2_walkActorToObject, OP_o2_clearState01,  /* F4 */
    OP_o2_isGreater, OP_o2_doSentence, OP_o2_verbOps, OP_o2_getActorWalkBox,  /* F8 */
    OP_o5_isSoundRunning, OP_o2_setActorElevation, OP_o2_walkActorTo, OP_o2_ifState01,  /* FC */
};

#ifdef HOST
static const char *const opcode_name[] =
{
    "o5_stopObjectCode",
    "o2_putActor",
    "o5_startMusic",
    "o5_getActorRoom",
    "o2_isGreaterEqual",
    "o2_drawObject",
    "o2_getActorElevation",
    "o2_setState08",
    "o5_isNotEqual",
    "o5_faceActor",
    "o2_assignVarWordIndirect",
    "o2_setObjPreposition",
    "o2_resourceRoutines",
    "o5_walkActorToActor",
    "o2_putActorAtObject",
    "o2_ifNotState08",
    "o5_getObjectOwner",
    "o5_animateActor",
    "o2_panCameraTo",
    "o2_actorOps",
    "o5_print",
    "o2_actorFromPos",
    "o5_getRandomNr",
    "o2_clearState02",
    "o5_jumpRelative",
    "o2_doSentence",
    "o5_move",
    "o2_setBitVar",
    "o5_startSound",
    "o2_ifClassOfIs",
    "o2_walkActorTo",
    "o2_ifState02",
    "o5_stopMusic",
    "o4_saveLoadGame",
    "o2_getActorY",
    "o2_loadRoomWithEgo",
    "o5_setVarRange",
    "o2_setState04",
    "o5_equalZero",
    "o2_setOwnerOf",
    "o2_addIndirect",
    "o5_delayVariable",
    "o2_assignVarByte",
    "o2_putActorInRoom",
    "o2_delay",
    "o2_ifNotState04",
    "o3_setBoxFlags",
    "o2_getBitVar",
    "o2_setCameraAt",
    "o2_roomOps",
    "o5_getDist",
    "o2_findObject",
    "o2_walkActorToObject",
    "o2_setState01",
    "o2_isLessEqual",
    "o2_subtract",
    "o2_waitForActor",
    "o5_stopSound",
    "o2_setActorElevation",
    "o2_ifNotState01",
    "o2_cutscene",
    "o2_startScript",
    "o2_getActorX",
    "o2_isLess",
    "o5_increment",
    "o2_clearState08",
    "o5_isEqual",
    "o2_chainScript",
    "o2_waitForSentence",
    "o2_ifState08",
    "o2_pickupObject",
    "o5_actorFollowCamera",
    "o5_setObjectName",
    "o5_getActorMoving",
    "o2_setState02",
    "o2_beginOverride",
    "o2_add",
    "o2_dummy",
    "o2_ifNotState02",
    "o2_cursorCommand",
    "o2_stopScript",
    "o5_getActorFacing",
    "o5_getClosestObjActor",
    "o2_clearState04",
    "o5_isScriptRunning",
    "o2_subIndirect",
    "o2_getObjPreposition",
    "o2_ifState04",
    "o2_lights",
    "o5_getActorCostume",
    "o5_loadRoom",
    "o2_clearState01",
    "o2_isGreater",
    "o2_verbOps",
    "o2_getActorWalkBox",
    "o5_isSoundRunning",
    "o2_ifState01",
    "o5_breakHere",
    "o2_restart",
    "o5_notEqualZero",
    "o2_switchCostumeSet",
    "o2_drawSentence",
    "o2_waitForMessage",
    "o2_endCutscene",
    "o5_decrement",
    "o5_pseudoRoom",
    "o5_printEgo",
};
#endif

