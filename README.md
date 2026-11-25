# 125A CYBERATOR
**Granular · Spectral · Reverse FX — Between Human and Machine.**

Status: P000 / concept baseline; no executable plugin yet. Owner approved 2026-11-25. One standalone VST3 audio effect, C++ with JUCE; Windows x64 primary, portable design, Linux proposed secondary target. No external plugin or speech generator required. Input may be recorded/synthetic speech, vocals, instruments or percussion.

## Product contract
Four independently usable, combinable engines:
- GRANULAR: SIZE, DENSITY, POSITION, SCATTER, PITCH, REVERSE, FEEDBACK, SPREAD; live buffer, overlapping windowed grains, micro-looping, tempo sync and freeze.
- CYBERNETIC: FORMANT, ANDROID, SPECTRAL, HARMONY; intelligible futuristic humanoid voices, formant and spectral transformations, pitch/harmony layers. Not merely a traditional vocoder or bitcrusher.
- GHOST: GHOST and SWELL, dedicated reverse echo and reverse swell distinct from reverse grains. Ghost voices are a core feature, not optional polish.
- SPACE: DELAY and SPACE; atmospheric stereo delay, diffusion and reverb.
- Global MOTION, FREEZE, SYNC, MIX, OUTPUT. Independently neutral/bypassable engines; exact latency-aligned dry signal at MIX 0. Intensity controls 0 off, 20–50 musical, 50–75 pronounced, 75–100 creative; signed pitch/formant and physical grain parameters require appropriate neutral definitions instead of arbitrary percentages.

Proposed routing: parallel Granular/Cybernetic, independently mixed into Ghost then Space; preserve ability to combine engines and use each alone. Routing, bypass semantics, gain staging and latency alignment must be derived and verified before freezing the architecture. True anticipatory reverse swells require reported lookahead; no claim of noncausal realtime processing.

Musical modes: DREAM, CYBER, GHOST, ANDROID, PULSE, SHIMMER, SHATTER, DRIFT, ABYSS, PHANTOM. Modes are intended to alter processing behavior, not merely presets; concrete mode semantics need design/verification.
Factory target: 120 curated presets (20 Cyber Vocals, 20 Ghost Voices, 20 Granular Atmospheres, 15 each Rhythmic, Dark & Industrial, Ambient & Shimmer, Extreme & Experimental). No duplicate filler.

References for independent research: Output Portal, Arturia Efx FRAGMENTS, Soundtoys Crystallizer, Polyverse Manipulator. Do not copy proprietary DSP/assets. No embedded TTS: user can supply generated speech externally.

## GUI
Static design first, then implementation. Premium dark hardware-style panel, restrained cyan/violet accents, custom physical-looking knobs, functional central waveform/grain/spectral/reverse display reflecting actual DSP, directly accessible controls, no decorative fake data or cheap glass. Follow current 125A-Branding. No final GUI approved yet.

## Development and acceptance
Read [125A Engineering START-HERE](https://github.com/challanger2000/125A-Engineering/blob/main/START-HERE.md) and complete its mandatory gate before implementation. JUCE infrastructure with framework-independent DSP where practical; pin JUCE revision after official license and compatibility check. No speculative Actions runs. Use version branch after initial documentation-only repository initialization. Research reuse and compare algorithms before choosing; validate with independent positive/negative oracles, real speech/audio fixtures, block partitioning, automation, state/recall, latency/tail, sample rates, feedback stability, finite extreme input, realtime bounds and multi-instance isolation. Windows actual VST3 host, Validator and Test Lab evidence required before final DAW acceptance. No PASS without executed evidence.

## Reading checkpoint — incomplete
Engineering immutable revision `53b10bd4a81fe6c5d62e110b932aef7155172ad6`.
- START-HERE.md blob `91a0e14dd24ff72874bbedf58962e0e8c14c444e`: complete visible content inspected, 164 lines including trailing newline.
- README.md blob `bd94e5a896bcc905af65ca5659e533eee854b46f`: complete visible content inspected, 74 lines including trailing newline.
- ENGINEERING-GUARDRAILS.md blob `8a076e21d8226a5b1ded7061362590415fe4839f`: inspected lines 1–470 of 3187. Resume at 471; remainder NOT READ. Full retrieval for extent determination is not reading.
- Remaining relevant STANDARDS, QA, RESEARCH, ROADMAP and Branding: NOT READ. Resolve immutable revision/blob IDs and read in required order.
- Applied rules from read sections: challenge architecture and reuse before writing DSP; preserve feature contract; independent end-to-end positive/negative tests; stable state ordering and coherent recall; genuine stereo input/processed frame evidence; exact sample boundaries for scheduling/crossfades; explicit no-go for incomplete coverage.
- Additional rules from lines 176–470: preserve approved observable behavior; distinguish payload/archive identity; prove UI→parameter→DSP with native interaction; positive oracles before mutant results; actual valid bus and clock context; sample-aligned tempo/cycle scheduling, signed modulo and rate-derived smoothing; coherent snapshot and transactional rollback; check platform headers and workflow syntax before CI; invalidate stale GUI gestures on successful recall.
- JUCE version and license eligibility: NOT VERIFIED. Official references: https://juce.com/get-juce/ and https://github.com/juce-framework/JUCE/releases. Name clearance before public product distribution remains open.
- No implementation, build, audio test or release approval. This commit only records the already agreed product scope and reading progress.
