#include "../common/constants.h"
#include "../common/controllers/encoder_controller.h"
#include "../common/cv_input.h"
#include "../common/cv_mapping_processor.h"
#include "../common/display.h"
#include "../common/midi_processor.h"
#include "../common/parameter.h"
#include "../common/preset_manager.h"
#include "../common/ui_state.h"
#include "cpu_monitor.h"
#include "daisy_patch.h"
#include "daisysp.h"
#include "logo_bitmap.h"
#include "plaits_port.h"
#include "user_data_manager.h"

using namespace daisy;
using namespace daisysp;
using namespace mutables_ui;
using namespace mutables_plaits;
using namespace mutables;

// Hardware
DaisyPatch hw;

// Module
PlaitsPort plaits_module;

// UI
MenuState menu;
Display display;
CVInputBank cv_inputs;
CVMappingProcessor cv_processor;
MIDIProcessor midi_processor;
EncoderController encoder_controller(cv_processor);

// SD Card / Presets / User Data
SdmmcHandler sdmmc;
FatFSInterface fsi;
PresetManager preset_manager;
UserDataManager user_data_manager;

// CPU Monitor
mutables::CpuMonitor cpu_monitor;

// Debug logger
daisy::Logger<daisy::LOGGER_INTERNAL> logger;
daisy::Logger<daisy::LOGGER_INTERNAL> *g_logger = &logger;

// Encoder state
bool encoder_button_last = false;
uint32_t encoder_press_time = 0;
const uint32_t LONG_PRESS_MS = 500;

// CC values storage (0.0 to 1.0)
float cc_values[128] = {0.0f};

// Sample-and-hold for Bank/Engine CV mapping
// When CV mapped with plugged=true, value is sampled on NoteOn
int bank_held_index = 0;
int engine_held_index = 0;
bool sample_hold_pending = false; // Flag set on NoteOn to trigger sampling

// File browser state for USER_DATA selection
static constexpr int MAX_USER_DATA_FILES = 32;
static char user_data_files[MAX_USER_DATA_FILES][32];
static int user_data_file_count = 0;

// Callback for file browser display
const char *GetUserDataFileNameCallback(int index) {
  if (index >= 0 && index < user_data_file_count) {
    return user_data_files[index];
  }
  return nullptr;
}

// Cached CV values (read once per block)
static float cached_cv_values_[4];

// Cached DAC values (write only when changed)
static uint16_t last_dac_1_ = 0;
static uint16_t last_dac_2_ = 0;

