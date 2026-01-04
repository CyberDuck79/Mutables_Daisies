# Project Name
TARGET = plaits_daisy

# Library Locations (relative paths) - must be defined before use
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP
EURORACK_DIR = ../eurorack

# Use Daisy Patch SM (Patch.Init())
OPT = -Os

# Use QSPI Flash (requires bootloader) - Plaits is too large for internal flash
APP_TYPE = BOOT_SRAM

# Sources - Main application
CPP_SOURCES = plaits_daisy.cpp

# Mutable Instruments Plaits DSP Sources - explicitly listed
# Note: These use .cc extension - we need to rename or add rules
# For simplicity, let's use symbolic links or copy as .cpp

# Include paths for Mutable Instruments code
# Local plaits folder first (for our stub headers), then eurorack
C_INCLUDES += -I.
C_INCLUDES += -I$(EURORACK_DIR)

# Compiler flags for Mutable Instruments compatibility
CPPFLAGS += -DTEST=0

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Additional rule for .cc files (Mutable Instruments uses .cc extension)
PLAITS_CC_SOURCES = \
	$(EURORACK_DIR)/plaits/dsp/voice.cc \
	$(EURORACK_DIR)/plaits/dsp/chords/chord_bank.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/additive_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/bass_drum_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/chord_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/fm_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/grain_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/hi_hat_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/modal_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/noise_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/particle_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/snare_drum_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/speech_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/string_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/swarm_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/virtual_analog_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/waveshaping_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine/wavetable_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/chiptune_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/phase_distortion_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/six_op_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/string_machine_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/virtual_analog_vcf_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/engine2/wave_terrain_engine.cc \
	$(EURORACK_DIR)/plaits/dsp/fm/algorithms.cc \
	$(EURORACK_DIR)/plaits/dsp/fm/dx_units.cc \
	$(EURORACK_DIR)/plaits/dsp/physical_modelling/modal_voice.cc \
	$(EURORACK_DIR)/plaits/dsp/physical_modelling/resonator.cc \
	$(EURORACK_DIR)/plaits/dsp/physical_modelling/string.cc \
	$(EURORACK_DIR)/plaits/dsp/physical_modelling/string_voice.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/lpc_speech_synth.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/lpc_speech_synth_controller.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/lpc_speech_synth_phonemes.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/lpc_speech_synth_words.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/naive_speech_synth.cc \
	$(EURORACK_DIR)/plaits/dsp/speech/sam_speech_synth.cc \
	$(EURORACK_DIR)/plaits/resources.cc \
	$(EURORACK_DIR)/stmlib/dsp/atan.cc \
	$(EURORACK_DIR)/stmlib/dsp/units.cc \
	$(EURORACK_DIR)/stmlib/utils/random.cc

# Add .cc objects to build
CC_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(PLAITS_CC_SOURCES:.cc=.o)))
vpath %.cc $(sort $(dir $(PLAITS_CC_SOURCES)))

# Rule for .cc files
$(BUILD_DIR)/%.o: %.cc Makefile | $(BUILD_DIR)
	$(CXX) -c $(CPPFLAGS) $(CPP_STANDARD) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.cc=.lst)) $< -o $@

# Override the elf target to include .cc objects
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(CC_OBJECTS) Makefile
	$(CXX) $(OBJECTS) $(CC_OBJECTS) $(LDFLAGS) -o $@
