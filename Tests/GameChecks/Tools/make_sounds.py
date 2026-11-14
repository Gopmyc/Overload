#!/usr/bin/env python3
"""Writes the sounds of the Audio check (Assets/Sounds), with no dependency: 16-bit mono WAV files of a few KB, made
here and so free of any licence.
- Tone.wav: a 440 Hz sine for 0.5 s, faded in and out over 5 ms;
- Loop.wav: a 220 Hz sine for 0.2 s, a whole number of periods, so that it loops without a click;
- Click.wav: 40 ms of noise that dies away, from a fixed seed, a sound short enough to end within a few frames;
- Broken.wav: text with a .wav name, a sound that cannot be read.
Run from anywhere: python3 Tests/GameChecks/Tools/make_sounds.py"""

import math
import os
import random
import struct

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Sounds")
RATE = 22050
SEED = 1701


def write_wav(path, samples):
    """Samples in [-1, 1], written as 16-bit mono PCM"""
    data = b"".join(struct.pack("<h", max(-32767, min(32767, round(sample * 32767)))) for sample in samples)
    with open(path, "wb") as file:
        file.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        file.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, RATE, RATE * 2, 2, 16))
        file.write(b"data" + struct.pack("<I", len(data)) + data)


def sine(frequency, seconds, amplitude, fade=0.0):
    count = round(seconds * RATE)
    fade_count = round(fade * RATE)
    samples = []
    for i in range(count):
        gain = 1.0
        if fade_count > 0:
            gain = min(1.0, i / fade_count, (count - 1 - i) / fade_count)
        samples.append(amplitude * gain * math.sin(2.0 * math.pi * frequency * i / RATE))
    return samples


def click(seconds, amplitude):
    noise = random.Random(SEED)
    count = round(seconds * RATE)
    return [amplitude * math.exp(-6.0 * i / count) * noise.uniform(-1.0, 1.0) for i in range(count)]


def main():
    os.makedirs(OUT, exist_ok=True)
    sounds = {
        "Tone.wav": sine(440.0, 0.5, 0.5, 0.005),
        "Loop.wav": sine(220.0, 0.2, 0.5),
        "Click.wav": click(0.04, 0.8),
    }
    for name, samples in sounds.items():
        write_wav(os.path.join(OUT, name), samples)
        print(f"{name}: {len(samples) / RATE:g} s, {os.path.getsize(os.path.join(OUT, name))} bytes")

    with open(os.path.join(OUT, "Broken.wav"), "w", newline="\n") as file:
        file.write("Not a sound: the Audio check reads this to see that a file that cannot be read gives no sound.\n")
    print("Broken.wav: not a sound")


if __name__ == "__main__":
    main()
