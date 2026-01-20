# Common UI documentation
*This section documents how to use the UI (user-focused).*
*For developers: for now, the ports’ source code is the best reference for how to use the library.*

## General usage
The cursor is shown as:
-  an underline when selecting a menu item
-  a white background (highlight) when editing a value

Encoder rotation scrolls through menu items.

There are two main menu item types: **submenus** and **parameters**.

### When the cursor is on a parameter
-  **Short press**: enter value edit mode (the value is highlighted)
-  **Encoder rotation**: change the value
-  **Short press**: exit value edit mode and return to item selection
-  **Long press**: open the parameter mapping section (CV/MIDI assignment)  
    Long press again to return to the previous menu.

### When the cursor is on a submenu
-  **Short press**: enter the submenu
-  There is no long-press behavior on submenu items.
-  Inside a submenu, the title at the top is also selectable: scroll to it and **short press** to go back to the main menu.

## Input detection
Mutable Instruments modules often change behavior depending on whether a cable is plugged into certain CV inputs. The UI abstractions replicate this behavior even when Patch hardware cannot detect cables directly.

Example (Plaits): if the CV inputs for **Frequency**, **Timbre**, or **Morph** are not considered “plugged in”, the attenuverter amount controls internal envelope modulation instead of external CV modulation.

In the following sections (knob and CV input abstractions), the documentation indicates when the abstraction reports “plugged in” vs “plugged out” to the original firmware, so you can still rely on the original MI manuals.

## The knob abstraction
On Daisy Patch, knobs and CV inputs are electrically linked (the knob value is summed with the CV input). To emulate a “real knob” behavior, the library supports a **virtual plugged/unplugged state**.

While there is no CV mapping, or while the CV mapping state is **Unplugged**, the abstraction reports **input not plugged in** to the original firmware. This allows original MI behaviors to be preserved.

> **Note**: You can physically plug a cable into the CV input even while the mapping state is **Unplugged**, but the original firmware will still behave as if nothing is plugged in (e.g., Plaits attenuverters will continue to control internal envelope modulation).

When a knob is mapped to a CV input, an additional **Plugged** ON/OFF parameter appears. It has two functions:
-  It controls whether the abstraction reports **input plugged in** to the original firmware.
-  When set to **ON**, it captures an **origin offset** from the current CV input value.

The origin offset helps emulate attenuverter behavior even though Patch sums knob + CV. It allows modulation to be applied **relative to the knob position**, even if the code cannot separate “knob contribution” from “external CV contribution”.

> **Warning:** After capturing the origin offset, changing the physical knob position can lead to incorrect attenuverter emulation.

Knobs can also be mapped to **MIDI CC**. In this remote mapping mode, the CC value maps directly to the parameter value. The CV input is not considered “plugged in”, so attenuverters do not affect CC mapping.

**Velocity mod** controls the amount of modulation coming from **MIDI velocity** information.

## CV input abstraction
The CV input abstraction is for inputs that are **not** associated with a knob (typically signal/CV/audio inputs). It is simpler than the knob abstraction:
-  These inputs cannot be edited manually in the UI.
-  By default, the abstraction reports **input not plugged in** to the original firmware.
-  When mapped to a CV input, it automatically reports **plugged in**.