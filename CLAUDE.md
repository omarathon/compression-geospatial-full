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
