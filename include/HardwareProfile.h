#ifndef EFERGY_HARDWARE_PROFILE_H
#define EFERGY_HARDWARE_PROFILE_H
#include "Compiler.h"
#include "board.h"
/* Matches original startup OSCTUNE=0x40 and J60 Ethernet crystal/PLL path. */
#define GetSystemClock() (41666667ul)
#define GetInstructionClock() (GetSystemClock()/4ul)
#define GetPeripheralClock() (GetSystemClock()/4ul)
#define _XTAL_FREQ 41666667ul
/* This board's status LEDs; do not use the reference board's demo pin map. */
#define EFERGY_NETWORK_ACTIVITY() (board_network_activity_pending=1)
#endif
