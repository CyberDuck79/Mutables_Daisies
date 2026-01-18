#!/usr/bin/env python3
"""
Convert Adventure Kid Waveforms (AKWF) to Plaits wavetable .bin format.

This tool helps you browse and convert AKWF single-cycle waveforms into
Plaits-compatible wavetables.

Usage:
    # List available categories in AKWF folder
    python akwf_to_wavetable.py --list /path/to/AKWF
    
    # Convert a category (picks 15 evenly-spaced waveforms)
    python akwf_to_wavetable.py /path/to/AKWF/AKWF_synth -o synth.bin
    
    # Convert specific files
    python akwf_to_wavetable.py /path/to/AKWF/AKWF_0001/*.wav -o basics.bin
    
    # Interactive mode - preview and select
    python akwf_to_wavetable.py --interactive /path/to/AKWF

AKWF Download:
    https://github.com/KristofferKarlAxelEkstrand/AKWF-FREE
    Or: http://www.adventurekid.se/AKRTfiles/AKWF/AKWF.zip

License: AKWF waveforms are CC0 (public domain) by Kristoffer Ekstrand

Requirements:
    pip install numpy scipy
"""

import sys
import os
import glob
import argparse
from pathlib import Path

try:
    import numpy as np
    from scipy.io import wavfile
except ImportError:
    print("Error: This script requires numpy and scipy.", file=sys.stderr)
    print("Install with: pip install numpy scipy", file=sys.stderr)
    sys.exit(1)

# Import conversion functions from wav_to_wavetable
try:
    from wav_to_wavetable import (
        read_wav, resample_to_wavetable, integrate_wave,
        create_default_map, WAVETABLE_SIZE, NUM_CUSTOM_WAVES,
        NUM_BUILTIN_WAVES, MAP_SIZE, EXPECTED_BIN_SIZE
    )
except ImportError:
    # Define inline if wav_to_wavetable not in path
    WAVETABLE_SIZE = 128
    NUM_CUSTOM_WAVES = 15
    NUM_BUILTIN_WAVES = 192
    MAP_SIZE = 8
    EXPECTED_BIN_SIZE = 4096
    
    def read_wav(path: str) -> np.ndarray:
        sample_rate, data = wavfile.read(path)
        if data.dtype == np.int16:
            data = data.astype(np.float32) / 32768.0
        elif data.dtype == np.int32:
            data = data.astype(np.float32) / 2147483648.0
        elif data.dtype == np.uint8:
            data = (data.astype(np.float32) - 128) / 128.0
        elif data.dtype != np.float32:
            data = data.astype(np.float32)
        if len(data.shape) > 1:
            data = data.mean(axis=1)
        return data
    
    def resample_to_wavetable(samples: np.ndarray, max_length: int = 8192,
                              phase_align: bool = True) -> np.ndarray:
        n = min(len(samples), max_length)
        samples = samples[:n]
        m = WAVETABLE_SIZE
        nyquist = min(n, m) // 2
        result = np.zeros(m, dtype=np.float32)
        for i in range(1, nyquist):
            norm = 2.0 / n
            xr = xi = 0.0
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
        n = len(wave)
        wave = wave - wave.mean()
        max_amp = np.abs(wave).max()
        if max_amp > 0:
            wave = wave / max_amp
        extended = np.tile(wave, 2)
        integral = np.cumsum(extended)
        integral_mean = integral[n:].mean()
        integrated = np.zeros(n + 4, dtype=np.int16)
        for i in range(n):
            val = (integral[n + i] - integral_mean) * (4 * 32767.0 / n)
            integrated[i] = int(np.clip(np.round(val), -32768, 32767))
        for i in range(4):
            integrated[n + i] = integrated[i]
        return integrated
    
    def create_default_map() -> np.ndarray:
        map_data = np.zeros(MAP_SIZE * MAP_SIZE, dtype=np.uint8)
        for x in range(MAP_SIZE):
            for y in range(MAP_SIZE):
                j = x * 4 // MAP_SIZE
                k = y * 4 // MAP_SIZE
                wave_idx = min(j + k * 4, NUM_CUSTOM_WAVES - 1)
                map_data[x + y * MAP_SIZE] = NUM_BUILTIN_WAVES + wave_idx
        return map_data


def list_akwf_categories(akwf_path: str) -> list:
    """List all AKWF category folders."""
    path = Path(akwf_path)
    categories = []
    
    for item in sorted(path.iterdir()):
        if item.is_dir() and item.name.startswith('AKWF'):
            wav_count = len(list(item.glob('*.wav')))
            if wav_count > 0:
                categories.append((item.name, wav_count, str(item)))
    
    return categories


def get_wavs_from_folder(folder: str, max_count: int = None) -> list:
    """Get WAV files from a folder, sorted numerically."""
    path = Path(folder)
    
    # Find all wav files
    wavs = list(path.glob('*.wav'))
    
    # Sort numerically by extracting numbers from filename
    def extract_num(p):
        name = p.stem
        # Extract trailing number (e.g., AKWF_0001 -> 1)
        import re
        match = re.search(r'(\d+)$', name)
        return int(match.group(1)) if match else 0
    
    wavs = sorted(wavs, key=extract_num)
    
    if max_count and len(wavs) > max_count:
        # Pick evenly-spaced samples
        indices = np.linspace(0, len(wavs) - 1, max_count, dtype=int)
        wavs = [wavs[i] for i in indices]
    
    return [str(w) for w in wavs]


