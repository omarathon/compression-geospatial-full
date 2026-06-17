#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <type_traits>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "codec_collection_uint16.h"
#include "direct_codec.h"
#include "direct_codec_uint16.h"
#include "gdal_priv.h"
#include "simdcomp_fused_codec_uint16.h"  // SimdCompFusedCodecU16 (.compressed bytes)
#include <cmath>

// Two-band lock-step fused aggregate kernels (from libNdvi2Band).
// op: 0=noop(xor) 1=add 2=div(NDVI). ndvi2_indep decodes from simdcomp_fused
// bytes in lock-step; ndvi2_raw runs the same op over raw uint16 (uncompressed).
extern "C" double ndvi2_indep(const uint8_t* encA, const uint8_t* encB,
                              size_t length, int op);
extern "C" double ndvi2_raw(const uint16_t* a, const uint16_t* b, size_t length,
                            int op);
extern "C" void ndvi2_set_count_threshold(float x);

// Two-band FoR+PFor fused kernels (from libNdvi2BandPFor).
// Shared-b encoding: both bands use max(opt_bA, opt_bB) per 256-block.
extern "C" void p4nenc256v16_for2band(
    const uint16_t* inA, const uint16_t* inB, size_t n,
    uint16_t* anchorsA, uint16_t* anchorsB,
    uint8_t* outA, size_t* sizeA,
    uint8_t* outB, size_t* sizeB);
extern "C" size_t p4nbound256v16_for2band(size_t n);
extern "C" double ndvi2_pfor_for_indep(
    const uint8_t* encA, const uint16_t* anchorsA,
    const uint8_t* encB, const uint16_t* anchorsB,
    size_t n, int op);

// Per-block pair for the pfor_for_2band codec.
// anchsA/anchsB hold one uint16 anchor per 256-element sub-block.
struct Block2BandPFor {
  std::vector<uint8_t>  encA, encB;
  std::vector<uint16_t> anchsA, anchsB;
};
using Grid2BandPFor = std::vector<Block2BandPFor>;
static float gCountThreshold = 0.3f;
static volatile double g_ndvi_sink2 = 0.0;

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
// Full CPU set the process may run on (from numactl --physcpubind), captured
// once at startup BEFORE the builder narrows its own affinity.
static cpu_set_t gFullMask;
static int gFullCount = 0;
static void CaptureFullAffinity() {
  CPU_ZERO(&gFullMask);
  if (sched_getaffinity(0, sizeof(gFullMask), &gFullMask) == 0)
    gFullCount = CPU_COUNT(&gFullMask);
}
static int NthAllowedCpu(int n) {  // n-th set CPU in gFullMask, or -1
  int seen = 0;
  for (int c = 0; c < CPU_SETSIZE; c++)
    if (CPU_ISSET(c, &gFullMask)) { if (seen++ == n) return c; }
  return -1;
}
static void PinToCpu(int cpu) {
  if (cpu < 0) return;
  cpu_set_t one; CPU_ZERO(&one); CPU_SET(cpu, &one);
  pthread_setaffinity_np(pthread_self(), sizeof(one), &one);
}
// Worker t -> t-th allowed CPU. Uses gFullMask (not sched_getaffinity) so it is
// robust to the inherited mask being narrowed by the builder pinning itself.
static void PinThreadToAllowedCpu(int idx) {
  if (gFullCount > 0) PinToCpu(NthAllowedCpu(idx % gFullCount));
}
// Builder/main thread -> LAST allowed CPU. Reserve one extra core in
// --physcpubind (ideally on the other socket) so the grid build never pre-warms
// any worker's CCX L3 -> every worker (incl. X=1) starts cold and comparable.
static void PinMainBuilderCpu() {
  if (gFullCount > 0) PinToCpu(NthAllowedCpu(gFullCount - 1));
}
#else
static void CaptureFullAffinity() {}
static void PinThreadToAllowedCpu(int) {}
static void PinMainBuilderCpu() {}
#endif

static bool gTraceSums = false;
static int gNumThreads = 1;  // X: parallel decode workers (each does -n blocks).

static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
BuildAllCodecsPipeline() {
  auto pool = InitCodecs(/* nonCascaded */ true, nullptr);
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<DeltaCodec>()))
    pool.push_back(std::move(c));
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<RLECodec>()))
    pool.push_back(std::move(c));
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<FORCodec>()))
    pool.push_back(std::move(c));
  pool.push_back(std::make_unique<DirectAccessCodec>());
  return pool;
}

static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
SplitIntoFullBlocks(GDALRasterBand* band, int rasterWidth, int rasterHeight,
                    int blockSize, int numBlocks,
                    std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec,
                    int32_t min, bool hasNoData, int32_t nodata32,
                    Transformation transformation, Ordering ordering,
                    bool normalize, int32_t globalGCD) {
  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
  codecs.reserve(numBlocks);

  for (auto& offset : SampleBlockOffsets(blocksInWidth, blocksInHeight,
                                          blockSize, numBlocks)) {
    std::vector<int32_t> blockData(blockSize * blockSize);
    CPLErr err =
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       blockData.data(), blockSize, blockSize, GDT_Int32, 0, 0);
    if (err != CE_None)
      throw std::runtime_error("Error reading raster block data");
    if (normalize) {
      for (auto& v : blockData) {
        if (hasNoData && v == nodata32)
          v = 0;
        else
          v = static_cast<int32_t>(
              (static_cast<int64_t>(v) - static_cast<int64_t>(min)) / globalGCD);
      }
    } else if (min < 0) {
      int32_t shift = -min;
      for (auto& v : blockData) {
        if (hasNoData && v == nodata32)
          v = 0;
        else
          v += shift;
      }
    } else if (hasNoData) {
      for (auto& v : blockData)
        if (v == nodata32) v = 0;
    }
    RemapAndTransform(blockData, ordering, transformation, blockSize);

    std::unique_ptr<StatefulIntegerCodec<int32_t>> cloned(
        baseCodec->CloneFresh());
    cloned->AllocEncoded(blockData.data(), blockData.size());
    cloned->EncodeArray(blockData.data(), blockData.size());
    codecs.push_back(std::move(cloned));
  }

  return codecs;
}

