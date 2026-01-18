#!/usr/bin/env python3
"""
PNG to Boot Logo Converter for Daisy OLED Display (128x64 monochrome)

Converts a PNG image to a C header file with embedded bitmap data.
The bitmap is stored as 1-bit per pixel, packed horizontally (8 pixels per byte, MSB first).

Usage:
    python png_to_logo.py input.png [--width W] [--height H] [--threshold T] [--dither] [--invert] [--output output.h]

Options:
    --width W       Target width (default: 128, max display width)
    --height H      Target height (default: 48, leaving room for text)
    --threshold T   Brightness threshold 0-255 (default: 128, ignored if --dither)
    --dither        Use Floyd-Steinberg dithering instead of threshold
    --invert        Invert colors (white on black vs black on white)
    --output FILE   Output header file (default: logo_bitmap.h)
    --name NAME     C variable name (default: logo_bitmap)
"""

import argparse
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Error: Pillow library required. Install with: pip install Pillow")
    sys.exit(1)


def load_and_resize(input_path: str, width: int, height: int) -> Image.Image:
    """Load image and resize while maintaining aspect ratio."""
    img = Image.open(input_path)
    
    # Handle transparency: composite onto black background for white logos,
    # or white background for dark logos
    if img.mode == 'RGBA' or img.mode == 'LA':
        # Check if image is predominantly light or dark
        # by sampling the alpha-weighted average
        temp = img.convert('LA')  # Luminance + Alpha
        pixels = list(temp.getdata())
        weighted_sum = sum(p[0] * p[1] for p in pixels)  # luminance * alpha
        alpha_sum = sum(p[1] for p in pixels)
        avg_luminance = weighted_sum / alpha_sum if alpha_sum > 0 else 128
        
        # Use black background for light images, white for dark images
        if avg_luminance > 128:
            background = Image.new('RGBA', img.size, (0, 0, 0, 255))  # Black
        else:
            background = Image.new('RGBA', img.size, (255, 255, 255, 255))  # White
        
        img = Image.alpha_composite(background, img.convert('RGBA'))
        print(f"Alpha composite onto {'black' if avg_luminance > 128 else 'white'} background (avg luminance: {avg_luminance:.0f})")
    
    # Convert to grayscale
    if img.mode != 'L':
        img = img.convert('L')
    
    # Calculate resize dimensions maintaining aspect ratio
    orig_w, orig_h = img.size
    aspect = orig_w / orig_h
    target_aspect = width / height
    
    if aspect > target_aspect:
        # Image is wider - fit to width
        new_w = width
        new_h = int(width / aspect)
    else:
        # Image is taller - fit to height
        new_h = height
        new_w = int(height * aspect)
    
    # Resize with high-quality resampling
    img = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
    
    # Create centered image on target canvas
    result = Image.new('L', (width, height), 0)  # Black background
    x_offset = (width - new_w) // 2
    y_offset = (height - new_h) // 2
    result.paste(img, (x_offset, y_offset))
    
    return result


def apply_threshold(img: Image.Image, threshold: int) -> Image.Image:
    """Convert grayscale to 1-bit using simple threshold."""
    return img.point(lambda p: 255 if p > threshold else 0, mode='1')


def apply_dithering(img: Image.Image) -> Image.Image:
    """Convert grayscale to 1-bit using Floyd-Steinberg dithering."""
    return img.convert('1', dither=Image.Dither.FLOYDSTEINBERG)


def image_to_bitmap(img: Image.Image, invert: bool = False) -> bytes:
    """
    Convert 1-bit image to packed bitmap bytes.
    Format: horizontal packing, 8 pixels per byte, MSB = leftmost pixel.
    """
    width, height = img.size
    pixels = list(img.getdata())
    
    # Calculate bytes per row (rounded up to nearest byte)
    bytes_per_row = (width + 7) // 8
    
    bitmap = bytearray()
    
    for y in range(height):
        for byte_x in range(bytes_per_row):
            byte_val = 0
            for bit in range(8):
                x = byte_x * 8 + bit
                if x < width:
                    pixel_idx = y * width + x
                    pixel_on = pixels[pixel_idx] > 0
                    if invert:
                        pixel_on = not pixel_on
                    if pixel_on:
                        byte_val |= (0x80 >> bit)  # MSB first
            bitmap.append(byte_val)
    
    return bytes(bitmap)


def print_ascii_preview(img: Image.Image, invert: bool = False):
    """Print ASCII art preview of the bitmap with correct aspect ratio."""
    width, height = img.size
    pixels = list(img.getdata())
    
    # Use half-block characters to pack 2 vertical pixels per character
    # This corrects for terminal font aspect ratio (~2:1)
    # ▀ = upper half, ▄ = lower half, █ = both, space = neither
    
    print(f"\n┌{'─' * width}┐")
    for y in range(0, height, 2):
        row = "│"
        for x in range(width):
            # Get upper pixel
            upper_on = pixels[y * width + x] > 0
            if invert:
                upper_on = not upper_on
            
            # Get lower pixel (may not exist on last row if height is odd)
            if y + 1 < height:
                lower_on = pixels[(y + 1) * width + x] > 0
                if invert:
                    lower_on = not lower_on
            else:
                lower_on = False
            
            # Choose character based on which pixels are on
            if upper_on and lower_on:
                row += "█"
            elif upper_on:
                row += "▀"
            elif lower_on:
                row += "▄"
            else:
                row += " "
        row += "│"
        print(row)
    print(f"└{'─' * width}┘")
    print(f"Dimensions: {width}x{height} pixels")


