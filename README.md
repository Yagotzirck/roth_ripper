# roth_ripper
An archive extractor tool for the DOS game Realms of the Haunting (1996).

There is an older version of this tool floating on the internet; I advise to forget about it and refer to this one instead,
since the older version had some bugs that got addressed here (e.g. DBASE300/208.tga had the wrong colors, and DBASE300.DAT wasn't extracted properly
if it belonged to a game version without .midi files, to name a few) and this is a bit more polished in general.</br>
There's probably more polishing/refactoring that needs to be done, but for now I'd say it can stay as it is.

## Usage
Create a directory and place the following files in it:

- roth_ripper.exe
- ICONS.ALL
- BACKDROP.RAW
- FXSCRIPT.SFX
- DBASE200.DAT
- DBASE300.DAT
- DBASE500.DAT
- ADEMO.DAS
- DEMO.DAS
- DEMO1.DAS
- DEMO2.DAS
- DEMO3.DAS
- DEMO4.DAS

Then simply launch the exe and a "ROTH_Resources" subdirectory should be created in the same directory where you've put all the files, which is going to contain all the extracted material.

If you're taking files from the game's CD, I'd suggest to take DATA/FX22.SFX and rename it to FXSCRIPT.SFX instead of taking DATA/FXSCRIPT.SFX;
they contain the same sounds, but some sounds in DATA/FX22.SFX are 22050 Hz while the sounds in DATA/FXSCRIPT.SFX are all 11025 Hz (lower quality).

## Extracted material
- Videos (.gdv files from DBASE300.DAT; go [here](https://www.realmsofthehaunting.com/downloads.html) to get a DOS .gdv player)
- Textures
- Sprites
- Various graphics (menus, HUDs, icons, etc)
- Palettes
- Sounds (including the speech audio files from DBASE500.DAT)
- Music

All picture-related material is extracted as .TGA pictures, which can be viewed fine with
IrfanView (albeit without transparency), SLADE3, MtPaint, and possibly other image viewers I'm unaware of;
if that's still a problem for you, Ken Silverman's PNGOUT is your friend :)

As for music, depending on the game version you're extracting you might have both .HMP files and their .midi counterparts, or just .HMPs;
in the latter case, foobar should play them just fine (see [here](http://www.vgmpf.com/Wiki/index.php?title=HMP) for more info).