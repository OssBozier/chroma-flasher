// Panel refresh for the Chroma 29: wakes up e-paper that has sat on one image for a long time by
// driving every pixel through full black, white and red refreshes over and over, showing a test
// pattern after each block of cycles, then resting on the last one. No radio.
// Each white screen shows the cycle count and an estimate of the minutes left, placed at a different
// spot each time so no area of the panel always gets the same drive.
// Reflash chroma29_full_0015.bin afterwards to turn the tag back into an OEPL tag.
#include <stdbool.h>
#include <stdint.h>
#include "board.h"
#include "drawing.h"
#include "powermgt.h"
#include "screen.h"
#include "frames.h"   // frames: 0 the intro, 1 the test pattern, 2 the final test pattern

#ifndef CYCLES
#define CYCLES 40
#endif
#ifndef TEST_EVERY
#define TEST_EVERY 10
#endif
#define PAUSE_MS 5000UL        // rest between refreshes
#define TEST_SHOW_MS 60000UL   // how long each test pattern stays up before cycling resumes
#define REFRESH_MS 8400UL      // one full refresh, timed on a tag on 20 Oct 2026 (30 cycles in 29.6 min)
#define BYTES_PER_LINE (SCREEN_WIDTH / 8)

// Counter text: a 5x7 font (one byte per column, bit 0 the top row) drawn 3 times size
#define SCALE 3
#define CHAR_W (6 * SCALE)          // 5 columns and a gap
#define TEXT_LINES 2
#define TEXT_CHARS 6
#define TEXT_W (TEXT_CHARS * CHAR_W)
#define LINE_H (9 * SCALE)          // 7 rows and a gap
#define TEXT_H (TEXT_LINES * LINE_H)

static const uint8_t __code gFont[][5] = {
   {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},   // 0 1
   {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},   // 2 3
   {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},   // 4 5
   {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},   // 6 7
   {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},   // 8 9
   {0x20, 0x10, 0x08, 0x04, 0x02}, {0x7F, 0x02, 0x0C, 0x02, 0x7F},   // / M
   {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x7F, 0x04, 0x08, 0x10, 0x7F},   // I N
   {0x00, 0x00, 0x00, 0x00, 0x00},                                   // space
};
enum { CH_SLASH = 10, CH_M, CH_I, CH_N, CH_SPACE };

enum { SHOW_FRAME, FILL_BLACK, FILL_WHITE, FILL_RED };
enum { FRAME_INTRO, FRAME_TEST, FRAME_DONE };

extern uint8_t __xdata blockbuffer[];
extern __xdata int16_t gDrawY;
extern __xdata int16_t gPartY;

static __xdata uint8_t gShow;
static __xdata uint8_t gFrame;
static const uint8_t __code *__xdata gSrc;
static __xdata uint8_t gRun;      // bytes left in a repeat run
static __xdata uint8_t gLit;      // bytes left in a literal run
static __xdata uint8_t gVal;
static __xdata uint8_t gText[TEXT_LINES][TEXT_CHARS];   // gFont indexes
static __xdata int16_t gTextX;
static __xdata int16_t gTextY;
static __xdata uint8_t gTextLines;   // 0 for no counter
static __xdata uint16_t gRandom = 0xACE1;

// 16-bit Galois LFSR: the counter's position on each white screen
static uint16_t Random(void)
{
   gRandom = (gRandom >> 1) ^ (-(int16_t) (gRandom & 1) & 0xB400u);
   return gRandom;
}

// Writes Value's digits from Pos on; returns the position after them
static uint8_t PutNumber(uint8_t *Line, uint8_t Pos, uint16_t Value)
{
   uint8_t Digits[5];
   uint8_t n = 0;

   do {
      Digits[n++] = Value % 10;
      Value /= 10;
   } while(Value && n < 5);
   while(n && Pos < TEXT_CHARS) {
      Line[Pos++] = Digits[--n];
   }
   return Pos;
}

// "12/40" over "45 MIN" (Lines 2) or just "12/40" (Lines 1), at a new random spot
static void SetCounter(uint8_t Cycle, uint8_t Lines)
{
   uint8_t i;
   uint8_t Pos;
   uint32_t LeftMs;
   uint8_t TestsLeft = (CYCLES - Cycle) / TEST_EVERY + 1;

   for(i = 0; i < TEXT_CHARS; i++) {
      gText[0][i] = CH_SPACE;
      gText[1][i] = CH_SPACE;
   }
   Pos = PutNumber(gText[0], 0, Cycle);
   gText[0][Pos++] = CH_SLASH;
   PutNumber(gText[0], Pos, CYCLES);

   LeftMs = (uint32_t) (CYCLES - Cycle + 1) * 4 * (REFRESH_MS + PAUSE_MS)
          + (uint32_t) TestsLeft * (REFRESH_MS + TEST_SHOW_MS);
   Pos = PutNumber(gText[1], 0, (uint16_t) ((LeftMs + 30000UL) / 60000UL));
   Pos++;
   gText[1][Pos++] = CH_M;
   gText[1][Pos++] = CH_I;
   if(Pos < TEXT_CHARS) {
      gText[1][Pos] = CH_N;
   }
   gTextLines = Lines;
   gTextX = Random() % (DISPLAY_WIDTH - TEXT_W);
   gTextY = Random() % (DISPLAY_HEIGHT - TEXT_H);
}

