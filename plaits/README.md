## Plaitsy (Plaits)
**Plaitsy** aims to replicate the original **Plaits** module. It also adds extra features to take advantage of Daisy Patch I/O and CPU.

General UI usage is described in the common UI documentation. This section focuses on features that are not covered by the original Plaits manual.
### On-demand user data loading
The original module allows uploading custom data for the **Wavetable**, **Waveterrain**, and **6-OP FM** engines via WAV files and CV inputs.
With an SD card, it’s more practical to store many datasets and load them on demand.
#### User data uploading / loading
The firmware reads binary files from these folders:
```
/plaits/user_data/six_op_bank_1/
/plaits/user_data/six_op_bank_2/
/plaits/user_data/six_op_bank_3/
/plaits/user_data/wavetable/
/plaits/user_data/wave_terrain/
```

To select the currently loaded user data, open the **User data** submenu.

**Note:** The selected user data is saved in presets (as a reference/path). The files must remain in the same location on the SD card.
#### User data creation tools
Because user data is loaded as raw binary (not through the original WAV encoding/decoding process), the original web app cannot be used for everything.

Local Python tools are provided in `plaits/tools`:
-   Create custom wavetables from a set of WAV files or from a formula
-   Create custom waveterrain data from images or from a formula
-   Convert SysEx banks to the format used by Plaits

I also provide some royalty-free example user data you can load if you want.
### MIDI
-   **Note On** acts as a trigger.
-   Note pitch follows the original V/Oct idea: the played note is interpreted relative to the **Octave** setting (in **Settings**).  
   **C4** is treated as “0 semitones” relative to the configured octave.
-   MIDI velocity can be used as a modulation source for mapped knob parameters.
-   **MIDI Out** acts as a passthrough: all incoming messages are relayed.
-   The MIDI receive channel is configurable in the **Settings** submenu.  
   (Passthrough is unaffected by the receive channel.)
### Gate inputs
Not configurable:
-   Gate In 1: trigger input
-   Gate In 2: clock input
### Audio outputs
Audio outputs replicate the original two outputs: **OUT** and **AUX**.

They are also split into “processed” vs “dry” outputs:
-   Outputs 3 and 4 are the **dry** OUT and AUX signals.
-   (Outputs 1 and 2 are the processed versions; see Audio inputs + Filter below.)

Because Eurorack levels can be very hot, a **Volume** parameter is available in the main menu to attenuate the outputs (and it can be modulated by velocity).
### CV outputs
To make use of the two CV outputs on Daisy Patch, Plaitsy can generate modulation signals.

In the main menu, each CV output has its own submenu where you select its function. Available modulators:
-   Internal envelope (note: some engines disable it)
-   A separate AD envelope (attack/decay), triggered on each note (Gate In 1 and MIDI)
-   LFO (waveform, rate, sync to Gate In 2 and/or MIDI clock, retrigger via gate/MIDI)
-   Envelope follower from Audio Inputs 3 and 4 (if enabled; see Audio inputs)

All modulators include an **amount** parameter for scaling.
### Audio inputs
Audio In 1 and 2 can apply audio-rate modulation algorithms inspired by Mutable Instruments **Warps**. Available algorithms:
-   XFADE
-   FOLD
-   AnaRM
-   DigRM
-   XOR
-   COMP
-   FM
-   VOCOD (Audio In 2 only)

Because line-level signals can be much quieter than Eurorack, a **Gain** parameter is available.

Signal chain:
`(Audio-rate modulation using In 1) -> (Audio-rate modulation using In 2) -> Filter`
### Ladder filter
Optional ladder filter with:
-   Mode: LP12, LP24, BP12, BP24, HP12, HP24
-   Frequency (with note tracking)
-   Resonance
-   Drive

**Note:** The Frequency knob attenuverter can replicate the “envelope amount” behavior from the original module. (Details depend on mapping/plugged state; see common UI documentation.)
### Gate output
Gate output can be configured as:
-   End-of-envelope trigger (EOC)
-   Trigger passthrough from Gate In 1 and MIDI
-   Trigger passthrough with probability
-   Clock passthrough with optional division (MIDI clock if present, otherwise Gate In 2)
-   Clock passthrough with probability
### Polyphony
**Polyphony is implemented.** Plaits already supports limited polyphony in some engines (e.g., 2 voices in the 6-OP FM engine), but Plaitsy extends this.

The **Voices** setting (in **Settings**) does not affect every engine, because polyphony is CPU-intensive on most engines.

Engines affected by the **Voices** setting:
-   Virtual Analog with VCF (engine2)
-   Phase Distortion (engine2)
-   Wave Terrain (engine2)
-   Virtual Analog
-   Waveshaping
-   2-operator FM
-   Wavetable

**Warning:** Even with lightweight engines, CPU usage can become too high with more than 2 voices, especially if you enable the filter or audio-rate modulation blocks.
A CPU alert is shown as a small exclamation mark in the top-right corner. You may also hear crackling or notice the UI becoming less responsive.
### Other implementation details
The trigger input is always considered “plugged in”. (See the original manual for the implications; in practice this has little impact.)
