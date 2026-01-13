#!/usr/bin/env python3
"""
Generate Plaits wavetable from equations or WAV files.

Usage:
    # From equation (generates 15 waves morphing through parameters)
    python wavetable_generator.py -e "sin(phi) + sin(phi * 2) * y" -o wavetable.bin
    
    # From WAV files (1-15 files)
    python wavetable_generator.py -w wave1.wav wave2.wav wave3.wav -o wavetable.bin
    
    # With custom wave map
    python wavetable_generator.py -w *.wav -m map.txt -o wavetable.bin
    
    # List available variables for equations
    python wavetable_generator.py --help-equations

Equation variables:
    t         : time/phase within one cycle (0 to 1)
    phi       : phase in radians (0 to 2π)
    x, y      : grid position (0 to 1), for morphing across 4×4 grid
    row, col  : integer grid position (0 to 3)
    i         : sample index (0 to 127)
    n         : total samples (128)
    pi        : 3.14159...
    
Built-in functions:
    sin, cos, tan, atan, atan2, floor, ceil, round, random,
    sqrt, exp, log, pow, abs, sign

The equation generates a 4×4 grid (16 slots), but only 15 custom waves
are stored. The wave at position (col, row) varies with x=col/3, y=row/3.

Example equations:
    "sin(phi)"                           : Pure sine
    "sin(phi) * (1 - t)"                 : Decaying sine
    "sin(phi * (1 + y * 7))"             : Morphing harmonics
    "sin(phi) + sin(phi * 2) * x"        : Variable 2nd harmonic
    "sin(phi) ** (1 + y * 3)"            : Sine power morph

Requirements:
    pip install numpy scipy
"""

import sys
import os
import argparse
import struct
import math

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
CUSTOM_MAP_SIZE = 4
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
    """Resample audio to a single-cycle wavetable using DFT."""
    n = min(len(samples), max_length)
    samples = samples[:n]
    m = WAVETABLE_SIZE
    nyquist = min(n, m) // 2
    
    result = np.zeros(m, dtype=np.float32)
    
    for i in range(1, nyquist):
        norm = 2.0 / n
        
        xr = 0.0
        xi = 0.0
        for j in range(n):
            e = j * i / n * 2.0 * np.pi
            xr += np.cos(e) * samples[j]
            xi += np.sin(e) * samples[j]
        
        xr *= norm
        xi *= norm
        
        if phase_align:
            xi = np.sqrt(xr * xr + xi * xi)
            xr = 0
        
        for j in range(m):
            e = j * i / m * 2.0 * np.pi
            result[j] += np.cos(e) * xr
            result[j] += np.sin(e) * xi
    
    return result


def integrate_wave(wave: np.ndarray) -> np.ndarray:
    """Convert a waveform to integrated wavetable format."""
    n = len(wave)
    
    # Center
    wave = wave - wave.mean()
    
    # Normalize
    max_amp = np.abs(wave).max()
    if max_amp > 0:
        wave = wave / max_amp
    
    # Compute integral
    extended = np.tile(wave, 2)
    integral = np.cumsum(extended)
    integral_mean = integral[n:].mean()
    
    # Store with guard samples
    integrated = np.zeros(n + 4, dtype=np.int16)
    for i in range(n):
        val = (integral[n + i] - integral_mean) * (4 * 32767.0 / n)
        integrated[i] = int(np.clip(np.round(val), -32768, 32767))
    
    for i in range(4):
        integrated[n + i] = integrated[i]
    
    return integrated


