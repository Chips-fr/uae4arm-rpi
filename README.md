# uae4arm-rpi
Port of uae4arm for Raspberry Pi and Libretro.

Compilation prerequisites:

   Retrieve the source code:

      git clone https://github.com/Chips-fr/uae4arm-rpi
      cd uae4arm-rpi

   Install compilation dependency packages:

      sudo apt-get install libsdl1.2-dev 
      sudo apt-get install libguichan-dev
      sudo apt-get install libsdl-ttf2.0-dev
      sudo apt-get install libsdl-gfx1.2-dev
      sudo apt-get install libxml2-dev
      sudo apt-get install libflac-dev
      sudo apt-get install libmpg123-dev
      sudo apt-get install libmpeg2-4-dev
      sudo apt-get install autoconf

# Standalone emulator compilation:

   Use the PLATFORM definition to specify the target, display backend, and optional features.

   Available targets:

|Target string|Description|
|---|---|
|rpi1|Raspberry Pi 1|
|rpi2|Raspberry Pi 2 and newer in 32-bit mode|
|rpi64bits|Raspberry Pi in 64-bit mode|
|arm32|Generic ARM 32-bit target|
|arm64|Generic ARM 64-bit target|

   Available display backends:

|Display backend string|Description|
|---|---|
|dispmanx|DispmanX display backend (only for older Raspbian / Raspberry Pi OS)|
|gles1|OpenGL ES 1.0 display backend|
|gles2|OpenGL ES 2.0 display backend|
|SDL|SDL display backend|

   Optional features:

|Optional feature string|Description|
|---|---|
|picasso96|Enable Picasso 96 support|
|neon|Enable NEON optimizations (arm32 target only)|

   Then combine theses strings to form the PLATFORM definition:


   Example for Raspberry Pi 5 in 64-bit mode with OpenGL ES 2.0:

          make PLATFORM=rpi64bits-gles2

   Example for generic ARM boards with OpenGL ES 1.0:

      Install development packages for your OpenGL ES 1.0 board.

       For example, for Mali:

          sudo apt-get install libmali-sunxi-dev

       Then compile the OpenGL ES 1.0 target:

          make PLATFORM=arm32-gles1



# Libretro core compilation:

   Use the platform definition to specify the target.

   Available targets:

|Target string|Description|
|---|---|
|rpi1|Raspberry Pi 1|
|rpi2|Raspberry Pi 2|
|rpi3|Raspberry Pi 3|
|rpi4|Raspberry Pi 4|
|unix|Generic ARM|
|classic_armv7_a7|SNES Classic, NES Classic and C64 Mini|
|classic_armv8_a35|PS Classic|

   Optional features:

|Optional feature string|Description|
|---|---|
|neon|Enable NEON optimizations (unix target only)|
|aarch64| Compile for a 64-bit target |

   WebOS targets are available using the CROSS_COMPILE definition:

|CROSS_COMPILE value|Description|
|---|---|
|webos| WebOS 32-bit target with NEON|
|webos-aarch64| WebOS 64-bit target|
|starfish| WebOS Starfish target (webOS 5.0+)|


For example, to build for Raspberry Pi 2:

     make -f Makefile.libretro platform=rpi2

Then copy the core to the RetroArch cores directory:

     cp uae4arm_libretro.so ~/.config/retroarch/cores/

## Libretro core controls:

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

The right analog stick controls the mouse.

In mouse emulation mode, the D-pad and fire buttons also control the mouse.

The core supports two joysticks and switches automatically between mouse mode and the second joystick when a mouse or second joystick button is pressed.

The L and R buttons cycle through disks for multi-disk ROMs (in DF0:). Each disk should include "(Disk x of y)" in its filename.

# Supported Kickstarts:

|System|Version|Filename|Size|MD5|
|---|---|---|---|---|
|A500|KS v1.3 rev 34.005|**kick34005.A500**|262 144|82a21c1890cae844b3df741f2762d48d|
|A600|KS v3.1 rev 40.063|**kick40063.A600**|524 288|e40a5dfb3d017ba8779faba30cbd1c8e|
|A1200|KS v3.1 rev 40.068|**kick40068.A1200**|524 288|646773759326fbac3b2311fd8c8793ee|
|CDTV|Extended ROM v1.00|**kick34005.CDTV**|262 144|89da1838a24460e4b93f4f0c5d92d48d|
|CD32|KS v3.1 rev 40.060|**kick40060.CD32**|524 288|5f8924d013dd57a89cf349f4cdedc6b1|
|CD32|Extended rev 40.060|**kick40060.CD32.ext**|524 288|bb72565701b1b6faece07d68ea5da639|