def generate_header(bitmap: bytes, width: int, height: int, var_name: str) -> str:
    """Generate C header file content."""
    bytes_per_row = (width + 7) // 8
    total_bytes = len(bitmap)
    
    lines = [
        "#pragma once",
        "",
        "// Auto-generated boot logo bitmap",
        f"// Dimensions: {width}x{height} pixels",
        f"// Format: 1-bit packed, horizontal, MSB first",
        f"// Total size: {total_bytes} bytes",
        "",
        "#include <cstdint>",
        "",
        f"static constexpr uint16_t {var_name}_width = {width};",
        f"static constexpr uint16_t {var_name}_height = {height};",
        f"static constexpr uint16_t {var_name}_bytes_per_row = {bytes_per_row};",
        "",
        f"static const uint8_t {var_name}[{total_bytes}] = {{",
    ]
    
    # Format bytes as hex, 16 per line
    for i in range(0, len(bitmap), 16):
        chunk = bitmap[i:i+16]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        if i + 16 < len(bitmap):
            hex_vals += ","
        lines.append(f"    {hex_vals}")
    
    lines.append("};")
    lines.append("")
    lines.append(f"// Draw function for Daisy OLED")
    lines.append(f"template<typename Display>")
    lines.append(f"inline void Draw{var_name.title().replace('_', '')}(Display& display, int x_offset = 0, int y_offset = 0) {{")
    lines.append(f"    for (int y = 0; y < {var_name}_height; y++) {{")
    lines.append(f"        for (int byte_x = 0; byte_x < {var_name}_bytes_per_row; byte_x++) {{")
    lines.append(f"            uint8_t byte_val = {var_name}[y * {var_name}_bytes_per_row + byte_x];")
    lines.append(f"            for (int bit = 0; bit < 8; bit++) {{")
    lines.append(f"                int x = byte_x * 8 + bit;")
    lines.append(f"                if (x < {var_name}_width) {{")
    lines.append(f"                    bool pixel_on = (byte_val & (0x80 >> bit)) != 0;")
    lines.append(f"                    display.DrawPixel(x_offset + x, y_offset + y, pixel_on);")
    lines.append(f"                }}")
    lines.append(f"            }}")
    lines.append(f"        }}")
    lines.append(f"    }}")
    lines.append(f"}}")
    lines.append("")
    
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Convert PNG to boot logo for Daisy OLED (128x64 monochrome)")
    parser.add_argument("input", help="Input PNG file")
    parser.add_argument("--width", type=int, default=128, help="Target width (default: 128)")
    parser.add_argument("--height", type=int, default=48, help="Target height (default: 48)")
    parser.add_argument("--threshold", type=int, default=128, help="Brightness threshold 0-255 (default: 128)")
    parser.add_argument("--dither", action="store_true", help="Use Floyd-Steinberg dithering")
    parser.add_argument("--invert", action="store_true", help="Invert colors")
    parser.add_argument("--output", default="logo_bitmap.h", help="Output header file")
    parser.add_argument("--name", default="logo_bitmap", help="C variable name")
    parser.add_argument("--no-preview", action="store_true", help="Skip ASCII preview")
    
    args = parser.parse_args()
    
    # Validate
    if not Path(args.input).exists():
        print(f"Error: Input file not found: {args.input}")
        sys.exit(1)
    
    if args.width > 128:
        print("Warning: Width > 128 will exceed OLED display width")
    if args.height > 64:
        print("Warning: Height > 64 will exceed OLED display height")
    
    print(f"Loading: {args.input}")
    
    # Load and resize
    img = load_and_resize(args.input, args.width, args.height)
    print(f"Resized to: {img.size[0]}x{img.size[1]}")
    
    # Convert to 1-bit
    if args.dither:
        print("Applying Floyd-Steinberg dithering...")
        img_1bit = apply_dithering(img)
    else:
        print(f"Applying threshold: {args.threshold}")
        img_1bit = apply_threshold(img, args.threshold)
    
    # Show preview
    if not args.no_preview:
        print("\nASCII Preview:")
        print_ascii_preview(img_1bit, args.invert)
    
    # Convert to bitmap
    bitmap = image_to_bitmap(img_1bit, args.invert)
    print(f"\nBitmap size: {len(bitmap)} bytes")
    
    # Generate header
    header_content = generate_header(bitmap, args.width, args.height, args.name)
    
    # Write output
    output_path = Path(args.output)
    output_path.write_text(header_content)
    print(f"Written to: {output_path}")
    
    print(f"\nUsage in code:")
    print(f"  #include \"{output_path.name}\"")
    print(f"  Draw{args.name.title().replace('_', '')}(hw_->display, 0, 0);")
    print(f"  // Then draw module name below at y = {args.height}")


if __name__ == "__main__":
    main()
