#ifdef BACKEND_3DS

#include <time.h>
#include <string.h>
#include <string>
#include <vector>
#include <cmath>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <strings.h>

#include <3ds.h>
#include <3ds/font.h>
#include <3ds/util/utf.h>
#include <citro3d.h>

#include "libs/stb_image/stb_image.h"
#include "platform/common/menu/filechooser.h"
#include "platform/common/manager.h"
#include "platform/common/rom_metadata.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/common/cheatengine.h"
#include "platform/3ds/scraper_worker.h"
#include "platform/3ds/ui_renderer.h"
#include "platform/input.h"
#include "gameboy.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static C3D_Tex s_boxartTex;
static bool s_boxartLoaded = false;
static bool s_boxartTexAllocated = false;
static std::string s_lastBoxartPath = "";
static bool s_boxartChecked = false;
static float s_boxartAngleY = 0.0f;
static bool s_isGBCBox = false;

static RomMetadata s_currentMeta;
static bool s_metaLoaded = false;
static std::string s_lastMetaRom = "";

// Standard 8x8 monochrome ASCII font (128 characters, 8 bytes per character)
static const u8 font8x8_basic[128][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x7e, 0x81, 0xa5, 0x81, 0xbd, 0x99, 0x81, 0x7e},
    {0x7e, 0xff, 0xdb, 0xff, 0xc3, 0xe7, 0xff, 0x7e},
    {0x6c, 0xfe, 0xfe, 0xfe, 0x7c, 0x38, 0x10, 0x00},
    {0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00},
    {0x38, 0x7c, 0x38, 0xfe, 0xfe, 0xd6, 0x10, 0x38},
    {0x10, 0x38, 0x7c, 0xfe, 0xfe, 0x7c, 0x10, 0x38},
    {0x00, 0x00, 0x18, 0x3c, 0x3c, 0x18, 0x00, 0x00},
    {0xff, 0xff, 0xe7, 0xc3, 0xc3, 0xe7, 0xff, 0xff},
    {0x00, 0x3c, 0x66, 0x42, 0x42, 0x66, 0x3c, 0x00},
    {0xff, 0xc3, 0x99, 0xbd, 0xbd, 0x99, 0xc3, 0xff},
    {0x0f, 0x07, 0x0f, 0x7d, 0xcc, 0xcc, 0xcc, 0x78},
    {0x3c, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x7e, 0x18},
    {0x3f, 0x33, 0x3f, 0x30, 0x30, 0x70, 0xf0, 0xe0},
    {0x7f, 0x63, 0x7f, 0x63, 0x63, 0x67, 0xe6, 0xc0},
    {0x18, 0xdb, 0x3c, 0xe7, 0xe7, 0x3c, 0xdb, 0x18},
    {0x80, 0xe0, 0xf8, 0xfe, 0xf8, 0xe0, 0x80, 0x00},
    {0x02, 0x0e, 0x3e, 0xfe, 0x3e, 0x0e, 0x02, 0x00},
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x7e, 0x3c, 0x18},
    {0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x66, 0x00},
    {0x7f, 0xdb, 0xdb, 0x7b, 0x1b, 0x1b, 0x1b, 0x00},
    {0x3e, 0x61, 0x3c, 0x66, 0x66, 0x3c, 0x86, 0x7c},
    {0x00, 0x00, 0x00, 0x00, 0x7e, 0x7e, 0x7e, 0x00},
    {0x18, 0x3c, 0x7e, 0x18, 0x7e, 0x3c, 0x18, 0xff},
    {0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x00},
    {0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00},
    {0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00},
    {0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00},
    {0x00, 0x00, 0xc0, 0xc0, 0xc0, 0xfe, 0x00, 0x00},
    {0x00, 0x24, 0x66, 0xff, 0x66, 0x24, 0x00, 0x00},
    {0x00, 0x18, 0x3c, 0x7e, 0xff, 0xff, 0x00, 0x00},
    {0x00, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x18, 0x3c, 0x3c, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x6c, 0x6c, 0xfe, 0x6c, 0xfe, 0x6c, 0x6c, 0x00},
    {0x18, 0x3e, 0x60, 0x3c, 0x06, 0x7c, 0x18, 0x00},
    {0x00, 0xc6, 0xcc, 0x18, 0x30, 0x66, 0xc6, 0x00},
    {0x38, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0x76, 0x00},
    {0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x0c, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00},
    {0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00},
    {0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00},
    {0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    {0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    {0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x80, 0x00},
    {0x38, 0x6c, 0xc6, 0xd6, 0xc6, 0x6c, 0x38, 0x00},
    {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00},
    {0x7c, 0xc6, 0x06, 0x1c, 0x30, 0x66, 0xfe, 0x00},
    {0x7c, 0xc6, 0x06, 0x3c, 0x06, 0xc6, 0x7c, 0x00},
    {0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x1e, 0x00},
    {0xfe, 0xc0, 0xc0, 0xfc, 0x06, 0xc6, 0x7c, 0x00},
    {0x38, 0x60, 0xc0, 0xfc, 0xc6, 0xc6, 0x7c, 0x00},
    {0xfe, 0xc6, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x00},
    {0x7c, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0x7c, 0x00},
    {0x7c, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0x78, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00},
    {0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30},
    {0x06, 0x0c, 0x18, 0x30, 0x18, 0x0c, 0x06, 0x00},
    {0x00, 0x00, 0x7e, 0x00, 0x00, 0x7e, 0x00, 0x00},
    {0x60, 0x30, 0x18, 0x0c, 0x18, 0x30, 0x60, 0x00},
    {0x7c, 0xc6, 0x0c, 0x18, 0x18, 0x00, 0x18, 0x00},
    {0x7c, 0xc6, 0xde, 0xde, 0xde, 0xc0, 0x78, 0x00},
    {0x38, 0x6c, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00},
    {0xfc, 0x66, 0x66, 0x7c, 0x66, 0x66, 0xfc, 0x00},
    {0x3c, 0x66, 0xc0, 0xc0, 0xc0, 0x66, 0x3c, 0x00},
    {0xf8, 0x6c, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00},
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x62, 0xfe, 0x00},
    {0xfe, 0x62, 0x68, 0x78, 0x68, 0x60, 0xf0, 0x00},
    {0x3c, 0x66, 0xc0, 0xc0, 0xce, 0x66, 0x3a, 0x00},
    {0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0x00},
    {0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00},
    {0x1e, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0x78, 0x00},
    {0xe6, 0x66, 0x6c, 0x78, 0x6c, 0x66, 0xe6, 0x00},
    {0xf0, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00},
    {0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0x00},
    {0xc6, 0xe6, 0xf6, 0xde, 0xce, 0xc6, 0xc6, 0x00},
    {0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},
    {0xfc, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00},
    {0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xce, 0x7c, 0x0e},
    {0xfc, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0xe6, 0x00},
    {0x3c, 0x66, 0x30, 0x18, 0x0c, 0x66, 0x3c, 0x00},
    {0x7e, 0x7e, 0x5a, 0x18, 0x18, 0x18, 0x3c, 0x00},
    {0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},
    {0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},
    {0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00},
    {0xc6, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0xc6, 0x00},
    {0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x3c, 0x00},
    {0xfe, 0xc6, 0x8c, 0x18, 0x32, 0x66, 0xfe, 0x00},
    {0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00},
    {0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x02, 0x00},
    {0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00},
    {0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff},
    {0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0x76, 0x00},
    {0xe0, 0x60, 0x7c, 0x66, 0x66, 0x66, 0xdc, 0x00},
    {0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc6, 0x7c, 0x00},
    {0x1c, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00},
    {0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0x7c, 0x00},
    {0x3c, 0x66, 0x60, 0xf8, 0x60, 0x60, 0xf0, 0x00},
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0xf8},
    {0xe0, 0x60, 0x6c, 0x76, 0x66, 0x66, 0xe6, 0x00},
    {0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3c, 0x00},
    {0x06, 0x00, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c},
    {0xe0, 0x60, 0x66, 0x6c, 0x78, 0x6c, 0xe6, 0x00},
    {0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00},
    {0x00, 0x00, 0xec, 0xfe, 0xd6, 0xd6, 0xd6, 0x00},
    {0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x00},
    {0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0x00},
    {0x00, 0x00, 0xdc, 0x66, 0x66, 0x7c, 0x60, 0xf0},
    {0x00, 0x00, 0x76, 0xcc, 0xcc, 0x7c, 0x0c, 0x1e},
    {0x00, 0x00, 0xdc, 0x76, 0x60, 0x60, 0xf0, 0x00},
    {0x00, 0x00, 0x7e, 0xc0, 0x7c, 0x06, 0xfc, 0x00},
    {0x30, 0x30, 0xfc, 0x30, 0x30, 0x36, 0x1c, 0x00},
    {0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00},
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x00},
    {0x00, 0x00, 0xc6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00},
    {0x00, 0x00, 0xc6, 0x6c, 0x38, 0x6c, 0xc6, 0x00},
    {0x00, 0x00, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0xfc},
    {0x00, 0x00, 0x7e, 0x4c, 0x18, 0x32, 0x7e, 0x00},
    {0x0e, 0x18, 0x18, 0x70, 0x18, 0x18, 0x0e, 0x00},
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    {0x70, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x70, 0x00},
    {0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0x00}
};

static C3D_Tex s_fontAtlasTex;
static bool s_fontAtlasInit = false;
static float s_glyphWidth[256];
static float s_glyphAdvance[256];

static BottomRightMode s_brMode = BR_MODE_PREVIEW;

void uiSetBottomRightMode(BottomRightMode mode) {
    s_brMode = mode;
}

BottomRightMode uiGetBottomRightMode() {
    return s_brMode;
}

void uiCycleBottomRightMode() {
    s_brMode = (BottomRightMode)((s_brMode + 1) % 3);
}

static inline void emitUiQuad(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2) {
    C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f); C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f); C3D_ImmSendAttrib(u2, v2, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f); C3D_ImmSendAttrib(u2, v1, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

    C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f); C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f); C3D_ImmSendAttrib(u1, v2, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f); C3D_ImmSendAttrib(u2, v2, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
}

