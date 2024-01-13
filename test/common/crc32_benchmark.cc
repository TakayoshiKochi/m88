#include <benchmark/benchmark.h>

#include "common/crc32.h"

namespace {
constexpr int kSize = 1024 * 1024 * 32;

uint32_t CRC32_Naive(const uint8_t* data, size_t size) {
  uint32_t crc = ~0;
  for (int i = 0; i < size; ++i) {
    crc ^= data[i] << 24;
    for (int j = 0; j < 8; ++j) {
      if (crc & 0x80000000) {
        crc = (crc << 1) ^ CRC32::crc32_poly;
      } else {
        crc <<= 1;
      }
    }
  }
  return ~crc;
}
} // namespace

static void BM_CRC32_Naive(benchmark::State& state) {
  static uint8_t buf[kSize];
  for (int i = 0; i < kSize; ++i) {
    buf[i] = i % 256;
  }

  uint32_t x = 0;
  for (auto _ : state) {
    // This code gets timed
    x = CRC32_Naive(buf, kSize);
  }
}

static void BM_CRC32_Fast(benchmark::State& state) {
  static uint8_t buf[kSize];
  for (int i = 0; i < kSize; ++i) {
    buf[i] = i % 256;
  }

  uint32_t x = 0;
  for (auto _ : state) {
    // This code gets timed
    x = CRC32::Calc(buf, kSize);
  }
}

// Register the function as a benchmark
BENCHMARK(BM_CRC32_Naive);
BENCHMARK(BM_CRC32_Fast);
// Run the benchmark
BENCHMARK_MAIN();