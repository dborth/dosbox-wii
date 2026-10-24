# DOSBox Wii Changelog

All notable changes to DOSBox Wii are recorded here, newest first. For the current features and setup instructions, see [README.md](README.md).

## 2.0 (unreleased)
* **A new port.** DOSBox Wii has been rebuilt on the same [libgui](https://github.com/dborth/libgui) interface and driver framework as the other GX emulators, and no longer uses SDL. The DOSBox core is stock SVN r4301, and one codebase now builds for both the Wii and the Wii U
* **Wii U release!** DOSBox Wii now runs natively on the Wii U as its own app, with its own icon on the Wii U Menu. Requires [Aroma](https://wiiu.hacks.guide/) - run the latest Aroma release and launch the `.wuhb` directly from it
  * Full **Wii U GamePad** support - touch screen and buttons - alongside Wiimote, Nunchuk, Classic Controller and Wii U Pro Controller
  * SD card plus up to 3 USB drives, mounted from the new **Drives** screen. USB supports FAT32, exFAT and NTFS (requires [Mocha](https://github.com/wiiu-env/MochaPayload), an optional Aroma component)
  * Recommended: install [Bloopair](https://github.com/GaryOderNichts/Bloopair) to pair Bluetooth controllers (Switch Pro, Joy-Con, DualShock/DualSense, Xbox, and more) as if they were a Wii U Pro Controller
  * The Wii build also runs in vWii, including as a VC-style injected channel, which lets the Wii U GamePad (buttons and sticks, no touch) act as player 1
* **PowerPC dynamic recompiler.** Adds the dynarec by jmarsh, updated by retro100, for much faster emulation than the interpreter. The **Status** page and the **Credits** screen both show whether it is the core running
* **A brand new Home menu.** The old menu had an on-screen keyboard and Exit and Close buttons, and nothing else. Everything below is new. Open it at any time with Home, with L + R + Plus on one controller (Start on a GameCube controller), or with a tap of a USB keyboard's Windows key
  * **Home screen** with quick settings for speed, frameskip, aspect and filter, a status strip for the connected controllers, and tiles for the pages below
  * **Settings** for Performance, Video, Audio, Keyboard & Mouse and the DOS joystick, plus an **All settings** editor for everything else in `dosbox.conf`. Changes that can apply live do so at once; the rest say whether they need the DOS prompt or a restart
  * **Settings → Save** writes your settings back to `dosbox.conf`, keeping the previous version as `dosbox.conf.bak`
  * **Status** page showing what each setting is configured to and what is actually in effect (CPU core, cycles, screen mode, audio output, mounted drives, loaded config), with anything not at DOSBox's default marked
  * **Drives** screen that mounts SD cards and USB drives as DOS drives and notices drives plugged in or removed while it is open
  * **Restart** from the menu, or `CONFIG -r` at the prompt, which restarts DOSBox without leaving the app
  * **Games** list: pick a game's own config and DOSBox restarts into it. A game config is layered over `dosbox.conf`, so it only has to hold what is different
* **New controller setup.** Each player gets a drawing of their controller with every button's current target beside it. Held buttons light up as you press them. Choose a key, joystick button, mouse button or DOSBox function for any button by picking from a list or by pressing the key or mouse button itself. Built-in presets cover common kinds of DOS games. Wiimote, Wiimote + Nunchuk, Classic Controller, Wii U Pro Controller, Wii U GamePad and (Wii build) GameCube controller are supported, for up to four players
* **USB keyboard and mouse** support, the Wiimote pointer as a mouse, the Wii U GamePad touch screen as a mouse, and an on-screen keyboard (activated with the TV button)
* **New video options:** original (4:3) or square-pixel aspect, fit / integer / fill sizing, sharp / bilinear / nearest filtering, scanlines, zoom and shift, and a widescreen setting on the Wii
* Updated to latest devkitPPC and libogc2 on the Wii, wut on the Wii U

## 1.7 - June 30, 2012
* Support for newer Wiimotes
* Updated to latest DOSBox SVN
* Updated to latest libogc and devkitPPC

## 1.6 - July 6, 2011
* Updated to latest DOSBox SVN
* Updated to latest libogc and devkitPPC

## 1.5 - December 14, 2010
* Updated to latest DOSBox SVN
* USB compatibility improvements
* Fix tilde key on keyboard

## 1.4 - August 11, 2010
* Updated to latest DOSBox SVN
* USB 2.0 via IOS58 support. Requires that IOS58 be pre-installed.
* Improved USB keyboard/mouse support

## 1.3 - June 24, 2010
* Updated to latest DOSBox SVN
* Updated to latest libwiigui
* Improved on-screen keyboard

## 1.2 - February 18, 2010
* Updated to latest DOSBox SVN

## 1.1 - October 27, 2009
* Fixed "black screen" issue with numerous games
* Minor optimizations and bug fixes
* Updated to latest DOSBox SVN

## 1.0 - October 9, 2009
* Home menu and on-screen keyboard (a proof of concept showing SDL + libwiigui)
* Small speed increases via SDL Wii improvements
* Compiled with devkitPPC r18 and libogc 1.8.0
* Updated to latest DOSBox SVN

## 0.73.02 - July 1, 2009
* Upgraded DOSBox engine to latest SVN
* Fixed lockup when only using the Wiimote (no attachments connected)
* Compiled with latest libogc and SDL Wii

## 0.73.01 - June 3, 2009
* Upgraded DOSBox engine to 0.73
* Sound issues fixed
* Compiled with latest libogc and SDL Wii

## 0.72.01 - May 14, 2009
* Initial release
