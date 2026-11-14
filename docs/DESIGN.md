# Kaleido: design plan (v0.1)

Kaleido is a MIDI arpeggiator and step-pattern generator. Hold notes in, get patterns out,
with every step of the pattern editable and a set of generators and tools to make new ones.
"Kaleido" is a working name.

## Goals

1. Deep options without menus: everything is on one screen.
2. Patterns that do not loop obviously: each lane has its own length (polymeter), plus
   probability, ratchets, humanise and generative tools.
3. Works everywhere: VST3 (Windows and macOS), AU and AU MIDI FX (macOS, Logic), and a
   standalone app with MIDI in and out ports, on macOS 11 Intel upwards and Windows.

## Architecture

Same layout as Crate12 and Tapehead Echo:

- `Source/engine`: plain C++17, no JUCE, unit tested.
  - `ArpEngine`: sample-accurate MIDI in to MIDI out. It syncs to the host transport when the
    host is playing (steps land on the bar grid and stay locked through loops and jumps) and
    runs on its own clock otherwise, so it plays when the transport is stopped.
  - `Generators`: Euclidean, random, Markov melody and rhythm, mutate, surprise, plus the
    lane tools (rotate, reverse, invert, ramp, humanise, double, clear).
  - `Presets`: plain-text preset format (`.kalp`, readable key=value lines) and the 86 factory
    presets.
- `Source/plugin`: JUCE wrapper. All dialled settings are host-automatable parameters. The
  step lanes and chord memory are saved in the plugin state. A small preview synth lets you
  hear the arp without routing it anywhere (on by default in the standalone app).

## Feature set in v0.1

- **18 modes**: Up, Down, Up/Down, Down/Up (with and without repeated end notes), Converge,
  Diverge, Con/Diverge, Thumb, Pinky, As Played, Random, Random Walk, Shuffle, Chord,
  Strum Up, Strum Down.
- **Octaves** 1 to 4, with Up, Down, Up/Down, Interleave and Random octave order.
- **18 rates** from 1/1 to 1/64, with dotted, triplet, quintuplet and septuplet rates.
- **7 step lanes**, each 1 to 32 steps long: Rhythm (on, rest, tie), Velocity, Gate,
  Octave, Semitone, Probability, Ratchet (1 to 8 repeats inside one step).
- **Feel**: swing, gate scale, note repeat, strum time, humanise timing and velocity,
  velocity from playing, from the lane, or both.
- **Pitch**: transpose, 20 scales with quantise, a "scale steps" mode where the semitone
  lane moves in scale degrees, note range with octave folding, chord memory with Learn.
- **Play**: latch, retrigger, rests can move the note order on, output MIDI channel.
- **Tools**: undo, user presets, 7 colour themes, resizable window.

## Decisions to revisit

- Name and branding.
- Plugin format strategy (see README: VST3 loads as an instrument with MIDI out).
- Whether to add MIDI-learn, a per-lane clock divider, and pattern chaining (A/B/C/D).
- Installer packaging, which comes later as with Scope.
