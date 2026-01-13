#!/usr/bin/env python3
"""
Convert WAV files to Plaits wavetable .bin format.

Usage:
    python wav_to_wavetable.py wave1.wav [wave2.wav ... wave15.wav] -o output.bin
    
    Or with a map file:
    python wav_to_wavetable.py --waves wave1.wav wave2.wav ... --map map.txt -o output.bin

Accepts 1-15 WAV files. Each is resampled to 128 samples and stored as an
integrated wavetable. The output is 4096 bytes containing:
  - Bytes 0-63: Wave map (8×8 grid of wave indices)
  - Bytes 64-4095: 15 custom waves as integrated 16-bit samples

If fewer than 15 waves are provided, remaining slots are filled with the last wave.

WAV files can be any sample rate and length - only the first cycle is used,
resampled via DFT to 128 samples.

Requirements:
    pip install numpy scipy
"""

import sys
import os
import argparse
import struct

try:
    import numpy as np
    from scipy.io import wavfile
except ImportError:
    print("Error: This script requires numpy and scipy.", file=sys.stderr)
    print("Install with: pip install numpy scipy", file=sys.stderr)
    sys.exit(1)

WAVETABLE_SIZE = 128
NUM_CUSTOM_WAVES = 15
NUM_BUILTIN_WAVES = 192
MAP_SIZE = 8
EXPECTED_BIN_SIZE = 4096


def read_wav(path: str) -> np.ndarray:
    """Read a WAV file and return samples as float32 array."""
    try:
        sample_rate, data = wavfile.read(path)
    except Exception as e:
        raise IOError(f"Error reading {path}: {e}")
    
    # Convert to float
    if data.dtype == np.int16:
        data = data.astype(np.float32) / 32768.0
    elif data.dtype == np.int32:
        data = data.astype(np.float32) / 2147483648.0
    elif data.dtype == np.uint8:
        data = (data.astype(np.float32) - 128) / 128.0
    elif data.dtype != np.float32:
        data = data.astype(np.float32)
    
    # Convert stereo to mono
    if len(data.shape) > 1:
        data = data.mean(axis=1)
    
    return data


def resample_to_wavetable(samples: np.ndarray, max_length: int = 8192, 
                          phase_align: bool = True) -> np.ndarray:
    """
    Resample audio to a single-cycle wavetable using DFT.
    
    This matches the Plaits web editor's algorithm:
    - Extract harmonics via DFT
    - Optionally phase-align (zero out phase, keep magnitude)
    - Reconstruct at target size
    """
    n = min(len(samples), max_length)
    samples = samples[:n]
    m = WAVETABLE_SIZE
    nyquist = min(n, m) // 2
    
    result = np.zeros(m, dtype=np.float32)
    
    for i in range(1, nyquist):
        norm = 2.0 / n
        
        # DFT at this frequency
        xr = 0.0
        xi = 0.0
        for j in range(n):
            e = j * i / n * 2.0 * np.pi
            xr += np.cos(e) * samples[j]
            xi += np.sin(e) * samples[j]
        
        xr *= norm
        xi *= norm
        
        # Phase alignment (optional)
        if phase_align:
            xi = np.sqrt(xr * xr + xi * xi)
            xr = 0
        
        # Reconstruct at target size
        for j in range(m):
            e = j * i / m * 2.0 * np.pi
            result[j] += np.cos(e) * xr
            result[j] += np.sin(e) * xi
    
    return result


def integrate_wave(wave: np.ndarray) -> np.ndarray:
    """
    Convert a waveform to integrated wavetable format.
    
    This matches Plaits' integrated wavetable format:
    - Center and normalize the wave
    - Compute cumulative sum (integral)
    - Store as 16-bit with 4 guard samples
    """
    n = len(wave)
    
    # Center
    wave = wave - wave.mean()
    
    # Normalize
    max_amp = np.abs(wave).max()
    if max_amp > 0:
        wave = wave / max_amp
    
    # Compute integral (cumsum over 2 cycles to get stable result)
    extended = np.tile(wave, 2)
    integral = np.cumsum(extended)
    
    # Get integral mean over second cycle
    integral_mean = integral[n:].mean()
    
    # Store second cycle minus mean
    integrated = np.zeros(n + 4, dtype=np.int16)
    for i in range(n):
        val = (integral[n + i] - integral_mean) * (4 * 32767.0 / n)
        integrated[i] = int(np.clip(np.round(val), -32768, 32767))
    
    # Add guard samples (wrap around)
    for i in range(4):
        integrated[n + i] = integrated[i]
    
    return integrated


