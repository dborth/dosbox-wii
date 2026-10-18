# DOSBox Wii

[github.com/dborth/dosbox-wii](https://github.com/dborth/dosbox-wii) — GPL licensed

DOSBox Wii is a DOS emulator for the **Nintendo Wii** and **Wii U**. It is [DOSBox](https://www.dosbox.com/) (SVN r4301, with a PowerPC dynamic recompiler) running on the shared [`libgui`](https://github.com/dborth/libgui) UI and driver framework, with a Home menu for settings, controllers, drives and saved setups.

DOSBox Wii is homebrew — it isn't signed by Nintendo, so your console needs to be set up to run unsigned code first. If you haven't done that yet, jump to **[Installation](#installation)**; it links to a step-by-step guide for each console.

## Table of Contents

- [Nightly Builds](#nightly-builds)
- [Features](#features)
- [Installation](#installation)
  - [All Platforms: SD Card & Folder Layout](#all-platforms-sd-card--folder-layout)
  - [Wii](#wii)
  - [Wii U](#wii-u)
- [First Run](#first-run)
- [Files, Drives & dosbox.conf](#files-drives--dosboxconf)
  - [The DOSBox folder and C:](#the-dosbox-folder-and-c)
  - [Mounting other drives](#mounting-other-drives)
  - [dosbox.conf](#dosboxconf)
- [The Home Menu](#the-home-menu)
  - [Opening and closing it](#opening-and-closing-it)
  - [Home screen](#home-screen)
  - [Settings](#settings)
  - [Status](#status)
- [Controls](#controls)
  - [Supported devices](#supported-devices)
  - [Setting up a controller](#setting-up-a-controller)
  - [Keyboard & mouse](#keyboard--mouse)
  - [Setups: saving controls per game](#setups-saving-controls-per-game)
- [Performance](#performance)
- [Troubleshooting](#troubleshooting)
- [Known Limitations](#known-limitations)
- [Building from Source](#building-from-source)
- [Credits](#credits)
- [Links](#links)

---

## Nightly Builds

Every push builds automatically. Grab the latest continuous-integration build:

| Platform    | Status                            | Download                          |
|-------------|-----------------------------------|-----------------------------------|
| Wii / vWii  | [![Build Status][Build]][Actions] | [![Download][Download]][dosbox-wii]   |
| Wii U       | [![Build Status][Build]][Actions] | [![Download][Download]][dosbox-wiiu]  |

[Actions]: https://github.com/dborth/dosbox-wii/actions/workflows/build.yml
[Build]: https://github.com/dborth/dosbox-wii/actions/workflows/build.yml/badge.svg
[Download]: https://img.shields.io/badge/Download-blue
[dosbox-wii]: https://github.com/dborth/dosbox-wii/releases/download/Pre-release/DOSBox-Wii.zip
[dosbox-wiiu]: https://github.com/dborth/dosbox-wii/releases/download/Pre-release/DOSBox-WiiU.zip

> The Wii build also runs in **vWii**, the Wii U's built-in Wii mode. The Wii U build is a separate, **native** Wii U (Aroma) application — see [Wii U](#wii-u).

---

## Features

- DOSBox SVN r4301 core with the PowerPC **dynamic recompiler** (dynarec), two to four times faster than the normal core on the benchmark in [`docs/Benchmark_Wii_WiiU.txt`](docs/Benchmark_Wii_WiiU.txt)
- A Home menu you can open at any time: speed, frameskip, aspect and filter at a glance, plus settings pages for Performance, Video, Audio, Keyboard & Mouse, and the DOS joystick
- A **Status** page showing what DOSBox is actually doing (resolved CPU core, effective cycles, screen mode, audio rate, mounted drives, loaded config), with anything that differs from the default marked
- Controller setup with a drawing of your controller, press-to-assign buttons, and built-in presets for common kinds of DOS game
- **Setups**: save a game's controls and settings under a name and load them again later
- Wiimote, Wiimote + Nunchuk, Classic Controller, GameCube Controller and Wii U Pro Controller support, plus the **Wii U GamePad** on the native Wii U build
- USB keyboard and mouse, the Wiimote pointer as a mouse, and an on-screen keyboard
- Our own video output: corrected 4:3 aspect or square pixels, fit / integer / fill sizing, sharp / bilinear / nearest filtering, scanlines, zoom and shift, and 16:9 handling
- Mounts SD cards and USB drives as DOS drives, with hot-plug, from a **Drives** screen in the menu
- Saves your changes back to `dosbox.conf` from the menu, keeping the previous file as a `.bak`
- Open source

---

## Installation

### All Platforms: SD Card & Folder Layout

DOSBox Wii keeps its files in a folder named `DOSBox` at the root of a storage device. It looks on the SD card first, then USB drives 1 to 3, and uses the first one that already has a `DOSBox` folder. If none has one, it creates the folder on the first usable device. (The folder is found by looking, not by where the app was launched from.)

Format your SD card as **FAT32** — it is the most reliable, best-tested option and the one this guide assumes.

Put your DOS games inside the `DOSBox` folder; DOS sees that folder as drive **C:**:

```text
SD:/DOSBox/
├── dosbox.conf      ← created on first run; see "dosbox.conf" below
├── Setups/          ← saved controller setups (created when you save one)
└── ...              ← your DOS games and files: this is drive C:
```

### Wii

1. Follow the **[Wii Homebrew Guide](https://wii.hacks.guide/)** if you haven't already installed the Homebrew Channel. This is a one-time setup per console.
2. Download the Wii build (`DOSBox-Wii.zip` above) and extract it to the root of your SD card. This adds:
   - `apps/dosbox-wii/` — `boot.dol`, `icon.png` and `meta.xml`, which the Homebrew Channel launches.
   - `DOSBox/` — the (empty) folder for your games.
3. Copy your DOS games into `DOSBox/`.
4. Insert the SD card, open the **Homebrew Channel**, and launch **DOSBox Wii**.

```text
SD:/
├── apps/
│   └── dosbox-wii/
│       ├── boot.dol
│       ├── icon.png
│       └── meta.xml
└── DOSBox/
    └── ...
```

You will want a **Wii Remote** to start with. A USB keyboard makes DOS much easier to use; without one, use the [on-screen keyboard](#home-screen) and a [controller preset](#setting-up-a-controller).

The Wii build runs unchanged in **vWii** (launch it from the Homebrew Channel inside Wii mode on a Wii U). The Wii U GamePad is not usable there; use the native build below for GamePad support.

### Wii U

The native Wii U build is a dedicated Aroma application (`.wuhb`).

1. Follow the **[Wii U Homebrew Guide](https://wiiu.hacks.guide/)** to install **Aroma** if you haven't already. One-time setup per console.
2. Download the Wii U build (`DOSBox-WiiU.zip` above). Copy the `dosbox-wiiu` folder (it contains `dosbox-wiiu.wuhb`) into `wiiu/apps/` on your SD card, and create a `DOSBox/` folder at the SD card root for your games.
3. Insert the SD card and turn on your Wii U. With Aroma installed, **DOSBox Wii** appears as its own icon on the Wii U Menu.

```text
SD:/
├── wiiu/
│   └── apps/
│       └── dosbox-wiiu/
│           └── dosbox-wiiu.wuhb
└── DOSBox/
    └── ...
```

> ⚠️ **Run the latest Aroma.** This build is only tested against the current Aroma release. If something looks wrong, update Aroma first.

**USB drives need Mocha.** On the native Wii U build, USB storage (FAT32, exFAT and NTFS) is reached through **[Mocha](https://github.com/wiiu-env/MochaPayload)**, which comes with the standard Aroma setup. If Mocha isn't available, USB drives won't appear, but the SD card still works.

---

## First Run

On the first launch DOSBox Wii finds your `DOSBox` folder, creates a default `dosbox.conf` in it if there isn't one, mounts the folder as drive **C:**, and starts at the DOS prompt (`Z:\>`).

To get to your games:

```text
C:
cd mygame
game.exe
```

Open the [Home menu](#the-home-menu) at any time to change settings, set up your controller, or exit.

---

## Files, Drives & dosbox.conf

### The DOSBox folder and C:

Drive **C:** is always the `DOSBox` folder described [above](#all-platforms-sd-card--folder-layout). Drive **Z:** is DOSBox's own virtual drive, as on a PC.

### Mounting other drives

Nothing else is mounted automatically. Two ways to add a drive:

- **From the menu:** *Settings → Drives* lists the SD card and USB drives the console currently sees. Press **A** on one to mount it on the first free drive letter, and **A** again to unmount it. Drives plugged in or removed while the screen is open show up by themselves. A DOS drive whose device has been removed is dropped automatically. Unmounting only works at the DOS prompt, because a running program may have files open.
- **From DOS**, with the `MOUNT` command and a device prefix (`sd:/`, `usb:/`, `usb2:/`, `usb3:/`):

  ```text
  mount e usb:/
  e:
  dir
  ```

The Wii's DVD drive and network shares can't be mounted as DOS drives at this time.

### dosbox.conf

DOSBox reads `dosbox.conf` from the `DOSBox` folder (for example `sd:/DOSBox/dosbox.conf`). If it doesn't exist, a default one is generated on first run. It is the standard DOSBox format, with the usual `[cpu]`, `[sblaster]`, `[dos]`, `[autoexec]` and other sections, plus these additions on this build:

| Section | What it holds |
|---|---|
| `[display]` | Video output: `fit`, `aspect`, `filter`, `scanlines`, `zoomx`, `zoomy`, `shiftx`, `shifty`, `widescreen` |
| `[sdl]` | `sensitivity` (mouse speed) and `mapperfile` (the key/button mapper file). `autolock` is ignored: the mouse is always captured |
| `[render]` | `frameskip` only |

Edit it with a text editor on your PC, or change settings from the menu and press **Save** on the Settings page. Saving writes the running configuration back to the file it was loaded from and keeps the file it replaced as `dosbox.conf.bak`.

Some settings (machine type, memory size, mixer options) are only read when DOSBox starts. Change them in `dosbox.conf` and relaunch.

To start a game automatically and quit when it ends, use `[autoexec]`:

```text
[autoexec]
C:
cd games\sc2000
sc2000.exe
exit
```

---

## The Home Menu

### Opening and closing it

Open the menu at any time, including mid-game, with any of:

| Input | Action |
|---|---|
| **Home** (Wiimote, Classic, Wii U Pro, GamePad) | Opens the menu |
| **L + R + Plus** on one controller | Opens the menu. On a GameCube controller this is **L + R + Start**, so a GameCube-only setup can reach it |
| **Tap the Left or Right GUI key** on a USB keyboard (the Windows / Super / Command key) | Opens the menu |

Choose **Resume** to go back to the game. Choose **Exit** to quit DOSBox. From DOS you can also type `exit`, or press **Ctrl+F9** on a USB keyboard.

### Home screen

| Control | What it does |
|---|---|
| **Resume** | Back to the game. Shows the running program, or "DOS prompt", and the loaded setup |
| **Speed** | Steps the CPU cycles up and down, and shows which core is running (dynamic, normal or full) |
| **Frameskip** | More skipped frames makes heavy games faster, but choppier |
| **Aspect** | *Corrected* draws the picture 4:3 like a real monitor; *Square* keeps pixels square |
| **Filter** | *Sharp* keeps pixel edges crisp; *Smooth* blends them |
| **Controllers** | Opens controller setup; see [Controls](#controls) |
| **Keyboard** | An on-screen keyboard for typing into DOS |
| **Settings** | All options |
| **Status** | What DOSBox is set to and doing right now |
| **Setups** | Save or load the buttons and settings for this game |
| **Exit** / **Credits** | Quit DOSBox / who made this |

The home screen also shows the controllers the console sees, with a battery level for each wireless one, and lights up icons for a connected keyboard and mouse.

### Settings

| Page | Contents |
|---|---|
| **Performance** | CPU core, CPU type, cycles mode and amount, cycle step up/down, frameskip |
| **Video** | Aspect, fit, filter, scanlines, zoom width/height, shift X/Y, widescreen |
| **Audio** | Output rate, Sound Blaster type, OPL mode and rate, PC speaker, Tandy, Disney Sound Source, Gravis Ultrasound |
| **Keyboard & Mouse** | Mouse speed (X and Y) |
| **DOS joystick** | Joystick type, button wrap, dead zone, autofire, circular input, timed axes |
| **Drives** | Mount and unmount SD and USB drives; see [above](#mounting-other-drives) |
| **All settings** | Every other DOSBox option, one page per section (including the DOS keyboard layout under *DOS memory and keyboard*) |

The **Save** button on the Settings page writes your changes to `dosbox.conf`.

Not every setting can change while a program is running. Video, frameskip, cycles and CPU type apply immediately. **Audio, DOS joystick and the DOS memory settings apply only at the DOS prompt** (the row says so when it's unavailable). Settings that DOSBox reads only at startup are marked **(next launch)**.

### Status

*Status* lists what each setting is configured to and what is actually in effect, grouped as CPU, Video, Audio, Memory, Input, and Drives & config, with a `*` on anything that differs from the default. It is the quickest way to answer "is the dynarec on?" (*CPU → Dynarec: Active / Not active*) or "what rate is my audio at?".

---

## Controls

### Supported devices

- **Wii:** Wiimote, Wiimote + Nunchuk, Classic Controller, GameCube Controller, Wii U Pro Controller
- **Wii U (native):** the Wii U GamePad (buttons, sticks and touch screen), Wii U Pro Controller, and the Wii controllers above
- **USB keyboard and mouse**
- The Wiimote **pointer** works as a mouse (it isn't available while a Classic Controller is attached)

Each connected controller is a *player* (player 1 to 4). The console decides the order. A Wiimote and a GameCube controller on the same slot count as one player. On Wii U the GamePad is always player 1.

### Setting up a controller

Open **Controllers** from the home screen. The setup screen shows a drawing of the controller with what each control does in DOS beside it; controls light up as you press them.

1. **Emulating** — what DOS should see:

   | Mode | What it gives you |
   |---|---|
   | **Joystick** | One joystick with four buttons, for player 1 |
   | **Two joysticks** | Players 1 and 2 each get a joystick with two buttons |
   | **Keyboard + mouse** | No joystick; the buttons act as keys and the mouse |

2. **Preset** — a starting point for a kind of game:

   | Preset | Good for |
   |---|---|
   | **Compatible** | Buttons fire the DOS joystick, as DOSBox does on its own |
   | **Arrow keys** | D-pad and buttons as arrows, Ctrl, Alt and Space (platformers) |
   | **Arrows + shooter** | Arrows, with Ctrl to fire, Space to use, Alt to strafe (Doom-style) |
   | **Point and click** | A and B click, the D-pad moves between hot spots (adventures) |
   | **Menu keys** | D-pad, Enter and Esc, for installers and the DOS prompt |

3. **Change a single control** — select it, then press the key on your USB keyboard or click the mouse button you want, or pick from the list. A control can be a key, a joystick button, a mouse button, a DOSBox function (cycles up/down, frameskip up/down, swap disk image, hold-to-fast-forward), or nothing.

The **Home** button (or **L + R + Plus**) always opens the menu and can't be reassigned. Use **Reset** to return a player to its preset, and **Load** / **Save** for [setups](#setups-saving-controls-per-game).

### Keyboard & mouse

- **USB keyboard:** plug it in; it works at the DOS prompt and in games. The DOS keyboard layout is `[dos] keyboardlayout`, under *All settings → DOS memory and keyboard*.
- **On-screen keyboard:** *Home → Keyboard*.
- **Mouse:** the Wiimote pointer, a USB mouse, or (Wii U) the GamePad touch screen as a trackpad: tap to click, **ZR / ZL** for left / right buttons. The mouse is always captured, so the first click isn't used up capturing it. Speed is under *Settings → Keyboard & Mouse*.

### Setups: saving controls per game

A **setup** is a named set of controls and joystick settings for one game. From the setup screen choose **Save**, type a name on the on-screen keyboard, and optionally attach it to a program name such as `KEEN4E.EXE`. **Load** brings it back later, and *Home → Setups* does the same from the home screen.

A setup is two ordinary DOSBox files in `DOSBox/Setups/`: `name.conf` (which points at the map through `[sdl] mapperfile`) and `name.map` (DOSBox's own mapper file). You can copy them between consoles or edit them by hand.

---

## Performance

Most 386-era games run well; demanding 486 and Pentium-class games may not. If a game is slow:

1. Check the **dynarec** is on: *Status → CPU → Dynarec*. With `core=auto`, DOSBox runs the normal core until a program enters protected mode, then switches to the dynarec.
2. **Cycles:** the default `auto` adapts to the game. Try `max`, or a fixed number such as `fixed 30000`, under *Settings → Performance*. Step up and down from the home screen.
3. **Frameskip:** raise it from the home screen until the game runs smoothly enough.
4. **Turn the sound down:** set the game's sound to PC speaker or none, or turn off Sound Blaster / Gravis Ultrasound under *Settings → Audio*.
5. If it's still too slow: the Wii can only do so much!

---

## Troubleshooting

### A game closes DOSBox, crashes, or hangs

- **Delete `dosbox.conf`** (from the `DOSBox` folder) to restore the default settings. A new one is created on next launch.
- **Disable the sound.** Use the sound setup program that came with the game, and/or set *Sound Blaster* to `none` and *Gravis Ultrasound* to off.
- **Change some settings**, alone or in combination:
  - Fixed cycles (for example `fixed 10000`)
  - `memsize` in `[dosbox]` (edit `dosbox.conf`; read at startup)
  - `ems`, `xms` and `umb` off in `[dos]`
- **Run `loadfix`** before starting the game.
- **Try the normal core** instead of the dynarec (*Settings → Performance → CPU core*), then compare.

### A game runs slowly

See [Performance](#performance).

### I can't open the menu

Press **Home** on a Wiimote, Classic Controller, Wii U Pro Controller or GamePad; **L + R + Start** on a GameCube controller; or tap the **Windows / Super** key on a USB keyboard.

### My USB drive doesn't show up

Plugged-in drives appear on *Settings → Drives* by themselves, and must be mounted from there (or with `MOUNT`) before DOS can use them. On the native Wii U build, USB needs **Mocha** (see [Wii U](#wii-u)).

### A setting is greyed out or says "(next launch)"

Audio, DOS joystick and DOS memory settings apply only at the DOS prompt. Startup-only settings need an edit to `dosbox.conf` and a relaunch.

---

## Known Limitations

- DVD and network shares can't be mounted as DOS drives.
- No CD audio.
- No MIDI output: a game that selects MIDI will play silence. Use Sound Blaster FM music instead.
- Modem and IPX networking are not compiled in, and the serial port is a dummy.
- No in-place restart. Changes to startup-only settings take effect the next time you launch DOSBox Wii.

---

## Building from Source

DOSBox Wii builds for Wii and Wii U from this one repository, with devkitPro, `libogc2` (Wii) or `wut` (Wii U), and `libsmb2`. The Wii U build also needs `libmocha` and `libdvm`. The toolchain and dependencies are the same as for every project that uses libgui, so they are documented once in the **[libgui Building guide](https://github.com/dborth/libgui/blob/master/doc/building.md)**. Follow it through the dependency steps, then:

```sh
git clone https://github.com/dborth/dosbox-wii.git
cd dosbox-wii
make -f Makefile.wii -j3     # Wii    -> executables/dosbox-wii.dol
make -f Makefile.wiiu -j3    # Wii U  -> executables/dosbox-wiiu.wuhb
```

This repository's [`.github/workflows/build.yml`](.github/workflows/build.yml) is a working reference for the full build. The DOSBox core is unmodified r4301 apart from the files listed in [`docs/PORTING`](docs/PORTING).

---

## Credits

| Role | Credit |
|---|---|
| Porting & menu coding | Tantric (Daryl Borth) |
| PowerPC dynarec | jmarsh |
| DOSBox / dynarec updates | retro100 |
| DOSBox | The DOSBox Team |
| devkitPPC & libogc2 | shagkur, WinterMute & Extrems |

This software is open source and may be copied, distributed, or modified under the terms of the GNU General Public License (GPL) Version 2.

## Links

- [DOSBox Wii on GitHub](https://github.com/dborth/dosbox-wii)
- [DOSBox Wii on WiiBrew](https://wiibrew.org/wiki/DOSBox_Wii) — version history and the compatibility list
- [Wii Homebrew Guide](https://wii.hacks.guide/)
- [Wii U Homebrew Guide](https://wiiu.hacks.guide/)
- [Mocha](https://github.com/wiiu-env/MochaPayload) — USB storage access for the native Wii U build
- [libgui](https://github.com/dborth/libgui)
