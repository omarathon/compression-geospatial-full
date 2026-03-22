#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <nmmintrin.h>  // SSE4.2 (includes SSSE3 for _mm_hadd_epi32)
#include <format>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "remappings.h"
#include "transformations.h"
#include "util.h"


enum class Ordering { RowMajor, Zigzag, Morton };

enum class Transformation {
  None,
  Threshold,
  SmoothAndShift,
  IndexBasedClassification,
  ValueBasedClassification,
  ValueShift
};

enum class AccessPattern { Linear, Random };

enum class AccessTransformation {
  LinearXOR,
  LinearSum,
  LinearSumSimd,   // SSE SIMD vectorised sum — fair baseline matching FastPFor ISA
  LinearSumFused,  // reads pre-computed 32-bit sum from codec overflow slot
  RandomXOR,
  RandomSum,
  Threshold,
  SmoothAndShift,
  IndexBasedClassification,
  ValueBasedClassification,
  ValueShift
};


inline Ordering ParseOrdering(const std::string& s) {
  if (s == "zigzag") return Ordering::Zigzag;
  if (s == "morton") return Ordering::Morton;
  if (s.empty() || s == "default") return Ordering::RowMajor;
  throw std::invalid_argument("Unknown ordering: " + s);
}

inline Transformation ParseTransformation(const std::string& s) {
  if (s.empty() || s == "none" || s == "default") return Transformation::None;
  if (s == "Threshold") return Transformation::Threshold;
  if (s == "SmoothAndShift") return Transformation::SmoothAndShift;
  if (s == "IndexBasedClassification")
    return Transformation::IndexBasedClassification;
  if (s == "ValueBasedClassification")
    return Transformation::ValueBasedClassification;
  if (s == "ValueShift") return Transformation::ValueShift;
  throw std::invalid_argument("Unknown transformation: " + s);
}

inline AccessPattern ParseAccessPattern(const std::string& s) {
  if (s == "random") return AccessPattern::Random;
  if (s.empty() || s == "default" || s == "linear") return AccessPattern::Linear;
  throw std::invalid_argument("Unknown access pattern: " + s);
}

inline AccessTransformation ParseAccessTransformation(const std::string& s) {
  if (s.empty() || s == "default" || s == "linearXOR")
    return AccessTransformation::LinearXOR;
  if (s == "linearSum") return AccessTransformation::LinearSum;
  if (s == "linearSumSimd") return AccessTransformation::LinearSumSimd;
  if (s == "linearSumFused") return AccessTransformation::LinearSumFused;
  if (s == "randomXOR") return AccessTransformation::RandomXOR;
  if (s == "randomSum") return AccessTransformation::RandomSum;
  if (s == "Threshold") return AccessTransformation::Threshold;
  if (s == "SmoothAndShift") return AccessTransformation::SmoothAndShift;
  if (s == "IndexBasedClassification")
    return AccessTransformation::IndexBasedClassification;
  if (s == "ValueBasedClassification")
    return AccessTransformation::ValueBasedClassification;
  if (s == "ValueShift") return AccessTransformation::ValueShift;
  throw std::invalid_argument("Unknown access transformation: " + s);
}


inline std::string ToString(Ordering o) {
  switch (o) {
    case Ordering::RowMajor:
      return "default";
    case Ordering::Zigzag:
      return "zigzag";
    case Ordering::Morton:
      return "morton";
  }
  return "";
}

inline std::string ToString(Transformation t) {
  switch (t) {
    case Transformation::None:
      return "none";
    case Transformation::Threshold:
      return "Threshold";
    case Transformation::SmoothAndShift:
      return "SmoothAndShift";
    case Transformation::IndexBasedClassification:
      return "IndexBasedClassification";
    case Transformation::ValueBasedClassification:
      return "ValueBasedClassification";
    case Transformation::ValueShift:
      return "ValueShift";
  }
  return "";
}

inline std::string ToString(AccessPattern p) {
  switch (p) {
    case AccessPattern::Linear:
      return "linear";
    case AccessPattern::Random:
      return "random";
  }
  return "";
}

