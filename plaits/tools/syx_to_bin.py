#!/usr/bin/env python3
"""
Convert DX7 SysEx bank files to Plaits .bin format.

Usage:
    python syx_to_bin.py input.syx [output.bin]
    
The input must be a standard DX7 bulk dump (32 voices, 4104 bytes with headers).
Output will be 4096 bytes (32 patches × 128 bytes each).

If output is not specified, creates input_basename.bin in the same directory.
"""

import sys
import os

# DX7 SysEx bulk dump format:
# - Byte 0: 0xF0 (SysEx start)
# - Byte 1: 0x43 (Yamaha ID)
# - Byte 2: 0x00 (sub-status + channel)
# - Byte 3: 0x09 (format = 32 voice bulk dump)
# - Byte 4-5: 0x20 0x00 (byte count = 4096)
# - Bytes 6-4101: 32 patches × 128 bytes
# - Byte 4102: checksum
# - Byte 4103: 0xF7 (SysEx end)

SYSEX_HEADER_SIZE = 6
SYSEX_FOOTER_SIZE = 2
EXPECTED_SYSEX_SIZE = 4104
EXPECTED_BIN_SIZE = 4096
PATCHES_PER_BANK = 32
PATCH_SIZE = 128


def convert_syx_to_bin(syx_path: str, bin_path: str = None) -> bool:
    """Convert a DX7 SysEx file to Plaits binary format."""
    
    if bin_path is None:
        base = os.path.splitext(syx_path)[0]
        bin_path = base + '.bin'
    
    # Read SysEx file
    try:
        with open(syx_path, 'rb') as f:
            syx_data = f.read()
    except IOError as e:
        print(f"Error reading {syx_path}: {e}", file=sys.stderr)
        return False
    
    # Validate SysEx format
    if len(syx_data) != EXPECTED_SYSEX_SIZE:
        print(f"Error: Expected {EXPECTED_SYSEX_SIZE} bytes, got {len(syx_data)}", 
              file=sys.stderr)
        print("This may not be a standard DX7 32-voice bulk dump.", file=sys.stderr)
        return False
    
    # Check SysEx markers
    if syx_data[0] != 0xF0:
        print("Error: File doesn't start with SysEx marker (0xF0)", file=sys.stderr)
        return False
    
    if syx_data[-1] != 0xF7:
        print("Error: File doesn't end with SysEx end marker (0xF7)", file=sys.stderr)
        return False
    
    if syx_data[1] != 0x43:
        print("Warning: Manufacturer ID is not Yamaha (0x43)", file=sys.stderr)
    
    if syx_data[3] != 0x09:
        print("Warning: Format byte is not 0x09 (32-voice bulk dump)", file=sys.stderr)
    
    # Extract patch data (skip header and footer)
    bin_data = syx_data[SYSEX_HEADER_SIZE:-SYSEX_FOOTER_SIZE]
    
    if len(bin_data) != EXPECTED_BIN_SIZE:
        print(f"Error: Extracted data is {len(bin_data)} bytes, expected {EXPECTED_BIN_SIZE}",
              file=sys.stderr)
        return False
    
    # Verify checksum (optional, just warn)
    checksum = syx_data[-2]
    calculated = 0
    for b in bin_data:
        calculated = (calculated + b) & 0x7F
    calculated = (128 - calculated) & 0x7F
    
    if checksum != calculated:
        print(f"Warning: Checksum mismatch (file: {checksum}, calculated: {calculated})",
              file=sys.stderr)
    
    # Write output
    try:
        with open(bin_path, 'wb') as f:
            f.write(bin_data)
    except IOError as e:
        print(f"Error writing {bin_path}: {e}", file=sys.stderr)
        return False
    
    print(f"Converted {syx_path} -> {bin_path}")
    print(f"  {PATCHES_PER_BANK} patches, {EXPECTED_BIN_SIZE} bytes")
    
    # Print patch names
    print("  Patches:")
    for i in range(PATCHES_PER_BANK):
        offset = i * PATCH_SIZE + 118  # Patch name at offset 118 within each patch
        name = bin_data[offset:offset + 10].decode('ascii', errors='replace').strip()
        print(f"    {i+1:2d}. {name}")
    
    return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    syx_path = sys.argv[1]
    bin_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    if not os.path.exists(syx_path):
        print(f"Error: File not found: {syx_path}", file=sys.stderr)
        sys.exit(1)
    
    success = convert_syx_to_bin(syx_path, bin_path)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
