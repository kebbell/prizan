# Prizan

A one-shot sampler that runs in the browser. Drop in a synth, lead, organ or any one-shot and reshape it.

## Use it

Open `index.html` in a browser. No build step and nothing to install.

- **Load:** drag an audio file (WAV, AIFF, MP3, FLAC) onto the screen, or click **Load file**.
- **Play:** click the screen, press **Space**, or click **Play**.
- **Knobs:** Attack, Decay, Release, Flanger, Pitch, Tone, and a big **Width** knob. Drag up or down, hold Shift for fine moves, double-click to reset.
- **Envelope points:** ⌘/Ctrl-click the screen to add a point, drag it to reshape the volume, double-click it to remove it. Alt-drag a line between points to bend it.
- **Stretch:** drag the grip on the right edge of the screen to lengthen or shorten the sample without changing its pitch.
- **Bottom bar:** Mix (dry against processed), flanger rate and feedback, volume, and Reverse.
- **Save WAV:** renders your version to a 16-bit stereo WAV.

## Plugin (VST3 / AU)

`plugin/` builds Prizan as a VST3 and Audio Unit instrument for Ableton and other DAWs (macOS), using JUCE 8.

```bash
cd plugin
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j10
```

The build installs `Prizan.vst3` to `~/Library/Audio/Plug-Ins/VST3` and `Prizan.component` to `~/Library/Audio/Plug-Ins/Components`. In Ableton, open Settings > Plug-Ins, turn on VST3 and Audio Units, and click Rescan.

In the plugin: play MIDI notes (C3 is the sample's original pitch), drop a sample on the window, and use **Drag to DAW** to drag your processed sound straight onto a track. Renders are kept in `~/Music/Prizan/Renders`.