def create_default_map() -> np.ndarray:
    """Create default 8×8 wave map pointing to custom waves."""
    map_data = np.zeros(MAP_SIZE * MAP_SIZE, dtype=np.uint8)
    
    for x in range(MAP_SIZE):
        for y in range(MAP_SIZE):
            # Map 8×8 grid to 4×4 custom wave grid
            j = x * 4 // MAP_SIZE
            k = y * 4 // MAP_SIZE
            wave_idx = min(j + k * 4, NUM_CUSTOM_WAVES - 1)
            map_data[x + y * MAP_SIZE] = NUM_BUILTIN_WAVES + wave_idx
    
    return map_data


def parse_map_file(path: str) -> np.ndarray:
    """Parse a map file (8×8 grid of wave indices)."""
    map_data = np.zeros(MAP_SIZE * MAP_SIZE, dtype=np.uint8)
    
    with open(path, 'r') as f:
        lines = f.readlines()
    
    row = 0
    for line in lines:
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        
        values = line.replace(',', ' ').split()
        for col, val in enumerate(values[:MAP_SIZE]):
            if row < MAP_SIZE:
                idx = int(val)
                # Allow both absolute indices and custom-relative
                if idx < 0:
                    idx = NUM_BUILTIN_WAVES + (-idx - 1)
                map_data[col + (MAP_SIZE - 1 - row) * MAP_SIZE] = idx
        row += 1
    
    return map_data


def convert_wavs_to_wavetable(wav_paths: list, bin_path: str, 
                               map_path: str = None,
                               phase_align: bool = True) -> bool:
    """Convert WAV files to Plaits wavetable binary format."""
    
    if len(wav_paths) > NUM_CUSTOM_WAVES:
        print(f"Warning: Only first {NUM_CUSTOM_WAVES} WAV files will be used",
              file=sys.stderr)
        wav_paths = wav_paths[:NUM_CUSTOM_WAVES]
    
    # Load and process waves
    waves = []
    for i, path in enumerate(wav_paths):
        print(f"Processing wave {i+1}: {path}")
        
        try:
            samples = read_wav(path)
        except IOError as e:
            print(f"  Error: {e}", file=sys.stderr)
            return False
        
        print(f"  Input: {len(samples)} samples")
        
        # Resample to wavetable
        resampled = resample_to_wavetable(samples, phase_align=phase_align)
        
        # Integrate
        integrated = integrate_wave(resampled)
        waves.append(integrated)
        
        print(f"  Output: {len(integrated)} samples (integrated)")
    
    # Fill remaining slots with last wave
    while len(waves) < NUM_CUSTOM_WAVES:
        waves.append(waves[-1] if waves else np.zeros(WAVETABLE_SIZE + 4, dtype=np.int16))
    
    # Create or load map
    if map_path:
        print(f"Loading map from: {map_path}")
        try:
            wave_map = parse_map_file(map_path)
        except Exception as e:
            print(f"Error reading map file: {e}", file=sys.stderr)
            return False
    else:
        print("Using default wave map")
        wave_map = create_default_map()
    
    # Build output binary
    bin_data = bytearray(EXPECTED_BIN_SIZE)
    
    # Write map (bytes 0-63)
    bin_data[0:64] = wave_map.tobytes()
    
    # Write waves (bytes 64-4095)
    offset = 64
    for i, wave in enumerate(waves):
        # Write as little-endian 16-bit
        for sample in wave:
            if offset + 2 <= EXPECTED_BIN_SIZE:
                struct.pack_into('<h', bin_data, offset, sample)
                offset += 2
    
    # Verify size
    if len(bin_data) != EXPECTED_BIN_SIZE:
        print(f"Error: Generated {len(bin_data)} bytes, expected {EXPECTED_BIN_SIZE}",
              file=sys.stderr)
        return False
    
    # Write output
    try:
        with open(bin_path, 'wb') as f:
            f.write(bin_data)
    except IOError as e:
        print(f"Error writing {bin_path}: {e}", file=sys.stderr)
        return False
    
    print(f"\nOutput: {bin_path}")
    print(f"  {len(wav_paths)} custom waves")
    print(f"  {EXPECTED_BIN_SIZE} bytes total")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Convert WAV files to Plaits wavetable format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('wavs', nargs='*', help='WAV files (1-15)')
    parser.add_argument('-o', '--output', required=True, help='Output .bin file')
    parser.add_argument('-m', '--map', help='Optional wave map file (8x8 grid)')
    parser.add_argument('--no-phase-align', action='store_true',
                        help='Disable phase alignment (preserves original phase)')
    
    args = parser.parse_args()
    
    if not args.wavs:
        parser.print_help()
        sys.exit(1)
    
    for path in args.wavs:
        if not os.path.exists(path):
            print(f"Error: File not found: {path}", file=sys.stderr)
            sys.exit(1)
    
    success = convert_wavs_to_wavetable(
        args.wavs, 
        args.output,
        args.map,
        phase_align=not args.no_phase_align
    )
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
