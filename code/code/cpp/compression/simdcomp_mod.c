void aggregate_sums(__m128i OutReg, __m128i* sum_lo, __m128i* sum_hi) {
    // Split the 32-bit integers in OutReg into two 64-bit integers and add to the running sums.
    __m128i OutReg_lo = _mm_unpacklo_epi32(OutReg, _mm_setzero_si128()); // Convert lower 2 integers to 64-bit
    __m128i OutReg_hi = _mm_unpackhi_epi32(OutReg, _mm_setzero_si128()); // Convert upper 2 integers to 64-bit

    *sum_lo = _mm_add_epi64(*sum_lo, OutReg_lo); // Add to lower sum
    *sum_hi = _mm_add_epi64(*sum_hi, OutReg_hi); // Add to upper sum
}



static void __SIMD_fastunpack1_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {
  __m128i *out = (__m128i *)(_out);
  __m128i InReg1 = _mm_loadu_si128(in);
  __m128i InReg2 = InReg1;
  __m128i OutReg1, OutReg2, OutReg3, OutReg4;
  const __m128i mask = _mm_set1_epi32(1);

  uint32_t i, shift = 0;

  for (i = 0; i < 8; ++i) {
    OutReg1 = _mm_and_si128(_mm_srli_epi32(InReg1, shift++), mask);
    OutReg2 = _mm_and_si128(_mm_srli_epi32(InReg2, shift++), mask);
    OutReg3 = _mm_and_si128(_mm_srli_epi32(InReg1, shift++), mask);
    OutReg4 = _mm_and_si128(_mm_srli_epi32(InReg2, shift++), mask);
    _mm_storeu_si128(out++, OutReg1);
    aggregate_sums(OutReg1, sum_lo, sum_hi);
    _mm_storeu_si128(out++, OutReg2);
    aggregate_sums(OutReg2, sum_lo, sum_hi);
    _mm_storeu_si128(out++, OutReg3);
    aggregate_sums(OutReg3, sum_lo, sum_hi);
    _mm_storeu_si128(out++, OutReg4);
    aggregate_sums(OutReg4, sum_lo, sum_hi);
  }
}

static void __SIMD_fastunpack2_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 2) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 26), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 28), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 26), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 28), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack3_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 3) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 21), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 27), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 3 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 19), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 25), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 28), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 3 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 23), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 26), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack4_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 4) - 1);

  __m128i sum = _mm_setzero_si128();

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack5_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 5) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 25), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 5 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 23), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 5 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 21), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 26), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 5 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 19), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 5 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack6_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 6) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 6 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 6 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 6 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 6 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack7_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 7) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 21), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 24), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 23), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 19), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 7 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack8_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 8) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack9_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 9) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 22), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 21), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 19), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 9 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack10_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 10) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 10 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack11_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 11) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 19), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 20), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 11 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack12_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 12) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 12 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack13_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 13) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 17), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 18), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 13 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack14_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 14) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 14 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack15_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 15) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 15), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 16), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 15 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack16_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 16) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack17_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 17) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 14), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 13), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 17 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack18_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 18) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 18 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack19_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 19) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 12), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 11), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 19 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack20_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 20) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 20 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack21_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 21) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 10), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 9), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 21 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack22_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 22) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 22 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack23_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 23) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 7), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 21), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 8), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 23 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 9);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack24_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 24) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 24 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack25_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 25) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 5), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 23), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 9);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 6), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 21), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 25 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 7);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack26_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 26) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 26 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack27_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 27) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 7);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 9);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 4), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 26), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 21), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 23), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 3), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 25), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 27 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 5);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack28_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 28) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 28 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack29_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 29) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 26), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 23), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 5);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 2), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 28), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 25), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 7);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(_mm_srli_epi32(InReg, 1), mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 27), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 21), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 9);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 29 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 3);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack30_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 30) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 28), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 26), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 2);
  InReg = _mm_loadu_si128(++in);

  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 28), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 26), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 30 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 2);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

static void __SIMD_fastunpack31_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {

  __m128i *out = (__m128i *)(_out);
  __m128i InReg = _mm_loadu_si128(in);
  __m128i OutReg;
  const __m128i mask = _mm_set1_epi32((1U << 31) - 1);

  OutReg = _mm_and_si128(InReg, mask);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 31);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 30), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 30);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 29), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 29);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 28), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 28);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 27), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 27);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 26), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 26);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 25), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 25);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 24), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 24);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 23), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 23);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 22), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 22);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 21), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 21);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 20), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 20);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 19), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 19);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 18), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 18);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 17), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 17);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 16), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 16);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 15), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 15);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 14), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 14);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 13), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 13);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 12), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 12);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 11), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 11);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 10), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 10);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 9), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 9);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 8), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 8);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 7), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 7);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 6), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 6);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 5), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 5);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 4), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 4);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 3), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 3);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 2), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 2);
  InReg = _mm_loadu_si128(++in);

  OutReg =
      _mm_or_si128(OutReg, _mm_and_si128(_mm_slli_epi32(InReg, 31 - 1), mask));
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);

  OutReg = _mm_srli_epi32(InReg, 1);
  _mm_storeu_si128(out++, OutReg);
    aggregate_sums(OutReg, sum_lo, sum_hi);
}

void __SIMD_fastunpack32_32(const __m128i *in, uint32_t *_out, __m128i* sum_lo, __m128i* sum_hi) {
  __m128i *out = (__m128i *)(_out);
  uint32_t outer;

  for (outer = 0; outer < 32; ++outer) {
    _mm_storeu_si128(out++, _mm_loadu_si128(in++));
    aggregate_sums(_mm_loadu_si128(in++), sum_lo, sum_hi);
  }
}