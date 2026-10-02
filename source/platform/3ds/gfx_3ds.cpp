#ifdef BACKEND_3DS

#include <malloc.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <sstream>

#include <3ds.h>
#include <citro3d.h>

#include "platform/3ds/default_shbin.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "platform/system.h"

static bool c3dInitialized;

static bool shaderInitialized;
static DVLB_s* dvlb;
static shaderProgram_s program;

static C3D_RenderTarget* targetTop;
static C3D_RenderTarget* targetBottom;
static C3D_Mtx projectionTop;
static C3D_Mtx projectionBottom;

static bool screenInit;
static C3D_Tex screenTexture;

static bool borderInit;
static u16 borderWidth;
static u16 borderHeight;
static u16 gpuBorderWidth;
static u16 gpuBorderHeight;
static C3D_Tex borderTexture;

static bool lcdGridInit = false;
static C3D_Tex lcdGridTexture;
extern int lcdGrid;

static u32* screenBuffer;

static bool gfxInitialized = false;

bool gfxInit() {
    gfxInitDefault();
    gfxInitialized = true;

    if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxCleanup();
        return false;
    }

    c3dInitialized = true;

    u32 displayFlags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    targetTop = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, 0);
    if(targetTop == NULL) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(targetTop, GFX_TOP, GFX_LEFT, displayFlags);

    targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, 0);
    if(targetBottom == NULL) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(targetBottom, GFX_BOTTOM, GFX_LEFT, displayFlags);

    dvlb = DVLB_ParseFile((u32*) default_shbin, default_shbin_len);
    if(dvlb == NULL) {
        gfxCleanup();
        return false;
    }

    Result progInitRes = shaderProgramInit(&program);
    if(R_FAILED(progInitRes)) {
        gfxCleanup();
        return false;
    }

    shaderInitialized = true;

    Result progSetVshRes = shaderProgramSetVsh(&program, &dvlb->DVLE[0]);
    if(R_FAILED(progSetVshRes)) {
        gfxCleanup();
        return false;
    }

    C3D_BindProgram(&program);

    C3D_AttrInfo* attrInfo = C3D_GetAttrInfo();
    if(attrInfo == NULL) {
        gfxCleanup();
        return false;
    }

    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2);

    C3D_TexEnv* env = C3D_GetTexEnv(0);
    if(env == NULL) {
        gfxCleanup();
        return false;
    }

    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    C3D_DepthTest(true, GPU_GEQUAL, GPU_WRITE_ALL);

    Mtx_OrthoTilt(&projectionTop, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0, true);
    Mtx_OrthoTilt(&projectionBottom, 0.0, 320.0, 240.0, 0.0, 0.0, 1.0, true);

    screenInit = false;
    borderInit = false;

    // Allocate and clear the screen buffer.
    screenBuffer = (u32*) linearAlloc(256 * 256 * sizeof(u32));
    memset(screenBuffer, 0, 256 * 256 * sizeof(u32));

    lcdGridInit = false;
    if(C3D_TexInit(&lcdGridTexture, 8, 8, GPU_RGBA8)) {
        lcdGridInit = true;
        u32* dst = (u32*) lcdGridTexture.data;
        for(int y = 0; y < 8; y++) {
            for(int x = 0; x < 8; x++) {
                u32 morton = ((y & 1) << 0) | ((x & 1) << 1) |
                             ((y & 2) << 1) | ((x & 2) << 2) |
                             ((y & 4) << 2) | ((x & 4) << 3);

                if(x == 7 && y == 7) {
                    // Esquinas: bisel tenue (~81% de brillo)
                    dst[morton] = 0xFFD0D0D0;
                } else if(x == 7 || y == 7) {
                    // Borde perimetral de 1 texel: sombra de fondo (~92% de brillo)
                    dst[morton] = 0xFFEAEAEA;
                } else {
                    // Núcleo: blanco puro reflectivo (100%)
                    dst[morton] = 0xFFFFFFFF;
                }
            }
        }
        GSPGPU_FlushDataCache(lcdGridTexture.data, 8 * 8 * sizeof(u32));
        C3D_TexSetWrap(&lcdGridTexture, GPU_REPEAT, GPU_REPEAT);
        C3D_TexSetFilter(&lcdGridTexture, GPU_LINEAR, GPU_LINEAR);
    }

    return true;
}

