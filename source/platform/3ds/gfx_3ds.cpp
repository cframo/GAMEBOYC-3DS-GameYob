#ifdef BACKEND_3DS

#include <malloc.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <sstream>

#include <3ds.h>
#include <citro3d.h>

#include "gameboy.h"
#include "ppu.h"
#include "platform/3ds/default_shbin.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/common/menu/filechooser.h"
#include "platform/gfx.h"
#include "platform/system.h"
#include "platform/3ds/ui_renderer.h"

static bool c3dInitialized;

static inline bool isBottomUIOrTouchActive() {
    return menuOn || isFileChooserActive() || ((hidKeysHeld() & KEY_TOUCH) != 0);
}

static u32 bottomFrameCount = 0;
static bool bottomScreenDirty = true;

static bool shaderInitialized;
static DVLB_s* dvlb;
static shaderProgram_s program;

static C3D_RenderTarget* targetTopLeft = nullptr;
static C3D_RenderTarget* targetTopRight = nullptr;
static C3D_RenderTarget* targetBottom = nullptr;
static int s_bottomClearCount = 2;
static C3D_Mtx projectionTop;
static C3D_Mtx projectionBottom;

static bool rttInit = false;
static C3D_Tex rttTex;
static C3D_RenderTarget* rttTarget = nullptr;
static C3D_Mtx projectionRtt;
static int uLocProjection = 0;

static bool screenInit;
static C3D_Tex screenTex[2];
static int curTexIdx = 0;

static bool sprTexInit = false;
static C3D_Tex sprTex[2];

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
static u32* sprBuffer;

static bool gfxInitialized = false;

static inline void drawQuad(float x1, float y1, float x2, float y2, float u1, float v1, float u2, float v2, float gu1, float gv1, float gu2, float gv2) {
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

bool gfxInit() {
    osSetSpeedupEnable(true);
    gfxInitDefault();
    gfxSet3D(true);
    gfxInitialized = true;

    if(!C3D_Init(0x100000)) {
        gfxCleanup();
        return false;
    }

    c3dInitialized = true;

    u32 displayFlags = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    targetTopLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, (C3D_DEPTHTYPE)-1);
    if(targetTopLeft == nullptr) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(targetTopLeft, GFX_TOP, GFX_LEFT, displayFlags);

    targetTopRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, (C3D_DEPTHTYPE)-1);
    if(targetTopRight == nullptr) {
        gfxCleanup();
        return false;
    }

    C3D_RenderTargetSetOutput(targetTopRight, GFX_TOP, GFX_RIGHT, displayFlags);

    targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, (C3D_DEPTHTYPE)-1);
    if(targetBottom == nullptr) {
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
    uLocProjection = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

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
    C3D_CullFace(GPU_CULL_NONE);

    Mtx_OrthoTilt(&projectionTop, 0.0, 400.0, 240.0, 0.0, 0.0, 1.0, true);
    Mtx_OrthoTilt(&projectionBottom, 0.0, 320.0, 240.0, 0.0, 0.0, 1.0, true);
    Mtx_Ortho(&projectionRtt, 0.0f, 512.0f, 512.0f, 0.0f, 0.0f, 1.0f, true);

    screenInit = false;
    borderInit = false;
    curTexIdx = 0;

    // Allocate and clear the screen buffer.
    u32* contiguousGfxMem = (u32*) linearMemAlign(256 * 256 * sizeof(u32) * 2, 0x80);
    screenBuffer = contiguousGfxMem;
    sprBuffer = contiguousGfxMem + (256 * 256);
    memset(contiguousGfxMem, 0, 256 * 256 * sizeof(u32) * 2);

    if(C3D_TexInitVRAM(&screenTex[0], 256, 256, GPU_RGBA8) && C3D_TexInitVRAM(&screenTex[1], 256, 256, GPU_RGBA8)) {
        screenInit = true;
    }

    sprTexInit = false;
    if(C3D_TexInitVRAM(&sprTex[0], 256, 256, GPU_RGBA8) && C3D_TexInitVRAM(&sprTex[1], 256, 256, GPU_RGBA8)) {
        sprTexInit = true;
    }

    rttInit = false;
    if(C3D_TexInitVRAM(&rttTex, 512, 512, GPU_RGBA8)) {
        rttTarget = C3D_RenderTargetCreateFromTex(&rttTex, GPU_TEXFACE_2D, 0, (C3D_DEPTHTYPE)-1);
        if(rttTarget != nullptr) {
            rttInit = true;
        } else {
            C3D_TexDelete(&rttTex);
        }
    }

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
                    dst[morton] = 0xD0D0D0FF;
                } else if(x == 7 || y == 7) {
                    // Borde perimetral de 1 texel: sombra de fondo (~92% de brillo)
                    dst[morton] = 0xEAEAEAFF;
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

    uiRenderInit();

    return true;
}

