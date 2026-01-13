#!/usr/bin/env python3
"""
Generate Plaits wave terrain from equations or images.

Usage:
    # From equation
    python terrain_generator.py -e "sin(5 * (y + theta))" -o terrain.bin
    
    # From image
    python terrain_generator.py -i image.png -o terrain.bin
    
    # List available variables for equations
    python terrain_generator.py --help-equations

Equation variables:
    x, y      : coordinates from -1 to +1
    r         : radius = sqrt(x² + y²)
    theta     : angle = atan2(y, x)
    mu        : normalized angle = (|theta| - π) / -π
    pi        : 3.14159...
    
Built-in functions:
    sin, cos, tan, atan, atan2, floor, ceil, round, random,
    sqrt, exp, log, pow, abs, sign
    
    ball(xm, ym, std) : Gaussian bump centered at (xm, ym)

Example equations:
    "sin(5 * theta)"                     : Star pattern
    "sin(5 * (y + theta))"               : Classic wave terrain
    "sin(x * 5) * sin(y * 5)"            : Grid
    "ball(0, 0, 0.5)"                    : Dome
    "sin(r * 10)"                        : Ripples
    "sin(x * 3) + cos(y * 4)"            : Interference

Requirements:
    pip install pillow numpy
"""

import sys
import os
import argparse
import math

try:
    from PIL import Image
    import numpy as np
except ImportError:
    print("Error: This script requires PIL and numpy.", file=sys.stderr)
    print("Install with: pip install pillow numpy", file=sys.stderr)
    sys.exit(1)

TERRAIN_SIZE = 64
EXPECTED_BIN_SIZE = TERRAIN_SIZE * TERRAIN_SIZE  # 4096


def evaluate_terrain_equation(expression: str) -> np.ndarray:
    """Evaluate a terrain equation on a 64×64 grid."""
    
    # Build the evaluation function
    # Export math functions
    exported = ['PI', 'sin', 'cos', 'tan', 'atan', 'atan2', 'floor', 'ceil',
                'round', 'random', 'sqrt', 'exp', 'log', 'pow', 'abs', 'sign']
    
    lib = ' '.join(f'let {x.lower()} = Math.{x};' for x in exported)
    lib += ' let r = Math.sqrt(x * x + y * y);'
    lib += ' let theta = Math.atan2(y, x);'
    lib += ' let mu = (Math.abs(theta) - Math.PI) / -Math.PI;'
    lib += ' let ball = function(xm, ym, std) { return Math.exp(-((x - xm) * (x - xm) + (y - ym) * (y - ym)) / (std * std)); };'
    
    # Create Python equivalent
    def make_terrain_func(expr):
        def f(x, y):
            r = math.sqrt(x * x + y * y)
            theta = math.atan2(y, x)
            mu = (abs(theta) - math.pi) / -math.pi
            
            def ball(xm, ym, std):
                return math.exp(-((x - xm)**2 + (y - ym)**2) / (std * std))
            
            # Expose math functions
            sin, cos, tan = math.sin, math.cos, math.tan
            atan, atan2 = math.atan, math.atan2
            floor, ceil = math.floor, math.ceil
            sqrt, exp, log, pow_f = math.sqrt, math.exp, math.log, math.pow
            pi = math.pi
            
            try:
                return eval(expr)
            except Exception as e:
                raise ValueError(f"Error evaluating '{expr}': {e}")
        return f
    
    f = make_terrain_func(expression)
    
    # Evaluate on grid
    terrain = np.zeros((TERRAIN_SIZE, TERRAIN_SIZE), dtype=np.float32)
    
    for i in range(TERRAIN_SIZE):
        for j in range(TERRAIN_SIZE):
            x = 2.0 * j / (TERRAIN_SIZE - 1) - 1.0
            y = 2.0 * i / (TERRAIN_SIZE - 1) - 1.0
            terrain[i, j] = f(x, y)
    
    return terrain


