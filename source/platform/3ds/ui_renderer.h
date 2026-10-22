#pragma once

#include <3ds.h>
#include <citro3d.h>

#define UI_COLOR_RGBA(r, g, b, a) ((u32)((r) & 0xFF) | (((u32)((g) & 0xFF)) << 8) | (((u32)((b) & 0xFF)) << 16) | (((u32)((a) & 0xFF)) << 24))

#define UI_COLOR_YELLOW   UI_COLOR_RGBA(255, 220, 48, 255)
#define UI_COLOR_WHITE    UI_COLOR_RGBA(255, 255, 255, 255)
#define UI_COLOR_CYAN     UI_COLOR_RGBA(0, 255, 255, 255)
#define UI_COLOR_GRAY     UI_COLOR_RGBA(150, 160, 175, 255)
#define UI_COLOR_DARKGRAY UI_COLOR_RGBA(100, 110, 125, 255)
#define UI_COLOR_PURPLE   UI_COLOR_RGBA(210, 140, 255, 255)
#define UI_COLOR_GREEN    UI_COLOR_RGBA(80, 220, 100, 255)

static inline u32 uiColorRGBA(u8 r, u8 g, u8 b, u8 a = 255) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

class FileChooser;

enum UiModalTab {
    UI_TAB_GAME = 0,
    UI_TAB_VIDEO = 1,
    UI_TAB_SYSTEM = 2,
    UI_TAB_CHEATS = 3
};

const C3D_Mtx* gfxGetProjectionTop();
const C3D_Mtx* gfxGetProjectionBottom();
int gfxGetULocProjection();

void uiRenderInit();
void uiRenderExit();
void uiDrawRect(float x, float y, float w, float h, u32 color);
void uiDrawRectOutline(float x, float y, float w, float h, float thickness, u32 color);
void uiDrawText(float x, float y, const char* str, u32 color);
void uiDrawWireframeTest();
void uiDrawFileChooser(const FileChooser* chooser);
void uiDrawTopScreenHub(float eyeOffset = 0.0f);

void uiToggle3DArt();
void uiToggleSynopsis();
bool uiIs3DArtEnabled();
bool uiIsSynopsisEnabled();

int uiGetPreviewSlot();
void uiSetPreviewSlot(int slot);
void uiCyclePreviewSlot(int delta);

void uiOpenConfigModal(UiModalTab initialTab = UI_TAB_GAME);
bool uiIsConfigModalOpen();
bool uiUpdateConfigModal();
void uiDrawConfigModal();

void uiUpdateBoxart(const char* romPath, const char* romTitle, u8 mbcType, bool isGBC);
void uiDrawBoxart3D(float centerX, float centerY, float size, const C3D_Mtx* proj = nullptr, float eyeOffset = 0.0f);
void uiInvalidateBoxart();
