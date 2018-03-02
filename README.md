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

