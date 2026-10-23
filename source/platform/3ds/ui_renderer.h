#pragma once

#include <3ds.h>
#include <citro3d.h>

#define UI_COLOR_RGBA(r, g, b, a) ((u32)((r) & 0xFF) | (((u32)((g) & 0xFF)) << 8) | (((u32)((b) & 0xFF)) << 16) | (((u32)((a) & 0xFF)) << 24))

#define UI_COLOR_YELLOW   UI_COLOR_RGBA(255, 220, 48, 255)
#define UI_COLOR_AMBER    UI_COLOR_RGBA(212, 168, 67, 255)
#define UI_COLOR_GOLD     UI_COLOR_RGBA(212, 168, 67, 255)
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

enum BottomRightMode {
    BR_MODE_PREVIEW = 0,
    BR_MODE_DISPLAY = 1,
    BR_MODE_PALETTE = 2
};

void uiSetBottomRightMode(BottomRightMode mode);
BottomRightMode uiGetBottomRightMode();
void uiCycleBottomRightMode();
bool uiHandleRightPanelTouch(int px, int py, char** result, FileChooser* chooser);

const C3D_Mtx* gfxGetProjectionTop();
const C3D_Mtx* gfxGetProjectionBottom();
int gfxGetULocProjection();

void uiRenderInit();
void uiRenderExit();
void uiDrawRect(float x, float y, float w, float h, u32 color);
void uiDrawRectOutline(float x, float y, float w, float h, float thickness, u32 color);
void uiDrawDropShadow(float x, float y, float w, float h, float radius, u8 maxAlpha);
void uiDrawRoundedRect(float x, float y, float w, float h, float r, u32 fillColor, u32 borderColor = 0, float borderWidth = 0.0f);
void uiDrawPillBadge(float x, float y, float w, float h, const char* text, u32 bgColor, u32 textColor);
void uiDrawModernButton(float x, float y, float w, float h, const char* label, u32 baseColor, u32 borderColor, bool isPressed = false);
void uiDrawTextScaled(float x, float y, float scale, const char* str, u32 color);
void uiDrawText(float x, float y, const char* str, u32 color);
float uiGetTextWidth(const char* str, float scale = 0.75f);
void uiDrawSysText(float x, float y, float scaleX, float scaleY, u32 color, const char* text);
float uiGetSysTextWidth(float scaleX, const char* text);
void uiDrawFolderIcon(float x, float y);
void uiDrawCartridgeIcon(float x, float y, bool isGBC);
void uiDrawWireframeTest();
void uiUpdateMenuState();
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