static void BenchmarkAccess(
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> accessCodec, int blockSize,
    AccessPattern accessPattern, AccessTransformation accessTransformation,
    RunningStats& statsDec, RunningStats& statsTrans, RunningStats& statsEnc) {
  srand(1);

  bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
  bool isDirectReenc = (accessCodec->name() == "custom_direct_access");
  bool dataChange = AccessTransformationMutatesData(accessTransformation);

  std::vector<int32_t> decbuf(blockSize * blockSize +
                               codecs[0]->GetOverflowSize(blockSize * blockSize));

  std::vector<std::size_t> accessIndexes(codecs.size());
  std::iota(accessIndexes.begin(), accessIndexes.end(), 0);
  if (accessPattern != AccessPattern::Linear) {
    std::default_random_engine engine(1);
    std::shuffle(accessIndexes.begin(), accessIndexes.end(), engine);
  }

  for (std::size_t i = 0; i < codecs.size(); i++) {
    std::size_t blockIndex = accessIndexes[i];
    auto& codec = codecs[blockIndex];

    auto benchblock = [&](std::vector<int32_t>& buf) {
      std::size_t decodeTime = 0;
      if (!isDirectAccess) {
        auto t0 = std::chrono::steady_clock::now();
        codec->DecodeArray(buf.data(), blockSize * blockSize);
        auto t1 = std::chrono::steady_clock::now();
        decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
      }
      statsDec.Update(decodeTime);

      std::size_t transTime =
          ApplyAccessTransformation(buf, accessTransformation, blockSize);
      if (gTraceSums) std::cout << std::format("TRACE block={} sum={}", blockIndex, kLinearSumSink) << '\n';
      statsTrans.Update(transTime);

      if (dataChange) {
        std::unique_ptr<StatefulIntegerCodec<int32_t>> reenc(
            accessCodec->CloneFresh());
        if (isDirectReenc) {
          statsEnc.Update(0);
          reenc->AllocEncoded(buf.data(), blockSize * blockSize);
          reenc->EncodeArray(buf.data(), blockSize * blockSize);
        } else {
          reenc->AllocEncoded(buf.data(), blockSize * blockSize);
          auto t0 = std::chrono::steady_clock::now();
          reenc->EncodeArray(buf.data(), blockSize * blockSize);
          auto t1 = std::chrono::steady_clock::now();
          statsEnc.Update(
              std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                  .count());
        }
        codecs[blockIndex] = std::move(reenc);
      }
    };

    if (!isDirectAccess)
      benchblock(decbuf);
    else
      benchblock(codec->GetEncoded());
  }
}

// One (ordering × initTrans × accessTrans) combination.
struct BenchCombo {
  Ordering ordering;
  Transformation initTrans;
  AccessTransformation accessTrans;
};

static void RunOneCombination(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int32_t min,
    bool hasNoData, int32_t nodata32,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<int32_t>& baseCodec,
    StatefulIntegerCodec<int32_t>& accessCodec,
    bool normalize, int32_t globalGCD) {
  std::cout << "**BENCHMARK ACCESS**\n";
  std::cout << std::format("file={},band={},blocksize={},numblocks={},numreps={},numskip={},basecodec={},"
               "accesscodec={},ordering={},initialtransformation={},"
               "sampleaccesspattern={},accesstransformation={},normalize={},globalGCD={}",
               filePath, band->GetBand(), blockSize, numBlocks, numReps, numSkip,
               baseCodec.name(), accessCodec.name(),
               ToString(combo.ordering), ToString(combo.initTrans),
               ToString(accessPattern), ToString(combo.accessTrans), normalize, globalGCD) << '\n';

  RunningStats statsDec, statsTrans, statsEnc;
  RepMedian medDec, medTrans, medEnc;

  for (int rep = 0; rep < numReps; rep++) {
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expBase(
        baseCodec.CloneFresh());
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expAccess(
        accessCodec.CloneFresh());

    auto codecGrid =
        SplitIntoFullBlocks(band, nXSize, nYSize, blockSize, numBlocks,
                             std::move(expBase), min, hasNoData, nodata32,
                             combo.initTrans, combo.ordering, normalize, globalGCD);
    if (codecGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }

    if (rep < numSkip) {
      RunningStats dummy1, dummy2, dummy3;
      BenchmarkAccess(codecGrid, std::move(expAccess), blockSize, accessPattern,
                      combo.accessTrans, dummy1, dummy2, dummy3);
    } else {
      double prevDec = statsDec.Total(), prevTrans = statsTrans.Total(), prevEnc = statsEnc.Total();
      BenchmarkAccess(codecGrid, std::move(expAccess), blockSize, accessPattern,
                      combo.accessTrans, statsDec, statsTrans, statsEnc);
      medDec.Push(statsDec.Total()   - prevDec,   codecGrid.size());
      medTrans.Push(statsTrans.Total() - prevTrans, codecGrid.size());
      medEnc.Push(statsEnc.Total()   - prevEnc,   codecGrid.size());
    }
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},medtimedec:{},mintimedec:{},maxtimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},medtimetrans:{},mintimetrans:{},maxtimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},medtimeenc:{},mintimeenc:{},maxtimeenc:{},vartimeenc:{}",
               statsDec.Total(),   statsDec.mean,   medDec.Median(),   statsDec.Min(),   statsDec.Max(),   statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, medTrans.Median(), statsTrans.Min(), statsTrans.Max(), statsTrans.Variance(),
               statsEnc.Total(),   statsEnc.mean,   medEnc.Median(),   statsEnc.Min(),   statsEnc.Max(),   statsEnc.Variance()) << '\n';
}

static void RunAllBenchmarks(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int32_t min,
    bool hasNoData, int32_t nodata32,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns,
    bool normalize, int32_t globalGCD) {
  std::vector<BenchCombo> combos;
  for (auto& o : orderings)
    for (auto& it : initialTransformations)
      for (auto& at : accessTransformations)
        combos.push_back({ParseOrdering(o), ParseTransformation(it),
                           ParseAccessTransformation(at)});

  for (auto& combo : combos)
    for (auto& baseCodec : baseCodecs)
      for (auto& accessCodec : accessCodecs)
        for (auto& pattern : sampleAccessPatterns)
          RunOneCombination(band, nXSize, nYSize, filePath, blockSize,
                            numBlocks, numReps, numSkip, min, hasNoData, nodata32,
                            combo, ParseAccessPattern(pattern), *baseCodec,
                            *accessCodec, normalize, globalGCD);
}

// ── uint16 pipeline ─────────────────────────────────────────────────────────

static std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
SplitIntoFullBlocksU16(GDALRasterBand* band, int rasterWidth, int rasterHeight,
                       int blockSize, int numBlocks,
                       std::unique_ptr<StatefulIntegerCodec<uint16_t>> baseCodec,
                       int16_t minShift, bool hasNoData, int16_t nodata16,
                       uint16_t nodataU16,
                       bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.reserve(numBlocks);

  for (auto& offset : SampleBlockOffsets(blocksInWidth, blocksInHeight,
                                          blockSize, numBlocks)) {
    std::vector<uint16_t> blockData(blockSize * blockSize);
    if (minShift < 0) {
      // Read as int16, replace nodata with 0, shift to uint16
      std::vector<int16_t> signed_buf(blockSize * blockSize);
      CPLErr err =
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         signed_buf.data(), blockSize, blockSize, GDT_Int16, 0, 0);
      if (err != CE_None)
        throw std::runtime_error("Error reading raster block data");
      int32_t shift = -static_cast<int32_t>(minShift);
      for (size_t i = 0; i < blockData.size(); i++) {
        if (hasNoData && signed_buf[i] == nodata16)
          blockData[i] = 0;  // nodata → 0 contribution to sum
        else
          blockData[i] = static_cast<uint16_t>(static_cast<int32_t>(signed_buf[i]) + shift);
      }
    } else {
      CPLErr err =
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         blockData.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
      if (err != CE_None)
        throw std::runtime_error("Error reading raster block data");
      if (hasNoData) {
        for (auto& v : blockData)
          if (v == nodataU16) v = 0;
      }
    }

    if (normalize) {
      for (auto& v : blockData) {
        if (v == 0) continue;  // nodata sentinel, leave as 0
        v = static_cast<uint16_t>(
            (static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16)) / normGCDU16);
      }
    }

    std::unique_ptr<StatefulIntegerCodec<uint16_t>> cloned(
        baseCodec->CloneFresh());
    cloned->AllocEncoded(blockData.data(), blockData.size());
    cloned->EncodeArray(blockData.data(), blockData.size());
    codecs.push_back(std::move(cloned));
  }

  return codecs;
}