inline std::string ToString(AccessTransformation t) {
  switch (t) {
    case AccessTransformation::LinearXOR:
      return "linearXOR";
    case AccessTransformation::LinearSum:
      return "linearSum";
    case AccessTransformation::LinearSumSimd:
      return "linearSumSimd";
    case AccessTransformation::LinearSumFused:
      return "linearSumFused";
    case AccessTransformation::RandomXOR:
      return "randomXOR";
    case AccessTransformation::RandomSum:
      return "randomSum";
    case AccessTransformation::Threshold:
      return "Threshold";
    case AccessTransformation::SmoothAndShift:
      return "SmoothAndShift";
    case AccessTransformation::IndexBasedClassification:
      return "IndexBasedClassification";
    case AccessTransformation::ValueBasedClassification:
      return "ValueBasedClassification";
    case AccessTransformation::ValueShift:
      return "ValueShift";
  }
  return "";
}


// Primary template: no-op for unsupported element types.
template <typename T>
void ApplyOrdering(std::vector<T>& /*data*/, Ordering /*o*/,
                   int /*blockSize*/) {}

template <>
inline void ApplyOrdering<int32_t>(std::vector<int32_t>& data, Ordering o,
                                    int blockSize) {
  if (o == Ordering::Zigzag) {
    auto remapped = RemapToZigzagOrder(data, blockSize);
    std::copy(remapped.begin(), remapped.end(), data.begin());
  } else if (o == Ordering::Morton) {
    auto remapped = RemapToMortonOrder(data, blockSize);
    std::copy(remapped.begin(), remapped.end(), data.begin());
  }
}


// Primary template: no-op for unsupported element types.
template <typename T>
void ApplyTransformation(std::vector<T>& /*data*/, Transformation /*t*/) {}

template <>
inline void ApplyTransformation<int32_t>(std::vector<int32_t>& data,
                                          Transformation t) {
  switch (t) {
    case Transformation::Threshold:
      Threshold(data, Avg(data));
      break;
    case Transformation::SmoothAndShift:
      SmoothAndShift(data);
      break;
    case Transformation::IndexBasedClassification:
      IndexBasedClassification(data, /* max_classes */ 8);
      break;
    case Transformation::ValueBasedClassification:
      ValueBasedClassification(data, /* num_classes */ 8);
      break;
    case Transformation::ValueShift:
      ValueShift(data, static_cast<int32_t>(std::pow(2, 23)));
      break;
    case Transformation::None:
      break;
  }
}

// Convenience: apply ordering then transformation in-place.
template <typename T>
void RemapAndTransform(std::vector<T>& data, Ordering o, Transformation t,
                       int blockSize) {
  ApplyOrdering(data, o, blockSize);
  ApplyTransformation(data, t);
}


// Sink for SIMD/fused sum results — file-scope prevents dead-code elimination.
inline int32_t kLinearSumSink = 0;

// Returns true for variants that mutate the block data (requiring re-encoding).
inline bool AccessTransformationMutatesData(AccessTransformation t) {
  switch (t) {
    case AccessTransformation::Threshold:
    case AccessTransformation::SmoothAndShift:
    case AccessTransformation::IndexBasedClassification:
    case AccessTransformation::ValueBasedClassification:
    case AccessTransformation::ValueShift:
      return true;
    default:
      return false;
  }
}

// Primary template: no-op for unsupported element types; returns 0 ns.
template <typename T>
std::size_t ApplyAccessTransformation(std::vector<T>& /*data*/,
                                       AccessTransformation /*t*/,
                                       std::size_t /*blockSize*/) {
  return 0;
}