void gfxCleanup() {
    if(lcdGridInit) {
        C3D_TexDelete(&lcdGridTexture);
        lcdGridInit = false;
    }

    if(screenBuffer != nullptr) {
        linearFree(screenBuffer);
        screenBuffer = nullptr;
    }

    if(borderInit) {
        C3D_TexDelete(&borderTexture);
        borderInit = false;
    }

    if(screenInit) {
        C3D_TexDelete(&screenTexture);
        screenInit = false;
    }

    if(shaderInitialized) {
        shaderProgramFree(&program);
        shaderInitialized = false;
    }

    if(dvlb != nullptr) {
        DVLB_Free(dvlb);
        dvlb = nullptr;
    }

    if(targetTop != nullptr) {
        C3D_RenderTargetDelete(targetTop);
        targetTop = nullptr;
    }

    if(targetBottom != nullptr) {
        C3D_RenderTargetDelete(targetBottom);
        targetBottom = nullptr;
    }

    if(c3dInitialized) {
        C3D_Fini();
        c3dInitialized = false;
    }

    if(gfxInitialized) {
        gfxExit();
        gfxInitialized = false;
    }
}

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(imgData == NULL || (borderInit && (borderWidth != (u16) imgWidth || borderHeight != (u16) imgHeight))) {
        if(borderInit) {
            C3D_TexDelete(&borderTexture);
            borderInit = false;
        }

        borderWidth = 0;
        borderHeight = 0;
        gpuBorderWidth = 0;
        gpuBorderHeight = 0;

        if(imgData == NULL) {
            return;
        }
    }

    // Adjust the texture to power-of-two dimensions.
    borderWidth = (u16) imgWidth;
    borderHeight = (u16) imgHeight;
    gpuBorderWidth = (u16) pow(2, ceil(log(borderWidth) / log(2)));
    gpuBorderHeight = (u16) pow(2, ceil(log(borderHeight) / log(2)));

    // Create the texture.
    if(!borderInit && !C3D_TexInit(&borderTexture, gpuBorderWidth, gpuBorderHeight, GPU_RGBA8)) {
        return;
    }

    C3D_TexSetFilter(&borderTexture, GPU_LINEAR, GPU_LINEAR);

    // Copy the texture to a power-of-two sized buffer.
    u32* imgBuffer = (u32*) imgData;
    u32* temp = (u32*) linearAlloc(gpuBorderWidth * gpuBorderHeight * sizeof(u32));
    for(int x = 0; x < borderWidth; x++) {
        for(int y = 0; y < borderHeight; y++) {
            temp[y * gpuBorderWidth + x] = imgBuffer[y * borderWidth + x];
        }
    }

    GSPGPU_FlushDataCache(temp, gpuBorderWidth * gpuBorderHeight * sizeof(u32));
    C3D_SyncDisplayTransfer(temp, (u32) GX_BUFFER_DIM(gpuBorderWidth, gpuBorderHeight), (u32*) borderTexture.data, (u32) GX_BUFFER_DIM(gpuBorderWidth, gpuBorderHeight), GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    linearFree(temp);

    GSPGPU_InvalidateDataCache(borderTexture.data, borderTexture.size);

    borderInit = true;
}

u32* gfxGetScreenBuffer() {
    return screenBuffer;
}

u32 gfxGetScreenPitch() {
    return 256;
}

