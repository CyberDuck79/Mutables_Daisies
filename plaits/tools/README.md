# Plaits User Data Conversion Tools

These Python scripts convert various file formats to the `.bin` format that Plaits can load from SD card.

## Requirements

```bash
pip install pillow numpy scipy
```

## Tools

### 1. `syx_to_bin.py` — DX7 SysEx to FM Bank

Convert standard DX7 bulk dump files (32 voices) to Plaits 6-op FM bank format.

```bash
# Basic usage
python syx_to_bin.py my_patches.syx

# Specify output file
python syx_to_bin.py my_patches.syx output.bin
```

**Input:** Standard DX7 SysEx bulk dump (4104 bytes with headers)
**Output:** 4096 bytes (32 patches × 128 bytes)

Place the output in one of these SD card folders:
- `/plaits/user_data/six_op_bank_1/`
- `/plaits/user_data/six_op_bank_2/`
- `/plaits/user_data/six_op_bank_3/`

---

### 2. `terrain_generator.py` — Equation/Image to Wave Terrain

Generate wave terrain from math equations or images.

```bash
# From equation
python terrain_generator.py -e "sin(5 * (y + theta))" -o terrain.bin

# From image
python terrain_generator.py -i terrain.png -o terrain.bin

# Show equation examples
python terrain_generator.py --help-equations
```

**Equation variables:**
| Variable | Description |
|----------|-------------|
| `x`, `y` | Coordinates from -1 to +1 |
| `r` | Radius = sqrt(x² + y²) |
| `theta` | Angle = atan2(y, x) |
| `mu` | Normalized angle |

**Built-in functions:** `sin`, `cos`, `tan`, `sqrt`, `exp`, `log`, `pow`, `abs`, `ball(xm, ym, std)`

**Example equations:**
```bash
"sin(5 * theta)"              # Star pattern
"sin(5 * (y + theta))"        # Classic wave terrain
"sin(x * 5) * sin(y * 5)"     # Grid
"ball(0, 0, 0.5)"             # Dome/bump
"sin(r * 10)"                 # Ripples from center
```

**Output:** 4096 bytes (64×64 signed int8 heightmap)

Place the output in: `/plaits/user_data/wave_terrain/`

---

### 3. `wavetable_generator.py` — Equation/WAV to Wavetable

Generate wavetable from math equations or WAV files.

```bash
# From equation (generates 15 waves morphing through parameters)
python wavetable_generator.py -e "sin(phi) + sin(phi * 2) * y" -o wavetable.bin

# From WAV files
python wavetable_generator.py -w wave1.wav wave2.wav wave3.wav -o wavetable.bin

# Show equation examples
python wavetable_generator.py --help-equations
```

**Equation variables:**
| Variable | Description |
|----------|-------------|
| `t` | Time/phase (0 to 1) |
| `phi` | Phase in radians (0 to 2π) |
| `x`, `y` | Grid position (0 to 1) for morphing |
| `row`, `col` | Integer grid position (0 to 3) |
| `i`, `n` | Sample index and total samples |

**Example equations:**
```bash
"sin(phi)"                        # Pure sine
"sin(phi) + sin(phi * 2) * x"     # Variable 2nd harmonic
"sin(phi * (1 + y * 7))"          # Morphing harmonics
"sin(phi) ** (1 + y * 3)"         # Sine power morph
```

**Output:** 4096 bytes (64-byte map + 15 integrated wavetables)

Place the output in: `/plaits/user_data/wavetable/`

---

### 5. `image_to_terrain.py` — Image to Wave Terrain (legacy)

Simpler image-only converter (use `terrain_generator.py` for equation support).

```bash
python image_to_terrain.py terrain.png output.bin
```

---

### 6. `wav_to_wavetable.py` — WAV Files to Wavetable (legacy)

Simpler WAV-only converter (use `wavetable_generator.py` for equation support).

```bash
python wav_to_wavetable.py wave1.wav wave2.wav -o wavetable.bin
```

---

## Wave Map Format

The optional map file is an 8×8 grid of wave indices (0-206):
- 0-191: Built-in Plaits waves
- 192-206: Custom waves (your uploaded WAV files)

Example `map.txt`:
```
# Row 7 (top, Y=1.0)
192 193 194 195 196 197 198 199
# Row 6
192 193 194 195 196 197 198 199
# ... rows 5-2 ...
192 193 194 195 196 197 198 199
# Row 0 (bottom, Y=0.0)
192 193 194 195 196 197 198 199
```

Use negative numbers for custom wave shorthand: `-1` = wave 192, `-2` = wave 193, etc.

## SD Card Folder Structure

```
/plaits/
  presets/
    preset_01.txt
    preset_02.txt
    ...
  user_data/
    six_op_bank_1/
      default.bin      (optional, auto-loaded)
      my_bass.bin
      my_keys.bin
    six_op_bank_2/
      ...
    six_op_bank_3/
      ...
    wavetable/
      default.bin
      my_waves.bin
    wave_terrain/
      default.bin
      mountains.bin
      waves.bin
```

## Alternative: Web Editor

The official Plaits web editor can also export `.bin` files directly:
- **Wavetable editor:** Has "Download" button for `.bin` format
- **Wave terrain editor:** Only exports WAV (use `image_to_terrain.py` instead)
- **FM patch editor:** Only exports WAV (use `syx_to_bin.py` with standard DX7 SysEx files)

Web editor: https://pichenettes.github.io/plaits-editor/
