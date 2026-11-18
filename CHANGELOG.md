# DOSBox Wii Changelog

All notable changes to DOSBox Wii are recorded here, newest first. For the current features and setup instructions, see [README.md](README.md).

## 2.1 - November 18, 2026
* **Start straight into a game (Wii).** Name a game when you launch DOSBox Wii and it starts with that game's config on top of `dosbox.conf`: from the Homebrew Channel (`<arguments>` in `meta.xml`) or from WiiFlow, either as a plugin (the zip now includes `wiiflow/plugins/dosbox-wii.ini`, which lists the `.conf` files in `DOSBox/Games` as games) or from its Homebrew list. A game can be named as `-gameconf <conf or folder>`, as a bare `.conf`, or as a bare folder that holds one (`autoboot.conf`, or its only `.conf`). A relative name is looked for as written, beside the launched `boot.dol`, then in the `DOSBox` folder, so a game's `.conf`, `.map` and files can all live in one folder, anywhere. If the game cannot be found DOSBox starts without one and says why on the DOS screen. The **Status** page has an **Autoboot** row, and the app now knows it was started this way (`IsAutoboot()`), ready for a single-game menu later. See the README's *Starting a game at launch*
* **A game in its own app (Wii U).** A `.wuhb` with an `autoboot.conf` in its content folder (`fs:/vol/content`, which Aroma mounts) starts straight into that game. Its Games list shows what the `.wuhb` holds. The new **DOSBox Wii U Autoboot Kit** download has the `.rpx`, `wuhbtool`, build scripts, a template and instructions for making one
* Add missing keyboard keys to the mapping UI
* **Video output mode (Wii).** Choose the video mode the TV gets while you play: 480i, 480p (component cable) or 240p, for NTSC, PAL60 and PAL, from *Settings → Video → Output mode* or `output` in `[display]`. The config accepts any of libogc2's 57 stock modes. The menu stays in the console's own mode and the game screen behind it lines up with the game's, whichever mode it was in. Per-game configs can carry their own mode.
* **Deflicker (Wii).** *Settings → Video → Deflicker* (`deflicker` in `[display]`: `auto`, `off`, `normal`, `strong`) sets the vertical filter used when the picture is sent to the TV. Until now the game was always shown unfiltered, so a 480i picture shimmered on a CRT even though the menu did not. `auto` uses the video mode's own filter, which steadies 480i
* Fix (Wii): **Scanlines** no longer flash on a 480i picture. They were drawn on every other line, which in 480i is a whole field, so the screen blinked black. The row is now offered only in a progressive 480-line mode; for a CRT, use the 240p output mode instead
* Fix: `MOUNT` waits for a USB drive that is slow to start. An `[autoexec]` such as `mount c usb:/dosbox` used to fail at launch when the drive was not ready yet, and only worked once the Drives screen had been opened. It now prints `Waiting for USB Mass Storage...` and tries for up to 8 seconds. On the Wii U, a `usb1:/` path mounts the drive itself
* `usb:/` and `usb1:/` are now accepted on both consoles, so a config written for one works on the other
* Fix: the `[display]` settings are now in place before the first picture, so a game config's `deflicker` (and `output`) is used from the start, not only after the menu has been opened
* Fix: **Restart now gives DOSBox the state of a first start.** Restarting into another game (or plain DOSBox) used to carry over leftovers from the run before, which could show up as a black screen, a mouse that stopped working, a wrong timer, or a crash on a later restart. The CPU registers and privilege level, the timer, the video card, DMA, DOS's memory and settings, the Num Lock / Caps Lock start state and any held stick direction are all reset now, and files a program left open are closed instead of piling up with each restart. A restart that is cut short by an error no longer ends in `abort called`
* Fix: the **mouse driver** is no longer overwritten by DOS's own tables after a restart (DOS's private memory is reset before the mouse driver is set up, not after)
* Fix: the PowerPC **dynarec's code cache is always set up at launch**, whatever `core` is set to, so the core can be switched to `dynamic` later (from the menu, or by a game's config) without it being unavailable. It has to be taken from memory at startup, before anything else uses it
* Fix (Wii): **frame pacing.** A program that draws faster than the display no longer stalls the emulation waiting for the screen, which also mixes the audio, so such programs no longer fall behind real time (#62)
* Fix (Wii U): **non-blocking frame present.** The same fix for the Wii U, where a present waited for the GPU and the swap (#62)
* **Faster Save and Games.** Saving a game or `dosbox.conf` is made in memory and written once, on a worker thread, behind a small **Saving** panel with a spinner, so a slow SD card or USB drive shows a moving spinner instead of a frozen screen. Renaming, deleting, saving and adding a game update the Games list directly instead of reading the card again
* Fix (Wii U): a USB drive with nothing DOSBox can mount on it (a Wii U WFS-formatted drive, or an unformatted one) is now noticed once and left alone, rather than probed again and again. Mounting is only tried when the set of USB storage devices changes, so an idle console makes no mount attempts
* Fix (Wii U): closing the **Drives** screen while a USB mount is still in progress no longer freezes the menu. The mount is allowed to finish while the menu keeps running. The USB and drive code also holds its locks more carefully, and the emulation thread has a larger stack
* Fix (Wii): IOS reload and the L2 cache enhancement are now the very first things done at startup

## 2.0 - October 25, 2026
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