void gfxTakeScreenshot() {
    gfxScreen_t screen = gameScreen == 0 ? GFX_TOP : GFX_BOTTOM;
    if(gfxGetScreenFormat(screen) != GSP_BGR8_OES) {
        return;
    }

    u16 width = 0;
    u16 height = 0;
    u8* fb = gfxGetFramebuffer(screen, GFX_LEFT, &height, &width);

    if(fb == NULL) {
        return;
    }

    u32 headerSize = 0x36;
    u32 imageSize = (u32) (width * height * 3);

    u8* header = new u8[headerSize]();

    *(u16*) &header[0x0] = 0x4D42;
    *(u32*) &header[0x2] = headerSize + imageSize;
    *(u32*) &header[0xA] = headerSize;
    *(u32*) &header[0xE] = 0x28;
    *(u32*) &header[0x12] = width;
    *(u32*) &header[0x16] = height;
    *(u32*) &header[0x1A] = 0x00180001;
    *(u32*) &header[0x22] = imageSize;

    u8* image = new u8[imageSize]();

    for(u32 x = 0; x < width; x++) {
        for(u32 y = 0; y < height; y++) {
            u8* src = &fb[(x * height + y) * 3];
            u8* dst = &image[(y * width + x) * 3];

            *(u16*) dst = *(u16*) src;
            dst[2] = src[2];
        }
    }

    std::stringstream fileStream;
    fileStream << "/gameyob_" << time(NULL) << ".bmp";

    std::ofstream stream(fileStream.str(), std::ios::binary);
    if(stream.is_open()) {
        stream.write((char*) header, (size_t) headerSize);
        stream.write((char*) image, (size_t) imageSize);
        stream.close();
    } else {
        systemPrintDebug("Failed to open screenshot file: %s\n", strerror(errno));
    }

    delete[] header;
    delete[] image;
}

