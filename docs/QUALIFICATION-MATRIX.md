# P022 — qualification coverage (not a full audit)

Baseline: exact P021 `d2be596e9275838cfc38bdd4a0f414c8380252ba`, verified Windows module SHA-256 `56478184e101e12d55fd1b37f6822aea68ec90bcaa591fb91029cb7e92e8d099`. Product artifact from run 45742434356 (no rebuild for independent P022).
 
| Requirement | Executed evidence | Status / remaining coverage |
|---|---|---|
| Actual Windows x64 VST3, stereo/mono buses, exact dry PDC | P021 21/21 CTest, native processor + VST3 host smoke, exact SHA | PASS in named cases; official validator pending |
| Granular launch, pitch, reverse, freeze, sync, stereo, feedback | P001–P004, P012 transport, core FFT/PCM fixtures, negative controls | PASS in tested core paths; each real GUI/host parameter-to-audio chain, variable automation and real-source quality not covered |
| Cybernetic formant/Android/spectral/harmony | P005/P007/P017/P021; independent reference waveform, narrow frequency and speech diagnostics | PASS limited numeric cases; pitch accuracy off coherent frequency, formant intelligibility, phase transitions, real listening still open |
| Reverse echo/swell and space | P004/P006 independent impulse oracles, full static graph regression | PASS limited core cases; exact native tail, real program transitions, full decay and extreme input open |
| All ten modes / 120 presets | Core pairwise distinctness, level calibration on speech/TTS, direct editor selected preset | PASS inventory/finite renders/one preset UI; real host full preset inventory, all controls, perceptual curation open |
| Session recall / automation / multi-instance | Portable corruption rollback, native and actual VST3 smoke, editor restore | PASS limited cases; independent concurrent state saves, dense automation, real host parameter sample offsets, restart/reentry open |
| GUI resize, genuine artwork and live display | P021 direct native editor at 65/100/150%, signal PCM exact match, actual VST3 native-peer smoke; downloaded screenshots checked | PASS tested render and reopen; physical mouse, ctrl reset, DPI change, host attach return, capture of all actions open |
| RT | P021 staged FFT improves some unpaced Linux p99 and overrun counts | FAIL high-rate deadline evidence; native paced worst-case, multi-instance, callback allocations/locks, real ASIO NOT RUN |
| Steinberg validator | P022 exact-module official validator | FAIL 46/47, advertised program 0 unnamed; P023 fix pending retest |
| Test Lab | P022 P74 independent baseline, six playback/save cases | TECHNICAL_BASELINE_PASS for P021; product-specific complete feature matrix/native cases not declared |
| User DAW, listening, release | None | NOT RUN; no GO, no final audit |

Independent QA command follows Lab P74/P98 documentation, no product-specific host modifications. It runs the exact downloaded module with independent native hosts, plus official validator on pinned SDK. Read-only generic GUI discovery is not user action coverage. The source pin is Lab `7213fe1f81f4036f74431cb5b2d7dbd09b73ee5c` and SDK `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`. Results cannot qualify later binaries automatically. P022 workflow records provenance, hashes, actual logs and exit results, including failures. Artifact expiry causes failure rather than silently selecting a different binary. Further feature-specific independent oracles and technical audit remain required.

P023 changes the product source; the P021 independent evidence is historical, not current-binary acceptance. P023 native and full-graph tests are pending. High-rate RT gate remains open; diagnostic collects duration and scheduled completion independently.

P023 Windows run 45806402015: 23/23 functional CTests pass, actual paced matrix FAIL; four fully active instances missed all 512 duration deadlines at 96/192k for both block sizes. One active instance at 48k had zero duration misses, but VM scheduling completion misses at 64. P024 kernel changes pending native build; local scalar-equivalence and sanitizer passed. No current-binary independent validator retest yet.

P024 24/24 Windows function tests PASS, 24-case paced matrix FAIL. 48k four active/256 passed duration and completion, four active/64 duration zero misses but completion 172/512. Four active at 96/192k continue 512/512 duration misses for both block sizes. P025 optimized sparse swell and harmony; native and independent same-run qualification pending. Retained serial worst-case gate; no scope reduction or physical ASIO claim.

P025 exact module d551fbd767412a1a34eb61208f3ad4ce82d4a4093e63d5c7ae98c26dac518f23: 26/26 CTests PASS; independent validator 47/47 and Lab TECHNICAL_BASELINE_PASS; serial paced RT FAIL. All single-instance duration cases passed but scheduler completion misses remain. P026 native tests NOT RUN at authoring; separate parallel four-track diagnostic does not replace serial gate.

P026 exact Windows module 1ce1c6177ed5871b566049d92612c6c9e31d251829447acf427a6fe46d2f5a3e: 28/28 functional tests PASS. Serial and separate parallel track deadline gates FAIL. Parallel active at 48k both blocks zero duration misses; 96k/64 one duration miss, 96k/256 zero; 192k/64 seven, /256 three. P027 optimizations locally compared against P026, native build and full qualification pending.

P027 exact Windows module c1b2127e8b43664ddab39608efe4d45b2326e3da362476df66fccd39c7f5e485: 30/30 CTests PASS. Four active serial 96k both blocks zero duration misses (P026 268/170); 192k remains 512/512. Parallel active all six cases zero duration misses, completion misses remain and two neutral 192k/64 duration misses; both full paced gates FAIL. P028 adds all 120 native menu selection states, actual VST3 all preset renders and opaque-state recalls across block partition and offline flag, plus individual host control changes. Pending execution, not a full audit. Transport with actual host context, concurrent state, true gesture/DPI, musical quality and ASIO remain open.

P029 measured freeze sustained >32s and max feedback >150s in actual core. Conservative tail report replaces unconditional 30s; portable policy/freeze tests PASS, native query and real VST3 tail/host export re-query NOT RUN. No audio-path change; P028 native run already active, P029 does not trigger duplicate build.