// Audio buffers
float *audio_in[4];
float *audio_out[4];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  // Start CPU measurement
  cpu_monitor.OnBlockStart();

  // Rebuild mapping cache if needed (happens when mappings change)
  // Rebuild mapping cache if needed (happens when mappings change)
  if (cv_processor.IsDirty()) {
    cv_processor.RebuildCache(plaits_module.GetParameters(),
                              plaits_module.GetParameterCount());
  }

  // Update CV inputs with raw ADC values (no pot scaling or processing)
  // This preserves precision for V/Oct and accurate offset capture
  // Invert values since ADC reads are inverted on Daisy Patch (0V = 1.0, 5V =
  // 0.0) Scale from hardware range (0.03-0.96) to full software range
  // (0.0-0.99)
  auto scale_adc = [](float raw) -> float {
    constexpr float kADCMin = 0.03f;
    constexpr float kADCMax = 0.96f;
    constexpr float kADCRange = kADCMax - kADCMin; // 0.93
    constexpr float kDeadzone =
        0.005f; // Values below 0.5% become true 0.0 (displays as 0.00)
    float scaled = (raw - kADCMin) / kADCRange * 0.99f;
    scaled = std::clamp(scaled, 0.0f, 0.99f);
    // Apply deadzone at bottom to ensure true 0.0 when knob is at minimum
    if (scaled < kDeadzone)
      scaled = 0.0f;
    return scaled;
  };

  float cv1 = scale_adc(1.0f - hw.controls[DaisyPatch::CTRL_1].GetRawFloat());
  float cv2 = scale_adc(1.0f - hw.controls[DaisyPatch::CTRL_2].GetRawFloat());
  float cv3 = scale_adc(1.0f - hw.controls[DaisyPatch::CTRL_3].GetRawFloat());
  float cv4 = scale_adc(1.0f - hw.controls[DaisyPatch::CTRL_4].GetRawFloat());

  cv_inputs.UpdateRawValues(cv1, cv2, cv3, cv4);

  // Cache filtered CV values (read once per block)
  cached_cv_values_[0] = cv_inputs.GetFiltered(0);
  cached_cv_values_[1] = cv_inputs.GetFiltered(1);
  cached_cv_values_[2] = cv_inputs.GetFiltered(2);
  cached_cv_values_[3] = cv_inputs.GetFiltered(3);

  auto params = plaits_module.GetParameters();

  // Configure S&H for Bank and Engine
  params[0].sample_and_hold = true;
  params[1].sample_and_hold = true;

  size_t param_count = plaits_module.GetParameterCount();

  // Calculate CV signals for Plaits modulation inputs
  // These are the raw CV signals (current - offset) that Plaits will attenuate
  float frequency_cv = 0.0f;
  float timbre_cv = 0.0f;
  float morph_cv = 0.0f;

  // Process CC-mapped parameters using processor
  cv_processor.ProcessCCMappings(midi_processor.GetCCValues(), kCVHysteresis);

  // Process CV-mapped parameters using processor
  // Detect Gate 1 Rising Edge for S&H Trigger (Original Plaits behavior)
  // Logic moved BEFORE processing to ensure zero-latency update
  static bool last_gate_state = false;
  bool gate_state = hw.gate_input[0].State();
  bool gate_trig = gate_state && !last_gate_state;
  last_gate_state = gate_state;

  // Combine with MIDI trigger (global pending flag)
  bool do_sample = gate_trig || sample_hold_pending;
  sample_hold_pending = false; // Clear global flag

  // Pass filtered CV values and trigger status
  cv_processor.ProcessCVMappings(cv_inputs, kCVHysteresis, do_sample);

  // Extract modulation signals for Plaits specific inputs
  // (Frequency/Timbre/Morph)
  if (params[2].mapping.plugged &&
      params[2].mapping.IsCVSource()) { // Frequency
    float cv = cv_inputs.GetFiltered(params[2].mapping.GetCVIndex());
    frequency_cv = cv - params[2].mapping.offset;
  }
  if (params[4].mapping.plugged && params[4].mapping.IsCVSource()) { // Timbre
    float cv = cv_inputs.GetFiltered(params[4].mapping.GetCVIndex());
    timbre_cv = cv - params[4].mapping.offset;
  }
  if (params[5].mapping.plugged && params[5].mapping.IsCVSource()) { // Morph
    float cv = cv_inputs.GetFiltered(params[5].mapping.GetCVIndex());
    morph_cv = cv - params[5].mapping.offset;
  }

  // Handle unmapped CV type parameters - set to 0
  for (size_t i = 0; i < param_count; i++) {
    if (params[i].type == ParamType::CV && !params[i].mapping.IsCVSource()) {
      params[i].SetNormalizedWithHysteresis(0.0f, kCVHysteresis);
    }
    // Also check children in SUB menus
    if (params[i].type == ParamType::SUB && params[i].children) {
      for (int j = 0; j < params[i].child_count; j++) {
        auto &child = params[i].children[j];
        if (child.type == ParamType::CV && !child.mapping.IsCVSource()) {
          child.SetNormalizedWithHysteresis(0.0f, kCVHysteresis);
        }
      }
    }
  }

  // Clear sample-and-hold pending flag after processing
  sample_hold_pending = false;

  // Pass CV modulation values to Plaits
  plaits_module.SetCVModulations(frequency_cv, timbre_cv, morph_cv);

  // Pass raw CV values for S&H source
  plaits_module.SetRawCVInputs(
      std::clamp(cv1, 0.0f, 1.0f), std::clamp(cv2, 0.0f, 1.0f),
      std::clamp(cv3, 0.0f, 1.0f), std::clamp(cv4, 0.0f, 1.0f));

  // Process gate inputs for module
  // Gate 1: Trigger input for AD envelopes (and MIDI note triggers)
  // Gate 2: Clock input for LFO sync
  bool gate1_state = hw.gate_input[0].State();
  bool gate2_state = hw.gate_input[1].State();
  plaits_module.ProcessGate(0, gate1_state);
  plaits_module.ProcessGate(1, gate2_state);

  // Setup audio pointers
  for (size_t i = 0; i < 4; i++) {
    audio_in[i] = (float *)in[i];
    audio_out[i] = out[i];
  }

  // Clear all outputs first
  for (size_t i = 0; i < size; i++) {
    out[0][i] = 0.0f;
    out[1][i] = 0.0f;
    out[2][i] = 0.0f;
    out[3][i] = 0.0f;
  }

  // Process audio
  plaits_module.Process(audio_in, audio_out, size);

  // Update sample counter for MIDI clock tracking
  plaits_module.UpdateSampleCounter(size);

  // Write CV modulator outputs to DAC (0-1 float -> 0-4095 DAC value)
  // Channel 1 = CV Out 1, Channel 2 = CV Out 2
  // Only write if values changed (reduce SPI traffic)
  float cv_out_1 = plaits_module.GetCVOutput(0);
  float cv_out_2 = plaits_module.GetCVOutput(1);
  uint16_t dac_1 =
      static_cast<uint16_t>(std::clamp(cv_out_1, 0.0f, 1.0f) * 4095.0f);
  uint16_t dac_2 =
      static_cast<uint16_t>(std::clamp(cv_out_2, 0.0f, 1.0f) * 4095.0f);

  if (dac_1 != last_dac_1_) {
    hw.seed.dac.WriteValue(DacHandle::Channel::ONE, dac_1);
    last_dac_1_ = dac_1;
  }
  if (dac_2 != last_dac_2_) {
    hw.seed.dac.WriteValue(DacHandle::Channel::TWO, dac_2);
    last_dac_2_ = dac_2;
  }

  // Write Gate Output
  hw.gate_output.Write(plaits_module.GetGateOutput());

  // End CPU measurement
  cpu_monitor.OnBlockEnd();
}