void uiRenderInit() {
    if(s_fontAtlasInit) {
        return;
    }

    if(!C3D_TexInit(&s_fontAtlasTex, 256, 256, GPU_RGBA8)) {
        return;
    }

    u32* temp = (u32*) linearAlloc(256 * 256 * sizeof(u32));
    if(temp == nullptr) {
        C3D_TexDelete(&s_fontAtlasTex);
        return;
    }

    memset(temp, 0, 256 * 256 * sizeof(u32));

    u8 fontTable[256][8];
    memset(fontTable, 0, sizeof(fontTable));
    for(int i = 0; i < 128; i++) {
        memcpy(fontTable[i], font8x8_basic[i], 8);
    }

    // Acentos españoles minúsculas (tilde aguda en filas 0..1 + cuerpo base)
    memcpy(fontTable[0xE1], font8x8_basic['a'], 8); fontTable[0xE1][0] = 0x0C; fontTable[0xE1][1] = 0x18;
    memcpy(fontTable[0xE9], font8x8_basic['e'], 8); fontTable[0xE9][0] = 0x0C; fontTable[0xE9][1] = 0x18;
    memcpy(fontTable[0xED], font8x8_basic['i'], 8); fontTable[0xED][0] = 0x0C; fontTable[0xED][1] = 0x18;
    memcpy(fontTable[0xF3], font8x8_basic['o'], 8); fontTable[0xF3][0] = 0x0C; fontTable[0xF3][1] = 0x18;
    memcpy(fontTable[0xFA], font8x8_basic['u'], 8); fontTable[0xFA][0] = 0x0C; fontTable[0xFA][1] = 0x18;
    memcpy(fontTable[0xF1], font8x8_basic['n'], 8); fontTable[0xF1][0] = 0x76; fontTable[0xF1][1] = 0x00;

    // Acentos españoles mayúsculas
    memcpy(fontTable[0xC1], font8x8_basic['A'], 8);
    memcpy(fontTable[0xC9], font8x8_basic['E'], 8);
    memcpy(fontTable[0xCD], font8x8_basic['I'], 8);
    memcpy(fontTable[0xD3], font8x8_basic['O'], 8);
    memcpy(fontTable[0xDA], font8x8_basic['U'], 8);
    memcpy(fontTable[0xD1], font8x8_basic['N'], 8);

    // Símbolos especiales
    static const u8 bullet[8]  = {0x00, 0x00, 0x3C, 0x7E, 0x7E, 0x3C, 0x00, 0x00}; memcpy(fontTable[0x80], bullet, 8);
    static const u8 rarrow[8]  = {0x00, 0x60, 0x78, 0x7E, 0x78, 0x60, 0x00, 0x00}; memcpy(fontTable[0x81], rarrow, 8);
    static const u8 larrow[8]  = {0x00, 0x06, 0x1E, 0x7E, 0x1E, 0x06, 0x00, 0x00}; memcpy(fontTable[0x82], larrow, 8);
    static const u8 uarrow[8]  = {0x00, 0x18, 0x3C, 0x7E, 0xFF, 0x00, 0x00, 0x00}; memcpy(fontTable[0x83], uarrow, 8);
    static const u8 darrow[8]  = {0x00, 0x00, 0xFF, 0x7E, 0x3C, 0x18, 0x00, 0x00}; memcpy(fontTable[0x84], darrow, 8);
    static const u8 star[8]    = {0x10, 0x38, 0xFE, 0x7C, 0x38, 0x6C, 0xC6, 0x00}; memcpy(fontTable[0x85], star, 8);
    static const u8 gear[8]    = {0x00, 0x66, 0x3C, 0x5A, 0x3C, 0x66, 0x00, 0x00}; memcpy(fontTable[0x86], gear, 8);
    static const u8 invExc[8]  = {0x00, 0x18, 0x00, 0x18, 0x18, 0x3C, 0x3C, 0x18}; memcpy(fontTable[0xA1], invExc, 8);
    static const u8 invQst[8]  = {0x00, 0x18, 0x00, 0x18, 0x18, 0x30, 0x63, 0x3E}; memcpy(fontTable[0xBF], invQst, 8);

    for(int c = 0; c < 256; c++) {
        int cellCol = c & 0x0F;
        int cellRow = (c >> 4) & 0x0F;
        int cellX = cellCol * 16;
        int cellY = cellRow * 16;

        int min_col = 8;
        int max_col = -1;
        for(int y = 0; y < 8; y++) {
            u8 row = fontTable[c][y];
            if(row != 0) {
                for(int x = 0; x < 8; x++) {
                    if(row & (0x80 >> x)) {
                        if(x < min_col) min_col = x;
                        if(x > max_col) max_col = x;
                    }
                }
            }
        }

        if(max_col < 0) {
            s_glyphWidth[c] = 0.0f;
            s_glyphAdvance[c] = 8.0f;
        } else {
            int charW8 = max_col - min_col + 1;
            s_glyphWidth[c] = (float)(charW8 * 2);
            s_glyphAdvance[c] = s_glyphWidth[c] + 2.0f;

            int shift = min_col;
            for(int sub_y = 0; sub_y < 8; sub_y++) {
                u8 rowBits = fontTable[c][sub_y];
                if(rowBits == 0) continue;
                for(int sub_x = 0; sub_x < 8; sub_x++) {
                    if(rowBits & (0x80 >> sub_x)) {
                        int localX = (sub_x - shift) * 2;
                        int localY = sub_y * 2;
                        temp[(cellY + localY) * 256 + (cellX + localX)] = 0xFFFFFFFF;
                        temp[(cellY + localY) * 256 + (cellX + localX + 1)] = 0xFFFFFFFF;
                        temp[(cellY + localY + 1) * 256 + (cellX + localX)] = 0xFFFFFFFF;
                        temp[(cellY + localY + 1) * 256 + (cellX + localX + 1)] = 0xFFFFFFFF;
                    }
                }
            }
        }
    }

    GSPGPU_FlushDataCache(temp, 256 * 256 * sizeof(u32));
    C3D_SyncDisplayTransfer(
        temp, GX_BUFFER_DIM(256, 256),
        (u32*) s_fontAtlasTex.data, GX_BUFFER_DIM(256, 256),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
    );
    linearFree(temp);

    GSPGPU_InvalidateDataCache(s_fontAtlasTex.data, s_fontAtlasTex.size);
    C3D_TexSetFilter(&s_fontAtlasTex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&s_fontAtlasTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    s_fontAtlasInit = true;

    // Boxart texture 128x128 RGBA8
    if(!s_boxartTexAllocated) {
        if(C3D_TexInit(&s_boxartTex, 128, 128, GPU_RGBA8)) {
            C3D_TexSetFilter(&s_boxartTex, GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap(&s_boxartTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            s_boxartTexAllocated = true;
        }
    }

    mkdir("/3ds", 0777);
    mkdir("/3ds/gameyob", 0777);
    mkdir("/3ds/gameyob/boxart", 0777);
}

static bool s_ptmuInitialized = false;
static u8 s_lastBatteryLevel = 5;
static u64 s_lastBatteryCheck = 0;

static u8 getBatteryLevel() {
    u64 now = osGetTime();
    if(now - s_lastBatteryCheck > 5000 || s_lastBatteryCheck == 0) {
        s_lastBatteryCheck = now;
        if(!s_ptmuInitialized) {
            if(R_SUCCEEDED(ptmuInit())) {
                s_ptmuInitialized = true;
            }
        }
        if(s_ptmuInitialized) {
            u8 level = 5;
            if(R_SUCCEEDED(PTMU_GetBatteryLevel(&level))) {
                s_lastBatteryLevel = level;
            }
        }
    }
    return s_lastBatteryLevel;
}

void uiRenderExit() {
    if(s_ptmuInitialized) {
        ptmuExit();
        s_ptmuInitialized = false;
    }
    if(s_fontAtlasInit) {
        C3D_TexDelete(&s_fontAtlasTex);
        s_fontAtlasInit = false;
    }
    if(s_boxartTexAllocated) {
        C3D_TexDelete(&s_boxartTex);
        s_boxartTexAllocated = false;
        s_boxartLoaded = false;
        s_lastBoxartPath = "";
        s_boxartChecked = false;
    }
}

void uiDrawRect(float x, float y, float w, float h, u32 color) {
    if(w <= 0.0f || h <= 0.0f) {
        return;
    }

    if((color & 0xFF000000) == 0) {
        if((color & 0x00FFFFFF) != 0) {
            color |= 0xFF000000;
        } else {
            return;
        }
    }

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
    C3D_TexEnvColor(env0, color);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_ImmDrawBegin(GPU_TRIANGLES);
    emitUiQuad(x, y, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmDrawEnd();
}

void uiDrawRectOutline(float x, float y, float w, float h, float thickness, u32 color) {
    if(w <= 0.0f || h <= 0.0f || thickness <= 0.0f) {
        return;
    }

    if((color & 0xFF000000) == 0) {
        if((color & 0x00FFFFFF) != 0) {
            color |= 0xFF000000;
        } else {
            return;
        }
    }

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
    C3D_TexEnvColor(env0, color);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_ImmDrawBegin(GPU_TRIANGLES);
    // Top
    emitUiQuad(x, y, x + w, y + thickness, 0.0f, 0.0f, 0.0f, 0.0f);
    // Bottom
    emitUiQuad(x, y + h - thickness, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f);
    // Left
    emitUiQuad(x, y + thickness, x + thickness, y + h - thickness, 0.0f, 0.0f, 0.0f, 0.0f);
    // Right
    emitUiQuad(x + w - thickness, y + thickness, x + w, y + h - thickness, 0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmDrawEnd();
}

void uiDrawDropShadow(float x, float y, float w, float h, float radius, u8 maxAlpha) {
    if(w <= 0.0f || h <= 0.0f || radius <= 0.0f || maxAlpha == 0) {
        return;
    }

    // Sombra proyectada con capas discretas escalonadas (cero texturas, cero DMA)
    int steps = (radius >= 3.0f) ? 2 : 1;
    for(int s = steps; s >= 1; s--) {
        float r = radius * ((float) s / (float) steps);
        u8 a = (u8) ((float) maxAlpha * ((float) (steps - s + 1) / (float) (steps * 2)));
        if(a == 0) {
            continue;
        }
        u32 col = uiColorRGBA(0, 0, 0, a);
        // Banda inferior
        uiDrawRect(x + 1.0f, y + h, w + r - 1.0f, r, col);
        // Banda derecha
        uiDrawRect(x + w, y + 1.0f, r, h, col);
    }
    // Núcleo suave bajo el cuerpo del elemento
    uiDrawRect(x + 2.0f, y + 2.0f, w - 4.0f, h - 2.0f, uiColorRGBA(0, 0, 0, (u8)(maxAlpha * 0.35f)));
}

void uiDrawRoundedRect(float x, float y, float w, float h, float r, u32 fillColor, u32 borderColor, float borderWidth) {
    if(w <= 0.0f || h <= 0.0f) {
        return;
    }

    if(r > w * 0.5f) {
        r = w * 0.5f;
    }
    if(r > h * 0.5f) {
        r = h * 0.5f;
    }

    if(r <= 1.0f) {
        if((fillColor >> 24) > 0) {
            uiDrawRect(x, y, w, h, fillColor);
        }
        if(borderWidth > 0.0f && (borderColor >> 24) > 0) {
            uiDrawRectOutline(x, y, w, h, borderWidth, borderColor);
        }
        return;
    }

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
    C3D_TexEnvInit(C3D_GetTexEnv(1));

    // 1. Relleno con esquinas biseladas (3 quads + 4 triángulos de esquina)
    if((fillColor >> 24) > 0) {
        C3D_TexEnvColor(env0, fillColor);
        C3D_ImmDrawBegin(GPU_TRIANGLES);

        // Centro vertical
        emitUiQuad(x, y + r, x + w, y + h - r, 0.0f, 0.0f, 0.0f, 0.0f);
        // Franja superior central
        emitUiQuad(x + r, y, x + w - r, y + r, 0.0f, 0.0f, 0.0f, 0.0f);
        // Franja inferior central
        emitUiQuad(x + r, y + h - r, x + w - r, y + h, 0.0f, 0.0f, 0.0f, 0.0f);

        auto emitTri = [](float px0, float py0, float px1, float py1, float px2, float py2) {
            C3D_ImmSendAttrib(px0, py0, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(px1, py1, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(px2, py2, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
        };

        // 4 esquinas biseladas a 45°
        // Top-Left
        emitTri(x + r, y + r, x, y + r, x + r, y);
        // Top-Right
        emitTri(x + w - r, y + r, x + w - r, y, x + w, y + r);
        // Bottom-Right
        emitTri(x + w - r, y + h - r, x + w, y + h - r, x + w - r, y + h);
        // Bottom-Left
        emitTri(x + r, y + h - r, x + r, y + h, x, y + h - r);

        C3D_ImmDrawEnd();
    }

    // 2. Borde con esquinas biseladas (4 quads rectos + 4 quads en las esquinas)
    if(borderWidth > 0.0f && (borderColor >> 24) > 0) {
        float bw = borderWidth;
        C3D_TexEnvColor(env0, borderColor);
        C3D_ImmDrawBegin(GPU_TRIANGLES);

        // 4 Bordes rectos
        emitUiQuad(x + r, y, x + w - r, y + bw, 0.0f, 0.0f, 0.0f, 0.0f);         // Top
        emitUiQuad(x + r, y + h - bw, x + w - r, y + h, 0.0f, 0.0f, 0.0f, 0.0f); // Bottom
        emitUiQuad(x, y + r, x + bw, y + h - r, 0.0f, 0.0f, 0.0f, 0.0f);         // Left
        emitUiQuad(x + w - bw, y + r, x + w, y + h - r, 0.0f, 0.0f, 0.0f, 0.0f); // Right

        auto emitBevelQuad = [](float p0x, float p0y, float p1x, float p1y,
                                float p2x, float p2y, float p3x, float p3y) {
            C3D_ImmSendAttrib(p0x, p0y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(p1x, p1y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(p2x, p2y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

            C3D_ImmSendAttrib(p0x, p0y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(p2x, p2y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
            C3D_ImmSendAttrib(p3x, p3y, 0.5f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
        };

        // 4 esquinas biseladas
        emitBevelQuad(x, y + r, x + r, y, x + r, y + bw, x + bw, y + r);                                         // Top-Left
        emitBevelQuad(x + w - r, y, x + w, y + r, x + w - bw, y + r, x + w - r, y + bw);                         // Top-Right
        emitBevelQuad(x + w, y + h - r, x + w - r, y + h, x + w - r, y + h - bw, x + w - bw, y + h - r);         // Bottom-Right
        emitBevelQuad(x + r, y + h, x, y + h - r, x + bw, y + h - r, x + r, y + h - bw);                         // Bottom-Left

        C3D_ImmDrawEnd();
    }
}

void uiDrawPillBadge(float x, float y, float w, float h, const char* text, u32 bgColor, u32 textColor) {
    if(!text || w <= 0.0f || h <= 0.0f) {
        return;
    }
    float r = h * 0.5f;
    uiDrawRoundedRect(x, y, w, h, r, bgColor, 0, 0.0f);
    int len = strlen(text);
    float textW = (float) len * 8.0f;
    float tx = x + (w - textW) * 0.5f;
    float ty = y + (h - 8.0f) * 0.5f;
    uiDrawText(tx, ty, text, textColor);
}

void uiDrawModernButton(float x, float y, float w, float h, const char* label, u32 baseColor, u32 borderColor, bool isPressed) {
    if(w <= 0.0f || h <= 0.0f) {
        return;
    }

    float r = 4.0f;
    if(!isPressed) {
        // Sombra proyectada exterior suave (radius = 3px)
        uiDrawDropShadow(x, y + 1.0f, w, h, 3.0f, 90);
        // Caja redondeada (r = 4px) con el color base
        uiDrawRoundedRect(x, y, w, h, r, baseColor, borderColor, 1.0f);
        // Línea de brillo superior (highlight de 1px en y+1) con mezcla aditiva o blanco translúcido
        uiDrawRect(x + r, y + 1.0f, w - 2.0f * r, 1.0f, uiColorRGBA(255, 255, 255, 45));
        // Línea de sombra inferior (1px en y+h-2) oscurecida
        uiDrawRect(x + r, y + h - 2.0f, w - 2.0f * r, 1.0f, uiColorRGBA(0, 0, 0, 60));
    } else {
        uiDrawDropShadow(x, y, w, h, 1.5f, 120);
        u8 rC = (baseColor >> 0) & 0xFF;
        u8 gC = (baseColor >> 8) & 0xFF;
        u8 bC = (baseColor >> 16) & 0xFF;
        u32 pressedColor = uiColorRGBA(rC * 0.8f, gC * 0.8f, bC * 0.8f, 255);
        uiDrawRoundedRect(x, y + 1.0f, w, h, r, pressedColor, borderColor, 1.0f);
    }

    if(label && *label) {
        float tw = uiGetTextWidth(label, 0.70f);
        float tx = x + (w - tw) * 0.5f;
        float ty = y + (h - 12.0f) * 0.5f + (isPressed ? 1.0f : 0.0f);
        uiDrawTextScaled(tx, ty, 0.70f, label, UI_COLOR_WHITE);
    }
}

void uiDrawFolderIcon(float x, float y) {
    uiDrawRect(x, y + 1.0f, 6.0f, 2.0f, uiColorRGBA(240, 195, 60, 255));
    uiDrawRect(x, y + 3.0f, 13.0f, 9.0f, uiColorRGBA(220, 165, 40, 255));
    uiDrawRect(x, y + 5.0f, 13.0f, 7.0f, uiColorRGBA(255, 215, 80, 255));
    uiDrawRect(x, y + 12.0f, 13.0f, 1.0f, uiColorRGBA(150, 100, 20, 255));
    uiDrawRectOutline(x, y + 3.0f, 13.0f, 10.0f, 1.0f, uiColorRGBA(140, 95, 15, 255));
}

void uiDrawCartridgeIcon(float x, float y, bool isGBC) {
    u32 colBody = isGBC ? uiColorRGBA(55, 65, 85, 255) : uiColorRGBA(175, 175, 180, 255);
    u32 colBorder = isGBC ? uiColorRGBA(80, 95, 125, 255) : uiColorRGBA(130, 130, 135, 255);
    u32 colRidge = isGBC ? uiColorRGBA(90, 105, 135, 255) : uiColorRGBA(205, 205, 210, 255);
    u32 colStripe = isGBC ? uiColorRGBA(180, 40, 160, 255) : uiColorRGBA(180, 30, 40, 255);

    uiDrawRect(x, y + 1.0f, 11.0f, 13.0f, colBody);
    uiDrawRectOutline(x, y + 1.0f, 11.0f, 13.0f, 1.0f, colBorder);
    uiDrawRect(x + 8.0f, y + 1.0f, 3.0f, 2.0f, uiColorRGBA(16, 21, 30, 255));
    uiDrawRect(x + 2.0f, y + 2.0f, 5.0f, 1.0f, colRidge);
    uiDrawRect(x + 2.0f, y + 5.0f, 7.0f, 7.0f, uiColorRGBA(245, 245, 250, 255));
    uiDrawRect(x + 2.0f, y + 5.0f, 7.0f, 2.0f, colStripe);
}

void uiDrawTextScaled(float x, float y, float scale, const char* str, u32 color) {
    if(!str || *str == '\0' || !s_fontAtlasInit) {
        return;
    }
    if((color & 0xFF000000) == 0) {
        color |= 0xFF000000;
    }

    C3D_TexBind(0, &s_fontAtlasTex);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_RGB, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_RGB, GPU_REPLACE);
    C3D_TexEnvSrc(env0, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Alpha, GPU_MODULATE);
    C3D_TexEnvColor(env0, color);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    C3D_ImmDrawBegin(GPU_TRIANGLES);

    float curX = x;
    float curY = y;
    const char* p = str;
    while(*p != '\0') {
        if(*p == '\n') {
            curX = x;
            curY += 16.0f * scale;
            p++;
            continue;
        }
        if(*p == '\r') {
            p++;
            continue;
        }

        u8 c = (u8)*p;
        u8 glyphIdx = 0;
        if((c & 0x80) == 0) {
            glyphIdx = c;
            p++;
        } else if((c & 0xE0) == 0xC0 && *(p + 1) != '\0') {
            u8 c2 = (u8)*(p + 1);
            p += 2;
            if(c == 0xC3) {
                switch(c2) {
                    case 0x81: glyphIdx = 0xC1; break; // Á
                    case 0x89: glyphIdx = 0xC9; break; // É
                    case 0x8D: glyphIdx = 0xCD; break; // Í
                    case 0x93: glyphIdx = 0xD3; break; // Ó
                    case 0x9A: glyphIdx = 0xDA; break; // Ú
                    case 0x91: glyphIdx = 0xD1; break; // Ñ
                    case 0xA1: glyphIdx = 0xE1; break; // á
                    case 0xA9: glyphIdx = 0xE9; break; // é
                    case 0xAD: glyphIdx = 0xED; break; // í
                    case 0xB3: glyphIdx = 0xF3; break; // ó
                    case 0xBA: glyphIdx = 0xFA; break; // ú
                    case 0xB1: glyphIdx = 0xF1; break; // ñ
                    default:   glyphIdx = c2;   break;
                }
            } else if(c == 0xC2) {
                switch(c2) {
                    case 0xBF: glyphIdx = 0xBF; break; // ¿
                    case 0xA1: glyphIdx = 0xA1; break; // ¡
                    default:   glyphIdx = c2;   break;
                }
            } else {
                glyphIdx = '?';
            }
        } else if((c & 0xF0) == 0xE0 && *(p + 1) != '\0' && *(p + 2) != '\0') {
            u8 c2 = (u8)*(p + 1);
            u8 c3 = (u8)*(p + 2);
            p += 3;
            if(c == 0xE2 && c2 == 0x80 && c3 == 0xA2) {
                glyphIdx = 0x80; // •
            } else if(c == 0xE2 && c2 == 0x96 && c3 == 0xBA) {
                glyphIdx = 0x81; // ►
            } else if(c == 0xE2 && c2 == 0x97 && c3 == 0x84) {
                glyphIdx = 0x82; // ◄
            } else if(c == 0xE2 && c2 == 0x96 && c3 == 0xB2) {
                glyphIdx = 0x83; // ▲
            } else if(c == 0xE2 && c2 == 0x96 && c3 == 0xBC) {
                glyphIdx = 0x84; // ▼
            } else if(c == 0xE2 && c2 == 0x98 && c3 == 0x85) {
                glyphIdx = 0x85; // ★
            } else if(c == 0xE2 && c2 == 0x9A && c3 == 0x99) {
                glyphIdx = 0x86; // ⚙
            } else {
                glyphIdx = '?';
            }
        } else {
            glyphIdx = '?';
            p++;
        }

        float gw = s_glyphWidth[glyphIdx];
        float ga = s_glyphAdvance[glyphIdx];
        if(gw > 0.0f) {
            float u0 = ((glyphIdx & 0x0F) * 16.0f) / 256.0f;
            float v0 = (((glyphIdx >> 4) & 0x0F) * 16.0f) / 256.0f;
            float u1 = u0 + (gw / 256.0f);
            float v1 = v0 + (16.0f / 256.0f);
            float qW = gw * scale;
            float qH = 16.0f * scale;
            emitUiQuad(curX, curY, curX + qW, curY + qH, u0, v0, u1, v1);
        }
        curX += ga * scale;
    }

    C3D_ImmDrawEnd();
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
}

float uiGetTextWidth(const char* str, float scale) {
    if(!str || *str == '\0' || !s_fontAtlasInit) {
        return 0.0f;
    }
    float totalW = 0.0f;
    float maxLineW = 0.0f;
    const char* p = str;
    while(*p != '\0') {
        if(*p == '\n') {
            if(totalW > maxLineW) maxLineW = totalW;
            totalW = 0.0f;
            p++;
            continue;
        }
        if(*p == '\r') {
            p++;
            continue;
        }

        u8 c = (u8)*p;
        u8 glyphIdx = 0;
        if((c & 0x80) == 0) {
            glyphIdx = c;
            p++;
        } else if((c & 0xE0) == 0xC0 && *(p + 1) != '\0') {
            u8 c2 = (u8)*(p + 1);
            p += 2;
            if(c == 0xC3) {
                switch(c2) {
                    case 0x81: glyphIdx = 0xC1; break;
                    case 0x89: glyphIdx = 0xC9; break;
                    case 0x8D: glyphIdx = 0xCD; break;
                    case 0x93: glyphIdx = 0xD3; break;
                    case 0x9A: glyphIdx = 0xDA; break;
                    case 0x91: glyphIdx = 0xD1; break;
                    case 0xA1: glyphIdx = 0xE1; break;
                    case 0xA9: glyphIdx = 0xE9; break;
                    case 0xAD: glyphIdx = 0xED; break;
                    case 0xB3: glyphIdx = 0xF3; break;
                    case 0xBA: glyphIdx = 0xFA; break;
                    case 0xB1: glyphIdx = 0xF1; break;
                    default:   glyphIdx = c2;   break;
                }
            } else if(c == 0xC2) {
                switch(c2) {
                    case 0xBF: glyphIdx = 0xBF; break;
                    case 0xA1: glyphIdx = 0xA1; break;
                    default:   glyphIdx = c2;   break;
                }
            } else {
                glyphIdx = '?';
            }
        } else if((c & 0xF0) == 0xE0 && *(p + 1) != '\0' && *(p + 2) != '\0') {
            u8 c2 = (u8)*(p + 1);
            u8 c3 = (u8)*(p + 2);
            p += 3;
            if(c == 0xE2 && c2 == 0x80 && c3 == 0xA2) glyphIdx = 0x80;
            else if(c == 0xE2 && c2 == 0x96 && c3 == 0xBA) glyphIdx = 0x81;
            else if(c == 0xE2 && c2 == 0x97 && c3 == 0x84) glyphIdx = 0x82;
            else if(c == 0xE2 && c2 == 0x96 && c3 == 0xB2) glyphIdx = 0x83;
            else if(c == 0xE2 && c2 == 0x96 && c3 == 0xBC) glyphIdx = 0x84;
            else if(c == 0xE2 && c2 == 0x98 && c3 == 0x85) glyphIdx = 0x85;
            else if(c == 0xE2 && c2 == 0x9A && c3 == 0x99) glyphIdx = 0x86;
            else glyphIdx = '?';
        } else {
            glyphIdx = '?';
            p++;
        }
        totalW += s_glyphAdvance[glyphIdx] * scale;
    }
    if(totalW > maxLineW) maxLineW = totalW;
    return maxLineW;
}

void uiDrawText(float x, float y, const char* str, u32 color) {
    uiDrawTextScaled(x, y, 0.75f, str, color);
}

void uiDrawSysText(float x, float y, float scaleX, float scaleY, u32 color, const char* text) {
    float effectiveScale = (scaleY <= 0.5f) ? (scaleY * 1.875f) : scaleY;
    uiDrawTextScaled(x, y, effectiveScale, text, color);
}

float uiGetSysTextWidth(float scaleX, const char* text) {
    float effectiveScale = (scaleX <= 0.5f) ? (scaleX * 1.875f) : scaleX;
    return uiGetTextWidth(text, effectiveScale);
}

static bool fileExists(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if(f != nullptr) {
        fclose(f);
        return true;
    }
    return false;
}

static std::string findBoxartPrefixMatch(const std::string& prefix) {
    if(prefix.length() < 4) {
        return "";
    }
    DIR* d = opendir("/3ds/gameyob/boxart");
    if(d == nullptr) {
        return "";
    }
    std::string match = "";
    struct dirent* ent;
    size_t cmpLen = std::min((size_t)6, prefix.length());
    while((ent = readdir(d)) != nullptr) {
        std::string fname = ent->d_name;
        if(fname.length() >= 4 && strcasecmp(fname.substr(fname.length() - 4).c_str(), ".png") == 0) {
            if(fname.length() >= cmpLen && strncasecmp(fname.c_str(), prefix.c_str(), cmpLen) == 0) {
                match = "/3ds/gameyob/boxart/" + fname;
                break;
            }
        }
    }
    closedir(d);
    return match;
}

void uiInvalidateBoxart() {
    s_lastBoxartPath = "";
    s_boxartLoaded = false;
    s_boxartChecked = false;
    s_metaLoaded = false;
    s_lastMetaRom = "";
}

void uiUpdateBoxart(const char* romPath, const char* romTitle, u8 mbcType, bool isGBC) {
    (void) mbcType;
    if(romPath == nullptr || romPath[0] == '\0') {
        uiInvalidateBoxart();
        return;
    }

    std::string pathStr = romPath;
    s_lastBoxartPath = pathStr;
    s_isGBCBox = isGBC;

    std::string romBaseName = pathStr;
    auto slash = romBaseName.find_last_of("/\\");
    std::string romDir = (slash != std::string::npos) ? romBaseName.substr(0, slash + 1) : "";
    if(slash != std::string::npos) {
        romBaseName = romBaseName.substr(slash + 1);
    }
    auto dot = romBaseName.find_last_of('.');
    if(dot != std::string::npos) {
        romBaseName = romBaseName.substr(0, dot);
    }

    auto fullDot = pathStr.find_last_of('.');
    std::string romPathSinExt = (fullDot != std::string::npos) ? pathStr.substr(0, fullDot) : pathStr;

    std::string cleanTitle = romTitle ? romTitle : "";
    if(cleanTitle.empty() && s_metaLoaded) {
        cleanTitle = s_currentMeta.title;
    }
    while(!cleanTitle.empty() && (cleanTitle.back() == ' ' || cleanTitle.back() == '\0')) {
        cleanTitle.pop_back();
    }

    // Búsqueda local flexible en 5 niveles:
    std::string cand1 = romPathSinExt + ".png";
    std::string cand2 = "/3ds/gameyob/boxart/" + romBaseName + ".png";
    std::string cand3 = cleanTitle.empty() ? "" : ("/3ds/gameyob/boxart/" + cleanTitle + ".png");
    std::string cand4 = romDir + "boxart/" + romBaseName + ".png";

    std::string targetPath = "";
    if(fileExists(cand1)) {
        targetPath = cand1;
    } else if(fileExists(cand2)) {
        targetPath = cand2;
    } else if(!cand3.empty() && fileExists(cand3)) {
        targetPath = cand3;
    } else if(fileExists(cand4)) {
        targetPath = cand4;
    } else {
        if(!cleanTitle.empty()) {
            targetPath = findBoxartPrefixMatch(cleanTitle);
        }
        if(targetPath.empty()) {
            targetPath = findBoxartPrefixMatch(romBaseName);
        }
    }

    bool loaded = false;
    if(!targetPath.empty() && s_boxartTexAllocated) {
        int imgW = 0, imgH = 0, imgComp = 0;
        u8* imgData = stbi_load(targetPath.c_str(), &imgW, &imgH, &imgComp, STBI_rgb_alpha);
        if(imgData != nullptr) {
            u32* temp = (u32*) linearAlloc(128 * 128 * sizeof(u32));
            if(temp != nullptr) {
                const u32* srcPx = (const u32*) imgData;
                for(int y = 0; y < 128; y++) {
                    int srcY = (y * imgH) / 128;
                    if(srcY >= imgH) srcY = imgH - 1;
                    int srcRow = srcY * imgW;
                    int dstRow = y * 128;
                    for(int x = 0; x < 128; x++) {
                        int srcX = (x * imgW) / 128;
                        if(srcX >= imgW) srcX = imgW - 1;
                        temp[dstRow + x] = srcPx[srcRow + srcX];
                    }
                }

                GSPGPU_FlushDataCache(temp, 128 * 128 * sizeof(u32));
                C3D_SyncDisplayTransfer(
                    temp, GX_BUFFER_DIM(128, 128),
                    (u32*) s_boxartTex.data, GX_BUFFER_DIM(128, 128),
                    GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                    GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
                );
                linearFree(temp);
                GSPGPU_InvalidateDataCache(s_boxartTex.data, s_boxartTex.size);
                loaded = true;
            }
            stbi_image_free(imgData);
        }
    }

    s_boxartLoaded = loaded;
    s_boxartChecked = true;
    if(!s_boxartLoaded) {
        scraperEnqueue(pathStr, cleanTitle, 0);
    }
}

static std::string s_lastSelectedRomPath = "";
static u64 s_menuSelectionChangeTime = 0;
static bool s_menuSelectionDebouncing = false;

void uiUpdateMenuState() {
    if(!isFileChooserActive()) {
        return;
    }
    FileChooser* chooser = getActiveFileChooser();
    if(chooser == nullptr) {
        return;
    }

    const std::vector<std::string>& files = chooser->getFilenames();
    const std::vector<int>& flags = chooser->getFlags();
    int numFiles = chooser->getNumFiles();
    int selection = chooser->getSelection();

    std::string currentPath = "";
    bool isDir = false;
    bool isGBC = false;

    if(selection >= 0 && selection < numFiles && selection < (int)files.size() && selection < (int)flags.size()) {
        isDir = (flags[selection] & 1) != 0;
        if(!isDir) {
            std::string dir = chooser->getDirectory();
            if(!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
                dir += "/";
            }
            currentPath = dir + files[selection];
            auto dot = files[selection].find_last_of('.');
            std::string ext = (dot != std::string::npos) ? files[selection].substr(dot + 1) : "";
            isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);
        }
    }

    u64 now = osGetTime();

    if(currentPath != s_lastSelectedRomPath) {
        s_lastSelectedRomPath = currentPath;
        s_menuSelectionChangeTime = now;
        s_menuSelectionDebouncing = true;
        // Mantener la carátula y metadatos previos visibles durante el debounce para evitar parpadeos negros
    }

    if(s_menuSelectionDebouncing && (now - s_menuSelectionChangeTime >= 120)) {
        s_menuSelectionDebouncing = false;
        if(!currentPath.empty() && !isDir) {
            s_metaLoaded = fetchRomMetadata(currentPath, s_currentMeta);
            s_lastMetaRom = currentPath;
            if(s_metaLoaded) {
                s_isGBCBox = s_currentMeta.isGBC;
            } else {
                s_isGBCBox = isGBC;
            }
            uiUpdateBoxart(currentPath.c_str(), s_metaLoaded ? s_currentMeta.title.c_str() : nullptr, 0, s_isGBCBox);
        } else {
            s_metaLoaded = false;
            s_boxartLoaded = false;
        }
    }
}

struct Vec3 { float x, y, z; };
struct Vec2 { float u, v; };

static inline void emitQuad3D(Vec3 p0, Vec2 uv0,
                              Vec3 p1, Vec2 uv1,
                              Vec3 p2, Vec2 uv2,
                              Vec3 p3, Vec2 uv3) {
    C3D_ImmSendAttrib(p0.x, p0.y, p0.z, 1.0f); C3D_ImmSendAttrib(uv0.u, uv0.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(p2.x, p2.y, p2.z, 1.0f); C3D_ImmSendAttrib(uv2.u, uv2.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(p1.x, p1.y, p1.z, 1.0f); C3D_ImmSendAttrib(uv1.u, uv1.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

    C3D_ImmSendAttrib(p0.x, p0.y, p0.z, 1.0f); C3D_ImmSendAttrib(uv0.u, uv0.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(p3.x, p3.y, p3.z, 1.0f); C3D_ImmSendAttrib(uv3.u, uv3.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
    C3D_ImmSendAttrib(p2.x, p2.y, p2.z, 1.0f); C3D_ImmSendAttrib(uv2.u, uv2.v, 0.0f, 0.0f); C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
}

static bool s_show3DArt = true;
static bool s_showSynopsis = false;
static int s_previewSlot = 0;

void uiToggle3DArt() {
    s_show3DArt = !s_show3DArt;
}

void uiToggleSynopsis() {
    s_showSynopsis = !s_showSynopsis;
}

bool uiIs3DArtEnabled() {
    return s_show3DArt;
}

bool uiIsSynopsisEnabled() {
    return s_showSynopsis;
}

int uiGetPreviewSlot() {
    return s_previewSlot;
}

void uiSetPreviewSlot(int slot) {
    if(slot < 0) slot = 9;
    else if(slot > 9) slot = 0;
    s_previewSlot = slot;
}

void uiCyclePreviewSlot(int delta) {
    uiSetPreviewSlot(s_previewSlot + delta);
}

static std::vector<std::string> wrapSynopsisText(const std::string& text, size_t maxLen) {
    std::vector<std::string> lines;
    size_t start = 0;
    while(start < text.length() && lines.size() < 4) {
        if(text.length() - start <= maxLen) {
            lines.push_back(text.substr(start));
            break;
        }
        size_t end = start + maxLen;
        size_t space = text.rfind(' ', end);
        if(space != std::string::npos && space > start) {
            lines.push_back(text.substr(start, space - start));
            start = space + 1;
        } else {
            lines.push_back(text.substr(start, maxLen));
            start = end;
        }
    }
    return lines;
}

void uiDrawBoxart3D(float centerX, float centerY, float size, const C3D_Mtx* proj, float eyeOffset) {
    if(proj == nullptr) {
        proj = gfxGetProjectionBottom();
    }
    if(eyeOffset <= 0.0f) {
        s_boxartAngleY += 0.02f;
    }
    float rotY = sinf(s_boxartAngleY) * 28.0f * (M_PI / 180.0f);
    float rotX = -8.0f * (M_PI / 180.0f);

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, centerX + eyeOffset, centerY, 0.5f, true);
    Mtx_Scale(&modelView, 1.0f, 1.0f, 0.005f);
    Mtx_RotateX(&modelView, rotX, true);
    Mtx_RotateY(&modelView, rotY, true);

    int uLocProj = gfxGetULocProjection();

    C3D_Mtx mvp;
    Mtx_Multiply(&mvp, proj, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &mvp);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);

    float hw = size * 0.44f;
    float hh = size * 0.44f;
    float hd = 8.0f; // 16px ancho en Z

    Vec3 FTL = {-hw, -hh,  hd};
    Vec3 FTR = { hw, -hh,  hd};
    Vec3 FBR = { hw,  hh,  hd};
    Vec3 FBL = {-hw,  hh,  hd};

    Vec3 BTL = {-hw, -hh, -hd};
    Vec3 BTR = { hw, -hh, -hd};
    Vec3 BBR = { hw,  hh, -hd};
    Vec3 BBL = {-hw,  hh, -hd};

    Vec2 UV_TL = {0.0f, 0.0f};
    Vec2 UV_TR = {1.0f, 0.0f};
    Vec2 UV_BR = {1.0f, 1.0f};
    Vec2 UV_BL = {0.0f, 1.0f};

    if(s_boxartLoaded && s_boxartTexAllocated) {
        C3D_TexBind(0, &s_boxartTex);
        C3D_TexEnv* env0 = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env0);
        C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
        C3D_TexEnvFunc(env0, C3D_Both, GPU_MODULATE);
        C3D_TexEnvInit(C3D_GetTexEnv(1));

        // 1. Cara Lateral (Lomo): 75% brillo
        C3D_TexEnvColor(env0, UI_COLOR_RGBA(190, 190, 190, 255));
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        if(rotY >= 0.0f) {
            emitQuad3D(BTL, UV_TL, FTL, UV_TL, FBL, UV_BL, BBL, UV_BL);
        } else {
            emitQuad3D(FTR, UV_TR, BTR, UV_TR, BBR, UV_BR, FBR, UV_BR);
        }
        C3D_ImmDrawEnd();

        // 2. Cara Superior: 85% brillo
        C3D_TexEnvColor(env0, UI_COLOR_RGBA(217, 217, 217, 255));
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        emitQuad3D(BTL, UV_TL, BTR, UV_TR, FTR, UV_TR, FTL, UV_TL);
        C3D_ImmDrawEnd();

        // 3. Cara Frontal: 100% brillo
        C3D_TexEnvColor(env0, UI_COLOR_RGBA(255, 255, 255, 255));
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        emitQuad3D(FTL, UV_TL, FTR, UV_TR, FBR, UV_BR, FBL, UV_BL);
        C3D_ImmDrawEnd();
    } else {
        C3D_TexEnv* env0 = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env0);
        C3D_TexEnvSrc(env0, C3D_Both, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
        C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
        C3D_TexEnvInit(C3D_GetTexEnv(1));

        u32 colBody = s_isGBCBox ? uiColorRGBA(48, 52, 60, 255) : uiColorRGBA(180, 180, 180, 255);
        u32 colSide = s_isGBCBox ? uiColorRGBA(36, 39, 45, 255) : uiColorRGBA(135, 135, 135, 255);
        u32 colTop  = s_isGBCBox ? uiColorRGBA(41, 44, 51, 255) : uiColorRGBA(153, 153, 153, 255);

        // 1. Cara Lateral
        C3D_TexEnvColor(env0, colSide);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        if(rotY >= 0.0f) {
            emitQuad3D(BTL, UV_TL, FTL, UV_TL, FBL, UV_BL, BBL, UV_BL);
        } else {
            emitQuad3D(FTR, UV_TR, BTR, UV_TR, BBR, UV_BR, FBR, UV_BR);
        }
        C3D_ImmDrawEnd();

        // 2. Cara Superior
        C3D_TexEnvColor(env0, colTop);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        emitQuad3D(BTL, UV_TL, BTR, UV_TR, FTR, UV_TR, FTL, UV_TL);
        C3D_ImmDrawEnd();

        // 3. Cara Frontal
        C3D_TexEnvColor(env0, colBody);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        emitQuad3D(FTL, UV_TL, FTR, UV_TR, FBR, UV_BR, FBL, UV_BL);
        C3D_ImmDrawEnd();

        // 4. Muesca superior
        u32 colGrip = s_isGBCBox ? uiColorRGBA(32, 35, 40, 255) : uiColorRGBA(140, 140, 140, 255);
        C3D_TexEnvColor(env0, colGrip);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        Vec3 GTL = {-hw + 4.0f, -hh + 3.0f, hd + 0.1f};
        Vec3 GTR = { hw - 4.0f, -hh + 3.0f, hd + 0.1f};
        Vec3 GBR = { hw - 4.0f, -hh + 7.0f, hd + 0.1f};
        Vec3 GBL = {-hw + 4.0f, -hh + 7.0f, hd + 0.1f};
        emitQuad3D(GTL, UV_TL, GTR, UV_TR, GBR, UV_BR, GBL, UV_BL);
        C3D_ImmDrawEnd();

        // 5. Etiqueta blanca frontal
        u32 colSticker = uiColorRGBA(245, 245, 248, 255);
        C3D_TexEnvColor(env0, colSticker);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        Vec3 STL = {-hw * 0.75f, -hh * 0.35f, hd + 0.2f};
        Vec3 STR = { hw * 0.75f, -hh * 0.35f, hd + 0.2f};
        Vec3 SBR = { hw * 0.75f,  hh * 0.85f, hd + 0.2f};
        Vec3 SBL = {-hw * 0.75f,  hh * 0.85f, hd + 0.2f};
        emitQuad3D(STL, UV_TL, STR, UV_TR, SBR, UV_BR, SBL, UV_BL);
        C3D_ImmDrawEnd();

        // 6. Franja de color superior de la etiqueta
        u32 colStripe = s_isGBCBox ? uiColorRGBA(140, 40, 180, 255) : uiColorRGBA(180, 30, 60, 255);
        C3D_TexEnvColor(env0, colStripe);
        C3D_ImmDrawBegin(GPU_TRIANGLES);
        Vec3 B1TL = {-hw * 0.75f, -hh * 0.35f, hd + 0.3f};
        Vec3 B1TR = { hw * 0.75f, -hh * 0.35f, hd + 0.3f};
        Vec3 B1BR = { hw * 0.75f, -hh * 0.15f, hd + 0.3f};
        Vec3 B1BL = {-hw * 0.75f, -hh * 0.15f, hd + 0.3f};
        emitQuad3D(B1TL, UV_TL, B1TR, UV_TR, B1BR, UV_BR, B1BL, UV_BL);
        C3D_ImmDrawEnd();
    }

    // Restaurar proyección ortográfica y estado de rasterizador
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, proj);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
    C3D_TexEnvInit(C3D_GetTexEnv(1));
}

// =============================================================================
// MODAL GRÁFICO INTERACTIVO (Hito 5.6)
// =============================================================================

static bool s_modalOpen = false;
static UiModalTab s_currentTab = UI_TAB_GAME;
static int s_selectedOption = 0;
static int s_cheatScroll = 0;

static char s_modalMessage[32] = "";
static u32 s_modalMessageColor = UI_COLOR_GREEN;
static u64 s_modalMessageTime = 0;

static void setModalMessage(const char* msg, u32 color = UI_COLOR_GREEN) {
    strncpy(s_modalMessage, msg, sizeof(s_modalMessage) - 1);
    s_modalMessage[sizeof(s_modalMessage) - 1] = '\0';
    s_modalMessageColor = color;
    s_modalMessageTime = osGetTime();
}

bool uiIsConfigModalOpen() {
    return s_modalOpen;
}

void uiOpenConfigModal(UiModalTab initialTab) {
    s_modalOpen = true;
    s_currentTab = initialTab;
    s_selectedOption = 0;
    s_cheatScroll = 0;
    s_modalMessage[0] = '\0';
}

static int getNumOptionsForTab(UiModalTab tab) {
    if(tab == UI_TAB_GAME) return 6;
    if(tab == UI_TAB_VIDEO) return 6;
    if(tab == UI_TAB_SYSTEM) return 6;
    if(tab == UI_TAB_CHEATS) {
        return (cheatEngine != nullptr) ? cheatEngine->getNumCheats() : 0;
    }
    return 0;
}

static bool triggerModalOption(int opt, int dir, bool isTouch) {
    (void) isTouch;
    if(s_currentTab == UI_TAB_GAME) {
        if(opt == 0) {
            // Resume Game
            configSave();
            s_modalOpen = false;
            menuOn = false;
            if(!isFileChooserActive()) {
                mgrUnpause();
                inputReleaseAll();
            }
            return false;
        } else if(opt == 1) {
            // State Slot
            stateNum = (dir < 0) ? ((stateNum + 9) % 10) : ((stateNum + 1) % 10);
            setMenuOption("State Slot", stateNum);
            return true;
        } else if(opt == 2) {
            // Save State
            bool ok = mgrSaveState(stateNum);
            setModalMessage(ok ? "State Saved!" : "Save Failed!", ok ? UI_COLOR_GREEN : UI_COLOR_RGBA(255, 90, 90, 255));
            return true;
        } else if(opt == 3) {
            // Load State
            if(mgrStateExists(stateNum)) {
                bool ok = mgrLoadState(stateNum);
                if(ok) {
                    setModalMessage("State Loaded!", UI_COLOR_GREEN);
                    configSave();
                    s_modalOpen = false;
                    menuOn = false;
                    if(!isFileChooserActive()) {
                        mgrUnpause();
                        inputReleaseAll();
                    }
                    return false;
                } else {
                    setModalMessage("Load Failed!", UI_COLOR_RGBA(255, 90, 90, 255));
                }
            } else {
                setModalMessage("Slot is Empty!", UI_COLOR_YELLOW);
            }
            return true;
        } else if(opt == 4) {
            // Delete State
            if(mgrStateExists(stateNum)) {
                mgrDeleteState(stateNum);
                setModalMessage("State Deleted!", UI_COLOR_PURPLE);
            } else {
                setModalMessage("Slot is Empty!", UI_COLOR_YELLOW);
            }
            return true;
        } else if(opt == 5) {
            // Suspend & Exit
            if(gameboy != nullptr && gameboy->isPoweredOn()) {
                mgrSaveState(-1);
            }
            configSave();
            s_modalOpen = false;
            menuOn = false;
            if(!isFileChooserActive()) {
                mgrUnpause();
            }
            mgrSelectRom();
            return false;
        }
    } else if(s_currentTab == UI_TAB_VIDEO) {
        if(opt == 0) {
            // Scaling (0..4)
            int val = getMenuOption("Scaling");
            val = (dir < 0) ? ((val + 4) % 5) : ((val + 1) % 5);
            setMenuOption("Scaling", val);
            return true;
        } else if(opt == 1) {
            // Scale Filter (0..2)
            int val = getMenuOption("Scale Filter");
            val = (dir < 0) ? ((val + 2) % 3) : ((val + 1) % 3);
            setMenuOption("Scale Filter", val);
            return true;
        } else if(opt == 2) {
            // LCD Grid (0..1)
            int val = getMenuOption("LCD Grid") ^ 1;
            setMenuOption("LCD Grid", val);
            return true;
        } else if(opt == 3) {
            // LCD Ghosting (0..1)
            int val = getMenuOption("LCD Ghosting") ^ 1;
            setMenuOption("LCD Ghosting", val);
            return true;
        } else if(opt == 4) {
            // Game Screen (0..1)
            int val = getMenuOption("Game Screen") ^ 1;
            setMenuOption("Game Screen", val);
            return true;
        } else if(opt == 5) {
            // Custom Border (0..1)
            int val = getMenuOption("Custom Borders") ^ 1;
            setMenuOption("Custom Borders", val);
            mgrRefreshBorder();
            return true;
        }
    } else if(s_currentTab == UI_TAB_SYSTEM) {
        if(opt == 0) {
            // DMG Palette (0..13)
            int val = getMenuOption("Colorize GB");
            val = (dir < 0) ? ((val + 13) % 14) : ((val + 1) % 14);
            setMenuOption("Colorize GB", val);
            mgrRefreshPalette();
            return true;
        } else if(opt == 1) {
            // GBC Colors (0..1)
            int val = getMenuOption("CGB Colors") ^ 1;
            setMenuOption("CGB Colors", val);
            return true;
        } else if(opt == 2) {
            // Sound (0..1)
            int val = getMenuOption("Master") ^ 1;
            setMenuOption("Master", val);
            return true;
        } else if(opt == 3) {
            // Fast Forward (0..1)
            bool ff = !mgrGetFastForward();
            mgrSetFastForward(ff);
            return true;
        } else if(opt == 4) {
            // GBC Mode (0..2)
            int val = getMenuOption("GBC Mode");
            val = (dir < 0) ? ((val + 2) % 3) : ((val + 1) % 3);
            setMenuOption("GBC Mode", val);
            return true;
        } else if(opt == 5) {
            // SGB Mode (0..2)
            int val = getMenuOption("SGB Mode");
            val = (dir < 0) ? ((val + 2) % 3) : ((val + 1) % 3);
            setMenuOption("SGB Mode", val);
            return true;
        }
    } else if(s_currentTab == UI_TAB_CHEATS) {
        int numCheats = (cheatEngine != nullptr) ? cheatEngine->getNumCheats() : 0;
        if(numCheats > 0 && opt >= 0 && opt < numCheats) {
            bool enabled = !cheatEngine->isCheatEnabled(opt);
            cheatEngine->toggleCheat(opt, enabled);
            setModalMessage(enabled ? "Cheat Enabled" : "Cheat Disabled", enabled ? UI_COLOR_GREEN : UI_COLOR_GRAY);
        }
        return true;
    }

    return true;
}

bool uiUpdateConfigModal() {
    if(!s_modalOpen) {
        return false;
    }

    u32 kDown = hidKeysDown();

    // 1. Interacción táctil
    if(kDown & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);

        // Barra superior de pestañas (y: 4..24)
        if(touch.py >= 4 && touch.py <= 24) {
            if(touch.px >= 4 && touch.px <= 81) {
                s_currentTab = UI_TAB_GAME;
                s_selectedOption = 0;
                return true;
            } else if(touch.px >= 82 && touch.px <= 159) {
                s_currentTab = UI_TAB_VIDEO;
                s_selectedOption = 0;
                return true;
            } else if(touch.px >= 160 && touch.px <= 237) {
                s_currentTab = UI_TAB_SYSTEM;
                s_selectedOption = 0;
                return true;
            } else if(touch.px >= 238 && touch.px <= 315) {
                s_currentTab = UI_TAB_CHEATS;
                s_selectedOption = 0;
                return true;
            }
        }

        // Dock inferior (y: 216..236)
        if(touch.py >= 216 && touch.py <= 236) {
            if(touch.px >= 4 && touch.px <= 79) {
                // FILES
                configSave();
                s_modalOpen = false;
                menuOn = false;
                if(!isFileChooserActive()) {
                    mgrUnpause();
                }
                mgrSelectRom();
                return false;
            } else if(touch.px >= 83 && touch.px <= 158) {
                // CONFIG: Alternar entre VIDEO y GAME
                if(s_currentTab == UI_TAB_VIDEO || s_currentTab == UI_TAB_SYSTEM) {
                    s_currentTab = UI_TAB_GAME;
                } else {
                    s_currentTab = UI_TAB_VIDEO;
                }
                s_selectedOption = 0;
                return true;
            } else if(touch.px >= 162 && touch.px <= 237) {
                // CHEATS: Pestaña directa de trucos
                s_currentTab = UI_TAB_CHEATS;
                s_selectedOption = 0;
                return true;
            } else if(touch.px >= 241 && touch.px <= 316) {
                // RESET
                configSave();
                s_modalOpen = false;
                menuOn = false;
                if(!isFileChooserActive()) {
                    mgrUnpause();
                }
                mgrReset();
                return false;
            }
        }

        // Área central de opciones (y: 28..188)
        if(touch.py >= 28 && touch.py <= 188) {
            int row = (touch.py - 30) / 26;
            if(row >= 0 && row < 6) {
                if(s_currentTab == UI_TAB_CHEATS) {
                    int numCheats = (cheatEngine != nullptr) ? cheatEngine->getNumCheats() : 0;
                    int idx = s_cheatScroll + row;
                    if(idx >= 0 && idx < numCheats) {
                        s_selectedOption = idx;
                        return triggerModalOption(idx, 1, true);
                    }
                } else {
                    s_selectedOption = row;
                    int dir = (touch.px >= 170 && touch.px < 230) ? -1 : 1;
                    return triggerModalOption(row, dir, true);
                }
            }
        }
    }

    // 2. Botones de cambio de pestaña (L / R)
    if(kDown & KEY_L) {
        s_currentTab = (UiModalTab)((s_currentTab + 3) % 4);
        s_selectedOption = 0;
        return true;
    }
    if(kDown & KEY_R) {
        s_currentTab = (UiModalTab)((s_currentTab + 1) % 4);
        s_selectedOption = 0;
        return true;
    }

    // 3. Navegación vertical (Up / Down)
    int numOpts = getNumOptionsForTab(s_currentTab);
    if(kDown & (KEY_DUP | KEY_CPAD_UP)) {
        if(s_currentTab == UI_TAB_CHEATS) {
            if(numOpts > 0) {
                s_selectedOption = (s_selectedOption + numOpts - 1) % numOpts;
                if(s_selectedOption < s_cheatScroll) s_cheatScroll = s_selectedOption;
                if(s_selectedOption >= s_cheatScroll + 6) s_cheatScroll = s_selectedOption - 5;
            }
        } else if(numOpts > 0) {
            s_selectedOption = (s_selectedOption + numOpts - 1) % numOpts;
        }
        return true;
    }
    if(kDown & (KEY_DDOWN | KEY_CPAD_DOWN)) {
        if(s_currentTab == UI_TAB_CHEATS) {
            if(numOpts > 0) {
                s_selectedOption = (s_selectedOption + 1) % numOpts;
                if(s_selectedOption >= s_cheatScroll + 6) s_cheatScroll = s_selectedOption - 5;
                if(s_selectedOption < s_cheatScroll) s_cheatScroll = s_selectedOption;
            }
        } else if(numOpts > 0) {
            s_selectedOption = (s_selectedOption + 1) % numOpts;
        }
        return true;
    }

    // 4. Modificación de valor / Ejecución (Left / Right / A)
    if(kDown & (KEY_DLEFT | KEY_CPAD_LEFT)) {
        if(numOpts > 0) {
            return triggerModalOption(s_selectedOption, -1, false);
        }
    }
    if(kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) {
        if(numOpts > 0) {
            return triggerModalOption(s_selectedOption, 1, false);
        }
    }
    if(kDown & KEY_A) {
        if(numOpts > 0) {
            return triggerModalOption(s_selectedOption, 1, false);
        }
    }

    // 5. Cierre y guardado (B / START)
    if(kDown & (KEY_B | KEY_START)) {
        configSave();
        s_modalOpen = false;
        menuOn = false;
        if(!isFileChooserActive()) {
            mgrUnpause();
            inputReleaseAll();
        }
        return false;
    }

    return true;
}

void uiDrawConfigModal() {
    const u32 colBg = uiColorRGBA(24, 26, 32, 255);
    const u32 colPanelBg = uiColorRGBA(18, 20, 26, 255);
    const u32 colBorder = uiColorRGBA(64, 80, 104, 255);
    const u32 colSelectedBg = uiColorRGBA(36, 54, 84, 255);
    const u32 colRowBg = uiColorRGBA(24, 28, 38, 255);
    const u32 colRowBorder = uiColorRGBA(40, 50, 68, 255);
    const u32 colTabActiveBg = uiColorRGBA(48, 64, 96, 255);
    const u32 colTabInactiveBg = uiColorRGBA(20, 24, 32, 255);
    const u32 colDockBtnBg = uiColorRGBA(34, 40, 52, 255);
    const u32 colDockBtnActiveBg = uiColorRGBA(48, 64, 96, 255);

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colGray = UI_COLOR_GRAY;

    // 1. Fondo oscuro
    uiDrawRect(0.0f, 0.0f, 320.0f, 240.0f, colBg);

    // 2. Barra de pestañas superior (y: 4..24)
    const float tabW = 77.0f;
    const float tabH = 20.0f;
    const float tabY = 4.0f;
    const float tabX[4] = { 4.0f, 82.0f, 160.0f, 238.0f };
    const char* tabNames[4] = { "GAME", "VIDEO", "SYSTEM", "CHEATS" };
    const float tabTextX[4] = { 26.0f, 100.0f, 172.0f, 250.0f };

    for(int t = 0; t < 4; t++) {
        bool active = (s_currentTab == (UiModalTab)t);
        u32 bg = active ? colTabActiveBg : colTabInactiveBg;
        u32 border = active ? colYellow : colBorder;
        u32 textCol = active ? colYellow : colGray;

        uiDrawRect(tabX[t], tabY, tabW, tabH, bg);
        uiDrawRectOutline(tabX[t], tabY, tabW, tabH, 1.0f, border);
        uiDrawText(tabTextX[t], tabY + 6.0f, tabNames[t], textCol);
    }

    // 3. Panel contenedor central (x: 4..316, y: 26..212)
    uiDrawRect(4.0f, 26.0f, 312.0f, 186.0f, colPanelBg);
    uiDrawRectOutline(4.0f, 26.0f, 312.0f, 186.0f, 1.0f, colBorder);

    // 4. Filas de opciones
    if(s_currentTab == UI_TAB_CHEATS) {
        int numCheats = (cheatEngine != nullptr) ? cheatEngine->getNumCheats() : 0;
        if(numCheats == 0) {
            uiDrawText(72.0f, 80.0f, "No cheats found (.cht)", colYellow);
            uiDrawText(36.0f, 104.0f, "Place .cht file in SD root or ROM dir", colGray);
        } else {
            for(int r = 0; r < 6 && (s_cheatScroll + r) < numCheats; r++) {
                int idx = s_cheatScroll + r;
                float rowY = 30.0f + r * 26.0f;
                bool selected = (idx == s_selectedOption);

                if(selected) {
                    uiDrawRect(9.0f, rowY, 302.0f, 24.0f, colSelectedBg);
                    uiDrawRectOutline(9.0f, rowY, 302.0f, 24.0f, 1.0f, colYellow);
                } else {
                    uiDrawRect(9.0f, rowY, 302.0f, 24.0f, colRowBg);
                    uiDrawRectOutline(9.0f, rowY, 302.0f, 24.0f, 1.0f, colRowBorder);
                }

                bool enabled = cheatEngine->isCheatEnabled(idx);
                std::string name = cheatEngine->getCheatName(idx);
                if(name.length() > 28) name = name.substr(0, 27) + "~";
                char line[36];
                snprintf(line, sizeof(line), "[%c] %s", enabled ? 'X' : ' ', name.c_str());
                uiDrawText(18.0f, rowY + 8.0f, line, enabled ? UI_COLOR_GREEN : (selected ? colWhite : colGray));
            }

            if(s_cheatScroll > 0) {
                uiDrawText(296.0f, 28.0f, "^", colYellow);
            }
            if(s_cheatScroll + 6 < numCheats) {
                uiDrawText(296.0f, 184.0f, "v", colYellow);
            }
        }
    } else {
        const char* labels[6];
        char values[6][32];

        if(s_currentTab == UI_TAB_GAME) {
            labels[0] = "Resume Game";
            snprintf(values[0], sizeof(values[0]), "[Play]");

            labels[1] = "State Slot";
            snprintf(values[1], sizeof(values[1]), "< %d > %s", stateNum, mgrStateExists(stateNum) ? "[Saved]" : "[Empty]");

            labels[2] = "Save State";
            snprintf(values[2], sizeof(values[2]), mgrStateExists(stateNum) ? "[Overwrite]" : "[Save]");

            labels[3] = "Load State";
            snprintf(values[3], sizeof(values[3]), mgrStateExists(stateNum) ? "[Ready]" : "[Empty]");

            labels[4] = "Delete State";
            snprintf(values[4], sizeof(values[4]), mgrStateExists(stateNum) ? "[Delete]" : "[None]");

            labels[5] = "Suspend & Exit";
            snprintf(values[5], sizeof(values[5]), "[Exit]");
        } else if(s_currentTab == UI_TAB_VIDEO) {
            labels[0] = "Scaling";
            static const char* scaleNames[5] = {"Off", "125%", "150%", "Aspect", "Full"};
            snprintf(values[0], sizeof(values[0]), "< %s >", scaleNames[getMenuOption("Scaling") % 5]);

            labels[1] = "Scale Filter";
            static const char* filterNames[3] = {"Nearest", "Linear", "Sharp 2x"};
            snprintf(values[1], sizeof(values[1]), "< %s >", filterNames[getMenuOption("Scale Filter") % 3]);

            labels[2] = "LCD Grid";
            snprintf(values[2], sizeof(values[2]), getMenuOption("LCD Grid") ? "< On >" : "< Off >");

            labels[3] = "LCD Ghosting";
            snprintf(values[3], sizeof(values[3]), getMenuOption("LCD Ghosting") ? "< On >" : "< Off >");

            labels[4] = "Game Screen";
            snprintf(values[4], sizeof(values[4]), getMenuOption("Game Screen") ? "< Bottom >" : "< Top >");

            labels[5] = "Custom Border";
            snprintf(values[5], sizeof(values[5]), getMenuOption("Custom Borders") ? "< On >" : "< Off >");
        } else { // UI_TAB_SYSTEM
            labels[0] = "DMG Palette";
            static const char* palNames[14] = {"Off", "Auto", "Invert", "Pastel", "Red", "Orange", "Yellow", "Green", "Blue", "Brown", "DkGreen", "DkBlue", "DkBrown", "Classic"};
            snprintf(values[0], sizeof(values[0]), "< %s >", palNames[getMenuOption("Colorize GB") % 14]);

            labels[1] = "GBC Colors";
            snprintf(values[1], sizeof(values[1]), getMenuOption("CGB Colors") ? "< Corrected >" : "< Raw >");

            labels[2] = "Sound";
            snprintf(values[2], sizeof(values[2]), getMenuOption("Master") ? "< On >" : "< Off >");

            labels[3] = "Fast Forward";
            snprintf(values[3], sizeof(values[3]), mgrGetFastForward() ? "< On >" : "< Off >");

            labels[4] = "GBC Mode";
            static const char* gbcNames[3] = {"Off", "If Needed", "On"};
            snprintf(values[4], sizeof(values[4]), "< %s >", gbcNames[getMenuOption("GBC Mode") % 3]);

            labels[5] = "SGB Mode";
            static const char* sgbNames[3] = {"Off", "Pref GBC", "Pref SGB"};
            snprintf(values[5], sizeof(values[5]), "< %s >", sgbNames[getMenuOption("SGB Mode") % 3]);
        }

        for(int r = 0; r < 6; r++) {
            float rowY = 30.0f + r * 26.0f;
            bool selected = (r == s_selectedOption);

            if(selected) {
                uiDrawRect(9.0f, rowY, 302.0f, 24.0f, colSelectedBg);
                uiDrawRectOutline(9.0f, rowY, 302.0f, 24.0f, 1.0f, colYellow);
            } else {
                uiDrawRect(9.0f, rowY, 302.0f, 24.0f, colRowBg);
                uiDrawRectOutline(9.0f, rowY, 302.0f, 24.0f, 1.0f, colRowBorder);
            }

            uiDrawText(18.0f, rowY + 8.0f, labels[r], selected ? colWhite : colGray);

            u32 valCol = UI_COLOR_CYAN;
            if(s_currentTab == UI_TAB_GAME) {
                if(r == 1) valCol = mgrStateExists(stateNum) ? UI_COLOR_GREEN : colGray;
                else if(r == 0) valCol = UI_COLOR_GREEN;
                else if(r == 5) valCol = colYellow;
                else if(r == 4) valCol = mgrStateExists(stateNum) ? UI_COLOR_PURPLE : UI_COLOR_DARKGRAY;
                else if(!mgrStateExists(stateNum) && (r == 3)) valCol = UI_COLOR_DARKGRAY;
            }
            uiDrawText(176.0f, rowY + 8.0f, values[r], valCol);
        }
    }

    // 5. Mensaje de estado / Ayuda en parte inferior del contenedor
    if(s_modalMessage[0] != '\0' && (osGetTime() - s_modalMessageTime < 1800)) {
        uiDrawText(18.0f, 192.0f, s_modalMessage, s_modalMessageColor);
    } else {
        uiDrawText(18.0f, 192.0f, "L/R:Tab  Pad:Navigate  A/Touch:Select  B:Resume", UI_COLOR_DARKGRAY);
    }

    // 6. Dock inferior (y: 216..236)
    const float btnW = 75.0f;
    const float btnH = 20.0f;
    const float btnY = 216.0f;
    const float btnX[4] = { 4.0f, 83.0f, 162.0f, 241.0f };
    const char* btnLabels[4] = { "FILES", "CONFIG", "CHEATS", "RESET" };
    const float labelOffsetsX[4] = { 21.0f, 98.0f, 177.0f, 261.0f };

    for(int i = 0; i < 4; i++) {
        bool active = false;
        if(i == 1 && s_currentTab != UI_TAB_CHEATS) active = true;
        if(i == 2 && s_currentTab == UI_TAB_CHEATS) active = true;

        u32 bg = active ? colDockBtnActiveBg : colDockBtnBg;
        u32 border = active ? colYellow : colBorder;
        u32 textCol = active ? colYellow : colWhite;

        uiDrawRect(btnX[i], btnY, btnW, btnH, bg);
        uiDrawRectOutline(btnX[i], btnY, btnW, btnH, 1.0f, border);
        uiDrawText(labelOffsetsX[i], btnY + 6.0f, btnLabels[i], textCol);
    }

    // Reset TexEnv para dejar el pipeline limpio
    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
}

void uiDrawWireframeTest() {
    uiDrawConfigModal();
}

bool uiHandleRightPanelTouch(int px, int py, char** result, FileChooser* chooser) {
    if(s_brMode == BR_MODE_PREVIEW) {
        // Selector de Slots: ◄ en (192..220, 148..172), ► en (250..278, 148..172)
        if(py >= 148 && py <= 172) {
            if(px >= 192 && px <= 220) {
                uiCyclePreviewSlot(-1);
                return false;
            } else if(px >= 250 && px <= 278) {
                uiCyclePreviewSlot(1);
                return false;
            }
        }
        // Botón [▶ Iniciar ROM] (y: 180..204, x: 190..308) o tap en carátula (y: 44..144, x: 198..300)
        if((py >= 180 && py <= 204 && px >= 190 && px <= 308) ||
           (py >= 44 && py <= 144 && px >= 198 && px <= 300)) {
            if(chooser != nullptr && chooser->getSelection() >= 0) {
                if(chooser->navigateToSelection(result)) {
                    stateNum = uiGetPreviewSlot();
                    return true;
                }
            }
        }
    } else if(s_brMode == BR_MODE_DISPLAY) {
        // Fila 0: Escalado (y: 45..67)
        if(py >= 45 && py <= 67) {
            int val = getMenuOption("Scaling");
            if(px < 265) {
                val = (val + 4) % 5;
            } else {
                val = (val + 1) % 5;
            }
            setMenuOption("Scaling", val);
            return false;
        }
        // Fila 1: Filtro (y: 69..91)
        if(py >= 69 && py <= 91) {
            int val = getMenuOption("Scale Filter");
            if(px < 265) {
                val = (val + 2) % 3;
            } else {
                val = (val + 1) % 3;
            }
            setMenuOption("Scale Filter", val);
            return false;
        }
        // Fila 2: Rejilla LCD (y: 93..115)
        if(py >= 93 && py <= 115) {
            int val = getMenuOption("LCD Grid") ^ 1;
            setMenuOption("LCD Grid", val);
            return false;
        }
        // Toque en tarjeta informativa (y: 122..198): conmutar filtro de escalado
        if(py >= 122 && py <= 198) {
            int val = getMenuOption("Scale Filter");
            val = (val + 1) % 3;
            setMenuOption("Scale Filter", val);
            return false;
        }
    } else if(s_brMode == BR_MODE_PALETTE) {
        // Selector táctil de paleta (y: 44..68)
        if(py >= 44 && py <= 68) {
            if(px < 240) {
                gbColorizeMode = (gbColorizeMode + 13) % 14;
            } else {
                gbColorizeMode = (gbColorizeMode + 1) % 14;
            }
            mgrRefreshPalette();
            return false;
        }
        // Botón [Siguiente Paleta] o tap en swatches (y: 70..200)
        if(py >= 70 && py <= 200) {
            mgrCyclePalette();
            return false;
        }
    }
    return false;
}

void uiDrawFileChooser(const FileChooser* chooser) {
    if(chooser == nullptr) {
        uiDrawWireframeTest();
        return;
    }

    const u32 colBg = uiColorRGBA(13, 17, 24, 255);
    const u32 colPanelBg = uiColorRGBA(16, 21, 30, 255);
    const u32 colBorder = uiColorRGBA(40, 60, 90, 255);
    const u32 colListBorder = uiColorRGBA(36, 70, 110, 255);
    const u32 colAmber = UI_COLOR_AMBER;

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colCyan = UI_COLOR_CYAN;
    const u32 colGray = UI_COLOR_GRAY;
    const u32 colDarkGray = UI_COLOR_DARKGRAY;
    const u32 colGreen = UI_COLOR_GREEN;

    // 1. Fondo general de la pantalla inferior (320x240)
    uiDrawRect(0.0f, 0.0f, 320.0f, 240.0f, colBg);

    // Header Superior (y: 8..22)
    std::string dirTitle = chooser->getDirectory();
    uiDrawTextScaled(12.0f, 8.0f, 0.75f, "ROMS: ", colAmber);
    float romsW = uiGetTextWidth("ROMS: ", 0.75f);
    uiDrawTextScaled(12.0f + romsW, 8.0f, 0.72f, dirTitle.c_str(), colWhite);
    uiDrawTextScaled(272.0f, 8.0f, 0.72f, "v1.0.9", colGray);
    uiDrawRect(10.0f, 22.0f, 300.0f, 1.0f, colBorder);

    // 2. Lista de ROMs (Panel Izquierdo, x: 10..186, y: 24..202)
    uiDrawDropShadow(10.0f, 24.0f, 176.0f, 178.0f, 3.0f, 80);
    uiDrawRoundedRect(10.0f, 24.0f, 176.0f, 178.0f, 2.0f, colPanelBg, colListBorder, 1.0f);

    const std::vector<std::string>& files = chooser->getFilenames();
    const std::vector<int>& flags = chooser->getFlags();
    int numFiles = chooser->getNumFiles();
    int selection = chooser->getSelection();
    float scrollY = chooser->getSubpixelScrollY();

    int firstIdx = (int)(scrollY / 24.0f);
    if(firstIdx < 0) firstIdx = 0;

    for(int i = firstIdx; i < numFiles; i++) {
        float itemY = 25.0f + (float)i * 24.0f - scrollY;
        if(itemY < 1.0f) continue;
        if(itemY > 202.0f) break;

        bool isSelected = (i == selection);
        bool isDir = (i < (int)flags.size()) && ((flags[i] & 1) != 0);
        std::string rawName = (i < (int)files.size()) ? files[i] : "";

        if(isSelected) {
            uiDrawRoundedRect(11.0f, itemY, 161.0f, 23.0f, 2.0f, uiColorRGBA(185, 138, 36, 255), colAmber, 1.0f);
            uiDrawTextScaled(13.0f, itemY + 4.0f, 0.70f, ">", colWhite);
            uiDrawTextScaled(161.0f, itemY + 4.0f, 0.70f, "<", colWhite);
        }

        if(isDir) {
            uiDrawFolderIcon(20.0f, itemY + 5.0f);
            uiDrawTextScaled(36.0f, itemY + 4.0f, 0.70f, rawName.c_str(), isSelected ? colWhite : colYellow);
        } else {
            auto dot = rawName.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? rawName.substr(dot + 1) : "";
            bool isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

            uiDrawCartridgeIcon(20.0f, itemY + 5.0f, isGBC);

            std::string label = (isGBC ? "[CGB] " : "[DMG] ") + rawName;
            uiDrawTextScaled(36.0f, itemY + 4.0f, 0.70f, label.c_str(), colWhite);
        }
    }

    if(numFiles == 0) {
        uiDrawTextScaled(20.0f, 40.0f, 0.75f, "Sin archivos", colDarkGray);
    } else {
        // Scrollbar vertical con botones ▲ y ▼
        uiDrawRect(174.0f, 25.0f, 10.0f, 11.0f, uiColorRGBA(25, 35, 50, 255));
        uiDrawRectOutline(174.0f, 25.0f, 10.0f, 11.0f, 1.0f, uiColorRGBA(50, 70, 95, 255));
        uiDrawTextScaled(175.0f, 23.0f, 0.60f, "▲", colWhite);

        uiDrawRect(174.0f, 190.0f, 10.0f, 11.0f, uiColorRGBA(25, 35, 50, 255));
        uiDrawRectOutline(174.0f, 190.0f, 10.0f, 11.0f, 1.0f, uiColorRGBA(50, 70, 95, 255));
        uiDrawTextScaled(175.0f, 188.0f, 0.60f, "▼", colWhite);

        float maxScroll = chooser->getMaxScroll();
        if(maxScroll > 0.0f) {
            const float trackY = 38.0f;
            const float trackH = 150.0f;
            float totalH = (float)numFiles * 24.0f;
            float thumbH = trackH * (168.0f / totalH);
            if(thumbH < 12.0f) thumbH = 12.0f;
            if(thumbH > trackH) thumbH = trackH;
            float thumbY = trackY + (scrollY / maxScroll) * (trackH - thumbH);
            if(thumbY < trackY) thumbY = trackY;
            if(thumbY + thumbH > trackY + trackH) thumbY = trackY + trackH - thumbH;
            uiDrawRoundedRect(175.0f, thumbY, 8.0f, thumbH, 2.0f, uiColorRGBA(70, 115, 175, 255), uiColorRGBA(100, 160, 240, 255), 1.0f);
        }
    }

    // Máscara superior e inferior para recortar listas desbordadas
    uiDrawRect(0.0f, 0.0f, 320.0f, 23.0f, colBg);
    uiDrawTextScaled(12.0f, 8.0f, 0.75f, "ROMS: ", colAmber);
    uiDrawTextScaled(12.0f + romsW, 8.0f, 0.72f, dirTitle.c_str(), colWhite);
    uiDrawTextScaled(272.0f, 8.0f, 0.72f, "v1.0.9", colGray);
    uiDrawRect(10.0f, 22.0f, 300.0f, 1.0f, colBorder);

    uiDrawRect(0.0f, 203.0f, 320.0f, 5.0f, colBg);

    // 3. Panel Derecho Multi-Modo Contextual (x: 192..310, y: 24..202)
    uiDrawDropShadow(192.0f, 24.0f, 118.0f, 178.0f, 3.0f, 80);
    uiDrawRoundedRect(192.0f, 24.0f, 118.0f, 178.0f, 2.0f, uiColorRGBA(14, 18, 26, 255), colAmber, 1.0f);

    std::string currentSelectedRom = "";
    std::string baseSelectedName = "";
    bool isCurrentDir = false;
    if(selection >= 0 && selection < numFiles && selection < (int)files.size() && selection < (int)flags.size()) {
        isCurrentDir = (flags[selection] & 1) != 0;
        if(!isCurrentDir) {
            std::string dir = chooser->getDirectory();
            if(!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
                dir += "/";
            }
            currentSelectedRom = dir + files[selection];
            baseSelectedName = files[selection];
            auto dot = baseSelectedName.find_last_of('.');
            if(dot != std::string::npos) {
                baseSelectedName = baseSelectedName.substr(0, dot);
            }
        }
    }

    bool hasState = false;
    bool hasSav = false;
    struct stat stState;
    struct stat stSav;
    if(!currentSelectedRom.empty() && !isCurrentDir) {
        std::string stateFile = currentSelectedRom + ".ys" + std::to_string(s_previewSlot);
        if(stat(stateFile.c_str(), &stState) == 0 && stState.st_size > 0) {
            hasState = true;
        }
        std::string savFile = currentSelectedRom + ".sav";
        if(stat(savFile.c_str(), &stSav) == 0 && stSav.st_size > 0) {
            hasSav = true;
        }
    }

    if(s_brMode == BR_MODE_PREVIEW) {
        // MODO 0: VISTA PREVIA / CAPTURA DE GUARDADO
        if(hasState) {
            uiDrawTextScaled(196.0f, 26.0f, 0.72f, "CAPTURA GUARDADO", colYellow);
        } else {
            uiDrawTextScaled(210.0f, 26.0f, 0.75f, "VISTA PREVIA", colAmber);
        }
        uiDrawRect(192.0f, 40.0f, 118.0f, 1.0f, colAmber);

        // Recuadro del visor (88x88 px en x=207, y=48)
        uiDrawDropShadow(207.0f, 48.0f, 88.0f, 88.0f, 2.0f, 70);

        if(s_boxartLoaded && s_boxartTexAllocated) {
            C3D_TexBind(0, &s_boxartTex);
            C3D_TexEnv* env0 = C3D_GetTexEnv(0);
            C3D_TexEnvInit(env0);
            C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
            C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
            C3D_TexEnvInit(C3D_GetTexEnv(1));
            C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
            C3D_CullFace(GPU_CULL_NONE);
            C3D_AlphaTest(false, GPU_ALWAYS, 0);

            C3D_ImmDrawBegin(GPU_TRIANGLES);
            emitUiQuad(207.0f, 48.0f, 295.0f, 136.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            C3D_ImmDrawEnd();

            uiDrawRectOutline(207.0f, 48.0f, 88.0f, 88.0f, 2.0f, colAmber);
        } else if(!currentSelectedRom.empty()) {
            bool isGBC = (currentSelectedRom.find(".gbc") != std::string::npos || currentSelectedRom.find(".cgb") != std::string::npos);
            u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
            uiDrawRect(221.0f, 56.0f, 60.0f, 72.0f, colCart);
            uiDrawRectOutline(221.0f, 56.0f, 60.0f, 72.0f, 2.0f, colAmber);
            uiDrawTextScaled(233.0f, 82.0f, 0.75f, isGBC ? "CGB" : "DMG", colWhite);
        } else {
            uiDrawTextScaled(208.0f, 84.0f, 0.72f, "Sin Archivo", colGray);
        }

        // Título limpio centrado debajo del visor (y=146)
        std::string cleanName = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : baseSelectedName;
        if(!cleanName.empty()) {
            float nameW = uiGetTextWidth(cleanName.c_str(), 0.70f);
            float nx = (nameW < 114.0f) ? (192.0f + (118.0f - nameW) * 0.5f) : 194.0f;
            uiDrawTextScaled(nx, 146.0f, 0.70f, cleanName.c_str(), colAmber);
        }

        // Publisher / Developer (y=162)
        std::string pubStr = s_metaLoaded ? (!s_currentMeta.developer.empty() ? s_currentMeta.developer : s_currentMeta.publisher) : "Nintendo / Game Freak";
        float pw = uiGetTextWidth(pubStr.c_str(), 0.65f);
        float px = (pw < 114.0f) ? (192.0f + (118.0f - pw) * 0.5f) : 194.0f;
        uiDrawTextScaled(px, 162.0f, 0.65f, pubStr.c_str(), colGray);

        // Selector "< Slot 0 >" interactivo (y: 180)
        char slotBuf[24];
        snprintf(slotBuf, sizeof(slotBuf), "< Slot %d >", s_previewSlot);
        uiDrawTextScaled(196.0f, 180.0f, 0.70f, slotBuf, colWhite);
        uiDrawPillBadge(272.0f, 178.0f, 34.0f, 14.0f,
                        hasState ? "SAV" : (hasSav ? "SRAM" : "---"),
                        hasState ? uiColorRGBA(20, 100, 40, 255) : (hasSav ? uiColorRGBA(15, 80, 100, 255) : uiColorRGBA(30, 35, 45, 255)),
                        hasState ? colWhite : (hasSav ? colCyan : colDarkGray));
    } else if(s_brMode == BR_MODE_DISPLAY) {
        // MODO 1: AJUSTES DE DISPLAY
        uiDrawTextScaled(194.0f, 26.0f, 0.72f, "⚙ AJUSTES DISPLAY", colAmber);
        uiDrawRect(192.0f, 40.0f, 118.0f, 1.0f, colAmber);

        // Fila 0: Escalado (y: 46)
        uiDrawTextScaled(194.0f, 46.0f, 0.70f, "• Escalado:", colWhite);
        static const char* scaleNames[5] = {"Off", "125%", "150%", "Aspect", "Full"};
        char v0[24];
        snprintf(v0, sizeof(v0), "◄ %s ►", scaleNames[getMenuOption("Scaling") % 5]);
        float w0 = uiGetTextWidth(v0, 0.68f);
        uiDrawTextScaled(306.0f - w0, 46.0f, 0.68f, v0, colAmber);

        // Fila 1: Filtro (y: 70)
        uiDrawTextScaled(194.0f, 70.0f, 0.70f, "• Filtro:", colWhite);
        static const char* filterNames[3] = {"Nearest", "Linear", "Sharp 2x"};
        char v1[24];
        snprintf(v1, sizeof(v1), "◄ %s ►", filterNames[getMenuOption("Scale Filter") % 3]);
        float w1 = uiGetTextWidth(v1, 0.68f);
        uiDrawTextScaled(306.0f - w1, 70.0f, 0.68f, v1, colAmber);

        // Fila 2: Rejilla LCD (y: 94)
        uiDrawTextScaled(194.0f, 94.0f, 0.70f, "• Rejilla LCD:", colWhite);
        const char* v2 = getMenuOption("LCD Grid") ? "◄  On  ►" : "◄  Off  ►";
        float w2 = uiGetTextWidth(v2, 0.68f);
        uiDrawTextScaled(306.0f - w2, 94.0f, 0.68f, v2, getMenuOption("LCD Grid") ? colGreen : colDarkGray);

        // Tarjeta informativa inferior (y: 122..196)
        uiDrawRoundedRect(194.0f, 122.0f, 114.0f, 74.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        int curFilt = getMenuOption("Scale Filter") % 3;
        if(curFilt == 2) {
            uiDrawTextScaled(198.0f, 126.0f, 0.68f, "ℹ SHARP BILINEAR", colAmber);
            uiDrawTextScaled(198.0f, 140.0f, 0.64f, "RTT 2x escalado", colWhite);
            uiDrawTextScaled(198.0f, 154.0f, 0.64f, "Pixeles definidos", colGray);
            uiDrawTextScaled(198.0f, 168.0f, 0.64f, "Sin shimmer ni blur", colGray);
            uiDrawTextScaled(198.0f, 182.0f, 0.64f, "Recomendado 3DS", colGreen);
        } else if(curFilt == 1) {
            uiDrawTextScaled(198.0f, 126.0f, 0.68f, "ℹ FILTRO LINEAL", colCyan);
            uiDrawTextScaled(198.0f, 140.0f, 0.64f, "Suavizado PICA200", colWhite);
            uiDrawTextScaled(198.0f, 154.0f, 0.64f, "Transicion suave", colGray);
            uiDrawTextScaled(198.0f, 168.0f, 0.64f, "Escala continua", colGray);
            uiDrawTextScaled(198.0f, 182.0f, 0.64f, "Aspecto uniforme", colGray);
        } else {
            uiDrawTextScaled(198.0f, 126.0f, 0.68f, "ℹ NEAREST NEIGHBOR", colYellow);
            uiDrawTextScaled(198.0f, 140.0f, 0.64f, "Muestreo directo", colWhite);
            uiDrawTextScaled(198.0f, 154.0f, 0.64f, "Pixel art puro", colGray);
            uiDrawTextScaled(198.0f, 168.0f, 0.64f, "Sin interpolacion", colGray);
            uiDrawTextScaled(198.0f, 182.0f, 0.64f, "Escalado no entero", colGray);
        }
    } else if(s_brMode == BR_MODE_PALETTE) {
        // MODO 2: PALETAS DMG
        uiDrawTextScaled(198.0f, 26.0f, 0.72f, "🎨 PALETAS DMG", colAmber);
        uiDrawRect(192.0f, 40.0f, 118.0f, 1.0f, colAmber);

        // Selector táctil de paleta (y: 45)
        char palBuf[32];
        snprintf(palBuf, sizeof(palBuf), "◄ %s ►", mgrGetPaletteName(gbColorizeMode));
        float pw = uiGetTextWidth(palBuf, 0.70f);
        float px = (pw < 114.0f) ? (192.0f + (118.0f - pw) * 0.5f) : 194.0f;
        uiDrawTextScaled(px, 45.0f, 0.70f, palBuf, colYellow);

        // Recuadro de los 4 colores de muestra (y: 65..115)
        uiDrawRoundedRect(194.0f, 65.0f, 114.0f, 50.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        const u32* palCols = mgrGetPaletteColors(gbColorizeMode);
        for(int c = 0; c < 4; c++) {
            float sx = 198.0f + (float)c * 26.0f;
            uiDrawRect(sx, 72.0f, 23.0f, 26.0f, palCols[c]);
            uiDrawRectOutline(sx, 72.0f, 23.0f, 26.0f, 1.0f, colWhite);
        }
        uiDrawTextScaled(200.0f, 102.0f, 0.60f, "4 tonos oficiales DMG", colGray);

        // Ficha descriptiva (y: 120..168)
        uiDrawRoundedRect(194.0f, 120.0f, 114.0f, 48.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        uiDrawTextScaled(198.0f, 124.0f, 0.65f, "Modo monocromo", colWhite);
        uiDrawTextScaled(198.0f, 138.0f, 0.62f, "Aplica a juegos DMG", colGray);
        uiDrawTextScaled(198.0f, 152.0f, 0.62f, "Colorizado en vivo", colAmber);

        // Botón [Siguiente Paleta] (y: 174..196)
        uiDrawModernButton(194.0f, 174.0f, 114.0f, 22.0f, "Siguiente Paleta", uiColorRGBA(45, 30, 15, 255), colAmber, false);
    }

    // 4. Dock Inferior Temático de 4 Botones (y: 208..234)
    // Botón 1 (x: 8..80): [Display] Cian (RGB: 26, 148, 175)
    bool dispActive = (s_brMode == BR_MODE_DISPLAY);
    uiDrawModernButton(8.0f, 208.0f, 72.0f, 26.0f, "[ Display ]",
                       dispActive ? uiColorRGBA(26, 148, 175, 255) : uiColorRGBA(18, 95, 115, 255),
                       colCyan, dispActive);

    // Botón 2 (x: 84..156): [Paleta] Naranja (RGB: 215, 110, 32)
    bool palActive = (s_brMode == BR_MODE_PALETTE);
    uiDrawModernButton(84.0f, 208.0f, 72.0f, 26.0f, "[ Paleta ]",
                       palActive ? uiColorRGBA(215, 110, 32, 255) : uiColorRGBA(145, 75, 22, 255),
                       colAmber, palActive);

    // Botón 3 (x: 160..236): [Carpeta] Azul (RGB: 38, 98, 215)
    uiDrawModernButton(160.0f, 208.0f, 76.0f, 26.0f, "[ Carpeta ]",
                       uiColorRGBA(38, 98, 215, 255), uiColorRGBA(100, 160, 255, 255), false);

    // Botón 4 (x: 240..312): [X Salir] Rojo (RGB: 205, 40, 45)
    uiDrawModernButton(240.0f, 208.0f, 72.0f, 26.0f, "[ X Salir ]",
                       uiColorRGBA(205, 40, 45, 255), uiColorRGBA(255, 80, 80, 255), false);

    // Reset TexEnv para dejar el pipeline limpio
    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
}

void uiDrawTopScreenHub(float eyeOffset) {
    const u32 colBg = uiColorRGBA(13, 17, 24, 255);
    const u32 colAmber = UI_COLOR_AMBER;
    const u32 colPanelBlue = uiColorRGBA(20, 28, 44, 255);

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colGray = UI_COLOR_GRAY;
    const u32 colCyan = UI_COLOR_CYAN;
    const u32 colGreen = UI_COLOR_GREEN;

    // 1. Fondo Azul Obsidiana muy oscuro (400x240)
    uiDrawRect(0.0f, 0.0f, 400.0f, 240.0f, colBg);

    // Obtener selección actual desde FileChooser (solo lectura, sin llamadas a fetch ni a updateBoxart)
    FileChooser* chooser = getActiveFileChooser();
    bool isDir = false;
    std::string selectedFile = "";
    if(chooser != nullptr) {
        const std::vector<std::string>& files = chooser->getFilenames();
        const std::vector<int>& flags = chooser->getFlags();
        int numFiles = chooser->getNumFiles();
        int selection = chooser->getSelection();

        if(selection >= 0 && selection < numFiles && selection < (int)files.size() && selection < (int)flags.size()) {
            isDir = (flags[selection] & 1) != 0;
            selectedFile = files[selection];
        }
    }

    std::string fullHeader = "";
    if(isDir) {
        fullHeader = "Carpeta: " + selectedFile;
    } else if(!selectedFile.empty()) {
        fullHeader = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : selectedFile;
        if(s_metaLoaded) {
            if(!s_currentMeta.region.empty() && s_currentMeta.region != "Unknown") {
                fullHeader += " (" + s_currentMeta.region + ")";
            }
            if(s_currentMeta.isGBC) fullHeader += " [CGB]";
            else if(s_currentMeta.isSGB) fullHeader += " [SGB]";
            else fullHeader += " [DMG]";
        }
    } else {
        fullHeader = "Game Hub";
    }

    // Hora actual
    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);
    char timeStr[16];
    if(timeinfo != nullptr) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    } else {
        snprintf(timeStr, sizeof(timeStr), "--:--");
    }

    // Batería
    u8 batLevel = getBatteryLevel();
    int batPct = (int)batLevel * 20;
    if(batPct > 100) batPct = 100;
    char batPctStr[8];
    snprintf(batPctStr, sizeof(batPctStr), "%d%%", batPct);
    u32 batCol = (batLevel <= 1) ? uiColorRGBA(255, 60, 60, 255) : (batLevel <= 2 ? colYellow : colGreen);

    // 2. Marco exterior doble dorado (x: 10..390, y: 8..232) con barra superior integrada
    uiDrawDropShadow(10.0f, 8.0f, 380.0f, 224.0f, 4.0f, 90);
    // Borde exterior de 2px en dorado
    uiDrawRectOutline(10.0f, 8.0f, 380.0f, 224.0f, 2.0f, colAmber);
    // Separación oscura de 2px
    uiDrawRect(12.0f, 10.0f, 376.0f, 220.0f, uiColorRGBA(10, 14, 20, 255));
    // Borde interior de 1px en dorado suave
    uiDrawRectOutline(14.0f, 12.0f, 372.0f, 216.0f, 1.0f, uiColorRGBA(150, 118, 45, 255));
    // Relleno interior en azul noche profundo
    uiDrawRect(15.0f, 13.0f, 370.0f, 214.0f, colPanelBlue);

    // Línea 1 de barra superior (y=15): "GAMEYOB 3DS" + Hora + Batería
    uiDrawTextScaled(18.0f, 15.0f, 0.75f, "GAMEYOB 3DS", colAmber);
    uiDrawTextScaled(312.0f, 15.0f, 0.72f, timeStr, colWhite);
    uiDrawTextScaled(344.0f, 15.0f, 0.65f, batPctStr, colGreen);
    uiDrawRectOutline(370.0f, 16.0f, 18.0f, 9.0f, 1.0f, colWhite);
    uiDrawRect(388.0f, 18.0f, 2.0f, 5.0f, colWhite);
    float fillW = ((float)batLevel / 5.0f) * 14.0f;
    if(fillW > 0.0f) {
        uiDrawRect(372.0f, 18.0f, fillW, 5.0f, batCol);
    }

    // Línea 2 de barra superior (y=27): Título completo del juego en blanco brillante
    uiDrawTextScaled(18.0f, 27.0f, 0.75f, fullHeader.c_str(), colWhite);

    // Línea divisoria dorada horizontal en y=40
    uiDrawRect(14.0f, 40.0f, 372.0f, 1.0f, colAmber);

    // 3. Cuadrante Izquierdo (x: 22..146, y: 48..196)
    uiDrawDropShadow(22.0f, 48.0f, 124.0f, 152.0f, 2.0f, 70);
    uiDrawRoundedRect(22.0f, 48.0f, 124.0f, 152.0f, 2.0f, uiColorRGBA(12, 18, 30, 255), uiColorRGBA(45, 65, 95, 255), 1.0f);

    if(isDir) {
        uiDrawTextScaled(44.0f, 85.0f, 0.75f, "[CARPETA]", colYellow);
        uiDrawTextScaled(28.0f, 110.0f, 0.70f, "Subdirectorio SDMC", colGray);
        uiDrawTextScaled(32.0f, 135.0f, 0.70f, "A: Abrir Carpeta", colWhite);
    } else if(!selectedFile.empty()) {
        uiDrawDropShadow(84.0f - 45.0f + eyeOffset * 0.3f, 160.0f, 90.0f, 10.0f, 3.0f, 130);

        if(s_show3DArt) {
            uiDrawBoxart3D(84.0f, 108.0f, 96.0f, gfxGetProjectionTop(), eyeOffset);
        } else {
            if(s_boxartLoaded && s_boxartTexAllocated) {
                C3D_TexBind(0, &s_boxartTex);
                C3D_TexEnv* env0 = C3D_GetTexEnv(0);
                C3D_TexEnvInit(env0);
                C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
                C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
                C3D_TexEnvInit(C3D_GetTexEnv(1));
                C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
                C3D_CullFace(GPU_CULL_NONE);
                C3D_AlphaTest(false, GPU_ALWAYS, 0);

                C3D_ImmDrawBegin(GPU_TRIANGLES);
                emitUiQuad(26.0f + eyeOffset * 0.5f, 50.0f, 142.0f + eyeOffset * 0.5f, 166.0f, 0.0f, 0.0f, 1.0f, 1.0f);
                C3D_ImmDrawEnd();

                uiDrawRectOutline(25.0f + eyeOffset * 0.5f, 49.0f, 118.0f, 118.0f, 1.0f, colAmber);
            } else {
                bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
                u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
                u32 colCartBorder = isGBC ? uiColorRGBA(85, 95, 120, 255) : uiColorRGBA(190, 190, 190, 255);

                uiDrawRect(39.0f + eyeOffset * 0.5f, 55.0f, 90.0f, 106.0f, colCart);
                uiDrawRectOutline(39.0f + eyeOffset * 0.5f, 55.0f, 90.0f, 106.0f, 1.0f, colCartBorder);
                uiDrawRect(47.0f + eyeOffset * 0.5f, 74.0f, 74.0f, 72.0f, uiColorRGBA(24, 34, 52, 255));
                uiDrawRectOutline(47.0f + eyeOffset * 0.5f, 74.0f, 74.0f, 72.0f, 1.0f, colAmber);

                uiDrawTextScaled(52.0f + eyeOffset * 0.5f, 84.0f, 0.75f, "GAME BOY", colWhite);
                if(isGBC) {
                    uiDrawTextScaled(58.0f + eyeOffset * 0.5f, 102.0f, 0.72f, "COLOR", colCyan);
                }
            }
        }

        std::string pubStr = s_metaLoaded ? (!s_currentMeta.developer.empty() ? s_currentMeta.developer : s_currentMeta.publisher) : "Nintendo";
        uiDrawTextScaled(26.0f + eyeOffset * 0.3f, 170.0f, 0.70f, pubStr.c_str(), colWhite);

        char yrGen[40];
        if(s_metaLoaded && s_currentMeta.year > 0) {
            snprintf(yrGen, sizeof(yrGen), "%d  %s", s_currentMeta.year, s_currentMeta.genre.c_str());
        } else {
            snprintf(yrGen, sizeof(yrGen), "Clasico GameBoy");
        }
        uiDrawTextScaled(26.0f + eyeOffset * 0.3f, 184.0f, 0.65f, yrGen, colAmber);
    } else {
        uiDrawTextScaled(44.0f, 105.0f, 0.72f, "Sin seleccion", colGray);
    }

    // 4. Cuadrante Derecho (x: 158..380, y: 48..204)
    uiDrawDropShadow(158.0f, 48.0f, 222.0f, 156.0f, 2.0f, 70);
    uiDrawRoundedRect(158.0f, 48.0f, 222.0f, 156.0f, 2.0f, uiColorRGBA(16, 22, 34, 255), uiColorRGBA(45, 65, 95, 255), 1.0f);

    if(!s_showSynopsis) {
        // MODO FICHA DE COMPATIBILIDAD
        uiDrawTextScaled(166.0f, 52.0f, 0.75f, "FICHA DE COMPATIBILIDAD", colAmber);
        uiDrawRect(164.0f, 66.0f, 210.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
            bool isSGB = s_metaLoaded ? s_currentMeta.isSGB : false;

            // Línea 1: Modo Nativo (y=74)
            uiDrawTextScaled(166.0f, 74.0f, 0.70f, "• Modo Nativo: ", colGray);
            float wL1 = uiGetTextWidth("• Modo Nativo: ", 0.70f);
            uiDrawTextScaled(166.0f + wL1, 74.0f, 0.70f, isGBC ? "Game Boy Color (CGB)" : (isSGB ? "Super GB (SGB)" : "Game Boy (DMG)"), colWhite);

            // Línea 2: Mapper (y=92)
            uiDrawTextScaled(166.0f, 92.0f, 0.70f, "• Mapper: ", colGray);
            float wL2 = uiGetTextWidth("• Mapper: ", 0.70f);
            std::string mapStr = (s_metaLoaded && !s_currentMeta.mbcName.empty()) ? s_currentMeta.mbcName : "MBC Auto";
            if(s_metaLoaded && s_currentMeta.mbcName.find("MBC3") != std::string::npos) mapStr += " (RTC Activo)";
            uiDrawTextScaled(166.0f + wL2, 92.0f, 0.70f, mapStr.c_str(), colCyan);

            // Línea 3: Guardado (y=110)
            uiDrawTextScaled(166.0f, 110.0f, 0.70f, "• Guardado: ", colGray);
            float wL3 = uiGetTextWidth("• Guardado: ", 0.70f);
            std::string savStr = "";
            if(s_metaLoaded) {
                if(s_currentMeta.ramSizeKB > 0) {
                    savStr = "SRAM " + std::to_string(s_currentMeta.ramSizeKB) + " KB";
                    if(s_currentMeta.hasBattery) savStr += " + Bateria";
                } else if(s_currentMeta.hasBattery) {
                    savStr = "Bateria (Sin RAM)";
                } else {
                    savStr = "Sin guardado";
                }
            } else {
                savStr = "SRAM / Flash";
            }
            uiDrawTextScaled(166.0f + wL3, 110.0f, 0.70f, savStr.c_str(), colWhite);

            // Línea 4: Tamaño ROM (y=128)
            uiDrawTextScaled(166.0f, 128.0f, 0.70f, "• Tamano ROM: ", colGray);
            float wL4 = uiGetTextWidth("• Tamano ROM: ", 0.70f);
            std::string romSzStr = "";
            if(s_metaLoaded && s_currentMeta.romSizeKB > 0) {
                int banks = s_currentMeta.romSizeKB / 16;
                romSzStr = std::to_string(s_currentMeta.romSizeKB) + " KB (" + std::to_string(banks) + " bancos)";
            } else {
                romSzStr = "Estandar";
            }
            uiDrawTextScaled(166.0f + wL4, 128.0f, 0.70f, romSzStr.c_str(), colCyan);

            // Línea 5: Audio (y=146)
            uiDrawTextScaled(166.0f, 146.0f, 0.70f, "• Audio: ", colGray);
            float wL5 = uiGetTextWidth("• Audio: ", 0.70f);
            uiDrawTextScaled(166.0f + wL5, 146.0f, 0.70f, "Anillo Dual SPSC 32,728 Hz", colWhite);

            // Línea 6: Motor (y=164)
            uiDrawTextScaled(166.0f, 164.0f, 0.70f, "• Motor: ", colGray);
            float wL6 = uiGetTextWidth("• Motor: ", 0.70f);
            uiDrawTextScaled(166.0f + wL6, 164.0f, 0.70f, "ARM11 JIT 60 FPS", colGray);

            // Al pie de la tarjeta: Última partida (y=184)
            std::string saveInfo = "Ultima partida: ";
            if(s_metaLoaded && s_currentMeta.hasSave) {
                saveInfo += s_currentMeta.saveDate;
            } else {
                saveInfo += "Ninguna";
            }
            uiDrawTextScaled(166.0f, 184.0f, 0.68f, saveInfo.c_str(), colAmber);
        } else {
            uiDrawTextScaled(166.0f, 74.0f, 0.70f, "Directorio SDMC", colWhite);
            uiDrawTextScaled(166.0f, 94.0f, 0.70f, "Seleccione una ROM para ver", colCyan);
            uiDrawTextScaled(166.0f, 110.0f, 0.70f, "su ficha tecnica completa.", colCyan);
            uiDrawTextScaled(166.0f, 136.0f, 0.70f, "Audio: Anillo Dual SPSC 32,728 Hz", colWhite);
            uiDrawTextScaled(166.0f, 156.0f, 0.70f, "Motor: ARM11 JIT 60 FPS", colGray);
        }
    } else {
        // MODO SINOPSIS Y DATOS
        uiDrawTextScaled(166.0f, 52.0f, 0.75f, "SINOPSIS Y DATOS", colAmber);
        uiDrawRect(164.0f, 66.0f, 210.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            // Línea 1: Año y Género (y=74)
            char l1[40];
            if(s_metaLoaded && s_currentMeta.year > 0) {
                snprintf(l1, sizeof(l1), "• Ano: %d | %s", s_currentMeta.year, s_currentMeta.genre.c_str());
            } else {
                snprintf(l1, sizeof(l1), "• Genero: %s", s_metaLoaded ? s_currentMeta.genre.c_str() : "Clasico");
            }
            uiDrawTextScaled(166.0f, 74.0f, 0.70f, l1, colWhite);

            // Línea 2: Desarrollador (y=92)
            std::string dev = s_metaLoaded && !s_currentMeta.developer.empty() ? s_currentMeta.developer : (s_metaLoaded && !s_currentMeta.publisher.empty() ? s_currentMeta.publisher : "Desconocido");
            std::string devLine = "• Dev: " + dev;
            uiDrawTextScaled(166.0f, 92.0f, 0.70f, devLine.c_str(), colCyan);

            // Línea 3: Tiempo Jugado (y=110)
            uiDrawTextScaled(166.0f, 110.0f, 0.70f, "• Tiempo: Registrado", colWhite);

            // Líneas 4..7: Sinopsis formateada
            std::string syn = s_metaLoaded && !s_currentMeta.synopsis.empty() ? s_currentMeta.synopsis : "Titulo clasico de Game Boy totalmente compatible con el nucleo GameYob.";
            std::vector<std::string> synLines = wrapSynopsisText(syn, 34);
            float sy = 128.0f;
            for(size_t i = 0; i < synLines.size() && i < 3; i++) {
                uiDrawTextScaled(166.0f, sy, 0.65f, synLines[i].c_str(), colGray);
                sy += 15.0f;
            }

            // Estado al pie (y=184)
            uiDrawTextScaled(166.0f, 184.0f, 0.68f, "• Nucleo: GameYob 100% OK", colGreen);
        } else {
            uiDrawTextScaled(166.0f, 74.0f, 0.70f, "Directorio SDMC", colWhite);
            uiDrawTextScaled(166.0f, 94.0f, 0.70f, "Seleccione una ROM para ver", colCyan);
            uiDrawTextScaled(166.0f, 110.0f, 0.70f, "su sinopsis y datos.", colCyan);
            uiDrawTextScaled(166.0f, 136.0f, 0.70f, "Audio: Anillo Dual SPSC 32,728 Hz", colWhite);
            uiDrawTextScaled(166.0f, 156.0f, 0.70f, "Motor: ARM11 JIT 60 FPS", colGray);
        }
    }

    // 5. Barra Inferior de Comandos (y: 214..228)
    uiDrawRect(14.0f, 214.0f, 372.0f, 15.0f, uiColorRGBA(10, 14, 20, 255));
    uiDrawRectOutline(14.0f, 214.0f, 372.0f, 15.0f, 1.0f, uiColorRGBA(150, 118, 45, 255));

    const char* footerLegend = "[A] Iniciar Juego   [Y] Alternar Ficha   [SELECT] Ajustes";
    float legW = uiGetTextWidth(footerLegend, 0.70f);
    float lx = 14.0f + (372.0f - legW) * 0.5f;
    uiDrawTextScaled(lx, 215.0f, 0.70f, footerLegend, colWhite);

    // Reset TexEnv para dejar el pipeline limpio
    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
}

#endif
