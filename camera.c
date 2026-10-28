/* Камера (банк A): перевод engines/scumm/camera.cpp для v2 (x в точках мира). Актёры —
 * в банке B: их x и комната — через actor_get. */
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
    camera.mode = kFollowActorCameraMode;
    camera.follows = act;
    uint8_t room = (uint8_t)actor_get(act, AF_ROOM);
    int16_t ax = (int16_t)(actor_get(act, AF_X) * V12_X_MULTIPLIER);
    if (room != currentRoom)
    {
        startScene(room, 0, 0);   /* скрипт входа мог переставить актёра */
        ax = (int16_t)(actor_get(act, AF_X) * V12_X_MULTIPLIER);
        camera.mode = kFollowActorCameraMode;
        camera.cur_x = ax;
        setCameraAt(camera.cur_x);
    }
    int16_t t = (int16_t)(ax / 8 - screenStartStrip);
    if (t < LEFT_TRIGGER || t > RIGHT_TRIGGER || setCamera) setCameraAt(ax);
    room_actors_redraw();
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
    int16_t ax = 0;
    if (camera.mode == kFollowActorCameraMode || camera.movingToActor)
        ax = (int16_t)(actor_get(camera.follows, AF_X) * V12_X_MULTIPLIER);
    if (camera.mode == kFollowActorCameraMode)
    {
        int16_t t = (int16_t)(ax / 8 - screenStartStrip);
        if (t < LEFT_TRIGGER || t > RIGHT_TRIGGER) camera.movingToActor = 1;
    }
    if (camera.movingToActor) camera.dest_x = ax;
    if (camera.dest_x < VAR(VAR_CAMERA_MIN_X)) camera.dest_x = VAR(VAR_CAMERA_MIN_X);
    if (camera.dest_x > VAR(VAR_CAMERA_MAX_X)) camera.dest_x = VAR(VAR_CAMERA_MAX_X);
    if (camera.cur_x < camera.dest_x) camera.cur_x += 8;
    if (camera.cur_x > camera.dest_x) camera.cur_x -= 8;
    if (camera.movingToActor && camera.cur_x / 8 == ax / 8) camera.movingToActor = 0;
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
