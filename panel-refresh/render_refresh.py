"""Render the panel-refresh firmware's screens (296x128, black/white/red) for make_images.py:
an intro, a test pattern for after each block of cycles, and a final one.

    python render_refresh.py OUTDIR CYCLES TEST_EVERY

Frames: 0 the intro, 1 the test pattern (the firmware draws the cycle count after its title),
2 the final test pattern.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 296, 128
BLACK, RED, WHITE = (0, 0, 0), (255, 0, 0), (255, 255, 255)


def font(size):
    return ImageFont.load_default(size=size)


def intro(path, cycles, minutes):
    img = Image.new("RGB", (W, H), WHITE)
    d = ImageDraw.Draw(img)
    d.text((8, 4), "PANEL REFRESH", font=font(30), fill=RED)
    lines = [f"Cycling black, white and red {cycles} times,",
             f"about {minutes} minutes. Keep the power on.",
             "The white screens count down; a test",
             "pattern shows after each block of cycles.",
             "Reflash OEPL when it says done."]
    for i, line in enumerate(lines):
        d.text((8, 42 + 17 * i), line, font=font(13), fill=BLACK)
    img.save(path)


def test_pattern(path, title):
    img = Image.new("RGB", (W, H), WHITE)
    d = ImageDraw.Draw(img)
    d.text((6, 2), title, font=font(16), fill=BLACK)
    top, size = 26, 44
    # Solid black, solid red, then 1-pixel checkerboards in each, which show weak drive first
    d.rectangle((6, top, 6 + size - 1, top + size - 1), fill=BLACK)
    d.rectangle((56, top, 56 + size - 1, top + size - 1), fill=RED)
    for x in range(size):
        for y in range(size):
            if (x + y) % 2 == 0:
                img.putpixel((106 + x, top + y), BLACK)
                img.putpixel((156 + x, top + y), RED)
    # Fine lines: 1, 2 and 3 pixels wide
    for i, w in enumerate((1, 2, 3)):
        x = 210 + 26 * i
        d.rectangle((x, top, x + w - 1, top + size - 1), fill=BLACK)
        d.rectangle((x + 10, top, x + 10 + w - 1, top + size - 1), fill=RED)
    # Small text like the OEPL screens' second line, in black and red
    d.text((6, 76), "Small text: we'll try again in a little while", font=font(12), fill=BLACK)
    d.text((6, 92), "Small red text: Open EPaper Link 0123456789", font=font(12), fill=RED)
    d.text((6, 108), "Compare with a good tag in the same light.", font=font(12), fill=BLACK)
    img.save(path)


def main():
    out, cycles, every = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    os.makedirs(out, exist_ok=True)
    # Timed on a tag: a full refresh takes about 8.4 s; each cycle is 4 refreshes with 5 s rests,
    # and each test pattern stays up for a minute
    seconds = 30 + cycles * 4 * (8.4 + 5) + cycles // every * (8.4 + 60)
    intro(os.path.join(out, "0_intro.png"), cycles, round(seconds / 60))
    # One test pattern for every block (the tag draws its "20/40" beside the title), and a last one
    test_pattern(os.path.join(out, "1_test.png"), "PANEL TEST after")
    test_pattern(os.path.join(out, "2_done.png"), f"DONE: {cycles} cycles. Reflash OEPL.")

if __name__ == "__main__":
    main()
