#include "platform/common/rom_metadata.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>

struct TitleDbEntry {
    const char* key;
    int year;
    const char* genre;
    const char* developer;
    const char* synopsis;
};

static const TitleDbEntry s_titleDb[] = {
    { "POKEMON GLD",      2000, "RPG",      "Game Freak", "Explora Johto, atrapa 100 nuevos Pokemon y derrota al Team Rocket en esta legendaria secuela." },
    { "POKEMON SLV",      2000, "RPG",      "Game Freak", "Explora Johto, atrapa 100 nuevos Pokemon y derrota al Team Rocket en esta legendaria secuela." },
    { "POKEMON CRY",      2001, "RPG",      "Game Freak", "Version definitiva de la 2da generacion con animaciones de combate y la subsaga de Suicune." },
    { "POKEMON YEL",      1998, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON RED",      1996, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON BLU",      1996, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON GOLD",     2000, "RPG",      "Game Freak", "Explora Johto, atrapa 100 nuevos Pokemon y derrota al Team Rocket en esta legendaria secuela." },
    { "POKEMON SILVER",   2000, "RPG",      "Game Freak", "Explora Johto, atrapa 100 nuevos Pokemon y derrota al Team Rocket en esta legendaria secuela." },
    { "POKEMON CRYSTAL",  2001, "RPG",      "Game Freak", "Version definitiva de la 2da generacion con animaciones de combate y la subsaga de Suicune." },
    { "POKEMON YELLOW",   1998, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON BLUE",     1996, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON GREEN",    1996, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "POKEMON",          1996, "RPG",      "Game Freak", "El inicio de la saga en Kanto. Consigue las 8 medallas y completa la Pokedex de 151 criaturas." },
    { "DONKEYKONGLAN",    1995, "Plataf.",  "Rareware",   "DK y Diddy recorren la isla con graficos pre-renderizados adaptados a la pantalla monocroma." },
    { "DONKEY KONG",      1994, "Plataf.",  "Nintendo",   "Mario rescata a Pauline a traves de 101 niveles llenos de ingenio, acrobacias y llaves." },
    { "TETRIS DX",        1998, "Puzzle",   "Nintendo",   "El clasico imperecedero de puzzles llevado al color con nuevos modos y sistema de guardado." },
    { "TETRIS",           1989, "Puzzle",   "Nintendo",   "El icono mundial de puzzles. Alinea los tetraminos y bate tu propio record." },
    { "ZELDA",            1993, "Aventura", "Nintendo",   "Despierta al Pez Viento en la misteriosa Isla Koholint en una de las mejores aventuras portatiles." },
    { "SUPER MARIOLAND",  1989, "Plataf.",  "Nintendo",   "Acompana a Mario por Sarasaland para salvar a la Princesa Daisy de la amenaza alienigena." },
    { "MARIOLAND2",       1992, "Plataf.",  "Nintendo",   "Recupera las 6 Monedas Doradas para retomar el castillo de Mario arrebatado por Wario." },
    { "METROID2",         1991, "Accion",   "Nintendo",   "Infiltrate en SR388 y erradica a los metroides en esta oscura mision de supervivencia con Samus." },
    { "WARIOLAND2",       1998, "Plataf.",  "Nintendo",   "Wario despierta en su castillo inundado para recuperar sus tesoros del capitan Syrup." },
    { "WARIOLAND",        1994, "Plataf.",  "Nintendo",   "Guia a Wario a traves de Kitchen Island acumulando monedas para comprar su propio castillo." },
    { "WARIO LAND",       1994, "Plataf.",  "Nintendo",   "Guia a Wario a traves de Kitchen Island acumulando monedas para comprar su propio castillo." },
    { "MARIO LAND",       1989, "Plataf.",  "Nintendo",   "Acompana a Mario por Sarasaland para salvar a la Princesa Daisy de la amenaza alienigena." },
    { "KIRBY DREAM LAND", 1992, "Plataf.",  "HAL Lab",    "El debut de Kirby en Dream Land para rescatar la comida robada por el Rey Dedede." },
    { "KIRBY",            1992, "Plataf.",  "HAL Lab",    "El debut de Kirby en Dream Land para rescatar la comida robada por el Rey Dedede." },
    { "MEGAMAN",          1991, "Accion",   "Capcom",     "Mega Man combate a los Robot Masters del Dr. Wily adaptado al hardware portatil." },
    { "ROCKMAN",          1991, "Accion",   "Capcom",     "Mega Man combate a los Robot Masters del Dr. Wily adaptado al hardware portatil." },
    { "CASTLEVANIA",      1989, "Accion",   "Konami",     "Christopher Belmont blande el latigo Vampire Killer contra las hordas del Conde Dracula." },
    { "DR. MARIO",        1990, "Puzzle",   "Nintendo",   "Erradica las bacterias de colores combinando pildoras vitaminicas en este frenetico puzzle." },
    { "DR.MARIO",         1990, "Puzzle",   "Nintendo",   "Erradica las bacterias de colores combinando pildoras vitaminicas en este frenetico puzzle." }
};

static std::string getNewLicensee(const std::string& code) {
    if(code == "01" || code == "31") return "Nintendo";
    if(code == "08" || code == "38") return "Capcom";
    if(code == "0A") return "Jaleco";
    if(code == "13" || code == "50" || code == "69") return "EA";
    if(code == "18") return "Hudson Soft";
    if(code == "19") return "ITC";
    if(code == "20") return "KSS";
    if(code == "25") return "San-X";
    if(code == "30") return "Viacom";
    if(code == "32" || code == "51" || code == "B2") return "Bandai";
    if(code == "33" || code == "67" || code == "93") return "Ocean";
    if(code == "34" || code == "54" || code == "A4") return "Konami";
    if(code == "35") return "Hector";
    if(code == "39" || code == "D9") return "Banpresto";
    if(code == "41") return "Ubi Soft";
    if(code == "42" || code == "EB") return "Atlus";
    if(code == "44") return "Malibu";
    if(code == "46" || code == "CF") return "Angel";
    if(code == "47" || code == "8B") return "Bullet-Proof";
    if(code == "49") return "Irem";
    if(code == "4F") return "Eidos";
    if(code == "52") return "Activision";
    if(code == "53" || code == "BF") return "Sammy";
    if(code == "55") return "Hi Tech";
    if(code == "56" || code == "DB") return "LJN";
    if(code == "57") return "Matchbox";
    if(code == "58") return "Mattel";
    if(code == "59") return "Milton Bradley";
    if(code == "60") return "Titus";
    if(code == "61") return "Virgin";
    if(code == "70") return "Infogrames";
    if(code == "71") return "Interplay";
    if(code == "72" || code == "AA") return "Broderbund";
    if(code == "73") return "Sculptured";
    if(code == "75") return "SCI";
    if(code == "78") return "THQ";
    if(code == "79") return "Accolade";
    if(code == "7F" || code == "C2") return "Kemco";
    if(code == "80") return "Misawa";
    if(code == "83") return "Lozc";
    if(code == "86" || code == "C4") return "Tokuma";
    if(code == "87") return "Tsukuda";
    if(code == "91") return "Chunsoft";
    if(code == "92") return "Video System";
    if(code == "95" || code == "E3") return "Varie";
    if(code == "96") return "Yonezawa";
    if(code == "97") return "Kaneko";
    if(code == "99") return "ARC";
    if(code == "A6") return "Kawada";
    if(code == "A7") return "Takara";
    if(code == "A9") return "Technos Japan";
    if(code == "AC") return "Toei";
    if(code == "AD") return "Toho";
    if(code == "AF") return "Namco";
    if(code == "B0") return "Acclaim";
    if(code == "B1") return "ASCII";
    if(code == "B4") return "Enix";
    if(code == "B6") return "HAL";
    if(code == "B7") return "SNK";
    if(code == "B9" || code == "CE") return "Pony Canyon";
    if(code == "BA") return "Culture Brain";
    if(code == "BB") return "Sunsoft";
    if(code == "BD") return "Sony";
    if(code == "C0") return "Taito";
    if(code == "C3") return "Square Soft";
    if(code == "C5") return "Data East";
    if(code == "C6") return "Tonkinhouse";
    if(code == "C8") return "Koei";
    if(code == "C9") return "UFL";
    if(code == "CA") return "Ultra";
    if(code == "CB") return "VAP";
    if(code == "CC") return "Use";
    if(code == "CD") return "Meldac";
    if(code == "D1") return "Sofel";
    if(code == "D2") return "Quest";
    if(code == "D3") return "Sigma";
    if(code == "D4") return "Ask Kodansha";
    if(code == "D6") return "Naxat Soft";
    if(code == "D7") return "Copya";
    if(code == "DA") return "Tomy";
    if(code == "DD") return "NCS";
    if(code == "DE") return "Human";
    if(code == "DF") return "Altron";
    if(code == "E1") return "Towachiki";
    if(code == "E2") return "Yutaka";
    if(code == "E5") return "Epoch";
    if(code == "E7") return "Athena";
    if(code == "E8") return "Asmik";
    if(code == "F0") return "A Wave";
    if(code == "00") return "Homebrew";
    return "Unknown";
}

static std::string getOldLicensee(u8 code) {
    switch(code) {
        case 0x01: case 0x31: return "Nintendo";
        case 0x08: case 0x38: return "Capcom";
        case 0x0A: return "Jaleco";
        case 0x13: case 0x50: case 0x69: return "EA";
        case 0x18: return "Hudson Soft";
        case 0x19: return "ITC";
        case 0x20: return "KSS";
        case 0x25: return "San-X";
        case 0x30: return "Viacom";
        case 0x32: case 0x51: case 0xB2: return "Bandai";
        case 0x33: case 0x67: case 0x93: return "Ocean";
        case 0x34: case 0x54: case 0xA4: return "Konami";
        case 0x35: return "Hector";
        case 0x39: case 0xD9: return "Banpresto";
        case 0x41: return "Ubi Soft";
        case 0x42: case 0xEB: return "Atlus";
        case 0x44: return "Malibu";
        case 0x46: case 0xCF: return "Angel";
        case 0x47: case 0x8B: return "Bullet-Proof";
        case 0x49: return "Irem";
        case 0x4F: return "Eidos";
        case 0x52: return "Activision";
        case 0x53: case 0xBF: return "Sammy";
        case 0x55: return "Hi Tech";
        case 0x56: case 0xDB: return "LJN";
        case 0x57: return "Matchbox";
        case 0x58: return "Mattel";
        case 0x59: return "Milton Bradley";
        case 0x60: return "Titus";
        case 0x61: return "Virgin";
        case 0x70: return "Infogrames";
        case 0x71: return "Interplay";
        case 0x72: case 0xAA: return "Broderbund";
        case 0x73: return "Sculptured";
        case 0x75: return "SCI";
        case 0x78: return "THQ";
        case 0x79: return "Accolade";
        case 0x7F: case 0xC2: return "Kemco";
        case 0x80: return "Misawa";
        case 0x83: return "Lozc";
        case 0x86: case 0xC4: return "Tokuma";
        case 0x87: return "Tsukuda";
        case 0x91: return "Chunsoft";
        case 0x92: return "Video System";
        case 0x95: case 0xE3: return "Varie";
        case 0x96: return "Yonezawa";
        case 0x97: return "Kaneko";
        case 0x99: return "ARC";
        case 0xA6: return "Kawada";
        case 0xA7: return "Takara";
        case 0xA9: return "Technos Japan";
        case 0xAC: return "Toei";
        case 0xAD: return "Toho";
        case 0xAF: return "Namco";
        case 0xB0: return "Acclaim";
        case 0xB1: return "ASCII";
        case 0xB4: return "Enix";
        case 0xB6: return "HAL";
        case 0xB7: return "SNK";
        case 0xB9: case 0xCE: return "Pony Canyon";
        case 0xBA: return "Culture Brain";
        case 0xBB: return "Sunsoft";
        case 0xBD: return "Sony";
        case 0xC0: return "Taito";
        case 0xC3: return "Square Soft";
        case 0xC5: return "Data East";
        case 0xC6: return "Tonkinhouse";
        case 0xC8: return "Koei";
        case 0xC9: return "UFL";
        case 0xCA: return "Ultra";
        case 0xCB: return "VAP";
        case 0xCC: return "Use";
        case 0xCD: return "Meldac";
        case 0xD1: return "Sofel";
        case 0xD2: return "Quest";
        case 0xD3: return "Sigma";
        case 0xD4: return "Ask Kodansha";
        case 0xD6: return "Naxat Soft";
        case 0xD7: return "Copya";
        case 0xDA: return "Tomy";
        case 0xDD: return "NCS";
        case 0xDE: return "Human";
        case 0xDF: return "Altron";
        case 0xE1: return "Towachiki";
        case 0xE2: return "Yutaka";
        case 0xE5: return "Epoch";
        case 0xE7: return "Athena";
        case 0xE8: return "Asmik";
        case 0xF0: return "A Wave";
        case 0x00: return "Homebrew";
        default:   return "Unknown";
    }
}

bool fetchRomMetadata(const std::string& romPath, RomMetadata& outMeta) {
    if(romPath.empty()) {
        return false;
    }

    outMeta.isCgbOnly = false;

    FILE* f = fopen(romPath.c_str(), "rb");
    if(f == nullptr) {
        return false;
    }

    if(fseek(f, 0x0100, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    u8 hdr[0x50];
    if(fread(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        fclose(f);
        return false;
    }
    fclose(f);

    // 1. CGB Flag (0x0143)
    u8 cgbFlag = hdr[0x43];
    outMeta.isGBC = ((cgbFlag & 0x80) != 0 || (cgbFlag & 0xC0) != 0);
    outMeta.isCgbOnly = (cgbFlag == 0xC0);

    // 2. Title (0x0134..0x013E para CGB, 0x0134..0x0143 para DMG)
    int titleLen = outMeta.isGBC ? 11 : 16;
    char titleBuf[17] = {0};
    memcpy(titleBuf, &hdr[0x34], titleLen);
    titleBuf[titleLen] = '\0';

    std::string cleanTitle = titleBuf;
    for(size_t i = 0; i < cleanTitle.length(); i++) {
        if(cleanTitle[i] == '_') {
            cleanTitle[i] = ' ';
        }
    }
    while(!cleanTitle.empty() && ((u8)cleanTitle.back() <= ' ' || (u8)cleanTitle.back() >= 127)) {
        cleanTitle.pop_back();
    }

    if(cleanTitle.empty()) {
        auto slash = romPath.find_last_of("/\\");
        std::string base = (slash != std::string::npos) ? romPath.substr(slash + 1) : romPath;
        auto dot = base.find_last_of('.');
        if(dot != std::string::npos) {
            base = base.substr(0, dot);
        }
        for(size_t i = 0; i < base.length(); i++) {
            if(base[i] == '_') {
                base[i] = ' ';
            }
        }
        cleanTitle = base;
    }
    outMeta.title = cleanTitle;

    // 3. SGB Flag (0x0146: 0x03 && 0x014B == 0x33)
    u8 sgbFlag = hdr[0x46];
    u8 oldLicensee = hdr[0x4B];
    outMeta.isSGB = (sgbFlag == 0x03 && oldLicensee == 0x33);

    // 4. Cartridge Type & Hardware (0x0147)
    u8 cartType = hdr[0x47];
    outMeta.hasBattery = false;
    switch(cartType) {
        case 0x00: outMeta.mbcName = "ROM ONLY"; break;
        case 0x01: outMeta.mbcName = "MBC1"; break;
        case 0x02: outMeta.mbcName = "MBC1+RAM"; break;
        case 0x03: outMeta.mbcName = "MBC1+BAT"; outMeta.hasBattery = true; break;
        case 0x05: outMeta.mbcName = "MBC2"; break;
        case 0x06: outMeta.mbcName = "MBC2+BAT"; outMeta.hasBattery = true; break;
        case 0x08: outMeta.mbcName = "ROM+RAM"; break;
        case 0x09: outMeta.mbcName = "ROM+RAM+BAT"; outMeta.hasBattery = true; break;
        case 0x0B: outMeta.mbcName = "MMM01"; break;
        case 0x0C: outMeta.mbcName = "MMM01+RAM"; break;
        case 0x0D: outMeta.mbcName = "MMM01+BAT"; outMeta.hasBattery = true; break;
        case 0x0F: outMeta.mbcName = "MBC3+RTC"; outMeta.hasBattery = true; break;
        case 0x10: outMeta.mbcName = "MBC3+RTC+BAT"; outMeta.hasBattery = true; break;
        case 0x11: outMeta.mbcName = "MBC3"; break;
        case 0x12: outMeta.mbcName = "MBC3+RAM"; break;
        case 0x13: outMeta.mbcName = "MBC3+BAT"; outMeta.hasBattery = true; break;
        case 0x19: outMeta.mbcName = "MBC5"; break;
        case 0x1A: outMeta.mbcName = "MBC5+RAM"; break;
        case 0x1B: outMeta.mbcName = "MBC5+BAT"; outMeta.hasBattery = true; break;
        case 0x1C: outMeta.mbcName = "MBC5+RUMB"; break;
        case 0x1D: outMeta.mbcName = "MBC5+RUMB+RAM"; break;
        case 0x1E: outMeta.mbcName = "MBC5+RUMB+BAT"; outMeta.hasBattery = true; break;
        case 0x20: outMeta.mbcName = "MBC6"; break;
        case 0x22: outMeta.mbcName = "MBC7+BAT"; outMeta.hasBattery = true; break;
        case 0xFC: outMeta.mbcName = "CAMERA"; break;
        case 0xFD: outMeta.mbcName = "TAMA5"; break;
        case 0xFE: outMeta.mbcName = "HuC3"; break;
        case 0xFF: outMeta.mbcName = "HuC1+BAT"; outMeta.hasBattery = true; break;
        default:   outMeta.mbcName = "MBC?"; break;
    }

    // 5. ROM Size (0x0148)
    u8 romCode = hdr[0x48];
    switch(romCode) {
        case 0x00: outMeta.romSizeKB = 32; break;
        case 0x01: outMeta.romSizeKB = 64; break;
        case 0x02: outMeta.romSizeKB = 128; break;
        case 0x03: outMeta.romSizeKB = 256; break;
        case 0x04: outMeta.romSizeKB = 512; break;
        case 0x05: outMeta.romSizeKB = 1024; break;
        case 0x06: outMeta.romSizeKB = 2048; break;
        case 0x07: outMeta.romSizeKB = 4096; break;
        case 0x08: outMeta.romSizeKB = 8192; break;
        case 0x52: outMeta.romSizeKB = 1152; break;
        case 0x53: outMeta.romSizeKB = 1280; break;
        case 0x54: outMeta.romSizeKB = 1536; break;
        default:   outMeta.romSizeKB = (32 << (romCode <= 8 ? romCode : 0)); break;
    }

    // 6. RAM Size (0x0149)
    u8 ramCode = hdr[0x49];
    if(cartType == 0x05 || cartType == 0x06) {
        outMeta.ramSizeKB = 1;
    } else {
        switch(ramCode) {
            case 0x00: outMeta.ramSizeKB = 0; break;
            case 0x01: outMeta.ramSizeKB = 2; break;
            case 0x02: outMeta.ramSizeKB = 8; break;
            case 0x03: outMeta.ramSizeKB = 32; break;
            case 0x04: outMeta.ramSizeKB = 128; break;
            case 0x05: outMeta.ramSizeKB = 64; break;
            default:   outMeta.ramSizeKB = 0; break;
        }
    }

    // 7. Region (0x014A)
    u8 regionCode = hdr[0x4A];
    outMeta.region = (regionCode == 0x00) ? "JPN" : "USA/EUR";

    // 8. Licensee / Publisher
    if(oldLicensee == 0x33) {
        char newLic[3] = { (char)hdr[0x44], (char)hdr[0x45], '\0' };
        outMeta.publisher = getNewLicensee(newLic);
    } else {
        outMeta.publisher = getOldLicensee(oldLicensee);
    }

    // 9. Compact Database (Year / Genre)
    std::string upperTitle = outMeta.title;
    std::transform(upperTitle.begin(), upperTitle.end(), upperTitle.begin(), ::toupper);

    auto slash = romPath.find_last_of("/\\");
    std::string fileBase = (slash != std::string::npos) ? romPath.substr(slash + 1) : romPath;
    auto dot = fileBase.find_last_of('.');
    if(dot != std::string::npos) {
        fileBase = fileBase.substr(0, dot);
    }
    std::transform(fileBase.begin(), fileBase.end(), fileBase.begin(), ::toupper);

    bool matched = false;
    for(size_t i = 0; i < sizeof(s_titleDb) / sizeof(s_titleDb[0]); i++) {
        const TitleDbEntry& entry = s_titleDb[i];
        if(upperTitle.find(entry.key) != std::string::npos || fileBase.find(entry.key) != std::string::npos) {
            outMeta.year = entry.year;
            outMeta.genre = entry.genre;
            outMeta.developer = entry.developer;
            outMeta.synopsis = entry.synopsis;
            matched = true;
            break;
        }
    }

    if(!matched) {
        outMeta.year = 0;
        outMeta.genre = "Clasico";
        outMeta.developer = outMeta.publisher.empty() ? "Desconocido" : outMeta.publisher;
        outMeta.synopsis = "Titulo clasico de Game Boy totalmente compatible con el nucleo GameYob.";
    }

    // 10. Check .sav file
    std::string savPath = romPath;
    auto dotPos = savPath.find_last_of('.');
    if(dotPos != std::string::npos) {
        savPath = savPath.substr(0, dotPos) + ".sav";
    } else {
        savPath += ".sav";
    }

    struct stat st;
    if(stat(savPath.c_str(), &st) == 0 && st.st_size > 0) {
        outMeta.hasSave = true;
        int kb = (int)(st.st_size / 1024);
        if(kb > 0) {
            outMeta.saveDate = std::to_string(kb) + " KB";
        } else {
            outMeta.saveDate = "OK";
        }
    } else {
        outMeta.hasSave = false;
        outMeta.saveDate = "No Data";
    }

    return true;
}
