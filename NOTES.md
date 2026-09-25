
## 2026-09-25 — Answer to "how do I get more synth-y sounds out of this?"

**Short answer: the controller already does its part (since 2026-09-15). Any
remaining sameness is a receiving-app patch-mapping issue, not firmware.**

1. **Does each mode send a distinct PC/CC?** Yes, as of commit `61530e9`
   ("Add per-mode GM Program Change on channel 1", 2026-09-15 16:47 EDT — this
   landed the same day you asked, so if you tested before that commit you'd
   have heard the old "everything sounds the same" behavior). `switchMode()`
   now fires a Program Change on MIDI channel 1 whenever a melodic mode is
   selected, plus once on boot:
   - Chromatic → PC 81 (Lead 2, sawtooth)
   - Scale     → PC 90 (Pad 3, polysynth)
   - Chord     → PC 89 (Pad 2, warm)
   - Custom    → PC 87 (Lead 8, bass+lead)
   - Drum mode is unaffected — it already had its own per-kit Program Change
     via `KIT_PROGRAMS`.

2. **The controller doesn't make sound.** It only sends MIDI (notes + these
   Program Change numbers). Timbre/synth character comes entirely from
   whatever's on the receiving end — a softsynth, DAW instrument track, or
   a multitimbral app like bs-16i — mapping those PC numbers to actual
   patches.

3. **If you're still hearing the same sound across modes after 9/15:** the
   receiver isn't respecting Program Change on that channel. Concretely:
   - **bs-16i / any multitimbral receiver:** confirm channel 1 is loaded
     with a General MIDI soundfont/bank (not a fixed single patch) so PC
     81/89/90/87 actually resolve to different instruments instead of being
     ignored.
   - **A DAW (Ableton, Logic, etc.):** on the MIDI track receiving from the
     device, make sure "respond to Program Change" isn't disabled, or set
     up a rack/multi-instrument that switches patches per incoming PC
     number rather than always playing one fixed instrument.
   - **Quick softsynth test:** route to FluidSynth (or any basic GM
     soundfont player) on channel 1 — GM synths honor PC by spec, so if
     modes still sound identical there, that's a real signal something else
     is wrong (worth re-flashing/re-checking channel routing), not just an
     app-config gap.

4. **Firmware gap check:** none found — this is not a firmware follow-up
   item. (See audit in Paperclip task t_a3b357fa / kanban.)

5. **Practical "more synth-y" suggestion:** if the built-in GM patches
   (saw lead, warm/poly pads) aren't synth-y enough for taste, point the
   receiver at a dedicated synth plugin with a bigger patch library (e.g.
   Surge XT, Vital, or any VST/AU synth) and build a 4-slot multi-instrument
   keyed off PC 81/87/89/90 — same PC numbers already being sent, no
   firmware change needed, just richer patches on the four slots.

**Not yet independently verified:** a live MIDI-monitor capture per mode
(`amidi -d` / DAW MIDI monitor) confirming these exact bytes hit the wire —
blocked in this pass on lack of physical device/MIDI-monitor access from the
agent sandbox. The code path is unambiguous, though, so this is a
confirmation step, not something the answer above depends on.

---

**Ready-to-post reply draft:**

> Good news — this was already fixed on 9/15 (same day as the question,
> commit `61530e9`): each mode now sends its own MIDI Program Change on
> channel 1 (Chromatic=Lead/saw, Scale=Pad/poly, Chord=Pad/warm,
> Custom=Lead/bass). The box itself doesn't make sound though — it just
> tells whatever's listening which patch to use, so if it's still sounding
> the same, check that your receiver (bs-16i, DAW track, etc.) is actually
> honoring incoming Program Change on channel 1 rather than staying locked
> to one patch. If you want it to sound noticeably more "synth," point it
> at a dedicated softsynth (Surge XT/Vital/etc.) with 4 patches mapped to
> those PC numbers — no firmware change needed.
