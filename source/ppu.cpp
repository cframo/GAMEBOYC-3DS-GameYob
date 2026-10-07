#include <math.h>
#include <string.h>

#ifdef BACKEND_3DS
#include <3ds.h>
#include "platform/gfx.h"
extern int gameScreen;
#endif

#include "cpu.h"
#include "gameboy.h"
#include "mmu.h"
#include "ppu.h"
#include "sgb.h"

enum {
    LCD_HBLANK = 0,
    LCD_VBLANK = 1,
    LCD_ACCESS_OAM = 2,
    LCD_ACCESS_OAM_VRAM = 3
};

static u32 grayScalePalette[] = {
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
        0xFFFFFFFF, 0xC0C0C0FF, 0x5E5E5EFF, 0x00000000,
};

static const int modeCycles[] = {
        204,
        456,
        80,
        172
};

static const u16 BitStretchTable256[] = {
        0x0000, 0x0001, 0x0004, 0x0005, 0x0010, 0x0011, 0x0014, 0x0015, 0x0040, 0x0041, 0x0044, 0x0045, 0x0050, 0x0051, 0x0054, 0x0055,
        0x0100, 0x0101, 0x0104, 0x0105, 0x0110, 0x0111, 0x0114, 0x0115, 0x0140, 0x0141, 0x0144, 0x0145, 0x0150, 0x0151, 0x0154, 0x0155,
        0x0400, 0x0401, 0x0404, 0x0405, 0x0410, 0x0411, 0x0414, 0x0415, 0x0440, 0x0441, 0x0444, 0x0445, 0x0450, 0x0451, 0x0454, 0x0455,
        0x0500, 0x0501, 0x0504, 0x0505, 0x0510, 0x0511, 0x0514, 0x0515, 0x0540, 0x0541, 0x0544, 0x0545, 0x0550, 0x0551, 0x0554, 0x0555,
        0x1000, 0x1001, 0x1004, 0x1005, 0x1010, 0x1011, 0x1014, 0x1015, 0x1040, 0x1041, 0x1044, 0x1045, 0x1050, 0x1051, 0x1054, 0x1055,
        0x1100, 0x1101, 0x1104, 0x1105, 0x1110, 0x1111, 0x1114, 0x1115, 0x1140, 0x1141, 0x1144, 0x1145, 0x1150, 0x1151, 0x1154, 0x1155,
        0x1400, 0x1401, 0x1404, 0x1405, 0x1410, 0x1411, 0x1414, 0x1415, 0x1440, 0x1441, 0x1444, 0x1445, 0x1450, 0x1451, 0x1454, 0x1455,
        0x1500, 0x1501, 0x1504, 0x1505, 0x1510, 0x1511, 0x1514, 0x1515, 0x1540, 0x1541, 0x1544, 0x1545, 0x1550, 0x1551, 0x1554, 0x1555,
        0x4000, 0x4001, 0x4004, 0x4005, 0x4010, 0x4011, 0x4014, 0x4015, 0x4040, 0x4041, 0x4044, 0x4045, 0x4050, 0x4051, 0x4054, 0x4055,
        0x4100, 0x4101, 0x4104, 0x4105, 0x4110, 0x4111, 0x4114, 0x4115, 0x4140, 0x4141, 0x4144, 0x4145, 0x4150, 0x4151, 0x4154, 0x4155,
        0x4400, 0x4401, 0x4404, 0x4405, 0x4410, 0x4411, 0x4414, 0x4415, 0x4440, 0x4441, 0x4444, 0x4445, 0x4450, 0x4451, 0x4454, 0x4455,
        0x4500, 0x4501, 0x4504, 0x4505, 0x4510, 0x4511, 0x4514, 0x4515, 0x4540, 0x4541, 0x4544, 0x4545, 0x4550, 0x4551, 0x4554, 0x4555,
        0x5000, 0x5001, 0x5004, 0x5005, 0x5010, 0x5011, 0x5014, 0x5015, 0x5040, 0x5041, 0x5044, 0x5045, 0x5050, 0x5051, 0x5054, 0x5055,
        0x5100, 0x5101, 0x5104, 0x5105, 0x5110, 0x5111, 0x5114, 0x5115, 0x5140, 0x5141, 0x5144, 0x5145, 0x5150, 0x5151, 0x5154, 0x5155,
        0x5400, 0x5401, 0x5404, 0x5405, 0x5410, 0x5411, 0x5414, 0x5415, 0x5440, 0x5441, 0x5444, 0x5445, 0x5450, 0x5451, 0x5454, 0x5455,
        0x5500, 0x5501, 0x5504, 0x5505, 0x5510, 0x5511, 0x5514, 0x5515, 0x5540, 0x5541, 0x5544, 0x5545, 0x5550, 0x5551, 0x5554, 0x5555
};

static const u8 BitReverseTable256[] = {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
};

extern int cgbColors;

static u32 cgbColorLut[32768];
static bool cgbLutInitialized = false;

static inline float clampColor(float v, float minVal, float maxVal) {
    if(v < minVal) {
        return minVal;
    }
    if(v > maxVal) {
        return maxVal;
    }
    return v;
}

void PPU::initCgbColorLut() {
    if(!cgbLutInitialized) {
        for(u32 rgb555 = 0; rgb555 < 32768; rgb555++) {
            u8 r5 = (u8) (rgb555 & 0x1F);
            u8 g5 = (u8) ((rgb555 >> 5) & 0x1F);
            u8 b5 = (u8) ((rgb555 >> 10) & 0x1F);

            float rLin = powf(r5 / 31.0f, 2.2f);
            float gLin = powf(g5 / 31.0f, 2.2f);
            float bLin = powf(b5 / 31.0f, 2.2f);

            float rOut = 0.82f * rLin + 0.12f * gLin + 0.06f * bLin;
            float gOut = 0.00f * rLin + 0.70f * gLin + 0.30f * bLin;
            float bOut = 0.12f * rLin + 0.08f * gLin + 0.80f * bLin;

            u8 r8 = (u8) clampColor(powf(rOut, 1.0f / 2.2f) * 255.0f, 0.0f, 255.0f);
            u8 g8 = (u8) clampColor(powf(gOut, 1.0f / 2.2f) * 255.0f, 0.0f, 255.0f);
            u8 b8 = (u8) clampColor(powf(bOut, 1.0f / 2.2f) * 255.0f, 0.0f, 255.0f);

            cgbColorLut[rgb555] = ((u32) r8 << 24) | ((u32) g8 << 16) | ((u32) b8 << 8) | 0xFF;
        }

        cgbLutInitialized = true;
    }
}