template <>
inline std::size_t ApplyAccessTransformation<int32_t>(
    std::vector<int32_t>& data, AccessTransformation t,
    std::size_t blockSize) {
  auto startRead = std::chrono::steady_clock::now();
  switch (t) {
    case AccessTransformation::Threshold:
      Threshold(data, Avg(data));
      break;
    case AccessTransformation::SmoothAndShift:
      SmoothAndShift(data);
      break;
    case AccessTransformation::IndexBasedClassification:
      IndexBasedClassification(data, /* max_classes */ 8);
      break;
    case AccessTransformation::ValueBasedClassification:
      ValueBasedClassification(data, /* num_classes */ 8);
      break;
    case AccessTransformation::ValueShift:
      ValueShift(data, static_cast<int32_t>(std::pow(2, 23)));
      break;
    case AccessTransformation::LinearSum: {
      volatile int64_t dummy = 0;  // volatile prevents auto-vectorisation
      for (std::size_t bi = 0; bi < blockSize * blockSize; bi++) {
        dummy += data[bi];
      }
      break;
    }
    case AccessTransformation::LinearSumSimd: {
      // Explicit SSE SIMD sum — same ISA FastPFor is allowed to use.
      int total = static_cast<int>(blockSize * blockSize);
      int i = 0;
      __m128i vsum = _mm_setzero_si128();
      for (; i + 4 <= total; i += 4) {
        __m128i v = _mm_loadu_si128((const __m128i*)&data[i]);
        vsum = _mm_add_epi32(vsum, v);
      }
      vsum = _mm_hadd_epi32(vsum, vsum);
      vsum = _mm_hadd_epi32(vsum, vsum);
      kLinearSumSink = _mm_cvtsi128_si32(vsum);
      for (; i < total; ++i)
        kLinearSumSink += data[i];
      break;
    }
    case AccessTransformation::LinearSumFused: {
      // Reads 32-bit sum written by the codec into the overflow slot.
      kLinearSumSink = data[blockSize * blockSize];
      break;
    }
    case AccessTransformation::RandomXOR: {
      volatile int32_t dummy = 0;
      std::vector<int> bis(blockSize * blockSize);
      for (std::size_t iti = 0; iti < blockSize * blockSize; iti++) {
        bis[iti] = rand() % static_cast<int>(blockSize * blockSize);
      }
      startRead = std::chrono::steady_clock::now();
      for (std::size_t iti = 0; iti < blockSize * blockSize; iti++) {
        dummy ^= data[bis[iti]];
      }
      break;
    }
    case AccessTransformation::RandomSum: {
      volatile int64_t dummy = 0;
      std::vector<int> bis(blockSize * blockSize);
      for (std::size_t iti = 0; iti < blockSize * blockSize; iti++) {
        bis[iti] = rand() % static_cast<int>(blockSize * blockSize);
      }
      startRead = std::chrono::steady_clock::now();
      for (std::size_t iti = 0; iti < blockSize * blockSize; iti++) {
        dummy += data[bis[iti]];
      }
      break;
    }
    default:  // LinearXOR
    {
      volatile int32_t dummy = 0;
      for (std::size_t bi = 0; bi < blockSize * blockSize; bi++) {
        dummy ^= data[bi];
      }
      break;
    }
  }
  auto endRead = std::chrono::steady_clock::now();
  return static_cast<std::size_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(endRead - startRead)
          .count());
}


struct RunningStats {
  std::size_t n = 0;
  double mean   = 0.0;
  double M2     = 0.0;

  void Update(std::size_t x) {
    ++n;
    double delta = static_cast<double>(x) - mean;
    mean += delta / static_cast<double>(n);
    M2   += delta * (static_cast<double>(x) - mean);
  }
  double Variance() const { return n > 1 ? M2 / static_cast<double>(n) : 0.0; }
  double Total()    const { return mean * static_cast<double>(n); }
};


struct CodecStats {
  float cf = 0;    // compression factor (encoded bytes / original bytes)
  float bpi = 0;   // encoded bytes per element
  float tenc = 0;  // encode time (ns)
  float tdec = 0;  // decode time (ns)
};


