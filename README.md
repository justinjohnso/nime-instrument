# NIME Two-Handed Musical Controller

A gesture-augmented, two-handed musical instrument controller built on the Electrosmith Daisy Seed platform. Features scale-locked note articulation, modal control switching, and distance-based waveform morphing for expressive performance.

## Features

- **5-note scale articulation** (left hand) with major pentatonic, blues, and chromatic scales
- **Modal control system** (right hand) for octave shifting, pitch bending, and mode switching
- **Gesture-based timbral control** using VL53L0X time-of-flight sensor (sine ↔ triangle waveform morphing)
- **Latch mode** for sustained notes and chord building
- **Real-time audio synthesis** at 48kHz with polyphonic capabilities
- **Volume control** via analog potentiometer with jitter suppression

## Hardware Requirements

### Core Platform
- **Electrosmith Daisy Seed** (STM32H750, 480MHz ARM Cortex-M7)
- **Audio codec**: Built-in on Daisy Seed (48kHz stereo)

### Input Hardware
- **10 momentary buttons** (note articulation + modifiers)
- **1 analog potentiometer** (10kΩ linear, for volume)
- **1 VL53L0X Time-of-Flight sensor** (I2C, for gesture control)

### Pin Assignments

#### Left Hand (Note Articulation)
- Buttons: D8, D9, D10, D13, D14
- VL53L0X Sensor: I2C1 (SDA=D11, SCL=D12)

#### Right Hand (Modifiers)
- Buttons: D15 (pinky), D16 (ring), D17 (middle), D18 (index), D19 (thumb)

#### Analog Controls
- Volume Pot: A5

See [`CONTROL_REFERENCE.md`](./docs/CONTROL_REFERENCE.md) for complete control mapping.

## Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE with DaisyDuino
- USB cable for programming (Daisy Seed uses DFU mode)

### Dependencies
All dependencies are managed automatically by PlatformIO:
- DaisyDuino (from electro-smith GitHub)
- Adafruit_VL53L0X@^1.2.4
- Wire (I2C library)

### Setup

1. **Clone or download this repository**

2. **Open in PlatformIO:**
   ```bash
   cd nime-midi-controller
   pio run
   ```

3. **Upload to Daisy Seed:**
   - Put Daisy Seed in bootloader mode (hold BOOT button, press RESET)
   - Upload via DFU:
     ```bash
     pio run -t upload
     ```

4. **Monitor serial output** (optional, for debugging):
   ```bash
   pio device monitor --baud 115200
   ```

## Usage

### Basic Operation

1. **Power on** the Daisy Seed
2. **Adjust volume** using the potentiometer (A5)
3. **Play notes** using the 5 left-hand buttons (scale degrees)
4. **Modify timbre** by moving your hand near the distance sensor (closer = brighter)

### Control Reference

#### Left Hand - Note Articulation
- Press buttons to play notes in the current scale
- Multiple buttons can be pressed simultaneously (polyphonic)
- In latch mode, pressing a button toggles the note on/off

#### Right Hand - Modifiers

**Single Button Actions:**
- Index: Octave up
- Pinky: Octave down
- Middle: Momentary sharp (+1 semitone while held)
- Ring: Momentary flat (-1 semitone while held)

**With Thumb (SHIFT) Held:**
- Thumb + Index: Select major pentatonic scale
- Thumb + Middle: Select blues scale
- Thumb + Ring: Select chromatic scale
- Thumb + Pinky: Toggle latch mode
- Middle + Ring: Enter key selection mode

See [`docs/CONTROL_REFERENCE.md`](./docs/CONTROL_REFERENCE.md) for complete control mapping and performance tips.

### Default Settings

- **Key:** C Major
- **Octave:** 4 (middle C range)
- **Scale:** Major Pentatonic
- **Mode:** Single Note
- **Latch:** Off
- **Waveform:** Sine (hand far from sensor)

## Project Structure

```
nime-midi-controller/
├── src/
│   └── main.cpp              # Main application code
├── docs/
│   ├── CONTROL_REFERENCE.md  # Visual control reference
│   ├── ARCHITECTURE_OVERVIEW.md  # Technical architecture
│   ├── CODE_REVIEW_SUMMARY.md    # Development notes
│   └── blog/
│       └── 2025-09-29-nime-instrument-update.md
├── platformio.ini            # PlatformIO configuration
├── README.md                 # This file
└── include/                  # Header files (if any)
```

## Configuration

Key parameters can be adjusted in `src/main.cpp`:

```cpp
// Audio
const float VOLUME_SCALE = 0.5f;          // Max volume (0.0-1.0)

// Distance sensor mapping
const int DISTANCE_MIN = 50;              // Min distance (mm)
const int DISTANCE_MAX = 300;             // Max distance (mm)

// Octave range
const int OCTAVE_MIN = 1;
const int OCTAVE_MAX = 8;

// Thresholds
const int VOLUME_CHANGE_THRESHOLD = 10;   // ADC hysteresis
const int DISTANCE_CHANGE_THRESHOLD = 5;  // Distance hysteresis (mm)
```

## Development

### Building from Source

```bash
# Clean build
pio run -t clean
pio run

# Upload to device
pio run -t upload

# Monitor serial output
pio device monitor --baud 115200
```

### Testing Hardware

1. **I2C Scan:** On startup, the serial monitor displays an I2C device scan
2. **Button Test:** Serial monitor shows button press/release events
3. **Sensor Test:** Distance readings appear when hand movement detected
4. **Volume Test:** Volume changes are logged to serial output

### Troubleshooting

**VL53L0X sensor not detected:**
- Check I2C wiring (SDA=D11, SCL=D12)
- Verify sensor at address 0x29 using I2C scan output
- Ensure 3.3V power supply

**No audio output:**
- Verify Daisy Seed audio codec initialization in serial monitor
- Check volume pot is not at minimum
- Ensure proper audio connections

**Buttons not responding:**
- Verify pull-up resistors or INPUT_PULLUP mode
- Check debounce timing (1000ms default)
- Monitor serial output for button events

## Technical Details

- **Sample Rate:** 48kHz
- **Audio Processing:** Direct oscillator synthesis in audio callback
- **Control Rate:** ~1kHz (1ms loop interval)
- **Sensor Poll Rate:** 20Hz (50ms interval)
- **Synthesis:** Dual oscillator per note (sine + triangle) with equal-power crossfade
- **Polyphony:** 5 simultaneous notes maximum

See [`docs/ARCHITECTURE_OVERVIEW.md`](./docs/ARCHITECTURE_OVERVIEW.md) for detailed technical documentation.

## Musical Design

This instrument is designed around **constraint-based performance**:
- Scale-locked notes eliminate "wrong" notes, encouraging rhythmic exploration
- Gestural timbral control provides visual, audience-readable expression
- Limited buttons with modal switching balance simplicity and capability

See the [blog post](./docs/blog/2025-09-29-nime-instrument-update.md) for design rationale and development notes.

## Future Enhancements

- [ ] Chord mode implementation (major/minor)
- [ ] MIDI output for external synthesizers
- [ ] Additional waveforms (sawtooth, square, custom)
- [ ] ADSR envelopes for dynamic articulation
- [ ] Preset save/recall system
- [ ] Effects chain (reverb, delay, filter)

## License

[Specify your license here]

## Acknowledgments

- Built with [DaisyDuino](https://github.com/electro-smith/DaisyDuino) by Electrosmith
- VL53L0X library by Adafruit
- Developed for New Interfaces for Musical Expression course, NYU
