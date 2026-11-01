// Stand-ins for the OEPL protocol and radio symbols that the shared board,
// drawing and power code reference. This firmware never uses the radio.
#include <stdint.h>

#define DEMO_BLOCKBUFFER_SIZE (74 * 16)   // one drawing part: LINES_PER_PART x BYTES_PER_LINE

uint8_t __xdata blockbuffer[DEMO_BLOCKBUFFER_SIZE];
uint8_t __xdata mSelfMac[8];
uint8_t __xdata gCurrentChannel;
uint8_t __xdata gSubGhzBand;
uint8_t __xdata mLastLqi;
int8_t __xdata mLastRSSI;
__bit gLowBatteryShown;
__bit noAPShown;

void DumpHex(const uint8_t *__xdata a, const uint16_t __xdata l)
{
   (void) a;
   (void) l;
}

void radioInit(void) {}

#pragma callee_saves radioSetTxPower
void radioSetTxPower(int8_t dBm) { (void) dBm; }

#pragma callee_saves radioSetChannel
void radioSetChannel(uint8_t ch) { (void) ch; }
