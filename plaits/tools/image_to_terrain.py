#!/usr/bin/env python3
"""
Convert an image to Plaits wave terrain .bin format.

Usage:
    python image_to_terrain.py input.png [output.bin]
    
Supported image formats: PNG, JPEG, BMP, GIF, etc. (anything PIL supports)
Output will be 4096 bytes (64×64 signed 8-bit heightmap).

The image is converted to grayscale, resized to 64×64, and normalized
to use the full -127 to +127 range.

If output is not specified, creates input_basename.bin in the same directory.

Requirements:
    pip install pillow numpy
"""

import sys
import os

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("Error: This script requires PIL and numpy.", file=sys.stderr)
    print("Install with: pip install pillow numpy", file=sys.stderr)
    sys.exit(1)

TERRAIN_SIZE = 64
EXPECTED_BIN_SIZE = TERRAIN_SIZE * TERRAIN_SIZE  # 4096


def convert_image_to_terrain(image_path: str, bin_path: str = None) -> bool:
    """Convert an image file to Plaits wave terrain binary format."""
    
    if bin_path is None:
        base = os.path.splitext(image_path)[0]
        bin_path = base + '.bin'
    
    # Load and convert image
    try:
        img = Image.open(image_path)
    except IOError as e:
        print(f"Error opening {image_path}: {e}", file=sys.stderr)
        return False
    
    print(f"Input: {image_path}")
    print(f"  Original size: {img.width}×{img.height}, mode: {img.mode}")
    
    # Convert to grayscale
    img = img.convert('L')
    
    # Resize to 64×64 (using high-quality resampling)
    img = img.resize((TERRAIN_SIZE, TERRAIN_SIZE), Image.Resampling.LANCZOS)
    
    # Convert to numpy array
    terrain = np.array(img, dtype=np.float32)
    
    # Normalize to -127..+127 range
    terrain_min = terrain.min()
    terrain_max = terrain.max()
    
    if terrain_max > terrain_min:
        # Scale to 0..1, then to -1..+1, then to -127..+127
        terrain = (terrain - terrain_min) / (terrain_max - terrain_min)
        terrain = (terrain * 2 - 1) * 127
    else:
        # Flat image - set to zero
        terrain = np.zeros_like(terrain)
    
    # Round and convert to signed 8-bit
    terrain = np.round(terrain).astype(np.int8)
    
    # Flip Y axis to match Plaits coordinate system
    # (image Y=0 is top, terrain Y=0 is bottom)
    terrain = np.flipud(terrain)
    
    # Convert to bytes
    # Note: terrain values are signed (-127 to +127)
    # We store them as unsigned bytes where negative values wrap around
    bin_data = terrain.tobytes()
    
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
    
    print(f"Output: {bin_path}")
    print(f"  Size: {TERRAIN_SIZE}×{TERRAIN_SIZE} = {EXPECTED_BIN_SIZE} bytes")
    print(f"  Value range: {terrain.min()} to {terrain.max()}")
    
    # Show ASCII preview
    print("\n  Preview (downsampled):")
    chars = " .:-=+*#%@"
    preview_size = 16
    step = TERRAIN_SIZE // preview_size
    for y in range(preview_size - 1, -1, -1):
        line = "    "
        for x in range(preview_size):
            val = terrain[y * step, x * step]
            idx = int((val + 127) / 255 * (len(chars) - 1))
            idx = max(0, min(len(chars) - 1, idx))
            line += chars[idx]
        print(line)
    
    return True


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    image_path = sys.argv[1]
    bin_path = sys.argv[2] if len(sys.argv) > 2 else None
    
    if not os.path.exists(image_path):
        print(f"Error: File not found: {image_path}", file=sys.stderr)
        sys.exit(1)
    
    success = convert_image_to_terrain(image_path, bin_path)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