static void BenchmarkAccessU16(
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> accessCodec, int blockSize,
    AccessPattern accessPattern, AccessTransformation accessTransformation,
    RunningStats& statsDec, RunningStats& statsTrans, RunningStats& statsEnc) {
  srand(1);

  bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
  bool isDirectReenc = (accessCodec->name() == "custom_direct_access");
  bool dataChange = AccessTransformationMutatesData(accessTransformation);

  std::vector<std::size_t> accessIndexes(codecs.size());
  std::iota(accessIndexes.begin(), accessIndexes.end(), 0);
  if (accessPattern != AccessPattern::Linear) {
    std::default_random_engine engine(1);
    std::shuffle(accessIndexes.begin(), accessIndexes.end(), engine);
  }

  // Parallel decode: gNumThreads workers, each owns a disjoint contiguous slice
  // of the grid ([t*per, (t+1)*per)) so no two threads touch the same codec
  // object or its buffer. Each worker has its OWN decode buffer and its OWN
  // stats; the Welford stats are merged into the shared accumulators after join.
  // Reads + per-thread buffers ⇒ no data races (kLinearSumSink is a benign
  // anti-DCE sink). The grid holds n*gNumThreads blocks, so per == n.
  const int nThreads = std::max(1, gNumThreads);
  const std::size_t total = codecs.size();
  const std::size_t per = total / static_cast<std::size_t>(nThreads);

  std::vector<RunningStats> tDec(nThreads), tTrans(nThreads), tEnc(nThreads);

  auto worker = [&](int t) {
    PinThreadToAllowedCpu(t);
    std::vector<uint16_t> decbuf(
        blockSize * blockSize +
        codecs[0]->GetOverflowSize(blockSize * blockSize));
    RunningStats& sd = tDec[t];
    RunningStats& st = tTrans[t];
    RunningStats& se = tEnc[t];
    const std::size_t lo = static_cast<std::size_t>(t) * per;
    const std::size_t hi = (t == nThreads - 1) ? total : lo + per;

    for (std::size_t i = lo; i < hi; i++) {
      std::size_t blockIndex = accessIndexes[i];
      auto& codec = codecs[blockIndex];

      auto benchblock = [&](std::vector<uint16_t>& buf) {
        std::size_t decodeTime = 0;
        if (!isDirectAccess) {
          auto t0 = std::chrono::steady_clock::now();
          codec->DecodeArray(buf.data(), blockSize * blockSize);
          auto t1 = std::chrono::steady_clock::now();
          decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        }
        sd.Update(decodeTime);

        std::size_t transTime =
            ApplyAccessTransformation(buf, accessTransformation, blockSize);
        if (gTraceSums) std::cout << std::format("TRACE block={} sum={}", blockIndex, kLinearSumSink) << '\n';
        st.Update(transTime);

        if (dataChange) {
          std::unique_ptr<StatefulIntegerCodec<uint16_t>> reenc(
              accessCodec->CloneFresh());
          if (isDirectReenc) {
            se.Update(0);
            reenc->AllocEncoded(buf.data(), blockSize * blockSize);
            reenc->EncodeArray(buf.data(), blockSize * blockSize);
          } else {
            reenc->AllocEncoded(buf.data(), blockSize * blockSize);
            auto t0 = std::chrono::steady_clock::now();
            reenc->EncodeArray(buf.data(), blockSize * blockSize);
            auto t1 = std::chrono::steady_clock::now();
            se.Update(
                std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                    .count());
          }
          codecs[blockIndex] = std::move(reenc);
        }
      };

      if (!isDirectAccess)
        benchblock(decbuf);
      else
        benchblock(codec->GetEncoded());
    }
  };

  // Always spawn worker threads (even X=1) so the reader is never the builder
  // thread — uniform cold-start across all X (no warm-cache advantage at X=1).
  std::vector<std::thread> pool;
  pool.reserve(nThreads);
  for (int t = 0; t < nThreads; t++) pool.emplace_back(worker, t);
  for (auto& th : pool) th.join();

  // Merge per-thread Welford stats into the shared accumulators (after join).
  auto mergeStats = [](RunningStats& a, const RunningStats& b) {
    if (b.n == 0) return;
    if (a.n == 0) { a = b; return; }
    double na = static_cast<double>(a.n), nb = static_cast<double>(b.n);
    double nt = na + nb, delta = b.mean - a.mean;
    a.M2 += b.M2 + delta * delta * na * nb / nt;
    a.mean += delta * nb / nt;
    a.n = static_cast<std::size_t>(nt);
    a.min_val = std::min(a.min_val, b.min_val);
    a.max_val = std::max(a.max_val, b.max_val);
  };
  for (int t = 0; t < nThreads; t++) {
    mergeStats(statsDec, tDec[t]);
    mergeStats(statsTrans, tTrans[t]);
    mergeStats(statsEnc, tEnc[t]);
  }
}

