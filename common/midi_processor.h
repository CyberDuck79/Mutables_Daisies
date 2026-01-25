#pragma once

#include <cstdint>
#include <cstring>

namespace mutables_ui {

class MIDIProcessor {
public:
  MIDIProcessor() = default;

  // Initialize with default MIDI channel
  // channel: 0 = Omni (all), 1-16 = specific channel
  void Init(int channel = 0) {
    midi_channel_ = channel;
    for (int i = 0; i < 128; i++) {
      cc_values_[i] = 0.0f;
    }
  }

  // Set MIDI channel filter
  void SetChannel(int channel) { midi_channel_ = channel; }
  int GetChannel() const { return midi_channel_; }

  // Check if event on given channel should be processed
  // event_channel: 0-15 (MIDI channel - 1)
  bool ShouldProcess(int event_channel) const {
    return (midi_channel_ == 0) || (event_channel == midi_channel_ - 1);
  }

  // CC value storage (normalized 0.0-1.0)
  float GetCC(int cc_num) const {
    if (cc_num < 0 || cc_num >= 128)
      return 0.0f;
    return cc_values_[cc_num];
  }

  void SetCC(int cc_num, uint8_t value) {
    if (cc_num >= 0 && cc_num < 128) {
      cc_values_[cc_num] = static_cast<float>(value) / 127.0f;
    }
  }

  // Get pointer to CC array (for CVMappingProcessor)
  const float *GetCCValues() const { return cc_values_; }

  // Build MIDI thru message bytes
  // Returns number of bytes written (0 if not a passthrough message type)
  static size_t BuildThruMessage(
      uint8_t status_type, // NoteOn=0x90, etc (already includes high nibble)
      uint8_t channel,     // 0-15
      uint8_t data0, uint8_t data1, uint8_t *out_bytes) {
    // Status type should be the high nibble (0x80, 0x90, 0xB0 etc)
    // We mask it just in case, though caller usually provides the full byte
    uint8_t type = status_type & 0xF0;

    if (type == 0x80 || type == 0x90 || type == 0xB0) {
      out_bytes[0] = type | (channel & 0x0F);
      out_bytes[1] = data0 & 0x7F;
      out_bytes[2] = data1 & 0x7F;
      return 3;
    }
    return 0;
  }

private:
  int midi_channel_ = 0;
  float cc_values_[128] = {0};
};

} // namespace mutables_ui