// Encoder Update Wrapper
void UpdateEncoder() {
  int inc = hw.encoder.Increment();
  bool pressed = hw.encoder.Pressed();
  bool rising = hw.encoder.RisingEdge();

  // Reuse static press_start logic locally
  static uint32_t press_start = 0;
  if (rising)
    press_start = System::GetNow();

  // If we want to pass duration to controller, we can:
  // But controller logic handles detection if we pass
  // released? Actually I removed the struct declaration too?
  // Let's restore the struct if Update uses it.

  EncoderHardwareState hw_state = {inc, pressed, rising, 0};

  // Logic from previous iteration to fix Short vs Long press
  // on Release
  static bool encoder_button_last = false;

  if (!pressed && encoder_button_last) {
    hw_state.press_duration = System::GetNow() - press_start;
  }
  encoder_button_last = pressed;

  // Pass to controller
  // Note: Controller's Update signature expects (hw_state,
  // menu, params...) BUT in Step 667, Update signature is:
  // Update(const EncoderHardwareState &hw, MenuState &menu,
  // ...) AND inside Update it calculates short/long press?
  // Wait, I updated Controller to calculate it itself in
  // theory? Let's check Controller Update in Step 667. It
  // says: "Detect short vs long press on release" "bool
  // just_released = !hw.pressed && last_pressed_;" So
  // Controller DOES IT ITSELF. SO main.cpp does NOT need to
  // calculate short_press/long_press? BUT main.cpp call site
  // might be passing them?
  // "encoder_controller.Update(hw_state, menu..."
  // If I pass hw_state, Controller handles logic.
  // BUT the user reported "saving not working".
  // And I claimed "main.cpp was passing both".

  // Let's look at `main.cpp` Usage in the Error Log (Step
  // 675): Line 299: encoder_controller.Update(hw_state,
  // menu, ...

  // If `EncoderController::Update` takes `hw_state`
  // (struct), then main.cpp logic for short/long is
  // REDUNDANT/IGNORED? In Step 667, Controller Update
  // signature is: void Update(const EncoderHardwareState
  // &hw, MenuState &menu, Parameter *params, size_t
  // param_count)

  // So I just need to construct hw_state correctly.
  // `hw_state.press_duration`?
  // Caller (main.cpp) calculates duration?
  // In Step 667, Update does:
  // "if (just_released) { if (hw.press_duration >= ...) }"
  // SO YES, main.cpp MUST calculate duration and put it in
  // hw_state!

  if (!pressed && encoder_button_last) {
    hw_state.press_duration = System::GetNow() - press_start;
  }

  encoder_controller.Update(hw_state, menu, plaits_module.GetParameters(),
                            plaits_module.GetParameterCount());
}