void gfxDrawScreen() {
    const int screenTexSize = 256;
    u32* transferBuffer = screenBuffer;
    GPU_TEXTURE_FILTER_PARAM filter = GPU_NEAREST;

    if(scaleMode != 0 && scaleFilter == 1) {
        filter = GPU_LINEAR;
    }

    if(!screenInit || screenTexture.width != screenTexSize || screenTexture.height != screenTexSize) {
        if(screenInit) {
            C3D_TexDelete(&screenTexture);
            screenInit = false;
        }

        screenInit = C3D_TexInit(&screenTexture, screenTexSize, screenTexSize, GPU_RGBA8);
    }

    C3D_TexSetFilter(&screenTexture, filter, filter);

    GSPGPU_FlushDataCache(transferBuffer, screenTexSize * screenTexSize * sizeof(u32));
    C3D_SyncDisplayTransfer(transferBuffer, (u32) GX_BUFFER_DIM(screenTexSize, screenTexSize), (u32*) screenTexture.data, (u32) GX_BUFFER_DIM(screenTexSize, screenTexSize), GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    GSPGPU_InvalidateDataCache(screenTexture.data, screenTexture.size);

    if(!C3D_FrameBegin(0)) {
        return;
    }

    C3D_RenderTarget* target = gameScreen == 0 ? targetTop : targetBottom;
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0);

    C3D_FrameDrawOn(target);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, shaderInstanceGetUniformLocation(program.vertexShader, "projection"), gameScreen == 0 ? &projectionTop : &projectionBottom);

    u16 viewportWidth = target->frameBuf.height;
    u16 viewportHeight = target->frameBuf.width;

    bool lcdActive = (lcdGrid == 1 && lcdGridInit);

    // Draw the screen.
    if(screenInit) {
        bool hasCustomBorder = customBordersEnabled && borderInit;

        float screenWidth = 0.0f;
        float screenHeight = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        float u2 = 0.0f;
        float v2 = 0.0f;

        if(hasCustomBorder) {
            screenWidth = 256.0f;
            screenHeight = 224.0f;
            if(borderScaleMode == 1) {
                if(scaleMode == 1) {
                    screenWidth *= 1.25f;
                    screenHeight *= 1.25f;
                } else if(scaleMode == 2) {
                    screenWidth *= 1.50f;
                    screenHeight *= 1.50f;
                } else if(scaleMode == 3) {
                    screenWidth *= (float) viewportHeight / 224.0f;
                    screenHeight = (float) viewportHeight;
                } else if(scaleMode == 4) {
                    screenWidth = (float) viewportWidth;
                    screenHeight = (float) viewportHeight;
                }
            }

            u1 = 0.0f;
            v1 = 0.0f;
            u2 = 256.0f / 256.0f;
            v2 = 224.0f / 256.0f;

            if(scaleMode != 0 && scaleFilter == 1) {
                const float baseFilterMod = 0.25f / (float) screenTexSize;
                u2 -= baseFilterMod;
                v2 -= baseFilterMod;
            }
        } else {
            screenWidth = 160.0f;
            screenHeight = 144.0f;
            if(scaleMode == 1) {
                screenWidth *= 1.25f;
                screenHeight *= 1.25f;
            } else if(scaleMode == 2) {
                screenWidth *= 1.50f;
                screenHeight *= 1.50f;
            } else if(scaleMode == 3) {
                screenHeight = (float) viewportHeight;
                screenWidth = screenHeight * (160.0f / 144.0f);
            } else if(scaleMode == 4) {
                screenWidth = (float) viewportWidth;
                screenHeight = (float) viewportHeight;
            }

            u1 = 48.0f / 256.0f;
            v1 = 40.0f / 256.0f;
            u2 = 208.0f / 256.0f;
            v2 = 184.0f / 256.0f;

            if(scaleMode != 0 && scaleFilter == 1) {
                const float baseFilterMod = 0.25f / (float) screenTexSize;
                u1 += baseFilterMod;
                v1 += baseFilterMod;
                u2 -= baseFilterMod;
                v2 -= baseFilterMod;
            }
        }

        const float gu1 = 0.0f;
        const float gv1 = 0.0f;
        const float gu2 = hasCustomBorder ? 256.0f : 160.0f;
        const float gv2 = hasCustomBorder ? 224.0f : 144.0f;

        // Calculate VBO points.
        const float x1 = ((float) viewportWidth - screenWidth) / 2.0f;
        const float y1 = ((float) viewportHeight - screenHeight) / 2.0f;
        const float x2 = x1 + screenWidth;
        const float y2 = y1 + screenHeight;

        C3D_TexBind(0, &screenTexture);
        C3D_TexEnv* env0 = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env0);
        C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
        C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);

        if(lcdActive) {
            C3D_TexBind(1, &lcdGridTexture);
            C3D_TexEnv* env1 = C3D_GetTexEnv(1);
            C3D_TexEnvInit(env1);
            C3D_TexEnvSrc(env1, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE1, (GPU_TEVSRC)0);
            C3D_TexEnvFunc(env1, C3D_Both, GPU_MODULATE);
        } else {
            C3D_TexEnvInit(C3D_GetTexEnv(1));
        }

        C3D_ImmDrawBegin(GPU_TRIANGLES);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu1, gv1, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u2, v2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu2, gv2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u2, v1, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu2, gv1, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u1, v1, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu1, gv1, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u1, v2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu1, gv2, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(u2, v2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(gu2, gv2, 0.0f, 0.0f);

        C3D_ImmDrawEnd();
    }

    // Draw the border.
    if(borderInit && customBordersEnabled && scaleMode != 4) {
        C3D_TexEnvInit(C3D_GetTexEnv(1));

        // Calculate VBO points.
        int scaledBorderWidth = borderWidth;
        int scaledBorderHeight = borderHeight;
        if(borderScaleMode == 1) {
            if(scaleMode == 1) {
                scaledBorderWidth *= 1.25f;
                scaledBorderHeight *= 1.25f;
            } else if(scaleMode == 2) {
                scaledBorderWidth *= 1.50f;
                scaledBorderHeight *= 1.50f;
            } else if(scaleMode == 3) {
                scaledBorderWidth *= viewportHeight / 224.0f;
                scaledBorderHeight *= viewportHeight / 224.0f;
            } else if(scaleMode == 4) {
                scaledBorderWidth *= viewportWidth / 256.0f;
                scaledBorderHeight *= viewportHeight / 224.0f;
            }
        }

        const float x1 = ((int) viewportWidth - scaledBorderWidth) / 2.0f;
        const float y1 = ((int) viewportHeight - scaledBorderHeight) / 2.0f;
        const float x2 = x1 + scaledBorderWidth;
        const float y2 = y1 + scaledBorderHeight;

        float tx2 = (float) borderWidth / (float) gpuBorderWidth;
        float ty2 = (float) borderHeight / (float) gpuBorderHeight;

        C3D_TexBind(0, &borderTexture);

        C3D_ImmDrawBegin(GPU_TRIANGLES);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, 0.0f, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y1, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x1, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(0.0f, ty2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmSendAttrib(x2, y2, 0.5f, 0.0f);
        C3D_ImmSendAttrib(tx2, ty2, 0.0f, 0.0f);
        C3D_ImmSendAttrib(0.0f, 0.0f, 0.0f, 0.0f);

        C3D_ImmDrawEnd();
    }

    C3D_FrameEnd(0);
    if(!mgrGetFastForward()) {
        gspWaitForVBlank();
    }
}

#endif
