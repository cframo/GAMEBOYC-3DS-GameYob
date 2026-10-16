#ifdef BACKEND_3DS

#include <string.h>
#include <string>
#include <vector>

#include <3ds.h>
#include <citro3d.h>

#include "platform/common/menu/filechooser.h"
#include "platform/common/manager.h"
#include "platform/3ds/ui_renderer.h"

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
}

void uiRenderExit() {
    if(s_fontInit) {
        C3D_TexDelete(&s_fontTex);
        s_fontInit = false;
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
    for(const char* p = str; *p != '\0' && len < 28; p++, len++) {
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
    for(const char* p = str; *p != '\0' && len < 28; p++, len++) {
        u8 c = (u8)*p;
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

void uiDrawWireframeTest() {
    const u32 colBg = uiColorRGBA(24, 26, 32, 255);
    const u32 colPanelBg = uiColorRGBA(18, 20, 26, 255);
    const u32 colBorder = uiColorRGBA(64, 80, 104, 255);
    const u32 colDockBtnBg = uiColorRGBA(34, 40, 52, 255);

    const u32 colYellow = UI_COLOR_YELLOW;
    const u32 colWhite = UI_COLOR_WHITE;
    const u32 colGray = UI_COLOR_GRAY;
    const u32 colPurple = UI_COLOR_PURPLE;

    // 1. Dark background
    uiDrawRect(0.0f, 0.0f, 320.0f, 240.0f, colBg);

    // 2. Pause container panel (x=8, y=6, w=304, h=202)
    uiDrawRect(8.0f, 6.0f, 304.0f, 202.0f, colPanelBg);
    uiDrawRectOutline(8.0f, 6.0f, 304.0f, 202.0f, 1.0f, colBorder);

    // Header
    uiDrawText(116.0f, 16.0f, "GAME PAUSED", colYellow);
    uiDrawRect(16.0f, 30.0f, 288.0f, 1.0f, colBorder);

    // Current ROM info
    std::string currentRom = mgrGetRomName();
    if(currentRom.empty()) {
        currentRom = "No ROM loaded";
    } else {
        std::string::size_type slash = currentRom.find_last_of("/\\");
        if(slash != std::string::npos) {
            currentRom = currentRom.substr(slash + 1);
        }
    }
    if(currentRom.length() > 28) {
        currentRom = currentRom.substr(0, 27) + "~";
    }
    uiDrawText(20.0f, 40.0f, "Current ROM:", colGray);
    uiDrawText(20.0f, 54.0f, currentRom.c_str(), colWhite);

    uiDrawRect(16.0f, 72.0f, 288.0f, 1.0f, colBorder);

    // Instructions
    uiDrawText(20.0f, 86.0f, "B / R / Touch: Resume Game", colWhite);
    uiDrawText(20.0f, 108.0f, "X / [FILES]  : Change ROM", colYellow);
    uiDrawText(20.0f, 130.0f, "Y / [RESET]  : Reset Game", colPurple);

    uiDrawText(20.0f, 166.0f, "Tap screen or press B/R to resume", colGray);

    // 3. Bottom dock (4 buttons from x=4..316, y=216..236)
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

    // Reset TexEnv to standard state
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
    const u32 colGray = UI_COLOR_GRAY;
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

    uiDrawText(196.0f, 8.0f, "ROM PREVIEW", colYellow);
    uiDrawRect(196.0f, 20.0f, 116.0f, 1.0f, colBorder);

    uiDrawRectOutline(208.0f, 26.0f, 92.0f, 80.0f, 1.0f, colBorder);
    uiDrawText(220.0f, 62.0f, "[No Cover]", colDarkGray);

    if(selection >= 0 && selection < numFiles && selection < (int) files.size() && selection < (int) flags.size()) {
        std::string selectedFile = files[selection];
        bool isDir = (flags[selection] & 1) != 0;
        bool isSuspended = (flags[selection] & 2) != 0;

        if(isDir) {
            uiDrawText(196.0f, 116.0f, "Type: Folder", colYellow);
            std::string dirTrunc = selectedFile;
            if(dirTrunc.length() > 13) {
                dirTrunc = dirTrunc.substr(0, 12) + "~";
            }
            uiDrawText(196.0f, 130.0f, dirTrunc.c_str(), colWhite);
        } else {
            std::string nameTrunc = selectedFile;
            if(nameTrunc.length() > 13) {
                nameTrunc = nameTrunc.substr(0, 12) + "~";
            }
            uiDrawText(196.0f, 116.0f, nameTrunc.c_str(), colWhite);

            std::string::size_type dot = selectedFile.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? selectedFile.substr(dot + 1) : "";
            if(strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0) {
                uiDrawText(196.0f, 134.0f, "Type: GBC ROM", colYellow);
            } else if(strcasecmp(ext.c_str(), "gb") == 0) {
                uiDrawText(196.0f, 134.0f, "Type: DMG ROM", colGray);
            } else if(strcasecmp(ext.c_str(), "sgb") == 0) {
                uiDrawText(196.0f, 134.0f, "Type: SGB ROM", colYellow);
            } else {
                uiDrawText(196.0f, 134.0f, "Type: File", colGray);
            }

            if(isSuspended) {
                uiDrawText(196.0f, 152.0f, "State: Saved", colPurple);
            }
        }
    } else {
        uiDrawText(196.0f, 116.0f, "No selection", colDarkGray);
    }

    uiDrawText(196.0f, 176.0f, "A: Launch", colWhite);
    uiDrawText(196.0f, 192.0f, "B: Back", colGray);

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

#endif
