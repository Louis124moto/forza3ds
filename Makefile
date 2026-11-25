# Forza3DS — devkitARM / libctru / Citro3D
# Requires DEVKITPRO and DEVKITARM configured by devkitPro.

TARGET      := forza3ds
BUILD       := build
SOURCES     := source
DATA        :=
INCLUDES    := include

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections -fdata-sections $(ARCH) -D__3DS__
CFLAGS      += $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir))
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions
ASFLAGS     := $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -Wl,--gc-sections
LIBS        := -lcitro3d -lctru -lm

include $(DEVKITPRO)/libctru/3ds_rules
