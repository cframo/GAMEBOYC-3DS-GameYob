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

    color |= 0xFF000000;

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);

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

    color |= 0xFF000000;

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);

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

void uiDrawText(float x, float y, const char* str, u32 color) {
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
                case 0xA1: c = 'a'; break; // á
                case 0xA9: c = 'e'; break; // é
                case 0xAD: c = 'i'; break; // í
                case 0xB3: c = 'o'; break; // ó
                case 0xBA: c = 'u'; break; // ú
                case 0xB1: c = 'n'; break; // ñ
                case 0x81: c = 'A'; break; // Á
                case 0x89: c = 'E'; break; // É
                case 0x8D: c = 'I'; break; // Í
                case 0x93: c = 'O'; break; // Ó
                case 0x9A: c = 'U'; break; // Ú
                case 0x91: c = 'N'; break; // Ñ
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

void uiDrawBoxart3D(float centerX, float centerY, float size) {
    s_boxartAngleY += 0.02f;
    float rotY = sinf(s_boxartAngleY) * 28.0f * (M_PI / 180.0f);
    float rotX = -8.0f * (M_PI / 180.0f);

    C3D_Mtx modelView;
    Mtx_Identity(&modelView);
    Mtx_Translate(&modelView, centerX, centerY, 0.5f, true);
    Mtx_Scale(&modelView, 1.0f, 1.0f, 0.005f);
    Mtx_RotateX(&modelView, rotX, true);
    Mtx_RotateY(&modelView, rotY, true);

    const C3D_Mtx* proj = gfxGetProjectionBottom();
    int uLocProj = gfxGetULocProjection();

    C3D_Mtx mvp;
    Mtx_Multiply(&mvp, proj, &modelView);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProj, &mvp);

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
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

void uiDrawFileChooser(const FileChooser* chooser) {
    if(chooser == nullptr) {
        uiDrawWireframeTest();
        return;
    }

    const u32 colBg = uiColorRGBA(24, 26, 32, 255);
    const u32 colPanelBg = uiColorRGBA(18, 20, 26, 255);
    const u32 colBorder = uiColorRGBA(64, 80, 104, 255);
    const u32 colSelectedBg = uiColorRGBA(36, 54, 82, 255);
    const u32 colSelectedBorder = uiColorRGBA(70, 110, 160, 255);
    const u32 colDockBtnBg = uiColorRGBA(34, 40, 52, 255);

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colCyan = UI_COLOR_CYAN;
    const u32 colDarkGray = UI_COLOR_DARKGRAY;
    const u32 colPurple = UI_COLOR_PURPLE;

    // 1. Dark background
    uiDrawRect(0.0f, 0.0f, 320.0f, 240.0f, colBg);

    // 2. ROM list panel (x=4, y=4, w=184, h=208)
    uiDrawRect(4.0f, 4.0f, 184.0f, 208.0f, colPanelBg);
    uiDrawRectOutline(4.0f, 4.0f, 184.0f, 208.0f, 1.0f, colBorder);

    uiDrawText(8.0f, 8.0f, "ROM SELECTOR", colYellow);
    uiDrawRect(8.0f, 20.0f, 176.0f, 1.0f, colBorder);

    const std::vector<std::string>& files = chooser->getFilenames();
    const std::vector<int>& flags = chooser->getFlags();
    int numFiles = chooser->getNumFiles();
    int selection = chooser->getSelection();
    float scrollY = chooser->getSubpixelScrollY();

    int firstIdx = (int) (scrollY / 14.0f);
    if(firstIdx < 0) {
        firstIdx = 0;
    }

    for(int i = firstIdx; i < numFiles; i++) {
        float itemY = 26.0f + (float) i * 14.0f - scrollY;
        if(itemY < 12.0f) {
            continue;
        }
        if(itemY > 206.0f) {
            break;
        }

        bool isSelected = (i == selection);

        if(isSelected) {
            uiDrawRect(6.0f, itemY - 2.0f, 178.0f, 13.0f, colSelectedBg);
            uiDrawRectOutline(6.0f, itemY - 2.0f, 178.0f, 13.0f, 1.0f, colSelectedBorder);
        }

        std::string fileName = (i < (int) files.size()) ? files[i] : "";
        bool isDir = (i < (int) flags.size()) && ((flags[i] & 1) != 0);      // FLAG_DIRECTORY
        bool isSuspended = (i < (int) flags.size()) && ((flags[i] & 2) != 0);// FLAG_SUSPENDED

        if(isDir) {
            fileName += "/";
        }

        if(fileName.length() > 18) {
            fileName = fileName.substr(0, 17) + "~";
        }

        std::string displayLine = (isSelected ? "> " : "  ") + fileName;
        if(displayLine.length() > 21) {
            displayLine = displayLine.substr(0, 21);
        }

        u32 itemColor;
        if(isDir) {
            itemColor = colYellow;
        } else if(isSuspended) {
            itemColor = colPurple;
        } else if(isSelected) {
            itemColor = colWhite;
        } else {
            itemColor = colCyan;
        }

        uiDrawText(8.0f, itemY, displayLine.c_str(), itemColor);
    }

    if(numFiles == 0) {
        uiDrawText(8.0f, 30.0f, "No files found", colDarkGray);
    } else {
        // Indicador de barra de desplazamiento proporcional
        float maxScroll = chooser->getMaxScroll();
        if(maxScroll > 0.0f) {
            const float trackY = 24.0f;
            const float trackH = 184.0f;
            float totalH = (float) numFiles * 14.0f;
            float thumbH = trackH * (188.0f / totalH);
            if(thumbH < 12.0f) {
                thumbH = 12.0f;
            }
            float thumbY = trackY + (scrollY / maxScroll) * (trackH - thumbH);
            if(thumbY < trackY) {
                thumbY = trackY;
            }
            if(thumbY + thumbH > trackY + trackH) {
                thumbY = trackY + trackH - thumbH;
            }
            uiDrawRect(184.0f, thumbY, 2.0f, thumbH, colBorder);
        }
    }

    // Máscara superior para recortar limpiamente los elementos que se desplazan hacia arriba
    uiDrawRect(4.0f, 4.0f, 184.0f, 17.0f, colPanelBg);
    uiDrawText(8.0f, 8.0f, "ROM SELECTOR", colYellow);
    uiDrawRect(8.0f, 20.0f, 176.0f, 1.0f, colBorder);

    // 3. Preview panel (x=192, y=4, w=124, h=208)
    uiDrawRect(192.0f, 4.0f, 124.0f, 208.0f, colPanelBg);
    uiDrawRectOutline(192.0f, 4.0f, 124.0f, 208.0f, 1.0f, colBorder);

    std::string currentSelectedRom = "";
    if(selection >= 0 && selection < numFiles && selection < (int) files.size() && selection < (int) flags.size()) {
        bool isDir = (flags[selection] & 1) != 0;
        if(!isDir) {
            std::string dir = chooser->getDirectory();
            if(!dir.empty() && dir.back() != '/' && dir.back() != '\\') {
                dir += "/";
            }
            currentSelectedRom = dir + files[selection];
        }
    }

    const char* scStatus = scraperGetStatusText();
    if(scStatus != nullptr && scStatus[0] != '\0') {
        u32 scCol = UI_COLOR_CYAN;
        if(strncmp(scStatus, "E:", 2) == 0) {
            scCol = UI_COLOR_RGBA(255, 90, 90, 255);
        } else if(strcmp(scStatus, "DL:OK") == 0) {
            scCol = UI_COLOR_GREEN;
        }
        uiDrawText(196.0f, 8.0f, "PREVIEW", colYellow);
        uiDrawText(256.0f, 8.0f, scStatus, scCol);
    } else {
        uiDrawText(196.0f, 8.0f, "ROM PREVIEW", colYellow);
        if(scraperHasPendingCovers(currentSelectedRom)) {
            uiDrawText(284.0f, 8.0f, "[DL]", UI_COLOR_GREEN);
        }
    }
    uiDrawRect(196.0f, 20.0f, 116.0f, 1.0f, colBorder);

    uiDrawRectOutline(208.0f, 26.0f, 92.0f, 80.0f, 1.0f, colBorder);

    if(selection >= 0 && selection < numFiles && selection < (int) files.size() && selection < (int) flags.size()) {
        std::string selectedFile = files[selection];
        bool isDir = (flags[selection] & 1) != 0;
        bool isSuspended = (flags[selection] & 2) != 0;

        if(isDir) {
            uiInvalidateBoxart();
            uiDrawText(224.0f, 60.0f, "[Folder]", UI_COLOR_YELLOW);

            uiDrawText(196.0f, 114.0f, "Type: Folder", UI_COLOR_YELLOW);
            std::string dirTrunc = selectedFile;
            if(dirTrunc.length() > 14) {
                dirTrunc = dirTrunc.substr(0, 13) + "~";
            }
            uiDrawText(196.0f, 127.0f, dirTrunc.c_str(), UI_COLOR_WHITE);
            uiDrawText(196.0f, 196.0f, "A:Open", UI_COLOR_WHITE);
            uiDrawText(260.0f, 196.0f, "B:Back", UI_COLOR_GRAY);
        } else {
            std::string fullPath = currentSelectedRom;

            std::string::size_type dot = selectedFile.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? selectedFile.substr(dot + 1) : "";
            bool isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

            if(scraperCheckCoverReady(fullPath)) {
                uiInvalidateBoxart();
            }

            uiUpdateBoxart(fullPath.c_str(), nullptr, 0, isGBC);
            uiDrawBoxart3D(254.0f, 66.0f, 64.0f);

            // Línea 1 (y=114): Título limpio del juego (máx 14 chars)
            std::string dispTitle = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : selectedFile;
            if(dispTitle.length() > 14) {
                dispTitle = dispTitle.substr(0, 13) + "~";
            }
            uiDrawText(196.0f, 114.0f, dispTitle.c_str(), UI_COLOR_WHITE);

            // Línea 2 (y=127): Publisher y Región
            char line2[16];
            if(s_metaLoaded) {
                snprintf(line2, sizeof(line2), "%.9s (%.2s)", s_currentMeta.publisher.c_str(), s_currentMeta.region.c_str());
            } else {
                snprintf(line2, sizeof(line2), "Reading...");
            }
            line2[14] = '\0';
            uiDrawText(196.0f, 127.0f, line2, UI_COLOR_YELLOW);

            // Línea 3 (y=140): Año y Género
            char line3[16];
            if(s_metaLoaded) {
                if(s_currentMeta.year > 0) {
                    snprintf(line3, sizeof(line3), "%d | %.7s", s_currentMeta.year, s_currentMeta.genre.c_str());
                } else {
                    snprintf(line3, sizeof(line3), "---- | %.7s", s_currentMeta.genre.c_str());
                }
            } else {
                snprintf(line3, sizeof(line3), "---- | Clasico");
            }
            line3[14] = '\0';
            uiDrawText(196.0f, 140.0f, line3, UI_COLOR_GRAY);

            // Línea 4 (y=153): Hardware compacto
            char line4[16];
            if(s_metaLoaded) {
                std::string mbcShort = s_currentMeta.mbcName.substr(0, 4);
                int romM = s_currentMeta.romSizeKB >= 1024 ? s_currentMeta.romSizeKB / 1024 : s_currentMeta.romSizeKB;
                const char* unit = s_currentMeta.romSizeKB >= 1024 ? "M" : "K";
                snprintf(line4, sizeof(line4), "%s %d%s %s", mbcShort.c_str(), romM, unit, s_currentMeta.hasBattery ? "BAT" : "");
            } else {
                snprintf(line4, sizeof(line4), "%s", isGBC ? "CGB Hardware" : "DMG Hardware");
            }
            line4[14] = '\0';
            uiDrawText(196.0f, 153.0f, line4, UI_COLOR_CYAN);

            // Línea 5 (y=166): Savefile
            char line5[16];
            if(s_metaLoaded) {
                snprintf(line5, sizeof(line5), "Sav: %s", s_currentMeta.saveDate.c_str());
            } else {
                snprintf(line5, sizeof(line5), "Sav: ...");
            }
            line5[14] = '\0';
            uiDrawText(196.0f, 166.0f, line5, (s_metaLoaded && s_currentMeta.hasSave) ? UI_COLOR_GREEN : UI_COLOR_DARKGRAY);

            // Línea 6 (y=180): Tipo de ROM
            if(isSuspended) {
                uiDrawText(196.0f, 180.0f, "State: Saved", UI_COLOR_PURPLE);
            } else if(s_metaLoaded ? s_currentMeta.isGBC : isGBC) {
                uiDrawText(196.0f, 180.0f, "Type: GBC ROM", UI_COLOR_YELLOW);
            } else if(s_metaLoaded && s_currentMeta.isSGB) {
                uiDrawText(196.0f, 180.0f, "Type: SGB ROM", UI_COLOR_YELLOW);
            } else {
                uiDrawText(196.0f, 180.0f, "Type: DMG ROM", UI_COLOR_GRAY);
            }

            // Línea 7 (y=196): "A:Play  B:Back"
            uiDrawText(196.0f, 196.0f, "A:Play", UI_COLOR_WHITE);
            uiDrawText(260.0f, 196.0f, "B:Back", UI_COLOR_GRAY);
        }
    } else {
        uiInvalidateBoxart();
        uiDrawText(220.0f, 60.0f, "[No Cover]", UI_COLOR_DARKGRAY);
        uiDrawText(196.0f, 114.0f, "No selection", UI_COLOR_DARKGRAY);
        uiDrawText(196.0f, 196.0f, "A:Play", UI_COLOR_WHITE);
        uiDrawText(260.0f, 196.0f, "B:Back", UI_COLOR_GRAY);
    }

    // 4. Bottom dock (4 buttons from x=4..316, y=216..236)
    const float btnW = 75.0f;
    const float btnH = 20.0f;
    const float btnY = 216.0f;

    const float btnX[4] = { 4.0f, 83.0f, 162.0f, 241.0f };
    const char* btnLabels[4] = { "FILES", "CONFIG", "CHEATS", "RESET" };
    const float labelOffsetsX[4] = { 21.0f, 98.0f, 177.0f, 261.0f };

    for(int i = 0; i < 4; i++) {
        uiDrawRect(btnX[i], btnY, btnW, btnH, colDockBtnBg);
        uiDrawRectOutline(btnX[i], btnY, btnW, btnH, 1.0f, colBorder);
        uiDrawText(labelOffsetsX[i], btnY + 6.0f, btnLabels[i], colWhite);
    }

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

void uiDrawTopScreenHub() {
    const u32 colBg = uiColorRGBA(24, 26, 32, 255);
    const u32 colAmber = uiColorRGBA(196, 148, 56, 255);
    const u32 colPanelBlue = uiColorRGBA(16, 26, 44, 255);
    const u32 colHeaderBlue = uiColorRGBA(24, 38, 64, 255);

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colGray = UI_COLOR_GRAY;

    // 1. Fondo oscuro de interfaz
    uiDrawRect(0.0f, 0.0f, 400.0f, 240.0f, colBg);

    // 2. Barra de estado superior (y: 4..18)
    uiDrawText(14.0f, 6.0f, "GAMEYOB 3DS", colYellow);
    uiDrawText(156.0f, 6.0f, "GAME HUB", colGray);

    time_t rawtime;
    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);
    char timeStr[16];
    if(timeinfo != nullptr) {
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
    } else {
        snprintf(timeStr, sizeof(timeStr), "--:--");
    }
    uiDrawText(310.0f, 6.0f, timeStr, colWhite);

    // Batería en esquina derecha (x: 356..382, y: 5..15)
    uiDrawRectOutline(356.0f, 5.0f, 22.0f, 10.0f, 1.0f, colWhite);
    uiDrawRect(378.0f, 7.0f, 2.0f, 6.0f, colWhite);
    u8 batLevel = getBatteryLevel();
    u32 batCol = (batLevel <= 1) ? UI_COLOR_RGBA(255, 60, 60, 255) : (batLevel <= 2 ? colYellow : UI_COLOR_GREEN);
    float fillW = ((float)batLevel / 5.0f) * 18.0f;
    if(fillW > 0.0f) {
        uiDrawRect(358.0f, 7.0f, fillW, 6.0f, batCol);
    }

    // 3. Marco decorativo principal (x: 12..388, y: 20..220)
    // Rectángulo exterior dorado/ámbar (RGB: 196, 148, 56) de 1px
    uiDrawRectOutline(12.0f, 20.0f, 376.0f, 200.0f, 1.0f, colAmber);
    // Rectángulo interior azul oscuro (RGB: 16, 26, 44)
    uiDrawRect(13.0f, 21.0f, 374.0f, 198.0f, colPanelBlue);

    // Cabecera del panel interior
    uiDrawRect(13.0f, 21.0f, 374.0f, 22.0f, colHeaderBlue);
    uiDrawRect(13.0f, 43.0f, 374.0f, 1.0f, colAmber);

    // Separador vertical entre Lado Izquierdo y Lado Derecho
    uiDrawRect(164.0f, 44.0f, 1.0f, 175.0f, colAmber);

    // 4. Obtener selección actual desde FileChooser
    FileChooser* chooser = getActiveFileChooser();
    bool isDir = false;
    std::string selectedFile = "";
    std::string fullPath = "";

    if(chooser != nullptr) {
        const std::vector<std::string>& files = chooser->getFilenames();
        const std::vector<int>& flags = chooser->getFlags();
        int numFiles = chooser->getNumFiles();
        int selection = chooser->getSelection();

        if(selection >= 0 && selection < numFiles && selection < (int) files.size() && selection < (int) flags.size()) {
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

    // Cabecera: Título o Carpeta
    if(isDir) {
        std::string dirTitle = "[Carpeta] " + selectedFile;
        if(dirTitle.length() > 44) dirTitle = dirTitle.substr(0, 43) + "~";
        uiDrawText(20.0f, 28.0f, dirTitle.c_str(), colYellow);
    } else if(!selectedFile.empty()) {
        std::string headerTitle = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : selectedFile;
        if(s_metaLoaded) {
            if(!s_currentMeta.region.empty() && s_currentMeta.region != "Unknown") {
                headerTitle += " (" + s_currentMeta.region + ")";
            }
            if(s_currentMeta.isGBC) headerTitle += " [CGB]";
            else if(s_currentMeta.isSGB) headerTitle += " [SGB]";
            else headerTitle += " [DMG]";
        }
        if(headerTitle.length() > 44) headerTitle = headerTitle.substr(0, 43) + "~";
        uiDrawText(20.0f, 28.0f, headerTitle.c_str(), colWhite);
    } else {
        uiDrawText(20.0f, 28.0f, "GameYob 3DS - Hub de Compatibilidad", colGray);
    }

    // 5. Lado Izquierdo (x: 22..156, y: 50..212)
    if(isDir) {
        uiDrawRect(22.0f, 50.0f, 134.0f, 162.0f, uiColorRGBA(20, 30, 50, 255));
        uiDrawRectOutline(22.0f, 50.0f, 134.0f, 162.0f, 1.0f, uiColorRGBA(48, 68, 100, 255));

        uiDrawText(48.0f, 96.0f, "[CARPETA]", colYellow);
        uiDrawText(32.0f, 118.0f, "Subdirectorio SDMC", colGray);
        uiDrawText(42.0f, 140.0f, "A: Abrir / Ver", colWhite);
    } else if(!selectedFile.empty()) {
        uiDrawRect(22.0f, 50.0f, 134.0f, 162.0f, uiColorRGBA(12, 18, 30, 255));
        uiDrawRectOutline(22.0f, 50.0f, 134.0f, 162.0f, 1.0f, uiColorRGBA(64, 80, 104, 255));

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
            emitUiQuad(29.0f, 56.0f, 149.0f, 176.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            C3D_ImmDrawEnd();

            std::string pubStr = s_metaLoaded ? s_currentMeta.publisher : "Nintendo";
            if(pubStr.length() > 15) pubStr = pubStr.substr(0, 14) + "~";
            uiDrawText(26.0f, 182.0f, pubStr.c_str(), colWhite);

            char yrGen[32];
            if(s_metaLoaded && s_currentMeta.year > 0) {
                snprintf(yrGen, sizeof(yrGen), "%d %.9s", s_currentMeta.year, s_currentMeta.genre.c_str());
            } else {
                snprintf(yrGen, sizeof(yrGen), "Clasico GameBoy");
            }
            uiDrawText(26.0f, 196.0f, yrGen, colYellow);
        } else {
            bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
            u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
            u32 colCartBorder = isGBC ? uiColorRGBA(85, 95, 120, 255) : uiColorRGBA(190, 190, 190, 255);

            uiDrawRect(36.0f, 58.0f, 106.0f, 118.0f, colCart);
            uiDrawRectOutline(36.0f, 58.0f, 106.0f, 118.0f, 1.0f, colCartBorder);

            // Muesca superior
            uiDrawRect(36.0f, 58.0f, 24.0f, 10.0f, uiColorRGBA(12, 18, 30, 255));

            // Etiqueta del cartucho
            uiDrawRect(44.0f, 80.0f, 90.0f, 86.0f, uiColorRGBA(24, 34, 52, 255));
            uiDrawRectOutline(44.0f, 80.0f, 90.0f, 86.0f, 1.0f, colAmber);

            uiDrawText(48.0f, 90.0f, "GAME BOY", colWhite);
            if(isGBC) {
                uiDrawText(48.0f, 102.0f, " COLOR", UI_COLOR_CYAN);
            }

            std::string miniTitle = s_metaLoaded ? s_currentMeta.title : selectedFile;
            if(miniTitle.length() > 10) miniTitle = miniTitle.substr(0, 9) + "~";
            uiDrawText(48.0f, 124.0f, miniTitle.c_str(), colYellow);

            std::string pubStr = s_metaLoaded ? s_currentMeta.publisher : "Nintendo";
            if(pubStr.length() > 15) pubStr = pubStr.substr(0, 14) + "~";
            uiDrawText(26.0f, 182.0f, pubStr.c_str(), colWhite);

            char yrGen[32];
            if(s_metaLoaded && s_currentMeta.year > 0) {
                snprintf(yrGen, sizeof(yrGen), "%d %.9s", s_currentMeta.year, s_currentMeta.genre.c_str());
            } else {
                snprintf(yrGen, sizeof(yrGen), "Clasico GameBoy");
            }
            uiDrawText(26.0f, 196.0f, yrGen, colYellow);
        }
    } else {
        uiDrawRect(22.0f, 50.0f, 134.0f, 162.0f, uiColorRGBA(20, 26, 38, 255));
        uiDrawRectOutline(22.0f, 50.0f, 134.0f, 162.0f, 1.0f, uiColorRGBA(48, 60, 84, 255));
        uiDrawText(42.0f, 118.0f, "Sin seleccion", colGray);
    }

    // 6. Lado Derecho: Ficha de Compatibilidad (x: 172..384, y: 50..214)
    uiDrawText(172.0f, 52.0f, "FICHA DE COMPATIBILIDAD", colYellow);
    uiDrawRect(172.0f, 65.0f, 204.0f, 1.0f, colAmber);

    if(!selectedFile.empty() && !isDir) {
        bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);

        // Línea 1: Modo Nativo
        std::string modoStr = "- Modo: ";
        if(s_metaLoaded ? s_currentMeta.isGBC : isGBC) modoStr += "GB Color (CGB)";
        else if(s_metaLoaded && s_currentMeta.isSGB) modoStr += "Super GB (SGB)";
        else modoStr += "Game Boy (DMG)";
        uiDrawText(172.0f, 74.0f, modoStr.c_str(), colWhite);

        // Línea 2: Mapper
        std::string mapStr = "- Mapper: ";
        if(s_metaLoaded && !s_currentMeta.mbcName.empty()) {
            mapStr += s_currentMeta.mbcName;
            if(s_currentMeta.mbcName.find("MBC3") != std::string::npos) mapStr += " (RTC)";
        } else {
            mapStr += "MBC Auto";
        }
        uiDrawText(172.0f, 90.0f, mapStr.c_str(), UI_COLOR_CYAN);

        // Línea 3: Guardado
        std::string savStr = "- Guardado: ";
        if(s_metaLoaded) {
            if(s_currentMeta.ramSizeKB > 0) {
                savStr += "SRAM " + std::to_string(s_currentMeta.ramSizeKB) + "KB";
                if(s_currentMeta.hasBattery) savStr += "+Bat";
            } else if(s_currentMeta.hasBattery) {
                savStr += "Bateria (Sin RAM)";
            } else {
                savStr += "Sin guardado";
            }
        } else {
            savStr += "SRAM / Flash";
        }
        uiDrawText(172.0f, 106.0f, savStr.c_str(), colWhite);

        // Línea 4: Tamaño ROM
        std::string romSzStr = "- ROM: ";
        if(s_metaLoaded && s_currentMeta.romSizeKB > 0) {
            int banks = s_currentMeta.romSizeKB / 16;
            romSzStr += std::to_string(s_currentMeta.romSizeKB) + " KB (" + std::to_string(banks) + " b)";
        } else {
            romSzStr += "Estandar";
        }
        uiDrawText(172.0f, 122.0f, romSzStr.c_str(), UI_COLOR_CYAN);

        // Línea 5: Audio
        uiDrawText(172.0f, 138.0f, "- Audio: SPSC 32,728 Hz", colWhite);

        // Línea 6: Última partida
        std::string saveInfo = "- Partida: ";
        if(s_metaLoaded && s_currentMeta.hasSave) {
            saveInfo += s_currentMeta.saveDate;
        } else {
            saveInfo += "Ninguna";
        }
        uiDrawText(172.0f, 154.0f, saveInfo.c_str(), (s_metaLoaded && s_currentMeta.hasSave) ? UI_COLOR_GREEN : UI_COLOR_DARKGRAY);

        // Línea 7: Motor
        uiDrawText(172.0f, 170.0f, "- Motor: ARM11 JIT 60 FPS", colGray);

        // Línea 8: Carátula / Scraping
        const char* scText = scraperGetStatusText();
        char statBuf[40];
        if(scText && scText[0] != '\0') snprintf(statBuf, sizeof(statBuf), "- Scraper: %s", scText);
        else snprintf(statBuf, sizeof(statBuf), "- Caratula: %s", s_boxartLoaded ? "Local SDMC OK" : "Pendiente");
        uiDrawText(172.0f, 186.0f, statBuf, s_boxartLoaded ? UI_COLOR_GREEN : colYellow);
    } else {
        uiDrawText(172.0f, 74.0f, "- Directorio SDMC", colWhite);
        uiDrawText(172.0f, 90.0f, "- Seleccione una ROM para", UI_COLOR_CYAN);
        uiDrawText(172.0f, 106.0f, "  ver su ficha tecnica.", UI_COLOR_CYAN);
        uiDrawText(172.0f, 138.0f, "- Audio: SPSC 32,728 Hz", colWhite);
        uiDrawText(172.0f, 170.0f, "- Motor: ARM11 JIT 60 FPS", colGray);
    }

    // 7. Pie de pantalla superior (y: 222..234)
    uiDrawText(24.0f, 224.0f, "[A] Iniciar  [Y] Cargar State  [SELECT] Menu", colWhite);

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