def evaluate_wavetable_equation(expression: str) -> list:
    """Evaluate a wavetable equation, generating 15 waves on a 4×4 grid."""
    
    def make_wave_func(expr):
        def f(i, j, k, m, n):
            t = i / n
            y = j / (m - 1) if m > 1 else 0
            x = k / (m - 1) if m > 1 else 0
            row = j
            col = k
            phi = math.pi * 2 * t
            pi = math.pi
            
            sin, cos, tan = math.sin, math.cos, math.tan
            atan, atan2 = math.atan, math.atan2
            floor, ceil = math.floor, math.ceil
            sqrt, exp, log = math.sqrt, math.exp, math.log
            
            def pow(base, exp_val):
                return math.pow(base, exp_val)
            
            try:
                return eval(expr)
            except Exception as e:
                raise ValueError(f"Error evaluating '{expr}' at i={i}, j={j}, k={k}: {e}")
        return f
    
    f = make_wave_func(expression)
    
    waves = []
    for wave_idx in range(NUM_CUSTOM_WAVES):
        col = wave_idx % CUSTOM_MAP_SIZE
        row = wave_idx // CUSTOM_MAP_SIZE
        
        # Generate waveform
        waveform = np.zeros(WAVETABLE_SIZE, dtype=np.float32)
        for i in range(WAVETABLE_SIZE):
            waveform[i] = f(i, row, col, CUSTOM_MAP_SIZE, WAVETABLE_SIZE)
        
        # Integrate
        integrated = integrate_wave(waveform)
        waves.append(integrated)
    
    return waves


def create_default_map() -> np.ndarray:
    """Create default 8×8 wave map pointing to custom waves."""
    map_data = np.zeros(MAP_SIZE * MAP_SIZE, dtype=np.uint8)
    
    # Fill last slot with the 15th wave
    map_data.fill(NUM_BUILTIN_WAVES + NUM_CUSTOM_WAVES - 1)
    
    for wave in range(NUM_CUSTOM_WAVES):
        col = wave % CUSTOM_MAP_SIZE
        row = wave // CUSTOM_MAP_SIZE
        # Map 4×4 custom grid to positions in 8×8 map
        for dy in range(2):
            for dx in range(2):
                mx = col * 2 + dx
                my = row * 2 + dy
                if my < MAP_SIZE and mx < MAP_SIZE:
                    map_data[mx + my * MAP_SIZE] = NUM_BUILTIN_WAVES + wave
    
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
                if idx < 0:
                    idx = NUM_BUILTIN_WAVES + (-idx - 1)
                map_data[col + (MAP_SIZE - 1 - row) * MAP_SIZE] = idx
        row += 1
    
    return map_data


def write_wavetable_bin(waves: list, wave_map: np.ndarray, bin_path: str) -> bool:
    """Write wavetable binary file."""
    
    bin_data = bytearray(EXPECTED_BIN_SIZE)
    
    # Write map (bytes 0-63)
    bin_data[0:64] = wave_map.tobytes()
    
    # Write waves (bytes 64-4095)
    offset = 64
    for i, wave in enumerate(waves):
        for sample in wave:
            if offset + 2 <= EXPECTED_BIN_SIZE:
                struct.pack_into('<h', bin_data, offset, sample)
                offset += 2
    
    if len(bin_data) != EXPECTED_BIN_SIZE:
        print(f"Error: Generated {len(bin_data)} bytes, expected {EXPECTED_BIN_SIZE}",
              file=sys.stderr)
        return False
    
    try:
        with open(bin_path, 'wb') as f:
            f.write(bin_data)
    except IOError as e:
        print(f"Error writing {bin_path}: {e}", file=sys.stderr)
        return False
    
    return True


def generate_from_equation(expression: str, bin_path: str, map_path: str = None) -> bool:
    """Generate wavetable from equation."""
    
    print(f"Generating wavetable from equation: {expression}")
    
    try:
        waves = evaluate_wavetable_equation(expression)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        return False
    
    print(f"  Generated {len(waves)} waves")
    
    if map_path:
        print(f"Loading map from: {map_path}")
        wave_map = parse_map_file(map_path)
    else:
        print("Using default wave map")
        wave_map = create_default_map()
    
    if not write_wavetable_bin(waves, wave_map, bin_path):
        return False
    
    print(f"\nOutput: {bin_path}")
    print(f"  {NUM_CUSTOM_WAVES} custom waves")
    print(f"  {EXPECTED_BIN_SIZE} bytes total")
    
    return True