void ProcessMidi() {
  while (hw.midi.HasEvents()) {
    MidiEvent event = hw.midi.PopEvent();

    // Handle MIDI clock (system realtime, not
    // channel-dependent)
    if (event.type == SystemRealTime) {
      // MIDI Clock = 0xF8
      if (event.srt_type == TimingClock) {
        plaits_module.OnMIDIClock();
      }
    }

    // MIDI Thru: Forward all events to output regardless of
    // channel
    if (event.type == NoteOn || event.type == NoteOff ||
        event.type == ControlChange) {
      uint8_t out_bytes[3];
      uint8_t status_type = 0;
      if (event.type == NoteOn)
        status_type = 0x90;
      else if (event.type == NoteOff)
        status_type = 0x80;
      else if (event.type == ControlChange)
        status_type = 0xB0;

      size_t size = MIDIProcessor::BuildThruMessage(
          status_type, event.channel, event.data[0], event.data[1], out_bytes);
      if (size > 0) {
        hw.midi.SendMessage(out_bytes, size);
      }
    }

    // Update MIDI channel from module settings
    midi_processor.SetChannel(plaits_module.GetMidiChannel());

    if (!midi_processor.ShouldProcess(event.channel))
      continue;

    if (event.type == NoteOn) {
      NoteOnEvent note = event.AsNoteOn();
      if (note.velocity > 0) {
        // Trigger sample-and-hold for Bank/Engine CV
        sample_hold_pending = true;
        plaits_module.NoteOn(note.note, note.velocity);
      } else {
        plaits_module.NoteOff(note.note, 0);
      }
    } else if (event.type == NoteOff) {
      NoteOffEvent note = event.AsNoteOff();
      plaits_module.NoteOff(note.note, note.velocity);
    } else if (event.type == ControlChange) {
      ControlChangeEvent cc = event.AsControlChange();
      midi_processor.SetCC(cc.control_number, cc.value);
    }
  }
}

// Helper function for preset list display
const char *GetPresetNameCallback(int index) {
  return preset_manager.GetPresetName(index);
}

void UpdateDisplay() {
  auto params = plaits_module.GetParameters();

  // Update CPU overload status for display alert
  display.SetCpuOverload(cpu_monitor.IsOverloaded());

  if (menu.state == UIState::CharInput) {
    display.RenderCharInput(menu);
  } else if (menu.state == UIState::PresetList) {
    display.RenderPresetList(menu, GetPresetNameCallback);
  } else if (menu.state == UIState::FileBrowser) {
    // File browser for USER_DATA selection
    // Get the title from the parameter being edited
    mutables_ui::Parameter *current_params =
        menu.IsInSub() && menu.sub_parent ? menu.sub_parent->children : params;
    const char *title = current_params[menu.file_browser_param_idx].name;
    display.RenderFileBrowser(menu, title, GetUserDataFileNameCallback);
  } else if (menu.IsInSubmenu() && menu.submenu_param_index >= 0) {
    // Get the correct parameter - either from SUB children
    // or root params
    mutables_ui::Parameter *submenu_params =
        menu.IsInSub() && menu.sub_parent ? menu.sub_parent->children : params;
    display.RenderSubmenu(menu, submenu_params[menu.submenu_param_index]);
  } else if (menu.IsInSub() && menu.sub_parent) {
    // Browsing SUB's children with visibility support
    display.RenderSubMenu(menu, menu.sub_parent);
  } else {
    display.RenderMenu(menu, params);
  }
}

