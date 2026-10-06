/* Камера (банк B): перевод engines/scumm/camera.cpp для v2 (x в точках мира). */
#include "actor.h"

#define SCREEN_W 320
#define NUM_STRIPS 40
#define LEFT_TRIGGER 10    /* resetScumm: camera._leftTrigger / _rightTrigger */
#define RIGHT_TRIGGER 30

/* camera.cpp: setCameraAtEx */
void setCameraAtEx(int16_t at)
{
    camera.mode = kNormalCameraMode;
    camera.cur_x = at;
    setCameraAt(at);
    camera.movingToActor = 0;
}

/* camera.cpp: setCameraAt */
void setCameraAt(int16_t pos_x)
{
    int16_t d = pos_x - camera.cur_x;
    if (d < 0) d = -d;
    if (camera.mode != kFollowActorCameraMode || d > SCREEN_W / 2) camera.cur_x = pos_x;
    camera.dest_x = pos_x;
    if (camera.cur_x < VAR(VAR_CAMERA_MIN_X)) camera.cur_x = VAR(VAR_CAMERA_MIN_X);
    if (camera.cur_x > VAR(VAR_CAMERA_MAX_X)) camera.cur_x = VAR(VAR_CAMERA_MAX_X);
}

/* camera.cpp: setCameraFollows */
void setCameraFollows(uint8_t act, uint8_t setCamera)
{
    struct Actor *a = &actors[act];
    camera.mode = kFollowActorCameraMode;
    camera.follows = act;
    if (a->room != currentRoom)
    {
        startScene(a->room, 0, 0);
        camera.mode = kFollowActorCameraMode;
        camera.cur_x = (int16_t)(a->x * V12_X_MULTIPLIER);
        setCameraAt(camera.cur_x);
    }
    int16_t t = (int16_t)(a->x * V12_X_MULTIPLIER / 8 - screenStartStrip);
    if (t < LEFT_TRIGGER || t > RIGHT_TRIGGER || setCamera) setCameraAt((int16_t)(a->x * V12_X_MULTIPLIER));
    for (uint8_t i = 1; i < NUM_ACTORS; i++)
        if (actors[i].room == currentRoom) actors[i].needRedraw = 1;
    redrawV2Inventory();   /* runInventoryScript(0) */
}

/* camera.cpp: moveCamera */
void moveCamera(void)
{
    camera.cur_x &= (int16_t)0xFFF8;
    if (camera.cur_x < VAR(VAR_CAMERA_MIN_X))
    {
        camera.cur_x += 8;
        cameraMoved();
        return;
    }
    if (camera.cur_x > VAR(VAR_CAMERA_MAX_X))
    {
        camera.cur_x -= 8;
        cameraMoved();
        return;
    }
    struct Actor *a = 0;
    if (camera.mode == kFollowActorCameraMode)
    {
        a = &actors[camera.follows];
        int16_t t = (int16_t)(a->x * V12_X_MULTIPLIER / 8 - screenStartStrip);
        if (t < LEFT_TRIGGER || t > RIGHT_TRIGGER) camera.movingToActor = 1;
    }
    if (camera.movingToActor)
    {
        a = &actors[camera.follows];
        camera.dest_x = (int16_t)(a->x * V12_X_MULTIPLIER);
    }
    if (camera.dest_x < VAR(VAR_CAMERA_MIN_X)) camera.dest_x = VAR(VAR_CAMERA_MIN_X);
    if (camera.dest_x > VAR(VAR_CAMERA_MAX_X)) camera.dest_x = VAR(VAR_CAMERA_MAX_X);
    if (camera.cur_x < camera.dest_x) camera.cur_x += 8;
    if (camera.cur_x > camera.dest_x) camera.cur_x -= 8;
    if (camera.movingToActor && camera.cur_x / 8 == a->x * V12_X_MULTIPLIER / 8) camera.movingToActor = 0;
    cameraMoved();
}

/* camera.cpp: cameraMoved */
void cameraMoved(void)
{
    if (camera.cur_x < SCREEN_W / 2)
        camera.cur_x = SCREEN_W / 2;
    else if (camera.cur_x > (int16_t)roomWidth - SCREEN_W / 2)
        camera.cur_x = (int16_t)roomWidth - SCREEN_W / 2;
    screenStartStrip = camera.cur_x / 8 - NUM_STRIPS / 2;
    screenEndStrip = screenStartStrip + NUM_STRIPS - 1;
}

/* camera.cpp: panCameraTo */
void panCameraTo(int16_t x)
{
    camera.dest_x = x;
    camera.mode = kPanningCameraMode;
    camera.movingToActor = 0;
}

/* camera.cpp: actorFollowCamera */
void actorFollowCamera(uint8_t act)
{
    uint8_t old = camera.follows;
    setCameraFollows(act, 0);
    if (camera.follows != old) redrawV2Inventory();
    camera.movingToActor = 0;
}