def generate_from_wavs(wav_paths: list, bin_path: str, map_path: str = None,
                       phase_align: bool = True) -> bool:
    """Generate wavetable from WAV files."""
    
    if len(wav_paths) > NUM_CUSTOM_WAVES:
        print(f"Warning: Only first {NUM_CUSTOM_WAVES} WAV files will be used",
              file=sys.stderr)
        wav_paths = wav_paths[:NUM_CUSTOM_WAVES]
    
    waves = []
    for i, path in enumerate(wav_paths):
        print(f"Processing wave {i+1}: {path}")
        
        try:
            samples = read_wav(path)
        except IOError as e:
            print(f"  Error: {e}", file=sys.stderr)
            return False
        
        print(f"  Input: {len(samples)} samples")
        
        resampled = resample_to_wavetable(samples, phase_align=phase_align)
        integrated = integrate_wave(resampled)
        waves.append(integrated)
        
        print(f"  Output: {len(integrated)} samples (integrated)")
    
    # Fill remaining slots with last wave
    while len(waves) < NUM_CUSTOM_WAVES:
        waves.append(waves[-1] if waves else np.zeros(WAVETABLE_SIZE + 4, dtype=np.int16))
    
    if map_path:
        print(f"Loading map from: {map_path}")
        wave_map = parse_map_file(map_path)
    else:
        print("Using default wave map")
        wave_map = create_default_map()
    
    if not write_wavetable_bin(waves, wave_map, bin_path):
        return False
    
    print(f"\nOutput: {bin_path}")
    print(f"  {len(wav_paths)} custom waves (padded to {NUM_CUSTOM_WAVES})")
    print(f"  {EXPECTED_BIN_SIZE} bytes total")
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Generate Plaits wavetable from equations or WAV files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('-e', '--equation', help='Wavetable equation')
    parser.add_argument('-w', '--waves', nargs='+', help='WAV files (1-15)')
    parser.add_argument('-m', '--map', help='Optional wave map file')
    parser.add_argument('-o', '--output', default='wavetable.bin', help='Output .bin file')
    parser.add_argument('--no-phase-align', action='store_true',
                        help='Disable phase alignment for WAV files')
    parser.add_argument('--help-equations', action='store_true',
                        help='Show equation help and examples')
    
    args = parser.parse_args()
    
    if args.help_equations:
        print(__doc__)
        print("\nMore examples:")
        examples = [
            ("sin(phi)", "Pure sine"),
            ("sin(phi) + 0.5 * sin(phi * 2)", "Sine with 2nd harmonic"),
            ("sin(phi) * (1 - t)", "Decaying sine"),
            ("sin(phi * (1 + y * 7))", "Morphing harmonics (vertical)"),
            ("sin(phi) + sin(phi * 2) * x", "Variable 2nd harmonic (horizontal)"),
            ("sin(phi) + sin(phi * (2 + col))", "Per-column harmonics"),
            ("sin(phi * (1 + row * 2 + col))", "Full grid morph"),
            ("sin(phi) ** (1 + y * 3)", "Sine power morph"),
            ("sin(phi) * cos(phi * x * 8)", "AM synthesis"),
        ]
        for expr, desc in examples:
            print(f"  '{expr}' - {desc}")
        sys.exit(0)
    
    if not args.equation and not args.waves:
        parser.print_help()
        sys.exit(1)
    
    if args.equation and args.waves:
        print("Error: Specify either equation (-e) or WAV files (-w), not both",
              file=sys.stderr)
        sys.exit(1)
    
    if args.waves:
        for path in args.waves:
            if not os.path.exists(path):
                print(f"Error: File not found: {path}", file=sys.stderr)
                sys.exit(1)
        success = generate_from_wavs(args.waves, args.output, args.map,
                                     phase_align=not args.no_phase_align)
    else:
        success = generate_from_equation(args.equation, args.output, args.map)
    
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