int main(void) {
  hw.Init();
  hw.SetAudioBlockSize(96);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);

  // Initialize USB serial logger for debug
  logger.StartLog(false); // Don't wait for PC
  logger.PrintLine("Plaits starting...");

  // Initialize SD card
  // Using official libDaisy initialization sequence with DMA
  // Buffer alignment handled in UserDataManager with
  // DMA_BUFFER_MEM_SECTION
  {
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults(); // FAST speed (50MHz), 4-bit width
    sdmmc.Init(sd_cfg);

    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    System::Delay(100); // Give card time to settle

    FRESULT fr = f_mount(&fsi.GetSDFileSystem(), "/", 1);
    if (fr == FR_OK) {
      logger.PrintLine("SD: Ready");
    } else {
      logger.PrintLine("SD: Mount failed (%d)", (int)fr);
    }
  }

  plaits_module.Init(48000.0f);

  // Initialize CPU monitor
  cpu_monitor.Init(48000.0f, hw.AudioBlockSize());

  // Initialize preset manager
  preset_manager.Init(sdmmc, fsi, plaits_module.GetShortName());
  logger.PrintLine("Preset manager initialized");

  // Try to load "default" preset if it exists
  if (preset_manager.IsSDAvailable()) {
    bool loaded =
        preset_manager.LoadPreset("default", plaits_module.GetParameters(),
                                  plaits_module.GetParameterCount());
    if (loaded) {
      logger.PrintLine("Loaded 'default' preset");
      // Rebuild CV mapping cache after loading preset
      cv_processor.MarkDirty();
    } else {
      logger.PrintLine("No 'default' preset found");
    }
  }

  // Initialize user data manager and load defaults from SD
  // card
  user_data_manager.Init(fsi, plaits_module.GetShortName());
  user_data_manager.CreateDirectories(); // Create dirs if
                                         // they don't exist
  user_data_manager.LoadDefaults();      // Logs internally

  // Sync user_data_params_ filenames with what was loaded
  // Find the User Data SUB param and update each child's
  // filename
  auto params = plaits_module.GetParameters();
  for (size_t i = 0; i < plaits_module.GetParameterCount(); i++) {
    if (params[i].type == ParamType::SUB && params[i].children) {
      for (size_t c = 0; c < params[i].child_count; c++) {
        auto &child = params[i].children[c];
        if (child.type == ParamType::USER_DATA) {
          UserDataManager::Target target =
              static_cast<UserDataManager::Target>(child.user_data_target);
          const char *loaded_file = user_data_manager.GetCurrentFile(target);
          if (loaded_file && loaded_file[0]) {
            child.SetUserDataFile(loaded_file);
          }
        }
      }
    }
  }

  // Register user data manager as the global provider for
  // Plaits
  plaits::g_user_data_provider = &user_data_manager;

  // Initialize Encoder Controller Callbacks
  EncoderCallbacks callbacks;

  callbacks.on_save_preset = [&]() {
    // Name is already validated by controller/menu before
    // calling this
    char final_name[MenuState::MAX_PRESET_NAME_LEN + 1];
    menu.GetFinalPresetName(final_name, sizeof(final_name));

    if (!preset_manager.IsSDAvailable()) {
      display.RenderMessage("Error", "No SD Card");
    } else {
      hw.StopAudio();
      bool success =
          preset_manager.SavePreset(final_name, plaits_module.GetParameters(),
                                    plaits_module.GetParameterCount());
      hw.StartAudio(AudioCallback);
      display.RenderMessage(success ? "Saved!" : "Error",
                            success ? final_name : "Save failed");
    }
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  };

  callbacks.on_scan_presets = [&]() -> int {
    if (!preset_manager.IsSDAvailable()) {
      display.RenderMessage("Error", "No SD Card");
      hw.display.Update();
      daisy::System::Delay(kMessageDisplayDelayMs);
      return 0;
    }
    return preset_manager.ScanPresets();
  };

  callbacks.on_load_preset = [&](int index) {
    const char *name = preset_manager.GetPresetName(index);
    if (!name)
      return;

    hw.StopAudio();
    bool success = preset_manager.LoadPreset(
        name, plaits_module.GetParameters(), plaits_module.GetParameterCount());
    if (success) {
      // Reload user data logic
      auto params = plaits_module.GetParameters();
      for (size_t i = 0; i < plaits_module.GetParameterCount(); i++) {
        if (params[i].type == ParamType::SUB && params[i].children) {
          for (size_t c = 0; c < params[i].child_count; c++) {
            auto &child = params[i].children[c];
            if (child.type == ParamType::USER_DATA) {
              UserDataManager::Target target =
                  static_cast<UserDataManager::Target>(child.user_data_target);
              if (child.user_data_filename[0])
                user_data_manager.LoadTarget(target, child.user_data_filename);
              else
                user_data_manager.LoadDefaultForTarget(target);
            }
          }
        }
      }
      plaits_module.ReloadUserData();
    }
    hw.StartAudio(AudioCallback);
    display.RenderMessage(success ? "Loaded!" : "Error",
                          success ? name : "Load failed");
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  };

  // on_enter_sub not strictly needed if controller handles
  // simple subs, but let's implement if needed
  // implementation in controller checks for it? Controller:
  // if (param.type == ParamType::SUB) ... menu.EnterSub...
  // Wait, controller header lines 206-212 handle SUB
  // internally without callback. The callback on_enter_sub
  // is defined but unused? Or used for custom subs? Let's
  // leave it empty or minimal.
  callbacks.on_enter_sub = [](mutables_ui::Parameter *param) {};

  callbacks.on_user_data_browser = [&](int target_int) {
    UserDataManager::Target target =
        static_cast<UserDataManager::Target>(target_int);
    user_data_file_count = user_data_manager.ListFiles(target, user_data_files,
                                                       MAX_USER_DATA_FILES);
    menu.EnterFileBrowser(menu.selected_param, user_data_file_count);
  };

  callbacks.on_user_data_selected = [&](mutables_ui::Parameter *param,
                                        int file_idx) {
    if (file_idx == 0) {
      // Default
      param->SetUserDataFile("");
    } else if (file_idx > 0 && file_idx <= user_data_file_count) {
      param->SetUserDataFile(user_data_files[file_idx - 1]);
    }

    UserDataManager::Target target =
        static_cast<UserDataManager::Target>(param->user_data_target);
    const char *filename =
        param->user_data_filename[0] ? param->user_data_filename : nullptr;

    hw.StopAudio();
    bool success = filename ? user_data_manager.LoadTarget(target, filename)
                            : user_data_manager.LoadDefaultForTarget(target);
    plaits_module.ReloadUserData();
    hw.StartAudio(AudioCallback);

    display.RenderMessage(success ? "Loaded!" : "Error",
                          filename ? filename : "Default");
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  };

  callbacks.on_display_message = [&](const char *title, const char *msg) {
    display.RenderMessage(title, msg);
    hw.display.Update();
    daisy::System::Delay(kMessageDisplayDelayMs);
  };

  encoder_controller.SetCallbacks(callbacks);
  midi_processor.Init(0); // Omni by default, will be updated in loop

  menu.param_count = plaits_module.GetParameterCount();
  display.Init(&hw,
               DrawPlaitsLogo); // Pass logo drawing function

  display.RenderBootScreen("Plaitsy");
  System::Delay(3000);

  // Enable CPU logging now that init is complete
  cpu_monitor.EnableLogging(true);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);
  hw.midi.StartReceive();

  uint32_t last_display_update = 0;
  while (1) {
    hw.midi.Listen();
    ProcessMidi();

    hw.ProcessAllControls();
    UpdateEncoder();

    uint32_t now = System::GetNow();

    // Periodic CPU logging (every 2 seconds in debug builds)
    cpu_monitor.Update(now, g_logger);

    if (now - last_display_update >= 16) {
      last_display_update = now;
      UpdateDisplay();
    }

    System::Delay(1);
  }
}
