#---------------------------------------------------------------------------------
# Silent Hill 3DS — Old 3DS homebrew shell
#---------------------------------------------------------------------------------
ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
TARGET		:=	silent-hill-3ds
BUILD		:=	build
SOURCES		:=	source source/platform source/game source/sound source/sound/libsd
INCLUDES	:=	include
ROMFS		:=	romfs

APP_TITLE		:=	Silent Hill 3DS
APP_DESCRIPTION	:=	Native Old 3DS port shell
APP_AUTHOR		:=	SH decomp port

#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__ -DSH3DS_TARGET_OLD3DS=1

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lctru -lm

#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(PORTLIBS)

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
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES_SOURCES	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN	:=	$(PICAFILES:.v.pica=.shbin.o)
export OFILES		:=	$(OFILES_BIN) $(OFILES_SOURCES)
export HFILES		:=	$(PICAFILES:.v.pica=_shbin.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh

ifneq ($(ROMFS),)
export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD) $(DEPSDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

$(DEPSDIR):
	@mkdir -p $@

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
ifeq ($(strip $(NO_SMDH)),)
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(OUTPUT).smdh
else
$(OUTPUT).3dsx	:	$(OUTPUT).elf
endif

ROMFS_DEPS := $(shell find $(TOPDIR)/$(ROMFS) -type f -o -type d)
$(OUTPUT).3dsx : $(ROMFS_DEPS)

$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SOURCES) : $(HFILES)

.PRECIOUS	:	%.shbin
%.shbin.o %_shbin.h : %.shbin
	@echo $(notdir $<)
	@$(bin2o)

$(OUTPUT).smdh	:	$(APP_ICON)
ifeq ($(strip $(APP_ICON)),)
	@echo "No icon.png — creating SMDH with default icon"
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(DEVKITPRO)/libctru/default_icon.png $@
else
	@smdhtool --create "$(APP_TITLE)" "$(APP_DESCRIPTION)" "$(APP_AUTHOR)" $(APP_ICON) $@
endif

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
