# DOSBox Wii

[github.com/dborth/dosbox-wii](https://github.com/dborth/dosbox-wii) — GPL licensed

**DOSBox Wii** is a DOS emulator for the **Nintendo Wii** and **Wii U**. It is [DOSBox](https://www.dosbox.com/) (SVN r4301, with a PowerPC dynamic recompiler) running on the shared [`libgui`](https://github.com/dborth/libgui) UI and driver framework, with a Home menu for settings, controllers, drives and games.

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
  - [Other files DOSBox Wii creates](#other-files-dosbox-wii-creates)
- [The Home Menu](#the-home-menu)
  - [Opening and closing it](#opening-and-closing-it)
  - [Home screen](#home-screen)
  - [Settings](#settings)
  - [Status](#status)
  - [Games](#games)
  - [Wii and Wii U](#wii-and-wii-u)
- [Controls](#controls)
  - [Supported devices](#supported-devices)
  - [Setting up a controller](#setting-up-a-controller)
  - [Controls on each controller](#controls-on-each-controller)
  - [Keyboard & mouse](#keyboard--mouse)
  - [The D-pad and the joystick](#the-d-pad-and-the-joystick)
- [Performance](#performance)
- [Memory](#memory)
- [Troubleshooting](#troubleshooting)
- [Known Limitations](#known-limitations)
- [Building from Source](#building-from-source)
- [Credits](#credits)
- [Links](#links)

> 📜 Looking for version notes? They're in the **[CHANGELOG.md](CHANGELOG.md)**.

---

## Nightly Builds

Every push builds automatically. Grab the latest continuous-integration build:

| Platform    | Status                            | Download                          |
|-------------|-----------------------------------|-----------------------------------|
| Wii         | [![Build Status][Build]][Actions] | [![Download][Download]][dosbox-wii]   |
| Wii U       | [![Build Status][Build]][Actions] | [![Download][Download]][dosbox-wiiu]  |

[Actions]: https://github.com/dborth/dosbox-wii/actions/workflows/build.yml
[Build]: https://github.com/dborth/dosbox-wii/actions/workflows/build.yml/badge.svg
[Download]: https://img.shields.io/badge/Download-blue
[dosbox-wii]: https://github.com/dborth/dosbox-wii/releases/download/Pre-release/DOSBox-Wii.zip
[dosbox-wiiu]: https://github.com/dborth/dosbox-wii/releases/download/Pre-release/DOSBox-WiiU.zip

> The Wii build also runs unmodified in **vWii** (the Wii U's built-in Wii-compatibility mode), including via VC-style injection for GamePad support. The Wii U build is a separate, **native** Wii U (Aroma) application — see [Wii U](#wii-u) below for how the three options compare.

---

## Features

- DOSBox SVN r4301 core with the PowerPC **dynamic recompiler** (see [Performance](#performance))
- A Home menu you can open at any time (Home, or L + R + Plus, or a tap of a keyboard's Windows key): speed, frameskip, aspect and filter at a glance, plus settings pages for Performance, Video, Audio, Keyboard & Mouse and the DOS joystick, and an *All settings* editor for the rest
- A **Status** page showing what DOSBox is configured to and what is in effect (running CPU core and whether the dynarec is active, cycles, screen mode, audio output, mounted drives, loaded config file), with anything not at DOSBox's default marked
- Controller setup with a drawing of each controller, live highlighting of the buttons you press, a chooser for keys, joystick buttons, mouse buttons and DOSBox functions, and five built-in presets for common kinds of DOS game
- **Games**: save a game's controls, settings and startup under a name, then switch to it from the menu without leaving the app. Games you've put on the card by hand are found too
- Wiimote, Wiimote + Nunchuk, Classic Controller and Wii U Pro Controller support, the GameCube Controller on the Wii build, and the **Wii U GamePad**: full touch and buttons on the native Wii U build, buttons and sticks (no touch) on vWii via VC-style injection
- USB keyboard and mouse, the Wiimote pointer as a mouse, and an on-screen keyboard
- Video output adjustments: original (4:3) or uncorrected (square-pixel) aspect, fit / integer / fill sizing, sharp / bilinear / nearest filtering, scanlines, zoom and shift, and a widescreen setting on the Wii
- Mounts SD cards and USB drives as DOS drives from a **Drives** screen in the menu, which also picks up drives plugged in or removed while it is open
- Saves your settings back to `dosbox.conf` from the menu (*Settings → Save*), keeping the previous version as `dosbox.conf.bak`
- Open source

---

## Installation

### All Platforms: SD Card & Folder Layout

DOSBox Wii keeps its files, and your DOS games, in a folder named `DOSBox`. When it starts it looks at the SD card, then USB drives 1, 2 and 3, in that order, and uses the first one that **already has** a `DOSBox` folder (capital letters don't matter on FAT). If none has one, it creates `DOSBox` on the first device that lets it. Where the app was launched from makes no difference.

- On the **Wii**, a `DOSBox` folder on a USB drive is used in preference to an SD card that has none.
- On the **Wii U**, USB drives are not mounted yet when this check runs (they are mounted from the *Drives* screen), so the folder is always on the SD card.

Format your SD card and USB drives as **FAT32**. It is what the Wii build recommends and the one this guide assumes (the Wii U build can also read exFAT and NTFS USB drives).

DOS sees the `DOSBox` folder as drive **C:**, so put your games inside it. Everything else in the folder is made by DOSBox Wii as it is needed (see [Other files](#other-files-dosbox-wii-creates)):

```text
<device>/DOSBox/                 ← drive C:
├── dosbox.conf                  ← first run, when no config file is found
├── dosbox.conf.bak              ← after Settings → Save over an existing file
├── mapper.map         ← after you change a controller button
├── Games/                       ← after you save a game or add one found on the card
├── capture/                     ← after a wave recording (Ctrl+F6, USB keyboard)
└── ...                          ← your DOS games and files
```

`<device>` is `sd:` on the Wii and the SD card's root (`/vol/external01/`) on the Wii U.

### Wii

1. Follow the **[Wii Homebrew Guide](https://wii.hacks.guide/)** if you haven't already installed the Homebrew Channel. This is a one-time setup per console.
2. Download the Wii build (`DOSBox-Wii.zip` above) and extract it to the root of your SD card. The zip holds:
   - `apps/dosbox-wii/` — `boot.dol` (the program), `icon.png` and `meta.xml` (the name, icon and description the Homebrew Channel shows).
   - `DOSBox/` — an empty folder for your games. (DOSBox Wii makes it itself on first run if it isn't there; see [above](#all-platforms-sd-card--folder-layout).)
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

The app is the single `boot.dol`; its font, language text and images are built into it.

When it starts, the Wii build looks at the IOS it is running on. If that isn't IOS 58 or 61, and the loader's preferred IOS is one of those, it reloads into it. This mostly matters when you launch it another way, such as a forwarder channel set to a different IOS. USB drives also work on other IOS versions, but IOS 58 has the best compatibility.

Running it in vWii: see [Wii U](#wii-u).

To start, a USB keyboard or any supported controller will do. DOSBox Wii opens at the DOS prompt, and the [menu](#opening-and-closing-it) opens with **Home** (or **L + R + Plus**) and has an on-screen keyboard; see [Controls](#controls) for the controllers and their buttons.

### Wii U

Wii U support comes in **three genuinely different forms**. Pick the one that matches what you've set up on your console:

|              | vWii (Homebrew Channel) | vWii (VC-style injection) | Native Wii U build |
|--------------|-------------------------|---------------------------|--------------------|
| What it is   | The regular **Wii** build, run inside vWii | The same Wii build, launched as its own injected Virtual-Console-style channel | A dedicated Wii U (Aroma) app, `.wuhb` |
| Requires     | Homebrew Channel *inside vWii* | Homebrew Channel *inside vWii*, plus a channel built with **[TeconMoon's WiiVC Injector Mod](https://github.com/Rodrigo-Matsuura/MoonWiiVCInjector/releases)** | **Aroma** (Wii U homebrew environment) |
| GamePad      | Not usable | Usable as **player 1** (buttons and sticks; no touch) | **Full support**: touch, buttons |
| Which download | `DOSBox-Wii.zip` (Wii build) | `DOSBox-Wii.zip` (Wii build) | `DOSBox-WiiU.zip` |

If you're not sure which you want: the **native build** is the strongest experience on a console with Aroma installed, with full GamePad support including the touch screen. **VC injection** is the best you'll get out of vWii itself (a usable GamePad), and plain **Homebrew Channel vWii** is the simplest but weakest of the three.

#### Native Wii U (Aroma)

1. Follow the **[Wii U Homebrew Guide](https://wiiu.hacks.guide/)** to install **Aroma** if you haven't already. One-time setup per console.
2. Download the Wii U build (`DOSBox-WiiU.zip` above) and copy `dosbox-wiiu.wuhb` to `wiiu/apps/` on your SD card, alongside your other Aroma apps.
3. Insert the SD card and turn on your Wii U. With Aroma installed, **DOSBox Wii U shows up as its own icon directly on the Wii U Menu**, right alongside your other software.

```text
SD:/
├── wiiu/
│   └── apps/
│       └── dosbox-wiiu.wuhb
└── DOSBox/
    └── ...
```

The zip does not contain a `DOSBox/` folder. DOSBox Wii creates it at the SD card root on first run, or you can make it yourself and fill it first.

> ⚠️ **Run the latest Aroma.** This app is only tested against, and only intended to work on, whatever the current Aroma release is at the time you're reading this. We can't promise it'll behave, or even boot, on an old Aroma build or an outdated Wii U system version. If something looks wrong, updating Aroma first is the right move before reporting it.

**You need an SD card in the console.** USB drives are not mounted when DOSBox Wii starts, so the `DOSBox` folder, your config and your games are on the SD card. USB drives are for extra DOS drives, mounted from *Settings → Drives* (see [Mounting other drives](#mounting-other-drives)).

**Recommended companions, installed through the same Wii U Homebrew Guide:**

- **[Mocha](https://github.com/wiiu-env/MochaPayload)**: an Aroma component that gives the console access to USB storage (FAT32, exFAT and NTFS). Without it, USB drives are not available at all (they won't show on *Settings → Drives*); the SD card works either way.
- **[Bloopair](https://github.com/GaryOderNichts/Bloopair)**: lets you pair non-Nintendo Bluetooth controllers (Switch Pro Controller, Joy-Con, DualShock/DualSense, Xbox controllers and others) to your Wii U as if they were a Wii U Pro Controller. Handy if you don't have a GamePad or Pro Controller. It works at the system level within the native Wii U environment and doesn't apply inside vWii.

#### vWii (Wii Homebrew Channel, inside Wii U)

1. Follow the **[Wii Homebrew Guide](https://wii.hacks.guide/)** to install the Homebrew Channel in vWii. The process runs from inside the Wii U's Wii mode and is otherwise the same as on a standalone Wii.
2. Follow the [Wii instructions](#wii) above exactly, using the same SD card. The vWii build is the Wii build. SD cards and USB drives work in vWii as on a Wii.
3. Boot into vWii on your Wii U (from the Wii U Menu) and launch it from the Homebrew Channel, same as on Wii.

This is the simplest Wii U path, but also the most limited: no GamePad support. For a GamePad without going all the way to the native build, see VC-style injection below.

#### vWii via VC-style injection (GamePad support)

Rather than launching DOSBox Wii from the Homebrew Channel every time, you can package it as its **own injected channel** using **[TeconMoon's WiiVC Injector Mod](https://github.com/timefox/TeconMoon-s-WiiVC-Injector-Mod)**. https://github.com/Rodrigo-Matsuura/MoonWiiVCInjector/releases vWii NAND rather than something launched through the Homebrew Channel, which is what lets you use the Wii U GamePad for display (without touch) and as player 1.

At a high level:

1. Install the Homebrew Channel in vWii first (see [above](#vwii-wii-homebrew-channel-inside-wii-u)). You'll still want it for updates and other homebrew.
2. Download and run **TeconMoon's WiiVC Injector Mod** on a PC, and choose **Wii Homebrew Injection (DOL)** as the injection type.
3. Point it at DOSBox Wii's `boot.dol` (from the Wii build), and pick one of the available **GamePad Emulation** modes so the injector configures GamePad input for the resulting channel.
4. Build the injected package and install it to your Wii U's vWii NAND with the tool of your choice (the injector's own documentation covers this step, since it depends on your existing vWii setup).
5. Keep the `DOSBox/` folder on your SD card exactly as described [above](#all-platforms-sd-card--folder-layout). The injected channel reads from the SD card the same way the Homebrew Channel version does.

Consult the injector's own documentation for anything version-specific: it is third-party software this README doesn't track closely.

---

## First Run

DOSBox Wii starts like this, in order:

1. The console is set up: video, audio, controllers and storage.
2. It finds the `DOSBox` folder (see [above](#all-platforms-sd-card--folder-layout)), making one if none exists.
3. It reads a config file (see [Where DOSBox looks for dosbox.conf](#where-dosbox-looks-for-dosboxconf)). If it finds none, it writes a default `dosbox.conf` into the `DOSBox` folder and reads that.
4. It mounts the `DOSBox` folder as drive **C:**.
5. DOSBox starts. There is no splash screen or menu first: the first thing on screen is DOSBox's own welcome banner, then any `[autoexec]` commands, then the DOS prompt (`Z:\>`).

So the second launch looks like the first, except that the `dosbox.conf` it reads is the one the first launch wrote.

To get to your games:

```text
C:
cd mygame
game.exe
```

If no storage device has a `DOSBox` folder or lets DOSBox Wii create one, DOSBox still starts, on its built-in settings, with no drive C:. Nothing is shown when it starts; open **Status** (see [Status](#status)): *Config file* reads `None (defaults)` and *Drives* reads `None mounted`.

Open the [Home menu](#opening-and-closing-it) at any time (press **Home**) to change settings, set up your controller, or exit.

---

## Files, Drives & dosbox.conf

### The DOSBox folder and C:

Drive **C:** is always the `DOSBox` folder found at startup (see [above](#all-platforms-sd-card--folder-layout)). It is mounted just before DOS starts, as a read-write hard disk. Whatever the card's real size, DOS reports about 1 GB in total with about 250 MB free, and the volume label is `C_DRIVE`. If there is no `DOSBox` folder, there is no C:.

Drive **Z:** is DOSBox's own virtual drive, as on a PC.

Because C: exists before `[autoexec]` runs, an `[autoexec]` that mounts something else on `c` has to free the letter first: `mount -u c`.

### Mounting other drives

Nothing else is mounted for you. There are two ways to add a drive.

#### From the menu: Settings → Drives

The screen has one row for each storage device, then a last row, **DOS drives**, listing every DOS drive and where it comes from (read-only; its help line scrolls).

| Console | Rows |
|---|---|
| Wii | Always four: **SD Card**, **USB Mass Storage**, **USB Mass Storage 2**, **USB Mass Storage 3** |
| Wii U | **SD Card** always; **USB Storage 1**, **2**, **3** only while that drive is mounted |

What a row's value says, and what **A** does:

| Value | Meaning | **A** |
|---|---|---|
| `Not available` | Nothing readable is in that slot (on the Wii, this includes a drive in a format it can't mount) | Says "That drive is not available." |
| `Not mounted` | The console can read it, and it isn't a DOS drive yet | Mounts it on the **first free letter from D to Y**, using the volume label (or `D_DRIVE`, `E_DRIVE`... if it has none) |
| `D:  label` | Mounted as a DOS drive from this screen | Unmounts it. **Only at the DOS prompt** |

A mounted row is the **whole device**, not the `DOSBox` folder: with the SD card on `D:`, your games are in `D:\DOSBox\`. You can mount while a program is running; you can't unmount then. A device is not ejected by unmounting it: only the DOS drive goes.

If **A** can't do it, a message says why:

| Message | Cause |
|---|---|
| That drive is not available. | The row's value is `Not available` |
| Unmount at the DOS prompt, not while a program is running. | Unmounting needs the DOS prompt |
| Could not unmount the drive. | DOS refused to remove the drive |
| Too many drives are mounted. | Eight drives mounted from this screen already |
| No drive letter is free. | D to Y are all in use |
| Could not mount the drive. | DOS refused to mount it |

**Hot-plug works only while this screen is open.** A background check runs about once a second while it is up (the rows refresh about twice a second), and at no other time. So:

- On the **Wii**, SD and USB drives that are present at launch are already readable. A drive plugged in later becomes readable, and appears here, once you open this screen.
- On the **Wii U**, USB drives are never mounted until this screen is open, including drives plugged in before launch. Open it and wait a moment for the row to appear.
- When a device is removed, a DOS drive that this screen mounted from it is taken away, but only while the screen is open and DOS is at the prompt. Otherwise it stays until you next open the screen at the prompt.

Drives you mount with `MOUNT` aren't tracked here: their row still says `Not mounted`, and pressing **A** would mount the same device again on another letter.

#### From DOS: MOUNT

`MOUNT` takes any folder the console can open, so use the device's prefix. The device has to be readable first (see above):

| Device | Wii | Wii U |
|---|---|---|
| SD card | `sd:/` | `/vol/external01/` (no `sd:` prefix) |
| USB drive 1 | `usb:/` | `usb1:/` |
| USB drive 2 | `usb2:/` | `usb2:/` |
| USB drive 3 | `usb3:/` | `usb3:/` |

```text
mount e usb:/
e:
dir
```

A subfolder works too (`mount e usb:/games`), and `mount -u e` removes the drive again. A folder that doesn't exist gives `Directory ... doesn't exist.` DOSBox Wii has no way to connect a network share or read a disc, so `dvd:/` and `smb:/` are never there to mount.

### dosbox.conf

#### Where DOSBox looks for dosbox.conf

Normally one file is read, the first of these that exists:

1. `dosbox.conf` in the program's working directory. DOSBox Wii doesn't set the working directory itself, so which folder this is depends on the launcher.
2. `dosbox.conf` in the `DOSBox` folder (for example `sd:/DOSBox/dosbox.conf`).

If neither exists, the default file described below is written to the `DOSBox` folder and read. DOSBox also understands command-line options such as `-conf`, but neither launcher entry in this repository passes any. To be safe, keep one `dosbox.conf`, in the `DOSBox` folder.

**Restart and game configs.** `CONFIG -r` at the DOS prompt restarts DOSBox with the options it started with, or with the ones you add. `CONFIG -r -gameconf name.conf` reads `name.conf` **after** the base config, so it only has to hold what differs and its values win. It is looked up as given, then in the `DOSBox` folder. The home screen's **Restart** keeps the game config, and `CONFIG -r -nogameconf` drops it. The **Games** screen chooses one from the menu; see [Games](#games).

Whichever file was read is the one **Settings → Save** writes back to, with the previous version kept as `dosbox.conf.bak` (see [Saving](#saving)).

Paths inside a config file that start with a device prefix (`sd:/...`, `usb:/...`) are used as written. Any other path, including a Wii U SD path starting `/vol/external01/`, is taken as relative to the folder of the config file. On the Wii U, write a bare file name for `mapperfile`.

#### What is in the default file

The default file holds every setting DOSBox Wii has, each with DOSBox's own comment above it, in this order:

| Section | What it holds |
|---|---|
| `[sdl]` | Mouse speed and the mapper file (below) |
| `[display]` | Video output (below) |
| `[dosbox]` | `language`, `machine` (default `svga_s3`), `captures`, `memsize` (default 16 MB; the Wii takes at most 16, the Wii U up to 63) |
| `[render]` | `frameskip` |
| `[cpu]` | CPU core, type and cycles |
| `[mixer]` | Mixer options |
| `[sblaster]`, `[gus]`, `[speaker]` | Sound Blaster and OPL, Gravis Ultrasound, PC speaker, Tandy and Disney |
| `[joystick]` | DOS joystick options |
| `[dos]` | `xms`, `ems`, `umb`, `keyboardlayout` |
| `[autoexec]` | Commands run at startup |

This build has no `[midi]`, `[serial]`, `[ipx]`, `[pci]` or `[debug]` section.

A conf from a PC build of DOSBox still loads. A section or setting this build doesn't have is ignored without a message: the line is read, matches nothing, and is dropped the next time you **Save**. (The lines under an unknown section header are read as part of the section above it, where a setting name that section doesn't have is ignored too.) A number outside its range is set to the nearest limit; a text value that isn't one of the listed choices falls back to the default.

#### Settings this build adds or changes

**`[display]`**: how the picture is drawn. These apply at once when changed from the menu, and are read at startup from the file.

| Setting | Values | Default |
|---|---|---|
| `fit` | `fit` (as large as fits, keeping its shape), `integer` (the largest whole-number multiple), `fill` (stretched over the screen) | `fit` |
| `aspect` | `corrected` (shape of a real monitor: 320x200 fills a 4:3 screen), `square` | `corrected` |
| `filter` | `sharp`, `bilinear`, `nearest` | `bilinear` |
| `scanlines` | 0 to 100 (percent; 0 is off) | 0 |
| `zoomx`, `zoomy` | 50 to 200 (percent of normal size; used by `fit` and `fill`) | 100 |
| `shiftx`, `shifty` | -100 to 100 (units of 1/640 of the screen width, and 1/480 of its height; negative is left or up) | 0 |
| `widescreen` | `auto` (follow the console's setting), `4x3`, `16x9` | `auto` |

`widescreen` has an effect on the Wii only, and `scanlines` only in 480-line modes on the Wii; see [Wii and Wii U](#wii-and-wii-u).

**`[sdl]`**

| Setting | Meaning | Default |
|---|---|---|
| `sensitivity` | Mouse speed in percent: `x,y`, each -1000 to 1000 (for example `100,-50`; the second number is the vertical speed and is optional) | `100` |
| `mapperfile` | The file the controller buttons are saved in. Read once at startup | `mapper.map` |
| `autolock` | **Ignored.** The mouse is always captured | `true` |

**`[render]`** has `frameskip` only (0 to 10, default 0). An old `scaler`, `forced` or `aspect` line in `[render]` is ignored; the aspect setting is now `[display] aspect`.

Everything else in `[cpu]`, `[sblaster]`, `[dos]` and the other sections is standard DOSBox r4301: see the comments in the file.

### Other files DOSBox Wii creates

| File or folder | Where | Made when |
|---|---|---|
| `dosbox.conf` | `DOSBox` folder | First launch with no config file found |
| `dosbox.conf.bak` | Beside the config file | **Settings → Save** over an existing file. Each save replaces the older `.bak`, so it is always the version before the last save |
| `dosbox.conf.tmp` | Beside the config file | During a save, and removed when it finishes. It is left behind only if a save was cut short, and removed by the next save |
| `mapper.map` | Folder of the config file | You change a controller button |
| `Games/` (`name.conf`, `name.map`) | `DOSBox` folder | The first time you save a [game](#games) or add one found on the card |
| `capture/` | Folder of the config file (`[dosbox] captures`) | The first wave recording (**Ctrl+F6** on a USB keyboard) or raw capture. Screenshots are not built into this build |

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

The Home menu is a full-screen overlay on the running game. While it is up DOSBox is stopped, the last picture is dimmed behind it, and the game's sound is replaced by the menu's; when you close it DOSBox carries on where it left off. It can only be opened once DOSBox is running.

### Opening and closing it

#### Opening

Any of the four players can open it, from any of these:

| Input | Notes |
|---|---|
| **Home** on a Wiimote, Classic Controller, Wii U Pro Controller or Wii U GamePad | Opens the menu. A GameCube controller has no Home button |
| **L + R + Plus**, all three held on **one device** | **Start** on a GameCube controller. Works on a GameCube controller, Classic Controller, Wii U Pro Controller and GamePad. The three buttons must be on the same device: a Wiimote with a Nunchuk cannot make the combo (the Wiimote has no L or R, and the Nunchuk has only C, which counts as L), so use **Home** there. On a Classic Controller on the Wii build, L and R count when pressed fully down |
| **Tap a GUI key** (Windows / Super / Command, left or right) on a USB keyboard | Press and release it with no other key pressed in between. GUI + another key does not count. DOS never sees the key. Ignored while the Wii U GamePad keyboard is open |

Nothing else opens it. (On the Wii U GamePad the **TV** button opens an on-screen keyboard on the GamePad screen, not the menu; see [Wii and Wii U](#wii-and-wii-u).)

#### Closing

| Action | What it does |
|---|---|
| **Resume** (top right) | Slides the menu away and returns to the game. Any key, mouse button or joystick button your controller buttons were holding is let go, and a button still held does nothing until you release it |
| **Home**, on any controller | The same as Resume, **on the home screen only**. Inside Settings, Status, Games, the controller screens or a popover, Home does nothing: go **Back** first. There is no Home on a GameCube controller, so use **Resume** |
| **B** (or **Back**) | On Settings, Status, Drives and the pages under them, goes up one level. On the home screen it does nothing |
| **L + R + Plus**, GUI key | Open the menu, but do not close it |
| **Keyboard** tile | After **OK** with some text typed, the menu closes and the text is typed into DOS. **Cancel**, or an empty box, stays in the menu |
| **Exit** (bottom left) | Fades out and leaves DOSBox at once. There is **no confirmation and nothing is saved** (see [Saving](#saving)). Wii: returns to the Homebrew Channel (or whatever launched it), or turns the console off if a power-off was requested. Wii U: returns to the Wii U Menu |
| **Restart** (beside Exit) | Closes DOSBox and starts it again from the config file, as if you had launched it again, without going back to the launcher. **Whatever is running is lost.** Settings you changed but did not **Save** are lost too, and settings DOSBox reads only at startup (**(next launch)**) now take effect. A game config, whether chosen in **Games** or given with `CONFIG -r -gameconf` (see [Where DOSBox looks for dosbox.conf](#where-dosbox-looks-for-dosboxconf)), is kept |
| Power button / system shutdown request | Honoured whether the menu is open or not, and ends DOSBox the same way as **Exit** |
| `exit` at the DOS prompt, **Ctrl+F9** on a USB keyboard | Also end DOSBox, without the menu. Ctrl+F9 is DOSBox's `shutdown` hotkey and is not offered for controller buttons |

### Home screen

The top bar says what is running. A **Game** card runs across the top. Under it the left card holds four quick settings and the right card the controllers and three tiles, and the bottom bar has **Exit**, a line saying what the highlighted thing does, and **Credits**.

| Control | What it does |
|---|---|
| **Program** (top, next to the logo) | The DOS program running now, by its file name as DOS recorded it when it started (for example `KEEN4E.EXE`), or **DOS prompt** |
| **Game** (under it) | `Game: name`, the file name of the game config DOSBox was started with, or `No game`. It follows a restart into a game and `CONFIG -r -gameconf`; see [Games](#games) |
| **Resume** | Back to the game |
| **Speed** `−` `value` `+` | The running CPU speed, changed at once. The line under it names the core that is running (*Dynamic core*, *Normal core* or *Full core*; *CPU speed* if that cannot be told). Details below |
| **Frameskip** `−` `value` `+` | The number of frames skipped, **0 to 10**, one per press; it stops at both ends. More skipping makes heavy games faster but choppier |
| **Aspect** `Corrected` `Square` | Sets *Settings → Video → Aspect*. Corrected draws the picture in the shape a monitor gave it (320x200 fills a 4:3 screen); Square keeps every pixel square |
| **Filter** `Sharp` `Smooth` | Sets *Settings → Video → Filter*: **Sharp** writes `sharp`, **Smooth** writes `bilinear`. *Sharp* also shows as lit when the filter is `nearest`; that third value can only be chosen on the Video page |
| **Controllers** card | Four slots, **P1** to **P4**, each with an icon for the device and a battery gauge (four bars; a GameCube controller shows none; every Wiimote-based controller shows its Wiimote's charge). Press a connected slot to set up that player. An empty slot is dimmed and cannot be selected. **Configure** opens controller setup (the first connected player). Under a rule: the Wii U GamePad (icon and battery; only on the Wii U build, and only if it was present when the menu opened), a keyboard icon and a mouse icon, lit while a USB keyboard / mouse is connected. These three are for display only. See [Controls](#controls) |
| **Keyboard** tile | An on-screen keyboard with an **Enter** key. The text is typed into DOS, as a US keyboard, after the menu closes; characters with no key on a US keyboard are skipped, and the box holds at most 255 characters at a time |
| **Settings** tile | The settings pages; see [Settings](#settings) |
| **Status** tile | What DOSBox is set to and doing; see [Status](#status) |
| **Game** card | The game DOSBox was started with, or **Plain DOSBox**. **Games** opens the list of games to load, and **Save** (**Save as game** at the plain prompt) saves what is running. What a save did is shown on the bottom line. See [Games](#games) |
| **Exit** / **Credits** | Quit DOSBox / who made this, with this console's details, the free memory, and `JIT: Enabled` when the dynamic core is the one running (`Disabled` for another core, `Unknown` if that cannot be told). Any button closes the credits |

**Speed** is a share of the console while DOSBox is in *max* mode, or in *auto* mode once a game has taken it there, and is shown as a percentage (`100%`). Otherwise it is a number of cycles. `−` and `+` work like this:

| Mode | Each press |
|---|---|
| Percentage (max) | 5 points down or up, from 1% to 105% |
| Cycles (fixed) | Uses *Settings → Performance → Cycle step down / up*. A step **below 100** is a percentage (up multiplies by 1 + step/100, down divides by it); **100 or more** is a number of cycles |

Speed and Frameskip change the running values, and the same numbers are copied into the configuration when the menu opens and after each press, so the Settings pages show them and **Save** writes them (a speed in *auto* mode is not copied: the setting stays `auto`). **Aspect** and **Filter** are the Video page's own rows, not copies.

### Settings

**Settings** lists the pages below. **A** opens a page and **B** or **Back** returns. **Save** (next to Back) is on this list only; see [Saving](#saving).

| Page | What is on it |
|---|---|
| **Performance** | CPU core, CPU type, cycles mode and amount, cycle step up / down, frameskip |
| **Video** | Aspect, fit, filter, scanlines, zoom width / height, shift X / Y, widescreen |
| **Audio** | Output (read-only), Sound Blaster, OPL mode and rate, PC speaker, Tandy, Disney, Gravis Ultrasound |
| **Keyboard & Mouse** | Mouse speed X and Y |
| **DOS joystick** | Joystick type, button wrap, dead zone, autofire, circular input, swap axes 3 and 4, timed axes |
| **Drives** | Mount and unmount SD and USB drives; see [Mounting other drives](#mounting-other-drives) |
| **All settings** | One page for each DOSBox config section, covering the rest |

On a page, **A** moves the highlighted row to its next value (wrapping round), and **Left** / **Right** step it back and forth. A change is applied to DOSBox at once. The line at the bottom shows the help for the highlighted row.

#### When a row can be changed

Each row is one of two kinds, and says so in its value:

| Kind | Behaviour |
|---|---|
| **Live** | Changes at any time the menu is open |
| **DOS prompt only** | Re-starts that piece of emulated hardware, so it works only when DOSBox is at the DOS prompt and not inside a program. Otherwise the value ends in **(DOS prompt only)**, the row does not move, and its help starts with "Only available at the DOS prompt." |

Other endings you may see on a value:

| Ending | Meaning |
|---|---|
| **(not used)** | The row has no effect at the moment: the zoom rows with *Fit* set to `integer`, and the DOS joystick rows (all but the type) while the joystick type is `none` |
| **(next launch)** | DOSBox only reads it at startup; the row cannot be changed from the menu. Edit `dosbox.conf`, then press **Restart** (or run `CONFIG -r`) |
| **(edit dosbox.conf)**, **(Performance page)**, **(Keyboard & Mouse page)** | Cannot be changed on an *All settings* page (free text, paths, and two-part values); the words say where, or how |
| **(unavailable)** | A DOS memory row that cannot be changed safely right now; the help line says why |
| **(not available here)** | An *All settings* row for something this console cannot do (the Wii U has no *Widescreen* setting) |
| *name* in brackets after the value, e.g. `auto (4axis)` | What the setting comes to in effect, when that is something else (only on *Joystick type*) |

#### Performance

| Row | Values | When |
|---|---|---|
| **CPU core** | `auto`, `dynamic`, `normal` | DOS prompt only |
| **CPU type** | `auto`, `386`, `386_slow`, `486_slow`, `pentium_slow` | Live |
| **Cycles mode** | Auto, Fixed, Max. Changing to Fixed starts from the cycle count now running, or 3000 if the speed is a percentage | Live |
| **Cycles amount** | Fixed: 500, 1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 15000, 20000, 25000, 30000, 40000, 50000 cycles. Max: 10% to 100% in tens. Auto: `-` (no amount) | Live |
| **Cycle step up** / **down** | 5, 10, 20, 25, 50, 100, 250, 500, 1000, 2000, 5000 (below 100 is a percentage, otherwise cycles). Used by the home screen's `+` / `−` in fixed mode only | Live |
| **Frameskip** | 0 to 10. Shows the running value | Live |

Changing any CPU row makes DOSBox read the cycles from the configuration again. Because the running fixed or max speed is copied into the configuration first, it stays as it was; in *auto* mode the speed goes back to what auto starts from.

#### Video

These are the `[display]` settings, read by the picture on its next frame, so every row is live. A row the console cannot honour is left off the page.

| Row | Values | Notes |
|---|---|---|
| **Aspect** | `corrected`, `square` | Default `corrected` |
| **Fit** | `fit` (as large as fits, keeping the shape), `integer` (the largest whole-number multiple), `fill` (stretched over the screen) | Default `fit` |
| **Filter** | `sharp`, `bilinear`, `nearest` | Default `bilinear`. `sharp` keeps pixel centres crisp and blends only the edges |
| **Scanlines** | 0% (off) to 100%, in tens | **Wii U:** always shown. **Wii:** only in a 480-line video mode, not the 240p modes |
| **Zoom width** / **height** | 50% to 200%, in fives | Used by `fit` and `fill`; **(not used)** with `integer` |
| **Shift X** / **Y** | −100 to +100, in fives | One unit is 1/640 of the screen width (X) or 1/480 of its height (Y) |
| **Widescreen** | `auto`, `4x3`, `16x9` | **Wii only.** `auto` follows the console's own aspect-ratio setting, the other two are overrides |

#### Audio

The mixer runs at the audio hardware's own rate. A device set to the same rate is not resampled.

| Row | Values | When |
|---|---|---|
| **Output** | Read-only: the rate and buffer from the audio driver, e.g. `32000 Hz, 544 frames (17 ms)` | |
| **Sound Blaster** | `sb1`, `sb2`, `sbpro1`, `sbpro2`, `sb16`, `gb`, `none` | DOS prompt only |
| **OPL mode** | `auto`, `cms`, `opl2`, `dualopl2`, `opl3`, `opl3gold`, `none` | DOS prompt only |
| **OPL rate** | **Fast** (22050 Hz), **Matched** (the output rate), **Native** (49716 Hz, OPL only), each offered only if DOSBox accepts it for that setting | DOS prompt only |
| **PC speaker** | On, Off | DOS prompt only |
| **PC speaker rate** | Fast, Matched | DOS prompt only |
| **Tandy sound** | `auto`, `on`, `off`. `on` is skipped unless the machine type is `tandy` or `pcjr` | DOS prompt only |
| **Tandy rate** | Fast, Matched | DOS prompt only |
| **Disney** | On, Off | DOS prompt only |
| **Gravis Ultrasound** | On, Off | DOS prompt only |
| **GUS rate** | Fast, Matched | DOS prompt only |

An `sb16` needs a VGA machine type; on any other it runs as an SB Pro 2 (the [Status](#status) page shows which).

#### Keyboard & Mouse

| Row | Values | When |
|---|---|---|
| **Mouse speed X** / **Y** | 25%, 50%, 75%, 100%, 125%, 150%, 200%, 250%, 300%, 400%, 500% | Live |

A value outside the list (a negative one, which flips the direction, can only be set in `dosbox.conf`) steps to the next preset above or below it.

#### DOS joystick

All rows are live. They re-start the emulated joystick and rebuild its button bindings, which changes nothing a running program keeps but the stick's position.

| Row | Values | Notes |
|---|---|---|
| **Joystick type** | `auto`, `2axis`, `4axis`, `4axis_2`, `fcs`, `ch`, `none` | `auto` is shown as, for example, `auto (4axis)`. Changing it moves any joystick-button binds to the joystick each player now has. The **In DOS** chip on a controller's setup screen sets the same thing |
| **Button wrap** | On, Off | Lets controller buttons past the joystick's own buttons share them |
| **Dead zone** | 0% to 100%, in fives | |
| **Autofire** | On, Off | |
| **Circular input** | On, Off | |
| **Swap axes 3 and 4** | On, Off | |
| **Timed axes** | On, Off | |

#### All settings

**All settings** lists one page per DOSBox config section that has something to show, named as below and in this order. Rows are named as in `dosbox.conf`. A setting that already has a row above is handled by that row.

| Page | What you can change here | Notes |
|---|---|---|
| **Mouse and mapper** (`[sdl]`) | Nothing: `sensitivity` is on the Keyboard & Mouse page and `mapperfile` is **(next launch)** | `autolock` is not listed (see below) |
| **Video output** (`[display]`) | The Video page's rows | |
| **Machine and memory** (`[dosbox]`) | Nothing: all **(next launch)** | Machine type, memory size, language, captures folder |
| **Rendering** (`[render]`) | Frameskip | |
| **CPU** | Core (DOS prompt only), CPU type, cycle step up / down. `cycles` shows **(Performance page)** | |
| **Mixer** | Nothing: all **(next launch)** | `rate` and `blocksize` are not listed (see below) |
| **Sound Blaster**, **Gravis Ultrasound**, **PC speaker, Tandy, Disney** | Everything with a list of values or on / off. The base port, IRQ and DMA rows are here, DOS prompt only | `ultradir` is **(edit dosbox.conf)** |
| **Joystick** | The DOS joystick page's rows | |
| **DOS memory and keyboard** (`[dos]`) | XMS, EMS, UMB, DOS prompt only | `keyboardlayout` is free text and read-only here: **(edit dosbox.conf)** |

On numbers, a list of values is stepped in ascending order; a whole-number range of 100 or fewer is stepped by 1 (by 5 above a span of 20). Anything else is read-only.

#### Saving

**Save** (bottom of the Settings list, not on the pages below it) writes the running configuration, including what you changed in the menu, to disk:

- With a [game](#games) active it saves **that game** (its `.conf` and `.map`) and leaves `dosbox.conf` alone. Otherwise it writes to the **first config file DOSBox loaded**, usually `DOSBox/dosbox.conf`. With none loaded it creates `dosbox.conf` in the DOSBox folder. (A file given with `-conf` is saved over if it was loaded first.)
- The old file is kept as `dosbox.conf.bak`; an earlier `.bak` is replaced, so only the **previous** version is kept. The new file is written under a temporary name, checked, and only then swapped in, so a card that fills up does not leave a half-written file.
- The whole file is **regenerated** from DOSBox's own settings, with DOSBox's usual comments. Comments you added by hand are lost (they stay in the `.bak`), as is any section this build does not declare. `[sdl]`, `[display]`, `[render]` and `[autoexec]` are all written. An old `aspect=` line in `[render]` is not written back, because that setting is now `[display] aspect`.
- The result is on the bottom line: `Saved <path>.`, with `The file it replaced is kept as dosbox.conf.bak.`, or `Not saved.` and the reason.
- It does **not** save controller buttons: those are written to the mapper file as you change them, or saved with a [game](#games).

#### Settings that don't take effect

| Setting | Why |
|---|---|
| `[sdl] autolock` | Ignored on this build: the mouse is always captured. It is hidden on the *All settings* pages, though a saved file still contains it |
| `[mixer] rate`, `blocksize` | The audio driver fixes both. Hidden on the *All settings* pages |
| Everything in `[dosbox]`, `[mixer]` (`nosound`, `prebuffer`) and `[sdl] mapperfile` | Read once at startup. Shown as **(next launch)** and cannot be changed from the menu |
| Tandy sound `on` | Refused unless the machine type is `tandy` or `pcjr`: forcing it on removes the second DMA controller until DOSBox restarts. The row skips the value and the help line says why |
| A Sound Blaster or Gravis Ultrasound base port, IRQ or DMA that would clash with the other card (or put the GUS at `2c0`, which is the VGA registers, or give an SB16 the same 8- and 16-bit DMA) | Refused: the row skips that value and the help line says why |
| Any DOS memory row (XMS / EMS / UMB) | Refused while a program is using upper memory, or when DOSBox is nearly out of internal handlers (each change of EMS uses one; the help line says "Edit dosbox.conf and restart") |
| A restart | **Restart** on the home screen, or `CONFIG -r` at the DOS prompt, starts DOSBox again and reads the config file again. Save first: what is only in the running machine is lost |

### Status

**Status** lists what each setting is **configured** to and what is **in effect**, in six groups. The configured value is what the running configuration holds (your `dosbox.conf` plus anything you changed in the menu and have not necessarily saved). When the value in effect is something else, it follows in brackets, like `auto (dynamic)`. A ` *` at the end marks a setting that is **not at DOSBox's built-in default** (not "different from your `dosbox.conf`"); for the cycles, zoom and shift lines, if any part differs. Press **A** on any row to read everything again; the page does not update by itself. The group headings are rows too.

| Group | Lines |
|---|---|
| **CPU** | **DOS state** (*At the DOS prompt* or *A program is running*, which decides whether DOS prompt only rows can change); **CPU core** (configured, then the core running); **Dynarec**; **CPU type**; **Cycles** (`auto`, `fixed N` or `max P%`, then in brackets what is running: the share of the console and the measured cycles for a percentage mode, or the cycle count) |
| **Video** | **Screen mode** (`Text` or `Graphics` and the size the emulated card draws, or *None yet*); **Frame to display** (the size handed to the display); **Picture shape** (e.g. 4:3, before it is fitted to the screen); **Fit**; **Aspect**; **Filter**; **Scanlines** (only where the console can draw them); **Zoom**; **Shift**; **Widescreen** (Wii only); **Frameskip** (configured, then the running value) |
| **Audio** | **Audio output**; **Sound Blaster** and **OPL mode** (configured, then what runs: an `sb16` on a non-VGA machine shows `sb16 (sbpro2)`); **OPL rate** (`resampled` when it is not the output rate); **PC speaker**; **Tandy sound**; **Disney**; **Gravis Ultrasound** |
| **Memory** | **Machine**; **Memory** (configured MB, then MB the machine has); **XMS**; **EMS** (configured, then `true`, `emsboard`, `emm386` or `false` as in effect); **UMB** |
| **Input** | One line per connected player: the controller (`Wiimote + Nunchuk`, `GameCube`, …) and `(Joystick 1)`, `(Joystick 2)` or `(no joystick)`, from the joystick type; or *None connected*. Then **Joystick** (configured type, then the type in effect: `auto` shows `auto (4axis)`) |
| **Drives and config** | **Config file** (the first file loaded, with `+N` for more `-conf` files; *None (defaults)* if there was none); one line for each mounted DOS drive, or *None mounted* |

**CPU → Dynarec** says **Active** when the CPU core running now is the dynamic recompiler, **Not active** when it is any other core, and **Unknown** when the core cannot be told. It tests only which core is running. On this build `core=auto` picks the dynamic recompiler **at startup**, as long as DOSBox could get memory for its code cache; if it could not, DOSBox falls back to the normal core. So *Not active* with `auto` or `dynamic` means there was no memory for the cache; *Not active* with `normal` is what you asked for.

### Games

A **game** is a saved way of running one DOS program: its controls, the settings that matter to it, and what to start. You can switch from one game to another without leaving DOSBox Wii, and the controls and settings you saved for a game come back with it.

Under the hood a game is an ordinary DOSBox config file that DOSBox reads **after** `dosbox.conf`, so its settings win and `dosbox.conf` keeps everything else. A game you save from the menu holds the whole running configuration as it was at that moment, plus a `.map` file with its controller buttons. A game you write by hand can be as small as you like.

The **Game** card across the top of the home screen shows the game DOSBox was started with: a tile with its first letter, its title, and `Runs KEEN4E.EXE` under it. At the plain prompt it reads **Plain DOSBox** and `Nothing is saved from this session`. Its two buttons are **Games** (the list, below) and **Save** (**Save as game** when no game is active).

#### The Games screen

**Games** opens a list with two tabs, **Load** and **Save**, which you switch with **Left** and **Right**. **Rescan** (beside **Back**) reads the card again, for example after you copy a game to it. The list is read in the background when the screen opens, so rows appear when it finishes.

**Load** is for switching games. **A** opens a small menu for the highlighted game:

| Choice | What it does |
|---|---|
| **Load** | Loads the game: DOSBox restarts with that game's config on top of `dosbox.conf`, then runs its startup. For the game that is already running this reads **Restart**. **Whatever is running is lost**, and so are menu changes you didn't save |
| **Rename** | Changes the name the list shows (the file names stay as they are) |
| **Delete** | Removes the game's `.conf`, and its `.map` if the `.conf` names one beside it. This cannot be undone. Not offered for the game that is running |

The first row is **Plain DOSBox**, which loads `dosbox.conf` alone. The game DOSBox is running is marked **RUNNING**. Under the games, **Found on your card** lists folders just below the `DOSBox` folder that hold an `.EXE`, `.COM` or `.BAT` file and have no game yet. **A** on one asks you to confirm its name (and which program to run, if it has several), then adds a small game for it: a startup that goes to that folder and runs the program, and nothing else, so it uses your `dosbox.conf` settings and the default controls.

**Save** is for keeping what is running. The first row, **New game**, asks for a name on the on-screen keyboard (it suggests the running program's name without `.EXE`, or `My game`), and asks before replacing a game with the same name. Every other row replaces that game with what is running, after a question. The game DOSBox was started with is marked **Save here** and keeps its own startup lines when it is saved.

With a game running, **Save** on the home screen, and **Save** on the Settings list, save **that game** and leave `dosbox.conf` alone. At the plain prompt, **Save as game** on the home screen is the same as **New game**.

#### What a saved game holds

Games are kept in `DOSBox/Games/` on the device that holds your `DOSBox` folder (for example `sd:/DOSBox/Games/`), made on the first save. Each game is two files:

```text
sd:/DOSBox/Games/
├── Commander Keen 4.conf
└── Commander Keen 4.map
```

Names are up to 24 characters, without dots, slashes or other characters a file name cannot hold. The `.map` is written first, then the `.conf`, and each is swapped in only once it is complete, so a save cut short never leaves a game without its map.

The `.conf` is the running configuration, written the way **Settings → Save** writes `dosbox.conf`, with these differences:

```text
# title=Commander Keen 4
# program=KEEN4E.EXE
[sdl]
mapperfile=Commander Keen 4.map
[joystick]
joysticktype=4axis
...                            ← every other setting, as it is running
[autoexec]
mount d "sd:/DOSBox/Other"     ← only if you had mounted it
cd \GAMES\KEEN4
KEEN4E.EXE
```

- `# title=` and `# program=` are comments DOSBox ignores. The title is what the list shows. Without a title the file name is used, and without a program the last line of `[autoexec]` that runs something is used.
- `mapperfile=` is the `.map` beside it, by file name.
- `joysticktype=` is what DOS sees, written out even if it was `auto`.
- `[autoexec]` is built from the drives mounted when you saved and the program running: `C:` is the `DOSBox` folder, which is always mounted, so it is not repeated; other mounts are written as `mount` or `imgmount` lines, then a `cd` and the program. Only the game's own lines are written, not the ones from `dosbox.conf`. A drive that can't be written (a disk image, or a `C:` that is not the `DOSBox` folder) is skipped, and the message after the save says so.

The `.map` is a copy of DOSBox's whole mapper file at the moment of the save: every event and its binds (keys, hotkeys, joystick buttons and axes, and each player's `pad_N` lines). It is the same format as `mapper.map`, so you can copy a game between consoles or edit the `.map` in a text editor. While a game is running its `.map` is the live mapper file, so button changes you make in the menu are held in memory and reach the file when you save.

#### Making a game by hand

Nothing has to be registered. The list shows every `.conf` (except `dosbox.conf`) in the `DOSBox` folder, in `DOSBox/Games/`, and in any folder one level below the `DOSBox` folder, whoever made it. The first lines can say how to show it:

```text
# title=Commander Keen 4
# program=KEEN4E.EXE
[cpu]
cycles=fixed 6000
[autoexec]
cd \GAMES\KEEN4
KEEN4E.EXE
```

Only what differs from `dosbox.conf` is needed. Other settings keep their `dosbox.conf` values, and `[autoexec]` lines from both files run, `dosbox.conf`'s first, so keep that one to mounts (an `exit` or a program started there stops the game's lines from ever running). `mapperfile=` is looked up beside the game's `.conf`. The list shows at most about 150 rows, and says how many more there are.

You can also restart into a game from the DOS prompt with `CONFIG -r -gameconf name.conf` (see [Where DOSBox looks for dosbox.conf](#where-dosbox-looks-for-dosboxconf)). The game then shows as **RUNNING**.

### Wii and Wii U

| | Wii (and vWii) | Wii U (native) |
|---|---|---|
| Home button | Opens the menu | Opens the menu. DOSBox Wii switches the Wii U's own HOME menu off while it runs, so the system HOME menu cannot be reached from inside it: use **Exit** |
| GameCube controller | Yes: **L + R + Start** opens the menu | Not supported |
| Wii U GamePad | Only through [VC-style injection](#vwii-via-vc-style-injection-gamepad-support): player 1, buttons and sticks, no touch | Yes. Shown under the player slots with its battery, if present when the menu opened |
| **TV** button on the GamePad | Nothing | Opens and closes the Wii U's on-screen keyboard on the GamePad screen; what you type goes into DOS. The TV keeps showing the game. (The home screen's **Keyboard** tile works on both) |
| Video: **Scanlines** | Only in 480-line video modes | Always |
| Video: **Widescreen** | Yes | No (each screen's shape is known) |
| **Exit** | To the Homebrew Channel / launcher, or the console off | To the Wii U Menu |
| Everything else on this page | The same | The same |

---

## Controls

DOSBox Wii separates two things:

- **The physical controller** you hold. The console has four controller slots, and each connected slot is a **player** (Player 1 to 4). The console decides which controller lands in which slot.
- **What DOS sees.** A joystick (or two), or nothing, on the gamepad port. A USB keyboard and mouse are always available to DOS, and you cannot switch them off.

Every control on a controller can be given one **target**: a keyboard key, a DOS joystick button, a mouse button, a DOSBox function, or nothing. Buttons are set per player, and a **preset** gives you a starting point for a kind of game.

### Supported devices

| Device | Wii build | Wii U build |
|---|---|---|
| Wiimote, Wiimote + Nunchuk | Yes | Yes |
| Classic Controller | Yes | Yes |
| Wii U Pro Controller | Yes | Yes |
| GameCube Controller | Yes | **No** (the Wii U input driver has no GameCube support) |
| Wii U GamePad | Only through [VC-style injection](#vwii-via-vc-style-injection-gamepad-support): player 1, buttons and sticks, no touch | Yes (buttons, sticks, touch screen) |
| USB keyboard, USB mouse | Yes | Yes |

A player can hold more than one device. A Wiimote with a Nunchuk or Classic attached counts as one device. A GameCube controller on the same slot is a second device, and the Wii U GamePad always shares **Player 1** with whatever is on slot 1. The setup screen shows a column of device icons so you can choose which one to look at. Everything held on a slot is merged into that one player, and the player has one set of bindings.

### Setting up a controller

Open **Controllers** on the home screen. **Configure** opens the setup screen, and a player's icon in the strip opens that player. The setup screen has:

- **Player N**, with what DOS does with that player ("Joystick 1", or "Keys + mouse") underneath. Arrows to switch players appear when two or more controllers are connected.
- A **drawing of the controller** with a row for each control. Each row shows what that control does now ("Key: Ctrl", "Joystick 2", "Mouse left", "DOS: Cycles up"), or why it does nothing ("Joystick off", "No button 3"). Controls light up on the drawing as you press them on the real controller.
- A **PRESET** chip (this player) and an **IN DOS** chip (the whole game) at the top.
- **Back** at the bottom. To keep your buttons for a game, save it from the home screen; see [Games](#games).

Press **A** on a row to change what that control does. Changes apply at once and are written to the mapper file (`[sdl] mapperfile`, `mapper.map` by default).

#### What DOS sees (the IN DOS chip)

| Mode | `joysticktype` | What it gives you |
|---|---|---|
| **Joystick** | `4axis` | One joystick with four buttons, for player 1 |
| **Two joysticks** | `2axis` | Players 1 and 2 each get a joystick with two buttons |
| **Keyboard + mouse** | `none` | No joystick; the buttons act as keys and the mouse |

Players who do not have a joystick use keys and the mouse. The type `auto` is shown as **Joystick**: with four controllers reported, the mapper picks `4axis`.

Three more types can only be set from *Settings → DOS joystick → Joystick type*, and appear in the IN DOS list only while one of them is active: **Joystick for player 2** (`4axis_2`), **Flight stick (FCS)** (`fcs`, four buttons and a hat) and **Flight stick (CH)** (`ch`, six buttons and a hat). The picker only ever offers joystick buttons 1 to 4. The *Joystick type* row in Settings and the IN DOS chip change the same setting.

Switching mode moves any joystick-button bindings to the joystick each player now has, and drops them for a player who no longer has one. A binding to a joystick nobody uses is left alone, so switching back to two joysticks finds player 2's buttons as they were.

#### Presets (the PRESET chip)

A preset replaces **this player's** buttons only. If you have changed buttons since the preset was applied, the list starts with **Undo my changes**. *Compatible* is hidden for a player who has no joystick.

| Source | Compatible | Arrow keys | Arrows + shooter | Point and click | Menu keys |
|---|---|---|---|---|---|
| D-pad | not assigned (moves the joystick) | arrow keys | arrow keys | arrow keys | arrow keys |
| A | Joystick 1 | Ctrl | Ctrl | Mouse left | Enter |
| B | Joystick 2 | Alt | Space | Mouse right | Esc |
| X (Wiimote `1`) | Joystick 3 | Space | Alt | — | Y |
| Y (Wiimote `2`) | Joystick 4 | Enter | Shift | — | N |
| Plus (Start) | — | Enter | Enter | Enter | Enter |
| Minus (Select) | — | Esc | Esc | Esc | Esc |
| L | — | PgUp | Z | — | — |
| R | — | PgDn | X | — | — |

ZL, ZR and the stick clicks are not assigned by any preset. Under *Two joysticks* only joystick buttons 1 and 2 exist, so *Compatible* leaves X and Y unassigned there (the preset list previews them as "No button 3" and "No button 4").

The preset name on screen is the one the buttons started from. DOSBox does not record it in the mapper file, so until you pick one it is the closest match to your current buttons.

#### Changing one control

Press **A** on a row. The chooser lists, down the left:

| Kind | What it offers |
|---|---|
| **Suggested** | What the presets give this control that this player can use (only shown when there is something to suggest) |
| **Key** | **Press a key on the keyboard** (shown only while a USB keyboard is connected), then 77 keys on tabs: Common, A-Z, 0-9, F-keys, Edit, Symbols (**L** and **R** change tab). Left and right Ctrl, Alt and Shift are one key each |
| **Joystick** | Buttons 1 to 4, limited to what the current mode has. Not shown for a player with no joystick |
| **Mouse** | Left, right and middle button; **Click a mouse button** is shown while a USB mouse is connected |
| **DOSBox** | Cycles up, Cycles down, Frameskip up, Frameskip down, Swap disk image, Fast forward (hold) |
| **None** | The control does nothing |

**Default** gives the control what this player's preset gives it. **B** cancels. A control has exactly one target, so choosing one replaces whatever it did before, including the DOSBox default binds. A key that is not in the list (a keypad key, say) is refused with "That one can't be used here".

Stick rows (**L Stick**, **R Stick**, **Main Stick**, **C-Stick**, **Nunchuk Stick**) are information only. They show "Joystick axes" when the current mode gives this player that stick as an axis (the left stick needs a joystick; the right stick needs the four-axis modes) and "Not used" otherwise. **A stick cannot be bound to keys or the mouse.** The **Home** row, and the GameCube's **Menu** row, are fixed.

### Controls on each controller

Each table lists the rows on the setup screen. **Preset source** says which line of the preset table above a control takes its default from. **Menu**: Home opens the Home menu on every controller that has it; on the GameCube controller (and on any controller, as an alternative) hold **L + R + Plus** (Start on a GameCube).

#### Wiimote

Drawn upright with every control listed beside it, or sideways if the Wiimote is held that way (the setup screen draws the one that matches).

| Row | Preset source | Notes |
|---|---|---|
| A Button | A | |
| B Trigger | B | |
| 1 Button | X | Shares the joystick button with X (a Wiimote has no X) |
| 2 Button | Y | Shares the joystick button with Y |
| Minus / Plus | Minus / Plus | |
| Up, Down, Left, Right | D-pad | Held sideways, the directions are the ones you hold it by |
| Home | — | Fixed: opens the menu |

#### Wiimote + Nunchuk

| Row | Preset source | Notes |
|---|---|---|
| C Button | L | |
| Z Button | ZL | |
| Nunchuk Stick | — | Information: joystick axes |
| A Button, B Trigger, 1 Button, 2 Button | A, B, X, Y | |
| Minus / Plus | Minus / Plus | |
| Up, Down, Left, Right | D-pad | The Wiimote's D-pad |
| Home | — | Fixed: opens the menu |

#### Classic Controller

| Row | Preset source | Notes |
|---|---|---|
| L Trigger, R Trigger | L, R | Treated as buttons |
| ZL Button, ZR Button | ZL, ZR | |
| Minus / Plus | Minus / Plus | |
| L Stick, R Stick | — | Information: joystick axes. **No stick clicks** (a Classic has none) |
| Up, Down, Left, Right | D-pad | |
| B, A, Y, X | B, A, Y, X | |
| Home | — | Fixed: opens the menu |

#### GameCube Controller (Wii build only)

| Row | Preset source | Notes |
|---|---|---|
| L Trigger, R Trigger | L, R | Treated as buttons |
| Z Button | ZR | |
| Start | Plus | |
| Main Stick, C-Stick | — | Information: joystick axes. No stick clicks |
| Up, Down, Left, Right | D-pad | |
| B, A, Y, X | B, A, Y, X | |
| Menu | — | Fixed: "Hold L + R + Start" |

A GameCube controller has no Minus or ZL, so those sources are never pressed.

#### Wii U Pro Controller

| Row | Preset source | Notes |
|---|---|---|
| L Bumper, R Bumper | L, R | |
| L Trigger, R Trigger | ZL, ZR | |
| Select, Start | Minus, Plus | |
| L Stick, R Stick | — | Information: joystick axes |
| L Click, R Click | L3, R3 (not in any preset) | **Wii U build only.** The Wii build never reports stick clicks, so these rows are left out there |
| Up, Down, Left, Right | D-pad | |
| B, A, Y, X | B, A, Y, X | |
| Home | — | Fixed: opens the menu |

#### Wii U GamePad (Wii U build)

The same rows as the Wii U Pro Controller, drawn as a GamePad, with **L Click** and **R Click**. The touch screen is not a row: it is a trackpad (see [Keyboard & mouse](#keyboard--mouse)).

### Keyboard & mouse

- **USB keyboard:** it works at the DOS prompt and in games, and it can be used to pick keys in the chooser. The DOS keyboard layout is `[dos] keyboardlayout`, under *All settings → DOS memory and keyboard*.
- **On-screen keyboard:** *Home → Keyboard*.
- **The mouse is always captured.** The first click is not used up capturing it, and `[sdl] autolock` is ignored. Speed is under *Settings → Keyboard & Mouse*.
- **Moving the mouse** takes one of three things: the Wiimote pointer, a USB mouse, or the GamePad touch screen. **There is no way to move the mouse with a stick or the D-pad.** A Classic, Wii U Pro or GameCube controller with nothing else attached cannot move it.
- **Wiimote pointer:** only **player 1's** Wiimote is used, on its own or with a Nunchuk attached. It is not available while a Classic Controller or Wii U Pro Controller is attached.
- **Player 1's Wiimote A and B are always also the left and right mouse buttons.** This is fixed, and it is in addition to whatever the preset gives A and B. A game that reads both the mouse and the joystick or keyboard sees both.
- **USB mouse:** left, right and middle buttons and movement. The wheel is not read.
- **GamePad touch screen** (Wii U build): a trackpad. Dragging moves the cursor, a tap is a left click, and **ZR** and **ZL** are the left and right buttons while the GamePad is connected. These are fixed, whatever the buttons are set to. While the on-screen keyboard is open, the touch screen belongs to it.

### The D-pad and the joystick

A DOS joystick has no D-pad, so with a joystick enabled the D-pad is folded into the joystick's left stick axes. To stop an arrow-key D-pad also steering the joystick, **the D-pad drives the joystick only while all four of its directions are left on the joystick default or Not assigned.** Give any direction a key, mouse button, joystick button or DOSBox function and the whole D-pad stops moving the joystick. The rule is checked when emulation starts and again after you leave the menu.

---

## Performance

Three things set how fast DOSBox Wii runs a game, and all three can be changed while it runs: the CPU core, the cycles, and frameskip.

1. **Check the dynarec is on.** *Status → CPU → Dynarec* reads *Active* when the dynamic recompiler is the core running. With the default `core=auto`, DOSBox starts on the dynamic core (it doesn't wait for a program to enter protected mode) if it could set up the code cache. The cache is 8 MB, or as little as 1 MB if that is all there is. On the **Wii** it comes from MEM1 and only MEM1 (see [Memory](#memory)). On the **Wii U** it comes from the area the console reserves for generated code, so if the console gives the app none, DOSBox runs the normal core and Status reads *Not active*.
2. **Cycles.** Set them under *Settings → Performance* (at any time), and step them from the home screen's speed buttons:

   | Mode | What it does | Home-screen **+** / **−** |
   |---|---|---|
   | `auto` (default) | Starts at 3000 cycles, and moves to `max` when a program enters protected mode | Steps the running amount |
   | `max` | As many as the console can spare; *Settings* offers shares from 10% to 100% | Moves the share in steps of 5%, from 5% to 105% |
   | `fixed N` | A set amount; *Settings* offers 500 to 50000 | Steps the amount by `cycleup` (default 10: a percentage below 100, a number of cycles from 100) |

   The home-screen buttons and the cycle hotkeys change the running value, and the config follows it, so *Settings* shows it and **Save** writes it. The exception is `auto`: the running amount there is DOSBox's own guess, and is not written.
3. **Frameskip** is 0 to 10 and is stepped from the home screen. The config follows it (`[render] frameskip`).
4. **The normal core** is *Settings → Performance → CPU core* (`auto`, `dynamic` or `normal`; at the DOS prompt only). Use it to compare.

---

## Memory

The Wii has two banks of memory, and nearly all of both is spoken for before a game starts. This section is the breakdown, so you can see where it goes, and why DOSBox Wii doesn't give DOS more RAM, draw bigger video modes, or run CPU upscaling filters.

### MEM1: 24 MB, fast

| What | MB |
|---|---|
| Held back before the program starts (the top of MEM1) | 1.0 |
| **The program itself:** code 3.5, read-only data 0.8, exception tables 0.5, data 0.15, bluetooth/GX FIFO 1.7 | 6.7 |
| **The heap pool** (everything left) | 16.2 |
| &nbsp;&nbsp;of which the **dynarec's code cache** | 8.0 |
| &nbsp;&nbsp;of which the rest, for anything that uses `malloc` or `new` | 8.2 |

The code cache is taken from MEM1 and nowhere else: it is 8 MB if the pool has that much free, as little as 1 MB if that is all it has, and the dynarec stays off below that.

### MEM2: 64 MB

These blocks are taken straight from MEM2, not from the heap. Most are taken once and kept until DOSBox Wii exits (a **Restart** reuses them), and several are taken at their largest size, so asking for less gives nothing back. Only the picture buffers follow the video mode.

| What | MB | Notes |
|---|---|---|
| **Paging TLB** | 20.0 | The full-size TLB |
| **DOS RAM** | 16.0 | The most the Wii allows, taken whatever `memsize` says |
| **Dynarec block table** | 8.0 | 128k blocks (64 bytes each) |
| **Video card memory** | 4.5 | 4 MB, the most the S3 card can have, taken whatever `machine` is set to, and a 0.5 MB fast copy used by 16-colour modes |
| **TV output** | 1.4 | Two 640×576 frames of 720 KB |
| **Picture buffers** | 0.3 to 2.4 | They follow the video mode. A full-size frame is 0.6 MB (640×480, 16 bits), and there are three: the frame, the texture the GPU draws from, and the scaler's cache of the last frame, which is 1, 2 or 4 bytes a pixel (so 0.06 MB for a 320×200 256-colour mode, 0.3 MB at 640×480, 1.2 MB for a 32-bit mode). The Sharp filter adds an enlarged copy, up to 0.6 MB, while it is on |
| **Gravis Ultrasound memory** | 1.0 | Only if the GUS is turned on |

### What the heap does

Anything that is not one of those blocks (the dynarec's code pages, the `C:` mount, the menu's own objects, fonts, DOS's bookkeeping) comes from the one ordinary heap. That heap starts in the MEM1 pool, and when the pool is full it carries on into MEM2. So **MEM1 free** reading close to zero is normal and is not a sign of trouble; what can run out is the two banks together.

Added up, the fixed blocks leave only a few MB spare. After startup ~7 MB is free across both banks, and a bigger video mode, the Sharp filter and the GUS can each take some of it.

### Why not more?

- **More DOS RAM.** The 16 MB limit is in the code (`MAX_MEMORY` in `memory.cpp`), and it is set aside already: the TLB, DOS RAM and the dynarec's block table alone are 44 of MEM2. Every extra MB would come out of the 6 to 7 MB spare. The Wii U allows up to 63.
- **Bigger video modes.** The renderer's buffers are 640×480, so a mode larger than that (the 800×600 and bigger VESA modes) isn't drawn. A bigger mode would also cost every buffer in the table above, at a larger size.
- **A bigger code cache.** 8 MB is the most the dynarec uses. It has to be one piece of MEM1, which is half the pool.
- **A smaller TLB.** The full TLB is 20 MB and is kept for speed. A reduced one of about 1.3 MB is in the core: it is used when `USE_FULL_TLB` is undefined in `paging.h`, which saves about 19 MB at some cost in speed that hasn't been measured.

### The menu

On Wii, the menu has no memory of its own. While it is open the emulator is paused, so the menu borrows the dynarec's 8 MB code cache for its images, text and the picture it keeps of the game behind it. When you close the menu the cache is handed back empty: the dynarec translates the code again as the game runs.

### Wii U

The Wii U build asks for the same blocks, from one large ordinary heap, so none of this is tight there: DOS RAM goes up to 63 MB and is taken at that size whatever `memsize` says.

---

## Troubleshooting

### A game closes DOSBox, crashes, or hangs

When DOSBox hits an internal error it stops and DOSBox Wii leaves, as if you had chosen **Exit**. It shows no message: the message goes to a debug log that release builds don't write. Things to try:

- **Delete `dosbox.conf`** (from the `DOSBox` folder) to restore the default settings. A new one is created on next launch. (Settings → Save keeps the version before your last save as `dosbox.conf.bak`.)
- **Turn the sound off**, alone or with the others below: *Sound Blaster* `none` and *Gravis Ultrasound* off under *Settings → Audio*. Those rows change at the DOS prompt only, so set them before you start the game (or edit `dosbox.conf` and press **Restart**).
- **Fixed cycles**, for example `fixed 10000`, under *Settings → Performance*.
- **`memsize`** in `[dosbox]`: default 16 MB. On the Wii 16 is also the most there is (a larger value is treated as 16), and the 16 MB stays set aside however low you set it, so lowering it changes what DOS sees, not how much memory is free ([Memory](#memory)). It is read once at startup, so edit `dosbox.conf` and press **Restart**.
- **DOS memory off** (`ems`, `xms`, `umb`), under *Settings → All settings → DOS memory and keyboard*, at the DOS prompt. A change is refused while a program is using upper memory.
- **`LOADFIX`**: `loadfix game.exe` reserves 64 KB of memory (`loadfix -32 game.exe` for another size), runs the program, then frees it.
- **The normal core** instead of the dynarec (*Settings → Performance → CPU core*).

### A game runs slowly

See [Performance](#performance).

### I can't open the menu

Press **Home** on a Wiimote, Classic Controller, Wii U Pro Controller or GamePad; hold **L + R + Plus** on one Classic, Wii U Pro or GamePad (**L + R + Start** on a GameCube controller); or tap the **Windows / Super** key on a USB keyboard. A Wiimote (with or without a Nunchuk) can only use **Home**. The menu opens only once DOSBox is running. See [Opening and closing it](#opening-and-closing-it).

### My USB drive doesn't show up

Open *Settings → Drives*: it only notices drives plugged in or removed while it is open, so wait a moment for the row to appear, then press **A** on it to mount it (or use `MOUNT`; see [Mounting other drives](#mounting-other-drives)). On the Wii U, USB drives are never mounted until that screen has been open, and USB needs **Mocha** (see [Wii U](#wii-u)). A drive in a format the console can't read shows as `Not available` (Wii) or doesn't appear (Wii U): use FAT32.

### A setting says "(DOS prompt only)" or "(next launch)"

Audio, CPU core and DOS memory settings change only at the DOS prompt (leave the program first). Rows marked **(next launch)** are read once at startup and cannot be changed from the menu: edit `dosbox.conf`, then press **Restart** on the home screen (or run `CONFIG -r`). See [When a row can be changed](#when-a-row-can-be-changed).

---

## Known Limitations

- Video modes larger than 640×480 aren't drawn (the renderer's buffers are that size), and on the Wii DOS gets at most 16 MB of RAM. Both come from the memory budget, see [Memory](#memory).
- Disc drives and network shares can't be used, and no CD drive is reported to DOS (so no CD audio): there is no screen to connect a share or read a disc, so `dvd:/` and `smb:/` don't exist for `MOUNT`.
- No MIDI or MPU-401, no serial ports and no modem or IPX networking: none of these is compiled in, and there is no `[midi]`, `[serial]` or `[ipx]` section. A game that looks for a MIDI device finds none; use Sound Blaster FM music instead.
- No screenshots (they are not built in).
- Drives are mounted from *Settings → Drives* or with `MOUNT`; see [Mounting other drives](#mounting-other-drives) for what the Drives screen can and can't see.
- GameCube controllers are not supported on the Wii U build.
- Sticks and the D-pad cannot move the mouse or be bound to keys. The mouse moves only from the Wiimote pointer, a USB mouse or the GamePad touch screen.
- A game's saved controls and settings come back only when you load it from **Games** (or `CONFIG -r -gameconf`); they do not switch on by themselves when its program starts.
- On the Wii U build the system HOME menu is switched off while DOSBox Wii runs, so Home opens DOSBox's menu and there is no way to the system one; use **Exit**.
- The DOS keyboard layout (`[dos] keyboardlayout`) is shown in the menu but can only be changed in `dosbox.conf`.

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
- [Change History (CHANGELOG.md)](CHANGELOG.md)
