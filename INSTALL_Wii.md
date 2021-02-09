# Compile Dosbox for Wii

This document describes the following points:

- Setup Wii Development Environment
- Install dependencies for compilation
- Compilation and installation of SDL Wii
- Compilation of Dosbox

Some commands are debian/ubuntu specific (apt command). If you use another linux distribution this commands must be replaced with the correct ones.

## Setup Wii Development Environment

devkitPro is required for Dosbox Wii compilation.

General informations about devkitPro:
https://devkitpro.org/wiki/Getting_Started

Installing devkitPro pacman (which is the package manager used by devkitPro)

Debian Package can be found at https://github.com/devkitPro/pacman/releases

All shell commands with a # must be entered as root or with sudo. The $ promt is the normal user.

```
# apt update
# apt install gdebi-core
$ wget https://github.com/devkitPro/pacman/releases/download/v1.0.2/devkitpro-pacman.amd64.deb
# gdebi devkitpro-pacman.amd64.deb
```

For a short introduction see: https://devkitpro.org/wiki/devkitPro_pacman#Using_Pacman

To resync the databases

```
# dkp-pacman -Sy
```

Install the packages for Wii development with the package wii-dev.

The group package wii-dev installs all necessary basic packages for wii development.

```
# dkp-pacman -S wii-dev
```

This will install following packages: devkitppc-rules, libfat-ogc, libogc, ppc-pkg-config, wii-examples, wii-pkg-config, devkit-env, devkitPPC, devkitPPC-gdb, gamecube-tools, general-tools, wiiload

```
To use the toolchains right away, use
$ source /etc/profile.d/devkit-env.sh
```

After the installation you can check your installed packages inclusive version numbers like this:

```
# dkp-pacman -Q
devkit-env 1.0.1-2
devkitPPC r38-1
devkitPPC-gdb 9.2-1
devkitppc-rules 1.1.0-1
devkitpro-keyring 20180316-1
gamecube-tools 1.0.2-1
general-tools 1.2.0-2
libfat-ogc 1.1.5-2
libogc 2.1.0-1
ppc-pkg-config 0.28-5
wii-examples 20200609-1
wii-pkg-config 0.28-5
wiiload 0.5.1-2
```

Search a package (e.g. freetype)

```
# dkp-pacman -Ss freetype
dkp-libs/3ds-freetype 2.9-2 (3ds-portlibs)
    TrueType font rendering library (mingw-w64)
dkp-libs/nds-freetype 2.9-1 (nds-portlibs)
    FreeType is a freely available software library to render fonts.
dkp-libs/ppc-freetype 2.9.1-2 (ppc-portlibs)
    FreeType is a freely available software library to render fonts.
dkp-libs/switch-freetype 2.10.1-1
    FreeType is a freely available software library to render fonts.
```

## Install dependencies for compilation

For Dosbox compilation the libraries freetype, libvorbisidec, sdl wii and there dependencies must be installed. The SDL version from pacman **can't** be used.

With the package manager freetype and libvorbisidec must be installed. The installation of these libraries also installs there dependencies.

```
# dkp-pacman -S ppc-freetype
```

Does also install there dependencies: ppc-bzip2, ppc-zlib, ppc-libpng

```
# dkp-pacman -S ppc-libvorbisidec
```

Does also install there dependencies: ppc-libogg

Currently the SDL Wii port **can't** be installed per dkp-pacman! **Don't** install the package wii-sdl.

The reason is this. Dosbox Wii currently uses functions from the SDL library that are not officially provided with header files. These functions are WII_AudioStart(), WII_AudioStop(), WII_VideoStart() and WII_VideoStop() and used in src/platform/wii/wiihardware.cpp. The wii-sdl package doesn't provide export these functions. This results in undefined references at linking. To prevent this the library from github should be used. The current version from the master branch https://github.com/dborth/sdl-wii works fine. It also exports the functions WII_AudioStart(), WII_AudioStop(), WII_VideoStart() and WII_VideoStop() (A header file is not provided for these functions. This is no problem because src/platform/wii/wiihardware.cpp make its own prototyp declarations for these functions.)

## Compilation and installation of SDL Wii

Install git which is needed to check out the source code of SDL Wii.

```
# apt install git
```

Checkout SDL Wii.

```
~$ git clone https://github.com/dborth/sdl-wii.git
```

Setup devkitPro Development Environment for compiling a Wii project and compile SDL Wii.

```
$ source /etc/profile.d/devkit-env.sh
~$ cd sdl-wii/SDL
~/sdl-wii/SDL$ make
```

Install SDL Wii into the correct directories from devkitPro.

```
~/sdl-wii/SDL# make install
'lib/libSDL.a' -> '/opt/devkitpro/portlibs/wii/lib/libSDL.a'
'include/SDL.h' -> '/opt/devkitpro/portlibs/wii/include/SDL/SDL.h'
'include/SDL_active.h' -> '/opt/devkitpro/portlibs/wii/include/SDL/SDL_active.h'
...
...
...
'include/begin_code.h' -> '/opt/devkitpro/portlibs/wii/include/SDL/begin_code.h'
'include/close_code.h' -> '/opt/devkitpro/portlibs/wii/include/SDL/close_code.h'
```

Now all dependencies for compiling the dosbox are installed.

## Compilation of Dosbox

For compiling dosbox-wii with the current devkitPro version and the current lib versions use the fork version of dosbox-wii from retro100 or the version from dborth if the changes from retro100 are already applied at github repo dborth/dosbox-wii.

```
~$ git clone https://github.com/retro100/dosbox-wii.git
```

or use the dborth/dosbox-wii repo (if the changes from retro100 are already applied at github)

```
~$ git clone https://github.com/dborth/dosbox-wii.git
```

Setup devkitPro Development Environment for compiling a Wii project. (if not already done)

```
~$ source /etc/profile.d/devkit-env.sh
```

Go into the dosbox-wii folder and compile the Dosbox with the commando make

```
~/dosbox-wii$ make
```

If the compilation is successful the file `dosbox-wii.dol` is created.

Now `dosbox-wii.dol` can be executed on the Wii. The easiest way is to replace the `boot.dol` file of an existing dosbox-wii installation (sd:/apps/dosbox-wii) with this file. In this case the name `dosbox-wii.dol` must also be renamed to `boot.dol` accordingly.

If you want do compile Dosbox with older devkitPro and lib versions (like the dosbox-wii 1.7 version was compiled) then open the Makefile and change `LIBS_VER:=CUR` to `LIBS_VER:=OLD`. This would only work if you really have the old versions installed. If you change this use `make clean`  after changing.
