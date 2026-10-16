#ifdef BACKEND_3DS

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <queue>

#include <3ds.h>

#include "platform/common/menu.h"
#include "platform/ui.h"

void uiInit() {
    consoleDebugInit(debugDevice_SVC);
}

void uiCleanup() {
}

void uiUpdateScreen() {
}

void uiGetSize(int* width, int* height) {
    if(width != nullptr) {
        *width = 40;
    }

    if(height != nullptr) {
        *height = 30;
    }
}

void uiClear() {
}

static TextColor textColor;
static bool highlighted;
static std::queue<UIKey> keyQueue;

void uiUpdateAttr() {
}

void uiSetLineHighlighted(bool highlight) {
    highlighted = highlight;
}

void uiSetTextColor(TextColor color) {
    textColor = color;
}

void uiPrint(const char* str, ...) {
    va_list list;
    va_start(list, str);
    uiPrintv(str, list);
    va_end(list);
}

void uiPrintv(const char* str, va_list list) {
    (void)str;
    (void)list;
}

void uiFlush() {
    // No-op en 3DS: el refresco de pantalla y presentación residen exclusivamente en Citro3D (C3D_FrameBegin / C3D_FrameEnd)
}

void uiSync() {
    gspWaitForVBlank();
}

void uiPushInput(UIKey key) {
    keyQueue.push(key);
}

UIKey uiReadKey() {
    if(!keyQueue.empty()) {
        UIKey key = keyQueue.front();
        keyQueue.pop();
        return key;
    }

    return UI_KEY_NONE;
}

#endif
