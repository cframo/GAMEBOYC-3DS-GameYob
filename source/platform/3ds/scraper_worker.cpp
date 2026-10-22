#include "platform/3ds/scraper_worker.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <vector>
#include <set>
#include <algorithm>
#include <cctype>

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

static Thread s_scraperThread = nullptr;
static LightLock s_queueLock;
static std::vector<ScrapeJob> s_jobQueue;
static std::set<std::string> s_readyCovers;
static std::set<std::string> s_failedPaths;
static std::string s_activeRomPath = "";
static volatile bool s_scraperRunning = false;
static volatile bool s_isJobActive = false;
static volatile u64 s_lastActivityTime = 0;

static char s_statusText[16] = "";
static u64 s_statusTime = 0;

static u8 s_threadStack[0x10000] __attribute__((aligned(8)));

static struct in_addr s_cachedWeservIp = { 0 };
static bool s_weservIpResolved = false;

static struct in_addr s_cachedLibretroIp = { 0 };
static bool s_libretroIpResolved = false;

static void scraperSetStatus(const char* text) {
    LightLock_Lock(&s_queueLock);
    strncpy(s_statusText, text, sizeof(s_statusText) - 1);
    s_statusText[sizeof(s_statusText) - 1] = '\0';
    s_statusTime = osGetTime();
    LightLock_Unlock(&s_queueLock);
}

const char* scraperGetStatusText() {
    LightLock_Lock(&s_queueLock);
    if(s_statusText[0] != '\0') {
        if(strncmp(s_statusText, "E:", 2) == 0 || strcmp(s_statusText, "DL:OK") == 0) {
            if(osGetTime() - s_statusTime > 2000) {
                s_statusText[0] = '\0';
            }
        }
    }
    const char* ret = s_statusText;
    LightLock_Unlock(&s_queueLock);
    return ret;
}

static bool containsIgnoreCase(const std::string& haystack, const std::string& needle) {
    if(needle.empty() || haystack.empty()) {
        return false;
    }
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(), needle.end(),
        [](char ch1, char ch2) { return std::toupper((unsigned char)ch1) == std::toupper((unsigned char)ch2); }
    );
    return (it != haystack.end());
}

static void resolveNoIntroInfo(const std::string& rawFilename, const std::string& headerTitle, bool isGBC, std::string& outRepoName, std::string& outBoxartName) {
    auto match = [&](const char* kw) {
        return containsIgnoreCase(rawFilename, kw) || containsIgnoreCase(headerTitle, kw);
    };

    if(match("GOLD") || match("ORO") || match("GLD")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Pokemon - Gold Version (USA, Europe) (SGB Enhanced) (GB Compatible).png";
        return;
    }
    if(match("SILVER") || match("PLATA") || match("SLV")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Pokemon - Silver Version (USA, Europe) (SGB Enhanced) (GB Compatible).png";
        return;
    }
    if(match("CRYSTAL") || match("CRISTAL") || match("CRY")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Pokemon - Crystal Version (USA, Europe) (Rev 1).png";
        return;
    }
    if(match("RED") || match("ROJO")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Pokemon - Red Version (USA, Europe) (SGB Enhanced).png";
        return;
    }
    if(match("BLUE") || match("AZUL")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Pokemon - Blue Version (USA, Europe) (SGB Enhanced).png";
        return;
    }
    if(match("YELLOW") || match("AMARILLO") || match("YEL")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Pokemon - Yellow Version - Special Pikachu Edition (USA, Europe) (CGB+SGB Enhanced).png";
        return;
    }
    if(match("TETRIS DX")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Tetris DX (World) (SGB Enhanced) (GB Compatible).png";
        return;
    }
    if(match("TETRIS")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Tetris (World) (Rev 1).png";
        return;
    }
    if(match("DONKEY KONG LAND") || match("DONKEYKONGLAN")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Donkey Kong Land (USA, Europe) (SGB Enhanced).png";
        return;
    }
    if(match("DONKEY KONG")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Donkey Kong (World) (Rev 1) (SGB Enhanced).png";
        return;
    }
    if(match("RAYMAN")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Rayman (USA) (En,Fr,De,Es,It,Nl).png";
        return;
    }
    if(match("ZELDA")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Legend of Zelda, The - Link's Awakening DX (USA, Europe) (Rev 2) (SGB Enhanced) (GB Compatible).png";
        return;
    }
    if(match("MARIO LAND 2") || match("MARIOLAND2") || match("6 GOLDEN COINS")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev 2).png";
        return;
    }
    if(match("WARIO LAND 2") || match("WARIOLAND2")) {
        outRepoName = "Nintendo_-_Game_Boy_Color";
        outBoxartName = "Wario Land II (USA, Europe) (SGB Enhanced) (GB Compatible).png";
        return;
    }
    if(match("WARIO")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Wario Land - Super Mario Land 3 (World).png";
        return;
    }
    if(match("MARIO")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Super Mario Land (World) (Rev 1).png";
        return;
    }
    if(match("METROID")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Metroid II - Return of Samus (World).png";
        return;
    }
    if(match("KIRBY")) {
        outRepoName = "Nintendo_-_Game_Boy";
        outBoxartName = "Kirby's Dream Land (USA, Europe).png";
        return;
    }

    outRepoName = isGBC ? "Nintendo_-_Game_Boy_Color" : "Nintendo_-_Game_Boy";
    outBoxartName = rawFilename + ".png";
}

