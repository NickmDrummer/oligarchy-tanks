#!/usr/bin/env python3
"""Synthesizes the cue sounds for Oligarchy Tanks (NES-flavoured).

Format matches the rest of src/sounds: mono, 22050 Hz, 16-bit PCM.

Voices: pulse (square with a duty cycle and staccato gaps), triangle
(NES bass), noise (snare/cymbals). No lowpass filtering anywhere — the
crisp square edge IS the chiptune character (a softened batch came out
flute-like and was thrown away).

Usage:
    tools/make-cues.py                # write every cue into src/sounds
    tools/make-cues.py stage-start    # only the named one(s)
    tools/make-cues.py --out DIR ...  # write somewhere else

The files in src/sounds are the shipped assets: only regenerate the
ones being deliberately re-tuned.
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 22050


def write_wav(path, samples, peak=0.5):
    top = max(1e-9, max(abs(s) for s in samples))
    gain = peak / top
    frames = b"".join(
        struct.pack("<h", int(32000.0 * min(1.0, max(-1.0, s * gain))))
        for s in samples
    )
    with wave.open(path, "w") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(frames)
    print(path, "frames", len(samples))


def mix(*voices):
    n = max(len(v) for v in voices)
    out = []
    for i in range(n):
        acc = 0.0
        for v in voices:
            if i < len(v):
                acc += v[i]
        out.append(acc)
    return out


def pulse(out, freq, dur, duty=0.25, attack=0.002, release=0.012, decay=0.0):
    n = int(RATE * dur)
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / attack) if attack > 0 else 1.0
        tail = min(1.0, (dur - t) / release) if release > 0 else 1.0
        if decay > 0:
            env *= math.exp(-t * decay)
        v = 1.0 if (math.fmod(freq * i / RATE, 1.0) < duty) else -1.0
        out.append(v * env * tail)


def tri(out, freq, dur, decay=0.0, attack=0.002):
    n = int(RATE * dur)
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / attack) if attack > 0 else 1.0
        if decay > 0:
            env *= math.exp(-t * decay)
        out.append((4.0 * abs(math.fmod(freq * t, 1.0) - 0.5) - 1.0) * env)


def tri_at(buf, at, freq, dur, decay=1.4):
    # A triangle note written at a time offset into an existing buffer: the
    # sparse bass of the stage-start cue needs the melody's clock, not its
    # own sequential one.
    v = []
    tri(v, freq, dur, decay=decay)
    start = int(RATE * at)
    for i, s in enumerate(v):
        j = start + i
        if j < len(buf):
            buf[j] += s


def melody(out, notes, slot, duty=0.25, fill=0.8):
    # notes: (freq, slots); a freq of 0 rests. Every tone is staccato: a
    # pulse through `fill` of its space, silence after — the gaps are
    # exactly what reads as 8-bit rather than as a sung line.
    for f, s in notes:
        d = slot * s
        if f:
            pulse(out, f, d * fill, duty)
            out.extend([0.0] * int(RATE * (d - d * fill)))
        else:
            out.extend([0.0] * int(RATE * d))


def snare_hit(buf, at, vol=1.0, dur=0.09, decay=30.0):
    # A noise burst added into an existing buffer: the drum rides a
    # timeline of its own, under melody and bass.
    n = int(RATE * dur)
    start = int(RATE * at)
    for i in range(n):
        if start + i < len(buf):
            buf[start + i] += (random.random() * 2.0 - 1.0) * math.exp(-i / RATE * decay) * vol


def gameover():
    # The player's approved take: two phrases stepping down an octave
    # apart over an octave-walking bass, closing on the held floor alone
    # (the two dying pulses were cut by the user).
    GO = 0.14
    mel_a, bass_a = [], []
    melody(mel_a, [(329.63, 1), (293.66, 1), (246.94, 1), (220.0, 1),
                   (246.94, 1), (220.0, 1), (196.0, 2)], GO)
    melody(bass_a, [(110.0, 2), (82.41, 2), (73.42, 2), (98.0, 2)], GO, fill=0.9)
    mel_b, bass_b = [], []
    melody(mel_b, [(164.81, 1), (146.83, 1), (123.47, 1), (110.0, 1),
                   (123.47, 1), (110.0, 1), (98.0, 2)], GO)
    melody(bass_b, [(55.0, 2), (82.41, 2), (73.42, 2), (65.41, 2)], GO, fill=0.9)
    tri_tail = []
    tri(tri_tail, 55.0, 1.8, decay=1.4)
    return mix(mel_a, bass_a) + mix(mel_b, bass_b) + tri_tail, 0.58


def victory():
    # The player's approved take: the flagpole run up, then a held chord
    # on its bass.
    VS = 0.085
    v = []
    melody(v, [(523.25, 1), (587.33, 1), (659.25, 1), (698.46, 1),
               (783.99, 1), (880.0, 1), (987.77, 1), (1046.50, 2)],
           VS, duty=0.5)
    chord_a, chord_b = [], []
    pulse(chord_a, 1046.50, 0.65, duty=0.5, decay=1.5)
    tri(chord_b, 130.81, 0.65, decay=1.2)
    return v + mix(chord_a, chord_b), 0.5


def score_tick(freq):
    t = []
    pulse(t, freq, 0.042, duty=0.5, attack=0.001, release=0.008, decay=9.0)
    return t, 0.4


def menu_move():
    m = []
    pulse(m, 1100, 0.035, duty=0.5, attack=0.001, release=0.006, decay=7.0)
    return m, 0.4


def menu_confirm():
    c = []
    melody(c, [(880.0, 1), (1174.66, 1)], 0.07, duty=0.5, fill=0.9)
    return c, 0.42


def stage_start():
    # Stage-start cue, NES stage-select flavour, two voices only. The
    # harmony is the user's loop: Csus4 -> Dsus4 -> F7sus4 -> G7sus4, two
    # beats each (one pass = 3.2 s at 150 bpm, so it repeats cleanly). The
    # lead runs sixteenths through the chord tones — the sus4 fourths are
    # the character. The triangle bass is a real line: four eighths per
    # chord (root, root, fifth, octave) pushing under the lead, kept in
    # C2-G3 where speakers actually reproduce it (the first octave-down
    # pass put it at 33-73 Hz and the user heard nothing). No drums, no
    # held final note: it says "next stage" and gets out of the way.
    BQ = 60.0 / 150.0    # the beat (172 - 22, the user's final pick)
    OCT = 0.5            # the lead an octave down (the user's ask)
    BOCT = 2.0           # the bass one octave up from the written roots
    S16 = BQ / 4.0       # the sixteenth

    # Chord tones in the lead's range: C F G | D G A | Bb C Eb F | C D F G.
    mel = []
    melody_notes = [
        # Csus4
        (392.00, 1), (523.25, 1), (698.46, 1), (783.99, 1),
        (698.46, 1), (523.25, 1), (392.00, 1), (523.25, 1),
        # Dsus4
        (440.00, 1), (587.33, 1), (783.99, 1), (880.00, 1),
        (783.99, 1), (587.33, 1), (440.00, 1), (587.33, 1),
        # F7sus4
        (466.16, 1), (523.25, 1), (622.25, 1), (698.46, 1),
        (622.25, 1), (523.25, 1), (466.16, 1), (523.25, 1),
        # G7sus4 (the run that turns the loop back to C)
        (523.25, 1), (587.33, 1), (698.46, 1), (783.99, 1),
        (698.46, 1), (587.33, 1), (523.25, 1), (392.00, 1),
    ]
    melody(mel, [(f * OCT, n) for f, n in melody_notes], S16,
           duty=0.25, fill=0.62)

    # The bass line: four eighths per chord — root, root, fifth, octave —
    # with a short pluck so it pushes instead of droning. (root, fifth)
    # per chord of the loop; the octave is the root doubled.
    chords = [
        (65.41, 98.00),    # Csus4
        (73.42, 110.00),   # Dsus4
        (87.31, 130.81),   # F7sus4
        (98.00, 146.83),   # G7sus4
    ]
    bass = [0.0] * len(mel)
    for i, (root, fifth) in enumerate(chords):
        t0 = i * 2 * BQ
        for j, f in enumerate((root, root, fifth, root * 2.0)):
            tri_at(bass, t0 + j * BQ / 2.0, f * BOCT, BQ * 0.42, decay=1.8)
    # With the line an octave up (C3-G4) the equal-loudness penalty is much
    # smaller than at C2-G3, so the boost comes back down to +3.5 dB: the
    # bass keeps the weight the user liked one octave lower.
    bass = [v * 1.5 for v in bass]

    return mix(mel, bass), 0.63   # +15% over the approved take (the user's last ask)


CUES = {
    "gameover": gameover,
    "victory": victory,
    "score-tick-1": lambda: score_tick(740),
    "score-tick-2": lambda: score_tick(880),
    "score-tick-3": lambda: score_tick(1100),
    "score-tick-4": lambda: score_tick(1320),
    "menu-move": menu_move,
    "menu-confirm": menu_confirm,
    "stage-start": stage_start,
}


def main(argv):
    out_dir = "src/sounds"
    wanted = []
    i = 0
    while i < len(argv):
        if argv[i] == "--out":
            out_dir = argv[i + 1]
            i += 2
        else:
            wanted.append(argv[i])
            i += 1
    if not wanted:
        wanted = list(CUES)
    os.makedirs(out_dir, exist_ok=True)
    for name in wanted:
        if name not in CUES:
            raise SystemExit("unknown cue: %s (have: %s)" % (name, ", ".join(CUES)))
        samples, peak = CUES[name]()
        write_wav(os.path.join(out_dir, name + ".wav"), samples, peak=peak)


if __name__ == "__main__":
    main(sys.argv[1:])
