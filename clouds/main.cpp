#include "../common/application_context.h"
#include "../common/calibration_manager.h"
#include "../common/constants.h"
#include "../common/controllers/encoder_controller.h"
#include "../common/cv_input.h"
#include "../common/cv_mapping_processor.h"
#include "../common/display.h"
#include "../common/midi_dispatcher.h"
#include "../common/midi_processor.h"
#include "../common/parameter.h"
#include "../common/preset_manager.h"
#include "../common/ui_state.h"
#include "clouds_port.h"
#include "daisy_patch.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;
using namespace mutables_ui;
using namespace mutables_clouds;

// Hardware
DaisyPatch hw;

// Module
CloudsPort clouds_module;

// UI
MenuState menu;
Display display;
CVInputBank cv_inputs;
CVMappingProcessor cv_processor;
MIDIProcessor midi_processor;
MIDIDispatcher midi_dispatcher;
EncoderController encoder_controller(cv_processor);

// SD Card / Presets / Calibration
SdmmcHandler sdmmc;
FatFSInterface fsi;
PresetManager preset_manager;
CalibrationManager calibration_manager;

// Audio buffers
float *audio_in[4];
float *audio_out[4];

// Cached CV values
static float cached_cv_values_[4];

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  // Rebuild mapping cache if needed
  if (cv_processor.IsDirty()) {
    cv_processor.RebuildCache(clouds_module.GetParameters(),
                              clouds_module.GetParameterCount());
  }

  // Update CV inputs with raw ADC values (inverted: 0V = 1.0, 5V = 0.0)
  float cv1 = 1.0f - hw.controls[DaisyPatch::CTRL_1].GetRawFloat();
  float cv2 = 1.0f - hw.controls[DaisyPatch::CTRL_2].GetRawFloat();
  float cv3 = 1.0f - hw.controls[DaisyPatch::CTRL_3].GetRawFloat();
  float cv4 = 1.0f - hw.controls[DaisyPatch::CTRL_4].GetRawFloat();

  cv_inputs.UpdateRawValues(cv1, cv2, cv3, cv4);

  // Cache filtered CV values
  cached_cv_values_[0] = cv_inputs.GetFiltered(0);
  cached_cv_values_[1] = cv_inputs.GetFiltered(1);
  cached_cv_values_[2] = cv_inputs.GetFiltered(2);
  cached_cv_values_[3] = cv_inputs.GetFiltered(3);

  auto params = clouds_module.GetParameters();
  size_t param_count = clouds_module.GetParameterCount();

  // Process CC mappings
  cv_processor.ProcessCCMappings(midi_processor.GetCCValues(), kCVHysteresis);

  // Process CV mappings
  cv_processor.ProcessCVMappings(cv_inputs, kCVHysteresis, false);

  // Zero unmapped CV params
  CVMappingProcessor::ZeroUnmappedCVParams(params, param_count, kCVHysteresis);

  // Setup audio pointers
  for (size_t i = 0; i < 4; i++) {
    audio_in[i] = (float *)in[i];
    audio_out[i] = out[i];
  }

  // Clear outputs
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = 0; j < size; j++) {
      out[i][j] = 0.0f;
    }
  }

  // Process audio
  clouds_module.Process(audio_in, audio_out, size);
}

// Encoder Update Wrapper
void UpdateEncoder() {
  int inc = hw.encoder.Increment();
  bool pressed = hw.encoder.Pressed();
  bool rising = hw.encoder.RisingEdge();

  static uint32_t press_start = 0;
  if (rising)
    press_start = System::GetNow();

  static bool encoder_button_last = false;
  uint32_t now = System::GetNow();
  EncoderHardwareState hw_state = {inc, pressed, rising, 0, now};

  if (!pressed && encoder_button_last) {
    hw_state.press_duration = now - press_start;
  }
  encoder_button_last = pressed;

  encoder_controller.Update(hw_state, menu, clouds_module.GetParameters(),
                            clouds_module.GetParameterCount());
}

