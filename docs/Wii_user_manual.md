# Wii user manual

## General

You need a Wii Remote and a USB keyboard. The C: drive will automatically be
mounted to sd:/DOSBox/ if loaded from apps/dosbox-wii/. Otherwise, the 
directory the dol is loaded from will be mounted as C, and sd:/DOSBox as D 
(if present). The Z: driver is a virtual drive that is part of DOSBox. 

Other drives can be mounted using the MOUNT command. Prefix sd: for an SD card,
and usb: for a USB drive. The Wii's DVD drive and network folders can't be 
mounted at this time. 

The configuration file is loaded from the directory the dol is located in 
(if present) and sd:/DOSBox/dosbox.conf otherwise. This file will be created 
automatically after you start DOSBox. Please edit it with a text editor 
to choose settings appropriate for each game. 

Press the Home button, type "exit", press Ctrl+F9, or press Reset (on the 
console) to exit.

## Compatibility

Most games work properly now; however, many games will require some sort 
of adjustment to the dosbox.conf file. It is recommended to try the game 
you want to play on the PC version of DOSBox first to discover the optimum 
settings.
If you need multiple different dosbox configurations (e.g. one
configuration per game) then follow the section *multiple different dosbox configurations*.

## Using multiple different dosbox configurations

By default, Dosbox Wii uses the dosbox.conf configuration file from the app
directory if it exists. If Dosbox Wii is installed in the sd:/apps/dosbox-wii
directory, this is the sd:/apps/dosbox-wii/dosbox.conf configuration file.

If the dosbox.conf file does not exist in the app directory
(e.g. sd:/apps/dosbox-wii/dosbox.conf), Dosbox Wii will try to load the file
from the DOSBox root directory by default (e.g. sd:/DOSBox/dosbox.conf). If it
does not exist either, it will be created in the DOSBox root directory
(e.g. sd:/DOSBox/dosbox.conf).

To use different configurations for different DOS games for the Dosbox Wii,
you can simply use multiple Dosbox Wii installations.

1. The configuration file in the DOSBox root directory should not exist
(no sd:/DOSBox/dosbox.conf file). Best is to move the configuration file
from sd:/DOSBox/dosbox.conf to sd:/apps/dosbox-wii/dosbox.conf.
  Create desired number of copies of the directory sd:/apps/dosbox-wii.
For example, sd:/apps/dosbox-wii-game1, sd:/apps/dosbox-wii-game2,
sd:/apps/dosbox-wii-game3, etc. All Dosbox Wii installations (copies) share
the same DOSBox root directory (sd:/DOSBox), which is not a problem.

2. The directories (copies) sd:/apps/dosbox-wii, sd:/apps/dosbox-wii-game1,
sd:/apps/dosbox-wii-game2, etc. should each contain the files boot.dol,
dosbox.conf, icon.png and meta.xml.

3. Per Dosbox Wii installation (sd:/apps/dosbox-wii, sd:/apps/dosbox-wii-game1,
sd:/apps/dosbox-wii-game2, etc.) the title in the file meta.xml should be
adjusted in the XML tag `<name>`, so that the different installations are also
listed with different titles in the Homebrew Channel.

4. Different configurations can now be made per Dosbox Wii installation.
The configuration is to be made in the files sd:/apps/dosbox-wii/dosbox.conf,
sd:/apps/dosbox-wii-game1/dosbox.conf, sd:/apps/dosbox-wii-game2/dosbox.conf,
etc.. In the section `[autoexec]` you can also start the respective game.
If the game is terminated, it can also terminate the dosbox automatically.
For example, an entry for the game SC2000 could look like this:

   ```
   # ... other stuff ...
   
   [autoexec]
   C:
   cd games/sc2000
   sc2000.exe
   exit
   ```

   The individual configurations per Dosbox Wii installation can also use a
   different mapper per installation. Accordingly in the section `[sdl]` with
   **mapperfile =** *path-to-mapper-file*.