void PPU::initBuffers() {
#ifdef BACKEND_3DS
    if(this->sprBuffer == nullptr) {
        this->sprBuffer = gfxGetSprBuffer();
    }
    if(this->sprBuffer != nullptr) {
        memset(this->sprBuffer, 0, 256 * 256 * sizeof(u32));
    }
#else
    if(this->sprBuffer == nullptr) {
        this->sprBuffer = new u32[256 * 256]();
    }
#endif
}

void PPU::clearSprBuffer() {
    if(!this->stereoEnabled || !this->sprDirty || this->sprBuffer == nullptr) {
        this->sprDirty = false;
        this->sprDrawnThisFrame = false;
        return;
    }
    // Unión de cotas: asegura que cualquier sprite que se haya movido o desaparecido quede en 0
    u8 clearStart = (this->sprMinY < this->prevSprMinY) ? this->sprMinY : this->prevSprMinY;
    u8 clearEnd = (this->sprMaxY > this->prevSprMaxY) ? this->sprMaxY : this->prevSprMaxY;
    if(clearStart <= clearEnd && clearEnd < 144) {
        u32 startLine = clearStart + 40;
        u32 count = clearEnd - clearStart + 1;
        memset(&this->sprBuffer[startLine * 256], 0, count * 256 * sizeof(u32));
    }
    this->prevSprMinY = this->sprMinY;
    this->prevSprMaxY = this->sprMaxY;
    this->sprMinY = 144;
    this->sprMaxY = 0;
    this->sprDirty = false;
    this->sprDrawnThisFrame = false;
}

PPU::PPU(Gameboy* gb) {
    this->gameboy = gb;
    PPU::initCgbColorLut();
    this->initBuffers();
}

PPU::~PPU() {
#ifdef BACKEND_3DS
    this->sprBuffer = nullptr;
#else
    if(this->sprBuffer != nullptr) {
        delete[] this->sprBuffer;
        this->sprBuffer = nullptr;
    }
#endif
}