def preview_wave(wav_path: str, width: int = 60) -> str:
    """Create ASCII preview of a waveform."""
    try:
        samples = read_wav(wav_path)
        # Resample to width
        indices = np.linspace(0, len(samples) - 1, width, dtype=int)
        preview = samples[indices]
        
        # Normalize to 0-4 range
        preview = (preview - preview.min()) / (preview.max() - preview.min() + 1e-10)
        preview = (preview * 4).astype(int)
        
        # ASCII art
        chars = ['▁', '▂', '▃', '▄', '█']
        return ''.join(chars[min(4, max(0, int(v)))] for v in preview)
    except:
        return '?' * width


def convert_wavs_to_bin(wav_paths: list, output_path: str) -> bool:
    """Convert WAV files to Plaits wavetable binary."""
    if len(wav_paths) == 0:
        print("Error: No WAV files provided", file=sys.stderr)
        return False
    
    if len(wav_paths) > NUM_CUSTOM_WAVES:
        print(f"Warning: Only first {NUM_CUSTOM_WAVES} files will be used")
        wav_paths = wav_paths[:NUM_CUSTOM_WAVES]
    
    # Process each wave
    waves = []
    for i, path in enumerate(wav_paths):
        try:
            samples = read_wav(path)
            resampled = resample_to_wavetable(samples)
            integrated = integrate_wave(resampled)
            waves.append(integrated)
            print(f"  [{i+1:2d}] {Path(path).name}: {preview_wave(path, 40)}")
        except Exception as e:
            print(f"Error processing {path}: {e}", file=sys.stderr)
            return False
    
    # Pad to 15 waves
    while len(waves) < NUM_CUSTOM_WAVES:
        waves.append(waves[-1].copy())
    
    # Create binary
    map_data = create_default_map()
    
    with open(output_path, 'wb') as f:
        # Write map (64 bytes)
        f.write(map_data.tobytes())
        
        # Write waves
        for wave in waves:
            f.write(wave.tobytes())
        
        # Pad to 4096 bytes
        current = f.tell()
        if current < EXPECTED_BIN_SIZE:
            f.write(b'\x00' * (EXPECTED_BIN_SIZE - current))
    
    print(f"\nCreated: {output_path} ({EXPECTED_BIN_SIZE} bytes)")
    return True


def interactive_mode(akwf_path: str):
    """Interactive category browser and converter."""
    categories = list_akwf_categories(akwf_path)
    
    if not categories:
        print(f"No AKWF folders found in {akwf_path}")
        return
    
    print("\n=== AKWF Wavetable Converter ===\n")
    print("Categories found:\n")
    
    for i, (name, count, path) in enumerate(categories):
        print(f"  [{i+1:3d}] {name:30s} ({count:4d} waves)")
    
    print(f"\nTotal: {len(categories)} categories")
    print("\nEnter category number to preview, or 'q' to quit:")
    
    while True:
        try:
            choice = input("\n> ").strip()
            
            if choice.lower() == 'q':
                break
            
            idx = int(choice) - 1
            if 0 <= idx < len(categories):
                name, count, path = categories[idx]
                wavs = get_wavs_from_folder(path, 15)
                
                print(f"\n{name} - {count} waves total, selected {len(wavs)}:\n")
                for i, w in enumerate(wavs):
                    print(f"  [{i+1:2d}] {Path(w).name}: {preview_wave(w, 40)}")
                
                output = input(f"\nEnter output filename (or Enter for '{name}.bin'): ").strip()
                if not output:
                    output = f"{name}.bin"
                if not output.endswith('.bin'):
                    output += '.bin'
                
                convert_wavs_to_bin(wavs, output)
            else:
                print("Invalid selection")
        except ValueError:
            print("Enter a number or 'q'")
        except KeyboardInterrupt:
            break


def main():
    parser = argparse.ArgumentParser(
        description='Convert AKWF waveforms to Plaits wavetable format',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    # List categories
    %(prog)s --list ~/Downloads/AKWF
    
    # Convert a category folder
    %(prog)s ~/Downloads/AKWF/AKWF_synth -o synth.bin
    
    # Convert specific files  
    %(prog)s file1.wav file2.wav file3.wav -o custom.bin
    
    # Interactive browser
    %(prog)s --interactive ~/Downloads/AKWF
""")
    
    parser.add_argument('input', nargs='*', 
                        help='WAV files or AKWF folder to convert')
    parser.add_argument('-o', '--output', 
                        help='Output .bin file path')
    parser.add_argument('--list', metavar='PATH',
                        help='List AKWF categories in folder')
    parser.add_argument('--interactive', metavar='PATH',
                        help='Interactive category browser')
    parser.add_argument('--count', type=int, default=15,
                        help='Number of waves to select from folder (default: 15)')
    
    args = parser.parse_args()
    
    # List mode
    if args.list:
        categories = list_akwf_categories(args.list)
        if not categories:
            print(f"No AKWF folders found in {args.list}")
            return 1
        
        print(f"\nAKWF Categories in {args.list}:\n")
        total_waves = 0
        for name, count, path in categories:
            print(f"  {name:35s} {count:5d} waves")
            total_waves += count
        print(f"\n  {'TOTAL':35s} {total_waves:5d} waves")
        return 0
    
    # Interactive mode
    if args.interactive:
        interactive_mode(args.interactive)
        return 0
    
    # Conversion mode
    if not args.input:
        parser.print_help()
        return 1
    
    if not args.output:
        print("Error: Output file required (-o)", file=sys.stderr)
        return 1
    
    # Check if input is a folder or files
    if len(args.input) == 1 and os.path.isdir(args.input[0]):
        wavs = get_wavs_from_folder(args.input[0], args.count)
        print(f"Selected {len(wavs)} waves from {args.input[0]}:")
    else:
        wavs = args.input
        print(f"Converting {len(wavs)} waves:")
    
    success = convert_wavs_to_bin(wavs, args.output)
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
