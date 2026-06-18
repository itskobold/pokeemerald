#ifndef GUARD_FIELD_CAMERA_H
#define GUARD_FIELD_CAMERA_H

struct CameraObject
{
    void (*callback)(struct CameraObject *);
    u32 spriteId;
    s32 movementSpeedX;
    s32 movementSpeedY;
    s32 x;
    s32 y;
};

extern struct CameraObject gFieldCamera;
// The camera's focus tile (map-relative), advanced by CameraMove. Usually equals the
// player's tile, but diverges when the camera is detached from the player. Seeded from the
// player's position on map entry (see SetPlayerCoordsFromWarp).
extern struct Coords16 gCameraPos;
extern u16 gTotalCameraPixelOffsetX;
extern u16 gTotalCameraPixelOffsetY;
extern u8 gCameraElevation;
extern u8 gCameraBiome;

void DrawWholeMapView(void);
void CurrentMapDrawMetatileAt(int x, int y);
void GetCameraOffsetWithPan(s16 *x, s16 *y);
void DrawDoorMetatileAt(int x, int y, u16 *tiles);
void ResetFieldCamera(void);
void ResetCameraUpdateInfo(void);
u32 InitCameraUpdateCallback(u8 trackedSpriteId);
void CameraUpdate(void);
void UpdateCameraElevation(void);
void UpdateCameraBiome(void);
bool8 IsFreecamActive(void);
void SetFreecamActive(bool8 active);
bool8 IsCameraDetachedFromPlayer(void);
void RecenterCameraOnPlayer(void);
void StopCameraObjectTracking(void);
void SetCameraTrackedLocalId(u8 localId);
u8 GetCameraTrackedLocalId(void);
void SetCameraPanningCallback(void (*callback)(void));
void SetCameraPanning(s16 horizontal, s16 vertical);
void InstallCameraPanAheadCallback(void);
void UpdateCameraPanning(void);
void FieldUpdateBgTilemapScroll(void);

#endif //GUARD_FIELD_CAMERA_H