def convert_image_to_terrain(image_path: str) -> np.ndarray:
    """Convert an image file to terrain array."""
    
    img = Image.open(image_path)
    img = img.convert('L')
    img = img.resize((TERRAIN_SIZE, TERRAIN_SIZE), Image.Resampling.LANCZOS)
    terrain = np.array(img, dtype=np.float32)
    
    # Flip Y axis
    terrain = np.flipud(terrain)
    
    return terrain


def normalize_terrain(terrain: np.ndarray) -> np.ndarray:
    """Normalize terrain to -127..+127 range."""
    
    terrain_min = terrain.min()
    terrain_max = terrain.max()
    
    if terrain_max > terrain_min:
        terrain = (terrain - terrain_min) / (terrain_max - terrain_min)
        terrain = (terrain * 2 - 1) * 127
    else:
        terrain = np.zeros_like(terrain)
    
    return np.round(terrain).astype(np.int8)


def print_terrain_preview(terrain: np.ndarray):
    """Print ASCII preview of terrain."""
    
    chars = " .:-=+*#%@"
    preview_size = 16
    step = TERRAIN_SIZE // preview_size
    
    print("\n  Preview:")
    for y in range(preview_size - 1, -1, -1):
        line = "    "
        for x in range(preview_size):
            val = terrain[y * step, x * step]
            idx = int((val + 127) / 255 * (len(chars) - 1))
            idx = max(0, min(len(chars) - 1, idx))
            line += chars[idx]
        print(line)


def generate_terrain(expression: str = None, image_path: str = None, 
                     bin_path: str = "terrain.bin") -> bool:
    """Generate terrain from equation or image."""
    
    if expression:
        print(f"Generating terrain from equation: {expression}")
        try:
            terrain = evaluate_terrain_equation(expression)
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            return False
    elif image_path:
        print(f"Generating terrain from image: {image_path}")
        try:
            terrain = convert_image_to_terrain(image_path)
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            return False
    else:
        print("Error: Must specify either equation (-e) or image (-i)", file=sys.stderr)
        return False
    
    # Normalize
    terrain = normalize_terrain(terrain)
    
    # Convert to bytes
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
    
    print_terrain_preview(terrain)
    
    return True


def main():
    parser = argparse.ArgumentParser(
        description='Generate Plaits wave terrain from equations or images',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('-e', '--equation', help='Terrain equation (e.g., "sin(5 * theta)")')
    parser.add_argument('-i', '--image', help='Input image file')
    parser.add_argument('-o', '--output', default='terrain.bin', help='Output .bin file')
    parser.add_argument('--help-equations', action='store_true', 
                        help='Show equation help and examples')
    
    args = parser.parse_args()
    
    if args.help_equations:
        print(__doc__)
        print("\nMore examples:")
        examples = [
            ("sin(5 * theta)", "Star pattern"),
            ("sin(5 * (y + theta))", "Classic wave terrain"),
            ("sin(x * 5) * sin(y * 5)", "Grid"),
            ("ball(0, 0, 0.5)", "Dome/bump"),
            ("sin(r * 10)", "Ripples from center"),
            ("sin(x * 3) + cos(y * 4)", "Interference pattern"),
            ("sin(r * 8) * exp(-r * 2)", "Damped ripples"),
            ("sin(theta * 6) * r", "Propeller"),
            ("sin(x * 10) * sin(y * 10) * exp(-r)", "Peaked grid"),
        ]
        for expr, desc in examples:
            print(f"  '{expr}' - {desc}")
        sys.exit(0)
    
    if not args.equation and not args.image:
        parser.print_help()
        sys.exit(1)
    
    if args.image and not os.path.exists(args.image):
        print(f"Error: File not found: {args.image}", file=sys.stderr)
        sys.exit(1)
    
    success = generate_terrain(args.equation, args.image, args.output)
    sys.exit(0 if success else 1)


if __name__ == '__main__':
    main()