// Persistent spin-coordinated worker pool for a STATIC grid (sum, no re-encode).
// The X workers are spawned ONCE and process all numReps in lockstep via a
// spin-barrier, so the cores never go idle between reps. Under schedutil (no
// root to set the governor) idle gaps from per-rep respawn made the governor
// downclock memory-stalled workers; a continuously-runnable core (spinning or
// decoding) stays "utilized" -> kept at boost -> stable frequency.
static void RunStaticRepsU16(
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    int blockSize, AccessPattern accessPattern,
    AccessTransformation accessTransformation, int numReps, int numSkip,
    RunningStats& statsDec, RunningStats& statsTrans,
    RepMedian& medDec, RepMedian& medTrans) {
  const bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");

  std::vector<std::size_t> accessIndexes(codecs.size());
  std::iota(accessIndexes.begin(), accessIndexes.end(), 0);
  if (accessPattern != AccessPattern::Linear) {
    std::default_random_engine engine(1);
    std::shuffle(accessIndexes.begin(), accessIndexes.end(), engine);
  }

  const int X = std::max(1, gNumThreads);
  const std::size_t total = codecs.size();
  const std::size_t per = total / static_cast<std::size_t>(X);

  std::vector<RunningStats> tDec(X), tTrans(X);
  std::vector<double> repDec(X, 0.0), repTrans(X, 0.0);
  std::atomic<int> goRep{-1};
  std::atomic<int> doneCount{0};

  auto worker = [&](int t) {
    PinThreadToAllowedCpu(t);
    std::vector<uint16_t> decbuf(
        blockSize * blockSize +
        codecs[0]->GetOverflowSize(blockSize * blockSize));
    const std::size_t lo = static_cast<std::size_t>(t) * per;
    const std::size_t hi = (t == X - 1) ? total : lo + per;
    for (int myRep = 0; myRep < numReps; myRep++) {
      while (goRep.load(std::memory_order_acquire) != myRep)
        __builtin_ia32_pause();
      const bool timed = (myRep >= numSkip);
      double rd = 0.0, rt = 0.0;
      for (std::size_t i = lo; i < hi; i++) {
        auto& codec = codecs[accessIndexes[i]];
        std::vector<uint16_t>& buf = isDirectAccess ? codec->GetEncoded() : decbuf;
        std::size_t dt = 0;
        if (!isDirectAccess) {
          auto c0 = std::chrono::steady_clock::now();
          codec->DecodeArray(buf.data(), blockSize * blockSize);
          auto c1 = std::chrono::steady_clock::now();
          dt = std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count();
        }
        if (timed) tDec[t].Update(dt);
        rd += static_cast<double>(dt);
        std::size_t tt = ApplyAccessTransformation(buf, accessTransformation, blockSize);
        if (timed) tTrans[t].Update(tt);
        rt += static_cast<double>(tt);
      }
      repDec[t] = rd;
      repTrans[t] = rt;
      doneCount.fetch_add(1, std::memory_order_release);
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(X);
  for (int t = 0; t < X; t++) pool.emplace_back(worker, t);

  for (int r = 0; r < numReps; r++) {
    doneCount.store(0, std::memory_order_release);
    goRep.store(r, std::memory_order_release);          // release workers for rep r
    while (doneCount.load(std::memory_order_acquire) != X)
      __builtin_ia32_pause();                           // main spins too (stays busy)
    if (r >= numSkip) {
      double td = 0.0, tt = 0.0;
      for (int t = 0; t < X; t++) { td += repDec[t]; tt += repTrans[t]; }
      medDec.Push(td, total);
      medTrans.Push(tt, total);
    }
  }
  for (auto& th : pool) th.join();

  auto mergeStats = [](RunningStats& a, const RunningStats& b) {
    if (b.n == 0) return;
    if (a.n == 0) { a = b; return; }
    double na = static_cast<double>(a.n), nb = static_cast<double>(b.n);
    double nt = na + nb, delta = b.mean - a.mean;
    a.M2 += b.M2 + delta * delta * na * nb / nt;
    a.mean += delta * nb / nt;
    a.n = static_cast<std::size_t>(nt);
    a.min_val = std::min(a.min_val, b.min_val);
    a.max_val = std::max(a.max_val, b.max_val);
  };
  for (int t = 0; t < X; t++) { mergeStats(statsDec, tDec[t]); mergeStats(statsTrans, tTrans[t]); }
}

static void RunOneCombinationU16(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<uint16_t>& baseCodec,
    StatefulIntegerCodec<uint16_t>& accessCodec,
    bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
  std::cout << "**BENCHMARK ACCESS**\n";
  std::cout << std::format("file={},band={},blocksize={},numblocks={},numreps={},numskip={},numthreads={},basecodec={},"
               "accesscodec={},ordering={},initialtransformation={},"
               "sampleaccesspattern={},accesstransformation={},normalize={},normMinU16={},normGCDU16={}",
               filePath, band->GetBand(), blockSize, numBlocks, numReps, numSkip, gNumThreads,
               baseCodec.name(), accessCodec.name(),
               ToString(combo.ordering), ToString(combo.initTrans),
               ToString(accessPattern), ToString(combo.accessTrans),
               normalize, normMinU16, normGCDU16) << '\n';

  RunningStats statsDec, statsTrans, statsEnc;
  RepMedian medDec, medTrans, medEnc;
  double meanCompRatio      = 0.0;
  double meanExceptPerBlock = -1.0;

  // For a non-mutating access transform (sum/min/max/...), BenchmarkAccessU16
  // never re-encodes (dataChange=false), so the encoded grid is IDENTICAL every
  // rep. Build+read+encode it ONCE and reuse across reps instead of re-reading +
  // re-encoding the whole n-block sample every rep (the dominant cost at large n,
  // ~13x at r=10 rs=3). Mutating transforms keep the per-rep rebuild.
  const bool staticGrid = !AccessTransformationMutatesData(combo.accessTrans);

  auto compute_cr = [&](const auto& grid) {
    const double uncompBytes =
        static_cast<double>(blockSize) * blockSize * sizeof(uint16_t);
    double sumRatio = 0.0, sumExcept = 0.0;
    for (const auto& c : grid) {
      sumRatio  += (c->EncodedNumValues() * c->EncodedSizeValue()) / uncompBytes;
      sumExcept += c->MeanExceptionsPerInnerBlock();
    }
    const double n = static_cast<double>(grid.size());
    meanCompRatio      = sumRatio  / n;
    meanExceptPerBlock = sumExcept / n;
  };

  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> sharedGrid;
  if (staticGrid) {
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> expBase(baseCodec.CloneFresh());
    sharedGrid =
        SplitIntoFullBlocksU16(band, nXSize, nYSize, blockSize, numBlocks,
                               std::move(expBase), minShift, hasNoData, nodata16,
                               nodataU16, normalize, normMinU16, normGCDU16);
    if (sharedGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }
    compute_cr(sharedGrid);
  }

  if (staticGrid) {
    // Persistent spin-coordinated pool -> cores stay boosted (schedutil fix).
    RunStaticRepsU16(sharedGrid, blockSize, accessPattern, combo.accessTrans,
                     numReps, numSkip, statsDec, statsTrans, medDec, medTrans);
  } else {
    for (int rep = 0; rep < numReps; rep++) {
      std::unique_ptr<StatefulIntegerCodec<uint16_t>> expBase(baseCodec.CloneFresh());
      auto localGrid =
          SplitIntoFullBlocksU16(band, nXSize, nYSize, blockSize, numBlocks,
                                 std::move(expBase), minShift, hasNoData, nodata16,
                                 nodataU16, normalize, normMinU16, normGCDU16);
      if (localGrid.empty()) {
        std::cerr << "NO CODECS FORMING GRID.\n";
        return;
      }
      if (rep == 0) compute_cr(localGrid);
      std::unique_ptr<StatefulIntegerCodec<uint16_t>> expAccess(
          accessCodec.CloneFresh());
      if (rep < numSkip) {
        RunningStats dummy1, dummy2, dummy3;
        BenchmarkAccessU16(localGrid, std::move(expAccess), blockSize, accessPattern,
                           combo.accessTrans, dummy1, dummy2, dummy3);
      } else {
        double prevDec = statsDec.Total(), prevTrans = statsTrans.Total(), prevEnc = statsEnc.Total();
        BenchmarkAccessU16(localGrid, std::move(expAccess), blockSize, accessPattern,
                           combo.accessTrans, statsDec, statsTrans, statsEnc);
        medDec.Push(statsDec.Total()     - prevDec,   localGrid.size());
        medTrans.Push(statsTrans.Total() - prevTrans, localGrid.size());
        medEnc.Push(statsEnc.Total()     - prevEnc,   localGrid.size());
      }
    }
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},medtimedec:{},mintimedec:{},maxtimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},medtimetrans:{},mintimetrans:{},maxtimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},medtimeenc:{},mintimeenc:{},maxtimeenc:{},vartimeenc:{},"
               "meancompratio:{:.6f},meanexceptperblock:{:.3f}",
               statsDec.Total(),   statsDec.mean,   medDec.Median(),   statsDec.Min(),   statsDec.Max(),   statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, medTrans.Median(), statsTrans.Min(), statsTrans.Max(), statsTrans.Variance(),
               statsEnc.Total(),   statsEnc.mean,   medEnc.Median(),   statsEnc.Min(),   statsEnc.Max(),   statsEnc.Variance(),
               meanCompRatio, meanExceptPerBlock) << '\n';
}

