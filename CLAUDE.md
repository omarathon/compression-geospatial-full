# CLAUDE.md

## 1) Hard Rules

- Goal: keep geospatial uint16 data **compressed in RAM** and run aggregations / NDVI / multiply directly over the fused-decode SIMD pipeline. Speedup comes from smaller working set fitting in L2/L3 + fused per-OutReg work.
- Honest decode: per-OutReg `add_anchor` must stay on the OutReg dep chain even though sum-fused doesn't strictly need it. Do **not** shortcut by adding `length * anchor` at the end. Post-correction (`hsum + length*anchor`) is sum-only and breaks generalisation to min/max/NDVI/multiply — all of which require the anchor applied per-OutReg during decode. In-flight per-OutReg correction (nobc scalar_acc or bc broadcast) is the right abstraction: it composes uniformly with any operation via the `anchor_kernel` / `kernel` two-kernel pattern.
- Encode pattern: `AllocEncoded` is a no-op; encode into shared thread-local `GetPackScratch()`; `compressed.assign(...)` exactly `actual_size` bytes. Never resize-to-worst-case + shrink_to_fit on `compressed` (glibc freelist bloats RSS).
- `bench_pipeline -b 256` ⇒ EncodeArray gets 65,536 elements. Sub-block size is **256 elements** (`kFusedSubBlockSize`), one byte of `b` per sub-block.
- Anchor for FoR = `min` of the relevant scope (per OutReg / per sub-block / per inner block). Min excludes nothing for now (approach (a) — global min is fine).
- Decoded values are NOT written by `simdunpack_u16*` (all variants are fused-sum-only). The `uint16_t *out` param is unused; helpers do `(void)_out;`.
- Aggregation order: `aggregate_sums_u16` is always the final per-OutReg step. Ablation toggles only skip the transforms in front of it.

## 2) Authority & Links

- Pipeline ablation toggles (in [external/simdcomp/scripts/gen_simdbitpacking_u16.py](external/simdcomp/scripts/gen_simdbitpacking_u16.py) and regenerated [external/simdcomp/src/simdbitpacking_u16.c](external/simdcomp/src/simdbitpacking_u16.c)): `ABLATE_ZIGZAG_{LOCAL,CARRY}`, `ABLATE_PREFIXSUM_{LOCAL,CARRY}`, `ABLATE_CARRY_ADD`, `ABLATE_BROADCAST_LANE15`.
- simdcomp Makefile honours `EXTRA_CFLAGS` (added). Use that, not `CFLAGS` overrides.
- Shared scratch buffers: [src/codecs/uint16/delta_scratch_u16.h](src/codecs/uint16/delta_scratch_u16.h) — `s_delta_scratch[256*256]` (uint16 deltas/residuals) and `GetPackScratch()` (256 KB packed-bytes scratch).
- SimdComp codec wrappers: [src/codecs/uint16/simdcomp_fused_codec_uint16.h](src/codecs/uint16/simdcomp_fused_codec_uint16.h), [src/codecs/uint16/simdcomp_for_codec_uint16.h](src/codecs/uint16/simdcomp_for_codec_uint16.h).
- FastPFor codec wrappers: [src/codecs/uint16/fastpfor_fused_codec_uint16.h](src/codecs/uint16/fastpfor_fused_codec_uint16.h); decoder internals at [external/FastPFor/headers/simdpfor_u16.h](external/FastPFor/headers/simdpfor_u16.h).
- Codec registration: [src/codecs/uint16/codec_collection_uint16.h](src/codecs/uint16/codec_collection_uint16.h) (multiple `InitPhysicalCodecsU16` / `BuildAllCodecsU16` variants — match the one the bench harness uses).
- Tests: [tests/test_fused_codecs.cpp](tests/test_fused_codecs.cpp) — round-trip via `CheckFusedSum` + compression-ratio asserts via `CheckCompressionRatio`.
- Block sampling for bench: [src/bench_utils.h](src/bench_utils.h) `SampleBlockOffsets` (wrap-around enabled; partial last wrap is evenly sampled).

## 3) Setup / Test

- Server path: `/home/omsst2/diss/compression-geospatial-full`. Local working copy: `/home/omar/compression-geospatial-git`.
- Test data: `/maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif` (Landsat B1, Zalipynis 2018).
- AVX2 only; simdcomp 256-bit (16-lane) path. Block geometry: 256-elem sub-blocks × 16 OutRegs × 16 uint16 lanes.

## 4) Workflow

```bash
# 1. Rebuild simdcomp (after generator or .c edits)
cd external/simdcomp && make clean && make && cd ../..

# 2. Rebuild bench_pipeline / bench_comp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target bench_pipeline -j32

# 3. Ablation rebuild (one variant per build, no .a swap needed)
cd external/simdcomp && make clean && \
  make EXTRA_CFLAGS="-DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15" && \
  cd ../..   # C1 level; see ablation_bench_carry.sh for the C0..C4 ladder

# 4. Bench (fused-sum, linear access)
./build/bench_pipeline <TIF> -b 256 -n 8000 -r 5 \
  --icodec <codec> --acodec <codec> \
  --ordering default --itrans none --pattern linear \
  --atrans linearSumFused --normalize

# 5. Round-trip + ratio tests
cmake --build build --target test_fused_codecs && ./build/test_fused_codecs
```

## 5) Stop Conditions

- Do NOT silently regenerate `simdbitpacking_u16.c` — confirm with user first (it overwrites manual edits if any drifted from the generator).
- Do NOT skip the per-OutReg `add_anchor` in any FoR decode. If considering a sum-only fast path, ask first.
- Do NOT add codec ifs / format changes without updating BOTH the python generator and the regenerated C, the codec wrapper header, the test file, and the codec collection registration.
- Pause and check in before starting any FastPFor-side FoR work (only SimdComp side is done).

## Project context (don't delete; read before continuing)

