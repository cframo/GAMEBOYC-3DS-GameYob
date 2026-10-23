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
static u64 s_selectionChangeTime = 0;
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

static C3D_Tex s_fontTex;
static bool s_fontInit = false;

static C3D_Tex s_sysFontTex[8];
static int s_sysFontNumSheets = 0;
static bool s_sysFontInit = false;

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
    if(s_fontInit) {
        return;
    }

    if(!C3D_TexInit(&s_fontTex, 128, 128, GPU_RGBA8)) {
        return;
    }

    u32* temp = (u32*) linearAlloc(128 * 128 * sizeof(u32));
    if(temp == nullptr) {
        C3D_TexDelete(&s_fontTex);
        return;
    }

    memset(temp, 0, 128 * 128 * sizeof(u32));

    for(int c = 0; c < 128; c++) {
        int col = c % 16;
        int row = c / 16;
        for(int sub_y = 0; sub_y < 8; sub_y++) {
            u8 rowBits = font8x8_basic[c][sub_y];
            for(int sub_x = 0; sub_x < 8; sub_x++) {
                if(rowBits & (0x80 >> sub_x)) {
                    int px = col * 8 + sub_x;
                    int py = row * 8 + sub_y;
                    temp[py * 128 + px] = 0xFFFFFFFF;
                    temp[(64 + py) * 128 + px] = 0xFFFFFFFF;
                }
            }
        }
    }

    GSPGPU_FlushDataCache(temp, 128 * 128 * sizeof(u32));
    C3D_SyncDisplayTransfer(
        temp, GX_BUFFER_DIM(128, 128),
        (u32*) s_fontTex.data, GX_BUFFER_DIM(128, 128),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO)
    );
    linearFree(temp);

    GSPGPU_InvalidateDataCache(s_fontTex.data, s_fontTex.size);
    C3D_TexSetFilter(&s_fontTex, GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&s_fontTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);

    s_fontInit = true;

    // Sistema de fuente nativa de alta resolución de 3DS (CFNT)
    Result fontRes = fontEnsureMapped();
    if(R_SUCCEEDED(fontRes)) {
        CFNT_s* sysFont = fontGetSystemFont();
        if(sysFont != nullptr) {
            TGLP_s* glyphInfo = fontGetGlyphInfo(sysFont);
            if(glyphInfo != nullptr) {
                s_sysFontNumSheets = glyphInfo->nSheets;
                if(s_sysFontNumSheets > 8) {
                    s_sysFontNumSheets = 8;
                }
                for(int i = 0; i < s_sysFontNumSheets; i++) {
                    memset(&s_sysFontTex[i], 0, sizeof(C3D_Tex));
                    s_sysFontTex[i].data = fontGetGlyphSheetTex(sysFont, i);
                    s_sysFontTex[i].fmt = (GPU_TEXCOLOR) glyphInfo->sheetFmt;
                    s_sysFontTex[i].size = glyphInfo->sheetSize;
                    s_sysFontTex[i].width = glyphInfo->sheetWidth;
                    s_sysFontTex[i].height = glyphInfo->sheetHeight;
                    s_sysFontTex[i].param = GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
                                          | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE);
                    s_sysFontTex[i].border = 0;
                    s_sysFontTex[i].lodParam = 0;
                }
                s_sysFontInit = true;
            }
        }
    }

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
    if(s_fontInit) {
        C3D_TexDelete(&s_fontTex);
        s_fontInit = false;
    }
    s_sysFontInit = false;
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
        float tw = s_sysFontInit ? uiGetSysTextWidth(0.40f, label) : ((float)strlen(label) * 8.0f);
        float tx = x + (w - tw) * 0.5f;
        float ty = y + (h - (s_sysFontInit ? 12.0f : 8.0f)) * 0.5f + (isPressed ? 1.0f : 0.0f);
        uiDrawText(tx, ty, label, UI_COLOR_WHITE);
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

void uiDrawLegacyText(float x, float y, const char* str, u32 color) {
    if(!str || *str == '\0' || !s_fontInit) {
        return;
    }

    bool hasVisible = false;
    int len = 0;
    for(const char* p = str; *p != '\0' && len < 64; p++, len++) {
        u8 c = (u8)*p;
        if(c > 32) {
            hasVisible = true;
            break;
        }
    }
    if(!hasVisible) {
        return;
    }

    C3D_TexBind(0, &s_fontTex);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_RGB, GPU_CONSTANT, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_RGB, GPU_REPLACE);
    C3D_TexEnvSrc(env0, C3D_Alpha, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Alpha, GPU_REPLACE);
    C3D_TexEnvColor(env0, color | 0xFF000000);

    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    C3D_ImmDrawBegin(GPU_TRIANGLES);
    float cur_x = x;
    float cur_y = y;
    len = 0;
    for(const char* p = str; *p != '\0' && len < 64; p++, len++) {
        u8 c = (u8)*p;
        if(c == 0xE2 && (u8)*(p + 1) == 0x80 && (u8)*(p + 2) == 0xA2) {
            p += 2;
            c = '-';
        } else if(c == 0xC3 && *(p + 1) != '\0') {
            p++;
            u8 next = (u8)*p;
            switch(next) {
                case 0xA1: c = 'a'; break;
                case 0xA9: c = 'e'; break;
                case 0xAD: c = 'i'; break;
                case 0xB3: c = 'o'; break;
                case 0xBA: c = 'u'; break;
                case 0xB1: c = 'n'; break;
                case 0x81: c = 'A'; break;
                case 0x89: c = 'E'; break;
                case 0x8D: c = 'I'; break;
                case 0x93: c = 'O'; break;
                case 0x9A: c = 'U'; break;
                case 0x91: c = 'N'; break;
                default:   c = '?'; break;
            }
        }
        if(c == '\n') {
            cur_x = x;
            cur_y += 8.0f;
            continue;
        }
        if(c == '\r') {
            continue;
        }
        if(c == ' ') {
            cur_x += 8.0f;
            continue;
        }
        if(c > 127) {
            c = '?';
        }
        if(c > 32) {
            int col = (int)c % 16;
            int row = (int)c / 16;
            float u0 = (float)(col * 8) / 128.0f;
            float v0 = (float)(row * 8) / 128.0f;
            float u1 = u0 + (8.0f / 128.0f);
            float v1 = v0 + (8.0f / 128.0f);
            emitUiQuad(cur_x, cur_y, cur_x + 8.0f, cur_y + 8.0f, u0, v0, u1, v1);
        }
        cur_x += 8.0f;
    }
    C3D_ImmDrawEnd();

    C3D_AlphaTest(false, GPU_ALWAYS, 0);
}