static void RunAllBenchmarksU16(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns,
    bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
  std::vector<BenchCombo> combos;
  for (auto& o : orderings)
    for (auto& it : initialTransformations)
      for (auto& at : accessTransformations)
        combos.push_back({ParseOrdering(o), ParseTransformation(it),
                           ParseAccessTransformation(at)});

  for (auto& combo : combos)
    for (auto& baseCodec : baseCodecs)
      for (auto& accessCodec : accessCodecs)
        for (auto& pattern : sampleAccessPatterns)
          RunOneCombinationU16(band, nXSize, nYSize, filePath, blockSize,
                              numBlocks, numReps, numSkip, minShift,
                              hasNoData, nodata16, nodataU16, combo,
                              ParseAccessPattern(pattern), *baseCodec,
                              *accessCodec, normalize, normMinU16, normGCDU16);
}

// ── 2-band additions ─────────────────────────────────────────────────────────

// Per-band normalize/nodata params (same scan as the single-band uint16 path).
struct BandU16Params {
  int16_t minShift = 0;
  bool hasNoData = false;
  int16_t nodata16 = 0;
  uint16_t nodataU16 = 0;
  uint16_t normMinU16 = 0;
  uint16_t normGCDU16 = 1;
};

static BandU16Params ScanBandU16(GDALRasterBand* band, int blockSize,
                                 int numBlocks, bool normalize) {
  BandU16Params p;
  GDALDataType dt = band->GetRasterDataType();
  int hnd = 0;
  double rawNoData = band->GetNoDataValue(&hnd);
  if (hnd) {
    if (dt == GDT_Int16 && (rawNoData < -32768.0 || rawNoData > 32767.0)) hnd = 0;
    else if ((dt == GDT_UInt16 || dt == GDT_Byte) &&
             (rawNoData < 0.0 || rawNoData > 65535.0)) hnd = 0;
  }
  p.hasNoData = hnd != 0;
  p.nodata16 = p.hasNoData ? (int16_t)rawNoData : 0;
  p.nodataU16 = p.hasNoData ? (uint16_t)rawNoData : 0;
  const int nX = band->GetXSize(), nY = band->GetYSize();
  auto offs = [&] {
    return SampleBlockOffsets(nX / blockSize, nY / blockSize, blockSize, numBlocks);
  };
  if (dt == GDT_Int16) {
    int16_t min16 = std::numeric_limits<int16_t>::max();
    for (auto& o : offs()) {
      std::vector<int16_t> t(blockSize * blockSize);
      band->RasterIO(GF_Read, o.x, o.y, blockSize, blockSize, t.data(), blockSize,
                     blockSize, GDT_Int16, 0, 0);
      for (auto v : t) if (!p.hasNoData || v != p.nodata16) min16 = std::min(min16, v);
    }
    p.minShift = min16;
    if (normalize && min16 < std::numeric_limits<int16_t>::max()) {
      uint32_t g = 0;
      for (auto& o : offs()) {
        std::vector<int16_t> t(blockSize * blockSize);
        band->RasterIO(GF_Read, o.x, o.y, blockSize, blockSize, t.data(), blockSize,
                       blockSize, GDT_Int16, 0, 0);
        for (auto v : t) if (!p.hasNoData || v != p.nodata16)
          g = std::gcd(g, (uint32_t)((int32_t)v - p.minShift));
      }
      p.normGCDU16 = g > 0 ? (uint16_t)g : 1;
    }
  } else if (normalize) {
    uint16_t minU16 = std::numeric_limits<uint16_t>::max();
    for (auto& o : offs()) {
      std::vector<uint16_t> t(blockSize * blockSize);
      band->RasterIO(GF_Read, o.x, o.y, blockSize, blockSize, t.data(), blockSize,
                     blockSize, GDT_UInt16, 0, 0);
      for (auto v : t) if (!p.hasNoData || v != p.nodataU16) minU16 = std::min(minU16, v);
    }
    if (minU16 < std::numeric_limits<uint16_t>::max()) {
      p.normMinU16 = minU16;
      uint32_t g = 0;
      for (auto& o : offs()) {
        std::vector<uint16_t> t(blockSize * blockSize);
        band->RasterIO(GF_Read, o.x, o.y, blockSize, blockSize, t.data(), blockSize,
                       blockSize, GDT_UInt16, 0, 0);
        for (auto v : t) if (!p.hasNoData || v != p.nodataU16)
          g = std::gcd(g, (uint32_t)v - (uint32_t)p.normMinU16);
      }
      p.normGCDU16 = g > 0 ? (uint16_t)g : 1;
    }
  }
  return p;
}

using GridU16 = std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>;