// Helper function for preset list display
const char *GetPresetNameCallback(int index) {
  return preset_manager.GetPresetName(index);
}

void UpdateDisplay() {
  auto params = clouds_module.GetParameters();

  if (menu.state == UIState::CharInput) {
    display.RenderCharInput(menu);
  } else if (menu.state == UIState::PresetList) {
    display.RenderPresetList(menu, GetPresetNameCallback);
  } else if (menu.state == UIState::Calibration) {
    float current_raw_cv = cv_inputs.GetRawADC(menu.calibration_cv_index);
    display.RenderCalibration(menu, calibration_manager.GetCalibration(), current_raw_cv);
  } else if (menu.IsInSubmenu() && menu.submenu_param_index >= 0) {
    mutables_ui::Parameter *submenu_params =
        menu.IsInSub() && menu.sub_parent ? menu.sub_parent->children : params;
    display.RenderSubmenu(menu, submenu_params[menu.submenu_param_index]);
  } else if (menu.IsInSub() && menu.sub_parent) {
    display.RenderSubMenu(menu, menu.sub_parent);
  } else {
    display.RenderMenu(menu, params);
  }
}

int main(void) {
  // Initialize hardware at 32 kHz with block size 32
  hw.Init();
  hw.SetAudioBlockSize(32);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_32KHZ);

  // Initialize SD card
  {
    SdmmcHandler::Config sd_cfg;
    sd_cfg.Defaults();
    sdmmc.Init(sd_cfg);

    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    System::Delay(100);

    FRESULT fr = f_mount(&fsi.GetSDFileSystem(), "/", 1);
    if (fr == FR_OK) {
      // SD ready
    }
  }

  // Initialize the Clouds module
  clouds_module.Init(32000.0f);

  // Initialize preset manager
  preset_manager.Init(sdmmc, fsi, clouds_module.GetShortName());

  // Initialize calibration manager
  calibration_manager.Init(fsi);
  calibration_manager.Load();
  cv_inputs.SetCalibration(&calibration_manager.GetCalibration());

  // Try to load "default" preset
  if (preset_manager.IsSDAvailable()) {
    bool loaded = preset_manager.LoadPreset(
        "default", clouds_module.GetParameters(),
        clouds_module.GetParameterCount());
    if (loaded) {
      cv_processor.MarkDirty();
    }
  }

  // ApplicationContext - simplifies callback setup
  ApplicationContext app_ctx(
      hw,
      clouds_module,
      menu,
      display,
      cv_inputs,
      cv_processor,
      midi_processor,
      preset_manager,
      calibration_manager,
      encoder_controller
  );

  app_ctx.audio_callback = AudioCallback;
  encoder_controller.SetCallbacks(app_ctx.BuildStandardCallbacks());

  midi_processor.Init(0); // Omni by default

  // Configure MIDIDispatcher callbacks
  midi_dispatcher.SetChannelCallback([&]() {
    return 0; // Omni mode
  });

  menu.param_count = clouds_module.GetParameterCount();
  display.Init(&hw, nullptr);

  display.RenderBootScreen("Clouds");
  System::Delay(2000);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);
  hw.midi.StartReceive();

  uint32_t last_display_update = 0;
  while (1) {
    hw.midi.Listen();
    midi_dispatcher.Process(hw, midi_processor);

    hw.ProcessAllControls();
    UpdateEncoder();

    // Prepare() called every main loop iteration (matches original firmware)
    clouds_module.Prepare();

    // Sync playback mode from UI parameter
    int mode = clouds_module.GetParameters()[0].GetIndex();
    if (mode != clouds_module.GetPlaybackMode()) {
      clouds_module.SetPlaybackMode(mode);
    }

    uint32_t now = System::GetNow();
    if (now - last_display_update >= 16) {
      last_display_update = now;
      UpdateDisplay();
    }

    System::Delay(1);
  }
}
