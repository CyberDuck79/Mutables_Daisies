# Project Name
TARGET = clouds_daisy

# Sources
CPP_SOURCES = main.cpp clouds_port.cpp

# Enable FatFS for SD card preset storage
USE_FATFS = 1

# Clouds DSP sources directory
CLOUDS_DIR = ../eurorack/clouds/dsp

# STMLib (Mutable's library)
STMLIB_DIR = ../eurorack/stmlib

# Library locations
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

# Use bootloader to load from QSPI flash (8MB)
APP_TYPE = BOOT_QSPI

# Clouds .cc sources
CLOUDS_CC_SOURCES = \
	$(CLOUDS_DIR)/granular_processor.cc \
	$(CLOUDS_DIR)/correlator.cc \
	$(CLOUDS_DIR)/pvoc/phase_vocoder.cc \
	$(CLOUDS_DIR)/pvoc/stft.cc \
	$(CLOUDS_DIR)/pvoc/frame_transformation.cc \
	$(CLOUDS_DIR)/mu_law.cc \
	../eurorack/clouds/resources.cc \
	$(STMLIB_DIR)/utils/random.cc \
	$(STMLIB_DIR)/dsp/units.cc \
	$(STMLIB_DIR)/dsp/atan.cc

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Include paths
# -I.. comes BEFORE -I../eurorack so clouds/drivers/debug_pin.h shadows the eurorack version
C_INCLUDES += \
	-I. \
	-I.. \
	-I../eurorack \
	-I../common

# Define CLOUDS_DAISY for platform-specific code
C_DEFS += -DCLOUDS_DAISY

# Build configuration flags:
#   make                    -> Debug build with logging enabled
#   make RELEASE=1          -> Release build with logging disabled

ifdef RELEASE
    C_DEFS += -DNDEBUG
else
    C_DEFS += -DDEBUG_LOGGING
endif

# Compiler flags
CPP_STANDARD = -std=gnu++17
OPT = -O2

CPPFLAGS += -funroll-loops -finline -finline-functions-called-once

# Add .cc source handling
CC_OBJ = $(addprefix $(BUILD_DIR)/,$(notdir $(CLOUDS_CC_SOURCES:.cc=.o)))
vpath %.cc $(sort $(dir $(CLOUDS_CC_SOURCES)))

# Compile .cc files with C++ compiler
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CPP_STANDARD) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cc=.lst)) $< -o $@

# Override link step to include .cc objects
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(CC_OBJ) $(LIBDAISY_DIR)/build/libdaisy.a
	$(CXX) $(CC_OBJ) $(OBJECTS) $(LDFLAGS) -o $@
