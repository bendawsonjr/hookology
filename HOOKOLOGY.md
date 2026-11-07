# Hookology

Custom firmware for the M-VAVE FM-1, built on [Melodee](https://github.com/keremimo/melodee) 1.6.1. It is not an official Melodee release, and it has not been made or endorsed by the Melodee, HERA, SLOOP or Felucca authors, or by M-VAVE.

## What's different from Melodee 1.6.1

- **HERA chord sets:** CHRD gains HERA, with 100 sets of 12 chords and a CHORD SET page showing the map. HOME SELECT and GLO SELECT can browse them.
- **TRACKS home page:** the FM-1 starts here. Each lane shows swing, steps, level and pan, and the dials edit the selected track. Tap HOME for Stage and the chord wheel.
- **KIT SAMPLE:** an eight-sound acoustic drum kit with five presets. Hold OCT- or OCT+ on HOME for ghost or hard hits.
- **REC MODE:** a menu setting. FREE lets a first take set the tempo and loop length. GRID, the default, records as Melodee always has.
- **Six sound tracks (v1.1-beta):** tracks 5 and 6 are full sound tracks, and tracks 7 and 8 stay MIDI. A new project starts with four: use Add track for more. Turn ALGORITHM past track 4 for the second page of tracks. All six share the same 16 to 18 voices.
- **Overload limit (v1.1-beta5):** the voice budget now uses timing measured on an FM-1, shared effects take their share of it (REVERB about twice the others), and when it is full fading notes go first, then the top note of the fullest chord. While the FM-1 is busy, notes that start together are spread over a few audio blocks (3 to 9 ms), drums first. While a song plays, HOME shows LOAD and the worst audio block just now; OVER means the song passed the limit. BUDGET and WEIGHTS still override the limit by hand.
- **SAFETY on by default:** when the FM-1 nears its limit, GUARD fades the quietest release tails, starting at 80% load. Held notes go only in an emergency: 97% load for 8 audio blocks in a row, about 23 ms, with no tails left. On GLO > WEIGHTS 2, shown as GUARD.
- **Bluetooth removed:** to make room. Build with `MELODEE_BLE=1` to bring it back.

Everything else is Melodee, and its [manual](README.md) still applies, apart from what it says about Bluetooth. Hookology still uses Melodee's USB and MIDI names, so the update tools and recovery can find it, and your computer may show it as Melodee or Felucca.

Projects saved by Melodee 1.6.1 load here. Projects saved here also load in Melodee 1.6.1, but a track set to HERA falls back to AUTO2 and loses its chord set.

### Beta limits on tracks 5 and 6

- Motion (recorded knob moves) works on tracks 1 to 4 only.
- FM6 function settings aren't saved, so they return to their defaults when a project loads.
- USB 4-track recording captures tracks 1 to 4. Tracks 5 and 6 are still in the stereo mix.
- Their page reuses the colours of tracks 1 to 4.
- Saved projects may not open in later betas until Hookology is final. A project with tracks 5 and 6 opened in v1.0 or Melodee loses their sounds.

## Going back to the official firmware

You need the unmodified official V15 `FM-1.fwsc` from M-VAVE. Its SHA-256 must be:

`db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a`

Back up anything you want to keep first, because official firmware can overwrite Melodee's songs, sounds and settings. Then, with the FM-1 charged and connected over USB, run:

```
python3 tools/fm1_install.py /path/to/FM-1.fwsc --no-resources
```

`--no-resources` matters if a `melodee-res.bin` sits beside that file: without it, the tool tries to send it to the official firmware.

The web installer's **Return to official V15** also works with this build.

## Updating

Keep the FM-1 on a HOME page, such as TRACKS, with other MIDI apps closed. On the settings page it may not answer the update tools.

## If an install goes wrong

1. Switch the FM-1 on, or hold **OCT-** alone while switching on. Three short key-light flashes mean the update loader is starting. Reconnect USB and install the same file again.
2. One long flash means the FM-1 is in ROM recovery. Use the `fm1-recover` tool from the Melodee release, as described in [docs/RECOVERY.md](docs/RECOVERY.md).

## Building

See [BUILDING.md](BUILDING.md). In short, with Docker Desktop running:

```
./build.sh
tests/run_tests.sh
```

## Credits

- **Melodee** by Kerem Kilic (Ellic Studio): the base firmware.
- **Felucca** by Leo Kuroshita (@kurogedelic), Hügelton Instruments: the platform Melodee is built on.
- **HERA** by Ben Badio: the 100 chord sets, in `assets/hera/chordsets.json`.
- **SLOOP** by isod89: the home screen layout, the drum kit's cuts and balance, and the free take design.
- **VCSL** by Versilian Studios / Sam Gossner: the drum recordings, CC0 1.0.

[LICENSING.md](LICENSING.md) has the full list, including third-party code and SDK files.

## Licence

GPL-3.0-only. See [LICENSE](LICENSE). The drum samples are CC0 1.0. If you share a build of this firmware, you must also share its complete source under the same licence.
