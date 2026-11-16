# DOSBox Wii U: a game in its own app

This kit turns a DOS game into its own Wii U app: one `.wuhb` that holds DOSBox Wii U **and** the game, with its own icon on the Wii U Menu. Start it and it goes straight into the game.

You need the game's files, and the right to share them if you give the app to anyone. Nothing here installs anything on the console: the finished `.wuhb` is copied to the SD card like any other Aroma app.

## What is in this folder

| | |
|---|---|
| `dosbox-wiiu.rpx` | DOSBox Wii U itself |
| `content/` | **Your game.** Everything in it ends up inside the `.wuhb` |
| `content/autoboot.conf` | The config the game starts with |
| `content/GAME/` | The game's files. A tiny `HELLO.BAT` is there so the kit works before you add anything |
| `meta/` | The icon and the two splash screens (in the download; added when it is made) |
| `build.sh`, `build.bat` | Build the `.wuhb` (Linux and macOS, Windows) |
| `tools/` | `wuhbtool`, which packs the `.wuhb` (see `tools/README.txt`) |

## Make your app

1. **Try the kit first.** Run `build.bat` (Windows) or `sh build.sh` (Linux, macOS). Copy the `.wuhb` it makes to `wiiu/apps/` on the SD card and start it from Aroma: it should show "Hello from a game bundled in a .wuhb".
2. **Add your game.** Delete `content/GAME/HELLO.BAT` and put your game's files in `content/GAME/` (or a folder of your own, and change the name in `autoboot.conf`).
3. **Tell DOSBox what to run.** Edit the end of `content/autoboot.conf`:

   ```text
   [autoexec]
   mount c GAME -pr
   c:
   cd KEEN4
   KEEN4E.EXE
   ```

   `mount c GAME -pr` makes the `GAME` folder next to `autoboot.conf` the C: drive. The `# title=` and `# program=` lines at the top are only labels (the Games list uses them).
4. **Name it.** Open `build.sh` or `build.bat` and change the name, the author and the output file name at the top. Replace the pictures in `meta/` to change the icon (128x128) and the splash screens (1280x720 for the TV, 854x480 for the GamePad).
5. **Build it again** and copy the `.wuhb` to the SD card.

## Good to know

- **The game is read-only.** The `.wuhb` is not writable, so a game that saves into its own folder cannot. Games that write their saves or setup files to C:\ need a writable place: make a folder on the SD card (say `DOSBox/saves`), mount it as well in `[autoexec]` with `mount d /vol/external01/DOSBox/saves` (the Wii U's name for the SD card), and tell the game to save to D:.
- **Settings are changed here, not on the console.** The menu's Save would write into the `.wuhb`, which it cannot, so it says it could not write the file. Edit `autoboot.conf` and build again. The SD card still counts for one thing: if its `DOSBox` folder has a `dosbox.conf`, that applies first and `autoboot.conf` after it, so `autoboot.conf` only needs what is different for this game. Keep `exit` and program starts out of that file's `[autoexec]`, or the game's lines never run.
- **Controls.** Set them up on the console in the normal DOSBox Wii U app, then **Save as game** in the Games menu. That writes a `.map` file to the `DOSBox/Games` folder on the SD card: copy it into `content/` as `autoboot.map`, and uncomment the `mapperfile=autoboot.map` lines in `autoboot.conf`.
- **More than one game in an app.** Other confs in `content/` (or one folder down) show up in the menu's **Games** list, next to the one the app starts with. In an app like this, the list shows what the `.wuhb` holds, not the SD card's `DOSBox` folder.
- **To check it worked,** open the menu and look at **Status**: the **Autoboot** row names the config in the `.wuhb`. No such row means the app did not find `content/autoboot.conf`, and it started plain DOSBox.
- **Needs Aroma,** like DOSBox Wii U itself. A bare `.rpx` has no content, so start the `.wuhb`.
