
PLATFLAGS :=

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
	platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
	platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
	platform = win
else ifneq ($(findstring arm,$(shell uname -a)),)
	PLATFLAGS +=  -DARM  -marm
endif
endif

TARGET_NAME := uae4arm

CORE_DIR  := .
ROOT_DIR  := .

	# Default (ARM) unix
ifneq (,$(findstring unix,$(platform)))
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	LDFLAGS := -lz -lpthread
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T 
   	ifneq (,$(findstring neon,$(platform)))
		PLATFORM_DEFINES += -mfpu=neon -march=armv7-a
		CFLAGS += $(PLATFORM_DEFINES)
		HAVE_NEON = 1
		CPU_FLAGS +=  -marm
	endif
else ifeq ($(platform), crosspi)
   	TARGET := $(TARGET_NAME)_libretro.so
   	fpic = -fPIC
   	SHARED :=-shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined -L ../usr/lib -static-libstdc++ -static-libgcc
	PLATFORM_DEFINES += -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	CPU_FLAGS +=  -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -D__arm__

	PLATFORM_DEFINES += -I ../usr/include -DWITH_LOGGING
	HAVE_NEON = 1
	USE_PICASSO96 = 1
	CFLAGS += $(PLATFORM_DEFINES)
	CXXFLAGS += $(PLATFORM_DEFINES)
   	CC = arm-linux-gnueabihf-gcc
   	CXX = arm-linux-gnueabihf-g++ 
	LDFLAGS += -lz -lpthread
else ifeq ($(platform), crossand)
   	TARGET := $(TARGET_NAME)_libretro.so
   	fpic = -fPIC
   	SHARED :=-shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined -L ../usr/lib -static-libstdc++ -static-libgcc  
	PLATFORM_DEFINES +=  -marm  -march=armv7-a -mfloat-abi=softfp -mfpu=neon
        CPU_FLAGS += -marm  -march=armv7-a -mfloat-abi=softfp -mfpu=neon -D__arm__ 

	#-march=armv7-a -mfloat-abi=hard -mhard-float  
	#-mfpu=neon
	#-mfpu=neon-vfpv4 -mfloat-abi=hard

	PLATFORM_DEFINES +=  -DWITH_LOGGING
	HAVE_NEON = 1
	USE_PICASSO96 = 1
	CFLAGS += $(PLATFORM_DEFINES)
	CXXFLAGS += $(PLATFORM_DEFINES)
	CC = arm-linux-androideabi-gcc
	CXX =arm-linux-androideabi-g++ 
	AR = @arm-linux-androideabi-ar
	LD = @arm-linux-androideabi-g++ 
	LDFLAGS += -lz -llog

	# Raspberry Pi 2 and above
else ifeq ($(platform), rpi2)
    	TARGET := $(TARGET_NAME)_libretro.so
   	fpic = -fPIC
   	SHARED :=-shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
	PLATFORM_DEFINES += -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
	CPU_FLAGS +=  -marm -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -D__arm__

	PLATFORM_DEFINES +=  -DWITH_LOGGING
	HAVE_NEON = 1
	USE_PICASSO96 = 1
	CFLAGS += $(PLATFORM_DEFINES)
	CXXFLAGS += $(PLATFORM_DEFINES)
   	CC = gcc
   	CXX = g++ 
	LDFLAGS += -lz -lpthread

        #LDFLAGS   += -fsanitize=address -fsanitize=bounds 
        #CPU_FLAGS += -fsanitize=address -fsanitize=bounds 

	# Raspberry Pi 0 & 1
else ifeq ($(platform), rpi) 
	TARGET := $(TARGET_NAME)_libretro.so
	fpic := -fPIC
	LDFLAGS := -lz -lpthread
	PLATFLAGS +=  -DARM  -marm
	SHARED := -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T 

	# Sony Ps vita
