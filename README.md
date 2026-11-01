# chroma-flasher

Hands-free [OpenEPaperLink](https://github.com/OpenEPaperLink/OpenEPaperLink) flashing for
**Chroma 29** electronic shelf labels (2.9", CC1110, board EDK286) with a **TI CC Debugger**,
built for flashing a batch of tags with hand-held probes and no jig.

While you hold the probes on the pads, `chroma-flash` drives the whole job and tells you out loud
what is happening, so your eyes and hands can stay on the tag:

0. **Reads the tag's serial from its label** with the computer's camera (barcode, or OCR of the
   printed serial), or you type it. You get 10 seconds to press `n` if the read is wrong.
1. **Waits for the chip.** No need to press the debugger's Reset button (see the cc-tool patch below).
2. **Backs up the stock firmware** if the chip allows it (factory tags are usually read-locked).
3. **Erases, writes and verifies**, retrying up to 8 times if a probe slips.
4. **Restarts the tag.**
5. **Reads the tag's MAC address back from RAM and checks the serial**:
   - matches the label: says so, nothing else to do;
   - missing (`4467090000000000`, the firmware couldn't read the factory serial): patches the
     label's serial into a copy of the image, flashes that, and checks again;
   - different from the label: stops and warns you.

A status board in the terminal shows each step, and every run is logged per serial
(`<SERIAL>_flash.log`).

**Bench guide:** [`docs/flashing-guide.html`](docs/flashing-guide.html) walks through one tag from
start to finish: the debugger socket and its pin 1, the power Y-lead, which wire colour goes on which
pad (with a photo of the rev B pads), opening the case, flashing, and troubleshooting. Download it
and open it in a browser.

**Once the tags run OpenEPaperLink**, [oepl-pc-ap](https://github.com/OssBozier/oepl-pc-ap) is a
small access point that runs on a PC with a CC1101 radio module, if you don't have an OEPL AP.

> Tested on one setup so far: Windows 11 (ARM64) with WSL Ubuntu 24.04, a TI CC Debugger through
> usbipd-win, and a dozen or so Chroma 29 tags (JA red and JF yellow). The plain-Linux and macOS
> paths are written but untested; reports welcome.

### How this differs from OpenEPaperLink's own tools

None of the usual ways of flashing a CC1110 Chroma tag check the result's serial:

| Tool | What it does for CC1110 Chroma tags |
|---|---|
| OEPL [Tag Flasher](https://github.com/OpenEPaperLink/OpenEPaperLink/tree/master/Tag_Flasher) (`OEPL-Flasher.py -c`) | Reads, writes and erases the flash. Its README notes the tag's EEPROM isn't accessible and autoflash isn't implemented for the CC1110; its MAC commands are for ZBS243 and nRF tags, which keep the MAC in an info page the CC1110 doesn't have |
| CC Debugger with cc-tool or SmartRF Flash Programmer | Writes the image. A lost serial only shows up later, as a tag with MAC `…0000000000` on the AP |
| `patch_sn.py` in [Tag_FW_Chroma](https://github.com/OpenEPaperLink/Tag_FW_Chroma) | The manual fix: read the label, patch the serial into an image, flash again. It refuses a serial whose letters differ from the image's (e.g. JF on the JA image) |

`chroma-flash` closes that loop in the same hold of the probes: it reads the MAC back from the
chip's RAM straight after the flash (with the `--read-xdata` cc-tool addition below), patches in
the label's serial, letters included, when the factory one was lost, reflashes and checks again,
and compares the result with the label it scanned. A tag that lost its serial is fixed at the
bench in one go, rather than found later on the AP and brought back for a second session.

The read-back needs to know where the firmware keeps `mSelfMac` in RAM, which each build decides.
`chroma-flash` finds it in the image it is about to flash, and failing that searches RAM for it, so a
new OEPL release, or another Chroma model, needs no update (see
[How the serial check works](#how-the-serial-check-works)).

## What you need

| | |
|---|---|
| Tag | Chroma 29, CC1110 (EDK286 board). Serials ending in **B** take `chroma29_full_*.bin`; serials ending in **C** have a UC8151 panel and take `chroma29_8151_full_*.bin` |
| Programmer | TI CC Debugger (USB `0451:16a2`) |
| Firmware | OpenEPaperLink's `binaries/Tag` folder (a clone of the OEPL repo) |
| Software | bash 4+, Python 3, a **patched cc-tool** (below); for the camera: [uv](https://docs.astral.sh/uv/) and ffmpeg |
| Speech (optional) | Windows PowerShell under WSL, `spd-say` or `espeak-ng` on Linux, `say` on macOS |

### Connections

| CC Debugger | Tag | Wire (default) |
|---|---|---|
| DD | TP3 | green |
| DC | TP5 | yellow |
| RESET | TP2 | blue |
| GND | battery negative | black |
| VDD (target voltage) | +VBAT (the solder blob by TP4, marked J2) | red |

The status board shows this wiring, and the "hold the probes" prompt speaks the three probe
colours. If your wires are different colours, set `WIRE_DD`, `WIRE_DC`, `WIRE_RESET`, `WIRE_VDD`
and `WIRE_GND` in the config file.

The debugger's VDD pin also **senses the tag's voltage** and its signals follow it, so connect it
even when a battery powers the tag. Check the TI CC Debugger user's guide for the pin numbers, and
never let a debugger supply and a coin cell fight each other: a lithium coin cell must not be charged.

**If every erase says "still locked after erasing" while the chip is found**, the probes are
touching but the erase isn't taking. Check the power and the VDD lead before anything else. One
such miss on the first attempt is common and the retry clears it.

## Install

### 1. cc-tool, with the patch

`chroma-flash` needs three small additions to cc-tool, in `cc-tool/cc-tool-chroma-flash.patch`
(against [dashesy/cc-tool](https://github.com/dashesy/cc-tool) `0d84df3`):

- **`CC_ASSUME_TARGET=1110`**: the CC Debugger only looks for a chip at power-up or when its Reset
  button is pressed, so with probes held by hand it usually reports "No target detected". With this
  set, cc-tool tries the real debug handshake anyway. `chroma-flash` sets it for you.
- **`--read-xdata ADDR:COUNT`**: reads RAM, used to read the MAC address after the flash.
- **`CC_CHECK_STATUS=1`**: only believe a chip whose debug status reads the same five times and isn't
  all zeros or all ones. Without it, a data line that is floating, or only half touching while a
  hand-held probe comes down, can pass as a debug-locked chip. `chroma-flash` sets it too, and also
  waits for three good answers in a row (`CHIP_ANSWERS`) before it says "chip found".

```sh
sudo apt install build-essential autoconf automake libtool pkg-config libusb-1.0-0-dev libboost-all-dev
git clone https://github.com/dashesy/cc-tool && cd cc-tool
git checkout 0d84df3
git apply /path/to/chroma-flasher/cc-tool/cc-tool-chroma-flash.patch
./bootstrap && ./configure && make
cp cc-tool ~/.local/bin/
```

### 2. These scripts

```sh
git clone https://github.com/OssBozier/chroma-flasher
cd chroma-flasher
ln -s "$PWD/chroma-flash" "$PWD/chroma-scan-sn" ~/.local/bin/   # add cc-attach too under WSL
mkdir -p ~/.config/chroma-flash && cp config.example ~/.config/chroma-flash/config
```

Then edit `~/.config/chroma-flash/config`. At least set `TAG_DIR` to OpenEPaperLink's
`binaries/Tag` folder.

### 3. USB access

- **Linux:** a udev rule, then replug the debugger:
  ```sh
  echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="0451", ATTR{idProduct}=="16a2", MODE="0666"' |
    sudo tee /etc/udev/rules.d/99-cc-debugger.rules && sudo udevadm control --reload
  ```
- **WSL:** install [usbipd-win](https://github.com/dorssel/usbipd-win), then once per USB port,
  in an admin PowerShell: `usbipd bind --busid <BUSID>` (`usbipd list` shows it). After that,
  `cc-attach` attaches the debugger and fixes the permissions; `chroma-flash` runs it for you when
  the debugger isn't reachable. This also works where TI has no Windows driver, such as Windows on ARM.

### 4. Camera (optional)

`chroma-scan-sn` reads the serial from the label. The first run takes a minute while uv fetches
its packages (zxing-cpp, RapidOCR). Under WSL it runs the **Windows** `ffmpeg.exe`, because WSL
can't see cameras; set `FFMPEG` if it isn't on the Windows PATH.

A laptop's built-in camera works better than a fixed-focus webcam: the barcode is only 3 cm wide,
and a webcam that can't focus at 10 cm gives a blur. Hold the label in the yellow box in the
preview window. A barcode read is accepted at once; an OCR read needs 3 matching reads.

The camera frames are processed in memory and never saved.

## Use

```sh
chroma-flash                      # read the label, then flash and check
chroma-flash JA10012345B          # serial known: skip the label
chroma-flash --no-backup          # skip the backup (quicker; factory tags are read-locked anyway)
chroma-flash --no-camera          # type the serial instead
chroma-flash --no-scan            # no label: the serial is only asked for if the tag turns out to have none
chroma-flash --no-check           # don't read the MAC afterwards
chroma-flash --image my.bin       # flash something else
chroma-flash --no-tts             # silent
chroma-flash --diag               # check contact, lock, firmware and MAC without changing anything
chroma-flash --help               # everything else
```

Every option can also go in `~/.config/chroma-flash/config` (see `config.example`) or the
environment; options win over both.

To get a fresh window for each tag from Windows, a Windows Terminal shortcut like this works:

```
wt.exe -w new --title "Chroma flash" wsl.exe -d Ubuntu-24.04 -- bash -lc "chroma-flash --no-backup || true && read -rp 'Press Enter to close'"
```

(Inside a `wt.exe` command line a `;` starts a new tab, hence `|| true &&`.)

## How the serial check works

OEPL builds the tag's MAC from its factory serial: `JF10012345B` becomes `4467090510012345`
(`44 67` fixed, then the two letters counted from A = 0, so `09 05` for JF, then the eight digits). When the firmware can't read the
factory serial, the MAC comes out as `4467090000000000` and the tag is hard to tell apart on the AP.

After the flash, `chroma-flash` reads the firmware's copy of the MAC (`mSelfMac`) from RAM with
`cc-tool --read-xdata`. Where that is depends on the build, so it is worked out each time:

1. **From the image.** The firmware sets the MAC's fixed top bytes with
   `mov dptr,#mSelfMac+7 / mov a,#0x44 / movx @dptr,a` (and the same for `+6` and `0x67`), and
   that instruction sequence appears exactly once in every OEPL Chroma image so far. Its address,
   less 7, is `mSelfMac`: `F950` in `chroma29_full_0015` and `chroma29_8151_full_0013`, `F956`/`F957`
   in the Chroma 42 images, `F990`/`F98E` in the Chroma 74 ones. For chroma29 that matches the
   build's `main.map`; the others are worked out the same way but not yet checked on a tag.
2. **From RAM**, if the image doesn't give it (or nothing is there): the first 3.75 KB of RAM is read
   and searched for the only 8 bytes that can be an OEPL MAC: `44 67`, two letters, eight BCD digits.
   One that matches the label's serial wins.
3. **Given** with `--mac-addr`, which skips both.

A read is only trusted when it shows the fixed `44 67` bytes. The 8151 image's MACs read `4467 29…`
where the other gives `4467 09…`. `--diag` works the address out from the flash it reads back.

To fix a missing serial, the serial (letters and digits) is patched into the default NVRAM block of a
copy of the image (after the bytes `56 12 09 85 01 08`), which is flashed instead. The firmware then
writes it back into the tag's factory block on first boot. This is the idea behind OEPL's
`patch_sn.py`, which refuses letters that differ from the image's (e.g. `JF` in the JA image).

## Files

| | |
|---|---|
| `chroma-flash` | the flasher |
| `chroma-scan-sn` | reads a serial from the label with the camera; prints it, or exits 1 |
| `cc-attach` | WSL only: attaches the debugger through usbipd-win and fixes its permissions |
| `config.example` | settings file template |
| `cc-tool/` | the cc-tool patch |
| `panel-refresh/` | firmware that cycles a tired panel through full refreshes, with test patterns |

## License

MIT for the scripts (see `LICENSE`). The cc-tool patch is under cc-tool's own license, GPL-2.0.
