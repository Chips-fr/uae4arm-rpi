PREFIX	=/usr

#SDL_BASE = $(PREFIX)/bin/
SDL_BASE = 

NAME   = uae4arm
O      = o
RM     = rm -f
CXX    = g++-4.8
STRIP  = strip
#AS     = as

PROG   = $(NAME)

all: $(PROG)

PANDORA=1

DEFAULT_CFLAGS = `$(SDL_BASE)sdl-config --cflags`
LDFLAGS = -lSDL -lpthread  -lz -lSDL_image -lpng -lrt

MORE_CFLAGS += -DGP2X -DPANDORA -DDOUBLEBUFFER -DARMV6_ASSEMBLY -DUSE_ARMNEON -DRASPBERRY -DSIX_AXIS_WORKAROUND
MORE_CFLAGS += -DSUPPORT_THREADS -DUAE_FILESYS_THREADS -DNO_MAIN_IN_MAIN_C -DFILESYS -DAUTOCONFIG -DSAVESTATE -DPICASSO96
MORE_CFLAGS += -DDONT_PARSE_CMDLINE
#MORE_CFLAGS += -DWITH_LOGGING
MORE_CFLAGS += -I/opt/vc/include -I/opt/vc/include/interface/vmcs_host/linux -I/opt/vc/include/interface/vcos/pthreads

MORE_CFLAGS += -DJIT -DCPU_arm -DARM_ASSEMBLY

MORE_CFLAGS += -Isrc -Isrc/od-pandora -Isrc/gp2x -Isrc/threaddep -Isrc/menu -Isrc/include -Isrc/gp2x/menu -fomit-frame-pointer -Wno-unused -Wno-format -DUSE_SDL -DGCCCONSTFUNC="__attribute__((const))" -DUSE_UNDERSCORE -DUNALIGNED_PROFITABLE -DOPTIMIZED_FLAGS
LDFLAGS +=  -lSDL_ttf -lguichan_sdl -lguichan -lbcm_host -L/opt/vc/lib 
MORE_CFLAGS += -fexceptions -fpermissive


MORE_CFLAGS += -DROM_PATH_PREFIX=\"./\" -DDATA_PREFIX=\"./data/\" -DSAVE_PREFIX=\"./saves/\"

MORE_CFLAGS += -mhard-float -ffast-math -mfpu=neon
ifndef DEBUG
MORE_CFLAGS += -O3
MORE_CFLAGS += -fstrict-aliasing
MORE_CFLAGS += -fweb -frename-registers -fomit-frame-pointer
#MORE_CFLAGS += -falign-functions=32 -falign-loops -falign-labels -falign-jumps
MORE_CFLAGS += -finline -finline-functions -fno-builtin
#MORE_CFLAGS += -S
else
MORE_CFLAGS += -ggdb
endif

ASFLAGS += -mfloat-abi=hard -mfpu=neon

CFLAGS  = $(DEFAULT_CFLAGS) $(MORE_CFLAGS)
CFLAGS+= -DCPUEMU_0 -DCPUEMU_11 -DFPUEMU