void PPU::reset() {
    this->lastScanlineCycle = 0;
    this->lastPhaseCycle = 0;
    this->halfSpeed = false;
    this->statInterruptSignal = false;

    this->scanlineX = 0;

    if(this->sprBuffer != nullptr) {
        memset(this->sprBuffer, 0, 256 * 256 * sizeof(u32));
    }
    this->sprDirty = false;
    this->sprDrawnThisFrame = false;
    this->sprMinY = 144;
    this->sprMaxY = 0;
    this->prevSprMinY = 0;
    this->prevSprMaxY = 143;

    memset(this->currTileLines, 0, sizeof(this->currTileLines));
    memset(this->currSpriteLines, 0, sizeof(this->currSpriteLines));
    this->currSprites = 0;

    memset(this->vram, 0, sizeof(this->vram));
    memset(this->oam, 0, sizeof(this->oam));
    memset(this->rawBgPalette, 0, sizeof(this->rawBgPalette));
    memset(this->rawSprPalette, 0, sizeof(this->rawSprPalette));

    memcpy(this->bgPalette, grayScalePalette, sizeof(this->bgPalette));
    memcpy(this->sprPalette, grayScalePalette, sizeof(this->sprPalette));

    if(this->gameboy->gbMode == MODE_CGB) {
        for(u8 i = 0; i < sizeof(this->expandedBgp); i++) {
            this->expandedBgp[i] = i;
        }

        for(u8 i = 0; i < sizeof(this->expandedObp); i++) {
            this->expandedObp[i] = (u8) (i & 3);
        }
    } else {
        memset(this->expandedBgp, 3, sizeof(this->expandedBgp));
        this->expandedBgp[0] = 0;
        memset(this->expandedObp, 3, sizeof(this->expandedObp));
    }

    this->mapBanks();

    if((this->gameboy->mmu->readIO(LCDC) & 0x80) != 0) {
        this->gameboy->mmu->writeIO(LY, 0);
        this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | LCD_ACCESS_OAM));
        this->lastScanlineCycle = 0;
        this->gameboy->cpu->setEventCycle(modeCycles[LCD_ACCESS_OAM]);
        this->updateStatSignal();
    }

    this->gameboy->mmu->mapIOReadFunc(BCPD, [this](u16 addr) -> u8 {
        if(this->gameboy->gbMode == MODE_CGB) {
            return this->rawBgPalette[this->gameboy->mmu->readIO(BCPS) & 0x3F];
        } else {
            return this->gameboy->mmu->readIO(BCPD);
        }
    });

    this->gameboy->mmu->mapIOReadFunc(OCPD, [this](u16 addr) -> u8 {
        if(this->gameboy->gbMode == MODE_CGB) {
            return this->rawSprPalette[this->gameboy->mmu->readIO(OCPS) & 0x3F];
        } else {
            return this->gameboy->mmu->readIO(OCPD);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(LCDC, [this](u16 addr, u8 val) -> void {
        u8 curr = this->gameboy->mmu->readIO(LCDC);
        this->gameboy->mmu->writeIO(LCDC, val);

        if((curr & 0x80) && !(val & 0x80)) {
            this->gameboy->mmu->writeIO(LY, 0);
            this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | LCD_HBLANK));
            this->updateStatSignal();
        } else if(!(curr & 0x80) && (val & 0x80)) {
            this->gameboy->mmu->writeIO(LY, 0);
            this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & ~3) | LCD_ACCESS_OAM));

            this->lastScanlineCycle = this->gameboy->cpu->getCycle() - (4 << this->halfSpeed);
            this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + modeCycles[LCD_ACCESS_OAM]);
            this->updateStatSignal();
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(STAT, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(STAT, (u8) ((this->gameboy->mmu->readIO(STAT) & 0x7) | (val & 0xF8)));
        this->updateStatSignal();
    });

    this->gameboy->mmu->mapIOWriteFunc(LY, [this](u16 addr, u8 val) -> void {
    });

    this->gameboy->mmu->mapIOWriteFunc(LYC, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(LYC, val);
        this->checkLYC();
    });

    this->gameboy->mmu->mapIOWriteFunc(DMA, [this](u16 addr, u8 val) -> void {
        u16 src = val << 8;

        for(u8 i = 0; i < 0xA0; i++) {
            this->oam[i] = this->gameboy->mmu->read(src++);
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BGP, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(BGP, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedBgp[i] = (u8) ((val >> (i * 2)) & 3);
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OBP0, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(OBP0, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedObp[i] = (u8) ((val >> (i * 2)) & 3);
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OBP1, [this](u16 addr, u8 val) -> void {
        this->gameboy->mmu->writeIO(OBP1, val);

        if(this->gameboy->gbMode != MODE_CGB) {
            for(u8 i = 0; i < 4; i++) {
                this->expandedObp[4 + i] = (u8) ((val >> (i * 2)) & 3);
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(VBK, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(VBK, (u8) (val | 0xFE));
            this->mapBanks();
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BCPS, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(BCPS, (u8) (val | 0x40));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(BCPD, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            u8 bcps = this->gameboy->mmu->readIO(BCPS);
            u8 selected = (u8) (bcps & 0x3F);

            u16 rgb555 = 0;
            if(selected & 1) {
                rgb555 = this->rawBgPalette[selected & ~1] | (val << 8);
            } else {
                rgb555 = val | (this->rawBgPalette[selected | 1] << 8);
            }

            this->rawBgPalette[selected] = val;
            u16 rgb555_masked = rgb555 & 0x7FFF;
            u32 colorFinal = (cgbColors == 1) ? cgbColorLut[rgb555_masked] : RGB555ToRGB8888(rgb555);
            this->bgPalette[selected >> 1] = colorFinal;

            if(bcps & 0x80) {
                this->gameboy->mmu->writeIO(BCPS, (u8) (((bcps & 0x3F) + 1) | (bcps & 0x80) | 0x40));
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPS, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            this->gameboy->mmu->writeIO(OCPS, (u8) (val | 0x40));
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(OCPD, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            u8 ocps = this->gameboy->mmu->readIO(OCPS);
            u8 selected = (u8) (ocps & 0x3F);

            u16 rgb555 = 0;
            if(selected & 1) {
                rgb555 = this->rawSprPalette[selected & ~1] | (val << 8);
            } else {
                rgb555 = val | (this->rawSprPalette[selected | 1] << 8);
            }

            this->rawSprPalette[selected] = val;
            u16 rgb555_masked = rgb555 & 0x7FFF;
            u32 colorFinal = (cgbColors == 1) ? cgbColorLut[rgb555_masked] : RGB555ToRGB8888(rgb555);
            this->sprPalette[selected >> 1] = colorFinal;

            if(ocps & 0x80) {
                this->gameboy->mmu->writeIO(OCPS, (u8) (((ocps & 0x3F) + 1) | (ocps & 0x80) | 0x40));
            }
        }
    });

    this->gameboy->mmu->mapIOWriteFunc(HDMA5, [this](u16 addr, u8 val) -> void {
        if(this->gameboy->gbMode == MODE_CGB) {
            if((this->gameboy->mmu->readIO(HDMA5) & 0x80) == 0) {
                if((val & 0x80) == 0) {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val | 0x80));
                }
            } else {
                if(((val >> 7) & 1) == 0) {
                    u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
                    u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                    u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                    u8 length = (u8) ((val & 0x7F) + 1);
                    for(u8 i = 0; i < length; i++) {
                        for(u8 j = 0; j < 0x10; j++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
                        }

                        dst &= 0x1FF0;
                    }

                    this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                    this->gameboy->mmu->writeIO(HDMA5, 0xFF);

                    this->gameboy->cpu->advanceCycles((u64) ((length * 8) << this->halfSpeed));
                } else {
                    this->gameboy->mmu->writeIO(HDMA5, (u8) (val & 0x7F));
                }
            }
        }
    });
}

void PPU::loadState(std::istream& data, u8 version) {
    data.read((char*) &this->lastScanlineCycle, sizeof(this->lastScanlineCycle));
    data.read((char*) &this->lastPhaseCycle, sizeof(this->lastPhaseCycle));
    data.read((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.read((char*) &this->scanlineX, sizeof(this->scanlineX));
    data.read((char*) this->vram, sizeof(this->vram));
    data.read((char*) this->oam, sizeof(this->oam));
    data.read((char*) this->rawBgPalette, sizeof(this->rawBgPalette));
    data.read((char*) this->rawSprPalette, sizeof(this->rawSprPalette));
    data.read((char*) this->bgPalette, sizeof(this->bgPalette));
    data.read((char*) this->sprPalette, sizeof(this->sprPalette));
    data.read((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.read((char*) this->expandedObp, sizeof(this->expandedObp));

    this->mapBanks();
    this->refreshPalettes();
}

void PPU::refreshPalettes() {
    if(this->gameboy->gbMode == MODE_CGB) {
        for(int i = 0; i < 32; i++) {
            u16 bg555 = ((this->rawBgPalette[i * 2 + 1] << 8) | this->rawBgPalette[i * 2]) & 0x7FFF;
            this->bgPalette[i] = (cgbColors == 1) ? cgbColorLut[bg555] : RGB555ToRGB8888(bg555);

            u16 spr555 = ((this->rawSprPalette[i * 2 + 1] << 8) | this->rawSprPalette[i * 2]) & 0x7FFF;
            this->sprPalette[i] = (cgbColors == 1) ? cgbColorLut[spr555] : RGB555ToRGB8888(spr555);
        }
    }
}

void PPU::saveState(std::ostream& data) {
    data.write((char*) &this->lastScanlineCycle, sizeof(this->lastScanlineCycle));
    data.write((char*) &this->lastPhaseCycle, sizeof(this->lastPhaseCycle));
    data.write((char*) &this->halfSpeed, sizeof(this->halfSpeed));
    data.write((char*) &this->scanlineX, sizeof(this->scanlineX));
    data.write((char*) this->vram, sizeof(this->vram));
    data.write((char*) this->oam, sizeof(this->oam));
    data.write((char*) this->rawBgPalette, sizeof(this->rawBgPalette));
    data.write((char*) this->rawSprPalette, sizeof(this->rawSprPalette));
    data.write((char*) this->bgPalette, sizeof(this->bgPalette));
    data.write((char*) this->sprPalette, sizeof(this->sprPalette));
    data.write((char*) this->expandedBgp, sizeof(this->expandedBgp));
    data.write((char*) this->expandedObp, sizeof(this->expandedObp));
}

void PPU::mapBanks() {
    u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
    this->gameboy->mmu->mapBankBlock(0x8, this->vram[bank] + 0x0000);
    this->gameboy->mmu->mapBankBlock(0x9, this->vram[bank] + 0x1000);
}

__attribute__((always_inline)) inline void PPU::updateStatSignal() {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);
    if((lcdc & 0x80) == 0) {
        this->statInterruptSignal = false;
        return;
    }

    u8 stat = this->gameboy->mmu->readIO(STAT);
    u8 mode = (u8) (stat & 3);

    bool lycCondition = ((stat & 0x40) != 0) && ((stat & 0x04) != 0);
    bool oamCondition = ((stat & 0x20) != 0) && (mode == LCD_ACCESS_OAM);
    bool vblankCondition = ((stat & 0x10) != 0) && (mode == LCD_VBLANK);
    bool hblankCondition = ((stat & 0x08) != 0) && (mode == LCD_HBLANK);

    bool newSignal = lycCondition || oamCondition || vblankCondition || hblankCondition;
    if(!this->statInterruptSignal && newSignal) {
        this->gameboy->mmu->writeIO(IF, (u8) (this->gameboy->mmu->readIO(IF) | INT_LCD));
    }

    this->statInterruptSignal = newSignal;
}

__attribute__((always_inline)) inline void PPU::checkLYC() {
    u8 stat = this->gameboy->mmu->readIO(STAT);
    if(this->gameboy->mmu->readIO(LY) == this->gameboy->mmu->readIO(LYC)) {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat | 4));
    } else {
        this->gameboy->mmu->writeIO(STAT, (u8) (stat & ~4));
    }
    this->updateStatSignal();
}

void PPU::update() {
    if((this->gameboy->mmu->readIO(LCDC) & 0x80) == 0) {
        this->statInterruptSignal = false;
        this->lastScanlineCycle = this->gameboy->cpu->getCycle();

        this->gameboy->mmu->writeIO(LY, 0);
        this->gameboy->mmu->writeIO(STAT, (u8) (this->gameboy->mmu->readIO(STAT) & 0xF8));

        // Ensure that we continue to return execution to frontend every frame, despite the LCD being off.
        while(this->gameboy->cpu->getCycle() >= this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed)) {
            this->lastPhaseCycle += CYCLES_PER_FRAME << this->halfSpeed;

            this->gameboy->ranFrame = true;
        }

        if(this->gameboy->ranFrame && this->gameboy->settings.frameBuffer != NULL) {
            for(u8 y = 40; y < 184; y++) {
                memset(&this->gameboy->settings.frameBuffer[y * this->gameboy->settings.framePitch + 48], 0xFF, 160 * sizeof(u32));
            }
        }

        this->gameboy->cpu->setEventCycle(this->lastPhaseCycle + (CYCLES_PER_FRAME << this->halfSpeed));
    } else {
        this->lastPhaseCycle = this->gameboy->cpu->getCycle();

        u8 stat = this->gameboy->mmu->readIO(STAT);
        u8 mode = (u8) (stat & 3);
        u64 threshold = this->lastScanlineCycle + (modeCycles[mode] << this->halfSpeed);

        while(this->gameboy->cpu->getCycle() >= threshold) {
            u8 ly = this->gameboy->mmu->readIO(LY);
            u8 hdma5 = this->gameboy->mmu->readIO(HDMA5);

            this->lastScanlineCycle = threshold;

            switch(mode) {
                case LCD_HBLANK:
                    ly++;
                    this->gameboy->mmu->writeIO(LY, ly);

                    if(ly >= 144) {
                        mode = LCD_VBLANK;
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_VBLANK));

                        u8 interrupts = this->gameboy->mmu->readIO(IF);
                        interrupts |= INT_VBLANK;
                        this->gameboy->mmu->writeIO(IF, interrupts);
                        this->gameboy->ranFrame = true;
                    } else {
                        mode = LCD_ACCESS_OAM;
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));
                    }

                    this->checkLYC();
                    break;
                case LCD_VBLANK:
                    if(ly == 0) {
                        mode = LCD_ACCESS_OAM;
                        this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM));
                        this->updateStatSignal();
                    } else {
                        ly++;
                        if(ly >= 153) {
                            // Don't change the mode. Scanline 0 is twice as
                            // long as normal - half of it identifies as being
                            // in the vblank period.
                            ly = 0;
                        }
                        this->gameboy->mmu->writeIO(LY, ly);
                        this->checkLYC();
                    }

                    break;
                case LCD_ACCESS_OAM:
                    this->scanlineX = 0;
                    if(this->gameboy->settings.perPixelRendering) {
                        this->updateLineSprites(this->gameboy->mmu->readIO(LCDC));
                    }

                    mode = LCD_ACCESS_OAM_VRAM;
                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_ACCESS_OAM_VRAM));
                    this->updateStatSignal();
                    break;
                case LCD_ACCESS_OAM_VRAM:
                    if(!this->gameboy->settings.perPixelRendering && this->gameboy->settings.drawEnabled) {
                        this->drawScanline(ly);
                    }

                    mode = LCD_HBLANK;
                    this->gameboy->mmu->writeIO(STAT, (u8) ((stat & ~3) | LCD_HBLANK));
                    this->updateStatSignal();

                    if(this->gameboy->gbMode == MODE_CGB && (hdma5 & 0x80) == 0) {
                        u8 bank = (u8) (this->gameboy->gbMode == MODE_CGB && (this->gameboy->mmu->readIO(VBK) & 0x1) != 0);
                        u16 src = (u16) ((this->gameboy->mmu->readIO(HDMA2) | (this->gameboy->mmu->readIO(HDMA1) << 8)) & 0xFFF0);
                        u16 dst = (u16) ((this->gameboy->mmu->readIO(HDMA4) | (this->gameboy->mmu->readIO(HDMA3) << 8)) & 0x1FF0);
                        for(u8 i = 0; i < 0x10; i++) {
                            this->vram[bank][dst++] = this->gameboy->mmu->read(src++);
                        }

                        dst &= 0x1FF0;

                        this->gameboy->mmu->writeIO(HDMA1, (u8) ((src >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA2, (u8) (src & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA3, (u8) ((dst >> 8) & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA4, (u8) (dst & 0xFF));
                        this->gameboy->mmu->writeIO(HDMA5, (u8) (hdma5 - 1));

                        this->gameboy->cpu->advanceCycles((u64) (8 << this->halfSpeed));
                    }

                    break;
                default:
                    break;
            }

            stat = this->gameboy->mmu->readIO(STAT);
            threshold = this->lastScanlineCycle + (modeCycles[mode] << this->halfSpeed);
        }

        if(__builtin_expect(this->gameboy->settings.perPixelRendering, 0)) {
            this->updateScanline();
        } else {
            this->gameboy->cpu->setEventCycle(threshold);
        }
    }
}

void PPU::setHalfSpeed(bool halfSpeed) {
    if(!this->halfSpeed && halfSpeed) {
        this->lastScanlineCycle -= this->gameboy->cpu->getCycle() - this->lastScanlineCycle;
        this->lastPhaseCycle -= this->gameboy->cpu->getCycle() - this->lastPhaseCycle;
    } else if(this->halfSpeed && !halfSpeed) {
        this->lastScanlineCycle += (this->gameboy->cpu->getCycle() - this->lastScanlineCycle) >> 1;
        this->lastPhaseCycle += (this->gameboy->cpu->getCycle() - this->lastPhaseCycle) >> 1;
    }

    this->halfSpeed = halfSpeed;
}

void PPU::transferTiles(u8* dest) {
    u8 lcdc = this->gameboy->mmu->readIO(LCDC);

    u32 map = (u32) (0x1800 + ((lcdc >> 3) & 1) * 0x400);
    u32 index = 0;
    for(u32 y = 0; y < 18; y++) {
        for(u32 x = 0; x < 20; x++) {
            if(index == 0x1000) {
                return;
            }

            u8 tile = this->vram[0][map + y * 32 + x];
            if(lcdc & 0x10) {
                memcpy(dest + index, &this->vram[0][tile * 0x10], 0x10);
            } else {
                memcpy(dest + index, &this->vram[0][0x1000 + (s8) tile * 0x10], 0x10);
            }

            index += 0x10;
        }
    }
}

inline void PPU::updateLineTile(u8 map, u8 x, u8 y) {
    TileLine* line = &this->currTileLines[map];

    u8 lcdc = this->gameboy->mmu->readIO(LCDC);

    u8 tileX = x >> 3;
    u8 tileY = y >> 3;
    u16 offset = (u16) (0x1800 + (map << 10) + (((tileY << 5) + tileX) & 0x3FF));

    u8 tileVal = this->vram[0][offset];
    u16 tile = (lcdc & 0x10) != 0 ? tileVal : (u16) ((s8) tileVal + 0x100);

    u8 bank = 0;
    bool flipX = false;
    u8 ty = (u8) (y & 7);
    u8 baseDepth = 1;

    if(this->gameboy->gbMode == MODE_CGB) {
        u8 flags = this->vram[1][offset];

        line->palette = (u8) (flags & 7);
        bank = (u8) ((flags >> 3) & 1);
        flipX = (bool) ((flags >> 5) & 1);
        ty ^= (u8) (((flags >> 6) & 1) * 7);
        baseDepth = (u8) ((((flags >> 6) & 2) + 1) * (lcdc & 0x01));
    }

    u16 pxOffset = (u16) ((tile << 4) + (ty << 1));

    u8 b1 = this->vram[bank][pxOffset];
    u8 b2 = this->vram[bank][pxOffset + 1];

    if(!flipX) {
        b1 = BitReverseTable256[b1];
        b2 = BitReverseTable256[b2];
    }

    u16 pxData = (u16) (BitStretchTable256[b1] | (BitStretchTable256[b2] << 1));

    for(u8 tx = 0; tx < 8; tx++) {
        u8 color = (u8) ((pxData >> (tx << 1)) & 3);
        line->color[tx] = color;
        line->depth[tx] = (u8) ((baseDepth - (u8) (color == 0)) & 3);
    }
}

inline void PPU::updateLineSprites(u8 lcdc) {
    if((lcdc & 0x02) == 0) {
        this->currSprites = 0;
        return;
    }

    u8 ly = this->gameboy->mmu->readIO(LY);

    bool large = (lcdc & 4) != 0;
    u8 height = (u8) (large ? 16 : 8);

    this->currSprites = 0;
    const u32* const oamWords = (const u32*) this->oam;

    for(int i = 0; i < 40; i++) {
        u32 entry = oamWords[i];
        int sprY = (int) (entry & 0xFF) - 16;
        if(ly >= sprY && ly < sprY + height) {
            SpriteLine* line = &this->currSpriteLines[this->currSprites];

            line->x = (u8) (((entry >> 8) & 0xFF) - 8);
            u8 tile = (u8) (((entry >> 16) & 0xFF) & ~((u8) large));
            u8 flags = (u8) (entry >> 24);

            line->palette = (u8) (flags & 7);
            u8 bank = (u8) ((flags >> 3) & 1);
            line->obp = (u8) ((flags >> 4) & 1);
            u8 ty = (u8) (ly - sprY);
            ty ^= (u8) (((flags >> 6) & 1) * (height - 1));
            u8 depth = (u8) (2 - ((flags >> 6) & 2));

            u16 pxOffset = (u16) ((tile << 4) + (ty << 1));

            u8 b1 = this->vram[bank][pxOffset];
            u8 b2 = this->vram[bank][pxOffset + 1];

            if((flags & 0x20) != 0) {
                for(int bit = 0; bit < 8; bit++) {
                    line->color[bit] = (u8) (((b1 >> bit) & 1) | (((b2 >> bit) & 1) << 1));
                    line->depth[bit] = depth;
                }
            } else {
                for(int bit = 0; bit < 8; bit++) {
                    line->color[bit] = (u8) (((b1 >> (7 - bit)) & 1) | (((b2 >> (7 - bit)) & 1) << 1));
                    line->depth[bit] = depth;
                }
            }

            this->currSprites++;
            if(this->currSprites >= 10) {
                break;
            }
        }
    }
}

__attribute__((always_inline)) inline void PPU::updateScanline() {
    u8 mode = (u8) (this->gameboy->mmu->readIO(STAT) & 3);
    u8 ly = this->gameboy->mmu->readIO(LY);

    bool drawing = this->gameboy->settings.perPixelRendering && this->gameboy->settings.drawEnabled && mode == LCD_ACCESS_OAM_VRAM;
    if(drawing && this->scanlineX < 160) {
        while(this->scanlineX < 160 && this->gameboy->cpu->getCycle() >= this->lastScanlineCycle + ((this->scanlineX + 7) << this->halfSpeed)) {
            this->drawPixel(this->scanlineX, ly);
            this->scanlineX++;
        }
    }

    if(drawing && this->scanlineX < 160) {
        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + ((this->scanlineX + 7) << this->halfSpeed));
    } else {
        this->gameboy->cpu->setEventCycle(this->lastScanlineCycle + (modeCycles[mode] << this->halfSpeed));
    }
}

inline void PPU::drawPixel(u8 x, u8 y) {
    if(this->gameboy->settings.frameBuffer == NULL) {
        return;
    }

    u32* colorOut = &this->gameboy->settings.frameBuffer[(y + 40) * this->gameboy->settings.framePitch + (x + 48)];

    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u8 lcdc = this->gameboy->mmu->readIO(LCDC);
            if((lcdc & 0x80) != 0) {
                u32 colorDst = 0;
                u8 depthDst = 0;

                u32* baseBgPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u32*) this->bgPalette : grayScalePalette;
                u32* baseSprPalette = this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped() ? (u32*) this->sprPalette : grayScalePalette;

                // Background
                if(this->gameboy->gbMode == MODE_CGB || (lcdc & 0x01) != 0) {
                    u8 map = (u8) ((lcdc >> 3) & 1);
                    u8 pixelX = (u8) (x + this->gameboy->mmu->readIO(SCX));
                    u8 pixelY = (u8) (y + this->gameboy->mmu->readIO(SCY));
                    u8 subX = (u8) (pixelX & 7);

                    TileLine* line = &this->currTileLines[map];
                    if(subX == 0 || x == 0) {
                        this->updateLineTile(map, pixelX, pixelY);
                    }

                    u8 palette = this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)] : line->palette;

                    colorDst = baseBgPalette[(palette << 2) + this->expandedBgp[line->color[subX]]];
                    depthDst = line->depth[subX];
                }

                // Window
                if((lcdc & 0x20) != 0) {
                    u8 wx = this->gameboy->mmu->readIO(WX);
                    u8 wy = this->gameboy->mmu->readIO(WY);
                    if(y >= wy && x >= wx - 7) {
                        u8 map = (u8) ((lcdc >> 6) & 1);
                        u8 pixelX = (u8) ((x - wx + 7) & 0xFF);
                        u8 pixelY = (u8) ((y - wy) & 0xFF);
                        u8 subX = (u8) (pixelX & 7);

                        TileLine* line = &this->currTileLines[map];
                        if(subX == 0 || x == 0) {
                            this->updateLineTile(map, pixelX, pixelY);
                        }

                        u8 palette = this->gameboy->gbMode == MODE_SGB ? this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)] : line->palette;

                        colorDst = baseBgPalette[(palette << 2) + this->expandedBgp[line->color[subX]]];
                        depthDst = line->depth[subX];
                    }
                }

                // Sprites
                if((lcdc & 0x02) != 0) {
                    for(s8 spriteId = (s8) (this->currSprites - 1); spriteId >= 0; spriteId--) {
                        SpriteLine* line = &this->currSpriteLines[spriteId];

                        u8 subX = x - line->x;
                        if(subX > 7) {
                            continue;
                        }

                        u8 palette = line->palette;
                        if(this->gameboy->gbMode == MODE_SGB) {
                            palette += this->gameboy->sgb->getPaletteMap()[(y >> 3) * 20 + (x >> 3)];
                        }

                        u8 color = line->color[subX];
                        u8 depth = line->depth[subX];

                        if(color != 0 && depth >= depthDst) {
                            colorDst = baseSprPalette[(palette << 2) + this->expandedObp[(line->obp << 2) + color]];
                            depthDst = depth;
                        }
                    }
                }

                *colorOut = colorDst;
            }

            break;
        }
        case 2:
            *colorOut = 0;
            break;
        case 3:
            *colorOut = this->bgPalette[this->gameboy->mmu->readIO(BGP) & 3];
            break;
        default:
            break;
    }
}