static std::string urlEncode(const std::string& str) {
    std::string escaped = "";
    for(size_t i = 0; i < str.length(); i++) {
        unsigned char c = (unsigned char) str[i];
        if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == '(' || c == ')') {
            escaped += c;
        } else if(c == ' ') {
            escaped += "%20";
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", c);
            escaped += hex;
        }
    }
    return escaped;
}

static bool resolveHost(const char* hostname, struct in_addr* outIp, bool isWeserv) {
    if(isWeserv && s_weservIpResolved && s_cachedWeservIp.s_addr != 0) {
        *outIp = s_cachedWeservIp;
        return true;
    }
    if(!isWeserv && s_libretroIpResolved && s_cachedLibretroIp.s_addr != 0) {
        *outIp = s_cachedLibretroIp;
        return true;
    }

    struct hostent* he = gethostbyname(hostname);
    if(he != nullptr && he->h_addr_list[0] != nullptr) {
        if(isWeserv) {
            s_cachedWeservIp = *(struct in_addr*) he->h_addr_list[0];
            s_weservIpResolved = true;
            *outIp = s_cachedWeservIp;
        } else {
            s_cachedLibretroIp = *(struct in_addr*) he->h_addr_list[0];
            s_libretroIpResolved = true;
            *outIp = s_cachedLibretroIp;
        }
        return true;
    }
    return false;
}

#ifdef HAVE_LIBCURL
struct CurlContext {
    std::vector<u8>* buffer;
    const std::string* jobRomPath;
    volatile bool* running;
    u64 lastKbReport;
};

static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalBytes = size * nmemb;
    CurlContext* ctx = (CurlContext*) userp;

    LightLock_Lock(&s_queueLock);
    bool shouldCancel = (!*(ctx->running) || s_activeRomPath != *(ctx->jobRomPath));
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        return 0;
    }

    ctx->buffer->insert(ctx->buffer->end(), (u8*)contents, (u8*)contents + totalBytes);
    s_lastActivityTime = osGetTime();

    u64 curKb = ctx->buffer->size() / 1024;
    if(curKb != ctx->lastKbReport) {
        ctx->lastKbReport = curKb;
        char kbStr[16];
        snprintf(kbStr, sizeof(kbStr), "DL:%uK", (unsigned)curKb);
        scraperSetStatus(kbStr);
    }

    return totalBytes;
}

static int curlXferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    (void)dltotal;
    (void)dlnow;
    (void)ultotal;
    (void)ulnow;
    CurlContext* ctx = (CurlContext*) clientp;
    LightLock_Lock(&s_queueLock);
    bool shouldCancel = (!*(ctx->running) || s_activeRomPath != *(ctx->jobRomPath));
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        return 1;
    }
    s_lastActivityTime = osGetTime();
    return 0;
}

