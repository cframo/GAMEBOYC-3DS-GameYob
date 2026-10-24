#pragma once

#include "types.h"

class CheatEngine;
class Gameboy;

extern Gameboy* gameboy;
extern CheatEngine* cheatEngine;

extern int fastForwardCounter;

void mgrInit();
void mgrExit();

void mgrReset();

std::string mgrGetRomName();
void mgrPowerOn(const char* romFile);
void mgrPowerOff(bool save = true);
void mgrSelectRom();

void mgrWriteSave();

bool mgrStateExists(int stateNum);
const std::string mgrGetStateName(int stateNum);
bool mgrLoadState(int stateNum);
bool mgrSaveState(int stateNum);
void mgrDeleteState(int stateNum);
void mgrDeriveThumbPath(const char* statePath, char* outThumbPath, size_t maxLen);
void mgrGetStateThumbPath(int stateNum, char* outPath, size_t maxLen);
void mgrRequestAutoloadState(int slot);

void mgrRefreshPalette();
void mgrCyclePalette();
const char* mgrGetPaletteName(int mode);
const u32* mgrGetPaletteColors(int mode);
void mgrRefreshBorder();

bool mgrGetFastForward();
void mgrSetFastForward(bool ff);
void mgrToggleFastForward();

void mgrPause();
void mgrUnpause();
bool mgrIsPaused();

void mgrRun();