else ifeq ($(platform), vita)
	TARGET := $(TARGET_NAME)_libretro_vita.a
	CC = arm-vita-eabi-gcc
	CXX = arm-vita-eabi-g++
	AR = arm-vita-eabi-ar
	PLATFLAGS += -marm -mfpu=neon -mtune=cortex-a9 -mfloat-abi=hard -ffast-math
	PLATFLAGS += -fno-asynchronous-unwind-tables -funroll-loops
	PLATFLAGS += -mword-relocations -fno-unwind-tables -fno-optimize-sibling-calls
	PLATFLAGS += -mvectorize-with-neon-quad -funsafe-math-optimizations
	PLATFLAGS += -mlittle-endian -munaligned-access
	PLATFLAGS += -D__arm__ -DVITA 
	HAVE_NEON = 1
	USE_PICASSO96 = 1
	PLATFLAGS += -U__INT32_TYPE__ -U __UINT32_TYPE__ -D__INT32_TYPE__=int
	PLATFLAGS += -DHAVE_STRTOUL -DVITA -DWITH_LOGGING
	CFLAGS += $(PLATFLAGS) 
	CXXFLAGS += $(PLATFLAGS) 
	STATIC_LINKING = 1
else ifeq ($(platform), osx)
	TARGET := $(TARGET_NAME)_libretro.dylib
	fpic := -fPIC -mmacosx-version-min=10.6
	SHARED := -dynamiclib
else ifeq ($(platform), android)
	CC = arm-linux-androideabi-gcc
	CXX =arm-linux-androideabi-g++
	AR = @arm-linux-androideabi-ar
	LD = @arm-linux-androideabi-g++ 
	TARGET := $(TARGET_NAME)_libretro_android.so
	fpic := -fPIC
	LDFLAGS := -lz -lm -llog
	SHARED :=  -Wl,--fix-cortex-a8 -shared -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined
	PLATFLAGS += -DWITH_LOGGING -DANDROID -DAND -DANDPORT -DARM_OPT_TEST=1
else ifeq ($(platform), wii)
	TARGET := $(TARGET_NAME)_libretro_wii.a
	CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
	AR = $(DEVKITPPC)/bin/powerpc-eabi-ar$(EXE_EXT)   
	ZLIB_DIR = $(LIBUTILS)/zlib/
	CFLAGS += -DSDL_BYTEORDER=SDL_BIG_ENDIAN -DMSB_FIRST -DBYTE_ORDER=BIG_ENDIAN  -DBYTE_ORDER=BIG_ENDIAN \
	-DWIIPORT=1 -DHAVE_MEMALIGN -DHAVE_ASPRINTF -I$(ZLIB_DIR) -I$(DEVKITPRO)/libogc/include \
	-D__powerpc__ -D__POWERPC__ -DGEKKO -DHW_RVL -mrvl -mcpu=750 -meabi -mhard-float -D__ppc__
	LDFLAGS :=   -lm -lpthread -lc
	PLATFLAGS += -DWIIPORT
else ifeq ($(platform), ps3)
	TARGET := $(TARGET_NAME)_libretro_ps3.a
	CC = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-gcc.exe
	AR = $(CELL_SDK)/host-win32/ppu/bin/ppu-lv2-ar.exe
	ZLIB_DIR = $(LIBUTILS)/zlib/
	LDFLAGS :=   -lm -lpthread -lc
	CFLAGS += -DSDL_BYTEORDER=SDL_BIG_ENDIAN -DMSB_FIRST -DBYTE_ORDER=BIG_ENDIAN  -DBYTE_ORDER=BIG_ENDIAN \
	-D__CELLOS_LV2 -DPS3PORT=1 -DHAVE_MEMALIGN -DHAVE_ASPRINTF -I$(ZLIB_DIR)  
else

ifeq ($(subplatform), 32)
	CC = i586-mingw32msvc-gcc
else
	CC = x86_64-w64-mingw32-gcc
	CFLAGS += -fno-aggressive-loop-optimizations
