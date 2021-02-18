# uae4arm-libretro

Based on old uae4arm version.

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

In mouse emulation mode dpad and fire button controls the mouse.

Two joysticks support. Switch automatically between mouse or second joystick when a mouse or 2nd joystick button is pressed.

L & R button can change DF0: current disk for multiple disk roms. Each disk should be named with "(Disk x of y)"

Automatic support of WHDload slave in .lha file. 

Kickstarts supported in options configuration:

|System|Version|Filename|Size|MD5|
|---|---|---|---|---|
|A500|KS v1.3 rev 34.005|**kick34005.A500**|262 144|82a21c1890cae844b3df741f2762d48d|
|A600|KS v3.1 rev 40.063|**kick40063.A600**|524 288|e40a5dfb3d017ba8779faba30cbd1c8e|
|A1200|KS v3.1 rev 40.068|**kick40068.A1200**|524 288|646773759326fbac3b2311fd8c8793ee|

If kickstart is not present, AROS kickstart replacement will be used.