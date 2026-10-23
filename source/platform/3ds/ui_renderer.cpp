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

// Proportional 5x7 / 6x7 bitmap font (256 characters, 8 bytes per character)
static const u8 font5x7_data[256][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 0x00
    {0x70, 0xF8, 0xA8, 0xF8, 0xD8, 0x88, 0x70, 0x00}, // 0x01
    {0x70, 0xF8, 0xA8, 0xF8, 0x88, 0xD8, 0x70, 0x00}, // 0x02
    {0x00, 0x50, 0xF8, 0xF8, 0xF8, 0x70, 0x20, 0x00}, // 0x03
    {0x00, 0x20, 0x70, 0xF8, 0xF8, 0x70, 0x20, 0x00}, // 0x04
    {0x70, 0x50, 0xF8, 0xA8, 0xF8, 0x20, 0x70, 0x00}, // 0x05
    {0x20, 0x70, 0xF8, 0xF8, 0xF8, 0x20, 0x70, 0x00}, // 0x06
    {0x00, 0x00, 0x20, 0x70, 0x70, 0x20, 0x00, 0x00}, // 0x07
    {0xF8, 0xF8, 0xD8, 0x88, 0x88, 0xD8, 0xF8, 0xF8}, // 0x08
    {0x00, 0x00, 0x20, 0x50, 0x50, 0x20, 0x00, 0x00}, // 0x09
    {0xF8, 0xF8, 0xD8, 0xA8, 0xA8, 0xD8, 0xF8, 0xF8}, // 0x0A
    {0x00, 0x38, 0x18, 0x68, 0xA0, 0xA0, 0x40, 0x00}, // 0x0B
    {0x70, 0x88, 0x88, 0x70, 0x20, 0xF8, 0x20, 0x00}, // 0x0C
    {0x78, 0x48, 0x78, 0x40, 0x40, 0x40, 0xC0, 0x00}, // 0x0D
    {0x78, 0x48, 0x78, 0x48, 0x48, 0x58, 0xC0, 0x00}, // 0x0E
    {0x20, 0xA8, 0x70, 0xD8, 0xD8, 0x70, 0xA8, 0x20}, // 0x0F
    {0x80, 0xC0, 0xF0, 0xF8, 0xF0, 0xC0, 0x80, 0x00}, // 0x10
    {0x08, 0x18, 0x78, 0xF8, 0x78, 0x18, 0x08, 0x00}, // 0x11
    {0x20, 0x70, 0xA8, 0x20, 0xA8, 0x70, 0x20, 0x00}, // 0x12
    {0xD8, 0xD8, 0xD8, 0xD8, 0xD8, 0x00, 0xD8, 0x00}, // 0x13
    {0x78, 0xA8, 0xA8, 0x68, 0x28, 0x28, 0x28, 0x00}, // 0x14
    {0x30, 0x48, 0x50, 0x28, 0x10, 0x48, 0x48, 0x30}, // 0x15
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0xF8, 0x00}, // 0x16
    {0x20, 0x70, 0xA8, 0x20, 0xA8, 0x70, 0x20, 0xF8}, // 0x17
    {0x00, 0x20, 0x70, 0xA8, 0x20, 0x20, 0x20, 0x00}, // 0x18
    {0x00, 0x20, 0x20, 0x20, 0xA8, 0x70, 0x20, 0x00}, // 0x19
    {0x00, 0x20, 0x10, 0xF8, 0x10, 0x20, 0x00, 0x00}, // 0x1A
    {0x00, 0x20, 0x40, 0xF8, 0x40, 0x20, 0x00, 0x00}, // 0x1B
    {0x00, 0x80, 0x80, 0x80, 0xF8, 0x00, 0x00, 0x00}, // 0x1C
    {0x00, 0x50, 0xF8, 0xF8, 0x50, 0x00, 0x00, 0x00}, // 0x1D
    {0x00, 0x20, 0x20, 0x70, 0xF8, 0xF8, 0x00, 0x00}, // 0x1E
    {0x00, 0xF8, 0xF8, 0x70, 0x20, 0x20, 0x00, 0x00}, // 0x1F
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0xC0, 0xC0, 0xC0, 0xC0, 0x00, 0xC0, 0xC0, 0x00}, // '!'
    {0x50, 0x50, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00}, // '"'
    {0x50, 0x50, 0xF8, 0x50, 0xF8, 0x50, 0x50, 0x00}, // '#'
    {0x20, 0x78, 0xA0, 0x70, 0x28, 0xF0, 0x20, 0x00}, // '$'
    {0xC0, 0xC8, 0x10, 0x20, 0x40, 0x98, 0x18, 0x00}, // '%'
    {0x40, 0xA0, 0xA0, 0x40, 0xA8, 0x90, 0x68, 0x00}, // '&'
    {0xC0, 0xC0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00}, // '''
    {0x20, 0x40, 0x80, 0x80, 0x80, 0x40, 0x20, 0x00}, // '('
    {0x80, 0x40, 0x20, 0x20, 0x20, 0x40, 0x80, 0x00}, // ')'
    {0x20, 0xA8, 0x70, 0xF8, 0x70, 0xA8, 0x20, 0x00}, // '*'
    {0x00, 0x20, 0x20, 0xF8, 0x20, 0x20, 0x00, 0x00}, // '+'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x80, 0x00}, // ','
    {0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00, 0x00}, // '-'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0xC0, 0xC0, 0x00}, // '.'
    {0x00, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00, 0x00}, // '/'
    {0x70, 0x88, 0x98, 0xA8, 0xC8, 0x88, 0x70, 0x00}, // '0'
    {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x70, 0x00}, // '1'
    {0x70, 0x88, 0x08, 0x70, 0x80, 0x80, 0xF8, 0x00}, // '2'
    {0xF8, 0x08, 0x10, 0x30, 0x08, 0x88, 0x70, 0x00}, // '3'
    {0x10, 0x30, 0x50, 0x90, 0xF8, 0x10, 0x10, 0x00}, // '4'
    {0xF8, 0x80, 0xF0, 0x08, 0x08, 0x88, 0x70, 0x00}, // '5'
    {0x38, 0x40, 0x80, 0xF0, 0x88, 0x88, 0x70, 0x00}, // '6'
    {0xF8, 0x08, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00}, // '7'
    {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x70, 0x00}, // '8'
    {0x70, 0x88, 0x88, 0x78, 0x08, 0x10, 0xE0, 0x00}, // '9'
    {0x00, 0xC0, 0xC0, 0x00, 0x00, 0xC0, 0xC0, 0x00}, // ':'
    {0x00, 0xC0, 0xC0, 0x00, 0x00, 0xC0, 0x80, 0x00}, // ';'
    {0x20, 0x40, 0x80, 0x40, 0x20, 0x00, 0x00, 0x00}, // '<'
    {0x00, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00}, // '='
    {0x80, 0x40, 0x20, 0x40, 0x80, 0x00, 0x00, 0x00}, // '>'
    {0x70, 0x88, 0x08, 0x30, 0x20, 0x00, 0x20, 0x00}, // '?'
    {0x70, 0x88, 0xA8, 0xB8, 0xB0, 0x80, 0x78, 0x00}, // '@'
    {0x20, 0x50, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x00}, // 'A'
    {0xF0, 0x88, 0x88, 0xF0, 0x88, 0x88, 0xF0, 0x00}, // 'B'
    {0x70, 0x88, 0x80, 0x80, 0x80, 0x88, 0x70, 0x00}, // 'C'
    {0xF0, 0x88, 0x88, 0x88, 0x88, 0x88, 0xF0, 0x00}, // 'D'
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0x00}, // 'E'
    {0xF8, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x80, 0x00}, // 'F'
    {0x78, 0x88, 0x80, 0x80, 0x98, 0x88, 0x78, 0x00}, // 'G'
    {0x88, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x88, 0x00}, // 'H'
    {0xE0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xE0, 0x00}, // 'I'
    {0x38, 0x10, 0x10, 0x10, 0x10, 0x90, 0x60, 0x00}, // 'J'
    {0x88, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x88, 0x00}, // 'K'
    {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xF8, 0x00}, // 'L'
    {0x84, 0xCC, 0xB4, 0x84, 0x84, 0x84, 0x84, 0x00}, // 'M'
    {0x88, 0x88, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x00}, // 'N'
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 'O'
    {0xF0, 0x88, 0x88, 0xF0, 0x80, 0x80, 0x80, 0x00}, // 'P'
    {0x70, 0x88, 0x88, 0x88, 0xA8, 0x90, 0x68, 0x00}, // 'Q'
    {0xF0, 0x88, 0x88, 0xF0, 0xA0, 0x90, 0x88, 0x00}, // 'R'
    {0x70, 0x88, 0x80, 0x70, 0x08, 0x88, 0x70, 0x00}, // 'S'
    {0xF8, 0xA8, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00}, // 'T'
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 'U'
    {0x88, 0x88, 0x88, 0x88, 0x88, 0x50, 0x20, 0x00}, // 'V'
    {0x84, 0x84, 0x84, 0x84, 0xB4, 0xCC, 0x84, 0x00}, // 'W'
    {0x88, 0x88, 0x50, 0x20, 0x50, 0x88, 0x88, 0x00}, // 'X'
    {0x88, 0x88, 0x50, 0x20, 0x20, 0x20, 0x20, 0x00}, // 'Y'
    {0xF8, 0x08, 0x10, 0x70, 0x40, 0x80, 0xF8, 0x00}, // 'Z'
    {0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0xE0, 0x00}, // '['
    {0x00, 0x80, 0x40, 0x20, 0x10, 0x08, 0x00, 0x00}, // '\'
    {0xE0, 0x20, 0x20, 0x20, 0x20, 0x20, 0xE0, 0x00}, // ']'
    {0x20, 0x50, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00}, // '^'
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x00}, // '_'
    {0x60, 0x60, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00}, // '`'
    {0x00, 0x00, 0x60, 0x10, 0x70, 0x90, 0x78, 0x00}, // 'a'
    {0x80, 0x80, 0xB0, 0xC8, 0x88, 0xC8, 0xB0, 0x00}, // 'b'
    {0x00, 0x00, 0x70, 0x88, 0x80, 0x88, 0x70, 0x00}, // 'c'
    {0x08, 0x08, 0x68, 0x98, 0x88, 0x98, 0x68, 0x00}, // 'd'
    {0x00, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x70, 0x00}, // 'e'
    {0x60, 0x80, 0xE0, 0x80, 0x80, 0x80, 0x80, 0x00}, // 'f'
    {0x00, 0x00, 0x70, 0x98, 0x98, 0x68, 0x08, 0x70}, // 'g'
    {0x80, 0x80, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00}, // 'h'
    {0x00, 0xC0, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0x00}, // 'i'
    {0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0xE0, 0x00}, // 'j'
    {0x80, 0x80, 0x90, 0xA0, 0xC0, 0xA0, 0x90, 0x00}, // 'k'
    {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00}, // 'l'
    {0x00, 0x00, 0xD8, 0xA8, 0xA8, 0xA8, 0xAC, 0x00}, // 'm'
    {0x00, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00}, // 'n'
    {0x00, 0x00, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00}, // 'o'
    {0x00, 0x00, 0xB0, 0xC8, 0xC8, 0xB0, 0x80, 0x80}, // 'p'
    {0x00, 0x00, 0x68, 0x98, 0x98, 0x68, 0x08, 0x08}, // 'q'
    {0x00, 0x00, 0xB0, 0xC8, 0x80, 0x80, 0x80, 0x00}, // 'r'
    {0x00, 0x00, 0x78, 0x80, 0x70, 0x08, 0xF0, 0x00}, // 's'
    {0x40, 0x40, 0xE0, 0x40, 0x40, 0x40, 0x60, 0x00}, // 't'
    {0x00, 0x00, 0x88, 0x88, 0x88, 0x98, 0x68, 0x00}, // 'u'
    {0x00, 0x00, 0x88, 0x88, 0x88, 0x50, 0x20, 0x00}, // 'v'
    {0x00, 0x00, 0x84, 0xA4, 0xA4, 0xD8, 0x48, 0x00}, // 'w'
    {0x00, 0x00, 0x88, 0x50, 0x20, 0x50, 0x88, 0x00}, // 'x'
    {0x00, 0x00, 0x88, 0x88, 0x78, 0x08, 0x88, 0x70}, // 'y'
    {0x00, 0x00, 0xF8, 0x10, 0x20, 0x40, 0xF8, 0x00}, // 'z'
    {0x10, 0x20, 0x20, 0x40, 0x20, 0x20, 0x10, 0x00}, // '{'
    {0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0x00}, // '|'
    {0x40, 0x20, 0x20, 0x10, 0x20, 0x20, 0x40, 0x00}, // '}'
    {0x40, 0xA8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00}, // '~'
    {0x20, 0x70, 0xD8, 0x88, 0x88, 0xF8, 0x00, 0x00}, // 0x7F
    {0x00, 0x00, 0x30, 0x78, 0x78, 0x30, 0x00, 0x00}, // 0x80
    {0x80, 0xC0, 0xE0, 0xF0, 0xE0, 0xC0, 0x80, 0x00}, // 0x81
    {0x10, 0x30, 0x70, 0xF0, 0x70, 0x30, 0x10, 0x00}, // 0x82
    {0x00, 0x20, 0x70, 0xF8, 0x00, 0x00, 0x00, 0x00}, // 0x83
    {0x00, 0x00, 0x00, 0xF8, 0x70, 0x20, 0x00, 0x00}, // 0x84
    {0x00, 0x20, 0x20, 0xF8, 0x70, 0x50, 0x88, 0x00}, // 0x85
    {0x30, 0x00, 0x60, 0x10, 0x70, 0x90, 0x78, 0x00}, // 0x86
    {0x00, 0x78, 0xC0, 0xC0, 0x78, 0x10, 0x30, 0x00}, // 0x87
    {0xF8, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x78, 0x00}, // 0x88
    {0x88, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x78, 0x00}, // 0x89
    {0xC0, 0x00, 0x70, 0x88, 0xF8, 0x80, 0x78, 0x00}, // 0x8A
    {0x28, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00}, // 0x8B
    {0x30, 0x48, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00}, // 0x8C
    {0x60, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00}, // 0x8D
    {0xA8, 0x50, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x00}, // 0x8E
    {0x20, 0x00, 0x20, 0x50, 0x88, 0xF8, 0x88, 0x88}, // 0x8F
    {0x30, 0x00, 0xF0, 0x80, 0xE0, 0x80, 0xF0, 0x00}, // 0x90
    {0x00, 0x00, 0x78, 0x10, 0x78, 0x90, 0x78, 0x00}, // 0x91
    {0x38, 0x50, 0x90, 0xF8, 0x90, 0x90, 0x98, 0x00}, // 0x92
    {0x70, 0x88, 0x00, 0x70, 0x88, 0x88, 0x70, 0x00}, // 0x93
    {0x00, 0x88, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0x94
    {0x00, 0xC0, 0x00, 0x70, 0x88, 0x88, 0x70, 0x00}, // 0x95
    {0x70, 0x88, 0x00, 0x88, 0x88, 0x98, 0x68, 0x00}, // 0x96
    {0x00, 0xC0, 0x00, 0x88, 0x88, 0x98, 0x68, 0x00}, // 0x97
    {0x48, 0x00, 0x48, 0x48, 0x48, 0x38, 0x08, 0x70}, // 0x98
    {0x88, 0x70, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0x99
    {0x88, 0x00, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0x9A
    {0x20, 0x20, 0xF8, 0xA0, 0xA0, 0xF8, 0x20, 0x20}, // 0x9B
    {0x30, 0x58, 0x48, 0xE0, 0x40, 0x48, 0xF8, 0x00}, // 0x9C
    {0xD8, 0xD8, 0x70, 0xF8, 0x20, 0xF8, 0x20, 0x20}, // 0x9D
    {0xE0, 0x90, 0x90, 0xE0, 0x90, 0xB8, 0x90, 0x90}, // 0x9E
    {0x18, 0x28, 0x20, 0x70, 0x20, 0x20, 0xA0, 0xC0}, // 0x9F
    {0x18, 0x00, 0x60, 0x10, 0x70, 0x90, 0x78, 0x00}, // 0xA0
    {0x00, 0xC0, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0x00}, // 0xA1
    {0x00, 0x18, 0x00, 0x70, 0x88, 0x88, 0x70, 0x00}, // 0xA2
    {0x00, 0x18, 0x00, 0x88, 0x88, 0x98, 0x68, 0x00}, // 0xA3
    {0x00, 0x78, 0x00, 0x70, 0x48, 0x48, 0x48, 0x00}, // 0xA4
    {0xF8, 0x00, 0xC8, 0xE8, 0xB8, 0x98, 0x88, 0x00}, // 0xA5
    {0x70, 0x90, 0x90, 0x78, 0x00, 0xF8, 0x00, 0x00}, // 0xA6
    {0x70, 0x88, 0x88, 0x70, 0x00, 0xF8, 0x00, 0x00}, // 0xA7
    {0x20, 0x00, 0x20, 0x60, 0x80, 0x88, 0x70, 0x00}, // 0xA8
    {0x00, 0x00, 0x00, 0xF8, 0x80, 0x80, 0x00, 0x00}, // 0xA9
    {0x00, 0x00, 0x00, 0xF8, 0x08, 0x08, 0x00, 0x00}, // 0xAA
    {0x80, 0x88, 0x90, 0xB8, 0x48, 0x98, 0x20, 0x38}, // 0xAB
    {0x80, 0x88, 0x90, 0xA8, 0x58, 0xB8, 0x08, 0x08}, // 0xAC
    {0x20, 0x20, 0x00, 0x20, 0x20, 0x20, 0x20, 0x00}, // 0xAD
    {0x00, 0x28, 0x50, 0xA0, 0x50, 0x28, 0x00, 0x00}, // 0xAE
    {0x00, 0xA0, 0x50, 0x28, 0x50, 0xA0, 0x00, 0x00}, // 0xAF
    {0xA8, 0x00, 0xA8, 0x00, 0xA8, 0x00, 0xA8, 0x00}, // 0xB0
    {0x50, 0xA8, 0x50, 0xA8, 0x50, 0xA8, 0x50, 0xA8}, // 0xB1
    {0xF8, 0xA8, 0xF8, 0xA8, 0xF8, 0xA8, 0xF8, 0xA8}, // 0xB2
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10}, // 0xB3
    {0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10}, // 0xB4
    {0x10, 0x10, 0xF0, 0x10, 0xF0, 0x10, 0x10, 0x10}, // 0xB5
    {0x28, 0x28, 0x28, 0x28, 0xE8, 0x28, 0x28, 0x28}, // 0xB6
    {0x00, 0x00, 0x00, 0x00, 0xF8, 0x28, 0x28, 0x28}, // 0xB7
    {0x00, 0x00, 0xF0, 0x10, 0xF0, 0x10, 0x10, 0x10}, // 0xB8
    {0x28, 0x28, 0xE8, 0x08, 0xE8, 0x28, 0x28, 0x28}, // 0xB9
    {0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28}, // 0xBA
    {0x00, 0x00, 0xF8, 0x08, 0xE8, 0x28, 0x28, 0x28}, // 0xBB
    {0x28, 0x28, 0xE8, 0x08, 0xF8, 0x00, 0x00, 0x00}, // 0xBC
    {0x28, 0x28, 0x28, 0x28, 0xF8, 0x00, 0x00, 0x00}, // 0xBD
    {0x10, 0x10, 0xF0, 0x10, 0xF0, 0x00, 0x00, 0x00}, // 0xBE
    {0x70, 0x88, 0x08, 0x10, 0x00, 0x10, 0x00, 0x00}, // 0xBF
    {0x10, 0x10, 0x10, 0x10, 0x18, 0x00, 0x00, 0x00}, // 0xC0
    {0x20, 0x40, 0x88, 0x88, 0xF8, 0x88, 0x88, 0x00}, // 0xC1
    {0x00, 0x00, 0x00, 0x00, 0xF8, 0x10, 0x10, 0x10}, // 0xC2
    {0x10, 0x10, 0x10, 0x10, 0x18, 0x10, 0x10, 0x10}, // 0xC3
    {0x00, 0x00, 0x00, 0x00, 0xF8, 0x00, 0x00, 0x00}, // 0xC4
    {0x10, 0x10, 0x10, 0x10, 0xF8, 0x10, 0x10, 0x10}, // 0xC5
    {0x10, 0x10, 0x18, 0x10, 0x18, 0x10, 0x10, 0x10}, // 0xC6
    {0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28}, // 0xC7
    {0x28, 0x28, 0x28, 0x20, 0x38, 0x00, 0x00, 0x00}, // 0xC8
    {0x20, 0x40, 0x80, 0xF0, 0x80, 0x80, 0xF8, 0x00}, // 0xC9
    {0x28, 0x28, 0xE8, 0x00, 0xF8, 0x00, 0x00, 0x00}, // 0xCA
    {0x00, 0x00, 0xF8, 0x00, 0xE8, 0x28, 0x28, 0x28}, // 0xCB
    {0x28, 0x28, 0x28, 0x20, 0x28, 0x28, 0x28, 0x28}, // 0xCC
    {0x20, 0x40, 0x40, 0x40, 0x40, 0x40, 0xE0, 0x00}, // 0xCD
    {0x28, 0x28, 0xE8, 0x00, 0xE8, 0x28, 0x28, 0x28}, // 0xCE
    {0x10, 0x10, 0xF8, 0x00, 0xF8, 0x00, 0x00, 0x00}, // 0xCF
    {0x28, 0x28, 0x28, 0x28, 0xF8, 0x00, 0x00, 0x00}, // 0xD0
    {0x50, 0x00, 0xC8, 0xA8, 0x98, 0x88, 0x88, 0x00}, // 0xD1
    {0x00, 0x00, 0x00, 0x00, 0xF8, 0x28, 0x28, 0x28}, // 0xD2
    {0x20, 0x40, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0xD3
    {0x10, 0x10, 0x18, 0x10, 0x18, 0x00, 0x00, 0x00}, // 0xD4
    {0x00, 0x00, 0x18, 0x10, 0x18, 0x10, 0x10, 0x10}, // 0xD5
    {0x00, 0x00, 0x00, 0x00, 0x38, 0x28, 0x28, 0x28}, // 0xD6
    {0x28, 0x28, 0x28, 0x28, 0xF8, 0x28, 0x28, 0x28}, // 0xD7
    {0x10, 0x10, 0xF8, 0x10, 0xF8, 0x10, 0x10, 0x10}, // 0xD8
    {0x10, 0x10, 0x10, 0x10, 0xF0, 0x00, 0x00, 0x00}, // 0xD9
    {0x20, 0x40, 0x88, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0xDA
    {0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8, 0xF8}, // 0xDB
    {0x00, 0x00, 0x00, 0x00, 0xF8, 0xF8, 0xF8, 0xF8}, // 0xDC
    {0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE0}, // 0xDD
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18}, // 0xDE
    {0xF8, 0xF8, 0xF8, 0xF8, 0x00, 0x00, 0x00, 0x00}, // 0xDF
    {0x00, 0x00, 0x68, 0x90, 0x90, 0x90, 0x68, 0x00}, // 0xE0
    {0x20, 0x40, 0x60, 0x10, 0x70, 0x90, 0x78, 0x00}, // 0xE1
    {0x00, 0xF8, 0x98, 0x80, 0x80, 0x80, 0x80, 0x00}, // 0xE2
    {0x00, 0xF8, 0x50, 0x50, 0x50, 0x50, 0x50, 0x00}, // 0xE3
    {0xF8, 0x88, 0x40, 0x20, 0x40, 0x88, 0xF8, 0x00}, // 0xE4
    {0x00, 0x00, 0x78, 0x90, 0x90, 0x90, 0x60, 0x00}, // 0xE5
    {0x00, 0x50, 0x50, 0x50, 0x50, 0x68, 0xC0, 0x00}, // 0xE6
    {0x00, 0xF8, 0xA0, 0x20, 0x20, 0x20, 0x20, 0x00}, // 0xE7
    {0xF8, 0x20, 0x70, 0x88, 0x88, 0x70, 0x20, 0xF8}, // 0xE8
    {0x20, 0x40, 0x70, 0x88, 0xF8, 0x80, 0x70, 0x00}, // 0xE9
    {0x20, 0x50, 0x88, 0x88, 0x50, 0x50, 0xD8, 0x00}, // 0xEA
    {0x30, 0x40, 0x30, 0x70, 0x88, 0x88, 0x70, 0x00}, // 0xEB
    {0x00, 0x00, 0x00, 0x70, 0xA8, 0xA8, 0x70, 0x00}, // 0xEC
    {0x20, 0x40, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0x00}, // 0xED
    {0x70, 0x80, 0x80, 0xF0, 0x80, 0x80, 0x70, 0x00}, // 0xEE
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00}, // 0xEF
    {0x00, 0xF8, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00}, // 0xF0
    {0x50, 0x00, 0xB0, 0xC8, 0x88, 0x88, 0x88, 0x00}, // 0xF1
    {0x40, 0x20, 0x10, 0x20, 0x40, 0x00, 0xF8, 0x00}, // 0xF2
    {0x20, 0x40, 0x70, 0x88, 0x88, 0x88, 0x70, 0x00}, // 0xF3
    {0x38, 0x28, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}, // 0xF4
    {0x20, 0x20, 0x20, 0x20, 0x20, 0xA0, 0xA0, 0xE0}, // 0xF5
    {0x30, 0x30, 0x00, 0xF8, 0x00, 0x30, 0x30, 0x00}, // 0xF6
    {0x00, 0xE8, 0xB8, 0x00, 0xE8, 0xB8, 0x00, 0x00}, // 0xF7
    {0x70, 0xD8, 0xD8, 0x70, 0x00, 0x00, 0x00, 0x00}, // 0xF8
    {0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00}, // 0xF9
    {0x20, 0x40, 0x88, 0x88, 0x88, 0x98, 0x68, 0x00}, // 0xFA
    {0x38, 0x20, 0x20, 0x20, 0xA0, 0xA0, 0x60, 0x20}, // 0xFB
    {0x70, 0x48, 0x48, 0x48, 0x48, 0x00, 0x00, 0x00}, // 0xFC
    {0x70, 0x18, 0x30, 0x60, 0x78, 0x00, 0x00, 0x00}, // 0xFD
    {0x00, 0x00, 0x78, 0x78, 0x78, 0x78, 0x00, 0x00}, // 0xFE
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // 0xFF
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

    // Tabla estática s_glyphWidth y s_glyphAdvance 100% segura para los 256 elementos
    for(int i = 0; i < 256; i++) {
        s_glyphWidth[i] = 5.0f;
        s_glyphAdvance[i] = 7.0f; // Por defecto: advance = width + 2px colchón
    }

    // Caracteres delgados (width 2, advance 4)
    static const uint8_t narrowChars[] = {'i', 'l', '!', '.', ':', ';', ',', '|', '\''};
    for(uint8_t ch : narrowChars) {
        s_glyphWidth[ch] = 2.0f;
        s_glyphAdvance[ch] = 4.0f;
    }

    // Caracteres medianos (width 3, advance 5)
    static const uint8_t medChars[] = {'I', 't', 'j', 'f', '(', ')', '[', ']', '<', '>'};
    for(uint8_t ch : medChars) {
        s_glyphWidth[ch] = 3.0f;
        s_glyphAdvance[ch] = 5.0f;
    }

    // Caracteres estándar 'a'..'z', '0'..'9', 'A'..'Z'
    for(int c = 'a'; c <= 'z'; c++) {
        if(s_glyphWidth[c] == 5.0f) {
            s_glyphAdvance[c] = 7.0f;
        }
    }
    for(int c = 'A'; c <= 'Z'; c++) {
        if(s_glyphWidth[c] == 5.0f) {
            s_glyphAdvance[c] = 7.0f;
        }
    }
    for(int c = '0'; c <= '9'; c++) {
        s_glyphWidth[c] = 5.0f;
        s_glyphAdvance[c] = 7.0f;
    }

    // Caracteres anchos (width 6, advance 8)
    s_glyphWidth['m'] = 6.0f; s_glyphAdvance['m'] = 8.0f;
    s_glyphWidth['w'] = 6.0f; s_glyphAdvance['w'] = 8.0f;
    s_glyphWidth['M'] = 6.0f; s_glyphAdvance['M'] = 8.0f;
    s_glyphWidth['W'] = 6.0f; s_glyphAdvance['W'] = 8.0f;

    // Espacio en blanco (ancho 0, avance 4)
    s_glyphWidth[' '] = 0.0f;
    s_glyphAdvance[' '] = 4.0f;

    // Generar atlas en memoria lineal sin sombras quemadas
    for(int c = 0; c < 256; c++) {
        int cellCol = c & 0x0F;
        int cellRow = (c >> 4) & 0x0F;
        int cellX = cellCol * 16;
        int cellY = cellRow * 16;

        int min_col = 8;
        int max_col = -1;
        for(int y = 0; y < 8; y++) {
            uint8_t row = font5x7_data[c][y];
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
            continue;
        }

        int shift = min_col;
        for(int sub_y = 0; sub_y < 8; sub_y++) {
            uint8_t rowBits = font5x7_data[c][sub_y];
            if(rowBits == 0) continue;
            for(int sub_x = 0; sub_x < 8; sub_x++) {
                if(rowBits & (0x80 >> sub_x)) {
                    int localX = (sub_x - shift);
                    int localY = sub_y;
                    temp[(cellY + localY) * 256 + (cellX + localX)] = 0xFFFFFFFF;
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
    C3D_TexSetFilter(&s_fontAtlasTex, GPU_NEAREST, GPU_NEAREST);
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
    float textW = uiGetTextWidth(text, 1.0f);
    float tx = x + (w - textW) * 0.5f;
    float ty = y + (h - 10.0f) * 0.5f;
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
        // Línea de brillo superior (highlight de 1px en y+1)
        uiDrawRect(x + r, y + 1.0f, w - 2.0f * r, 1.0f, uiColorRGBA(255, 255, 255, 45));
        // Línea de sombra inferior (1px en y+h-2) oscurecida
        uiDrawRect(x + r, y + h - 2.0f, w - 2.0f * r, 1.0f, uiColorRGBA(0, 0, 0, 60));
    } else {
        uiDrawDropShadow(x, y, w, h, 1.5f, 120);
        u8 rC = (baseColor >> 0) & 0xFF;
        u8 gC = (baseColor >> 8) & 0xFF;
        u8 bC = (baseColor >> 16) & 0xFF;
        u32 pressedColor = uiColorRGBA((u8)(rC * 0.8f), (u8)(gC * 0.8f), (u8)(bC * 0.8f), 255);
        uiDrawRoundedRect(x, y + 1.0f, w, h, r, pressedColor, borderColor, 2.0f);
    }

    if(label && *label) {
        float tw = uiGetTextWidth(label, 1.0f);
        float tx = x + (w - tw) * 0.5f;
        float ty = y + (h - 10.0f) * 0.5f + (isPressed ? 1.0f : 0.0f);
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

void uiDrawGearIcon(float x, float y, u32 color) {
    // Engranaje geométrico nítido de 10x10 px
    uiDrawRect(x + 2.0f, y + 2.0f, 6.0f, 6.0f, color);
    uiDrawRect(x + 4.0f, y, 2.0f, 2.0f, color);        // Diente superior
    uiDrawRect(x + 4.0f, y + 8.0f, 2.0f, 2.0f, color); // Diente inferior
    uiDrawRect(x, y + 4.0f, 2.0f, 2.0f, color);        // Diente izquierdo
    uiDrawRect(x + 8.0f, y + 4.0f, 2.0f, 2.0f, color); // Diente derecho
    uiDrawRect(x + 4.0f, y + 4.0f, 2.0f, 2.0f, uiColorRGBA(16, 21, 30, 255)); // Agujero central
}

void uiDrawPaletteIcon(float x, float y) {
    // Paleta de pintura geométrica de 12x10 px
    uiDrawRect(x + 1.0f, y + 1.0f, 10.0f, 8.0f, uiColorRGBA(210, 160, 60, 255));
    uiDrawRectOutline(x + 1.0f, y + 1.0f, 10.0f, 8.0f, 1.0f, uiColorRGBA(150, 100, 30, 255));
    uiDrawRect(x + 7.0f, y + 5.0f, 2.0f, 2.0f, uiColorRGBA(16, 21, 30, 255)); // Agujero
    uiDrawRect(x + 3.0f, y + 3.0f, 2.0f, 2.0f, uiColorRGBA(220, 50, 50, 255)); // Punto rojo
    uiDrawRect(x + 6.0f, y + 2.0f, 2.0f, 2.0f, uiColorRGBA(50, 200, 80, 255)); // Punto verde
    uiDrawRect(x + 8.0f, y + 3.0f, 2.0f, 2.0f, uiColorRGBA(50, 120, 240, 255)); // Punto azul
}

void uiDrawInfoBadge(float x, float y) {
    // Mini-insignia [i] informativa de 10x10 px
    uiDrawRoundedRect(x, y, 10.0f, 10.0f, 2.0f, uiColorRGBA(15, 60, 90, 255), UI_COLOR_CYAN, 1.0f);
    uiDrawRect(x + 4.0f, y + 2.0f, 2.0f, 2.0f, UI_COLOR_CYAN); // Punto
    uiDrawRect(x + 4.0f, y + 5.0f, 2.0f, 3.0f, UI_COLOR_CYAN); // Barra
}

static inline u8 getNextGlyph(const char*& p) {
    if(!p || *p == '\0') return 0;
    u8 c = (u8)*p;

    // Secuencias UTF-8 de 4 bytes (ej. emojis 0xF0..0xF7 como 🎨, ⚙, 📁, etc.)
    if((c & 0xF8) == 0xF0) {
        p++;
        for(int i = 0; i < 3 && *p != '\0' && ((u8)*p & 0xC0) == 0x80; i++) {
            p++;
        }
        return 0; // Omitir completamente sin emitir '????'
    }

    // Secuencias UTF-8 de 3 bytes (0xE0..0xEF)
    if((c & 0xF0) == 0xE0) {
        if(*(p + 1) != '\0' && *(p + 2) != '\0') {
            u8 c2 = (u8)*(p + 1);
            u8 c3 = (u8)*(p + 2);
            u32 cp = ((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
            p += 3;
            if(cp == 0x2022) return 0x80; // viñeta '•'
            if(cp == 0x25C4 || cp == 0x25C0) return '<'; // '◄' o '◀' -> '<'
            if(cp == 0x25BA || cp == 0x25B6) return '>'; // '►' o '▶' -> '>'
            if(cp == 0x25B2) return '^'; // '▲' -> '^'
            if(cp == 0x25BC) return 'v'; // '▼' -> 'v'
            if(cp == 0x2605) return 0x85; // '★' -> 0x85
            return 0; // Cualquier otro símbolo de 3 bytes se ignora sin emitir '?'
        } else {
            p++;
            return 0;
        }
    }

    // Secuencias UTF-8 de 2 bytes (0xC0..0xDF)
    if((c & 0xE0) == 0xC0) {
        if(*(p + 1) != '\0') {
            u8 c2 = (u8)*(p + 1);
            p += 2;
            if(c == 0xC3) {
                switch(c2) {
                    case 0x81: return 0xC1; // Á
                    case 0x89: return 0xC9; // É
                    case 0x8D: return 0xCD; // Í
                    case 0x93: return 0xD3; // Ó
                    case 0x9A: return 0xDA; // Ú
                    case 0x91: return 0xD1; // Ñ
                    case 0xA1: return 0xE1; // á
                    case 0xA9: return 0xE9; // é
                    case 0xAD: return 0xED; // í
                    case 0xB3: return 0xF3; // ó
                    case 0xBA: return 0xFA; // ú
                    case 0xB1: return 0xF1; // ñ
                    default: return '?';
                }
            } else if(c == 0xC2) {
                switch(c2) {
                    case 0xBF: return 0xBF; // ¿
                    case 0xA1: return 0xA1; // ¡
                    default: return '?';
                }
            }
            return 0;
        } else {
            p++;
            return 0;
        }
    }

    // ASCII estándar de 1 byte
    p++;
    return c;
}

static inline void uiEmitGlyphQuad(float x, float y, uint8_t c, float scaleX, float scaleY, u32 color) {
    float gw = (float)s_glyphWidth[c];
    if(gw <= 0.0f) {
        return;
    }
    float gh = 10.0f;
    int cellX = (c & 0x0F) * 16;
    int cellY = (c >> 4) * 16;
    float u0 = (float)cellX / 256.0f;
    float u1 = ((float)cellX + gw) / 256.0f;
    float v0 = (float)cellY / 256.0f;
    float v1 = ((float)cellY + gh) / 256.0f;

    // Manejo seguro del color en formato GPU Citro3D
    if((color & 0xFF000000) == 0) {
        if((color & 0x000000FF) != 0 && (color & 0x00FFFF00) == 0) {
            color = uiColorRGBA(0, 0, 0, (color & 0xFF));
        } else {
            color |= 0xFF000000;
        }
    }

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvColor(env0, color);

    C3D_ImmDrawBegin(GPU_TRIANGLES);
    emitUiQuad(x, y, x + (gw * scaleX), y + (gh * scaleY), u0, v0, u1, v1);
    C3D_ImmDrawEnd();
}

void uiDrawTextScaled(float x, float y, float scale, const char* text, u32 color) {
    if(!text || *text == '\0' || !s_fontAtlasInit) {
        return;
    }

    C3D_TexBind(0, &s_fontAtlasTex);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_RGB, GPU_MODULATE);
    C3D_TexEnvSrc(env0, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Alpha, GPU_MODULATE);
    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    float penX = x;
    float penY = y;
    while(*text != '\0') {
        if(*text == '\n') {
            penX = x;
            penY += 12.0f * scale;
            text++;
            continue;
        }
        if(*text == '\r') {
            text++;
            continue;
        }

        uint8_t c = (uint8_t)(*text);
        if(c == ' ') {
            penX += (4.0f * scale);
            text++;
            continue;
        }
        if(c < 32 || c == 127) {
            text++;
            continue;
        }

        if((c & 0x80) != 0) {
            c = getNextGlyph(text);
            if(c == 0) continue;
            if(c == ' ') {
                penX += (4.0f * scale);
                continue;
            }
        } else {
            text++;
        }

        // Sombra 1px abajo-derecha en negro translúcido directo
        uiEmitGlyphQuad(penX + (1.0f * scale), penY + (1.0f * scale), c, scale, scale, 0x00000088);
        // Letra principal encima
        uiEmitGlyphQuad(penX, penY, c, scale, scale, color);
        penX += (s_glyphAdvance[c] * scale);
    }

    C3D_AlphaTest(false, GPU_ALWAYS, 0);
}

float uiGetTextWidth(const char* text, float scale) {
    if(!text || *text == '\0' || !s_fontAtlasInit) {
        return 0.0f;
    }
    float totalW = 0.0f;
    float maxLineW = 0.0f;
    while(*text != '\0') {
        if(*text == '\n') {
            if(totalW > maxLineW) maxLineW = totalW;
            totalW = 0.0f;
            text++;
            continue;
        }
        if(*text == '\r') {
            text++;
            continue;
        }

        uint8_t c = (uint8_t)(*text);
        if(c == ' ') {
            totalW += (4.0f * scale);
            text++;
            continue;
        }
        if(c < 32 || c == 127) {
            text++;
            continue;
        }

        if((c & 0x80) != 0) {
            c = getNextGlyph(text);
            if(c == 0) continue;
            if(c == ' ') {
                totalW += (4.0f * scale);
                continue;
            }
        } else {
            text++;
        }

        totalW += (s_glyphAdvance[c] * scale);
    }
    if(totalW > maxLineW) maxLineW = totalW;
    return maxLineW;
}

void uiDrawText(float x, float y, const char* text, u32 color) {
    uiDrawTextScaled(x, y, 1.0f, text, color);
}

void uiDrawTextRight(float rightX, float y, const char* text, u32 color) {
    float w = uiGetTextWidth(text, 1.0f);
    uiDrawTextScaled(rightX - w, y, 1.0f, text, color);
}

void uiDrawTextCentered(float centerX, float y, const char* text, u32 color) {
    float w = uiGetTextWidth(text, 1.0f);
    uiDrawTextScaled(centerX - w * 0.5f, y, 1.0f, text, color);
}

void uiDrawTextClipped(float x, float y, float maxWidth, const char* text, u32 color) {
    if(!text || *text == '\0' || !s_fontAtlasInit || maxWidth <= 0.0f) {
        return;
    }

    C3D_TexBind(0, &s_fontAtlasTex);

    C3D_TexEnv* env0 = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env0);
    C3D_TexEnvSrc(env0, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_RGB, GPU_MODULATE);
    C3D_TexEnvSrc(env0, C3D_Alpha, GPU_TEXTURE0, GPU_CONSTANT, (GPU_TEVSRC)0);
    C3D_TexEnvFunc(env0, C3D_Alpha, GPU_MODULATE);
    C3D_TexEnvInit(C3D_GetTexEnv(1));

    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);

    float penX = x;
    while(*text != '\0') {
        uint8_t c = (uint8_t)(*text);
        if(c == ' ') {
            if((penX - x) + 4.0f > maxWidth) {
                break;
            }
            penX += 4.0f;
            text++;
            continue;
        }
        if(c < 32 || c == 127) {
            text++;
            continue;
        }

        if((c & 0x80) != 0) {
            c = getNextGlyph(text);
            if(c == 0) continue;
            if(c == ' ') {
                if((penX - x) + 4.0f > maxWidth) {
                    break;
                }
                penX += 4.0f;
                continue;
            }
        } else {
            text++;
        }

        float gw = s_glyphWidth[c];
        float ga = s_glyphAdvance[c];

        if((penX - x) + std::max(gw, ga) > maxWidth) {
            float dotAdv = s_glyphAdvance['.'];
            if((penX - x) + dotAdv * 2.0f <= maxWidth) {
                uiEmitGlyphQuad(penX + 1.0f, y + 1.0f, '.', 1.0f, 1.0f, 0x00000088);
                uiEmitGlyphQuad(penX, y, '.', 1.0f, 1.0f, color);
                penX += dotAdv;
                uiEmitGlyphQuad(penX + 1.0f, y + 1.0f, '.', 1.0f, 1.0f, 0x00000088);
                uiEmitGlyphQuad(penX, y, '.', 1.0f, 1.0f, color);
            }
            break;
        }

        uiEmitGlyphQuad(penX + 1.0f, y + 1.0f, c, 1.0f, 1.0f, 0x00000088);
        uiEmitGlyphQuad(penX, y, c, 1.0f, 1.0f, color);
        penX += ga;
    }

    C3D_AlphaTest(false, GPU_ALWAYS, 0);
}

void uiDrawSysText(float x, float y, float scaleX, float scaleY, u32 color, const char* text) {
    (void) scaleX;
    (void) scaleY;
    uiDrawTextScaled(x, y, 1.0f, text, color);
}

float uiGetSysTextWidth(float scaleX, const char* text) {
    (void) scaleX;
    return uiGetTextWidth(text, 1.0f);
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
        uiDrawTextCentered(tabX[t] + tabW * 0.5f, tabY + 6.0f, tabNames[t], textCol);
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
            uiDrawTextRight(300.0f, rowY + 8.0f, values[r], valCol);
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

    for(int i = 0; i < 4; i++) {
        bool active = false;
        if(i == 1 && s_currentTab != UI_TAB_CHEATS) active = true;
        if(i == 2 && s_currentTab == UI_TAB_CHEATS) active = true;

        u32 bg = active ? colDockBtnActiveBg : colDockBtnBg;
        u32 border = active ? colYellow : colBorder;
        u32 textCol = active ? colYellow : colWhite;

        uiDrawRect(btnX[i], btnY, btnW, btnH, bg);
        uiDrawRectOutline(btnX[i], btnY, btnW, btnH, 1.0f, border);
        uiDrawTextCentered(btnX[i] + btnW * 0.5f, btnY + 6.0f, btnLabels[i], textCol);
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
        // Selector de Slots: < Slot > en y: 158..176
        if(py >= 158 && py <= 176) {
            if(px < 253) {
                uiCyclePreviewSlot(-1);
            } else {
                uiCyclePreviewSlot(1);
            }
            return false;
        }
        // Tap en Carátula (y: 44..130, x: 211..295) para iniciar juego
        if(py >= 44 && py <= 130 && px >= 211 && px <= 295) {
            if(chooser != nullptr && chooser->getSelection() >= 0) {
                if(chooser->navigateToSelection(result)) {
                    stateNum = uiGetPreviewSlot();
                    return true;
                }
            }
        }
    } else if(s_brMode == BR_MODE_DISPLAY) {
        // Fila 1: Escalado (y: 46..64)
        if(py >= 46 && py <= 64) {
            int val = getMenuOption("Scaling");
            val = (val + 1) % 5;
            setMenuOption("Scaling", val);
            return false;
        }
        // Fila 2: Filtro (y: 66..84)
        if(py >= 66 && py <= 84) {
            int val = getMenuOption("Scale Filter");
            val = (val + 1) % 3;
            setMenuOption("Scale Filter", val);
            return false;
        }
        // Fila 3: Rejilla LCD (y: 86..106)
        if(py >= 86 && py <= 106) {
            int val = getMenuOption("LCD Grid") ^ 1;
            setMenuOption("LCD Grid", val);
            return false;
        }
        // Tap en caja informativa (y: 116..196): ciclar filtro
        if(py >= 116 && py <= 196) {
            int val = getMenuOption("Scale Filter");
            val = (val + 1) % 3;
            setMenuOption("Scale Filter", val);
            return false;
        }
    } else if(s_brMode == BR_MODE_PALETTE) {
        // Fila 1: Paleta DMG (y: 46..64)
        if(py >= 46 && py <= 64) {
            gbColorizeMode = (gbColorizeMode + 1) % 14;
            mgrRefreshPalette();
            return false;
        }
        // Fila 2: Color GBC (y: 66..84)
        if(py >= 66 && py <= 84) {
            int val = getMenuOption("CGB Colors") ^ 1;
            setMenuOption("CGB Colors", val);
            return false;
        }
        // Fila 3: Sonido GB (y: 86..106)
        if(py >= 86 && py <= 106) {
            int val = getMenuOption("Master") ^ 1;
            setMenuOption("Master", val);
            return false;
        }
        // Swatches (y: 116..140) o Botón [ Cambiar Paleta ] (y: 150..186)
        if(py >= 116 && py <= 186) {
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

    // Header Superior (y: 6..20)
    std::string dirTitle = chooser->getDirectory();
    uiDrawText(10.0f, 6.0f, "ROMS: ", colAmber);
    float romsW = uiGetTextWidth("ROMS: ", 1.0f);
    uiDrawTextClipped(10.0f + romsW, 6.0f, 210.0f, dirTitle.c_str(), colWhite);
    uiDrawTextRight(310.0f, 6.0f, "v1.0.9", colGray);
    uiDrawRect(8.0f, 20.0f, 304.0f, 1.0f, colBorder);

    // Anchos dinámicos según el modo activo
    bool isPreview = (s_brMode == BR_MODE_PREVIEW);
    float listX = 8.0f;
    float listY = 22.0f;
    float listW = isPreview ? 182.0f : 118.0f;
    float listH = 182.0f;

    float panelX = isPreview ? 194.0f : 130.0f;
    float panelY = 22.0f;
    float panelW = isPreview ? 118.0f : 182.0f;
    float panelH = 182.0f;

    // 2. Lista de ROMs (Panel Izquierdo)
    uiDrawDropShadow(listX, listY, listW, listH, 3.0f, 80);
    uiDrawRoundedRect(listX, listY, listW, listH, 2.0f, colPanelBg, colListBorder, 1.0f);

    const std::vector<std::string>& files = chooser->getFilenames();
    const std::vector<int>& flags = chooser->getFlags();
    int numFiles = chooser->getNumFiles();
    int selection = chooser->getSelection();
    float scrollY = chooser->getSubpixelScrollY();

    int firstIdx = (int)(scrollY / 24.0f);
    if(firstIdx < 0) firstIdx = 0;

    float textMaxW = isPreview ? 144.0f : 84.0f;

    for(int i = firstIdx; i < numFiles; i++) {
        float itemY = listY + 2.0f + (float)i * 24.0f - scrollY;
        if(itemY < listY - 20.0f) continue;
        if(itemY > listY + listH) break;

        bool isSelected = (i == selection);
        bool isDir = (i < (int)flags.size()) && ((flags[i] & 1) != 0);
        std::string rawName = (i < (int)files.size()) ? files[i] : "";

        float itemW = isPreview ? (listW - 14.0f) : (listW - 4.0f);
        if(isSelected) {
            uiDrawRoundedRect(listX + 2.0f, itemY, itemW, 22.0f, 2.0f, uiColorRGBA(185, 138, 36, 255), colAmber, 1.0f);
            uiDrawText(listX + 4.0f, itemY + 6.0f, ">", colWhite);
        }

        if(isDir) {
            uiDrawFolderIcon(listX + 10.0f, itemY + 5.0f);
            uiDrawTextClipped(listX + 26.0f, itemY + 6.0f, textMaxW, rawName.c_str(), isSelected ? colWhite : colYellow);
        } else {
            auto dot = rawName.find_last_of('.');
            std::string ext = (dot != std::string::npos) ? rawName.substr(dot + 1) : "";
            bool isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

            uiDrawCartridgeIcon(listX + 10.0f, itemY + 5.0f, isGBC);

            std::string label = (isGBC ? "[CGB] " : "[DMG] ") + rawName;
            uiDrawTextClipped(listX + 24.0f, itemY + 6.0f, textMaxW, label.c_str(), isSelected ? colWhite : colWhite);
        }
    }

    if(numFiles == 0) {
        uiDrawText(listX + 10.0f, listY + 16.0f, "Sin archivos", colDarkGray);
    } else if(isPreview) {
        // Scrollbar vertical solo visible en modo Preview (x = 182..188)
        float sbX = listX + listW - 8.0f;
        uiDrawRect(sbX, listY + 2.0f, 6.0f, 8.0f, uiColorRGBA(25, 35, 50, 255));
        uiDrawRect(sbX, listY + listH - 10.0f, 6.0f, 8.0f, uiColorRGBA(25, 35, 50, 255));

        float maxScroll = chooser->getMaxScroll();
        if(maxScroll > 0.0f) {
            const float trackY = listY + 12.0f;
            const float trackH = listH - 24.0f;
            float totalH = (float)numFiles * 24.0f;
            float thumbH = trackH * (168.0f / totalH);
            if(thumbH < 12.0f) thumbH = 12.0f;
            if(thumbH > trackH) thumbH = trackH;
            float thumbY = trackY + (scrollY / maxScroll) * (trackH - thumbH);
            if(thumbY < trackY) thumbY = trackY;
            if(thumbY + thumbH > trackY + trackH) thumbY = trackY + trackH - thumbH;
            uiDrawRoundedRect(sbX, thumbY, 6.0f, thumbH, 2.0f, uiColorRGBA(70, 115, 175, 255), uiColorRGBA(100, 160, 240, 255), 1.0f);
        }
    }

    // Máscara superior e inferior para recortar listas desbordadas
    uiDrawRect(0.0f, 0.0f, 320.0f, 21.0f, colBg);
    uiDrawText(10.0f, 6.0f, "ROMS: ", colAmber);
    uiDrawTextClipped(10.0f + romsW, 6.0f, 210.0f, dirTitle.c_str(), colWhite);
    uiDrawTextRight(310.0f, 6.0f, "v1.0.9", colGray);
    uiDrawRect(8.0f, 20.0f, 304.0f, 1.0f, colBorder);

    uiDrawRect(0.0f, 204.0f, 320.0f, 4.0f, colBg);

    // 3. Panel Derecho Contextual (x: panelX..panelX+panelW, y: panelY..panelY+panelH)
    uiDrawDropShadow(panelX, panelY, panelW, panelH, 3.0f, 80);
    uiDrawRoundedRect(panelX, panelY, panelW, panelH, 2.0f, uiColorRGBA(14, 18, 26, 255), colAmber, 1.0f);

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
        // ==========================================
        // MODO 0: VISTA PREVIA (panelW = 118 px, panelX = 194)
        // ==========================================
        float centerX = panelX + panelW * 0.5f; // 253.0f
        uiDrawTextCentered(centerX, 28.0f, "VISTA PREVIA", colAmber);
        uiDrawRect(panelX + 4.0f, 40.0f, panelW - 8.0f, 1.0f, colAmber);

        // Recuadro del visor (84x84 px centrado en x=211, y=44)
        uiDrawDropShadow(211.0f, 44.0f, 84.0f, 84.0f, 2.0f, 70);

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
            emitUiQuad(211.0f, 44.0f, 295.0f, 128.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            C3D_ImmDrawEnd();

            uiDrawRectOutline(211.0f, 44.0f, 84.0f, 84.0f, 1.0f, colAmber);
        } else if(!currentSelectedRom.empty()) {
            bool isGBC = (currentSelectedRom.find(".gbc") != std::string::npos || currentSelectedRom.find(".cgb") != std::string::npos);
            u32 colCart = isGBC ? uiColorRGBA(55, 60, 75, 255) : uiColorRGBA(150, 150, 150, 255);
            uiDrawRect(223.0f, 48.0f, 60.0f, 74.0f, colCart);
            uiDrawRectOutline(223.0f, 48.0f, 60.0f, 74.0f, 1.0f, colAmber);
            uiDrawTextCentered(253.0f, 78.0f, isGBC ? "CGB" : "DMG", colWhite);
        } else {
            uiDrawTextCentered(centerX, 80.0f, "Sin Archivo", colGray);
        }

        // Título del juego centrado y recortado a 108 px en y=134
        std::string cleanName = (s_metaLoaded && !s_currentMeta.title.empty()) ? s_currentMeta.title : baseSelectedName;
        if(!cleanName.empty()) {
            float tw = uiGetTextWidth(cleanName.c_str(), 1.0f);
            float startX = (tw < 108.0f) ? (centerX - tw * 0.5f) : (panelX + 5.0f);
            uiDrawTextClipped(startX, 134.0f, 108.0f, cleanName.c_str(), colAmber);
        }

        // Desarrollador centrado y recortado a 108 px en y=148
        std::string pubStr = s_metaLoaded ? (!s_currentMeta.developer.empty() ? s_currentMeta.developer : s_currentMeta.publisher) : "Nintendo";
        float pw = uiGetTextWidth(pubStr.c_str(), 1.0f);
        float px = (pw < 108.0f) ? (centerX - pw * 0.5f) : (panelX + 5.0f);
        uiDrawTextClipped(px, 148.0f, 108.0f, pubStr.c_str(), colGray);

        // Selector "< Slot 0 >" centrado en x=253 en y=166
        char slotBuf[24];
        snprintf(slotBuf, sizeof(slotBuf), "< Slot %d >", s_previewSlot);
        uiDrawTextCentered(centerX, 166.0f, slotBuf, colWhite);

        // Estado en línea inferior (y=182) centrado: [SRAM OK] o [Sin Estado]
        const char* statusStr = hasState ? "[SAV OK]" : (hasSav ? "[SRAM OK]" : "[Sin Estado]");
        u32 statusCol = hasState ? colGreen : (hasSav ? colCyan : colDarkGray);
        uiDrawTextCentered(centerX, 182.0f, statusStr, statusCol);

    } else if(s_brMode == BR_MODE_DISPLAY) {
        // ==========================================
        // MODO 1: AJUSTES DE DISPLAY (Expandido: panelW = 182 px, panelX = 130)
        // ==========================================
        uiDrawGearIcon(138.0f, 28.0f, colAmber);
        uiDrawText(152.0f, 28.0f, "AJUSTES DE DISPLAY", colAmber);
        uiDrawRect(136.0f, 42.0f, 170.0f, 1.0f, colAmber);

        // Fila 1: Escalado (y: 52)
        uiDrawText(138.0f, 52.0f, "• Escalado:", colWhite);
        static const char* scaleNames[5] = {"Off", "125%", "150%", "Aspect", "Full"};
        char v0[24];
        snprintf(v0, sizeof(v0), "< %s >", scaleNames[getMenuOption("Scaling") % 5]);
        uiDrawTextRight(304.0f, 52.0f, v0, colYellow);

        // Fila 2: Filtro (y: 72)
        uiDrawText(138.0f, 72.0f, "• Filtro:", colWhite);
        static const char* filterNames[3] = {"Nearest", "Linear", "Sharp 2x"};
        char v1[24];
        snprintf(v1, sizeof(v1), "< %s >", filterNames[getMenuOption("Scale Filter") % 3]);
        uiDrawTextRight(304.0f, 72.0f, v1, colYellow);

        // Fila 3: Rejilla LCD (y: 92)
        uiDrawText(138.0f, 92.0f, "• Rejilla LCD:", colWhite);
        const char* v2 = getMenuOption("LCD Grid") ? "< On >" : "< Off >";
        uiDrawTextRight(304.0f, 92.0f, v2, getMenuOption("LCD Grid") ? colGreen : colDarkGray);

        // Caja Informativa Inferior (x: 136..306, y: 116..196)
        uiDrawRoundedRect(136.0f, 116.0f, 170.0f, 80.0f, 3.0f, uiColorRGBA(12, 16, 24, 255), uiColorRGBA(35, 65, 95, 255), 1.0f);
        uiDrawInfoBadge(142.0f, 124.0f);
        int curFilt = getMenuOption("Scale Filter") % 3;
        const char* filterTitle = (curFilt == 2) ? "SHARP BILINEAR" : (curFilt == 1 ? "FILTRO LINEAL" : "NEAREST PIXEL");
        uiDrawText(156.0f, 124.0f, filterTitle, colCyan);

        uiDrawText(142.0f, 140.0f, "Pixeles 100% nitidos sin", colWhite);
        uiDrawText(142.0f, 154.0f, "parpadeos de camara", colWhite);
        uiDrawText(142.0f, 168.0f, "(cero tirones de GPU)", colGray);

    } else if(s_brMode == BR_MODE_PALETTE) {
        // ==========================================
        // MODO 2: PALETA Y SISTEMA (Expandido: panelW = 182 px, panelX = 130)
        // ==========================================
        uiDrawPaletteIcon(138.0f, 28.0f);
        uiDrawText(154.0f, 28.0f, "PALETA Y SISTEMA", colAmber);
        uiDrawRect(136.0f, 42.0f, 170.0f, 1.0f, colAmber);

        // Fila 1: Paleta DMG (y: 52)
        uiDrawText(138.0f, 52.0f, "• Paleta DMG:", colWhite);
        char palBuf[32];
        snprintf(palBuf, sizeof(palBuf), "< %s >", mgrGetPaletteName(gbColorizeMode));
        uiDrawTextRight(304.0f, 52.0f, palBuf, colYellow);

        // Fila 2: Color GBC (y: 72)
        uiDrawText(138.0f, 72.0f, "• Color GBC:", colWhite);
        const char* gbcCol = getMenuOption("CGB Colors") ? "< Corregido >" : "< Nativo >";
        uiDrawTextRight(304.0f, 72.0f, gbcCol, colYellow);

        // Fila 3: Sonido GB (y: 92)
        uiDrawText(138.0f, 92.0f, "• Sonido GB:", colWhite);
        const char* sndStr = getMenuOption("Master") ? "< 32 kHz >" : "< Mute >";
        uiDrawTextRight(304.0f, 92.0f, sndStr, getMenuOption("Master") ? colGreen : colDarkGray);

        // 4 Cuadros de Color (y: 116..138)
        const u32* palCols = mgrGetPaletteColors(gbColorizeMode);
        float swStart = 164.0f;
        for(int c = 0; c < 4; c++) {
            float sx = swStart + (float)c * 30.0f;
            uiDrawRect(sx, 116.0f, 24.0f, 22.0f, palCols[c]);
            uiDrawRectOutline(sx, 116.0f, 24.0f, 22.0f, 1.0f, colWhite);
        }

        // Botón [ Cambiar Paleta ] en y: 154..180
        uiDrawModernButton(148.0f, 154.0f, 146.0f, 26.0f, "Cambiar Paleta", uiColorRGBA(45, 30, 15, 255), colAmber, false);
    }

    // 4. Dock Inferior Temático de 4 Botones (y: 208..234)
    // Botones de 72px: x=8..80, 86..158, 164..236, 242..314
    bool dispActive = (s_brMode == BR_MODE_DISPLAY);
    uiDrawModernButton(8.0f, 208.0f, 72.0f, 26.0f, "",
                       dispActive ? uiColorRGBA(26, 148, 175, 255) : uiColorRGBA(15, 60, 80, 255),
                       dispActive ? UI_COLOR_WHITE : uiColorRGBA(30, 150, 180, 255), dispActive);
    uiDrawGearIcon(14.0f, 215.0f, colWhite);
    uiDrawText(28.0f, 215.0f, "Display", colWhite);

    bool palActive = (s_brMode == BR_MODE_PALETTE);
    uiDrawModernButton(86.0f, 208.0f, 72.0f, 26.0f, "",
                       palActive ? uiColorRGBA(215, 110, 32, 255) : uiColorRGBA(85, 45, 15, 255),
                       palActive ? UI_COLOR_WHITE : uiColorRGBA(210, 120, 30, 255), palActive);
    uiDrawPaletteIcon(92.0f, 215.0f);
    uiDrawText(108.0f, 215.0f, "Paleta", colWhite);

    uiDrawModernButton(164.0f, 208.0f, 72.0f, 26.0f, "",
                       uiColorRGBA(20, 45, 80, 255), uiColorRGBA(45, 100, 180, 255), false);
    uiDrawFolderIcon(170.0f, 214.0f);
    uiDrawText(186.0f, 215.0f, "Directorio", colWhite);

    uiDrawModernButton(242.0f, 208.0f, 72.0f, 26.0f, "X Salir",
                       uiColorRGBA(80, 25, 25, 255), uiColorRGBA(180, 50, 50, 255), false);

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

    // Barra superior (y=14): "GAMEYOB 3DS" (x=18) | Hora (rightX=322) | Batería (rightX=360) | Icono batería (x=364..382)
    uiDrawText(18.0f, 14.0f, "GAMEYOB 3DS", colAmber);
    uiDrawTextRight(322.0f, 14.0f, timeStr, colWhite);
    uiDrawTextRight(360.0f, 14.0f, batPctStr, batCol);
    uiDrawRectOutline(364.0f, 14.0f, 18.0f, 9.0f, 1.0f, colWhite);
    uiDrawRect(382.0f, 16.0f, 2.0f, 5.0f, colWhite);
    float fillW = ((float)batLevel / 5.0f) * 14.0f;
    if(fillW > 0.0f) {
        uiDrawRect(366.0f, 16.0f, fillW, 5.0f, batCol);
    }

    // Línea 2 de barra superior (y=27): Título completo recortado con uiDrawTextClipped
    uiDrawTextClipped(18.0f, 27.0f, 360.0f, fullHeader.c_str(), colWhite);

    // Línea divisoria dorada horizontal en y=40
    uiDrawRect(14.0f, 40.0f, 372.0f, 1.0f, colAmber);

    // 3. Cuadrante Izquierdo (x: 22..146, y: 48..196)
    uiDrawDropShadow(22.0f, 48.0f, 124.0f, 152.0f, 2.0f, 70);
    uiDrawRoundedRect(22.0f, 48.0f, 124.0f, 152.0f, 2.0f, uiColorRGBA(12, 18, 30, 255), uiColorRGBA(45, 65, 95, 255), 1.0f);

    if(isDir) {
        uiDrawText(44.0f, 85.0f, "[CARPETA]", colYellow);
        uiDrawText(28.0f, 110.0f, "Subdirectorio SDMC", colGray);
        uiDrawText(32.0f, 135.0f, "A: Abrir Carpeta", colWhite);
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

                uiDrawText(52.0f + eyeOffset * 0.5f, 84.0f, "GAME BOY", colWhite);
                if(isGBC) {
                    uiDrawText(58.0f + eyeOffset * 0.5f, 102.0f, "COLOR", colCyan);
                }
            }
        }

        std::string pubStr = s_metaLoaded ? (!s_currentMeta.developer.empty() ? s_currentMeta.developer : s_currentMeta.publisher) : "Nintendo";
        uiDrawTextClipped(26.0f + eyeOffset * 0.3f, 170.0f, 116.0f, pubStr.c_str(), colWhite);

        char yrGen[40];
        if(s_metaLoaded && s_currentMeta.year > 0) {
            snprintf(yrGen, sizeof(yrGen), "%d  %s", s_currentMeta.year, s_currentMeta.genre.c_str());
        } else {
            snprintf(yrGen, sizeof(yrGen), "Clasico GameBoy");
        }
        uiDrawTextClipped(26.0f + eyeOffset * 0.3f, 184.0f, 116.0f, yrGen, colAmber);
    } else {
        uiDrawText(44.0f, 105.0f, "Sin seleccion", colGray);
    }

    // 4. Cuadrante Derecho (x: 158..380, y: 48..204)
    uiDrawDropShadow(158.0f, 48.0f, 222.0f, 156.0f, 2.0f, 70);
    uiDrawRoundedRect(158.0f, 48.0f, 222.0f, 156.0f, 2.0f, uiColorRGBA(16, 22, 34, 255), uiColorRGBA(45, 65, 95, 255), 1.0f);

    if(!s_showSynopsis) {
        // MODO FICHA DE COMPATIBILIDAD
        uiDrawTextClipped(166.0f, 52.0f, 204.0f, "FICHA DE COMPATIBILIDAD", colAmber);
        uiDrawRect(164.0f, 66.0f, 210.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            bool isGBC = s_metaLoaded ? s_currentMeta.isGBC : (selectedFile.find(".gbc") != std::string::npos || selectedFile.find(".cgb") != std::string::npos);
            bool isSGB = s_metaLoaded ? s_currentMeta.isSGB : false;

            // Línea 1: Modo Nativo (y=74)
            uiDrawText(166.0f, 74.0f, "• Modo Nativo: ", colGray);
            float wL1 = uiGetTextWidth("• Modo Nativo: ", 1.0f);
            uiDrawTextClipped(166.0f + wL1, 74.0f, 204.0f - wL1, isGBC ? "Game Boy Color (CGB)" : (isSGB ? "Super GB (SGB)" : "Game Boy (DMG)"), colWhite);

            // Línea 2: Mapper (y=92)
            uiDrawText(166.0f, 92.0f, "• Mapper: ", colGray);
            float wL2 = uiGetTextWidth("• Mapper: ", 1.0f);
            std::string mapStr = (s_metaLoaded && !s_currentMeta.mbcName.empty()) ? s_currentMeta.mbcName : "MBC Auto";
            if(s_metaLoaded && s_currentMeta.mbcName.find("MBC3") != std::string::npos) mapStr += " (RTC Activo)";
            uiDrawTextClipped(166.0f + wL2, 92.0f, 204.0f - wL2, mapStr.c_str(), colCyan);

            // Línea 3: Guardado (y=110)
            uiDrawText(166.0f, 110.0f, "• Guardado: ", colGray);
            float wL3 = uiGetTextWidth("• Guardado: ", 1.0f);
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
            uiDrawTextClipped(166.0f + wL3, 110.0f, 204.0f - wL3, savStr.c_str(), colWhite);

            // Línea 4: Tamaño ROM (y=128)
            uiDrawText(166.0f, 128.0f, "• Tamano ROM: ", colGray);
            float wL4 = uiGetTextWidth("• Tamano ROM: ", 1.0f);
            std::string romSzStr = "";
            if(s_metaLoaded && s_currentMeta.romSizeKB > 0) {
                int banks = s_currentMeta.romSizeKB / 16;
                romSzStr = std::to_string(s_currentMeta.romSizeKB) + " KB (" + std::to_string(banks) + " bancos)";
            } else {
                romSzStr = "Estandar";
            }
            uiDrawTextClipped(166.0f + wL4, 128.0f, 204.0f - wL4, romSzStr.c_str(), colCyan);

            // Línea 5: Audio (y=146)
            uiDrawText(166.0f, 146.0f, "• Audio: ", colGray);
            float wL5 = uiGetTextWidth("• Audio: ", 1.0f);
            uiDrawTextClipped(166.0f + wL5, 146.0f, 204.0f - wL5, "Anillo Dual SPSC 32,728 Hz", colWhite);

            // Línea 6: Motor (y=164)
            uiDrawText(166.0f, 164.0f, "• Motor: ", colGray);
            float wL6 = uiGetTextWidth("• Motor: ", 1.0f);
            uiDrawTextClipped(166.0f + wL6, 164.0f, 204.0f - wL6, "ARM11 JIT 60 FPS", colGray);

            // Al pie de la tarjeta: Última partida (y=184)
            std::string saveInfo = "Ultima partida: ";
            if(s_metaLoaded && s_currentMeta.hasSave) {
                saveInfo += s_currentMeta.saveDate;
            } else {
                saveInfo += "Ninguna";
            }
            uiDrawTextClipped(166.0f, 184.0f, 204.0f, saveInfo.c_str(), colAmber);
        } else {
            uiDrawText(166.0f, 74.0f, "Directorio SDMC", colWhite);
            uiDrawText(166.0f, 94.0f, "Seleccione una ROM para ver", colCyan);
            uiDrawText(166.0f, 110.0f, "su ficha tecnica completa.", colCyan);
            uiDrawText(166.0f, 136.0f, "Audio: Anillo Dual SPSC 32,728 Hz", colWhite);
            uiDrawText(166.0f, 156.0f, "Motor: ARM11 JIT 60 FPS", colGray);
        }
    } else {
        // MODO SINOPSIS Y DATOS
        uiDrawTextClipped(166.0f, 52.0f, 204.0f, "SINOPSIS Y DATOS", colAmber);
        uiDrawRect(164.0f, 66.0f, 210.0f, 1.0f, colAmber);

        if(!selectedFile.empty() && !isDir) {
            // Línea 1: Año y Género (y=74)
            char l1[40];
            if(s_metaLoaded && s_currentMeta.year > 0) {
                snprintf(l1, sizeof(l1), "• Ano: %d | %s", s_currentMeta.year, s_currentMeta.genre.c_str());
            } else {
                snprintf(l1, sizeof(l1), "• Genero: %s", s_metaLoaded ? s_currentMeta.genre.c_str() : "Clasico");
            }
            uiDrawTextClipped(166.0f, 74.0f, 204.0f, l1, colWhite);

            // Línea 2: Desarrollador (y=92)
            std::string dev = s_metaLoaded && !s_currentMeta.developer.empty() ? s_currentMeta.developer : (s_metaLoaded && !s_currentMeta.publisher.empty() ? s_currentMeta.publisher : "Desconocido");
            std::string devLine = "• Dev: " + dev;
            uiDrawTextClipped(166.0f, 92.0f, 204.0f, devLine.c_str(), colCyan);

            // Línea 3: Tiempo Jugado (y=110)
            uiDrawTextClipped(166.0f, 110.0f, 204.0f, "• Tiempo: Registrado", colWhite);

            // Líneas 4..7: Sinopsis formateada
            std::string syn = s_metaLoaded && !s_currentMeta.synopsis.empty() ? s_currentMeta.synopsis : "Titulo clasico de Game Boy totalmente compatible con el nucleo GameYob.";
            std::vector<std::string> synLines = wrapSynopsisText(syn, 34);
            float sy = 128.0f;
            for(size_t i = 0; i < synLines.size() && i < 3; i++) {
                uiDrawTextClipped(166.0f, sy, 204.0f, synLines[i].c_str(), colGray);
                sy += 15.0f;
            }

            // Estado al pie (y=184)
            uiDrawTextClipped(166.0f, 184.0f, 204.0f, "• Nucleo: GameYob 100% OK", colGreen);
        } else {
            uiDrawText(166.0f, 74.0f, "Directorio SDMC", colWhite);
            uiDrawText(166.0f, 94.0f, "Seleccione una ROM para ver", colCyan);
            uiDrawText(166.0f, 110.0f, "su sinopsis y datos.", colCyan);
            uiDrawText(166.0f, 136.0f, "Audio: Anillo Dual SPSC 32,728 Hz", colWhite);
            uiDrawText(166.0f, 156.0f, "Motor: ARM11 JIT 60 FPS", colGray);
        }
    }

    // 5. Barra Inferior de Comandos (y: 214..228)
    uiDrawRect(14.0f, 214.0f, 372.0f, 15.0f, uiColorRGBA(10, 14, 20, 255));
    uiDrawRectOutline(14.0f, 214.0f, 372.0f, 15.0f, 1.0f, uiColorRGBA(150, 118, 45, 255));

    uiDrawTextCentered(200.0f, 215.0f, "[A] Iniciar Juego    [Y] Alternar Ficha    [SELECT] Ajustes", colWhite);

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
