# Factory preset level calibration — P018

Baseline P017. Offline GCC 14.2 optimized C++23, core studio profile 48 kHz, duplicated mono input, each fixture peak normalized to 0.25 (-12.04 dBFS); compare against latency-aligned dry before one-second tail. Two actual audio fixtures, not only harmonic stimuli:
- Locally installed pyannote.audio annotated speech sample, sample.wav SHA256 c319b4abca767b124e41432d364fd7df006cb26bb79d09326c487d606a134e6e; 8.4–10.9 s excerpt (RTTM includes speaking activity). Original media is not redistributed and its underlying license/provenance has not been independently established. Local test only.
- Synthetic female-variant eSpeak 1.48.15, original text "Between human and machine. The future is calling." Command: espeak -v en+f3 -s 145 -w synthetic.wav 'Between human and machine. The future is calling.' Not a human voice recording.
Convert with ffmpeg -v error -ss 8.4 -i sample.wav -t 2.5 -ar 48000 -ac 1 -f f32le speech.f32; for synthetic omit -ss/-t. Exact resulting raw fixture SHA256 speech 7faf45220a7fc79dd5dcd14ed1884960288bcd535e0b8ec647b7e1a01aefa709, synthetic 0fdf07a75be2d5056959833bd26020755f63c2b9191673d9897b2f0ed7faa4bd. Recreate only with authorized audio.
Compile: g++ -O3 -std=c++23 -Wall -Wextra -Wconversion -Werror -Isrc tests/voice_fixture.cpp -o voice_fixture. Invoke FILE 0 120, or batches of 20.

For each preset decompose output into dry contribution D=(1-mix)*aligned dry and scaled wet W=y-D. For a prospective additional wet gain g, normalized output energy is A*g²+2B*g+C with A=sum(W²)/dryEnergy, B=sum(D*W)/dryEnergy, C=sum(D²)/dryEnergy. Average the two fixture coefficients; solve positive root for -2 dB target energy ratio, -3 dB for experimental 15; cap g using 0.65 peak bound (dry peak + g*wet peak), convert to dB, clamp output range, round to 0.5 dB. Then render both again. Holdout existing 48k live harmonic/stereo fixture revealed Broken Packet peak .974667 and Digital Breakdown .876095; reduce their proposed output by 2 and 1 dB respectively, re-render affected rows on both voice inputs and rerun complete harmonic bank.

| Fixture | Before RMS range | After RMS range | Before median | After median | Final maximum peak |
|---|---|---|---|---|---|
| Annotated speech | -18.20 to -0.53 dB | -5.39 to -1.37 dB | -5.19 dB | -2.22 dB | 0.447441 |
| Synthetic voice | -17.59 to -0.36 dB | -4.80 to -0.83 dB | -5.03 dB | -1.88 dB | 0.503560 |

All 120 presets on both fixtures final finite, nonzero and not gain-only dry: minimum shape-error 0.07757 / 0.09304. 17/15 presets initially below -10 dB relative to dry, final 0/0. Existing synthetic bank final max peak .780821, minimum pairwise energy-normalized waveform distance .0910482, all 120 distinct state and PCM hashes. 15/15 local CTests PASS (12.46s). This is a measured RMS/headroom calibration, NOT perceptual loudness (LUFS), intelligibility, actual listening or final curation. Higher input levels, different material, all sample rates and full tails need separate coverage. No signal-path DSP or mix changes, only per-preset OUTPUT trims. No media in product assets.