void gfxCleanup() {
    if(sprTexInit) {
        C3D_TexDelete(&sprTex[0]);
        C3D_TexDelete(&sprTex[1]);
        sprTexInit = false;
    }

    if(lcdGridInit) {
        C3D_TexDelete(&lcdGridTexture);
        lcdGridInit = false;
    }

    if(screenBuffer != nullptr) {
        linearFree(screenBuffer);
        screenBuffer = nullptr;
        sprBuffer = nullptr;
    }

    if(borderInit) {
        C3D_TexDelete(&borderTexture);
        borderInit = false;
    }

    if(screenInit) {
        C3D_TexDelete(&screenTex[0]);
        C3D_TexDelete(&screenTex[1]);
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

    if(targetTopLeft != nullptr) {
        C3D_RenderTargetDelete(targetTopLeft);
        targetTopLeft = nullptr;
    }

    if(targetTopRight != nullptr) {
        C3D_RenderTargetDelete(targetTopRight);
        targetTopRight = nullptr;
    }

    if(targetBottom != nullptr) {
        C3D_RenderTargetDelete(targetBottom);
        targetBottom = nullptr;
    }

    if(rttTarget != nullptr) {
        C3D_RenderTargetDelete(rttTarget);
        rttTarget = nullptr;
    }

    if(rttInit) {
        C3D_TexDelete(&rttTex);
        rttInit = false;
    }

    uiRenderExit();

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

u32* gfxGetSprBuffer() {
    return sprBuffer;
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

bool gfxSaveStateThumbnail(const char* thumbPath) {
    if(thumbPath == nullptr || screenBuffer == nullptr) {
        return false;
    }

    static u32 thumbBuffer[128 * 128];
    // Fondo oscuro RGBA(14, 18, 26, 255) en formato 0xRRGGBBAA
    for(int i = 0; i < 128 * 128; i++) {
        thumbBuffer[i] = 0x0E121AFF;
    }

    for(int py = 0; py < 115; py++) {
        int srcY = (py * 144) / 115;
        if(srcY >= 144) {
            srcY = 143;
        }
        int rowIdx = (srcY + 40) * 256 + 48;
        int dstIdx = (py + 6) * 128;
        for(int px = 0; px < 128; px++) {
            int srcX = (px * 160) / 128;
            if(srcX >= 160) {
                srcX = 159;
            }
            u32 srcPx = screenBuffer[rowIdx + srcX];
            u8 r = (srcPx >> 24) & 0xFF;
            u8 g = (srcPx >> 16) & 0xFF;
            u8 b = (srcPx >> 8) & 0xFF;
            // Formato de textura PICA200: 0xRRGGBBAA
            thumbBuffer[dstIdx + px] = ((u32) r << 24) | ((u32) g << 16) | ((u32) b << 8) | 0xFF;
        }
    }

    FILE* f = fopen(thumbPath, "wb");
    if(!f) {
        return false;
    }

    const char magic[8] = {'G', 'Y', 'T', 'H', 'M', '2', '\0', '\0'};
    if(fwrite(magic, 1, 8, f) != 8) {
        fclose(f);
        remove(thumbPath);
        return false;
    }

    if(fwrite(thumbBuffer, 4, 128 * 128, f) != 128 * 128) {
        fclose(f);
        remove(thumbPath);
        return false;
    }

    fclose(f);
    return true;
}

void gfxDrawScreen() {
    uiUpdateMenuState();

    const int screenTexSize = 256;
    GPU_TEXTURE_FILTER_PARAM filter = GPU_NEAREST;

    if(scaleMode != 0 && scaleFilter == 1) {
        filter = GPU_LINEAR;
    }

    if(scaleFilter == 2 && scaleMode != 0 && !rttInit) {
        if(C3D_TexInitVRAM(&rttTex, 512, 512, GPU_RGBA8)) {
            rttTarget = C3D_RenderTargetCreateFromTex(&rttTex, GPU_TEXFACE_2D, 0, (C3D_DEPTHTYPE)-1);
            if(rttTarget != nullptr) {
                rttInit = true;
            } else {
                C3D_TexDelete(&rttTex);
            }
        }
    }
    bool useRtt = (scaleFilter == 2 && scaleMode != 0 && rttInit);

    if(!screenInit) {
        bool init0 = C3D_TexInitVRAM(&screenTex[0], screenTexSize, screenTexSize, GPU_RGBA8);
        bool init1 = C3D_TexInitVRAM(&screenTex[1], screenTexSize, screenTexSize, GPU_RGBA8);
        if(init0 && init1) {
            screenInit = true;
        } else {
            if(init0) {
                C3D_TexDelete(&screenTex[0]);
            }
            if(init1) {
                C3D_TexDelete(&screenTex[1]);
            }
            return;
        }
    }

    if(!screenInit || screenBuffer == nullptr) {
        return;
    }

    C3D_TexSetFilter(&screenTex[0], filter, filter);
    C3D_TexSetFilter(&screenTex[1], filter, filter);

    float currentSlider = (gameScreen == 0) ? osGet3DSliderState() : 0.0f;
    bool isStereoActive = (currentSlider > 0.001f);
    if(gameboy != nullptr) {
        gameboy->currentSlider = currentSlider;
        gameboy->stereoEnabled = isStereoActive;
        if(gameboy->getPPU() != nullptr) {
            gameboy->getPPU()->setStereoEnabled(isStereoActive);
            gameboy->getPPU()->sprBuffer = sprBuffer;
        }
    }

    float slider = currentSlider;
    bool isStereo = isStereoActive;

    if(isStereo) {
        if(!sprTexInit) {
            bool init0 = C3D_TexInitVRAM(&sprTex[0], screenTexSize, screenTexSize, GPU_RGBA8);
            bool init1 = C3D_TexInitVRAM(&sprTex[1], screenTexSize, screenTexSize, GPU_RGBA8);
            if(init0 && init1) {
                sprTexInit = true;
            } else {
                if(init0) {
                    C3D_TexDelete(&sprTex[0]);
                }
                if(init1) {
                    C3D_TexDelete(&sprTex[1]);
                }
            }
        }

        if(sprTexInit) {
            C3D_TexSetFilter(&sprTex[0], filter, filter);
            C3D_TexSetFilter(&sprTex[1], filter, filter);
        }
    }

    u32 displayFlags = GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                       GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

    bool hasGame = (gameboy != nullptr && gameboy->isPoweredOn());
    bool hasSprites = hasGame && isStereoActive && sprTexInit && gameboy->getPPU() != nullptr && gameboy->getPPU()->hasSpritesThisFrame();
    bool dmaDispatched = false;

    if(hasGame && screenTex[curTexIdx].data != nullptr) {
        // 1. Vaciado acotado de D-Cache a 144 líneas útiles (144 KiB en vez de 256 KiB)
        GSPGPU_FlushDataCache((u32*) screenBuffer + 40 * 256, 144 * 256 * sizeof(u32));
        if(hasSprites) {
            u8 sprMin = gameboy->getPPU()->getSprMinY();
            u8 sprMax = gameboy->getPPU()->getSprMaxY();
            u8 prevMin = gameboy->getPPU()->prevSprMinY;
            u8 prevMax = gameboy->getPPU()->prevSprMaxY;

            u8 startY = (sprMin < prevMin) ? sprMin : prevMin;
            u8 endY   = (sprMax > prevMax) ? sprMax : prevMax;

            if(startY <= endY && endY < 144) {
                u32 lineCount = (endY - startY + 1);
                GSPGPU_FlushDataCache((u32*) sprBuffer + (startY + 40) * 256, lineCount * 256 * sizeof(u32));
            }
        }

        // 2. Despachar DMA sin bloqueo inmediato
        GX_DisplayTransfer(
            (u32*) screenBuffer, GX_BUFFER_DIM(256, 256),
            (u32*) screenTex[curTexIdx].data, GX_BUFFER_DIM(256, 256),
            displayFlags
        );

        if(hasSprites) {
            GX_DisplayTransfer(
                (u32*) sprBuffer, GX_BUFFER_DIM(256, 256),
                (u32*) sprTex[curTexIdx].data, GX_BUFFER_DIM(256, 256),
                displayFlags
            );
        }

        dmaDispatched = true;
    }

    if(!C3D_FrameBegin(0)) {
        if(dmaDispatched) {
            gspWaitForPPF();
        }
        return;
    }

    if(isFileChooserActive()) {
        if(dmaDispatched) {
            gspWaitForPPF();
        }
        float slider = osGet3DSliderState();
        C3D_FrameDrawOn(targetTopLeft);
        C3D_RenderTargetClear(targetTopLeft, C3D_CLEAR_ALL, 0x181A20FF, 0);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionTop);
        uiDrawTopScreenHub(-slider * 2.0f);
        if(slider > 0.0f && targetTopRight != nullptr) {
            C3D_FrameDrawOn(targetTopRight);
            C3D_RenderTargetClear(targetTopRight, C3D_CLEAR_ALL, 0x181A20FF, 0);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionTop);
            uiDrawTopScreenHub(slider * 2.0f);
        }
    } else if(!hasGame) {
        // No hay juego cargado: limpiar targetTopLeft (y targetTopRight si 3D activo) con color de fondo oscuro
        C3D_FrameDrawOn(targetTopLeft);
        C3D_RenderTargetClear(targetTopLeft, C3D_CLEAR_ALL, 0x121317FF, 0);
        if(isStereoActive) {
            C3D_FrameDrawOn(targetTopRight);
            C3D_RenderTargetClear(targetTopRight, C3D_CLEAR_ALL, 0x121317FF, 0);
        }
    } else {
        // 3. MIENTRAS el DMA transfiere en hardware, la CPU ejecuta el setup y clear normal de Citro3D
        bool renderTarget = true;
    if(!isStereo) {
        C3D_RenderTarget* target = (gameScreen != 0) ? targetBottom : targetTopLeft;
        if(target == targetBottom) {
            if(isBottomUIOrTouchActive()) {
                bottomScreenDirty = true;
                renderTarget = true;
            } else if(bottomScreenDirty) {
                bottomScreenDirty = false;
                renderTarget = true;
            } else {
                renderTarget = ((++bottomFrameCount % 4) == 0);
            }
        }

        if(renderTarget) {
            C3D_FrameDrawOn(target);
            C3D_RenderTargetClear(target, C3D_CLEAR_COLOR, 0, 0);
        }
    } else {
        C3D_FrameDrawOn(targetTopLeft);
        C3D_RenderTargetClear(targetTopLeft, C3D_CLEAR_COLOR, 0, 0);
        if(isStereoActive) {
            C3D_FrameDrawOn(targetTopRight);
            C3D_RenderTargetClear(targetTopRight, C3D_CLEAR_COLOR, 0, 0);
        }
    }

    // 4. Sincronización diferida
    if(dmaDispatched) {
        gspWaitForPPF();
    }

    u16 viewportWidth = (gameScreen == 0 ? targetTopLeft : targetBottom)->frameBuf.height;
    u16 viewportHeight = (gameScreen == 0 ? targetTopLeft : targetBottom)->frameBuf.width;

    bool lcdActive = (lcdGrid == 1 && lcdGridInit);
    bool blurActive = (gameboy != nullptr && gameboy->settings.emulateBlur);

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

        const float bgOffset = slider * 2.0f;
        const float sprOffset = slider * 2.5f;

        const float rttW = hasCustomBorder ? 512.0f : 320.0f;
        const float rttH = hasCustomBorder ? 448.0f : 288.0f;

        float ru1 = 0.0f;
        float rv1 = 0.0f;
        float ru2 = rttW / 512.0f;
        float rv2 = rttH / 512.0f;

        if(useRtt) {
            const float rttFilterMod = 0.25f / 512.0f;
            ru1 = 0.0f + rttFilterMod;
            ru2 = (rttW / 512.0f) - rttFilterMod;
            rv1 = 1.0f - rttFilterMod;
            rv2 = ((512.0f - rttH) / 512.0f) + rttFilterMod;

            // FASE 1: Renderizado al Target Intermedio en VRAM (Integer 2x Nearest)
            C3D_FrameDrawOn(rttTarget);
            C3D_RenderTargetClear(rttTarget, C3D_CLEAR_COLOR, 0, 0);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionRtt);

            C3D_TexBind(0, &screenTex[curTexIdx]);
            C3D_TexEnv* rttEnv0 = C3D_GetTexEnv(0);
            C3D_TexEnvInit(rttEnv0);

            if(blurActive) {
                C3D_TexBind(2, &screenTex[curTexIdx ^ 1]);
                C3D_TexEnvSrc(rttEnv0, C3D_Both, GPU_TEXTURE0, GPU_TEXTURE2, GPU_CONSTANT);
                C3D_TexEnvOpRgb(rttEnv0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
                C3D_TexEnvOpAlpha(rttEnv0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
                C3D_TexEnvFunc(rttEnv0, C3D_Both, GPU_INTERPOLATE);
                C3D_TexEnvColor(rttEnv0, 0xA8A8A8A8);
            } else {
                C3D_TexEnvSrc(rttEnv0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
                C3D_TexEnvFunc(rttEnv0, C3D_Both, GPU_REPLACE);
            }

            C3D_TexEnvInit(C3D_GetTexEnv(1));
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            C3D_AlphaTest(false, GPU_ALWAYS, 0);
            C3D_CullFace(GPU_CULL_NONE);

            drawQuad(0.0f, 0.0f, rttW, rttH, u1, v1, u2, v2, 0.0f, 0.0f, 0.0f, 0.0f);

            C3D_FrameSplit(0);
        }

        auto setupBgEnv = [&]() {
            if(useRtt) {
                C3D_TexSetFilter(&rttTex, GPU_LINEAR, GPU_LINEAR);
                C3D_TexSetWrap(&rttTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
                C3D_TexBind(0, &rttTex);

                C3D_TexEnv* env0 = C3D_GetTexEnv(0);
                C3D_TexEnvInit(env0);
                C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
                C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
            } else {
                C3D_TexBind(0, &screenTex[curTexIdx]);
                C3D_TexEnv* env0 = C3D_GetTexEnv(0);
                C3D_TexEnvInit(env0);

                if(blurActive) {
                    C3D_TexBind(2, &screenTex[curTexIdx ^ 1]);
                    C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, GPU_TEXTURE2, GPU_CONSTANT);
                    C3D_TexEnvOpRgb(env0, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
                    C3D_TexEnvOpAlpha(env0, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
                    C3D_TexEnvFunc(env0, C3D_Both, GPU_INTERPOLATE);
                    C3D_TexEnvColor(env0, 0xA8A8A8A8);
                } else {
                    C3D_TexEnvSrc(env0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
                    C3D_TexEnvFunc(env0, C3D_Both, GPU_REPLACE);
                }
            }

            if(lcdActive) {
                C3D_TexBind(1, &lcdGridTexture);
                C3D_TexEnv* env1 = C3D_GetTexEnv(1);
                C3D_TexEnvInit(env1);
                C3D_TexEnvSrc(env1, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE1, (GPU_TEVSRC)0);
                C3D_TexEnvFunc(env1, C3D_Both, GPU_MODULATE);
            } else {
                C3D_TexEnvInit(C3D_GetTexEnv(1));
            }

            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            C3D_AlphaTest(false, GPU_ALWAYS, 0);
        };

        auto setupSprEnv = [&]() {
            C3D_TexBind(0, &sprTex[curTexIdx]);
            C3D_TexEnv* sprEnv0 = C3D_GetTexEnv(0);
            C3D_TexEnvInit(sprEnv0);
            C3D_TexEnvSrc(sprEnv0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
            C3D_TexEnvFunc(sprEnv0, C3D_Both, GPU_REPLACE);

            if(lcdActive) {
                C3D_TexBind(1, &lcdGridTexture);
                C3D_TexEnv* env1 = C3D_GetTexEnv(1);
                C3D_TexEnvInit(env1);
                C3D_TexEnvSrc(env1, C3D_RGB, GPU_PREVIOUS, GPU_TEXTURE1, (GPU_TEVSRC)0);
                C3D_TexEnvOpRgb(env1, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, (GPU_TEVOP_RGB)0);
                C3D_TexEnvFunc(env1, C3D_RGB, GPU_MODULATE);
                C3D_TexEnvSrc(env1, C3D_Alpha, GPU_PREVIOUS, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
                C3D_TexEnvOpAlpha(env1, GPU_TEVOP_A_SRC_ALPHA, (GPU_TEVOP_A)0, (GPU_TEVOP_A)0);
                C3D_TexEnvFunc(env1, C3D_Alpha, GPU_REPLACE);
            } else {
                C3D_TexEnvInit(C3D_GetTexEnv(1));
            }

            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
            C3D_AlphaTest(true, GPU_GREATER, 0);
        };

        const float cu1 = useRtt ? ru1 : u1;
        const float cv1 = useRtt ? rv1 : v1;
        const float cu2 = useRtt ? ru2 : u2;
        const float cv2 = useRtt ? rv2 : v2;

        if(!isStereo) {
            if(renderTarget) {
                setupBgEnv();
                C3D_RenderTarget* target = (gameScreen != 0) ? targetBottom : targetTopLeft;
                C3D_FrameDrawOn(target);
                C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, gameScreen == 0 ? &projectionTop : &projectionBottom);

                drawQuad(x1, y1, x2, y2, cu1, cv1, cu2, cv2, gu1, gv1, gu2, gv2);
            }
        } else {
            // ---------------- FASE 1: FONDOS (AMBOS OJOS) ----------------
            setupBgEnv();

            C3D_FrameDrawOn(targetTopLeft);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionTop);
            drawQuad(x1 - bgOffset, y1, x2 - bgOffset, y2, cu1, cv1, cu2, cv2, gu1, gv1, gu2, gv2);

            C3D_FrameDrawOn(targetTopRight);
            C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionTop);
            drawQuad(x1 + bgOffset, y1, x2 + bgOffset, y2, cu1, cv1, cu2, cv2, gu1, gv1, gu2, gv2);

            // ---------------- FASE 2: SPRITES (AMBOS OJOS) ----------------
            if(hasSprites) {
                setupSprEnv();
                C3D_AlphaTest(true, GPU_GREATER, 0);

                // Dibujar Quad Sprites Ojo Izquierdo
                C3D_FrameDrawOn(targetTopLeft);
                drawQuad(x1 + sprOffset, y1, x2 + sprOffset, y2, u1, v1, u2, v2, gu1, gv1, gu2, gv2);

                // Dibujar Quad Sprites Ojo Derecho
                C3D_FrameDrawOn(targetTopRight);
                drawQuad(x1 - sprOffset, y1, x2 - sprOffset, y2, u1, v1, u2, v2, gu1, gv1, gu2, gv2);

                C3D_AlphaTest(false, GPU_ALWAYS, 0);
            }

            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            C3D_AlphaTest(false, GPU_ALWAYS, 0);
        }

        // Draw the border.
        if(borderInit && customBordersEnabled && scaleMode != 4) {
            C3D_TexEnv* borderEnv0 = C3D_GetTexEnv(0);
            C3D_TexEnvInit(borderEnv0);
            C3D_TexEnvSrc(borderEnv0, C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC)0, (GPU_TEVSRC)0);
            C3D_TexEnvFunc(borderEnv0, C3D_Both, GPU_REPLACE);

            C3D_TexEnvInit(C3D_GetTexEnv(1));

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

            const float bx1 = ((int) viewportWidth - scaledBorderWidth) / 2.0f;
            const float by1 = ((int) viewportHeight - scaledBorderHeight) / 2.0f;
            const float bx2 = bx1 + scaledBorderWidth;
            const float by2 = by1 + scaledBorderHeight;

            float tx2 = (float) borderWidth / (float) gpuBorderWidth;
            float ty2 = (float) borderHeight / (float) gpuBorderHeight;

            C3D_TexBind(0, &borderTexture);

            if(!isStereo) {
                if(renderTarget) {
                    C3D_RenderTarget* target = (gameScreen != 0) ? targetBottom : targetTopLeft;
                    C3D_FrameDrawOn(target);
                    drawQuad(bx1, by1, bx2, by2, 0.0f, 0.0f, tx2, ty2, 0.0f, 0.0f, 0.0f, 0.0f);
                }
            } else {
                C3D_FrameDrawOn(targetTopLeft);
                drawQuad(bx1, by1, bx2, by2, 0.0f, 0.0f, tx2, ty2, 0.0f, 0.0f, 0.0f, 0.0f);

                C3D_FrameDrawOn(targetTopRight);
                drawQuad(bx1, by1, bx2, by2, 0.0f, 0.0f, tx2, ty2, 0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
    }

    if(!menuOn && !isFileChooserActive() && !uiIsConfigModalOpen()) {
        if(gameScreen == 0 && s_bottomClearCount > 0) {
            C3D_FrameDrawOn(targetBottom);
            C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0);
            s_bottomClearCount--;
        }
    } else if(uiIsConfigModalOpen()) {
        s_bottomClearCount = 2;
        C3D_FrameDrawOn(targetBottom);
        C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x181A20FF, 0);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionBottom);
        uiDrawConfigModal();
    } else if(isFileChooserActive() && getActiveFileChooser() != nullptr) {
        s_bottomClearCount = 2;
        C3D_FrameDrawOn(targetBottom);
        C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x181A20FF, 0);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionBottom);
        uiDrawFileChooser(getActiveFileChooser());
    } else if(menuOn) {
        s_bottomClearCount = 2;
        C3D_FrameDrawOn(targetBottom);
        C3D_RenderTargetClear(targetBottom, C3D_CLEAR_ALL, 0x181A20FF, 0);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, uLocProjection, &projectionBottom);
        uiDrawConfigModal();
    }

    C3D_FrameEnd(0);
    curTexIdx ^= 1;
}

const C3D_Mtx* gfxGetProjectionTop() {
    return &projectionTop;
}

const C3D_Mtx* gfxGetProjectionBottom() {
    return &projectionBottom;
}

int gfxGetULocProjection() {
    return uLocProjection;
}

#endif
