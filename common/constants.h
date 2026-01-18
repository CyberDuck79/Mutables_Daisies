#pragma once

#include <cstdint>

namespace mutables {

//=============================================================================
// Audio Constants
//=============================================================================
static constexpr float kDefaultSampleRate = 48000.0f;
static constexpr float kAudioScaleInt16 = 32768.0f;
static constexpr float kAudioMaxInt16 = 32767.0f;

//=============================================================================
// MIDI Constants
//=============================================================================
static constexpr float kMidiNoteC4 = 60.0f;
static constexpr int kMidiCCMax = 127;
static constexpr int kMidiChannelMax = 16;
static constexpr int kMidiNoteMax = 127;
static constexpr int kMidiVelocityMax = 127;

//=============================================================================
// Timing Constants
//=============================================================================
static constexpr float kTriggerDurationS = 0.01f;       // 10ms trigger pulse
static constexpr float kMsToSeconds = 0.001f;           // Milliseconds to seconds
static constexpr uint32_t kMessageDisplayDelayMs = 1500; // UI message display time
static constexpr float kEnvThreshold = 0.01f;           // Envelope near-zero threshold

//=============================================================================
// CV Constants
//=============================================================================
static constexpr float kCVCenter = 0.5f;                // CV center point (bipolar zero)
static constexpr float kCVHysteresis = 0.001f;          // Hysteresis threshold for CV
static constexpr float kVOctRange = 60.0f;              // V/Oct range in semitones (±30)

//=============================================================================
// UI Layout Constants (128x64 OLED)
//=============================================================================
static constexpr int kScreenWidth = 128;
static constexpr int kScreenHeight = 64;
static constexpr int kLineHeight = 14;                  // Main menu line height
static constexpr int kCompactLineHeight = 13;           // Submenu line height
static constexpr int kValueColumn = 76;                 // X position for value display
static constexpr int kTitleBarHeight = 10;              // Height of title bar

// Font dimensions
static constexpr int kFont7x10Width = 7;
static constexpr int kFont7x10Height = 10;
static constexpr int kFont6x8Width = 6;
static constexpr int kFont6x8Height = 8;

//=============================================================================
// Parameter UI Constants
//=============================================================================
static constexpr float kEncoderStepSmall = 0.01f;       // Fine adjustment step
static constexpr float kEncoderStepMedium = 0.05f;      // Normal adjustment step
static constexpr float kEncoderStepLarge = 0.1f;        // Coarse adjustment step

//=============================================================================
// Mathematical Constants
//=============================================================================
static constexpr float kLn10 = 2.302585093f;            // Natural log of 10
static constexpr float kPi = 3.14159265358979f;
static constexpr float kTwoPi = 6.28318530717958f;

//=============================================================================
// Envelope/Modulation Constants
//=============================================================================
static constexpr float kDefaultAttack = 0.01f;          // Default attack coefficient
static constexpr float kDefaultRelease = 0.01f;         // Default release coefficient
static constexpr float kEnvNearZero = 0.001f;           // Envelope "finished" threshold
static constexpr float kMinThreshold = 0.01f;           // Minimum follower threshold

} // namespace mutables