void uiDrawSysText(float x, float y, float scaleX, float scaleY, u32 color, const char* text) {
    if(!text || *text == '\0') {
        return;
    }
    if(!s_sysFontInit) {
        uiDrawLegacyText(x, y, text, color);
        return;
    }

    CFNT_s* sysFont = fontGetSystemFont();
    if(!sysFont) {
        return;
    }

    if((color & 0xFF000000) == 0) {
        color |= 0xFF000000;
    }

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

    float curX = x;
    float curY = y;
    int curSheet = -1;
    bool inDraw = false;

    const char* p = text;
    while(*p != '\0') {
        if(*p == '\n') {
            curX = x;
            curY += (float) sysFont->finf.lineFeed * scaleY;
            p++;
            continue;
        }
        if(*p == '\r') {
            p++;
            continue;
        }

        uint32_t code = 0;
        ssize_t units = decode_utf8(&code, (const uint8_t*) p);
        if(units <= 0) {
            code = (uint8_t)*p;
            units = 1;
        }
        p += units;

        int glyphIdx = fontGlyphIndexFromCodePoint(sysFont, code);
        fontGlyphPos_s data;
        fontCalcGlyphPos(&data, sysFont, glyphIdx, GLYPH_POS_CALC_VTXCOORD, scaleX, scaleY);

        if(data.width > 0.0f && data.vtxcoord.right > data.vtxcoord.left) {
            if(data.sheetIndex >= 0 && data.sheetIndex < s_sysFontNumSheets) {
                if(curSheet != data.sheetIndex) {
                    if(inDraw) {
                        C3D_ImmDrawEnd();
                        inDraw = false;
                    }
                    curSheet = data.sheetIndex;
                    C3D_TexBind(0, &s_sysFontTex[curSheet]);
                }

                if(!inDraw) {
                    C3D_ImmDrawBegin(GPU_TRIANGLES);
                    inDraw = true;
                }

                emitUiQuad(curX + data.vtxcoord.left, curY + data.vtxcoord.top,
                           curX + data.vtxcoord.right, curY + data.vtxcoord.bottom,
                           data.texcoord.left, data.texcoord.top,
                           data.texcoord.right, data.texcoord.bottom);
            }
        }

        curX += data.xAdvance;
    }

    if(inDraw) {
        C3D_ImmDrawEnd();
    }
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
}

float uiGetSysTextWidth(float scaleX, const char* text) {
    if(!text || *text == '\0') {
        return 0.0f;
    }
    if(!s_sysFontInit) {
        return (float) strlen(text) * 8.0f;
    }
    CFNT_s* sysFont = fontGetSystemFont();
    if(!sysFont) {
        return (float) strlen(text) * 8.0f;
    }

    float width = 0.0f;
    float maxWidth = 0.0f;
    const char* p = text;
    while(*p != '\0') {
        if(*p == '\n') {
            if(width > maxWidth) {
                maxWidth = width;
            }
            width = 0.0f;
            p++;
            continue;
        }
        if(*p == '\r') {
            p++;
            continue;
        }

        uint32_t code = 0;
        ssize_t units = decode_utf8(&code, (const uint8_t*) p);
        if(units <= 0) {
            code = (uint8_t)*p;
            units = 1;
        }
        p += units;

        int glyphIdx = fontGlyphIndexFromCodePoint(sysFont, code);
        fontGlyphPos_s data;
        fontCalcGlyphPos(&data, sysFont, glyphIdx, 0, scaleX, scaleX);
        width += data.xAdvance;
    }
    if(width > maxWidth) {
        maxWidth = width;
    }
    return maxWidth;
}

