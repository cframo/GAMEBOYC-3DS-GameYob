#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu/menu.h"
#include "platform/system.h"

#ifdef BACKEND_3DS
#include <3ds.h>
#include "platform/3ds/scraper_worker.h"
#endif

int main(int argc, char* argv[]) {
#ifdef BACKEND_3DS
    osSetSpeedupEnable(true);
#endif
    if(!systemInit(argc, argv)) {
        return false;
    }

#ifdef BACKEND_3DS
    scraperInit();
#endif

    mgrInit();
    setMenuDefaults();
    configLoad();

    while(true) {
        if(!systemIsRunning()) {
            break;
        }

        mgrRun();
    }

    mgrExit();

#ifdef BACKEND_3DS
    scraperExit();
#endif

    systemExit();
    return 0;
}
