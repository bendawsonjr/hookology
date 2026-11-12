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
- **Fewer SEQ pages (v1.1-beta13):** SEQ shows NOTES, PATTERN and SECTIONS (plus MOTION and DRUM HIT when they apply). PATTERNS and SONG are hidden; their code stays, and a build with HOOKOLOGY_PATTERNS_PAGE=1 or HOOKOLOGY_SONG_PAGE=1 brings either back. Holding SEQ opens SECTIONS. TEMPO moved to GLO, right after MIXER.
- **Whole-section repeats (v1.1-beta14):** in the song, one repeat of a section entry (every track on the same section, as SECTIONS and BUILD make them) lasts as long as the section's longest track with notes, so A x2 plays the whole section twice. Shorter tracks loop inside it. Live SONG REC converts what it counted into whole-section repeats. Entries mixed on the old SONG page (shown as *) keep track 1's timing.
- **Vertical MIXER (v1.1-beta15):** GLO > MIXER shows a channel strip per track side by side: its number, sound, pan, a vertical fader with a live meter beside it, and the level (dB on wide strips) or MUTE. The knobs are unchanged: ALGORITHM picks the track, KNOB 1 LEVEL, 2 PAN, 4 MUTE.
- **Six tracks to start (v1.1-beta16):** power-on, a blank project and New Song (with all four roles chosen) start with all six sound tracks; tracks 5 and 6 have their power-on sounds (FM6, Prophet). Saved projects keep their own track count. Hold OCT+ on a home page for Add track / Remove track as before.
- **Live section switch waits for the whole section (v1.1-beta17):** a section tapped on SECTIONS while playing starts when the section playing reaches its loop end — its longest track with notes (the rule a song entry uses since beta14), not track 1's. An empty track doesn't count. The waiting pad breathes as before.
- **Templates keep all six tracks (v1.1-beta18):** SAVE > PROJECT, SLOT TMPL now stores tracks 5 and 6 too (engine, preset, settings, FM6 functions, patch). To fit the settings sector the template keeps only each track's own engine's patch (TPLM, 2,544 bytes, was 3,044). A template saved before loads tracks 1–4 as before and gives tracks 5 and 6 their power-on sounds; save it again to keep 5 and 6.
- **Drum levels from TRACKS (v1.1-beta19):** on a drum track, hold a drum key and turn KNOB 1: that drum's level (the same as EDIT > LANES). The strip names the drum and shows the kit's eight levels under it; let go and KNOB 1 is the track's LEVEL again. The held key plays (and records with REC on). New Song from a template keeps the template's tracks (5 and 6 too) when every role is kept.
- **The held drum's own strip (v1.1-beta20):** with a drum key held on TRACKS, all four knobs are that drum's: KNOB 1 LEVEL, KNOB 2 TUNE, KNOB 3 DECAY, KNOB 4 TONE (SNAPPY / SNAP / ATTACK by kit), the same values as EDIT > LANES and EDIT > DRUM SOUND. PAN, SWING and STEPS come back when the key is let go. (Per-drum pan: not possible without a stereo drum engine; not planned.)
- **MIDI OUT ALL (v1.1-beta21):** a new menu setting, MIDI OUT: KEYS (as Melodee: the keys you play) or ALL: every note tracks 1..6 play also goes out over USB/Bluetooth MIDI, track k on channel k (patterns, takes, chords, strums, ratchets, arps, sections, the song; the keys once). Mute and solo silence a track's MIDI too; notes that come in over MIDI are never sent back (no loop through a DAW); stop, pattern and section changes, mute and turning it off release what is held. Tracks 7..8 keep their own CH. Saved with the device settings (menu byte 24 bit 7). For recording a song into a DAW: turn the DAW track's monitoring off.
- **ROUT CH1-8 (v1.1-beta21):** GLO > SYSTEM ROUT gains CH1-8 (appended, saved id 5): MIDI IN channels 1..8 play tracks 1..8, so a DAW can play MIDI OUT ALL's recording back into all six sound tracks and the MIDI tracks.
- **A DAW's Start plays the song on SECTIONS (v1.1-beta22):** with CLK on USB or TRS, a song built and the FM-1 stopped on SECTIONS, the song stays cued (prepared as for PLAY, refreshed every quarter second), so the DAW's Start, or its Continue from the top (Song Position 0), plays the song from its first entry on that downbeat, as PLAY there does. Any other page, CLK INT, or no song: the loop, as before. Continue from a later position: the loop (a song seek is not done).
- **MIXER on HOME (v1.1-beta23):** the mixer moved from GLO to HOME, whose pages now turn TRACKS > MIXER > (Stage, hidden for now) > the chord view (GLO opens TEMPO first). KNOB 1 LEVEL, 2 PAN, 3 FX SEND (CHAIN's send, or SPLIT's to slot 1), 4 MUTE of the selected track, the value turned shown in the header ("T2 PAN L20"). Each strip: PAN and SEND as sliders, the sound's name sideways (turned -90 degrees, reading up) beside a taller fader and the meter; the level number is gone. HOME SELECT gains PAGES (after BPM): SELECT then turns HOME's pages both ways, as in the other sections (GLO SELECT does not offer it).
- **SONG LOOP on SECTIONS (v1.1-beta23):** OCT+ on SECTIONS (not in BUILD or SONG REC) turns the song's SONG LOOP on or off (the setting SONG's sheet had, saved with the song): stopped, an edit EDIT held undoes; playing, at once, for the song playing. The header says LOOP on SECTIONS while it is on, SONG LOOP while the song plays. PLAY on HOME's MIXER plays the song as on SECTIONS when SONG LOOP is on (LOOP off, or no song: the loop, as on TRACKS), so a looped song can play while mixing.
- **Stage hidden (v1.1-beta23):** HOME turns TRACKS > MIXER > the chord view for now (changes to Stage to come); a build with HOOKOLOGY_STAGE_PAGE=1 brings it back. Adding a track (OCT+ held) works on any HOME page.
- **SAVE's page order (v1.1-beta23):** SAVE opens PROJECT first, then USER (OCT+ its sheet on Save here, as before), then the presets (ALL ..), then PROJECT again; SELECT turns them in the same order.
- **VOICES and a smarter SAFETY (v1.1-beta24):** EDIT > VOICE (the list's last row; VOICE 3 KNOB 2): VOICES, ALL (default) or 1..8, the track's voice limit (P_VMAX in a retired slot, kept across sound loads); a note past it reuses the oldest released voice, else the oldest held but the lowest. SAFETY now fades the costliest engine's release tail first (voice_units: a Prophet's before a drum's), the quietest of those; it acts after one half over 90 % as well as two over 80 %, and takes two tails a half over 97 %. Measured on the user's Bop (a Prophet with NOTE TAIL beside five tracks): 80 deadline misses in 30 s, none with the Prophet track muted.
- **A Prophet's QUALITY, FULL or LIGHT (v1.1-beta25):** EDIT > VOICE, the row after VOICES on a Prophet track (P_P5Q in a retired slot, kept across sound loads). LIGHT renders the voice's oscillators and filter at half rate (the cutoff an octave up in its steps, coefficients every other half-rate sample unless osc B moves the filter), back to the full rate by a four-point half-band midpoint; envelopes and amplifier stay at the full rate. On the host it costs 57 % of FULL; tone below 4 kHz within 1 dB per octave band (Whiny Opener within 0.3 dB); bright programs lose 3 to 10 dB above about 12 kHz. FULL's kernel (p5_samples) is unchanged, chosen per voice by p5_kernel (tests/prophet_quality_test.c). A coefficient hold for FULL was tried and dropped: no measurable gain on the host.
- **The budget counts LIGHT as FULL (v1.1-beta27):** beta26 counted a LIGHT Prophet voice 6 units (FULL 9); on Bop (track 3 LIGHT, VOICES ALL, 30 s) the budget stole fewer notes (129 against 192) but the processor then faded more (175 against 107), missed a deadline and reached 3155 us; at FULL's count (beta25): no miss, 604 halves over 80 %, worst 2825 us. Back to FULL's count; the other engines' and effects' units are on the low side for such a mix (a calibration for later).
- **CZ-1 QUALITY LIGHT (v1.1-beta29):** QUALITY shows on CZ-1 tracks too. LIGHT (cz_native_light, .audio_text) renders both lines at half rate (twice the increment, the DC tracker twice as fast, the wave inlined), mixes them and brings them up by the same four-point half-band midpoint; about 60 % of FULL's host cost. FAT ENSEMBLE and BRASS 2 within 0.03 dB below 4 kHz (they hold almost nothing above 6 kHz); bright tones lose top end above about 9 kHz. FULL's CZ-1 code is unchanged: beta28 inlined its wave for everyone (bit-identical, 12 % less on the host) but the FM-1 showed no gain (Bop, track 3 FULL: 64 deadline misses against 53 to 56), so it was undone. On Bop with track 3 FULL, tracks 2 and 4 on LIGHT halved the misses (64 to 32). The budget counts LIGHT as FULL.
- **KNOB 1 picks, KNOB 2 changes (v1.1-beta30):** everywhere a row is chosen and then changed, KNOB 1 now moves through the rows and KNOB 2 changes the value: the HOME menu, list pages (EDIT > VOICE, SCL ..), action sheets, the FX and SHARED racks (KNOB 1 the slot, 2 the effect, 4 the amount; SHARED's 3 ROUTE), MOD (KNOB 1 the route, 2 its source), MOTION (KNOB 1 the lane, 2 Play; SEQ + KNOB 1 clears the lane), the sound designer's FEATURES and the Bluetooth page. On MOD and the racks the physical knobs trade places (ui.c knob_slot) and the cards with them. A value list (OCT+ on a setting) also steps with KNOB 1; the effects' list keeps KNOB 1 the family, 2 the effect. The browser, slot pages and ordinary pages were already left to right.
- **ECONOMY (v1.1-beta31):** GLO > TEMPO KNOB 4, ECO OFF/ON. On, every track plays its QUALITY LIGHT, whatever its own setting (core.h trk_light); the header shows ECO. Saved with the project (wire byte PROJ_FX_MODE_OFF + 1; older projects read OFF). On Bop with every track FULL it gave the same result as setting each to LIGHT: no glitches.
- **QUALITY LIGHT for FM6 (v1.1-beta32) and SID (v1.1-beta33):** the same half-rate idea as the Prophet's and CZ-1's. FM6 (fm6_core.c fm6_run_light) runs its operators at half rate: about 80% of FULL's work, tone the same to 6 kHz; on the FM-1 a held FM6 pad in Bop had 17 to 30% fewer busy moments. SID (eng_sid.c sid_render_light, a copy so FULL is untouched) retunes its filter and output stage for half rate: about 65 to 73% of FULL's work, tone the same to 4 kHz and darker above. Both appear as EDIT > VOICE QUALITY and follow ECONOMY.
- **No LIGHT for DRUM (v1.1-beta34/35):** on the FM-1 Bop's two kits cause about half its busy moments, but half-rate playback of the recorded kits gained nothing there (beta34, removed in beta35); the drums' cost on the FM-1 is not the per-sample work the host measures. Next step if wanted: time the drum code on the FM-1 itself.
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
