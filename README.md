grackle
=======

Grackle is a dumper and play for adventures written using the Graphics Adventure toolkit. At the moment it supports games from the following systems:
* Spectrum (in .sna format)
* CPC (as extracted files with the AMSDOS header from a disk image or as a .sna)
* BBC Micro (as the raw datafile extracted from the disc image, usually called $.DATA)
* C64 (as a .sna file)

Both of these are the runnable versions of the files, rather than the raw data files. The BBC Micro datafile works both in the native interpreter and runner. BBC Micro games aren't quite working yet; the high profile conditions don't seem to quite work.


Command line syntax:

```grackle [-p|-l] [-s room] file```

-p = play game (default)
-l = list game
-s = provide a start room

You'll probably want to save the dump to a file, this can easily be done via
the > output operator (in both Linux and Windows), see the last example below.

For example:
```grackle "Winter Wonderland.sna"
grackle ransom.sna dump
grackle 12lost
grackle $.DATA >ransom.txt
```
The are probably lots of bugs.

Current LOAD, SAVE and wait for a key press are unsupported.

grackle-draw.c is included in this version, this is my basic messing code to
redraw the graphics on the BBC Micro from the graphics data file (usually
$.GDATA). The BBC Micro used a different format than other platforms. The image
drawing code is very basic and there will be errors in the output.
