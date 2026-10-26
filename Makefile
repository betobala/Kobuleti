RACK_DIR ?= ../Harmonion/dep/Rack-SDK

# The Batumi firmware runs at a fixed 16384 Hz; TEST disables the STM32 RAM sections in stmlib
FLAGS += -Isrc/batumi -DSAMPLE_RATE=16384 -DTEST
FLAGS += -Wno-narrowing
CFLAGS +=
CXXFLAGS +=

LDFLAGS +=

SOURCES += $(wildcard src/*.cpp) $(wildcard src/batumi/*.cc)

DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)

include $(RACK_DIR)/plugin.mk
