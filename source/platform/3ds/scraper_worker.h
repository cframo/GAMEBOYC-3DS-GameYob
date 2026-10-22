#pragma once

#include <string>
#include "types.h"

struct ScrapeJob {
    std::string romPath;
    std::string romTitle;
    std::string cleanName;
    u8 headerChecksum;
    bool isGBC;
};

void scraperInit();
void scraperExit();
void scraperEnqueue(const std::string& romPath, const std::string& cleanTitle, u8 checksum);
bool scraperHasPendingCovers(const std::string& currentRomPath = "");
bool scraperCheckCoverReady(const std::string& romPath);
const char* scraperGetStatusText();
