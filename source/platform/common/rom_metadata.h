#pragma once

#include <string>
#include "types.h"

struct RomMetadata {
    std::string title;
    std::string publisher;
    std::string developer;
    std::string synopsis;
    std::string genre;
    int year;
    std::string mbcName;
    int romSizeKB;
    int ramSizeKB;
    bool hasBattery;
    bool isGBC;
    bool isCgbOnly;
    bool isSGB;
    std::string region;
    bool hasSave;
    std::string saveDate;
};

bool fetchRomMetadata(const std::string& romPath, RomMetadata& outMeta);
