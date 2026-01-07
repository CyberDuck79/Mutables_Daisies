# Project Name
TARGET = plaits_daisy

# Sources
CPP_SOURCES = main.cpp plaits_port.cpp

# Plaits DSP sources directory
PLAITS_DIR = ../eurorack/plaits/dsp

# STMLib (Mutable's library)
STMLIB_DIR = ../eurorack/stmlib

# Library Locations
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

# Use bootloader to load from QSPI flash (8MB)
APP_TYPE = BOOT_QSPI

# Plaits .cc sources (will be handled separately)
PLAITS_CC_SOURCES = \
	$(PLAITS_DIR)/voice.cc \
	$(PLAITS_DIR)/chords/chord_bank.cc \
	$(PLAITS_DIR)/fm/algorithms.cc \
	$(PLAITS_DIR)/fm/dx_units.cc \
	$(PLAITS_DIR)/engine/additive_engine.cc \
	$(PLAITS_DIR)/engine/bass_drum_engine.cc \
	$(PLAITS_DIR)/engine/chord_engine.cc \
	$(PLAITS_DIR)/engine/fm_engine.cc \
	$(PLAITS_DIR)/engine/grain_engine.cc \
	$(PLAITS_DIR)/engine/hi_hat_engine.cc \
	$(PLAITS_DIR)/engine/modal_engine.cc \
	$(PLAITS_DIR)/engine/noise_engine.cc \
	$(PLAITS_DIR)/engine/particle_engine.cc \
	$(PLAITS_DIR)/engine/snare_drum_engine.cc \
	$(PLAITS_DIR)/engine/speech_engine.cc \
	$(PLAITS_DIR)/engine/string_engine.cc \
	$(PLAITS_DIR)/engine/swarm_engine.cc \
	$(PLAITS_DIR)/engine/virtual_analog_engine.cc \
	$(PLAITS_DIR)/engine/waveshaping_engine.cc \
	$(PLAITS_DIR)/engine/wavetable_engine.cc \
	$(PLAITS_DIR)/engine2/chiptune_engine.cc \
	$(PLAITS_DIR)/engine2/phase_distortion_engine.cc \
	$(PLAITS_DIR)/engine2/six_op_engine.cc \
	$(PLAITS_DIR)/engine2/string_machine_engine.cc \
	$(PLAITS_DIR)/engine2/virtual_analog_vcf_engine.cc \
	$(PLAITS_DIR)/engine2/wave_terrain_engine.cc \
	$(PLAITS_DIR)/physical_modelling/modal_voice.cc \
	$(PLAITS_DIR)/physical_modelling/resonator.cc \
	$(PLAITS_DIR)/physical_modelling/string.cc \
	$(PLAITS_DIR)/physical_modelling/string_voice.cc \
	$(PLAITS_DIR)/speech/lpc_speech_synth.cc \
	$(PLAITS_DIR)/speech/lpc_speech_synth_controller.cc \
	$(PLAITS_DIR)/speech/lpc_speech_synth_phonemes.cc \
	$(PLAITS_DIR)/speech/lpc_speech_synth_words.cc \
	$(PLAITS_DIR)/speech/naive_speech_synth.cc \
	$(PLAITS_DIR)/speech/sam_speech_synth.cc \
	../eurorack/plaits/resources.cc \
	$(STMLIB_DIR)/utils/random.cc \
	$(STMLIB_DIR)/dsp/units.cc

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Include paths
C_INCLUDES += \
	-I. \
	-I../eurorack \
	-I../common

# Compiler flags for Plaits
CPP_STANDARD = -std=gnu++17
OPT = -O2

# Create a wrapper for user_data.h to avoid hardware-specific code
# This needs to be done before compiling
$(shell echo '#include "user_data_stub.h"' > plaits/user_data.h 2>/dev/null || true)

# Add .cc source handling
CC_OBJ = $(addprefix $(BUILD_DIR)/,$(notdir $(PLAITS_CC_SOURCES:.cc=.o)))
vpath %.cc $(sort $(dir $(PLAITS_CC_SOURCES)))

# Compile .cc files with C++ compiler
$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -c $(CPPFLAGS) $(CPP_STANDARD) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cc=.lst)) $< -o $@

# Override link step to include .cc objects
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(CC_OBJ) $(LIBDAISY_DIR)/build/libdaisy.a
	$(CXX) $(CC_OBJ) $(OBJECTS) $(LDFLAGS) -o $@