OBJS =	\
	src/audio.o \
	src/autoconf.o \
	src/blitfunc.o \
	src/blittable.o \
	src/blitter.o \
	src/cfgfile.o \
	src/cia.o \
	src/crc32.o \
	src/custom.o \
	src/disk.o \
	src/drawing.o \
	src/ersatz.o \
	src/expansion.o \
	src/filesys.o \
	src/fpp.o \
	src/fsdb.o \
	src/fsdb_unix.o \
	src/fsusage.o \
	src/gfxutil.o \
	src/hardfile.o \
	src/inputdevice.o \
	src/keybuf.o \
	src/main.o \
	src/memory.o \
	src/missing.o \
	src/native2amiga.o \
	src/savestate.o \
	src/scsi-none.o \
	src/traps.o \
	src/uaelib.o \
	src/zfile.o \
	src/zfile_archive.o \
	src/archivers/7z/7zAlloc.o \
	src/archivers/7z/7zBuffer.o \
	src/archivers/7z/7zCrc.o \
	src/archivers/7z/7zDecode.o \
	src/archivers/7z/7zExtract.o \
	src/archivers/7z/7zHeader.o \
	src/archivers/7z/7zIn.o \
	src/archivers/7z/7zItem.o \
	src/archivers/7z/7zMethodID.o \
	src/archivers/7z/LzmaDecode.o \
	src/archivers/dms/crc_csum.o \
	src/archivers/dms/getbits.o \
	src/archivers/dms/maketbl.o \
	src/archivers/dms/pfile.o \
	src/archivers/dms/tables.o \
	src/archivers/dms/u_deep.o \
	src/archivers/dms/u_heavy.o \
	src/archivers/dms/u_init.o \
	src/archivers/dms/u_medium.o \
	src/archivers/dms/u_quick.o \
	src/archivers/dms/u_rle.o \
	src/archivers/wrp/warp.o \
	src/archivers/zip/unzip.o \
	src/md-pandora/support.o \
	src/od-pandora/neon_helper.o \
	src/od-pandora/fsdb_host.o \
	src/od-pandora/joystick.o \
	src/od-pandora/keyboard.o \
	src/od-pandora/inputmode.o \
	src/od-pandora/picasso96.o \
	src/od-pandora/writelog.o \
	src/od-pandora/pandora.o \
	src/od-pandora/pandora_filesys.o \
	src/od-pandora/pandora_gui.o \
	src/od-rasp/rasp_gfx.o \
	src/od-pandora/pandora_mem.o \
	src/od-pandora/sigsegv_handler.o \
	src/od-pandora/menu/menu_config.o \
	src/sd-sdl/sound_sdl_new.o \
	src/od-pandora/gui/UaeRadioButton.o \
	src/od-pandora/gui/UaeDropDown.o \
	src/od-pandora/gui/UaeCheckBox.o \
	src/od-pandora/gui/UaeListBox.o \
	src/od-pandora/gui/InGameMessage.o \
	src/od-pandora/gui/SelectorEntry.o \
	src/od-pandora/gui/ShowMessage.o \
	src/od-pandora/gui/SelectFolder.o \
	src/od-pandora/gui/SelectFile.o \
	src/od-pandora/gui/EditFilesysVirtual.o \
	src/od-pandora/gui/EditFilesysHardfile.o \
	src/od-pandora/gui/PanelPaths.o \
	src/od-pandora/gui/PanelConfig.o \
	src/od-pandora/gui/PanelCPU.o \
	src/od-pandora/gui/PanelChipset.o \
	src/od-pandora/gui/PanelROM.o \
	src/od-pandora/gui/PanelRAM.o \
	src/od-pandora/gui/PanelFloppy.o \
	src/od-pandora/gui/PanelHD.o \
	src/od-pandora/gui/PanelDisplay.o \
	src/od-pandora/gui/PanelSound.o \
	src/od-pandora/gui/PanelInput.o \
	src/od-pandora/gui/PanelMisc.o \
	src/od-pandora/gui/PanelSavestate.o \
	src/od-pandora/gui/main_window.o \
	src/od-pandora/gui/Navigation.o
ifdef PANDORA
OBJS += src/od-pandora/gui/sdltruetypefont.o
endif

OBJS += src/newcpu.o
OBJS += src/readcpu.o
OBJS += src/cpudefs.o
OBJS += src/cpustbl.o
OBJS += src/cpuemu_0.o
OBJS += src/cpuemu_11.o
OBJS += src/compemu.o
OBJS += src/compemu_fpp.o
OBJS += src/compstbl.o
OBJS += src/compemu_support.o

CPPFLAGS  = $(CFLAGS)

src/osdep/neon_helper.o: src/osdep/neon_helper.s
	$(CXX) -O3 -pipe -march=armv7-a -mcpu=cortex-a8 -mtune=cortex-a8 -mfpu=neon -mfloat-abi=hard -Wall -o src/osdep/neon_helper.o -c src/osdep/neon_helper.s

$(PROG): $(OBJS)
	$(CXX) $(CFLAGS) -o $(PROG) $(OBJS) $(LDFLAGS)

ifndef DEBUG
	$(STRIP) $(PROG)
endif


clean:
	$(RM) $(PROG) $(OBJS)
