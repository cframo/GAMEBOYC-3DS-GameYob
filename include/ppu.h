#pragma once

#include <istream>
#include <ostream>

#include "types.h"

class Gameboy;

class PPU {
public:
    PPU(Gameboy* gb);
    ~PPU();

    void reset();

    void loadState(std::istream& data, u8 version);
    void saveState(std::ostream& data);

    void update();

    void setHalfSpeed(bool halfSpeed);

    void transferTiles(u8* dest);

    static void initCgbColorLut();
    void refreshPalettes();

    void initBuffers();
    void clearSprBuffer();

    inline u8 readOam(u16 addr) {
        return this->oam[addr & 0xFF];
    }

    inline void writeOam(u16 addr, u8 val) {
        this->oam[addr & 0xFF] = val;
    }

    inline u32* getBgPalette() {
        return this->bgPalette;
    }

    inline u32* getSprPalette() {
        return this->sprPalette;
    }

    inline u32* getSprBuffer() {
        return this->sprBuffer;
    }

    inline bool hasSpritesThisFrame() const {
        return this->sprDrawnThisFrame;
    }

    inline void setStereoEnabled(bool enabled) {
        this->stereoEnabled = enabled;
    }

    inline u8 getSprMinY() const {
        return this->sprMinY;
    }

    inline u8 getSprMaxY() const {
        return this->sprMaxY;
    }

    u32* sprBuffer = nullptr;
    float currentSlider = 0.0f;
    bool stereoEnabled = false;
    bool sprDirty = false;
    bool sprDrawnThisFrame = false;
    u8 sprMinY = 144;
    u8 sprMaxY = 0;
private:
    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 palette;
    } TileLine;

    typedef struct {
        u8 color[8];
        u8 depth[8];
        u8 x;
        u8 palette;
        u8 obp;
    } SpriteLine;

    void mapBanks();

    __attribute__((always_inline)) inline void checkLYC();
    __attribute__((always_inline)) inline void updateStatSignal();

    void updateLineTile(u8 map, u8 x, u8 y);
    void updateLineSprites();

    __attribute__((always_inline)) inline void updateScanline();
    void drawPixel(u8 x, u8 y);
    void drawScanline(u8 scanline);

    Gameboy* gameboy;

    u64 lastScanlineCycle;
    u64 lastPhaseCycle;
    bool halfSpeed;
    bool statInterruptSignal;

    u8 scanlineX;

    TileLine currTileLines[2];
    SpriteLine currSpriteLines[10];
    u8 currSprites;

    u8 vram[2][0x2000];
    u8 oam[0xA0];
    u8 rawBgPalette[0x40];
    u8 rawSprPalette[0x40];

    u32 bgPalette[0x20];
    u32 sprPalette[0x20];

    u8 expandedBgp[4];
    u8 expandedObp[8];
};