- **Strategic frame**: 4× decode speedup on delta+zigzag pipelines was infeasible — `broadcast_lane15` + `prefix_sum` are structurally expensive (port-5 saturation, inter-OutReg dep chain). Ablation on `simdcomp_fused_delta_carry` confirmed: C4 = 3.94× C0 (aggregate-only); broadcast_lane15 alone = +117%; prefix_sum = +42%.
- **Pivot**: FoR (Frame of Reference) — anchor = `min`, subtract per scope, bit-pack unsigned residuals. No zigzag, no prefix-sum, no carry chain. Decoder is `simdunpack_u16_corrected(in, out, b, corrections, sum)` where `corrections[k]` is the broadcast anchor for OutReg k.
- **Diminishing returns observed**: at the user's cache hierarchy, raw→77%CR gave 1.45×; 77%→17%CR only gave 1.10×. Beyond ~50% CR, compute dominates, not memory bandwidth. ⇒ design target: "OK" compression (~40-60%) + maximally simple compute, not lowest possible CR.
- **Chunked-b (per-256-elem sub-block bit width)**: critical for compression — single-`b`-per-65K-element-encode pinned `b` at the max element value. After chunking, delta_carry went from 82% → 18.8% CR, matching TurboPack128-class numbers.
- **Anchor scopes**: FoR-global (one anchor per 256-elem sub-block), FoR-local (16 anchors per sub-block, one per OutReg), FoR-hierarchical (1 global per sub-block + 16 small local deltas, deltas bit-packed via sequential scalar pack helper `seq_pack_u16` in [simdcomp_for_codec_uint16.h](src/codecs/uint16/simdcomp_for_codec_uint16.h)).
- **Codec inventory (SimdComp side, done)**:
  - `simdcomp_fused` (chunked, no transform) — uses `GetPackScratch()` + `assign` ✓
  - `simdcomp_fused_delta_local` — still uses old worst-case-alloc + shrink_to_fit
  - `simdcomp_fused_delta_carry` — uses `GetPackScratch()` + `assign` ✓
  - `simdcomp_fused_for_global` — uses `GetPackScratch()` + `assign` ✓
  - `simdcomp_fused_for_local` — still uses old pattern (asymmetric RSS — flag if benchmarking)
  - `simdcomp_fused_for_hierarchical` — still uses old pattern
- **Codec inventory (FastPFor side, NOT yet done)**: 3 FoR codecs planned (global / local / hierarchical), all using FastPFor's own bitpack primitives (not simdcomp's) for the hierarchical local-delta stream. Needs:
  - `uncompressblockPFOR_u16_corrected_for` in simdpfor_u16.h (anchor pre-fills `corrections_data`, exceptions override)
  - `decodeArrayCorrectedFor` dispatcher in compositecodec_u16.h
  - 3 wrapper classes in `fastpfor_for_codec_uint16.h` (to be created)
- **PFor anchor strategy**: use global min (approach (a) — simplest, correctly handles unsigned residuals; low-side outliers are rare for DEM/Landsat).
- **RSS trap (now mostly fixed)**: per-codec `compressed` vector worst-case allocation + libstdc++ shrink_to_fit DOES shrink the vector, but glibc keeps freed chunks in arena freelist — RSS doesn't drop. Fix: scratch + assign pattern (3 codecs done, 3 pending).
- **bench_pipeline wrap-around**: `SampleBlockOffsets` now does N-block wrap when requested > raster size; final partial wrap is evenly sampled. Lets working set match requested grid size instead of being capped by raster.
- **Data characteristics**:
  - DEM (SRTM, uint16, 30m): smooth, b ≈ 6 after delta+zigzag, range over 256 elems ≈ 0-200m → FoR `b ≈ 8`. Adjacent-pixel deltas mostly ≤ 20.
  - Landsat reflectance (uint16, 0-15000 typical): edges + cloud spikes; FastPFor delta_carry → ~17% CR (with exceptions handling the giant first delta). simdcomp without chunking → 82% CR. With chunking → 18.8%.
