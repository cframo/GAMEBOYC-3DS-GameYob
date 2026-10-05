#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu/menu.h"
#include "platform/system.h"

#ifdef BACKEND_3DS
#include <3ds.h>
#endif

int main(int argc, char* argv[]) {
#ifdef BACKEND_3DS
    osSetSpeedupEnable(true);
#endif
    if(!systemInit(argc, argv)) {
        return false;
    }

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

    systemExit();
    return 0;
}
