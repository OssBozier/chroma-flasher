# Panel refresh firmware

A small firmware for Chroma 29 tags (BWR) that tries to wake up e-paper which has sat on one
image for years. Thin lines and small text are the first to go pale on a tired panel.

It drives every pixel through full **black, white, red, white** refreshes, 40 cycles by default
(about 41 minutes, at about 8.4 s a refresh). Each white screen shows the cycle and the minutes
left, at a new random spot each time so no area always gets the same drive. After every 10 cycles
a **test pattern** stays up for a minute: solid black and red, 1-pixel checkerboards, 1 to 3 px
lines and small text, to compare with a good tag. After the last cycle it rests on a "DONE" test
pattern, which e-paper keeps with no power. No radio.

Flash it, keep the tag powered for the run, then flash OpenEPaperLink back:

```sh
chroma-flash JA00012345B --no-backup --image panel-refresh/chroma29_panel_refresh.bin
# ... about 41 minutes later
chroma-flash JA00012345B --no-backup
```

What to expect: most of any improvement shows in the first 10–20 cycles. On the first tag tried,
solid areas and 2–3 px lines came up well, but 1-pixel lines and small red text stayed faint from
cycle 20 on: that part is the panel's age, which no amount of driving brings back.

## Building

No prebuilt image is included, because it is compiled together with Tag_FW_Chroma's code. To build
`chroma29_panel_refresh.bin`, you need SDCC, uv,
and a checkout of [Tag_FW_Chroma](https://github.com/OpenEPaperLink/Tag_FW_Chroma) (it uses that
tree's board, screen and power code):

```sh
make FW=/path/to/Tag_FW_Chroma/Chroma_Tag_FW               # 40 cycles, a test pattern every 10
make FW=/path/to/Tag_FW_Chroma/Chroma_Tag_FW CYCLES=20 TEST_EVERY=5
```

`render_refresh.py` draws the intro and test-pattern screens (296x128 PNGs) and `make_images.py`
packs them into `frames.h`; the counter on the white screens is drawn by the firmware itself.
The CC1110 has 32 KB of flash, so there is room for only a few full screens.