void uiDrawText(float x, float y, const char* str, u32 color) {
    if(!str || *str == '\0') {
        return;
    }
    if(s_sysFontInit) {
        uiDrawSysText(x, y, 0.44f, 0.44f, color, str);
    } else {
        uiDrawLegacyText(x, y, str, color);
    }
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
    if(pathStr != s_lastBoxartPath) {
        s_lastBoxartPath = pathStr;
        s_selectionChangeTime = osGetTime();
        s_boxartLoaded = false;
        s_boxartChecked = false;
        s_metaLoaded = false;
        s_lastMetaRom = pathStr;
        s_isGBCBox = isGBC;
    }

    // Debounce de 120 ms
    if(!s_boxartChecked && (osGetTime() - s_selectionChangeTime > 120)) {
        s_boxartChecked = true;

        if(!s_metaLoaded) {
            s_metaLoaded = fetchRomMetadata(pathStr, s_currentMeta);
            if(s_metaLoaded) {
                s_isGBCBox = s_currentMeta.isGBC;
            }
        }

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
        // 1) Misma carpeta que la ROM: <romFullPathSinExt>.png (ej. '/gb/Pokemon Oro.png')
        std::string cand1 = romPathSinExt + ".png";
        // 2) '/3ds/gameyob/boxart/<romFileNameSinExt>.png'
        std::string cand2 = "/3ds/gameyob/boxart/" + romBaseName + ".png";
        // 3) '/3ds/gameyob/boxart/<cleanHeaderTitle>.png'
        std::string cand3 = cleanTitle.empty() ? "" : ("/3ds/gameyob/boxart/" + cleanTitle + ".png");
        // 4) '<romDir>boxart/<romFileNameSinExt>.png'
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
            // 5) Escaneo rápido case-insensitive en '/3ds/gameyob/boxart/'
            if(!cleanTitle.empty()) {
                targetPath = findBoxartPrefixMatch(cleanTitle);
            }
            if(targetPath.empty()) {
                targetPath = findBoxartPrefixMatch(romBaseName);
            }
        }

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
                    s_boxartLoaded = true;
                }
                stbi_image_free(imgData);
            }
        }

        if(!s_boxartLoaded) {
            scraperEnqueue(pathStr, cleanTitle, 0);
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

    // Header Superior (y: 4..20)
    std::string dirTitle = chooser->getDirectory();
    uiDrawSysText(10.0f, 5.0f, 0.44f, 0.44f, colAmber, "ROMS: ");
    float romsW = uiGetSysTextWidth(0.44f, "ROMS: ");
    uiDrawSysText(10.0f + romsW, 5.0f, 0.42f, 0.42f, colWhite, dirTitle.c_str());
    uiDrawSysText(272.0f, 5.0f, 0.42f, 0.42f, colGray, "v1.0.9");
    uiDrawRect(10.0f, 22.0f, 300.0f, 1.0f, colBorder);

    // 2. Lista de ROMs (Panel Izquierdo, x: 10..182, y: 24..204)
    uiDrawDropShadow(10.0f, 24.0f, 172.0f, 180.0f, 3.0f, 80);
    uiDrawRoundedRect(10.0f, 24.0f, 172.0f, 180.0f, 3.0f, colPanelBg, colListBorder, 1.0f);

    const std::vector<std::string>& files = chooser->getFilenames();
    const std::vector<int>& flags = chooser->getFlags();
    int numFiles = chooser->getNumFiles();
    int selection = chooser->getSelection();
    float scrollY = chooser->getSubpixelScrollY();

    int firstIdx = (int)(scrollY / 20.0f);
    if(firstIdx < 0) firstIdx = 0;

    for(int i = firstIdx; i < numFiles; i++) {
        float itemY = 25.0f + (float)i * 20.0f - scrollY;
        if(itemY < 12.0f) continue;
        if(itemY > 204.0f) break;

        bool isSelected = (i == selection);
        bool isDir = (i < (int)flags.size()) && ((flags[i] & 1) != 0);
        std::string rawName = (i < (int)files.size()) ? files[i] : "";

        if(isSelected) {
            uiDrawRoundedRect(11.0f, itemY, 159.0f, 19.0f, 3.0f, uiColorRGBA(180, 135, 30, 255), colAmber, 1.0f);
            uiDrawSysText(13.0f, itemY + 2.0f, 0.38f, 0.38f, colWhite, "►");
            uiDrawSysText(159.0f, itemY + 2.0f, 0.38f, 0.38f, colWhite, "◄");
        }

        if(isDir) {
            uiDrawFolderIcon(20.0f, itemY + 3.0f);
            uiDrawSysText(36.0f, itemY + 2.0f, 0.40f, 0.40f, isSelected ? colWhite : colYellow, rawName.c_str());
        } else {
            auto dot = rawName.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? rawName.substr(dot + 1) : "";
            bool isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

            uiDrawCartridgeIcon(20.0f, itemY + 3.0f, isGBC);

            std::string label = (isGBC ? "[CGB] " : "[DMG] ") + rawName;
            u32 textCol = isSelected ? colWhite : (isGBC ? colWhite : colGray);
            uiDrawSysText(36.0f, itemY + 2.0f, 0.40f, 0.40f, textCol, label.c_str());
        }
    }

    if(numFiles == 0) {
        uiDrawSysText(20.0f, 40.0f, 0.42f, 0.42f, colDarkGray, "Sin archivos");
    } else {
        // Scrollbar vertical con botones ▲ y ▼
        uiDrawRect(172.0f, 25.0f, 8.0f, 10.0f, uiColorRGBA(25, 35, 50, 255));
        uiDrawRectOutline(172.0f, 25.0f, 8.0f, 10.0f, 1.0f, uiColorRGBA(50, 70, 95, 255));
        uiDrawSysText(173.0f, 24.0f, 0.30f, 0.30f, colWhite, "▲");

        uiDrawRect(172.0f, 193.0f, 8.0f, 10.0f, uiColorRGBA(25, 35, 50, 255));
        uiDrawRectOutline(172.0f, 193.0f, 8.0f, 10.0f, 1.0f, uiColorRGBA(50, 70, 95, 255));
        uiDrawSysText(173.0f, 192.0f, 0.30f, 0.30f, colWhite, "▼");

        float maxScroll = chooser->getMaxScroll();
        if(maxScroll > 0.0f) {
            const float trackY = 36.0f;
            const float trackH = 156.0f;
            float totalH = (float)numFiles * 20.0f;
            float thumbH = trackH * (160.0f / totalH);
            if(thumbH < 12.0f) thumbH = 12.0f;
            if(thumbH > trackH) thumbH = trackH;
            float thumbY = trackY + (scrollY / maxScroll) * (trackH - thumbH);
            if(thumbY < trackY) thumbY = trackY;
            if(thumbY + thumbH > trackY + trackH) thumbY = trackY + trackH - thumbH;
            uiDrawRoundedRect(173.0f, thumbY, 6.0f, thumbH, 2.0f, uiColorRGBA(70, 115, 175, 255), uiColorRGBA(100, 160, 240, 255), 1.0f);
        }
    }

    // Máscara superior e inferior para recortar listas desbordadas
    uiDrawRect(0.0f, 0.0f, 320.0f, 23.0f, colBg);
    uiDrawSysText(10.0f, 5.0f, 0.44f, 0.44f, colAmber, "ROMS: ");
    uiDrawSysText(10.0f + romsW, 5.0f, 0.42f, 0.42f, colWhite, dirTitle.c_str());
    uiDrawSysText(272.0f, 5.0f, 0.42f, 0.42f, colGray, "v1.0.9");
    uiDrawRect(10.0f, 22.0f, 300.0f, 1.0f, colBorder);

    uiDrawRect(0.0f, 205.0f, 320.0f, 7.0f, colBg);

    // 3. Panel Derecho Multi-Modo Contextual (x: 188..310, y: 24..204)
    uiDrawDropShadow(188.0f, 24.0f, 122.0f, 180.0f, 3.0f, 80);
    uiDrawRoundedRect(188.0f, 24.0f, 122.0f, 180.0f, 3.0f, uiColorRGBA(14, 18, 26, 255), colAmber, 1.0f);

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
            uiDrawSysText(194.0f, 27.0f, 0.40f, 0.40f, colYellow, "CAPTURA GUARDADO");
        } else {
            uiDrawSysText(200.0f, 27.0f, 0.40f, 0.40f, colAmber, "VISTA PREVIA");
        }
        uiDrawRect(190.0f, 39.0f, 118.0f, 1.0f, colAmber);

        // Recuadro del visor (x: 198..300, y: 44..142)
        uiDrawDropShadow(198.0f, 44.0f, 102.0f, 98.0f, 2.0f, 70);
        uiDrawRoundedRect(198.0f, 44.0f, 102.0f, 98.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);

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
            emitUiQuad(200.0f, 46.0f, 298.0f, 140.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            C3D_ImmDrawEnd();

            uiDrawRectOutline(199.0f, 45.0f, 100.0f, 96.0f, 1.0f, colAmber);
        } else if(!currentSelectedRom.empty()) {
            bool isGBC = (currentSelectedRom.find(".gbc") != std::string::npos || currentSelectedRom.find(".cgb") != std::string::npos);
            u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
            uiDrawRect(224.0f, 54.0f, 50.0f, 60.0f, colCart);
            uiDrawRectOutline(224.0f, 54.0f, 50.0f, 60.0f, 1.0f, colAmber);
            uiDrawSysText(230.0f, 72.0f, 0.42f, 0.42f, colWhite, isGBC ? "CGB" : "DMG");
            uiDrawSysText(210.0f, 122.0f, 0.36f, 0.36f, hasState ? colGreen : (hasSav ? colCyan : colGray),
                          hasState ? "Estado OK" : (hasSav ? "SRAM OK" : "Sin Partida"));
        } else {
            uiDrawSysText(208.0f, 84.0f, 0.38f, 0.38f, colGray, "Sin Archivo");
        }

        // Título limpio centrado debajo del visor
        std::string cleanName = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : baseSelectedName;
        if(!cleanName.empty()) {
            float nameW = uiGetSysTextWidth(0.38f, cleanName.c_str());
            float nx = (nameW < 118.0f) ? (188.0f + (122.0f - nameW) * 0.5f) : 192.0f;
            uiDrawSysText(nx, 146.0f, 0.38f, 0.38f, colAmber, cleanName.c_str());
        }

        // Selector de Slots interactivo (y: 162)
        uiDrawSysText(196.0f, 162.0f, 0.38f, 0.38f, colAmber, "◄");
        char sBuf[16];
        snprintf(sBuf, sizeof(sBuf), "Slot %d", s_previewSlot);
        uiDrawSysText(210.0f, 162.0f, 0.38f, 0.38f, colWhite, sBuf);
        uiDrawSysText(256.0f, 162.0f, 0.38f, 0.38f, colAmber, "►");
        uiDrawPillBadge(272.0f, 161.0f, 32.0f, 14.0f,
                        hasState ? "SAV" : (hasSav ? "SRAM" : "---"),
                        hasState ? uiColorRGBA(20, 100, 40, 255) : (hasSav ? uiColorRGBA(15, 80, 100, 255) : uiColorRGBA(30, 35, 45, 255)),
                        hasState ? colWhite : (hasSav ? colCyan : colDarkGray));

        // Botón de atajo para iniciar ROM (y: 182..200)
        uiDrawModernButton(192.0f, 182.0f, 114.0f, 18.0f, "▶ Iniciar ROM", uiColorRGBA(25, 45, 75, 255), colAmber, false);
    } else if(s_brMode == BR_MODE_DISPLAY) {
        // MODO 1: AJUSTES DE DISPLAY
        uiDrawSysText(192.0f, 27.0f, 0.40f, 0.40f, colAmber, "⚙ AJUSTES DISPLAY");
        uiDrawRect(190.0f, 39.0f, 118.0f, 1.0f, colAmber);

        // Fila 0: Escalado (y: 45)
        uiDrawSysText(192.0f, 45.0f, 0.38f, 0.38f, colWhite, "• Escalado:");
        static const char* scaleNames[5] = {"Off", "125%", "150%", "Aspect", "Full"};
        char v0[24];
        snprintf(v0, sizeof(v0), "◄ %s ►", scaleNames[getMenuOption("Scaling") % 5]);
        float w0 = uiGetSysTextWidth(0.36f, v0);
        uiDrawSysText(306.0f - w0, 45.0f, 0.36f, 0.36f, colAmber, v0);

        // Fila 1: Filtro (y: 69)
        uiDrawSysText(192.0f, 69.0f, 0.38f, 0.38f, colWhite, "• Filtro:");
        static const char* filterNames[3] = {"Nearest", "Linear", "Sharp 2x"};
        char v1[24];
        snprintf(v1, sizeof(v1), "◄ %s ►", filterNames[getMenuOption("Scale Filter") % 3]);
        float w1 = uiGetSysTextWidth(0.36f, v1);
        uiDrawSysText(306.0f - w1, 69.0f, 0.36f, 0.36f, colAmber, v1);

        // Fila 2: Rejilla LCD (y: 93)
        uiDrawSysText(192.0f, 93.0f, 0.38f, 0.38f, colWhite, "• Rejilla LCD:");
        const char* v2 = getMenuOption("LCD Grid") ? "◄  On  ►" : "◄  Off  ►";
        float w2 = uiGetSysTextWidth(0.36f, v2);
        uiDrawSysText(306.0f - w2, 93.0f, 0.36f, 0.36f, getMenuOption("LCD Grid") ? colGreen : colDarkGray, v2);

        // Tarjeta informativa inferior (y: 122..198)
        uiDrawRoundedRect(192.0f, 122.0f, 114.0f, 76.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        int curFilt = getMenuOption("Scale Filter") % 3;
        if(curFilt == 2) {
            uiDrawSysText(196.0f, 126.0f, 0.36f, 0.36f, colAmber, "ℹ SHARP BILINEAR");
            uiDrawSysText(196.0f, 142.0f, 0.34f, 0.34f, colWhite, "RTT 2x escalado");
            uiDrawSysText(196.0f, 156.0f, 0.34f, 0.34f, colGray, "Pixeles definidos");
            uiDrawSysText(196.0f, 170.0f, 0.34f, 0.34f, colGray, "Sin shimmer ni blur");
            uiDrawSysText(196.0f, 184.0f, 0.34f, 0.34f, colGreen, "Recomendado 3DS");
        } else if(curFilt == 1) {
            uiDrawSysText(196.0f, 126.0f, 0.36f, 0.36f, colCyan, "ℹ FILTRO LINEAL");
            uiDrawSysText(196.0f, 142.0f, 0.34f, 0.34f, colWhite, "Suavizado PICA200");
            uiDrawSysText(196.0f, 156.0f, 0.34f, 0.34f, colGray, "Transicion suave");
            uiDrawSysText(196.0f, 170.0f, 0.34f, 0.34f, colGray, "Escala continua");
            uiDrawSysText(196.0f, 184.0f, 0.34f, 0.34f, colGray, "Aspecto uniforme");
        } else {
            uiDrawSysText(196.0f, 126.0f, 0.36f, 0.36f, colYellow, "ℹ NEAREST NEIGHBOR");
            uiDrawSysText(196.0f, 142.0f, 0.34f, 0.34f, colWhite, "Muestreo directo");
            uiDrawSysText(196.0f, 156.0f, 0.34f, 0.34f, colGray, "Pixel art puro");
            uiDrawSysText(196.0f, 170.0f, 0.34f, 0.34f, colGray, "Sin interpolacion");
            uiDrawSysText(196.0f, 184.0f, 0.34f, 0.34f, colGray, "Escalado no entero");
        }
    } else if(s_brMode == BR_MODE_PALETTE) {
        // MODO 2: PALETAS DMG
        uiDrawSysText(196.0f, 27.0f, 0.40f, 0.40f, colAmber, "🎨 PALETAS DMG");
        uiDrawRect(190.0f, 39.0f, 118.0f, 1.0f, colAmber);

        // Selector táctil de paleta (y: 44..62)
        char palBuf[32];
        snprintf(palBuf, sizeof(palBuf), "◄ %s ►", mgrGetPaletteName(gbColorizeMode));
        float pw = uiGetSysTextWidth(0.38f, palBuf);
        float px = (pw < 118.0f) ? (188.0f + (122.0f - pw) * 0.5f) : 192.0f;
        uiDrawSysText(px, 45.0f, 0.38f, 0.38f, colYellow, palBuf);

        // Recuadro de los 4 colores de muestra (y: 65..115)
        uiDrawRoundedRect(192.0f, 65.0f, 114.0f, 50.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        const u32* palCols = mgrGetPaletteColors(gbColorizeMode);
        for(int c = 0; c < 4; c++) {
            float sx = 196.0f + (float)c * 27.0f;
            uiDrawRect(sx, 72.0f, 24.0f, 26.0f, palCols[c]);
            uiDrawRectOutline(sx, 72.0f, 24.0f, 26.0f, 1.0f, colWhite);
        }
        uiDrawSysText(198.0f, 102.0f, 0.32f, 0.32f, colGray, "4 tonos oficiales DMG");

        // Ficha descriptiva (y: 120..168)
        uiDrawRoundedRect(192.0f, 120.0f, 114.0f, 48.0f, 2.0f, uiColorRGBA(10, 14, 22, 255), uiColorRGBA(40, 60, 90, 255), 1.0f);
        uiDrawSysText(196.0f, 124.0f, 0.34f, 0.34f, colWhite, "Modo monocromo");
        uiDrawSysText(196.0f, 138.0f, 0.32f, 0.32f, colGray, "Aplica a juegos DMG");
        uiDrawSysText(196.0f, 152.0f, 0.32f, 0.32f, colAmber, "Colorizado en vivo");

        // Botón [Siguiente Paleta] (y: 174..198)
        uiDrawModernButton(192.0f, 174.0f, 114.0f, 22.0f, "Siguiente Paleta", uiColorRGBA(45, 30, 15, 255), colAmber, false);
    }

    // 4. Dock Inferior Temático (y: 212..236)
    // Botón 1 (x: 8..78): [Display]
    bool dispActive = (s_brMode == BR_MODE_DISPLAY);
    uiDrawModernButton(8.0f, 212.0f, 70.0f, 24.0f, "[Display]",
                       dispActive ? uiColorRGBA(25, 60, 90, 255) : uiColorRGBA(15, 35, 50, 255),
                       dispActive ? colCyan : uiColorRGBA(30, 180, 210, 255), dispActive);

    // Botón 2 (x: 84..154): [Paleta]
    bool palActive = (s_brMode == BR_MODE_PALETTE);
    uiDrawModernButton(84.0f, 212.0f, 70.0f, 24.0f, "[Paleta]",
                       palActive ? uiColorRGBA(70, 45, 20, 255) : uiColorRGBA(45, 30, 15, 255),
                       palActive ? colAmber : uiColorRGBA(230, 130, 30, 255), palActive);

    // Botón 3 (x: 160..236): [Carpeta]
    uiDrawModernButton(160.0f, 212.0f, 76.0f, 24.0f, "[Carpeta]", uiColorRGBA(15, 25, 52, 255), uiColorRGBA(40, 120, 220, 255), false);

    // Botón 4 (x: 242..312): [X Salir]
    uiDrawModernButton(242.0f, 212.0f, 70.0f, 24.0f, "[X Salir]", uiColorRGBA(160, 30, 30, 255), uiColorRGBA(230, 50, 50, 255), false);

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

    // 2. Barra de estado superior (y: 3..19)
    uiDrawSysText(10.0f, 4.0f, 0.44f, 0.44f, colAmber, "GAMEYOB 3DS");
    float tagW = uiGetSysTextWidth(0.44f, "GAMEYOB 3DS  ");

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
    uiDrawSysText(312.0f, 4.0f, 0.42f, 0.42f, colWhite, timeStr);

    // Batería en esquina derecha (porcentaje + icono)
    u8 batLevel = getBatteryLevel();
    int batPct = (int)batLevel * 20;
    if(batPct > 100) batPct = 100;
    char batPctStr[8];
    snprintf(batPctStr, sizeof(batPctStr), "%d%%", batPct);
    uiDrawSysText(346.0f, 4.0f, 0.36f, 0.36f, colGreen, batPctStr);

    uiDrawRectOutline(372.0f, 5.0f, 20.0f, 10.0f, 1.0f, colWhite);
    uiDrawRect(392.0f, 7.0f, 2.0f, 6.0f, colWhite);
    u32 batCol = (batLevel <= 1) ? uiColorRGBA(255, 60, 60, 255) : (batLevel <= 2 ? colYellow : colGreen);
    float fillW = ((float)batLevel / 5.0f) * 16.0f;
    if(fillW > 0.0f) {
        uiDrawRect(374.0f, 7.0f, fillW, 6.0f, batCol);
    }

    // Obtener selección actual desde FileChooser
    FileChooser* chooser = getActiveFileChooser();
    bool isDir = false;
    std::string selectedFile = "";
    std::string fullPath = "";

    if(chooser != nullptr) {
        const std::vector<std::string>& files = chooser->getFilenames();
        const std::vector<int>& flags = chooser->getFlags();
        int numFiles = chooser->getNumFiles();
        int selection = chooser->getSelection();

        if(selection >= 0 && selection < numFiles && selection < (int)files.size() && selection < (int)flags.size()) {
            isDir = (flags[selection] & 1) != 0;
            selectedFile = files[selection];
            if(!isDir) {
                std::string dir = chooser->getDirectory();
                if(!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
                    dir += "/";
                }
                fullPath = dir + selectedFile;

                std::string::size_type dot = selectedFile.find_last_of('.');
                std::string ext = (dot != std::string::npos) ? selectedFile.substr(dot + 1) : "";
                bool isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

                if(!s_metaLoaded || s_lastMetaRom != fullPath) {
                    s_metaLoaded = fetchRomMetadata(fullPath, s_currentMeta);
                    s_lastMetaRom = fullPath;
                }

                uiUpdateBoxart(fullPath.c_str(), nullptr, 0, isGBC);
            }
        }
    }

    // Título completo sin truncar en la cabecera
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
    uiDrawSysText(10.0f + tagW, 4.0f, 0.42f, 0.42f, colWhite, fullHeader.c_str());

    // 3. Marco decorativo principal doble dorado (x: 10..390, y: 22..218)
    uiDrawDropShadow(10.0f, 22.0f, 380.0f, 196.0f, 5.0f, 100);
    // Borde exterior de 2px en dorado
    uiDrawRectOutline(10.0f, 22.0f, 380.0f, 196.0f, 2.0f, colAmber);
    // Separación oscura de 2px
    uiDrawRect(12.0f, 24.0f, 376.0f, 192.0f, uiColorRGBA(10, 14, 20, 255));
    // Borde interior de 1px en dorado suave
    uiDrawRectOutline(14.0f, 26.0f, 372.0f, 188.0f, 1.0f, uiColorRGBA(150, 118, 45, 255));
    // Relleno interior en azul noche profundo
    uiDrawRect(15.0f, 27.0f, 370.0f, 186.0f, colPanelBlue);

    // 4. Cuadrante Izquierdo (x: 22..152, y: 34..204)
    uiDrawDropShadow(22.0f, 34.0f, 130.0f, 170.0f, 3.0f, 80);
    uiDrawRoundedRect(22.0f, 34.0f, 130.0f, 170.0f, 3.0f, uiColorRGBA(12, 18, 30, 255), uiColorRGBA(50, 70, 95, 255), 1.0f);

    if(isDir) {
        uiDrawSysText(50.0f, 80.0f, 0.44f, 0.44f, colYellow, "[CARPETA]");
        uiDrawSysText(32.0f, 105.0f, 0.40f, 0.40f, colGray, "Subdirectorio SDMC");
        uiDrawSysText(38.0f, 130.0f, 0.40f, 0.40f, colWhite, "A: Abrir Carpeta");
    } else if(!selectedFile.empty()) {
        // Sombra proyectada inferior
        uiDrawDropShadow(87.0f - 45.0f + eyeOffset * 0.3f, 160.0f, 90.0f, 12.0f, 4.0f, 140);

        if(s_show3DArt) {
            uiDrawBoxart3D(87.0f, 98.0f, 100.0f, gfxGetProjectionTop(), eyeOffset);
        } else {
            if(s_boxartLoaded && s_boxartTexAllocated) {
                float shadowOffset = 3.0f;
                uiDrawRect(28.0f + eyeOffset * 0.5f + shadowOffset, 40.0f + shadowOffset, 120.0f, 120.0f, uiColorRGBA(0, 0, 0, 160));

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
                emitUiQuad(28.0f + eyeOffset * 0.5f, 40.0f, 148.0f + eyeOffset * 0.5f, 160.0f, 0.0f, 0.0f, 1.0f, 1.0f);
                C3D_ImmDrawEnd();

                uiDrawRectOutline(27.0f + eyeOffset * 0.5f, 39.0f, 122.0f, 122.0f, 1.0f, colAmber);
            } else {
                bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
                u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
                u32 colCartBorder = isGBC ? uiColorRGBA(85, 95, 120, 255) : uiColorRGBA(190, 190, 190, 255);

                uiDrawRect(36.0f + eyeOffset * 0.5f, 42.0f, 102.0f, 114.0f, colCart);
                uiDrawRectOutline(36.0f + eyeOffset * 0.5f, 42.0f, 102.0f, 114.0f, 1.0f, colCartBorder);
                uiDrawRect(44.0f + eyeOffset * 0.5f, 62.0f, 86.0f, 82.0f, uiColorRGBA(24, 34, 52, 255));
                uiDrawRectOutline(44.0f + eyeOffset * 0.5f, 62.0f, 86.0f, 82.0f, 1.0f, colAmber);

                uiDrawSysText(50.0f + eyeOffset * 0.5f, 72.0f, 0.44f, 0.44f, colWhite, "GAME BOY");
                if(isGBC) {
                    uiDrawSysText(50.0f + eyeOffset * 0.5f, 88.0f, 0.42f, 0.42f, colCyan, "COLOR");
                }
            }
        }

        std::string pubStr = s_metaLoaded ? (!s_currentMeta.developer.empty() ? s_currentMeta.developer : s_currentMeta.publisher) : "Nintendo";
        uiDrawSysText(26.0f + eyeOffset * 0.3f, 172.0f, 0.40f, 0.40f, colWhite, pubStr.c_str());

        char yrGen[40];
        if(s_metaLoaded && s_currentMeta.year > 0) {
            snprintf(yrGen, sizeof(yrGen), "%d  %s", s_currentMeta.year, s_currentMeta.genre.c_str());
        } else {
            snprintf(yrGen, sizeof(yrGen), "Clasico GameBoy");
        }
        uiDrawSysText(26.0f + eyeOffset * 0.3f, 188.0f, 0.38f, 0.38f, colAmber, yrGen);
    } else {
        uiDrawSysText(44.0f, 105.0f, 0.40f, 0.40f, colGray, "Sin seleccion");
    }

    // 5. Cuadrante Derecho (x: 164..382, y: 34..204)
    uiDrawDropShadow(164.0f, 34.0f, 218.0f, 170.0f, 3.0f, 80);
    uiDrawRoundedRect(164.0f, 34.0f, 218.0f, 170.0f, 3.0f, uiColorRGBA(14, 20, 32, 255), uiColorRGBA(40, 56, 80, 255), 1.0f);

    if(!s_showSynopsis) {
        // MODO FICHA DE COMPATIBILIDAD
        uiDrawSysText(172.0f, 38.0f, 0.44f, 0.44f, colAmber, "FICHA DE COMPATIBILIDAD");
        uiDrawRect(166.0f, 52.0f, 214.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
            bool isSGB = s_metaLoaded ? s_currentMeta.isSGB : false;

            // Línea 1: Modo Nativo
            uiDrawSysText(172.0f, 58.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 58.0f, 0.40f, 0.40f, colGray, "Modo Nativo: ");
            uiDrawSysText(250.0f, 58.0f, 0.40f, 0.40f, colWhite, isGBC ? "Game Boy Color (CGB)" : (isSGB ? "Super GB (SGB)" : "Game Boy (DMG)"));

            // Línea 2: Mapper
            uiDrawSysText(172.0f, 74.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 74.0f, 0.40f, 0.40f, colGray, "Mapper: ");
            std::string mapStr = (s_metaLoaded && !s_currentMeta.mbcName.empty()) ? s_currentMeta.mbcName : "MBC Auto";
            if(s_metaLoaded && s_currentMeta.mbcName.find("MBC3") != std::string::npos) mapStr += " (RTC Activo)";
            uiDrawSysText(226.0f, 74.0f, 0.40f, 0.40f, colCyan, mapStr.c_str());

            // Línea 3: Guardado
            uiDrawSysText(172.0f, 90.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 90.0f, 0.40f, 0.40f, colGray, "Guardado: ");
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
            uiDrawSysText(238.0f, 90.0f, 0.40f, 0.40f, colWhite, savStr.c_str());

            // Línea 4: Tamaño ROM
            uiDrawSysText(172.0f, 106.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 106.0f, 0.40f, 0.40f, colGray, "Tamano ROM: ");
            std::string romSzStr = "";
            if(s_metaLoaded && s_currentMeta.romSizeKB > 0) {
                int banks = s_currentMeta.romSizeKB / 16;
                romSzStr = std::to_string(s_currentMeta.romSizeKB) + " KB (" + std::to_string(banks) + " bancos)";
            } else {
                romSzStr = "Estandar";
            }
            uiDrawSysText(256.0f, 106.0f, 0.40f, 0.40f, colCyan, romSzStr.c_str());

            // Línea 5: Audio
            uiDrawSysText(172.0f, 122.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 122.0f, 0.40f, 0.40f, colGray, "Audio: ");
            uiDrawSysText(220.0f, 122.0f, 0.40f, 0.40f, colWhite, "Anillo Dual SPSC 32,728 Hz");

            // Línea 6: Motor
            uiDrawSysText(172.0f, 138.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 138.0f, 0.40f, 0.40f, colGray, "Motor: ");
            uiDrawSysText(220.0f, 138.0f, 0.40f, 0.40f, colGray, "ARM11 JIT 60 FPS");

            // Línea 7: Carátula / Scraping
            uiDrawSysText(172.0f, 154.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 154.0f, 0.40f, 0.40f, colGray, "Caratula: ");
            const char* scText = scraperGetStatusText();
            char statBuf[40];
            if(scText && scText[0] != '\0') snprintf(statBuf, sizeof(statBuf), "%s", scText);
            else snprintf(statBuf, sizeof(statBuf), "%s", s_boxartLoaded ? "Local SDMC OK" : "Pendiente");
            uiDrawSysText(234.0f, 154.0f, 0.40f, 0.40f, s_boxartLoaded ? colGreen : colYellow, statBuf);

            // Al pie de la tarjeta: Última partida
            std::string saveInfo = "Ultima partida: ";
            if(s_metaLoaded && s_currentMeta.hasSave) {
                saveInfo += s_currentMeta.saveDate;
            } else {
                saveInfo += "Ninguna";
            }
            uiDrawSysText(172.0f, 184.0f, 0.38f, 0.38f, colAmber, saveInfo.c_str());
        } else {
            uiDrawSysText(172.0f, 64.0f, 0.40f, 0.40f, colWhite, "Directorio SDMC");
            uiDrawSysText(172.0f, 84.0f, 0.40f, 0.40f, colCyan, "Seleccione una ROM para ver");
            uiDrawSysText(172.0f, 100.0f, 0.40f, 0.40f, colCyan, "su ficha tecnica completa.");
            uiDrawSysText(172.0f, 126.0f, 0.40f, 0.40f, colWhite, "Audio: Anillo Dual SPSC 32,728 Hz");
            uiDrawSysText(172.0f, 146.0f, 0.40f, 0.40f, colGray, "Motor: ARM11 JIT 60 FPS");
        }
    } else {
        // MODO SINOPSIS Y DATOS
        uiDrawSysText(172.0f, 38.0f, 0.44f, 0.44f, colAmber, "SINOPSIS Y DATOS");
        uiDrawRect(166.0f, 52.0f, 214.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            // Línea 1: Año y Género
            uiDrawSysText(172.0f, 58.0f, 0.40f, 0.40f, colAmber, "• ");
            char l1[32];
            if(s_metaLoaded && s_currentMeta.year > 0) {
                snprintf(l1, sizeof(l1), "Ano: %d | %s", s_currentMeta.year, s_currentMeta.genre.c_str());
            } else {
                snprintf(l1, sizeof(l1), "Genero: %s", s_metaLoaded ? s_currentMeta.genre.c_str() : "Clasico");
            }
            uiDrawSysText(182.0f, 58.0f, 0.40f, 0.40f, colWhite, l1);

            // Línea 2: Desarrollador
            uiDrawSysText(172.0f, 74.0f, 0.40f, 0.40f, colAmber, "• ");
            std::string dev = s_metaLoaded && !s_currentMeta.developer.empty() ? s_currentMeta.developer : (s_metaLoaded && !s_currentMeta.publisher.empty() ? s_currentMeta.publisher : "Desconocido");
            std::string devLine = "Dev: " + dev;
            uiDrawSysText(182.0f, 74.0f, 0.40f, 0.40f, colCyan, devLine.c_str());

            // Línea 3: Tiempo Jugado
            uiDrawSysText(172.0f, 90.0f, 0.40f, 0.40f, colAmber, "• ");
            uiDrawSysText(182.0f, 90.0f, 0.40f, 0.40f, colWhite, "Tiempo: Registrado");

            // Líneas 4..7: Sinopsis formateada
            std::string syn = s_metaLoaded && !s_currentMeta.synopsis.empty() ? s_currentMeta.synopsis : "Titulo clasico de Game Boy totalmente compatible con el nucleo GameYob.";
            std::vector<std::string> synLines = wrapSynopsisText(syn, 34);
            float sy = 110.0f;
            for(size_t i = 0; i < synLines.size() && i < 4; i++) {
                uiDrawSysText(172.0f, sy, 0.38f, 0.38f, colGray, synLines[i].c_str());
                sy += 16.0f;
            }

            // Estado al pie
            uiDrawSysText(172.0f, 184.0f, 0.40f, 0.40f, colGreen, "• Nucleo: GameYob 100% OK");
        } else {
            uiDrawSysText(172.0f, 64.0f, 0.40f, 0.40f, colWhite, "Directorio SDMC");
            uiDrawSysText(172.0f, 84.0f, 0.40f, 0.40f, colCyan, "Seleccione una ROM para ver");
            uiDrawSysText(172.0f, 100.0f, 0.40f, 0.40f, colCyan, "su sinopsis y datos.");
            uiDrawSysText(172.0f, 126.0f, 0.40f, 0.40f, colWhite, "Audio: Anillo Dual SPSC 32,728 Hz");
            uiDrawSysText(172.0f, 146.0f, 0.40f, 0.40f, colGray, "Motor: ARM11 JIT 60 FPS");
        }
    }

    // 6. Pie de Pantalla Superior (y: 221..237)
    uiDrawRect(10.0f, 221.0f, 380.0f, 16.0f, uiColorRGBA(10, 14, 20, 255));
    uiDrawRectOutline(10.0f, 221.0f, 380.0f, 16.0f, 1.0f, colAmber);

    const char* footerLegend = "[A] Iniciar Juego    [Y] Alternar Sinopsis/Ficha    [X] Alternar Arte 3D";
    float legW = uiGetSysTextWidth(0.40f, footerLegend);
    float lx = 10.0f + (380.0f - legW) * 0.5f;
    uiDrawSysText(lx, 223.0f, 0.40f, 0.40f, colWhite, footerLegend);

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