// ORs the counter's pixels in logical column X into one physical line
static void DrawCounterColumn(uint8_t __xdata *pLine, int16_t X)
{
   uint8_t Line;
   uint8_t Bits;
   uint8_t Row;
   uint8_t s;
   int16_t Y;
   int16_t dx = X - gTextX;

   if(dx < 0 || dx >= TEXT_W || (dx % CHAR_W) >= 5 * SCALE) {
      return;
   }
   for(Line = 0; Line < gTextLines; Line++) {
      Bits = gFont[gText[Line][dx / CHAR_W]][(dx % CHAR_W) / SCALE];
      for(Row = 0; Row < 7; Row++) {
         if(Bits & (1 << Row)) {
            for(s = 0; s < SCALE; s++) {
               Y = gTextY + Line * LINE_H + Row * SCALE + s;
               pLine[Y >> 3] |= 0x80 >> (Y & 7);
            }
         }
      }
   }
}

// PackBits decoder: the planes are read strictly in order, one screen line per call
static uint8_t NextByte(void)
{
   uint8_t Hdr;

   if(gRun) {
      gRun--;
      return gVal;
   }
   if(gLit) {
      gLit--;
      return *gSrc++;
   }
   Hdr = *gSrc++;
   if(Hdr < 128) {
      gLit = Hdr;
      return *gSrc++;
   }
   gRun = (uint8_t) (256 - Hdr);   // 257 - Hdr copies, one returned now
   gVal = *gSrc++;
   return gVal;
}

// Called by DoPass() once per physical line, black pass first then red
void DrawLine(void) __reentrant
{
   uint8_t __xdata *pLine;
   uint8_t i;
   uint8_t Fill;

   pLine = &blockbuffer[(gDrawY - gPartY) * BYTES_PER_LINE];
   if(gShow == SHOW_FRAME) {
      if(gDrawY == 0) {
         gSrc = gRedPass ? gCardRed[gFrame] : gCardBlack[gFrame];
         gRun = 0;
         gLit = 0;
      }
      for(i = 0; i < BYTES_PER_LINE; i++) {
         pLine[i] |= NextByte();
      }
      if(gTextLines && !gRedPass) {
         DrawCounterColumn(pLine, DISPLAY_WIDTH - 1 - gDrawY);
      }
      return;
   }
   Fill = ((gShow == FILL_BLACK && !gRedPass) || (gShow == FILL_RED && gRedPass)) ? 0xff : 0;
   for(i = 0; i < BYTES_PER_LINE; i++) {
      pLine[i] |= Fill;
   }
   if(gShow == FILL_WHITE && !gRedPass) {
   // physical line p holds logical column 295 - p (see make_images.py)
      DrawCounterColumn(pLine, DISPLAY_WIDTH - 1 - gDrawY);
   }
}

static void Show(uint8_t What, uint32_t RestMs)
{
   gShow = What;
   DrawScreen(DrawLine);
   doSleep(RestMs);
}

void main()
{
   uint8_t Cycle;

   powerUp(INIT_BASE);
   boardInitStage2();
// a little sleep, as OEPL does, to avoid a partial boot during battery insertion
   doSleep(400UL);

   gFrame = FRAME_INTRO;
   gTextLines = 0;
   Show(SHOW_FRAME, 20000UL);
   for(Cycle = 1; Cycle <= CYCLES; Cycle++) {
      SetCounter(Cycle, 2);
      Show(FILL_BLACK, PAUSE_MS);
      Show(FILL_WHITE, PAUSE_MS);
      Show(FILL_RED, PAUSE_MS);
      SetCounter(Cycle, 2);   // a new spot for the second white screen
      Show(FILL_WHITE, PAUSE_MS);
      if(Cycle == CYCLES) {
         gFrame = FRAME_DONE;
         gTextLines = 0;
         Show(SHOW_FRAME, 1000UL);
      }
      else if(Cycle % TEST_EVERY == 0) {
      // the count goes after the "PANEL TEST after" title
         gFrame = FRAME_TEST;
         SetCounter(Cycle, 1);
         gTextX = 150;
         gTextY = 2;
         Show(SHOW_FRAME, TEST_SHOW_MS);
      }
   }
// Rest on the final test pattern: e-paper keeps it with no power
   for(;;) {
      doSleep(3600000UL);
   }
}
