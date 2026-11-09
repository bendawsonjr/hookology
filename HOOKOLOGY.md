# Hookology

Custom firmware for the M-VAVE FM-1, built on [Melodee](https://github.com/keremimo/melodee) 1.6.1. It is not an official Melodee release, and it has not been made or endorsed by the Melodee, HERA, SLOOP or Felucca authors, or by M-VAVE.

## What's different from Melodee 1.6.1

- **HERA chord sets:** CHRD gains HERA, with 100 sets of 12 chords and a CHORD SET page showing the map. HOME SELECT and GLO SELECT can browse them.
- **TRACKS home page:** the FM-1 starts here. Each lane shows the track's sound and the bar playing; the knobs edit the selected track (see beta10 below). Tap HOME for Stage and the chord wheel.
- **KIT SAMPLE:** an eight-sound acoustic drum kit with five presets. Hold OCT- or OCT+ on HOME for ghost or hard hits.
- **REC MODE:** a menu setting. FREE lets a first take set the tempo and loop length. GRID, the default, records as Melodee always has.
- **Six sound tracks (v1.1-beta):** tracks 5 and 6 are full sound tracks, and tracks 7 and 8 stay MIDI. A new project starts with four: use Add track for more. Turn ALGORITHM past track 4 for the second page of tracks. All six share the same 16 to 18 voices.
- **Overload limit (v1.1-beta6):** the voice budget now uses timing measured on an FM-1, shared effects take their share of it (REVERB about twice the others), and when it is full fading notes go first, then the top note of the fullest chord. While the FM-1 is busy, notes that start together are spread over a few audio blocks (3 to 9 ms), drums first. While a song plays, HOME shows LOAD and the worst audio block just now; OVER means the song passed the limit. BUDGET and WEIGHTS still override the limit by hand.
- **SECTIONS page (v1.1-beta7):** build a song from whole sections, after SLOOP's sections. Section A to H is pattern 1 to 8 on every track. On SEQ > SECTIONS (after PATTERNS), the eight white keys from F3 to F4 are A to H: tap one to play it (while playing, every track switches together at the end of track 1's loop), or hold one for a second to copy what plays now into it. REC records the song: play sections in order, then tap REC or STOP (from beta9, REC while stopped opens BUILD instead). The knobs edit the song as a list of sections and repeats, and PLAY there plays the song. SONG stays for rests, single tracks, presets and punch FX.
- **BUILD on SECTIONS (v1.1-beta9):** with the transport stopped, REC on SECTIONS opens BUILD (BUILD in the header, REC lit). Tap section keys in order to write the song: each tap adds that section after the picked entry, and tapping the section already picked (or the one right after it) adds a repeat instead. KNOB 1 picks an entry to add after, OCT- deletes the picked entry (the same section either side becomes one entry), and OCT- held for a second clears the song after asking. A hold copies nothing in BUILD, and EDIT held undoes. REC again, PLAY or leaving the page ends BUILD. REC while the loop plays still records the song live.
- **Section on TRACKS (v1.1-beta8):** the TRACKS page shows the section playing in the tempo's place: its letter in the track's colour, red while a take or the song records into it (song recording adds a dot), the next section dashed beside it while it waits, and * when the tracks play no one section. The tempo comes back while SELECT turns it.
- **TRACKS like Stage (v1.1-beta10):** TRACKS shows only the lanes (each track's number, sound and every step as Stage shows them, a line of 16 a bar, stacked past 16, with NEXT and a length while a new step count waits for the loop end). Turning a knob brings in a strip of the selected track's four settings, KNOB 1 LEVEL, 2 PAN, 3 SWING, 4 STEPS, and under it that setting for every track of the group side by side; 1.5 seconds after the last turn the lanes come back. Under it all, Stage's footer shows the notes and chord the selected track plays over its waveform.
- **Clear a track on TRACKS (v1.1-beta11):** hold OCT- on TRACKS. After one second the selected track's lane says HOLD TO CLEAR and a red bar fills; keep holding another second and the pattern it plays is cleared (notes and motion; while playing, at the loop end). Let go early and nothing happens; a quick tap still shifts the octave down. EDIT held undoes. On TRACKS, OCT- no longer plays ghost hits on a sampled kit (Stage still does).
- **Safer note delete (v1.1-beta12):** on NOTES, the drum grid and DRUM HIT, tapping EDIT opens the EDIT pages as everywhere else instead of deleting. To delete, hold OCT- for a second: the highlighted note (or every selected note), the drum grid's step, or the hit. A quick OCT- tap still goes back, and EDIT held still undoes.
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
