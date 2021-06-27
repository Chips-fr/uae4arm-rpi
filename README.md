# uae4arm-rpi
Port of uae4arm on Raspberry Pi

How to compile for raspberry:

   Retrieve the source of this emulator:

      git clone https://github.com/Chips-fr/uae4arm-rpi
      cd uae4arm-rpi

   Install following packages:

      sudo apt-get install libsdl1.2-dev 
      sudo apt-get install libguichan-dev
      sudo apt-get install libsdl-ttf2.0-dev
      sudo apt-get install libsdl-gfx1.2-dev
      sudo apt-get install libxml2-dev
      sudo apt-get install libflac-dev
      sudo apt-get install libmpg123-dev
      sudo apt-get install libmpeg2-4-dev
      sudo apt-get install autoconf

   Then for Raspberry Pi 2 & 3:  

      make

   Or for Raspberry Pi 1:  

      make PLATFORM=rpi1

For all ARM boards with OpenGLES:

   Install same packages as for raspberry.

   Install development package for your OpenGLES board.

   Ex for Mali:

      sudo apt-get install libmali-sunxi-dev

   Then compile the OpenGLES target:

      make PLATFORM=gles

# uae4arm-libretro core

Early port.

How to compile (ex for Raspberry Pi 2):

     make -f Makefile.libretro platform=rpi2

Then copy the core to retroarch core directory:

     cp uae4arm_libretro.so ~/.config/retroarch/cores/

## Controls

|RetroPad button|Action|
|---|---|
|B|Fire button 1 / Red|
|A|Fire button 2 / Blue|
|L2|Left mouse button|
|R2|Right mouse button|
|L|Switch to previous disk|
|R|Switch to next disk|
|Select|Toggle virtual keyboard|
|Start|Toggle mouse emulation|

Right analog stick controls the mouse.

In mouse emulation dpad and fire buttons controls the mouse.

Two joysticks support. Switch automatically between mouse or second joystick when a mouse or 2nd joystick button is pressed.

L & R button can change DF0: current disk for multiple disk roms. Each disk should be named with "(Disk x of y)"

Kickstarts supported:

|System|Version|Filename|Size|MD5|
|---|---|---|---|---|
|A500|KS v1.3 rev 34.005|**kick34005.A500**|262 144|82a21c1890cae844b3df741f2762d48d|
|A600|KS v3.1 rev 40.063|**kick40063.A600**|524 288|e40a5dfb3d017ba8779faba30cbd1c8e|
|A1200|KS v3.1 rev 40.068|**kick40068.A1200**|524 288|646773759326fbac3b2311fd8c8793ee|
|CDTV|Extended ROM v1.00|**kick34005.CDTV**|262 144|89da1838a24460e4b93f4f0c5d92d48d|
|CD32|KS v3.1 rev 40.060|**kick40060.CD32**|524 288|5f8924d013dd57a89cf349f4cdedc6b1|
|CD32|Extended rev 40.060|**kick40060.CD32.ext**|524 288|bb72565701b1b6faece07d68ea5da639|