static bool downloadWithCurl(const std::string& url, const std::string& outPath, const std::string& jobRomPath, volatile bool* running) {
    scraperSetStatus("DL:Con");

    LightLock_Lock(&s_queueLock);
    bool shouldCancel = (!*running || s_activeRomPath != jobRomPath);
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        return false;
    }

    CURL* curl = curl_easy_init();
    if(!curl) {
        scraperSetStatus("E:Init");
        return false;
    }

    std::vector<u8> buffer;
    buffer.reserve(65536);

    CurlContext ctx;
    ctx.buffer = &buffer;
    ctx.jobRomPath = &jobRomPath;
    ctx.running = running;
    ctx.lastKbReport = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlXferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "GameYob3DS/1.0");
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    s_lastActivityTime = osGetTime();
    CURLcode res = curl_easy_perform(curl);

    LightLock_Lock(&s_queueLock);
    shouldCancel = (!*running || s_activeRomPath != jobRomPath);
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        curl_easy_cleanup(curl);
        return false;
    }

    if(res != CURLE_OK) {
        char errBuf[16];
        snprintf(errBuf, sizeof(errBuf), "E:C%d", (int)res);
        scraperSetStatus(errBuf);
        curl_easy_cleanup(curl);
        return false;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if(httpCode != 200) {
        char errBuf[16];
        snprintf(errBuf, sizeof(errBuf), "E:H%d", (int)httpCode);
        scraperSetStatus(errBuf);
        return false;
    }

    const u8 pngMagic[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    auto it = std::search(buffer.begin(), buffer.end(), pngMagic, pngMagic + 8);
    if(it == buffer.end()) {
        scraperSetStatus("E:PNG");
        return false;
    }

    size_t pngOffset = std::distance(buffer.begin(), it);
    size_t pngSize = buffer.size() - pngOffset;
    if(pngSize < 32) {
        scraperSetStatus("E:PNG");
        return false;
    }

    std::string tmpPath = outPath + ".tmp";
    FILE* fp = fopen(tmpPath.c_str(), "wb");
    if(fp == nullptr) {
        scraperSetStatus("E:IO");
        return false;
    }

    size_t written = fwrite(buffer.data() + pngOffset, 1, pngSize, fp);
    fclose(fp);

    if(written != pngSize) {
        remove(tmpPath.c_str());
        scraperSetStatus("E:IO");
        return false;
    }

    remove(outPath.c_str());
    if(rename(tmpPath.c_str(), outPath.c_str()) == 0) {
        scraperSetStatus("DL:OK");
        return true;
    } else {
        remove(tmpPath.c_str());
        scraperSetStatus("E:IO");
        return false;
    }
}
#endif

static bool downloadWithSocket(const std::string& host, const std::string& path, const std::string& outPath, const std::string& jobRomPath, volatile bool* running) {
    scraperSetStatus("DL:DNS");

    bool isWeserv = (host.find("weserv") != std::string::npos);
    struct in_addr serverIp;
    if(!resolveHost(host.c_str(), &serverIp, isWeserv)) {
        scraperSetStatus("E:DNS");
        return false;
    }

    LightLock_Lock(&s_queueLock);
    bool shouldCancel = (!*running || s_activeRomPath != jobRomPath);
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        return false;
    }

    scraperSetStatus("DL:Con");

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0) {
        char errBuf[16];
        snprintf(errBuf, sizeof(errBuf), "E:C%d", errno);
        scraperSetStatus(errBuf);
        return false;
    }

    int flags = fcntl(sock, F_GETFL);
    if(flags != -1) {
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    serverAddr.sin_addr = serverIp;

    int connRet = connect(sock, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
    if(connRet < 0) {
        if(errno != EINPROGRESS && errno != EAGAIN && errno != EWOULDBLOCK && errno != -EINPROGRESS) {
            int err = errno;
            closesocket(sock);
            if(isWeserv) s_weservIpResolved = false;
            else s_libretroIpResolved = false;
            char errBuf[16];
            snprintf(errBuf, sizeof(errBuf), "E:C%d", err);
            scraperSetStatus(errBuf);
            return false;
        }
    }

    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int pollRet = poll(&pfd, 1, 3000);
    if(pollRet <= 0 || !(pfd.revents & POLLOUT) || (pfd.revents & (POLLERR | POLLHUP))) {
        int err = errno;
        closesocket(sock);
        if(isWeserv) s_weservIpResolved = false;
        else s_libretroIpResolved = false;
        char errBuf[16];
        snprintf(errBuf, sizeof(errBuf), "E:C%d", err != 0 ? err : 1);
        scraperSetStatus(errBuf);
        return false;
    }

    LightLock_Lock(&s_queueLock);
    shouldCancel = (!*running || s_activeRomPath != jobRomPath);
    LightLock_Unlock(&s_queueLock);
    if(shouldCancel) {
        closesocket(sock);
        return false;
    }

    s_lastActivityTime = osGetTime();

    std::string req = "GET " + path + " HTTP/1.0\r\n"
                      "Host: " + host + "\r\n"
                      "User-Agent: GameYob3DS/1.0\r\n"
                      "Connection: close\r\n\r\n";

    ssize_t sent = send(sock, req.c_str(), req.length(), 0);
    if(sent <= 0) {
        int err = errno;
        closesocket(sock);
        char errBuf[16];
        snprintf(errBuf, sizeof(errBuf), "E:C%d", err != 0 ? err : 2);
        scraperSetStatus(errBuf);
        return false;
    }
    size_t totalSent = (size_t)sent;
    while(totalSent < req.length()) {
        ssize_t s = send(sock, req.c_str() + totalSent, req.length() - totalSent, 0);
        if(s <= 0) {
            closesocket(sock);
            scraperSetStatus("E:Con");
            return false;
        }
        totalSent += s;
    }

    std::vector<u8> buffer;
    buffer.reserve(65536);

    char chunk[4096];
    u64 lastKbReport = 0;

    while(*running) {
        LightLock_Lock(&s_queueLock);
        shouldCancel = (!*running || s_activeRomPath != jobRomPath);
        LightLock_Unlock(&s_queueLock);
        if(shouldCancel) {
            break;
        }

        pfd.fd = sock;
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pRet = poll(&pfd, 1, 2000);
        if(pRet <= 0) {
            break;
        }

        ssize_t bytesRead = recv(sock, chunk, sizeof(chunk), 0);
        if(bytesRead <= 0) {
            break;
        }

        buffer.insert(buffer.end(), (u8*)chunk, (u8*)chunk + bytesRead);
        s_lastActivityTime = osGetTime();

        u64 curKb = buffer.size() / 1024;
        if(curKb != lastKbReport) {
            lastKbReport = curKb;
            char kbStr[16];
            snprintf(kbStr, sizeof(kbStr), "DL:%uK", (unsigned)curKb);
            scraperSetStatus(kbStr);
        }
    }

    closesocket(sock);

    if(shouldCancel || buffer.empty()) {
        return false;
    }

    std::string headStr((char*)buffer.data(), std::min((size_t)512, buffer.size()));
    if(headStr.find("HTTP/1.1 200") == std::string::npos && headStr.find("HTTP/1.0 200") == std::string::npos) {
        scraperSetStatus("E:HTTP");
        return false;
    }

    const u8 pngMagic[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    auto it = std::search(buffer.begin(), buffer.end(), pngMagic, pngMagic + 8);
    if(it == buffer.end()) {
        scraperSetStatus("E:PNG");
        return false;
    }

    size_t pngOffset = std::distance(buffer.begin(), it);
    size_t pngSize = buffer.size() - pngOffset;
    if(pngSize < 32) {
        scraperSetStatus("E:PNG");
        return false;
    }

    std::string tmpPath = outPath + ".tmp";
    FILE* fp = fopen(tmpPath.c_str(), "wb");
    if(fp == nullptr) {
        scraperSetStatus("E:IO");
        return false;
    }

    size_t written = fwrite(buffer.data() + pngOffset, 1, pngSize, fp);
    fclose(fp);

    if(written != pngSize) {
        remove(tmpPath.c_str());
        scraperSetStatus("E:IO");
        return false;
    }

    remove(outPath.c_str());
    if(rename(tmpPath.c_str(), outPath.c_str()) == 0) {
        scraperSetStatus("DL:OK");
        return true;
    } else {
        remove(tmpPath.c_str());
        scraperSetStatus("E:IO");
        return false;
    }
}

static void scraperThreadFunc(void* arg) {
    (void) arg;

    while(s_scraperRunning) {
        ScrapeJob job;
        bool hasJob = false;

        LightLock_Lock(&s_queueLock);
        if(!s_jobQueue.empty()) {
            job = s_jobQueue.front();
            s_jobQueue.erase(s_jobQueue.begin());
            hasJob = true;
            s_isJobActive = true;
            s_activeRomPath = job.romPath;
        } else {
            s_isJobActive = false;
        }
        LightLock_Unlock(&s_queueLock);

        if(!hasJob) {
            svcSleepThread(100000000ULL); // 100 ms
            continue;
        }

        // 1. Comprobacion previa de archivo local en SD
        std::string targetPng = "/3ds/gameyob/boxart/" + job.cleanName + ".png";
        std::string targetTitlePng = job.romTitle.empty() ? "" : ("/3ds/gameyob/boxart/" + job.romTitle + ".png");

        struct stat st;
        if(stat(targetPng.c_str(), &st) == 0 && st.st_size > 0) {
            LightLock_Lock(&s_queueLock);
            s_readyCovers.insert(job.romPath);
            s_isJobActive = false;
            LightLock_Unlock(&s_queueLock);
            scraperSetStatus("DL:OK");
            continue;
        }
        if(!targetTitlePng.empty() && stat(targetTitlePng.c_str(), &st) == 0 && st.st_size > 0) {
            LightLock_Lock(&s_queueLock);
            s_readyCovers.insert(job.romPath);
            s_isJobActive = false;
            LightLock_Unlock(&s_queueLock);
            scraperSetStatus("DL:OK");
            continue;
        }

        // 2. Aborto inmediato si no hay conexion
        u8 wifi = osGetWifiStrength();
        if(wifi == 0) {
            LightLock_Lock(&s_queueLock);
            s_failedPaths.insert(job.romPath);
            s_isJobActive = false;
            LightLock_Unlock(&s_queueLock);
            scraperSetStatus("E:WiFi");
            svcSleepThread(500000000ULL);
            continue;
        }

        // 3. Resolver ruta No-Intro flexible
        std::string repoName;
        std::string boxartName;
        resolveNoIntroInfo(job.cleanName, job.romTitle, job.isGBC, repoName, boxartName);
        std::string encodedFileName = urlEncode(boxartName);

        bool downloaded = false;

#ifdef HAVE_LIBCURL
        // FUENTE 1: GitHub Raw HTTPS (TLS moderno por software vía libcurl + mbedtls en Core 1)
        if(!downloaded && s_scraperRunning) {
            s_lastActivityTime = osGetTime();
            std::string ghUrl = "https://raw.githubusercontent.com/libretro-thumbnails/" + repoName + "/master/Named_Boxarts/" + encodedFileName;
            downloaded = downloadWithCurl(ghUrl, targetPng, job.romPath, &s_scraperRunning);
        }

        // FUENTE 2: CDN Libretro HTTPS
        if(!downloaded && s_scraperRunning) {
            LightLock_Lock(&s_queueLock);
            bool preempted = (s_activeRomPath != job.romPath);
            LightLock_Unlock(&s_queueLock);
            if(!preempted) {
                s_lastActivityTime = osGetTime();
                std::string repoDir = (repoName == "Nintendo_-_Game_Boy_Color") ? "Nintendo%20-%20Game%20Boy%20Color" : "Nintendo%20-%20Game%20Boy";
                std::string cdnUrl = "https://thumbnails.libretro.com/" + repoDir + "/Named_Boxarts/" + encodedFileName;
                downloaded = downloadWithCurl(cdnUrl, targetPng, job.romPath, &s_scraperRunning);
            }
        }

        // FUENTE 3a: Proxy images.weserv.nl vía libcurl (Puerto 80 HTTP, 128x128)
        if(!downloaded && s_scraperRunning) {
            LightLock_Lock(&s_queueLock);
            bool preempted = (s_activeRomPath != job.romPath);
            LightLock_Unlock(&s_queueLock);
            if(!preempted) {
                s_lastActivityTime = osGetTime();
                std::string weservUrl = "http://images.weserv.nl/?url=raw.githubusercontent.com/libretro-thumbnails/" + repoName + "/master/Named_Boxarts/" + encodedFileName + "&w=128&h=128&output=png";
                downloaded = downloadWithCurl(weservUrl, targetPng, job.romPath, &s_scraperRunning);
            }
        }
#endif

        // FUENTE 3b: Fallback de Socket BSD no-bloqueante corregido (Puerto 80 HTTP)
        if(!downloaded && s_scraperRunning) {
            LightLock_Lock(&s_queueLock);
            bool preempted = (s_activeRomPath != job.romPath);
            LightLock_Unlock(&s_queueLock);
            if(!preempted) {
                s_lastActivityTime = osGetTime();
                std::string weservPath = "/?url=raw.githubusercontent.com/libretro-thumbnails/" + repoName + "/master/Named_Boxarts/" + encodedFileName + "&w=128&h=128&output=png";
                downloaded = downloadWithSocket("images.weserv.nl", weservPath, targetPng, job.romPath, &s_scraperRunning);
            }
        }

        LightLock_Lock(&s_queueLock);
        if(downloaded) {
            s_readyCovers.insert(job.romPath);
        } else {
            if(s_activeRomPath == job.romPath) {
                s_failedPaths.insert(job.romPath);
            }
        }
        s_isJobActive = false;
        LightLock_Unlock(&s_queueLock);

        svcSleepThread(50000000ULL);
    }
}

void scraperInit() {
    LightLock_Init(&s_queueLock);
    s_scraperRunning = true;
    s_isJobActive = false;
    s_activeRomPath = "";
    s_lastActivityTime = 0;
    s_statusText[0] = '\0';
    s_statusTime = 0;
    s_jobQueue.clear();
    s_readyCovers.clear();
    s_failedPaths.clear();

    mkdir("/3ds", 0777);
    mkdir("/3ds/gameyob", 0777);
    mkdir("/3ds/gameyob/boxart", 0777);

#ifdef HAVE_LIBCURL
    curl_global_init(CURL_GLOBAL_ALL);
#endif

    // Habilitar multi-threading en Core 1 mediante APT
    APT_SetAppCpuTimeLimit(30);

    // Obtener prioridad relativa del hilo actual
    s32 mainPrio = 0x30;
    svcGetThreadPriority(&mainPrio, CUR_THREAD_HANDLE);
    s32 workerPrio = (mainPrio < 0x3E) ? (mainPrio + 2) : 0x3E;

    // Intentar crear hilo en Core 1
    s_scraperThread = threadCreate(scraperThreadFunc, nullptr, sizeof(s_threadStack), workerPrio, 1, false);

    // FALLBACK CRITICO: Si Core 1 devuelve NULL, crearlo en Core 0
    if(!s_scraperThread) {
        s_scraperThread = threadCreate(scraperThreadFunc, nullptr, sizeof(s_threadStack), workerPrio, 0, false);
    }
}

void scraperExit() {
    s_scraperRunning = false;
    if(s_scraperThread != nullptr) {
        threadJoin(s_scraperThread, U64_MAX);
        threadFree(s_scraperThread);
        s_scraperThread = nullptr;
    }
#ifdef HAVE_LIBCURL
    curl_global_cleanup();
#endif
}

void scraperEnqueue(const std::string& romPath, const std::string& cleanTitle, u8 checksum) {
    if(romPath.empty()) {
        return;
    }

    if(!s_scraperThread) {
        return;
    }

    if(osGetWifiStrength() == 0) {
        return;
    }

    auto slash = romPath.find_last_of("/\\");
    std::string base = (slash != std::string::npos) ? romPath.substr(slash + 1) : romPath;
    auto dot = base.find_last_of('.');
    std::string ext = "";
    if(dot != std::string::npos) {
        ext = base.substr(dot + 1);
        base = base.substr(0, dot);
    }

    // Comprobacion previa de archivo local en SD
    std::string path1 = "/3ds/gameyob/boxart/" + base + ".png";
    std::string path2 = cleanTitle.empty() ? "" : ("/3ds/gameyob/boxart/" + cleanTitle + ".png");

    struct stat st;
    if((stat(path1.c_str(), &st) == 0 && st.st_size > 0) ||
       (!path2.empty() && stat(path2.c_str(), &st) == 0 && st.st_size > 0)) {
        LightLock_Lock(&s_queueLock);
        s_readyCovers.insert(romPath);
        LightLock_Unlock(&s_queueLock);
        return;
    }

    LightLock_Lock(&s_queueLock);

    if(s_failedPaths.find(romPath) != s_failedPaths.end() ||
       s_readyCovers.find(romPath) != s_readyCovers.end()) {
        LightLock_Unlock(&s_queueLock);
        return;
    }

    if(s_isJobActive && s_activeRomPath == romPath) {
        LightLock_Unlock(&s_queueLock);
        return;
    }

    // Poda de cola a tamano 1 con cancelacion preemptiva
    s_jobQueue.clear();
    s_activeRomPath = romPath;
    s_lastActivityTime = osGetTime();

    ScrapeJob job;
    job.romPath = romPath;
    job.romTitle = cleanTitle;
    job.headerChecksum = checksum;
    job.cleanName = base;
    job.isGBC = (strcasecmp(ext.c_str(), "gbc") == 0 || strcasecmp(ext.c_str(), "cgb") == 0);

    s_jobQueue.push_back(job);
    LightLock_Unlock(&s_queueLock);
}

bool scraperHasPendingCovers(const std::string& currentRomPath) {
    LightLock_Lock(&s_queueLock);

    // Heartbeat Watchdog: si no hay actividad real durante 4500 ms, abortar por inactividad
    if(s_isJobActive || !s_jobQueue.empty()) {
        if(s_lastActivityTime > 0 && (osGetTime() - s_lastActivityTime > 4500)) {
            s_jobQueue.clear();
            s_isJobActive = false;
            if(!currentRomPath.empty()) {
                s_failedPaths.insert(currentRomPath);
            }
            if(!s_activeRomPath.empty()) {
                s_failedPaths.insert(s_activeRomPath);
            }
            s_lastActivityTime = 0;
            strncpy(s_statusText, "E:Time", sizeof(s_statusText) - 1);
            s_statusText[sizeof(s_statusText) - 1] = '\0';
            s_statusTime = osGetTime();
            LightLock_Unlock(&s_queueLock);
            return false;
        }
    }

    bool pending = false;
    if(currentRomPath.empty()) {
        pending = s_isJobActive || !s_jobQueue.empty();
    } else {
        if(s_isJobActive && s_activeRomPath == currentRomPath) {
            pending = true;
        } else if(!s_jobQueue.empty() && s_jobQueue.front().romPath == currentRomPath) {
            pending = true;
        }
    }
    LightLock_Unlock(&s_queueLock);
    return pending;
}

bool scraperCheckCoverReady(const std::string& romPath) {
    LightLock_Lock(&s_queueLock);
    auto it = s_readyCovers.find(romPath);
    if(it != s_readyCovers.end()) {
        s_readyCovers.erase(it);
        LightLock_Unlock(&s_queueLock);
        return true;
    }
    LightLock_Unlock(&s_queueLock);
    return false;
}
