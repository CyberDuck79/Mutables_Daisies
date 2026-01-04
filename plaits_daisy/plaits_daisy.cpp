/**
 * Plaits for Daisy Patch.Init()
 * 
 * Port of Mutable Instruments Plaits to Electrosmith Daisy Patch.Init() hardware.
 * 
 * This is a minimal working example that:
 * - Uses 4 knobs (CV_1 to CV_4) mapped to Plaits parameters
 * - Outputs audio on the main audio output
 * 
 * Knob mapping:
 * - CV_1: Frequency (pitch)
 * - CV_2: Harmonics
 * - CV_3: Timbre
 * - CV_4: Morph
 */

#include "daisy_patch_sm.h"
#include "daisysp.h"

// Plaits DSP includes
#include "plaits/dsp/voice.h"
#include "stmlib/utils/buffer_allocator.h"

using namespace daisy;
using namespace patch_sm;

// Hardware
DaisyPatchSM hw;

// Plaits Voice
plaits::Voice       voice;
plaits::Patch       patch;
plaits::Modulations modulations;

// Shared memory buffer for Plaits engines
// Plaits needs 16KB of shared buffer for its engines
char shared_buffer[16384];

// Audio block size - Plaits works with frames, we'll match Daisy's block size
constexpr size_t kBlockSize = 16;

// Output buffer for conversion from int16 to float
plaits::Voice::Frame output_frames[kBlockSize];

/**
 * Initialize Plaits default patch settings
 */
void InitPatch() {
    patch.note = 48.0f;  // Base note (MIDI note number, 48 = C3)
    patch.harmonics = 0.5f;
    patch.timbre = 0.5f;
    patch.morph = 0.5f;
    patch.frequency_modulation_amount = 0.0f;
    patch.timbre_modulation_amount = 0.0f;
    patch.morph_modulation_amount = 0.0f;
    patch.engine = 0;  // First engine (Virtual Analog VCF)
    patch.decay = 0.5f;
    patch.lpg_colour = 0.5f;
}

/**
 * Initialize Plaits modulations (external CV inputs)
 * For now, we set everything to default/unpatched state
 */
void InitModulations() {
    modulations.engine = 0.0f;
    modulations.note = 0.0f;
    modulations.frequency = 0.0f;
    modulations.harmonics = 0.0f;
    modulations.timbre = 0.0f;
    modulations.morph = 0.0f;
    modulations.trigger = 0.0f;
    modulations.level = 1.0f;
    
    // Patched states - set to false for knob-only control
    modulations.frequency_patched = false;
    modulations.timbre_patched = false;
    modulations.morph_patched = false;
    modulations.trigger_patched = false;
    modulations.level_patched = false;
}

/**
 * Audio callback - processes audio at sample rate
 */
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // Process controls (read knobs)
    hw.ProcessAllControls();
    
    // Map knobs to Plaits parameters
    // CV_1 to CV_4 are the 4 knobs on Patch.Init()
    // GetAdcValue returns 0.0 to 1.0
    
    // Knob 1: Frequency - map to MIDI note range (24-96, i.e., C1 to C7)
    float freq_knob = hw.GetAdcValue(CV_1);
    patch.note = 24.0f + freq_knob * 72.0f;
    
    // Knob 2: Harmonics (0-1)
    patch.harmonics = hw.GetAdcValue(CV_2);
    
    // Knob 3: Timbre (0-1)
    patch.timbre = hw.GetAdcValue(CV_3);
    
    // Knob 4: Morph (0-1)
    patch.morph = hw.GetAdcValue(CV_4);
    
    // Render Plaits voice
    // Note: size might not equal kBlockSize, handle appropriately
    size_t frames_rendered = 0;
    while (frames_rendered < size) {
        size_t frames_to_render = std::min(kBlockSize, size - frames_rendered);
        
        voice.Render(patch, modulations, output_frames, frames_to_render);
        
        // Convert from int16 to float and write to output
        // Plaits outputs int16 (-32767 to 32767), Daisy expects float (-1 to 1)
        for (size_t i = 0; i < frames_to_render; i++) {
            // Main output (OUT)
            out[0][frames_rendered + i] = static_cast<float>(output_frames[i].out) / 32768.0f;
            // Aux output (AUX)
            out[1][frames_rendered + i] = static_cast<float>(output_frames[i].aux) / 32768.0f;
        }
        
        frames_rendered += frames_to_render;
    }
}

int main(void)
{
    // Initialize Daisy Patch.Init() hardware
    hw.Init();
    hw.SetAudioBlockSize(kBlockSize);
    hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
    
    // Initialize Plaits voice with buffer allocator
    stmlib::BufferAllocator allocator(shared_buffer, sizeof(shared_buffer));
    voice.Init(&allocator);
    
    // Initialize patch and modulations
    InitPatch();
    InitModulations();
    
    // Start audio processing
    hw.StartAudio(AudioCallback);
    
    // Main loop - LED heartbeat
    while(1) {
        // Toggle LED to show we're running
        hw.SetLed(true);
        hw.Delay(250);
        hw.SetLed(false);
        hw.Delay(250);
    }
}
