#include "compat.h"
#include "register.h"
#include "preferences.h"
#include "packs.h"
#include "resources.h"
#include "interface.h"
#include "screen.h"

UInt32 gKey = 0x1E42A71F;
int gRegistered = 1;

/* Registration check - always succeeds in the free version */
int CheckRegi() {
    gRegistered = 1;
    gKey = 0x1E42A71F;
    return 1;
}

void Register(int fullscreen) {
    (void)fullscreen;
    /* Show the about/credits screen */
    ShowPicScreen(1005);
    WaitForPress();
    ShowPicScreen(1000);
}