endif
	PLATFLAGS += -DWIN32PORT -DWIN32
	TARGET := $(TARGET_NAME)_libretro.dll
	fpic := -fPIC
	SHARED := -shared -static-libgcc -s -Wl,--version-script=$(CORE_DIR)/libretro/link.T -Wl,--no-undefined 
	LDFLAGS := -lm -lz
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g
else
	CFLAGS += -O3 -fomit-frame-pointer -finline -fno-builtin
endif


DEFS += -DCPU_arm -DARM_ASSEMBLY -DARMV6_ASSEMBLY -DPANDORA

ifeq ($(platform), vita)
DEFS += -DROM_PATH_PREFIX=\"ux0:/data/retroarch/system/uae4arm/\" -DDATA_PREFIX=\"ux0:/data/retroarch/system/uae4arm/data/\" -DSAVE_PREFIX=\"ux0:/data/retroarch/system/uae4arm/saves/\"
else
DEFS += -DROM_PATH_PREFIX=\"./\" -DDATA_PREFIX=\"./data/\" -DSAVE_PREFIX=\"./saves/\"
endif
DEFS += -DRASPBERRY
#-DANDROIDSDL 

ifeq ($(USE_PICASSO96), 1)
	DEFS += -DPICASSO96
endif

ifeq ($(HAVE_NEON), 1)
	DEFS += -DUSE_ARMNEON
endif

DEFINES += -D__LIBRETRO__ $(DEFS)
#-std=gnu99 
CFLAGS += $(DEFINES) -DINLINE="inline" $(CPU_FLAGS) -fexceptions -fpermissive

include Makefile.common

OBJECTS += $(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o) $(SOURCES_ASM:.S=.o)

INCDIRS := $(EXTRA_INCLUDES) $(INCFLAGS)


OBJECTS += $(OBJS)

all: $(TARGET)

ifeq ($(platform), wii)
$(TARGET): $(OBJECTS) 
	$(AR) rcs $@ $(OBJECTS) 
else ifeq ($(platform), ps3)
$(TARGET): $(OBJECTS) 
	$(AR) rcs $@ $(OBJECTS) 
else ifeq ($(platform), vita)
$(TARGET): $(OBJECTS) 
	$(AR) -cr $@ $(OBJECTS) 
else ifeq ($(platform), win)
$(TARGET): $(OBJECTS)
	$(CC) $(fpic) $(SHARED) $(INCDIRS) -o $@ $(OBJECTS) $(LDFLAGS)
else
$(TARGET): $(OBJECTS)
	$(CXX) $(fpic) $(SHARED) $(INCDIRS) -o $@ $(OBJECTS) $(LDFLAGS)

endif

ifeq ($(platform), vita)
$(LIBRETRO)/osdep/neon_helper.o: $(LIBRETRO)/osdep/neon_helper.s
	$(CXX) $(CFLAGS)  $(PLATFLAGS) -Wall -o $(LIBRETRO)/osdep/neon_helper.o -c $(LIBRETRO)/osdep/neon_helper.s
else
ifeq ($(HAVE_NEON), 1)
$(LIBRETRO)/osdep/neon_helper.o: $(LIBRETRO)/osdep/neon_helper.s
	$(CXX) $(CPU_FLAGS) $(PLATFORM_DEFINES) -Wall -o $(LIBRETRO)/osdep/neon_helper.o -c $(LIBRETRO)/osdep/neon_helper.s
	echo $(OBJS)
endif
endif

%.o: %.cpp
	$(CXX) $(fpic) $(CFLAGS) $(PLATFLAGS) $(INCDIRS) -c -o $@ $<

%.o: %.c
	$(CC) $(fpic) $(CFLAGS) $(PLATFLAGS) $(INCDIRS) -c -o $@ $<

%.o: %.S
	$(CC_AS) $(CFLAGS)  $(PLATFLAGS) -c $^ -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) 

.PHONY: clean

