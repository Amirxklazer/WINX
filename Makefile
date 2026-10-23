#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITPRO)/libnx/switch_rules

TARGET		:=	WinNX
BUILD		:=	build
SOURCES		:=	.
DATA		:=
INCLUDES	:=	.

APP_TITLE	:=	WinNX
APP_AUTHOR	:=	WinNX
APP_VERSION	:=	0.9.0

ARCH	:=	-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

# Optional features: only built in when their libraries are installed (build.yml installs them),
# so a missing library can never break the build.
PKGCONF	:=	$(PORTLIBS)/bin/aarch64-none-elf-pkg-config
OPTDEFS	:=
OPTLIBS	:=
ifneq ($(strip $(shell $(PKGCONF) --exists SDL2_mixer && echo yes)),)
OPTDEFS	+=	-DWINNX_AUDIO
OPTLIBS	+=	$(shell $(PKGCONF) --static --libs SDL2_mixer)
endif
ifneq ($(strip $(shell $(PKGCONF) --exists libcurl && echo yes)),)
OPTDEFS	+=	-DWINNX_NET
OPTLIBS	+=	$(shell $(PKGCONF) --static --libs libcurl)
endif

CFLAGS	:=	-g -Wall -O2 -ffunction-sections $(ARCH) $(DEFINES) $(OPTDEFS)
CFLAGS	+=	$(INCLUDE) -D__SWITCH__ $(shell sdl2-config --cflags 2>/dev/null)
CXXFLAGS	:= $(CFLAGS) -fno-rtti -std=gnu++17
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

PKGCONF	:=	$(PORTLIBS)/bin/aarch64-none-elf-pkg-config
TTFLIBS	:=	$(shell $(PKGCONF) --static --libs SDL2_ttf SDL2_image 2>/dev/null)
ifeq ($(strip $(TTFLIBS)),)
TTFLIBS	:=	-lSDL2_ttf -lSDL2_image -lfreetype -lbz2 -lpng -ljpeg -lz
endif
LIBS	:=	$(OPTLIBS) $(TTFLIBS) $(shell sdl2-config --libs 2>/dev/null) -lnx -lz

LIBDIRS	:=	$(PORTLIBS) $(LIBNX)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)
export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES		:=	$(OFILES_SRC)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# The hbmenu icon: must be a 256x256 baseline JPEG called icon.jpg next to this Makefile.
# The build now FAILS if it is missing instead of silently making an icon-less .nro.
APP_ICON := $(CURDIR)/icon.jpg
ifeq ($(wildcard $(APP_ICON)),)
$(error icon.jpg not found in $(CURDIR) - put icon.jpg (256x256 JPEG) in the repo root next to the Makefile)
endif
export NROFLAGS += --icon=$(APP_ICON) --nacp=$(CURDIR)/$(TARGET).nacp

.PHONY: $(BUILD) clean all

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).nro $(TARGET).nacp $(TARGET).elf

#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

all	:	$(OUTPUT).nro

$(OUTPUT).nro	:	$(OUTPUT).elf $(OUTPUT).nacp

$(OUTPUT).elf	:	$(OFILES)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