// Persistent spin-coordinated pool over TWO aligned grids (same as the
// single-band RunStaticRepsU16 — pinning + schedutil-boost fix — but each block
// is a fused 2-band aggregate over the (A,B) pair instead of decode+transform).
static void RunStaticReps2Band(GridU16& gA, GridU16& gB, int blockSize, int op,
                               bool compressed, int numReps, int numSkip,
                               RunningStats& statsDec, RepMedian& medDec) {
  const size_t N = (size_t)blockSize * blockSize;
  const int X = std::max(1, gNumThreads);
  const std::size_t total = gA.size();
  const std::size_t per = total / (size_t)X;
  std::vector<RunningStats> tDec(X);
  std::vector<double> repDec(X, 0.0), repSink(X, 0.0);
  std::atomic<int> goRep{-1};
  std::atomic<int> doneCount{0};

  auto worker = [&](int t) {
    PinThreadToAllowedCpu(t);
    if (op == 8) ndvi2_set_count_threshold(gCountThreshold);  // thread_local coef
    const std::size_t lo = (size_t)t * per;
    const std::size_t hi = (t == X - 1) ? total : lo + per;
    for (int myRep = 0; myRep < numReps; myRep++) {
      while (goRep.load(std::memory_order_acquire) != myRep)
        __builtin_ia32_pause();
      const bool timed = (myRep >= numSkip);
      double rd = 0.0, sink = 0.0;
      for (std::size_t i = lo; i < hi; i++) {
        auto c0 = std::chrono::steady_clock::now();
        double r;
        if (compressed) {
          const uint8_t* a =
              static_cast<SimdCompFusedCodecU16*>(gA[i].get())->compressed.data();
          const uint8_t* b =
              static_cast<SimdCompFusedCodecU16*>(gB[i].get())->compressed.data();
          r = ndvi2_indep(a, b, N, op);
        } else {
          r = ndvi2_raw(gA[i]->GetEncoded().data(), gB[i]->GetEncoded().data(), N, op);
        }
        auto c1 = std::chrono::steady_clock::now();
        double dt =
            std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count();
        if (timed) tDec[t].Update((size_t)dt);
        rd += dt;
        sink += r;
      }
      repDec[t] = rd;
      repSink[t] = sink;
      doneCount.fetch_add(1, std::memory_order_release);
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(X);
  for (int t = 0; t < X; t++) pool.emplace_back(worker, t);
  for (int r = 0; r < numReps; r++) {
    doneCount.store(0, std::memory_order_release);
    goRep.store(r, std::memory_order_release);
    while (doneCount.load(std::memory_order_acquire) != X)
      __builtin_ia32_pause();
    if (r >= numSkip) {
      double td = 0.0, sk = 0.0;
      for (int t = 0; t < X; t++) { td += repDec[t]; sk += repSink[t]; }
      medDec.Push(td, total);
      g_ndvi_sink2 = sk;
    }
  }
  for (auto& th : pool) th.join();

  auto mergeStats = [](RunningStats& a, const RunningStats& b) {
    if (b.n == 0) return;
    if (a.n == 0) { a = b; return; }
    double na = a.n, nb = b.n, nt = na + nb, delta = b.mean - a.mean;
    a.M2 += b.M2 + delta * delta * na * nb / nt;
    a.mean += delta * nb / nt;
    a.n = (size_t)nt;
    a.min_val = std::min(a.min_val, b.min_val);
    a.max_val = std::max(a.max_val, b.max_val);
  };
  for (int t = 0; t < X; t++) mergeStats(statsDec, tDec[t]);
}

// Build a two-band PFor-FoR grid: reads both bands together, applies the same
// normalization as SplitIntoFullBlocksU16, encodes with shared-b per 256-block.
static Grid2BandPFor BuildGrid2BandPFor(
    GDALRasterBand* bandA, GDALRasterBand* bandB,
    int rasterWidth, int rasterHeight, int blockSize, int numBlocks,
    const BandU16Params& pA, const BandU16Params& pB, bool normalize) {
  const int blocksInWidth  = rasterWidth  / blockSize;
  const int blocksInHeight = rasterHeight / blockSize;
  const size_t N = (size_t)blockSize * blockSize;
  const size_t bound = p4nbound256v16_for2band(N);

  Grid2BandPFor grid;
  grid.reserve(numBlocks);

  auto readAndNorm = [&](GDALRasterBand* band, const BandU16Params& p,
                          int ox, int oy, std::vector<uint16_t>& out) {
    if (p.minShift < 0) {
      std::vector<int16_t> tmp(N);
      band->RasterIO(GF_Read, ox, oy, blockSize, blockSize,
                     tmp.data(), blockSize, blockSize, GDT_Int16, 0, 0);
      int32_t shift = -(int32_t)p.minShift;
      for (size_t i = 0; i < N; ++i)
        out[i] = (p.hasNoData && tmp[i] == p.nodata16)
                   ? 0u
                   : (uint16_t)((int32_t)tmp[i] + shift);
    } else {
      band->RasterIO(GF_Read, ox, oy, blockSize, blockSize,
                     out.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
      if (p.hasNoData)
        for (size_t i = 0; i < N; ++i)
          if (out[i] == p.nodataU16) out[i] = 0;
    }
    if (normalize)
      for (size_t i = 0; i < N; ++i)
        if (out[i])
          out[i] = (uint16_t)(((uint32_t)out[i] - p.normMinU16) / p.normGCDU16);
  };

  std::vector<uint16_t> blkA(N), blkB(N);
  std::vector<uint8_t> scrA(bound), scrB(bound);

  for (auto& off : SampleBlockOffsets(blocksInWidth, blocksInHeight,
                                       blockSize, numBlocks)) {
    readAndNorm(bandA, pA, off.x, off.y, blkA);
    readAndNorm(bandB, pB, off.x, off.y, blkB);

    Block2BandPFor blk;
    const size_t numAnchors = N / 256;  // one anchor per 256-elem sub-block
    blk.anchsA.resize(numAnchors);
    blk.anchsB.resize(numAnchors);
    size_t sA = 0, sB = 0;
    p4nenc256v16_for2band(blkA.data(), blkB.data(), N,
                           blk.anchsA.data(), blk.anchsB.data(),
                           scrA.data(), &sA, scrB.data(), &sB);
    blk.encA.assign(scrA.data(), scrA.data() + sA);
    blk.encB.assign(scrB.data(), scrB.data() + sB);
    grid.push_back(std::move(blk));
  }
  return grid;
}

// Spin-pool worker loop for Grid2BandPFor (mirrors RunStaticReps2Band).
static void RunStaticReps2BandPFor(Grid2BandPFor& grid, size_t N, int op,
                                    int numReps, int numSkip,
                                    RunningStats& statsDec, RepMedian& medDec) {
  const int X = std::max(1, gNumThreads);
  const size_t total = grid.size();
  const size_t per   = total / (size_t)X;
  std::vector<RunningStats> tDec(X);
  std::vector<double> repDec(X, 0.0), repSink(X, 0.0);
  std::atomic<int> goRep{-1};
  std::atomic<int> doneCount{0};

  auto worker = [&](int t) {
    PinThreadToAllowedCpu(t);
    if (op == 8) ndvi2_set_count_threshold(gCountThreshold);
    const size_t lo = (size_t)t * per;
    const size_t hi = (t == X - 1) ? total : lo + per;
    for (int myRep = 0; myRep < numReps; myRep++) {
      while (goRep.load(std::memory_order_acquire) != myRep)
        __builtin_ia32_pause();
      const bool timed = (myRep >= numSkip);
      double rd = 0.0, sink = 0.0;
      for (size_t i = lo; i < hi; i++) {
        const Block2BandPFor& blk = grid[i];
        auto c0 = std::chrono::steady_clock::now();
        double r = ndvi2_pfor_for_indep(
            blk.encA.data(), blk.anchsA.data(),
            blk.encB.data(), blk.anchsB.data(), N, op);
        auto c1 = std::chrono::steady_clock::now();
        double dt = std::chrono::duration_cast<std::chrono::nanoseconds>(c1 - c0).count();
        if (timed) tDec[t].Update((size_t)dt);
        rd += dt; sink += r;
      }
      repDec[t] = rd; repSink[t] = sink;
      doneCount.fetch_add(1, std::memory_order_release);
    }
  };

  std::vector<std::thread> pool;
  pool.reserve(X);
  for (int t = 0; t < X; t++) pool.emplace_back(worker, t);
  for (int r = 0; r < numReps; r++) {
    doneCount.store(0, std::memory_order_release);
    goRep.store(r, std::memory_order_release);
    while (doneCount.load(std::memory_order_acquire) != X)
      __builtin_ia32_pause();
    if (r >= numSkip) {
      double td = 0.0, sk = 0.0;
      for (int t = 0; t < X; t++) { td += repDec[t]; sk += repSink[t]; }
      medDec.Push(td, total);
      g_ndvi_sink2 = sk;
    }
  }
  for (auto& th : pool) th.join();

  auto mergeStats2 = [](RunningStats& a, const RunningStats& b) {
    if (b.n == 0) return;
    if (a.n == 0) { a = b; return; }
    double na = a.n, nb = b.n, nt = na + nb, delta = b.mean - a.mean;
    a.M2 += b.M2 + delta * delta * na * nb / nt;
    a.mean += delta * nb / nt;
    a.n = (size_t)nt;
    a.min_val = std::min(a.min_val, b.min_val);
    a.max_val = std::max(a.max_val, b.max_val);
  };
  for (int t = 0; t < X; t++) mergeStats2(statsDec, tDec[t]);
}

int main(int argc, char* argv[]) {
  CLI::App app{"Two-band fused NDVI decode benchmark (off bench_pipeline_multithread)"};

  std::string fileA, fileB, opStr = "div";
  std::vector<std::string> icodecNames = {"simdcomp_fused"};
  int blockSize{}, numBlocks{}, numReps{}, numSkip{0}, numThreads{1};
  bool normalize = false;

  app.add_option("file", fileA, "Band A (NIR) GeoTIFF")->required();
  app.add_option("--fileB", fileB, "Band B (RED) GeoTIFF")->required();
  app.add_option("--blocksize,-b", blockSize, "Block side length")->required();
  app.add_option("--numblocks,-n", numBlocks, "Blocks per thread")->required();
  app.add_option("--numreps,-r", numReps, "Reps")->required();
  app.add_option("--rs", numSkip, "Warm-up reps to skip");
  app.add_option("--icodec", icodecNames,
                 "simdcomp_fused (compressed) | custom_direct_access (uncompressed)");
  app.add_option("--op", opStr, "noop|add|div|rcp|rcpraw|ndvi_div|ndvi_rcp|ndvi_rcpraw|count");
  app.add_option("--threshold", gCountThreshold,
                 "NDVI threshold for --op count (default 0.3)");
  app.add_flag("--normalize", normalize,
               "Normalize blocks: subtract per-band min, divide by GCD");
  app.add_option("--threads,-X", numThreads,
                 "Parallel workers; each does -n block-pairs (grid = n*threads)");
  CLI11_PARSE(app, argc, argv);

  gNumThreads = std::max(1, numThreads);
  numBlocks *= gNumThreads;
  CaptureFullAffinity();
  PinMainBuilderCpu();
  const int op = (opStr == "noop") ? 0 : (opStr == "add") ? 1 : (opStr == "div") ? 2
                 : (opStr == "rcp") ? 3 : (opStr == "rcpraw") ? 4
                 : (opStr == "ndvi_div") ? 5 : (opStr == "ndvi_rcp") ? 6
                 : (opStr == "ndvi_rcpraw") ? 7 : 8;

  GDALAllRegister();
  GDALSetCacheMax(64 * 1024 * 1024);
  GDALDataset* dsA = (GDALDataset*)GDALOpen(fileA.c_str(), GA_ReadOnly);
  GDALDataset* dsB = (GDALDataset*)GDALOpen(fileB.c_str(), GA_ReadOnly);
  if (!dsA || !dsB) { std::cerr << "open failed\n"; return 1; }
  GDALRasterBand* bandA = dsA->GetRasterBand(1);
  GDALRasterBand* bandB = dsB->GetRasterBand(1);
  const int nX = bandA->GetXSize(), nY = bandA->GetYSize();
  if (bandB->GetXSize() != nX || bandB->GetYSize() != nY) {
    std::cerr << "band dimensions differ\n"; return 1;
  }

  BandU16Params pA = ScanBandU16(bandA, blockSize, numBlocks, normalize);
  BandU16Params pB = ScanBandU16(bandB, blockSize, numBlocks, normalize);

  // Check for pfor_for_2band before touching the codec pool (it's not in the pool).
  const std::string reqCodec = icodecNames.empty() ? "" : icodecNames[0];
  const bool isPForFor2Band  = (reqCodec == "pfor_for_2band");

  RunningStats statsDec;
  RepMedian medDec;
  double cr = 1.0;
  size_t numGridBlocks = 0;
  std::string codecName;

  if (isPForFor2Band) {
    codecName = "pfor_for_2band";
    const size_t N = (size_t)blockSize * blockSize;
    Grid2BandPFor grid = BuildGrid2BandPFor(bandA, bandB, nX, nY, blockSize,
                                             numBlocks, pA, pB, normalize);
    if (grid.empty()) { std::cerr << "empty grid\n"; return 1; }
    numGridBlocks = grid.size();
    // CR: (encA + encB + anchor arrays) / raw
    const double rawPerBlock = 2.0 * N * sizeof(uint16_t);
    double enc = 0;
    for (auto& blk : grid)
      enc += blk.encA.size() + blk.encB.size()
           + (blk.anchsA.size() + blk.anchsB.size()) * sizeof(uint16_t);
    cr = enc / (rawPerBlock * numGridBlocks);
    RunStaticReps2BandPFor(grid, N, op, numReps, numSkip, statsDec, medDec);
  } else {
    auto pool = BuildAllCodecsU16();
    auto sel = SelectCodecsByName(pool, icodecNames);
    if (sel.empty()) { std::cerr << "no codec matched --icodec\n"; return 1; }
    codecName = sel[0]->name();
    const bool compressed = (codecName != "custom_direct_access");

    GridU16 gA = SplitIntoFullBlocksU16(
        bandA, nX, nY, blockSize, numBlocks,
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(sel[0]->CloneFresh()),
        pA.minShift, pA.hasNoData, pA.nodata16, pA.nodataU16, normalize,
        pA.normMinU16, pA.normGCDU16);
    GridU16 gB = SplitIntoFullBlocksU16(
        bandB, nX, nY, blockSize, numBlocks,
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(sel[0]->CloneFresh()),
        pB.minShift, pB.hasNoData, pB.nodata16, pB.nodataU16, normalize,
        pB.normMinU16, pB.normGCDU16);
    if (gA.empty() || gB.empty()) { std::cerr << "empty grid\n"; return 1; }
    numGridBlocks = gA.size();

    if (compressed) {
      double enc = 0, raw = 0;
      for (size_t i = 0; i < gA.size(); i++) {
        enc += gA[i]->EncodedNumValues() * gA[i]->EncodedSizeValue() +
               gB[i]->EncodedNumValues() * gB[i]->EncodedSizeValue();
        raw += 2.0 * blockSize * blockSize * sizeof(uint16_t);
      }
      cr = enc / raw;
    }
    RunStaticReps2Band(gA, gB, blockSize, op, compressed, numReps, numSkip,
                       statsDec, medDec);
  }

  const double resultSink = g_ndvi_sink2;
  std::cout << std::format(
      "**NDVI 2BAND** fileA={} fileB={} op={} codec={} X={} n={} blocks={} "
      "normalize={} medtimedec:{:.1f} meantimedec:{:.1f} compratio:{:.4f} result:{:.1f}\n",
      fileA, fileB, opStr, codecName, gNumThreads, numBlocks / gNumThreads,
      numGridBlocks, normalize, medDec.Median(), statsDec.mean, cr, resultSink);

  GDALClose(dsA);
  GDALClose(dsB);
  return 0;
}

// ── (unused single-band orchestration retained from bench_pipeline_multithread)
namespace {
[[maybe_unused]] void unused_keep() {
  (void)&RunOneCombinationU16;
  (void)&RunAllBenchmarksU16;
  (void)&RunOneCombination;
  (void)&RunAllBenchmarks;
  (void)&BuildAllCodecsPipeline;
}
}  // namespace

int main_single_band_unused(int argc, char* argv[]) {
  CLI::App app{"Benchmark codec access-pattern performance on a GeoTIFF raster"};

  std::string filePath;
  int blockSize{}, numBlocks{}, numReps{}, numSkip{0}, numThreads{1};
  std::vector<std::string> initialCodecNames = {"all"};
  std::vector<std::string> accessCodecNames = {"all"};
  std::vector<std::string> orderings = {"default"};
  std::vector<std::string> initialTransformations = {"none"};
  std::vector<std::string> sampleAccessPatterns = {"linear"};
  std::vector<std::string> accessTransformations = {"linearXOR"};
  bool forceInt32 = false;
  bool traceSums = false;
  bool normalize = false;

  app.add_option("file", filePath, "GeoTIFF file path")->required();
  app.add_option("--blocksize,-b", blockSize, "Block side length in pixels")
      ->required();
  app.add_option("--numblocks,-n", numBlocks, "Number of blocks to sample")
      ->required();
  app.add_option("--numreps,-r", numReps, "Repetitions per combination")
      ->required();
  app.add_option("--rs", numSkip, "Warm-up reps to skip in statistics (default: 0)");
  app.add_option("--icodec", initialCodecNames,
                 "Initial codec name(s), or 'all'");
  app.add_option("--acodec", accessCodecNames,
                 "Access codec name(s), or 'all'");
  app.add_option("--ordering", orderings,
                 "Block ordering(s): default|zigzag|morton");
  app.add_option("--itrans", initialTransformations,
                 "Initial transformation(s): none|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");
  app.add_option("--pattern", sampleAccessPatterns,
                 "Access pattern(s): linear|random");
  app.add_option("--atrans", accessTransformations,
                 "Access transformation(s): linearXOR|linearSum|linearSumSimd|"
                 "linearSumFused|randomXOR|randomSum|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");
  app.add_flag("--force-int32", forceInt32, "Force int32 pipeline for any raster type");
  app.add_flag("--trace-sums", traceSums, "Print per-block sums for verification");
  app.add_flag("--normalize", normalize,
               "Normalize blocks: subtract global min, divide by global GCD");
  app.add_option("--threads,-X", numThreads,
                 "Parallel decode workers; each benchmarks -n blocks "
                 "(grid = n*threads). Default 1 = identical to bench_pipeline.");

  CLI11_PARSE(app, argc, argv);
  gTraceSums = traceSums;
  gNumThreads = std::max(1, numThreads);
  numBlocks  *= gNumThreads;  // grid + min/GCD scan cover all n*threads blocks
  CaptureFullAffinity();  // record the full --physcpubind set before any pinning
  PinMainBuilderCpu();    // build on the reserved last core -> cold-start workers

  srand(1);  // rand() is used in random access patterns; seed before benchmarking.

  GDALAllRegister();
  GDALSetCacheMax(64 * 1024 * 1024);  // 64 MB — prevents GDAL cache inflating RSS
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(filePath.c_str(), GA_ReadOnly));
  if (dataset == nullptr) {
    std::cerr << std::format("Failed to open file: {}", filePath) << '\n';
    return 1;
  }

  int nBands = dataset->GetRasterCount();
  for (int bandIdx = 1; bandIdx <= nBands; bandIdx++) {
  GDALRasterBand* band = dataset->GetRasterBand(bandIdx);
  int nXSize = band->GetXSize();
  int nYSize = band->GetYSize();
  GDALDataType dt = band->GetRasterDataType();

  if (!forceInt32 && (dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_Byte)) {
    // ── uint16 / int16 path ───────────────────────────────────────────────
    int hasNoData = 0;
    double rawNoData = band->GetNoDataValue(&hasNoData);
    // If the nodata value doesn't fit in the raster's data type, it can
    // never occur in the data — treat as "no nodata".
    if (hasNoData) {
      if (dt == GDT_Int16 &&
          (rawNoData < -32768.0 || rawNoData > 32767.0))
        hasNoData = 0;
      else if ((dt == GDT_UInt16 || dt == GDT_Byte) &&
               (rawNoData < 0.0 || rawNoData > 65535.0))
        hasNoData = 0;
    }
    int16_t nodata16 = hasNoData ? static_cast<int16_t>(rawNoData) : 0;
    uint16_t nodataU16 = hasNoData ? static_cast<uint16_t>(rawNoData) : 0;

    int16_t minShift = 0;
    uint16_t normMinU16 = 0;
    uint16_t normGCDU16 = 1;

    if (dt == GDT_Int16) {
      // Compute min excluding nodata
      int16_t min16 = std::numeric_limits<int16_t>::max();
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<int16_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_Int16, 0, 0);
        for (auto v : tmp)
          if (!hasNoData || v != nodata16)
            min16 = std::min(min16, v);
      }
      minShift = min16;
      // normMinU16 = 0: post-shift non-nodata values have min 0

      if (normalize && min16 < std::numeric_limits<int16_t>::max()) {
        uint32_t gcdAcc = 0;
        for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                                nYSize / blockSize, blockSize,
                                                numBlocks)) {
          std::vector<int16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         tmp.data(), blockSize, blockSize, GDT_Int16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodata16)
              gcdAcc = std::gcd(gcdAcc,
                  static_cast<uint32_t>(static_cast<int32_t>(v) - minShift));
        }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    } else if (normalize) {
      // UInt16: scan for normMin then normGCD
      uint16_t minU16 = std::numeric_limits<uint16_t>::max();
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<uint16_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
        for (auto v : tmp)
          if (!hasNoData || v != nodataU16)
            minU16 = std::min(minU16, v);
      }
      if (minU16 < std::numeric_limits<uint16_t>::max()) {
        normMinU16 = minU16;
        uint32_t gcdAcc = 0;
        for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                                nYSize / blockSize, blockSize,
                                                numBlocks)) {
          std::vector<uint16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         tmp.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodataU16)
              gcdAcc = std::gcd(gcdAcc,
                  static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16));
        }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    }

    auto allCodecsU16_initial = BuildAllCodecsU16();
    auto allCodecsU16_access = BuildAllCodecsU16();
    auto baseCodecs =
        SelectCodecsByName(allCodecsU16_initial, initialCodecNames);
    auto accessCodecs =
        SelectCodecsByName(allCodecsU16_access, accessCodecNames);

    RunAllBenchmarksU16(band, nXSize, nYSize, filePath.c_str(), blockSize,
                        numBlocks, numReps, numSkip, minShift, hasNoData != 0, nodata16,
                        nodataU16, baseCodecs, accessCodecs, orderings,
                        initialTransformations, accessTransformations,
                        sampleAccessPatterns, normalize, normMinU16, normGCDU16);
  } else {
    // ── int32 path (existing) ─────────────────────────────────────────────
    int hasNoData32 = 0;
    double rawNoData32 = band->GetNoDataValue(&hasNoData32);
    int32_t nodata32 = hasNoData32 ? static_cast<int32_t>(rawNoData32) : 0;

    int32_t min = std::numeric_limits<int32_t>::max();
    for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                            nYSize / blockSize, blockSize,
                                            numBlocks)) {
      std::vector<int32_t> tmp(blockSize * blockSize);
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     tmp.data(), blockSize, blockSize, GDT_Int32, 0, 0);
      for (auto v : tmp)
        if (!hasNoData32 || v != nodata32)
          min = std::min(min, v);
    }

    int32_t globalGCD = 1;
    if (normalize && min < std::numeric_limits<int32_t>::max()) {
      uint64_t gcdAcc = 0;
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<int32_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_Int32, 0, 0);
        for (auto v : tmp)
          if (!hasNoData32 || v != nodata32)
            gcdAcc = std::gcd(gcdAcc, static_cast<uint64_t>(
                                  static_cast<int64_t>(v) - static_cast<int64_t>(min)));
      }
      globalGCD = gcdAcc > 0 ? static_cast<int32_t>(gcdAcc) : 1;
    }

    auto allCodecs_initial = BuildAllCodecsPipeline();
    auto allCodecs_access = BuildAllCodecsPipeline();
    auto baseCodecs = SelectCodecsByName(allCodecs_initial, initialCodecNames);
    auto accessCodecs = SelectCodecsByName(allCodecs_access, accessCodecNames);

    RunAllBenchmarks(band, nXSize, nYSize, filePath.c_str(), blockSize,
                     numBlocks, numReps, numSkip, min, hasNoData32 != 0, nodata32,
                     baseCodecs, accessCodecs, orderings,
                     initialTransformations, accessTransformations,
                     sampleAccessPatterns, normalize, globalGCD);
  }

  } // bandIdx loop
  GDALClose(dataset);
  return 0;
}