// Encodes, decodes, and verifies round-trip correctness. Returns zeroed stats
// on error (details printed to cerr/cout). Resets codec via CloneFresh.
template <typename T>
CodecStats BenchmarkOneCodec(std::vector<T>& data,
                              std::unique_ptr<StatefulIntegerCodec<T>>& codec) {
  CodecStats stats;
  codec->AllocEncoded(data.data(), data.size());
  auto startEncode = std::chrono::steady_clock::now();
  try {
    codec->EncodeArray(data.data(), data.size());
  } catch (const std::exception& e) {
    std::cout << " ERROR see cerr\n";
    std::cerr << std::format("error encoding {}: {}", codec->name(), e.what()) << '\n';
    return stats;
  }
  auto endEncode = std::chrono::steady_clock::now();

  std::size_t numCodedValues = codec->EncodedNumValues();
  std::size_t sizeCodedValue = codec->EncodedSizeValue();

  std::vector<T> dataBack(data.size() + codec->GetOverflowSize(data.size()));
  auto startDecode = std::chrono::steady_clock::now();
  try {
    codec->DecodeArray(dataBack.data(), data.size());
  } catch (const std::exception& e) {
    std::cout << " ERROR see cerr\n";
    std::cerr << std::format("error decoding {}: {}", codec->name(), e.what()) << '\n';
    return stats;
  }
  auto endDecode = std::chrono::steady_clock::now();

  for (std::size_t i = 0; i < data.size(); i++) {
    if (data[i] != dataBack[i]) {
      std::cout << " ERROR see cerr\n";
      std::cerr << std::format("in!=out {}(i={}:o{}b{},len={})",
                   codec->name(), i, data[i], dataBack[i], data.size()) << '\n';
      return stats;
    }
  }

  stats.cf = static_cast<float>(numCodedValues * sizeCodedValue) /
             static_cast<float>(data.size() * sizeof(T));
  stats.bpi = static_cast<float>(numCodedValues * sizeCodedValue) /
              static_cast<float>(data.size());
  stats.tenc = static_cast<float>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode -
                                                           startEncode)
          .count());
  stats.tdec = static_cast<float>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode -
                                                           startDecode)
          .count());

  codec = std::unique_ptr<StatefulIntegerCodec<T>>(codec->CloneFresh());
  return stats;
}


// Returns clones of codecs in `pool` whose name is in `names`.
// If names == {"all"} or {"*"}, all codecs are cloned.
template <typename T>
std::vector<std::unique_ptr<StatefulIntegerCodec<T>>> SelectCodecsByName(
    std::vector<std::unique_ptr<StatefulIntegerCodec<T>>>& pool,
    const std::vector<std::string>& names) {
  std::vector<std::unique_ptr<StatefulIntegerCodec<T>>> selected;
  bool selectAll =
      names.size() == 1 && (names[0] == "all" || names[0] == "*");
  for (auto& codec : pool) {
    if (selectAll) {
      selected.emplace_back(codec->CloneFresh());
      continue;
    }
    for (const auto& name : names) {
      if (name == codec->name()) {
        selected.emplace_back(codec->CloneFresh());
        break;
      }
    }
  }
  return selected;
}


struct BlockOffset {
  int x;  // pixel x offset (block column * blockSize)
  int y;  // pixel y offset (block row * blockSize)
};

// Returns numBlocks evenly-spaced block pixel offsets across the raster grid.
inline std::vector<BlockOffset> SampleBlockOffsets(int blocksInWidth,
                                                    int blocksInHeight,
                                                    int blockSize,
                                                    int numBlocks) {
  std::vector<BlockOffset> offsets;
  offsets.reserve(numBlocks);
  int totalBlocks = blocksInWidth * blocksInHeight;
  int sampleInterval = std::max(1, totalBlocks / numBlocks);
  for (int blockNum = 0, sampled = 0; sampled < numBlocks;
       blockNum += sampleInterval, sampled++) {
    int blockIndex = blockNum % totalBlocks;
    offsets.push_back({(blockIndex % blocksInWidth) * blockSize,
                       (blockIndex / blocksInWidth) * blockSize});
  }
  return offsets;
}