- **XOR-delta**: tried, doesn't apply — XOR over arithmetic data inflates b at binary carry boundaries, and breaks the weighted-madd identity that makes sum-fused fast. Stay on arithmetic delta.
- **128 vs 256-element sub-blocks**: TurboPack128 uses 128. We use 256 (matches simdcomp's `SIMDBlockSize_u16`). Gap to TurboPack128 is ~1-2 bits/elem typically. Switching to 128 requires generator changes to emit half-iteration variants. Not done; only do if benchmarks show the gap matters.
- **Block layout reminder**: 3 levels — OutReg (16 elems), inner SIMD block (256 elems = `kFusedSubBlockSize`), logical bench block (65,536 elems = 256×256).

## Lossy compressibility study (LERC + median)

- **Goal**: measure compressibility gain from lossy preprocessing (LERC, median filter) at controlled error budgets, expressed as NRMSE. Error metric: NRMSE_std = RMSE / std(original) (std-normalized, conservative; preferred over range-normalized which is outlier-sensitive and looks more optimistic).
- **Error-budget convention**: `maxZError_formula = std * threshold`. Empirically this yields NRMSE ≈ threshold/2 (because LERC RMSE ≈ maxZError × 0.5 for smooth data — most pixels well below the per-pixel bound). So "5% formula" ≈ 2.5% actual NRMSE. Report `nrmse_at_formula_maxz` alongside.
- **Two interpretations**: `formula` (maxZError = std·t, pure arithmetic) vs `search` (binary-search maxZError achieving exact NRMSE target, ±1 DN tol, ~12 LERC roundtrips/target). Decided to lead with formula; only justify search/NRMSE framing if CR gains warrant it.
- **Scripts** (repo root): [build_lossy_index.py](build_lossy_index.py) builds per-(TIF,band) index (std, range, maxZError per threshold {5,10,15,30%}, median NRMSE k={3,5,7}); flags `--formula/--search/--median`. [run_comp_bench_lossy_index.py](run_comp_bench_lossy_index.py) runs `bench_comp` on lossless + index-derived LERC + median variants. [parse_bench_comp_results.py](parse_bench_comp_results.py) emits 4 CSVs (one per ordering×nodata), matrices per variant, collections aggregated by mean-of-mean CR. [patch_index_formula_nrmse.py](patch_index_formula_nrmse.py) backfills `nrmse_at_formula_maxz` into an existing index. [query_index_std.py](query_index_std.py) dumps per-collection std.
- **Key conventions**: COLLECTIONS list (4-tuple: name, blocksize, globs, widths) is shared across the lossy scripts; keep in sync. Multi-band TIFs are skipped (data is split into per-band TIFs). LERC roundtrip reuses `scripts/lossy_transform_tiff.py` helpers (preserves metadata/nodata/dtype). Temp dir `/scratch/omsst2/diss/temp` (local ZFS, fast but ~200GB free — watch it). Stats sampled over ≤2000 evenly-spaced 256×256 windows (full read if fewer blocks exist). Workers can use up to 64 cores (`list(range(0,64))`); no CCX pinning needed since bench_comp isn't latency-sensitive.
- **bench_comp wrap-around**: `SampleBlockOffsets` takes `allow_wrap` (default true). bench_comp passes `false` (each block once — wrapping inflates apparent compressibility); bench_pipeline keeps wrap.
- **Cascaded-codec size constraint**: logical codecs feeding a fused physical codec MUST output a multiple of `kFusedSubBlockSize`=256. `FORCodecU16(w)` outputs `length + length/w` (one inline min per window) — safe only when **w divides 256** ({32,64,128,256} OK; wfull→65537, w512→65664 both crash). RLE (2·num_runs) is uncascadable. These are commented out in [codec_collection_uint16.h](src/codecs/uint16/codec_collection_uint16.h).
- **LERC dtype limitation**: GDAL LERC fails on some SampleFormat/bitspersample combos (uint8 WorldCover, certain Sentinel2 `others/sentinel2/*` files) → "Unsupported combination" RuntimeError. Scripts catch it and store `nrmse=None`; these are expected skips, not bugs.
- **Finding — fused codecs barely improve with lossy preprocessing**: simdcomp/FastPFor fused codecs pick `b` from the sub-block max (uint16 normalize has GCD hardcoded to 1; LERC's per-block local quantization doesn't align to a global grid), so lossy preprocessing rarely lowers `b`. Delta helps (shrinks max→residual). Only adaptive codecs (TurboPFor128/TurboPack128) show real CR gains from reduced post-LERC range. For the in-RAM pipeline-speedup thesis the relevant metric is bench_pipeline decode speed, not bench_comp CR.

## Session 2026-06: TurboPFor fused 256v16 (DONE) + FoR sep-metadata + 128-vs-256 perf

**FoR separate-metadata (early):** `FORCodecU16`/`FORHierarchicalCodecU16` ([custom_unvec_logic_codecs_u16.h](src/codecs/uint16/custom_unvec_logic_codecs_u16.h)) gained a `separate_metadata` flag: residuals go to the physical codec, anchors held in `metadata_` (not fed downstream). New virtual `ExtraEncodedBytes()` ([generic_codecs.h](src/codecs/generic/generic_codecs.h)) counts side-channel bytes; `CompositeStatefulIntegerCodec::EncodedNumValues()` ([composite_codec.h](src/codecs/generic/composite_codec.h)) adds them so sep=true CR is honest. **Gotcha fixed:** composite `clear()` runs between encode/decode → must NOT clear `metadata_` (decoder needs it). Registered as loops over windows×gw×sep in [codec_collection_uint16.h](src/codecs/uint16/codec_collection_uint16.h). New script [parse_bench_comp_results_for_agg.py](parse_bench_comp_results_for_agg.py): aggregates over FoR windows (min CR), strips window from codec name, short names (FoR/HFoR/PFor/Pack), lossless gets a `0%` NRMSE col for column alignment.

**256-width fused-sum 16-bit PFor (`TurboPFor_fused_256v16_sum`) — built, correct, FAST:**
- Files: [vp4d256v16_fused.c](external/TurboPFor/lib/vp4d256v16_fused.c) (encoder `p4nenc256v16` + fused decoder `p4ndec256v16_sum`), wrapper [turbopfor_fused_256_codec_uint16.h](src/codecs/uint16/turbopfor_fused_256_codec_uint16.h). Built into the `TurboPForFused` CMake lib.
- Low bits use **simdcomp's** AVX2 16-lane `simdpack_u16`/`simdunpack_u16` (NOT TurboPFor — there is no stock `p4n*256v16`). Exception coding mirrors TurboPFor for **CR parity**: bit-packed excess (`bitpack16`), bitmap-OR-vbyte positions (whichever smaller), constant-block opcode. Format: `ctrl=(mode<<5)|b`, modes PLAIN/BITMAP/VBYTE/CONST; per-256-block.
- **Exception merge is in-register** (issues 1/2/3 fixed): `simdunpack_u16_pfor` left-packs `excess<<b` into exception lanes via two 8-lane `pshufb` halves (`kShuffle16` table) per OutReg — same idea as the 128's `_shuffle_16`, since AVX2 has no 16-lane shuffle table. Corrected OutReg materialised in-register before the aggregate (**no sum hack** — aggregate is swappable). Element `i` ↔ OutReg `i/16`, lane `i%16`.
- CR: TurboPFor256 ≈ TurboPFor128 (within ~1% block-granularity gap); both ≪ simdcomp (which has no exceptions).

**simdcomp generator ([gen_simdbitpacking_u16.py](external/simdcomp/scripts/gen_simdbitpacking_u16.py)) — 3 new modes, all emit SEPARATE files; canonical `simdbitpacking_u16.c` NEVER regenerated (stop-condition honoured):**
- `--suffix _w128 --width 128`: minimal self-contained SSE variant → `simdbitpacking_u16_w128.c` (powers the `simdcomp_fused_128` codec, [simdcomp_fused_codec_uint16_w128.h](src/codecs/uint16/simdcomp_fused_codec_uint16_w128.h), lib `SimdCompU16W128`, kept for width comparison).
- `--pfor`: self-contained `simdunpack_u16_pfor` → `simdbitpacking_u16_pfor.c` (now superseded by the inline header; lib removed).
- `--inline-decode`: STATIC `simdunpack_u16_il` + `simdunpack_u16_pfor_il` → `simdbitpacking_u16_decode_inl.h`, **#included into the decoder TU** so kernels inline.

**PERF LEARNINGS (the important part):**
- **256 width is genuinely faster than 128** within the same codegen — `simdcomp_fused` (256) beats `simdcomp_fused_128` on every TIF. The width is not the problem.
- Two bugs made the 256 *look* slow; both fixed in [vp4d256v16_fused.c](external/TurboPFor/lib/vp4d256v16_fused.c):
  1. **Non-inlined cross-TU call** → the `__m256i *sum` accumulator spilled to memory every sub-block (256×/decode). Fix: `#include "simdbitpacking_u16_decode_inl.h"` (static `_il` kernels) so the unpack inlines, the `switch(b)` folds, and `sum` stays in a YMM register. This fixed a 4× high-`b` regression (b=16 went 4.33×→0.72× vs 128).
  2. **`b=0` memset**: `simdunpack_u16(b=0)`→`SIMD_nullunpacker16` memsets 512 B of scratch per all-zero block (pure waste for a fused sum; inlined it lands in the hot loop and blows up mostly-zero rasters like srtm). Fix: **skip `b=0` PLAIN** (all-zero ⇒ sum+=0), and do CONST's sum inline (16 widen-accumulates, no memset). The 128 already avoided this (sums zeros inline).
- After both fixes: **256 beats 128 on all 3 local TIFs and all bit-widths** (~1.2–2×).
- simdcomp's and TurboPFor's AVX2 unpack are **structurally identical** (same vertical AND/shift/OR, just `epi16` vs `epi32`) — no hand-tuning needed at 256; the generated code is already as good. The lever was inlining, not codegen.
- **PFor-vs-bitpack tradeoff** (in-RAM thesis): PFor (TurboPFor) wins CR when data is low-`b` + sparse outliers (smooth DEM post-normalize), and wins *decode too* only when exceptions are **sparse** (srtm). Dense-exception data (slope-srtm, WorldCover categorical) → PFor gives big CR wins but decodes slower than plain bitpack (simdcomp); use simdcomp-256 there. Uniformly high-entropy (Landsat) → PFor barely helps CR.

**Local test TIFs** (`/home/omar/diss/geotiffs/`): `srtm_45_15.tif` (Int16 DEM, ~all zeros post-normalize, CR~0.06, sparse exc — PFor wins both), `slope-srtm_35_11.tif` (Int16, CR~0.22, dense exc), `WorldCover_nyc_ESA_WorldCover_10m_2021_v200_N39W075.tif` (Int16 categorical, CR PFor 0.08 vs bitpack 0.14, dense exc), `landsat8_path190_row031_stack.tif` (UInt16, high-entropy, PFor +3% only).

**Methodology notes for the next agent:**
- **No working profiler on this WSL2 box**: `perf` is dead (wrong kernel), callgrind SIGILLs on `-march=native` and mis-attributes. Use **rdtsc instrumentation** (compile decoder with `-DFUSED_PROFILE`; globals `g_fused_kernel_cyc`/`g_fused_total_cyc` for 128, `g_fused256_*` for 256; `rest = total − kernel`, rest slightly inflated by rdtsc) or a **wall-clock microbench**. Microbenches MUST defeat loop-hoisting (`asm volatile("":::"memory")` or perturb input) — an opaque loop-invariant call gets hoisted/CSE'd.
- **Microbench (L1-resident, decode one block repeatedly) ≠ bench_pipeline (streams many blocks, memory-bound)** — they can disagree; trust bench_pipeline `medtimedec` for real-world, microbench for isolating the kernel. Run perf on a quiet machine; use `--rs 3` (skip 3 warm-up reps).
- Verified throughout: 196 `test_fused_codecs` pass; [verify_fusion_latest.sh](verify_fusion_latest.sh) (now loops all 4 TIFs) green for 128 & 256; a 2M-trial per-lane placement test confirmed the pfor shuffle merge is correct (not just the sum).
- **Final fused decode (ns/65536-block, bench_pipeline, quiet, --rs 3):** srtm: sc128 4565 / sc256 4449 / tp128 3777 / **tp256 2999**; slope: 7095 / **4383** / 20311 / 16057; WorldCover: 5380 / **3502** / 11664 / 9598. (sc=simdcomp, tp=TurboPFor.)

## Session 2026-06: simdcomp 128-width FoR fused — DONE; next = 256-width + TurboPFor FoR

**DESIGN GOAL (the whole point): minimise decode LATENCY at a *given* CR.** Not best CR, not fewest bytes — fastest fused-sum/aggregation decode for whatever CR a window/codec gives. The lever that delivered it: **fully fuse the anchor correction into the unpack kernel — zero memory round-trips.** Anchors are broadcast inline from the (cached) anchor stream straight into the OutReg add; never materialised as a `__m128i` corrections array (that store→reload was *the* dominant cost). Every technique below serves this: keep the correction in-register, keep the aggregate off port 5, never touch memory you don't have to. Carry the same discipline to 256 + TurboPFor.

**What exists (128):** [simdcomp_for_codec_uint16_w128.h](src/codecs/uint16/simdcomp_for_codec_uint16_w128.h) — `SimdCompFusedForCodecU16_128(window∈{4,8,16,32,64,128,256}, separate)` (regular) + `SimdCompFusedForHierarchicalCodecU16_128(outer∈{128,256}, inner)`. Base = [simdcomp_fused_codec_uint16_w128.h](src/codecs/uint16/simdcomp_fused_codec_uint16_w128.h). Kernels generated by `gen_simdbitpacking_u16.py --width 128 --suffix _w128 [--corrected]` → `simdbitpacking_u16_w128{,_corrected}.c`, lib `SimdCompU16W128`. Parity harness [bench/for128_parity.cpp](bench/for128_parity.cpp); timing `bench_pipeline` + `bench_pipeline_noagg` (`-DFOR_DECODE_NOAGG`, produces OutReg w/ XOR sink, sums wrong — decode-timing only). 198 `test_fused_codecs` pass; CR byte-exact vs unvec composite (FORCodecU16→SimdCompFusedCodecU16_128) for separate=true + hier.

**Run / verify (128):**
```bash
# after editing the generator, regen BOTH w128 files (NEVER the canonical 256 .c):
python3 external/simdcomp/scripts/gen_simdbitpacking_u16.py --width 128 --suffix _w128
python3 external/simdcomp/scripts/gen_simdbitpacking_u16.py --width 128 --suffix _w128 --corrected
cmake --build build --target test_fused_codecs bench_pipeline bench_pipeline_noagg for128_parity -j32
# CORRECTNESS (round-trip fused sum, all windows×sep×hier, madd+unpack paths): must be 198/198
./build/test_fused_codecs
# CR-PARITY vs unvec (sep=true/hier should be ~1.000): prints vec/ref per codec×TIF
./build/for128_parity
# DECODE TIMING (medtimedec ns/65536-block). Codec names: simdcomp_fused_128 (baseline),
#   simdcomp_fused_for_128_w{4,8,16,32,64,128,256}[_sep], simdcomp_fused_for_hier_128_g{128,256}_l{..}
./build/bench_pipeline <TIF> -b 256 -n 1000 -r 6 --rs 2 --icodec <codec> --acodec <codec> \
  --ordering default --itrans none --pattern linear --atrans linearSumFused --normalize
# produce-OutReg-only (no widening sum, XOR sink): same flags, ./build/bench_pipeline_noagg
```
Local TIFs in `/home/omar/diss/geotiffs/`: srtm_45_15 (b≈0), slope-srtm_35_11 (mid-b), WorldCover_nyc_…N39W075 (FoR cuts CR ~2×). Quiet machine + `--rs 2`; runs are noisy ±10%, re-run outliers. No working profiler on WSL2 — use bench_pipeline `medtimedec` (streaming, real) + L1 microbench (`g++ -O3 -march=native …` linking the regenerated `.o`s; `asm volatile("":::"memory")` to defeat hoisting) to isolate the kernel.

**Geometry:** 128-block = 16 OutRegs × 8 lanes; elem i ↔ OutReg i/8, lane i%8. Window only sets anchor granularity; residuals always 128-block chunked-b. Decode dispatch = `decode_block()` by mode: **uniform** (w≥128: 1 broadcast/block, `corrected_uniform`), **scalar** (8≤w<128: rolling single `_bc` per window-group, kernels `cscalar0..3`, shg=log2(w/8)), **half** (w=4: inline `[a×4,b×4]` via 2 set1+blend, `chalf`). Anchors read straight from the (raw/`simdpack`'d) stream — NO `__m128i` corrections array.

**KEY TECHNIQUES (carry to 256 + TurboPFor):**
1. **No corrections array** — the 16-vector store→reload is the dominant overhead; broadcast per-OutReg anchor inline instead.
2. **may_alias trap** — GCC `__m256i/__m128i` are `may_alias`, so `*sum` writes force a reload of `set1(a_block[..])` between OutRegs, defeating broadcast reuse. Fix: hold the broadcast in a **named local `_bc`**, re-broadcast only at window-group boundaries (1 register, no spill).
3. **madd aggregate** (`aggregate_sums_u16_madd` = `madd_epi16(OutReg, set1(1))`, port 0/1) vs default unpack-widen (2× `unpacklo/hi`, **port 5 = #1 cost**). ~1.5× kernel (L1), ~1.15× streaming. SIGNED → gated: encoder ORs all values, sets 1-byte `madd_safe` flag (max<2^15) after `num_blk` in header; decoder dispatches, falls back to unpack. Both kernel sets generated (`agg='unpack'|'madd'|'noagg'`).
4. **b=0 skips the `SIMD_nullunpacker16` memset** (fused sum never writes `out`) — big on srtm (mostly b=0). FoR b=0 still aggregates the anchors (honest decode).

**PERF (128, server sherwood n500 r11 rs3, sequential):** FoR-128 is **always slower than the plain bitpack baseline in absolute decode time** — the earlier "parity–1.17×" was WSL2 noise. Server results on WorldCover_nyc (CR cuts ~2×): baseline 3647 ns; best FoR (w=128/256) ~4553 ns = ~25% slower. On Landsat8_B1: baseline 9156 ns; best FoR ~8098 ns (w=128) — slight improvement there because high-b data is more memory-bandwidth-bound. **FoR value is CR, not decode speedup.** Fine windows (w=4..16) are 1.5–2× slower than baseline due to per-OutReg correction overhead. Decode = extract(~2400) + broadcasts(0–1800 by window) + per-lane add(~800) + aggregate(madd~1000/unpack~2800). Corrections are never free — they add ~800–1800 ns/block that the plain codec avoids entirely.

**Shuffle correction (w=4, 128-bit):** `corrected_half_shuf` uses `vmovq + vpshufb` instead of `2×vpbroadcastw + vpblendw` (3 port-5 ops → 1). Server result: **+7% on WorldCover_nyc** (7841→7296 ns non-sep, 7247→6755 sep). Bigger on high-b data (Landsat8_B8: +77% = 23233→13096 ns at w=4). For w≥8 (cscalar modes), shuffle is neutral-to-slightly-slower (tested exhaustively; vpbroadcastw m16 is already 1 micro-fused port-5 op — vmovq+vpshufb adds a load uop). **256-bit w=8 is the next predicted win**: at 256-bit, the "half" case (kModeHalf) occurs at w=8 (OutReg straddles 2 anchors across 16 lanes), so the same 3→1 port-5 saving applies. Generator mode `corrected_half_shuf` is 128-only; 256-bit half shuf not yet implemented. Codec `shuf_` flag now covers all windows but only benefits w=4 (128) and will benefit w=8 (256).

**Server workflow (sherwood):**
- SSH: `ssh omsst2@sherwood.cl.cam.ac.uk` (key loaded in `~/.zshrc`, no password needed)
- Repo: `~/diss/compression-geospatial-full` (same structure as local); TIF paths in `run_benchmarks_sweep_realdata_parallel.py` (DISS=`/maps/omsst2/diss`)
- Key TIFs: srtm=`/maps/omsst2/diss/papers/rasterlite/SRTM_Italy/.../SRTM_Italy_final.tif`; Landsat8_B1=`.../zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif`; Landsat8_B8=`.../rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF`; WorldCover_nyc=`/maps/omsst2/diss/others/worldcover_int16/WorldCover_nyc_ESA_WorldCover_10m_2021_v200_N39W075.tif`
- Push/pull: work on local branch `fuse_for_experiments` (off `bench_fuse_256width`); `git push origin fuse_for_experiments`; on server `git pull origin fuse_for_experiments`; then `cd external/simdcomp && make clean && make -j16 && cd ../..`; then `cmake --build build --target bench_pipeline -j32`
- TurboPFor submodule: `git submodule update --init external/TurboPFor` (after main repo checkout; FastPFor_fused entry in .gitmodules is broken/deleted — ignore its error, only TurboPFor matters)
- Benchmark pattern (use `--rs 3`, `r=11`, `n=500`; **run codecs sequentially** — parallel jobs compete and produce noisy results):
```bash
BP=~/diss/compression-geospatial-full/build/bench_pipeline
$BP <TIF> -b 256 -n 500 -r 11 --rs 3 --icodec <codec> --acodec <codec> \
  --ordering default --itrans none --pattern linear --atrans linearSumFused --normalize
```
- Parallel is fine for independent TIF×codec combos if the server is otherwise idle and you want throughput; avoid parallel within the same codec comparison (cache/scheduler interference).

**Next — TurboPFor FoR fused (128 & 256):** same FoR-correction idea + exceptions. `uncompressblockPFOR_u16_corrected_for` ([simdpfor_u16.h](external/FastPFor/headers/simdpfor_u16.h)) already does global/uniform anchor; extend to per-window (cscalar) + madd. 256 fused lives in [vp4d256v16_fused.c](external/TurboPFor/lib/vp4d256v16_fused.c), wrappers `turbopfor_fused_{,256_}codec_uint16.h`. Apply the same nobc_sum_t and -mno-avx512f discipline described in the session below.

## Session 2026-06: 256-width simdcomp FoR nobc — DONE

**256-width FoR fused is now complete.** Generated files: [simdbitpacking_u16_w256_nobc.c](external/simdcomp/src/simdbitpacking_u16_w256_nobc.c) and [simdbitpacking_u16_w128_nobc.c](external/simdcomp/src/simdbitpacking_u16_w128_nobc.c). Built into `SimdCompU16W256` / `SimdCompU16W128` libs alongside the existing `_w256.c` / `_w256_corrected.c` files. Geometry: 256-block = 16 OutRegs × 16 lanes; windows shift vs 128: w=16 → cscalar0 (1 anchor/OutReg), w=8 → half, w=4 → quarter (new worst case, 4 anchors/OutReg).

**nobc (no-broadcast-correction) design:** FoR sum-fused exploits `sum(v + a) = sum(v) + N*a` to defer anchor contribution. Residual SIMD sum accumulates in `*sum`; anchor contributions accumulate in scalar `*scalar_acc += lanes * a[outReg]`. Final: `hsum(*sum) + scalar_acc`. Avoids per-OutReg `vpadd` with anchor broadcast — the anchor correction overhead (broadcast + vadd) was the dominant cost for fine-window FoR.

**CRITICAL — nobc_sum_t typedef to break may_alias (must carry to TurboPFor FoR):**
`__m256i` / `__m128i` are declared `__attribute__((__may_alias__))` by GCC headers. This means any write through a `__m256i*` forces GCC to assume it may alias ANY other pointer — including the anchor array `a_block`. Without the fix, GCC inserts a reload of `a_block[k]` after every `vmovdqa` store to `*sum`, defeating anchor broadcast reuse. This causes a **2.8× perf hit** on fine-window (w=4) modes.

Fix: use a custom vector typedef WITHOUT may_alias for the sum parameter:
```c
typedef int nobc_sum_t __attribute__((vector_size(32)));  // 256-bit
// (or vector_size(16) for 128-bit)
```
The aggregate helper signature becomes `aggregate_sums_u16_nobc(OutReg, a, scalar_acc, nobc_sum_t* sum)`. Call sites cast: `(nobc_sum_t*)sum`. Writes through `nobc_sum_t*` do NOT carry may_alias, so GCC keeps `set1(a[k])` live in a YMM register across OutRegs.

**Alternative tried and REVERTED — `_sum_local` local accumulator:** Instead of a helper with `nobc_sum_t*`, hold sum as `__m256i _sum_local = *sum` at function entry, accumulate all 16 OutRegs in-register, write back once at exit. This eliminates the store-load chain entirely. But it increases register pressure by 1 YMM, causing GCC to spill 3 YMM registers to stack for large-b (b=15) quarter-mode functions (`vmovdqa %ymm4,-0x20(%rsp)` etc). Net result: 1-5% **worse** than nobc_sum_t across all windows. Reverted in commit `3bcadb8`; do NOT re-attempt.

**CRITICAL — `-mno-avx512f` on SimdCompU16W128 and SimdCompU16W256 CMake targets:**
GCC on AVX-512-capable machines (e.g. this WSL2 host) emits EVEX-encoded YMM instructions (`vmovdqu64`, `ymm16`/`ymm17` etc.) even when targeting AVX2 code, because `-march=native` enables AVX-512. The server (sherwood) is AVX2-only and will SIGILL on EVEX. Fix is already in CMakeLists.txt:
```cmake
target_compile_options(SimdCompU16W128 PRIVATE -O3 -march=native -mno-avx512f)
target_compile_options(SimdCompU16W256 PRIVATE -O3 -march=native -mno-avx512f)
```
Apply the same flag to any new lib built from generated nobc/corrected kernels (e.g. future TurboPFor FoR libs). Verify with `objdump -d | grep 'ymm1[6-9]\|ymm[23][0-9]\|{evex}'` — should be empty.

**Performance (server sherwood, n=500 r=11 rs=3, sequential, nobc_madd variants):**

| codec (w) | srtm | ETOPO1 | WorldCover | WorldCover baseline |
|---|---|---|---|---|
| w4 | 6529 | 8066 | 5475 | 2357 |
| w8 | 5170 | 7163 | 3770 | |
| w16 | 4525 | 6686 | 3167 | |
| w32 | 4317 | 6462 | 2484 | |
| w64 | 4147 | 6436 | 2326 | |
| w128 | 4044 | 6377 | 2349 | |
| w256 | 3903 | 6323 | 2096 | |
| baseline | 5614 | 7996 | 2357 | |

w4 is ~12-29% slower than baseline (nobc removes the accidental ILP benefit where anchor L1 reads filled the ~5-cycle store-load-forwarding latency). w≥16 beats baseline. WorldCover w≥32 wins significantly because FoR cuts CR ~2× (categorical data has tight local ranges).

**Kernel fusion abstraction — design notes for future ops (NDVI, min, max, multiply):**

The aggregate kernel is the natural abstraction point for plugging in different operations. The two-kernel pattern eliminates redundant anchor work at w≥32:

```
anchor_state = anchor_kernel(n, r, ...)   // once per window group
kernel(OutReg_A, OutReg_B, anchor_state)  // once per OutReg
```

`anchor_kernel` is called at window-group granularity (multiple OutRegs share the same anchor group). `kernel` is called per OutReg. For operations with algebraic simplifications this avoids redundant broadcasts:

- **sum (nobc)**: `anchor_kernel(a) → {a_scalar}`; `kernel(OutReg, a)` → `*sum += widen(OutReg); *scalar_acc += lanes*a`
- **min/max (kModeUniform only)**: `anchor_kernel(a) → {a_scalar}`; `kernel(OutReg, a)` → `min_running = min(min_running, hmin(OutReg) + a)`. For kModeScalar (varying anchors per OutReg) hmin per OutReg is expensive; corrected path is better.
- **NDVI** `(NIR-RED)/(NIR+RED)`: `anchor_kernel(n, r) → {broadcast(n), broadcast(r)}`; `kernel(N, R, bc_n, bc_r)` computes `bc_diff = bc_n - bc_r`, `bc_sum = bc_n + bc_r` internally (2 cheap SIMD ops, ~free even at w=256 with 16× reuse). Saves 2 broadcasts and 2 adds vs correcting both bands before passing to kernel. Kernel signature takes pre-broadcast scalar anchors `{bc_n, bc_r}` — NOT `{bc_diff, bc_sum}` — so the outer loop stays generic (just broadcasts anchors) and NDVI algebra lives entirely inside the kernel.
- **multiply (no algebraic simplification)**: `anchor_kernel(a, b) → {broadcast(a), broadcast(b)}`; `kernel(A, B, bc_a, bc_b)` corrects inline: `(A + bc_a) * (B + bc_b)`. Still benefits from w≥32 broadcast reuse.

The outer dispatch loop (cscalar/uniform/half/quarter) is identical regardless of operation — only the two kernel implementations differ. Half/quarter modes (multiple anchors per OutReg) need a different kernel signature for multi-band ops; cleanest to treat as a separate code path or restrict multi-band ops to w≥16 (256-bit) / w≥8 (128-bit).

## Session 2026-06-16: two-band fused NDVI (`bench_pipeline_2band`) + `count(NDVI > x)` — kernel DONE, server bench PENDING

**The vehicle:** [bench/bench_pipeline_2band.cpp](bench/bench_pipeline_2band.cpp) drives a lock-step two-band fused decode (NIR=B5, RED=B4) over compressed uint16 grids, computing per-pixel NDVI ops in-register. Kernels live in [external/simdcomp/src/ndvi2band_w256.cpp](external/simdcomp/src/ndvi2band_w256.cpp) (lib `Ndvi2Band`, built `-O3 -march=native -mno-avx512f -std=c++20` — server is AVX2-only, SIGILLs on EVEX). Three entry paths share the same templated `acc_op<OP>`: `ndvi2_raw` (decompressed arrays), `ndvi2_indep` (per-band independent unpack), and the fused compressed pipeline. `X` = number of concurrent sum threads (spin-pool, spread-pinned). Codecs compared: `simdcomp_fused` vs `custom_direct_access`.

**Ops enum** (`ndvi2band_w256.cpp:41`): `OP_SUM`/`OP_ADD`/… `OP_NDVI_DIV=5` (`(a-b)/(a+b)`, plain vdivps), `OP_NDVI_RCP=6` (rcp+1 Newton step), `OP_NDVI_RCPRAW=7` (vrcpps, no NR — bench the raw rcp cost), `OP_NDVI_COUNT=8` (NEW).

**X=1 bottleneck for all float NDVI ops = the int→float widen, NOT the divide.** `cvtepu16_epi32`+`cvtepi32_ps` per band dominates (~7,680ns of ~18,539ns for ndvi_div); vdivps / rcp+NR / raw-rcp all time identically because the divide hides behind the widen. At X≥8 the box is DRAM-bound (EPYC 7702 ~105GB/s), float work fully hides, and speedup tracks 1/CR (noop X=32 hit 2.12× > 1/CR=1.78× because both byte volume and load-port pressure drop).

**Measured `ndvi_div` results (local, `(a−b)/(a+b)` + float widen; comp=`simdcomp_fused` vs uncomp baseline, same kernel):**

| X | comp (ns) | uncomp (ns) | speedup |
|---|---|---|---|
| 1 | 17,390 | 18,539 | 1.07× |
| 2 | 17,553 | 19,100 | 1.09× |
| 4 | 18,664 | 28,531 | 1.53× |
| 8 | 26,202 | 56,124 | **2.14×** |
| 16 | 30,619 | 60,017 | 1.96× |
| 32 | 33,472 | 67,597 | 2.02× |

X=1–2 ≈ flat (1.07–1.09×) — single-thread is compute-bound on the float widen, so compression's bandwidth saving barely helps. The climb to ~2× by X=8 is the DRAM-saturation regime where the smaller compressed working set wins. `count` should lift the X=1 end specifically (no widen → less compute to hide behind). These are the float-op baselines `count` must beat.

**`count(NDVI > x)` — the strongest win (eliminates BOTH divide and float widen → pure int, ~OP_ADD cost):**
- **Formula:** `NDVI > x  ⟺  (a−b)/(a+b) > x  ⟺  (a−b) > x(a+b)` [valid, `a+b ≥ 0`] `⟺  a·(1−x) − b·(1+x) > 0`. Because `x` is a FIXED threshold, `(1−x)` and `(1+x)` fold to fixed-point constants `K1=lrintf((1−x)·4096)`, `K2=lrintf((1+x)·4096)` (SCALE=4096) — the legitimate constant-coefficient case (unlike per-pixel NDVI which genuinely needs a per-pixel divide).
- **SIMD:** interleave band pairs `lo=unpacklo_epi16(va,vb)`, `hi=unpackhi_epi16(va,vb)`; one `vpmaddwd` against coefficient register `g_count_coef = set1_epi32( (uint16)K1 | ((uint16)(−K2))<<16 )` yields `a·K1 − b·K2` per pixel directly in int32 (no widen, no float). `cmpgt_epi32(d, 0)` → −1/0 per lane, accumulate into `accx`; final `−hsum_epi32(x)` negates the −count. Range: `a·K1 ≤ 65535·4096 ≈ 2.68e8`, the madd's two-product sum ≈ `±5.4e8 < 2^31` — safe.
- **Threshold setter:** `extern "C" void ndvi2_set_count_threshold(float x)` (`ndvi2band_w256.cpp:224`) packs K1/−K2 into `thread_local __m256i g_count_coef`. MUST be called inside each spin-pool worker before its rep loop (thread_local) — done at [bench_pipeline_2band.cpp:805](bench/bench_pipeline_2band.cpp#L805) `if (op==8) ndvi2_set_count_threshold(gCountThreshold);`. CLI: `--op count --threshold <x>` (default 0.3).
- **CORRECTNESS VERIFIED:** standalone `/tmp/test_count.cpp` (link against `build/CMakeFiles/Ndvi2Band.dir/external/simdcomp/src/ndvi2band_w256.cpp.o`) passes all 3 cases: a=b=100 NDVI=0 → x=0.1 count 0, x=−0.1 count 16; a=200,b=0 NDVI=1 → x=0.5 count 16. The earlier "reversed" counts in the full bench were a **data artifact** (same TIF for both bands → after per-band min-subtraction normalize + 65535 nodata, not a clean a=b), **NOT a kernel/sign bug**. Sign logic is correct.

**PENDING (next session — explicit user directive):** benchmark `count` ON THE SERVER (sherwood), not locally, at **X=1,2,4,8,16, N=1000**, `simdcomp_fused` vs `custom_direct_access`, sequentially (no parallel — RAM saturates at large X). Server TIFs: B5(NIR)=`/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif`, B4(RED)=`/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif` (both min=0 genuine, max=65534, NoData=65535). Workflow: commit+push `fuse_for_experiments`; on server `git pull`; `cd external/simdcomp && make clean && make -j16 && cd ../..`; `cmake --build build --target bench_pipeline_2band -j32`; run sequentially. Hypothesis: count should beat all float NDVI ops at X=1 (no widen) and approach 1/CR earlier (less compute to hide). PFor+FoR codecs (CR ~0.30–0.33 at 10% NRMSE vs ~0.53 bitpack) would further lift X=1 via bandwidth — not yet wired into the 2band path.

**Float-NDVI dead ends (don't re-explore):** integer-scaled NDVI `x·(num·x/(den·x))` simplifies to num/den (no help, AVX2 has no vector int divide); `_mm256_mulhrs_epi16` fixed reciprocal only works for ONE constant divisor (not per-pixel); the SO "scalar divide is faster" claim rests on a `_mm256_castsi256_ps` bug (bit-reinterpret, not convert → denormals into vdivps).

## Session 2026-06: Two-band FoR+PFor fused decoder (`pfor_for_2band`) — DONE, server bench PENDING

**What was built:** [external/TurboPFor/lib/ndvi2band_pfor_w256.cpp](external/TurboPFor/lib/ndvi2band_pfor_w256.cpp) — encoder `p4nenc256v16_for2band` + fused decoder `ndvi2_pfor_for_indep`. Lib: `Ndvi2BandPFor` (CMakeLists). Wired into `bench_pipeline_2band` as codec `"pfor_for_2band"`.

**Design:**
- FoR: anchor = per-256-block min per band; residuals = value − anchor; anchors stored as separate `std::vector<uint16_t>` arrays (N/256 per band). MUST be separate from the packed payload — not interleaved.
- Shared-b: `shared_b = max(best_b(resA), best_b(resB))` per sub-block. Both bands encoded at the same b → 17-entry decoder dispatch instead of 289. M_CONST suppressed to maintain invariant.
- Exception format: M_PLAIN (xn=0, early exit), M_BITMAP (xn>255 or bitmap < vbyte), M_VBYTE (small xn) — same as `p4nenc256v16`. `bitpack16`/`bitunpack16`/`vbenc16`/`vbdec16` from TurboPFor `ic.h`; `simdpack_u16`/`simdunpack_u16` from simdcomp for packed low-bit planes.
- `pshufb` exception merge: same `kShuffle16[256][16]` table + `PFOR_MERGE_J` macro as single-band PFor.

**CRITICAL BUG FIXED (b=0 with exceptions):** For sparse sub-blocks with a few large values, the cost model picks b=0 (no low bits stored) with xn exceptions. The decoder's `b==0` shortcut (`if (b==0) { anchor contrib + continue; }`) fired unconditionally, silently DROPPING exception values (the exceptions ARE the full decoded values when b=0). Fix: compute `has_exc = (modeA != M_PLAIN || modeB != M_PLAIN)` BEFORE the b==0 check; gate shortcut on `b==0 && !has_exc`; fall through to `sub_pfor2<OP,0>` when has_exc. The `ex<0,J>` template returns 0 regardless; exceptions carry the full corrected values via pshufb merge.

**Correctness:** All 14 unit tests in `/tmp/test_pfor2band.cpp` pass. Targeted b=0→b>0 transition tests in `/tmp/test_pfor_targeted.cpp` pass. Bench against `simdcomp_fused` and `custom_direct_access` on srtm and Landsat: all three codecs produce identical `result` fields for both `--op add` and `--op noop`.

**CR results (local, srtm/Landsat, OP_ADD):**
- srtm: pfor_for_2band CR=0.1053, simdcomp_fused CR=0.1151. PFor ~8% smaller.
- Landsat: pfor_for_2band CR=0.4738, simdcomp_fused CR=0.6785. PFor ~30% smaller.

**Run / verify locally:**
```bash
# Build
cmake --build build --target bench_pipeline_2band Ndvi2BandPFor -j8

# Unit tests (link against built objects)
g++ -O3 -march=native -mno-avx512f -std=c++20 \
  -I external/TurboPFor/include -I external/simdcomp/include \
  /tmp/test_pfor2band.cpp /tmp/test_pfor_targeted.cpp \
  build/libNdvi2BandPFor.a build/libNdvi2Band.a \
  external/TurboPFor/libic.a external/simdcomp/libsimdcomp.a -o /tmp/t && /tmp/t

# Correctness bench (all three results must match)
TIF=/home/omar/diss/geotiffs/srtm_45_15.tif
for c in simdcomp_fused pfor_for_2band custom_direct_access; do
  ./build/bench_pipeline_2band "$TIF" --fileB "$TIF" -b 256 -n 10 -r 3 \
    --icodec "$c" --op add --normalize --threads 1
done
```

**Server bench (sherwood, X=1, n=500, r=6, --rs 2, Landsat B5+B4):**

| op | simdcomp_fused | pfor_for_2band | custom_direct_access |
|---|---|---|---|
| noop | 16298 ns | 24074 ns (+48%) | 20925 ns |
| add | 18705 ns | 24311 ns (+30%) | 22592 ns |

CR: pfor_for_2band=0.5125, simdcomp_fused=0.5708, raw=1.000. PFor is ~10% better CR but ~30-48% slower decode at X=1 (Landsat is high-entropy → many exceptions → scalar bitunpack16+pshufb is the bottleneck). Pattern matches single-band TurboPFor on dense-exception data. For sparser data (srtm) PFor would flip faster; for X>1 bandwidth savings may help. All three codecs agree on result (correctness ✓).

## Session 2026-06-17: fine-window FoR + SIMD SKIP_MERGE fix for pfor_for_2band — DONE, server bench DONE

**SIMD SKIP_MERGE_EXC fix:** [ndvi2band_pfor_w256.cpp](external/TurboPFor/lib/ndvi2band_pfor_w256.cpp) `PFOR_SKIP_MERGE_EXC` path replaced scalar `for k<xn: exc_sum += ex[k]<<b` loops with `sum_excess_u16()` helper (AVX2 `unpacklo/hi_epi16` widen+sum, mirrors `vp4d256v16_fused.c`).

**Fine-window FoR added to pfor_for_2band:** New encoder `p4nenc256v16_for2band_w(n, w, ...)` + decoder `ndvi2_pfor_for_indep_w(n, w, op)` with per-window-size dispatch tables `g_plain_csc/half/qtr` and `g_pfor_csc/half/qtr`. Codec names: `pfor_for_2band_w4/8/16/32/64/128/256`. Kernel modes:
- w≥16 (csc): 1 anchor/OutReg → `set1_epi16(ancs[J])`
- w=8 (half): 2 anchors/OutReg → `_mm256_set_m128i(set1(ancs[2J+1]), set1(ancs[2J]))`
- w=4 (qtr): 4 anchors/OutReg → `_mm_setr_epi16(a,a,a,a,b,b,b,b)` per quarter

**Server bench results (sherwood, n=500, r=11, rs=3, X=1, op=add, Landsat B5+B4 mosaic):**

Lossless:
| codec | CR | ns |
|---|---|---|
| simdcomp_fused | 0.5708 | 15,346 |
| pfor_for_2band_w256 | 0.5125 | 33,784 |
| w128 | 0.5121 | 35,613 |
| w64 | 0.5134 | 37,747 |
| w32 | 0.5193 | 42,518 |
| w16 | 0.5357 | 47,565 |
| w8 | 0.5745 | 46,024 |
| w4 | 0.6400 | 50,702 |

LERC t10pct (MaxZ B5=1214.8, B4=939.1, NRMSE≈4.3%):
| codec | CR | ns |
|---|---|---|
| simdcomp_fused | 0.5711 | 15,705 |
| pfor_for_2band_w256 | 0.5128 | 34,134 |
| w128 | 0.5118 | 36,291 |
| w64 | 0.5113 | 39,934 |
| w32 | 0.5009 | 57,313 |
| w16 | 0.4527 | 65,776 |
| **w8** | **0.3719** | **43,315** |
| w4 | 0.4309 | 40,799 |

**Key lessons:**
- `simdcomp_fused` barely benefits from LERC (CR 0.5708→0.5711): b picked from sub-block max, LERC quantization grid doesn't help
- **w=8 is LERC sweet spot** (CR 0.3719 = 1.54× better than simdcomp lossless) — LERC quantization aligns with 8-element FoR windows
- w=4 CR rebounds (0.4309) — 4 anchors/OutReg overhead + residuals can't shrink further
- Fine windows HURT lossless CR (no quantization to exploit); global w=256 best for lossless Landsat
- **w=16 is slowest** on LERC (65,776 ns) despite moderate CR — csc per-OutReg anchor extraction more expensive than half/qtr broadcasts at fine windows
- All PFor+FoR 2.2–4.2× slower than simdcomp at X=1 (exception-bound, dense Landsat)
- **Shared-b penalty**: `shared_b = max(bA, bB)` dilutes LERC CR gains vs single-band
- LERC TIFs at `/scratch/omsst2/diss/temp/for_cr_bench_2band/` (B5=MaxZ1214p789, B4=MaxZ939p0739)
- Bandwidth crossover (w=8 LERC beats simdcomp) expected around X≈4-8; NOT yet verified with multithread bench
