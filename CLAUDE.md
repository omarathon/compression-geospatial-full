# CLAUDE.md

## 1) Hard Rules

- Goal: keep geospatial uint16 data **compressed in RAM** and run aggregations / NDVI / multiply directly over the fused-decode SIMD pipeline. Speedup comes from smaller working set fitting in L2/L3 + fused per-OutReg work.
- Honest decode: per-OutReg `add_anchor` must stay on the OutReg dep chain even though sum-fused doesn't strictly need it. Do **not** shortcut by adding `length * anchor` at the end.
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

**PERF (128, n1000 r6 rs2, sx vs baseline):** coarse (w128/256, hier l128/256) ≈ parity–1.17× **where FoR cuts CR** (WorldCover 8% vs 14%); slope ~0.85; srtm slower (baseline near-free at b=0). Fine (w8–w64) 0.3–0.9×, **port-5-bound** (broadcasts + aggregate both port 5); w8 floor ~1.5× (16 distinct broadcasts, unavoidable). Decode = extract(~2400) + broadcasts(0–1800 by window) + per-lane add(~800) + aggregate(madd~1000/unpack~2800). Verdict: **FoR-128 is a coarse-window CR play; fine windows trade decode speed for CR.** delta+zigzag fusion considered & rejected — ~2–3× FoR's ops + serial carry/prefix-sum chain (FoR OutRegs are independent), slower than even w=4.

**Next — 256-width simdcomp FoR:** run gen `--width 256`. 256-block = 16 OutRegs × **16 lanes** → corrections amortize over 16 (per-element overhead ~halves); windows shift: w=16→"1 anchor/OutReg" (the cscalar0 case), w=8→half, w=4→quarter (new worst). **MUST inline the kernel** (cross-TU 256 `sum` spills → 4× regression, see inline-decode header trick). The old [simdcomp_for_codec_uint16.h](src/codecs/uint16/simdcomp_for_codec_uint16.h) (256 global/local/hier) uses the slow `simdunpack_u16_corrected` array path — replace with cscalar/uniform/madd. Expect ratio similar-to-better than 128 for medium windows.

**Next — TurboPFor FoR fused (128 & 256):** same FoR-correction idea + exceptions. `uncompressblockPFOR_u16_corrected_for` ([simdpfor_u16.h](external/FastPFor/headers/simdpfor_u16.h)) already does global/uniform anchor; extend to per-window (cscalar) + madd. 256 fused lives in [vp4d256v16_fused.c](external/TurboPFor/lib/vp4d256v16_fused.c), wrappers `turbopfor_fused_{,256_}codec_uint16.h`.