inline void PPU::drawScanline(u8 scanline) {
    if(this->gameboy->settings.frameBuffer == nullptr) {
        return;
    }

    u32* lineBuffer = &this->gameboy->settings.frameBuffer[(scanline + 40) * this->gameboy->settings.framePitch + 48];

    switch(this->gameboy->sgb->getGfxMask()) {
        case 0: {
            u8 lcdc = this->gameboy->mmu->readIO(LCDC);
            if((lcdc & 0x80) != 0) {
                u8 depthBuffer[160];

                const bool isCGB = (this->gameboy->gbMode == MODE_CGB);
                const bool isSGB = (this->gameboy->gbMode == MODE_SGB);

                const u32* const baseBgPalette = (this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped()) ? (u32*) this->bgPalette : grayScalePalette;
                const u32* const baseSprPalette = (this->gameboy->gbMode != MODE_GB || !this->gameboy->mmu->isBiosMapped()) ? (u32*) this->sprPalette : grayScalePalette;

                const u8* const subSgbMap = isSGB ? &this->gameboy->sgb->getPaletteMap()[(scanline >> 3) * 20] : nullptr;

                const bool bgWinEnabled = isCGB || ((lcdc & 0x01) != 0);
                const u8 wx = this->gameboy->mmu->readIO(WX);
                const u8 wy = this->gameboy->mmu->readIO(WY);
                const bool windowVisible = bgWinEnabled && ((lcdc & 0x20) != 0) && (wy <= scanline) && (wy < 144) && (wx < 167);
                const s16 winLeft = (s16) (wx - 7);

                if(!bgWinEnabled) {
                    memset(depthBuffer, 0, sizeof(depthBuffer));
                    u32 clearColor = baseBgPalette[0];
                    for(u8 i = 0; i < 160; i++) {
                        lineBuffer[i] = clearColor;
                    }
                } else if(!windowVisible || winLeft > 0) {
                    // Background is visible (either fully or up to window start)
                    const u8 bgPixelLimit = windowVisible ? (u8) winLeft : 160;

                    u8 basePixelX = this->gameboy->mmu->readIO(SCX);
                    u8 baseTileX = basePixelX >> 3;
                    u8 baseSubTileX = (u8) (basePixelX & 7);

                    u8 pixelY = (u8) (scanline + this->gameboy->mmu->readIO(SCY));
                    u8 tileY = pixelY >> 3;
                    u8 subTileY = (u8) (pixelY & 7);

                    u16 lineMapOffset = (u16) (0x1800 + ((lcdc >> 3) & 1) * 0x400 + (tileY * 32));
                    const u8* const lineTileMap = &this->vram[0][lineMapOffset];
                    const u8* const lineFlagMap = &this->vram[1][lineMapOffset];

                    u8 maxTileX = (u8) ((bgPixelLimit + baseSubTileX + 7) >> 3);
                    if(maxTileX > 21) {
                        maxTileX = 21;
                    }

                    for(u8 tileX = 0; tileX < maxTileX; tileX++) {
                        s16 tileStartPixelX = (s16) (tileX * 8 - baseSubTileX);
                        if(tileStartPixelX >= bgPixelLimit) {
                            break;
                        }

                        u8 mapTileX = (u8) ((baseTileX + tileX) & 31);
                        u16 tileId = (u16) ((lcdc & 0x10) != 0 ? lineTileMap[mapTileX] : (s8) lineTileMap[mapTileX] + 0x100);
                        u8 flags = (u8) (isCGB ? lineFlagMap[mapTileX] : 0);

                        u8 paletteId = (u8) (flags & 7);
                        u8 bank = (u8) ((flags >> 3) & 1);
                        bool flipX = (bool) ((flags >> 5) & 1);
                        bool flipY = (bool) ((flags >> 6) & 1);
                        u8 depth = (u8) (1 + ((flags >> 6) & 2));

                        u16 offset = (u16) ((tileId * 0x10) + ((flipY ? 7 - subTileY : subTileY) * 2));

                        u8 b1 = this->vram[bank][offset];
                        u8 b2 = this->vram[bank][offset + 1];

                        if(!flipX) {
                            b1 = BitReverseTable256[b1];
                            b2 = BitReverseTable256[b2];
                        }

                        u16 pxData = (u16) (BitStretchTable256[b1] | (BitStretchTable256[b2] << 1));
                        const u32* const tilePalette = &baseBgPalette[paletteId << 2];

                        for(u8 x = 0; x < 8; x++) {
                            s16 pixelX = (s16) (tileStartPixelX + x);
                            if(pixelX < 0) {
                                continue;
                            }
                            if(pixelX >= bgPixelLimit) {
                                break;
                            }

                            u8 colorId = (u8) ((pxData >> (x << 1)) & 3);
                            depthBuffer[pixelX] = (isCGB && (lcdc & 0x01) == 0) ? 0 : (colorId == 0 ? (u8) (depth - 1) : depth);

                            u32 outputColor;
                            if(isCGB) {
                                outputColor = tilePalette[colorId];
                            } else if(isSGB) {
                                u8 palette = subSgbMap[pixelX >> 3];
                                outputColor = baseBgPalette[(palette << 2) + this->expandedBgp[colorId]];
                            } else {
                                outputColor = baseBgPalette[this->expandedBgp[colorId]];
                            }

                            lineBuffer[pixelX] = outputColor;
                        }
                    }
                }

                // Window
                if(windowVisible) {
                    s16 basePixelX = winLeft;
                    s16 baseTileX = (s16) (basePixelX >> 3);

                    u8 pixelY = (u8) (scanline - wy);
                    u8 tileY = pixelY >> 3;
                    u8 subTileY = (u8) (pixelY & 7);

                    u16 lineMapOffset = (u16) (0x1800 + ((lcdc >> 6) & 1) * 0x400 + (tileY * 32));
                    const u8* const lineTileMap = &this->vram[0][lineMapOffset];
                    const u8* const lineFlagMap = &this->vram[1][lineMapOffset];

                    u8 tileCount = (u8) (20 - baseTileX);
                    for(u8 tileX = 0; tileX < tileCount; tileX++) {
                        s16 tileStartPixelX = (s16) (tileX * 8 + basePixelX);
                        if(tileStartPixelX >= 160) {
                            break;
                        }

                        u16 tileId = (u16) ((lcdc & 0x10) != 0 ? lineTileMap[tileX] : (s8) lineTileMap[tileX] + 0x100);
                        u8 flags = (u8) (isCGB ? lineFlagMap[tileX] : 0);

                        u8 paletteId = (u8) (flags & 7);
                        u8 bank = (u8) ((flags >> 3) & 1);
                        bool flipX = (bool) ((flags >> 5) & 1);
                        bool flipY = (bool) ((flags >> 6) & 1);
                        u8 depth = (u8) (1 + ((flags >> 6) & 2));

                        u16 offset = (u16) ((tileId * 0x10) + ((flipY ? 7 - subTileY : subTileY) * 2));

                        u8 b1 = this->vram[bank][offset];
                        u8 b2 = this->vram[bank][offset + 1];

                        if(!flipX) {
                            b1 = BitReverseTable256[b1];
                            b2 = BitReverseTable256[b2];
                        }

                        u16 pxData = (u16) (BitStretchTable256[b1] | (BitStretchTable256[b2] << 1));
                        const u32* const tilePalette = &baseBgPalette[paletteId << 2];

                        for(u8 x = 0; x < 8; x++) {
                            s16 pixelX = (s16) (tileStartPixelX + x);
                            if(pixelX < 0) {
                                continue;
                            }
                            if(pixelX >= 160) {
                                break;
                            }

                            u8 colorId = (u8) ((pxData >> (x << 1)) & 3);
                            depthBuffer[pixelX] = (isCGB && (lcdc & 0x01) == 0) ? 0 : (colorId == 0 ? (u8) (depth - 1) : depth);

                            u32 outputColor;
                            if(isCGB) {
                                outputColor = tilePalette[colorId];
                            } else if(isSGB) {
                                u8 palette = subSgbMap[pixelX >> 3];
                                outputColor = baseBgPalette[(palette << 2) + this->expandedBgp[colorId]];
                            } else {
                                outputColor = baseBgPalette[this->expandedBgp[colorId]];
                            }

                            lineBuffer[pixelX] = outputColor;
                        }
                    }
                }

                // Sprites
                if((lcdc & 0x02) != 0) {
                    this->updateLineSprites(lcdc);
#ifdef BACKEND_3DS
                    if(this->stereoEnabled && this->sprBuffer != nullptr) {
                        u32* sprLine = &this->sprBuffer[(scanline + 40) * 256 + 48];
                        bool lineHasSprite = false;

                        for(s8 spriteId = (s8) (this->currSprites - 1); spriteId >= 0; spriteId--) {
                            SpriteLine* line = &this->currSpriteLines[spriteId];
                            const u32* const spritePalette = isCGB ? &baseSprPalette[line->palette << 2] : nullptr;

                            for(u8 x = 0; x < 8; x++) {
                                u8 pixelX = (u8) (line->x + x);
                                if(pixelX >= 160) {
                                    continue;
                                }

                                u8 colorId = line->color[x];
                                u8 depth = line->depth[x];
                                if(colorId != 0 && depth >= depthBuffer[pixelX]) {
                                    depthBuffer[pixelX] = depth;

                                    u32 outputColor;
                                    if(isCGB) {
                                        outputColor = spritePalette[colorId];
                                    } else if(isSGB) {
                                        u8 palette = line->palette + subSgbMap[pixelX >> 3];
                                        outputColor = baseSprPalette[(palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                    } else {
                                        outputColor = baseSprPalette[(line->palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                    }

                                    sprLine[pixelX] = outputColor | 0x000000FF;
                                    lineHasSprite = true;
                                }
                            }
                        }

                        if(lineHasSprite) {
                            this->sprDirty = true;
                            this->sprDrawnThisFrame = true;
                            if(scanline < this->sprMinY) {
                                this->sprMinY = scanline;
                            }
                            if(scanline > this->sprMaxY) {
                                this->sprMaxY = scanline;
                            }
                        }
                    } else {
                        for(s8 spriteId = (s8) (this->currSprites - 1); spriteId >= 0; spriteId--) {
                            SpriteLine* line = &this->currSpriteLines[spriteId];
                            const u32* const spritePalette = isCGB ? &baseSprPalette[line->palette << 2] : nullptr;

                            for(u8 x = 0; x < 8; x++) {
                                u8 pixelX = (u8) (line->x + x);
                                if(pixelX >= 160) {
                                    continue;
                                }

                                u8 colorId = line->color[x];
                                u8 depth = line->depth[x];
                                if(colorId != 0 && depth >= depthBuffer[pixelX]) {
                                    depthBuffer[pixelX] = depth;

                                    u32 outputColor;
                                    if(isCGB) {
                                        outputColor = spritePalette[colorId];
                                    } else if(isSGB) {
                                        u8 palette = line->palette + subSgbMap[pixelX >> 3];
                                        outputColor = baseSprPalette[(palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                    } else {
                                        outputColor = baseSprPalette[(line->palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                    }

                                    lineBuffer[pixelX] = outputColor;
                                }
                            }
                        }
                    }
#else
                    for(s8 spriteId = (s8) (this->currSprites - 1); spriteId >= 0; spriteId--) {
                        SpriteLine* line = &this->currSpriteLines[spriteId];
                        const u32* const spritePalette = isCGB ? &baseSprPalette[line->palette << 2] : nullptr;

                        for(u8 x = 0; x < 8; x++) {
                            u8 pixelX = (u8) (line->x + x);
                            if(pixelX >= 160) {
                                continue;
                            }

                            u8 colorId = line->color[x];
                            u8 depth = line->depth[x];
                            if(colorId != 0 && depth >= depthBuffer[pixelX]) {
                                depthBuffer[pixelX] = depth;

                                u32 outputColor;
                                if(isCGB) {
                                    outputColor = spritePalette[colorId];
                                } else if(isSGB) {
                                    u8 palette = line->palette + subSgbMap[pixelX >> 3];
                                    outputColor = baseSprPalette[(palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                } else {
                                    outputColor = baseSprPalette[(line->palette << 2) + this->expandedObp[(line->obp << 2) + colorId]];
                                }

                                lineBuffer[pixelX] = outputColor;
                            }
                        }
                    }
#endif
                }
            }

            break;
        }
        case 2:
            memset(lineBuffer, 0, 160 * sizeof(u32));
            break;
        case 3: {
            u32 clearColor = this->bgPalette[this->gameboy->mmu->readIO(BGP) & 3];
            for(u32 i = 0; i < 160; i++) {
                lineBuffer[i] = clearColor;
            }

            break;
        }
        default:
            break;
    }
}