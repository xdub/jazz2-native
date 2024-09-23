#include "StringUtils.h"
#include "GrowableArray.h"
#include "../Asserts.h"
#include "../Cpu.h"

#if defined(DEATH_ENABLE_AVX2) || defined(DEATH_ENABLE_BMI1)
#	include "../IntrinsicsAvx.h" /* TZCNT is in AVX headers :( */
#elif defined(DEATH_ENABLE_SSE41)
#	include "../IntrinsicsSse4.h"
#elif defined(DEATH_ENABLE_SSE2)
#	include "../IntrinsicsSse2.h"
#endif
#if defined(DEATH_ENABLE_SIMD128)
#	include <wasm_simd128.h>
#endif

namespace Death { namespace Utf8 {
//###==##====#=====--==~--~=~- --- -- -  -  -   -

	// Forward declarations for the Death::Utf8 namespace
	std::pair<char32_t, std::size_t> NextChar(Containers::ArrayView<const char> text, std::size_t cursor);

}}

namespace Death { namespace Containers { namespace StringUtils {
//###==##====#=====--==~--~=~- --- -- -  -  -   -

	namespace Implementation
	{
		namespace
		{
			// Basically a variant of the stringFindCharacterImplementation(), using the same high-level logic with branching only on every four vectors.
			// See its documentation for more information.
#if defined(DEATH_ENABLE_SSE2) && defined(DEATH_ENABLE_BMI1)
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE(SSE2, BMI1) typename std::decay<decltype(commonPrefix)>::type commonPrefixImplementation(DEATH_CPU_DECLARE(Cpu::Sse2 | Cpu::Bmi1)) {
				return [](const char* const a, const char* const b, const std::size_t sizeA, const std::size_t sizeB) DEATH_ENABLE(SSE2, BMI1) {
					const std::size_t size = std::min(sizeA, sizeB);

					// If we have less than 16 bytes, do it the stupid way
					{
						const char* i = a, * j = b;
						switch (size) {
							case 15: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case 14: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case 13: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case 12: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case 11: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case 10: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  9: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  8: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  7: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  6: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  5: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  4: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  3: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  2: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  1: if (*i++ != *j++) return i - 1; DEATH_FALLTHROUGH
							case  0: return a + size;
						}
					}

					// Unconditionally compare the first vector a slower, unaligned way
					{
						const __m128i chunkA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(a));
						const __m128i chunkB = _mm_loadu_si128(reinterpret_cast<const __m128i*>(b));
						const int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffff)
							return a + _tzcnt_u32(~mask);
					}

					// Go to the next aligned position of *one* of the inputs. If the pointer was already aligned,
					// we'll go to the next aligned vector; if not, there will be an overlap and we'll check some bytes twice.
					// The other input is then processed unaligned, or if we're lucky it's aligned the same way (such
					// as when the strings are at the start of a default-aligned allocation, which on 64 bits is 16 bytes).
					// Alternatively we could try to load both in an aligned way and then compare shifted values,
					// but on recent architecture the extra overhead from the patching would probably be larger
					// than just reading unaligned.
					const char* i = reinterpret_cast<const char*>(reinterpret_cast<std::uintptr_t>(a + 16) & ~0xf);
					const char* j = b + (i - a);
					DEATH_DEBUG_ASSERT(i > a && j > b && reinterpret_cast<std::uintptr_t>(i) % 16 == 0);

					// Go four vectors at a time with the aligned pointer
					const char* const endA = a + size;
					const char* const endB = b + size;
					for (; i + 4 * 16 <= endA; i += 4 * 16, j += 4 * 16) {
						const __m128i iA = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 0);
						const __m128i iB = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 1);
						const __m128i iC = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 2);
						const __m128i iD = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 3);
						// The second input is loaded unaligned always
						const __m128i jA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j) + 0);
						const __m128i jB = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j) + 1);
						const __m128i jC = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j) + 2);
						const __m128i jD = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j) + 3);

						const __m128i eqA = _mm_cmpeq_epi8(iA, jA);
						const __m128i eqB = _mm_cmpeq_epi8(iB, jB);
						const __m128i eqC = _mm_cmpeq_epi8(iC, jC);
						const __m128i eqD = _mm_cmpeq_epi8(iD, jD);

						const __m128i and1 = _mm_and_si128(eqA, eqB);
						const __m128i and2 = _mm_and_si128(eqC, eqD);
						const __m128i and3 = _mm_and_si128(and1, and2);
						if (_mm_movemask_epi8(and3) != 0xffff) {
							const int maskA = _mm_movemask_epi8(eqA);
							if (maskA != 0xffff)
								return i + 0 * 16 + _tzcnt_u32(~maskA);
							const int maskB = _mm_movemask_epi8(eqB);
							if (maskB != 0xffff)
								return i + 1 * 16 + _tzcnt_u32(~maskB);
							const int maskC = _mm_movemask_epi8(eqC);
							if (maskC != 0xffff)
								return i + 2 * 16 + _tzcnt_u32(~maskC);
							const int maskD = _mm_movemask_epi8(eqD);
							if (maskD != 0xffff)
								return i + 3 * 16 + _tzcnt_u32(~maskD);
							// Unreachable
						}
					}

					// Handle remaining less than four aligned vectors
					for (; i + 16 <= endA; i += 16, j += 16) {
						const __m128i chunkA = _mm_load_si128(reinterpret_cast<const __m128i*>(i));
						// The second input is loaded unaligned always
						const __m128i chunkB = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j));
						const int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffff)
							return i + _tzcnt_u32(~mask);
					}

					// Handle remaining less than a vector with an unaligned load, again overlapping back with the previous already-compared elements
					if (i < endA) {
						DEATH_DEBUG_ASSERT(i + 16 > endA && endB - j == endA - i);
						i = endA - 16;
						j = endB - 16;
						const __m128i chunkA = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
						const __m128i chunkB = _mm_loadu_si128(reinterpret_cast<const __m128i*>(j));
						const int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffff)
							return i + _tzcnt_u32(~mask);
					}

					return endA;
				};
			}
#endif

#if defined(DEATH_ENABLE_AVX2) && defined(DEATH_ENABLE_BMI1)
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE(AVX2, BMI1) typename std::decay<decltype(commonPrefix)>::type commonPrefixImplementation(DEATH_CPU_DECLARE(Cpu::Avx2 | Cpu::Bmi1)) {
				return [](const char* const a, const char* const b, const std::size_t sizeA, const std::size_t sizeB) DEATH_ENABLE(AVX2, BMI1) {
					const std::size_t size = std::min(sizeA, sizeB);

					// If we have less than 32 bytes, fall back to the SSE variant
					if (size < 32)
						return commonPrefixImplementation(DEATH_CPU_SELECT(Cpu::Sse2 | Cpu::Bmi1))(a, b, sizeA, sizeB);

					// Unconditionally compare the first vector a slower, unaligned way
					{
						// _mm256_lddqu_si256 is just an alias to _mm256_loadu_si256, no reason to use it: https://stackoverflow.com/a/47426790
						const __m256i chunkA = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(a));
						const __m256i chunkB = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(b));
						const std::uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffffffffu)
							return a + _tzcnt_u32(~mask);
					}

					// Go to the next aligned position of *one* of the inputs. If the pointer was already aligned,
					// we'll go to the next aligned vector; if not, there will be an overlap and we'll check some bytes twice.
					// Second input is treated as unaligned always, see the SSE2 variant for explanation.
					const char* i = reinterpret_cast<const char*>(reinterpret_cast<std::uintptr_t>(a + 32) & ~0x1f);
					const char* j = b + (i - a);
					DEATH_DEBUG_ASSERT(i > a && j > b && reinterpret_cast<std::uintptr_t>(i) % 32 == 0);

					// Go four vectors at a time with the aligned pointer
					const char* const endA = a + size;
					const char* const endB = b + size;
					for (; i + 4 * 32 <= endA; i += 4 * 32, j += 4 * 32) {
						const __m256i iA = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 0);
						const __m256i iB = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 1);
						const __m256i iC = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 2);
						const __m256i iD = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 3);
						// The second input is loaded unaligned always
						const __m256i jA = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j) + 0);
						const __m256i jB = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j) + 1);
						const __m256i jC = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j) + 2);
						const __m256i jD = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j) + 3);

						const __m256i eqA = _mm256_cmpeq_epi8(iA, jA);
						const __m256i eqB = _mm256_cmpeq_epi8(iB, jB);
						const __m256i eqC = _mm256_cmpeq_epi8(iC, jC);
						const __m256i eqD = _mm256_cmpeq_epi8(iD, jD);

						const __m256i and1 = _mm256_and_si256(eqA, eqB);
						const __m256i and2 = _mm256_and_si256(eqC, eqD);
						const __m256i and3 = _mm256_and_si256(and1, and2);
						if (std::uint32_t(_mm256_movemask_epi8(and3)) != 0xffffffffu) {
							const std::uint32_t maskA = _mm256_movemask_epi8(eqA);
							if (maskA != 0xffffffffu)
								return i + 0 * 32 + _tzcnt_u32(~maskA);
							const std::uint32_t maskB = _mm256_movemask_epi8(eqB);
							if (maskB != 0xffffffffu)
								return i + 1 * 32 + _tzcnt_u32(~maskB);
							const std::uint32_t maskC = _mm256_movemask_epi8(eqC);
							if (maskC != 0xffffffffu)
								return i + 2 * 32 + _tzcnt_u32(~maskC);
							const std::uint32_t maskD = _mm256_movemask_epi8(eqD);
							if (maskD != 0xffffffffu)
								return i + 3 * 32 + _tzcnt_u32(~maskD);
							// Unreachable
						}
					}

					// Handle remaining less than four aligned vectors
					for (; i + 32 <= endA; i += 32, j += 32) {
						const __m256i chunkA = _mm256_load_si256(reinterpret_cast<const __m256i*>(i));
						// The second input is loaded unaligned always
						const __m256i chunkB = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j));
						const std::uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffffffffu)
							return i + _tzcnt_u32(~mask);
					}

					// Handle remaining less than a vector with an unaligned load, again overlapping back with the previous already-compared elements
					if (i < endA) {
						DEATH_DEBUG_ASSERT(i + 32 > endA && endB - j == endA - i);
						i = endA - 32;
						j = endB - 32;
						const __m256i chunkA = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(i));
						const __m256i chunkB = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(j));
						const std::uint32_t mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(chunkA, chunkB));
						if (mask != 0xffffffffu)
							return i + _tzcnt_u32(~mask);
					}

					return endA;
				};
			}
#endif

			DEATH_CPU_MAYBE_UNUSED typename std::decay<decltype(commonPrefix)>::type commonPrefixImplementation(DEATH_CPU_DECLARE(Cpu::Scalar)) {
				return [](const char* const a, const char* const b, const std::size_t sizeA, const std::size_t sizeB) {
					const std::size_t size = std::min(sizeA, sizeB);
					const char* const endA = a + size;
					for (const char* i = a, *j = b; i != endA; ++i, ++j)
						if (*i != *j) return i;
					return endA;
				};
			}
		}

#if defined(DEATH_TARGET_X86)
		DEATH_CPU_DISPATCHER(commonPrefixImplementation, Cpu::Bmi1)
#else
		DEATH_CPU_DISPATCHER(commonPrefixImplementation)
#endif
		DEATH_CPU_DISPATCHED(commonPrefixImplementation, const char* DEATH_CPU_DISPATCHED_DECLARATION(commonPrefix)(const char* a, const char* b, std::size_t sizeA, std::size_t sizeB))({
			return commonPrefixImplementation(DEATH_CPU_SELECT(Cpu::Default))(a, b, sizeA, sizeB);
		})

		namespace
		{
			DEATH_CPU_MAYBE_UNUSED typename std::decay<decltype(lowercaseInPlace)>::type lowercaseInPlaceImplementation(Cpu::ScalarT) {
				return [](char* data, const std::size_t size) {
					// A proper Unicode-aware *and* locale-aware solution would involve far more than iterating over bytes

					// Branchless idea from https://stackoverflow.com/a/3884737, what it does is adding (1 << 5) for 'A'
					// and all 26 letters after, and (0 << 5) for anything after (and before as well, which is what
					// the unsigned cast does). The (1 << 5) bit (0x20) is what differs between lowercase and uppercase
					// characters. See Test/StringBenchmark.cpp for other alternative implementations leading up to this
					// point. In particular, the std::uint8_t() is crucial, unsigned() is 6x to 8x slower.
					const char* const end = data + size;
					for (char* c = data; c != end; ++c) {
						*c += (std::uint8_t(*c - 'A') < 26) << 5;
					}
				};
			}

			DEATH_CPU_MAYBE_UNUSED typename std::decay<decltype(uppercaseInPlace)>::type uppercaseInPlaceImplementation(Cpu::ScalarT) {
				return [](char* data, const std::size_t size) {
					// Same as above, except that (1 << 5) is subtracted for 'a' and all 26 letters after.
					const char* const end = data + size;
					for (char* c = data; c != end; ++c) {
						*c -= (std::uint8_t(*c - 'a') < 26) << 5;
					}
				};
			}

			DEATH_CPU_MAYBE_UNUSED typename std::decay<decltype(equalsIgnoreCase)>::type equalsIgnoreCaseImplementation(Cpu::ScalarT) {
				return [](const char* data1, const char* data2, const std::size_t size) {
					const auto notEqualsOneVector = [&](std::uint64_t w1, std::uint64_t w2) {
						const std::uint64_t highMask = std::uint64_t(0x80) * std::uint64_t(0x0101010101010101);
						const std::uint64_t loweringMask = std::uint64_t(0x20) * std::uint64_t(0x0101010101010101);
						const std::uint64_t vecA = std::uint64_t(0x80 - 'A') * std::uint64_t(0x0101010101010101);
						const std::uint64_t vecZ = std::uint64_t(0x80 - 'Z' - 1) * std::uint64_t(0x0101010101010101);

						const std::uint64_t diff = w1 ^ w2;
						if ((diff & 0xdfdfdfdfdfdfdfdf) != 0) {	// ~0x20 = 0xdf
							return true;
						}
						//const std::uint64_t anyNonAscii = (w1 | w2) & highMask;
						//if (anyNonAscii != 0) {
						//	return false;
						//}

						w1 |= loweringMask;

						// data[i] >= 'A' && !(data[i] >= 'Z' - 1)
						const std::uint64_t A = w1 + vecA;
						const std::uint64_t Z = w1 + vecZ;
						const std::uint64_t maskLower = (A ^ Z) & highMask;
						return (maskLower == highMask);
					};

					std::size_t i = 0;
					for (; i + sizeof(std::uint64_t) <= size; i += sizeof(std::uint64_t)) {
						std::uint64_t w1, w2;
						std::memcpy(&w1, data1 + i, sizeof(w1));
						std::memcpy(&w2, data2 + i, sizeof(w2));
						if (notEqualsOneVector(w1, w2)) {
							return false;
						}
					}

					// Handle remaining less than 8 bytes
					if (i < size) {
						std::uint64_t w1 = 0, w2 = 0;
						std::memcpy(&w1, data1 + i, size - i);
						std::memcpy(&w2, data2 + i, size - i);
						if (notEqualsOneVector(w1, w2)) {
							return false;
						}
					}

					return true;
				};
			}

#if defined(DEATH_ENABLE_SSE2)
			// The core vector algorithm was reverse-engineered from what GCC (and apparently also Clang) does for the scalar
			// case with SSE2 optimizations enabled. It's the same count of instructions as the "obvious" case of doing two
			// comparisons per character, ORing that, and then applying a bitmask, but considerably faster.
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SSE2 typename std::decay<decltype(lowercaseInPlace)>::type lowercaseInPlaceImplementation(Cpu::Sse2T) {
				return [](char* const data, const std::size_t size) DEATH_ENABLE_SSE2 {
					char* const end = data + size;

					// If we have less than 16 bytes, do it the stupid way, equivalent to the scalar variant and just unrolled.
					{
						char* j = data;
						switch (size) {
							case 15: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 14: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 13: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 12: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 11: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 10: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  9: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  8: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  7: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  6: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  5: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  4: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  3: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  2: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  1: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					// Core algorithm
					const __m128i aAndAbove = _mm_set1_epi8(char(256u - std::uint8_t('A')));
					const __m128i lowest25 = _mm_set1_epi8(25);
					const __m128i lowercaseBit = _mm_set1_epi8(0x20);
					const auto lowercaseOneVector = [&](const __m128i chars) DEATH_ENABLE_SSE2 {
						// Moves 'A' and everything above to 0 and up (it overflows and wraps around)
						const __m128i uppercaseInLowest25 = _mm_add_epi8(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'A' to 'Z' (now 0 to 25) zero and everything else non-zero
						const __m128i lowest25IsZero = _mm_subs_epu8(uppercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values are now zero
						const __m128i maskUppercase = _mm_cmpeq_epi8(lowest25IsZero, _mm_setzero_si128());
						// For the masked chars a lowercase bit is set, and the bit is then added to the original chars,
						// making the uppercase chars lowercase
						return _mm_add_epi8(chars, _mm_and_si128(maskUppercase, lowercaseBit));
					};

					// Unconditionally convert the first vector in a slower, unaligned way. Any extra branching to avoid the unaligned
					// load & store if already aligned would be most probably more expensive than the actual operation.
					{
						const __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(data), lowercaseOneVector(chars));
					}

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned vector;
					// if not, there will be an overlap and we'll convert some bytes twice. Which is fine,
					// lowercasing already-lowercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);

					// Convert all aligned vectors using aligned load/store
					for (; i + 16 <= end; i += 16) {
						const __m128i chars = _mm_load_si128(reinterpret_cast<const __m128i*>(i));
						_mm_store_si128(reinterpret_cast<__m128i*>(i), lowercaseOneVector(chars));
					}

					// Handle remaining less than a vector with an unaligned load & store, again overlapping back
					// with the previous already-converted elements
					if (i < end) {
						i = end - 16;
						const __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(i), lowercaseOneVector(chars));
					}
				};
			}

			// Compared to the lowercase implementation it (obviously) uses the scalar uppercasing code in the less-than-16 case.
			// In the vector case zeroes out the a-z range instead of A-Z, and subtracts the lowercase bit instead of adding.
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SSE2 typename std::decay<decltype(uppercaseInPlace)>::type uppercaseInPlaceImplementation(Cpu::Sse2T) {
				return [](char* const data, const std::size_t size) DEATH_ENABLE_SSE2 {
					char* const end = data + size;

					// If we have less than 16 bytes, do it the stupid way, equivalent to the scalar variant and just unrolled.
					{
						char* j = data;
						switch (size) {
							case 15: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 14: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 13: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 12: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 11: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 10: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  9: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  8: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  7: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  6: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  5: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  4: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  3: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  2: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  1: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					// Core algorithm
					const __m128i aAndAbove = _mm_set1_epi8(char(256u - std::uint8_t('a')));
					const __m128i lowest25 = _mm_set1_epi8(25);
					const __m128i lowercaseBit = _mm_set1_epi8(0x20);
					const auto uppercaseOneVector = [&](const __m128i chars) DEATH_ENABLE_SSE2 {
						// Moves 'a' and everything above to 0 and up (it overflows and wraps around)
						const __m128i lowercaseInLowest25 = _mm_add_epi8(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'a' to 'z' (now 0 to 25) zero and everything else non-zero
						const __m128i lowest25IsZero = _mm_subs_epu8(lowercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values arenow zero
						const __m128i maskUppercase = _mm_cmpeq_epi8(lowest25IsZero, _mm_setzero_si128());
						// For the masked chars a lowercase bit is set, and the bit is then subtracted from the original chars,
						// making the lowercase chars uppercase
						return _mm_sub_epi8(chars, _mm_and_si128(maskUppercase, lowercaseBit));
					};

					// Unconditionally convert the first vector in a slower, unaligned way. Any extra branching to avoid the unaligned
					// load & store if already aligned would be most probably more expensive than the actual operation.
					{
						const __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(data), uppercaseOneVector(chars));
					}

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll convert some bytes twice. Which is fine,
					// uppercasing already-uppercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);

					// Convert all aligned vectors using aligned load/store
					for (; i + 16 <= end; i += 16) {
						const __m128i chars = _mm_load_si128(reinterpret_cast<const __m128i*>(i));
						_mm_store_si128(reinterpret_cast<__m128i*>(i), uppercaseOneVector(chars));
					}

					// Handle remaining less than a vector with an unaligned load & store, again overlapping back
					// with the previous already-converted elements
					if (i < end) {
						i = end - 16;
						const __m128i chars = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(i), uppercaseOneVector(chars));
					}
				};
			}

			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SSE2 typename std::decay<decltype(equalsIgnoreCase)>::type equalsIgnoreCaseImplementation(Cpu::Sse2T) {
				return [](const char* data1, const char* data2, const std::size_t size) DEATH_ENABLE_SSE2 {
					if (size < 16)
						return equalsIgnoreCaseImplementation(Cpu::Scalar)(data1, data2, size);

					const __m128i loweringMask = _mm_set1_epi8(0x20);
					const __m128i vecA = _mm_set1_epi8('a');
					const __m128i vecZMinusA = _mm_set1_epi8('z' - 'a');

					const auto notEqualsOneVector = [&](const __m128i& chars1, const __m128i& chars2) DEATH_ENABLE_SSE2 {
						// notEquals = ~(chars1 == chars2);
						const __m128i notEquals = _mm_andnot_si128(_mm_cmpeq_epi8(chars1, chars2), _mm_set1_epi32(-1));
						if (_mm_movemask_epi8(_mm_cmpeq_epi8(notEquals, _mm_setzero_si128())) != 0xFFFF) {
							// Not exact match
							// chars1Lower = chars1 | loweringMask;
							const __m128i chars1Lower = _mm_or_si128(chars1, loweringMask);
							// chars2Lower = chars2 | loweringMask;
							const __m128i chars2Lower = _mm_or_si128(chars2, loweringMask);
							// greaterThan = ((chars1Lower - vecA) & notEquals) > vecZMinusA;
							const __m128i greaterThan = _mm_cmpgt_epi8(_mm_and_si128(_mm_subs_epi8(chars1Lower, vecA), notEquals), vecZMinusA);
							// if (greatedThan || (chars1Lower != chars2Lower))
							if (_mm_movemask_epi8(greaterThan) != 0x0000 || _mm_movemask_epi8(_mm_cmpeq_epi8(chars1Lower, chars2Lower)) != 0xFFFF) {
								return true;
							}
						}

						return false;
					};

					std::size_t i = 0;
					std::size_t lengthToExamine = size - 16;

					do {
						const __m128i chars1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data1 + i));
						const __m128i chars2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data2 + i));
						if (notEqualsOneVector(chars1, chars2)) {
							return false;
						}
						i += 16;
					} while (i <= lengthToExamine);

					if (i != size) {
						i = size - 16;
						const __m128i chars1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data1 + i));
						const __m128i chars2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data2 + i));
						if (notEqualsOneVector(chars1, chars2)) {
							return false;
						}
					}

					return true;
				};
			}
#endif

#if defined(DEATH_ENABLE_AVX2)
			// Trivial extension of the SSE2 code to AVX2. The only significant difference is a workaround for MinGW, see the comment below.
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_AVX2 typename std::decay<decltype(lowercaseInPlace)>::type lowercaseInPlaceImplementation(Cpu::Avx2T) {
				return [](char* const data, const std::size_t size) DEATH_ENABLE_AVX2 {
					char* const end = data + size;

					// If we have less than 32 bytes, fall back to the SSE variant
					if (size < 32)
						return lowercaseInPlaceImplementation(Cpu::Sse2)(data, size);

					// Core algorithm
					const __m256i aAndAbove = _mm256_set1_epi8(char(256u - std::uint8_t('A')));
					const __m256i lowest25 = _mm256_set1_epi8(25);
					const __m256i lowercaseBit = _mm256_set1_epi8(0x20);
					// Compared to the SSE2 case, this performs the operation in-place on a __m256i reference instead
					// of taking and returning it by value. This is in order to work around a MinGW / Windows GCC bug,
					// where it doesn't align __m256i instances passed to or returned from functions to 32 bytes,
					// but still uses aligned load/store for them. Reported back in 2011, still not fixed even in late 2023:
					//	https://gcc.gnu.org/bugzilla/show_bug.cgi?id=49001
					//	https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54412
					const auto lowercaseOneVectorInPlace = [&](__m256i& chars) DEATH_ENABLE_AVX2 {
						// Moves 'A' and everything above to 0 and up (it overflows and wraps around)
						const __m256i uppercaseInLowest25 = _mm256_add_epi8(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'A' to 'Z' (now 0 to 25) zero and everything else non-zero
						const __m256i lowest25IsZero = _mm256_subs_epu8(uppercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values are now zero
						const __m256i maskUppercase = _mm256_cmpeq_epi8(lowest25IsZero, _mm256_setzero_si256());
						// For the masked chars a lowercase bit is set, and the bit is then added to the original chars,
						// making the uppercase chars lowercase
						chars = _mm256_add_epi8(chars, _mm256_and_si256(maskUppercase, lowercaseBit));
					};

					// Unconditionally convert the first vector in a slower, unaligned way. Any extra branching to avoid the unaligned
					// load & store if already aligned would be most probably more expensive than the actual operation.
					{
						__m256i chars = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
						lowercaseOneVectorInPlace(chars);
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(data), chars);
					}

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll convert some bytes twice. Which is fine,
					// lowercasing already-lowercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 32) & ~0x1f);

					// Convert all aligned vectors using aligned load/store
					for (; i + 32 <= end; i += 32) {
						__m256i chars = _mm256_load_si256(reinterpret_cast<const __m256i*>(i));
						lowercaseOneVectorInPlace(chars);
						_mm256_store_si256(reinterpret_cast<__m256i*>(i), chars);
					}

					// Handle remaining less than a vector with an unaligned load & store, again overlapping back
					// with the previous already-converted elements
					if (i < end) {
						i = end - 32;
						__m256i chars = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(i));
						lowercaseOneVectorInPlace(chars);
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(i), chars);
					}
				};
			}

			// Again just trivial extension to AVX2, and the MinGW workaround
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_AVX2 typename std::decay<decltype(uppercaseInPlace)>::type uppercaseInPlaceImplementation(Cpu::Avx2T) {
				return [](char* const data, const std::size_t size) DEATH_ENABLE_AVX2 {
					char* const end = data + size;

					// If we have less than 32 bytes, fall back to the SSE variant
					if (size < 32)
						return uppercaseInPlaceImplementation(Cpu::Sse2)(data, size);

					// Core algorithm
					const __m256i aAndAbove = _mm256_set1_epi8(char(256u - std::uint8_t('a')));
					const __m256i lowest25 = _mm256_set1_epi8(25);
					const __m256i lowercaseBit = _mm256_set1_epi8(0x20);
					// See the comment next to lowercaseOneVectorInPlace() above for why this is done in-place
					const auto uppercaseOneVectorInPlace = [&](__m256i& chars) DEATH_ENABLE_AVX2 {
						// Moves 'a' and everything above to 0 and up (it overflows and wraps around)
						const __m256i lowercaseInLowest25 = _mm256_add_epi8(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'a' to 'z' (now 0 to 25) zero and everything else non-zero
						const __m256i lowest25IsZero = _mm256_subs_epu8(lowercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values are now zero
						const __m256i maskUppercase = _mm256_cmpeq_epi8(lowest25IsZero, _mm256_setzero_si256());
						// For the masked chars a lowercase bit is set, and the bit is then subtracted from the original chars,
						// making the lowercase chars uppercase
						chars = _mm256_sub_epi8(chars, _mm256_and_si256(maskUppercase, lowercaseBit));
					};

					// Unconditionally convert the first vector in a slower, unaligned way. Any extra branching to avoid the unaligned
					// load & store if already aligned would be most probably more expensive than the actual operation.
					{
						__m256i chars = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
						uppercaseOneVectorInPlace(chars);
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(data), chars);
					}

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll convert some bytes twice. Which is fine,
					// uppercasing already-uppercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 32) & ~0x1f);

					// Convert all aligned vectors using aligned load/store
					for (; i + 32 <= end; i += 32) {
						__m256i chars = _mm256_load_si256(reinterpret_cast<const __m256i*>(i));
						uppercaseOneVectorInPlace(chars);
						_mm256_store_si256(reinterpret_cast<__m256i*>(i), chars);
					}

					// Handle remaining less than a vector with an unaligned load & store, again overlapping back
					// with the previous already-converted elements
					if (i < end) {
						i = end - 32;
						__m256i chars = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(i));
						uppercaseOneVectorInPlace(chars);
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(i), chars);
					}
				};
			}

			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_AVX2 typename std::decay<decltype(equalsIgnoreCase)>::type equalsIgnoreCaseImplementation(Cpu::Avx2T) {
				return [](const char* data1, const char* data2, const std::size_t size) DEATH_ENABLE_AVX2 {
					if (size < 32)
						return equalsIgnoreCaseImplementation(Cpu::Sse2)(data1, data2, size);
					
					const __m256i loweringMask = _mm256_set1_epi8(0x20);
					const __m256i vecA = _mm256_set1_epi8('a');
					const __m256i vecZMinusA = _mm256_set1_epi8('z' - 'a');

					const auto notEqualsOneVector = [&](const __m256i& chars1, const __m256i& chars2) DEATH_ENABLE_AVX2 {
						// notEquals = ~(chars1 == chars2);
						const __m256i notEquals = _mm256_andnot_si256(_mm256_cmpeq_epi8(chars1, chars2), _mm256_set1_epi32(-1));
						if (_mm256_testz_si256(notEquals, notEquals) == 0) {
							// Not exact match
							// chars1Lower = chars1 | loweringMask;
							const __m256i chars1Lower = _mm256_or_si256(chars1, loweringMask);
							// chars2Lower = chars2 | loweringMask;
							const __m256i chars2Lower = _mm256_or_si256(chars2, loweringMask);
							// greaterThan = ((chars1Lower - vecA) & notEquals) > vecZMinusA;
							const __m256i greaterThan = _mm256_cmpgt_epi8(_mm256_and_si256(_mm256_subs_epi8(chars1Lower, vecA), notEquals), vecZMinusA);
							// if (greatedThan || (chars1Lower != chars2Lower))
							if (_mm256_testz_si256(greaterThan, greaterThan) == 0 || _mm256_movemask_epi8(_mm256_cmpeq_epi8(chars1Lower, chars2Lower)) != 0xFFFFFFFFu) {
								return true;
							}
						}

						return false;
					};

					std::size_t i = 0;
					std::size_t lengthToExamine = size - 32;

					do {
						const __m256i chars1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data1 + i));
						const __m256i chars2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data2 + i));
						if (notEqualsOneVector(chars1, chars2)) {
							return false;
						}
						i += 32;
					} while (i <= lengthToExamine);

					if (i != size) {
						i = size - 32;
						const __m256i chars1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data1 + i));
						const __m256i chars2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data2 + i));
						if (notEqualsOneVector(chars1, chars2)) {
							return false;
						}
					}

					return true;
				};
			}
#endif

#if defined(DEATH_ENABLE_SIMD128)
			// Trivial port of the SSE2 code to WASM SIMD. As WASM SIMD doesn't differentiate between aligned and unaligned
			// load, the load/store code is the same for both aligned and unaligned case, making everything slightly shorter.
			// The high-level operation stays the same as with SSE2 tho, even if just for memory access patterns I think
			// it still makes sense to do as much as possible aligned.
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SIMD128 typename std::decay<decltype(lowercaseInPlace)>::type lowercaseInPlaceImplementation(Cpu::Simd128T) {
				return [](char* data, const std::size_t size) DEATH_ENABLE_SIMD128 {
					char* const end = data + size;

					// If we have less than 16 bytes, do it the stupid way, equivalent to the scalar variant and just unrolled.
					{
						char* j = data;
						switch (size) {
							case 15: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 14: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 13: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 12: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 11: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 10: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  9: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  8: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  7: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  6: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  5: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  4: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  3: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  2: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  1: *j += (std::uint8_t(*j - 'A') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					// Core algorithm
					const v128_t aAndAbove = wasm_i8x16_const_splat(char(256u - std::uint8_t('A')));
					const v128_t lowest25 = wasm_i8x16_const_splat(25);
					const v128_t lowercaseBit = wasm_i8x16_const_splat(0x20);
					const v128_t zero = wasm_i8x16_const_splat(0);
					const auto lowercaseOneVectorInPlace = [&](v128_t* const data) DEATH_ENABLE_SIMD128 {
						const v128_t chars = wasm_v128_load(data);
						// Moves 'A' and everything above to 0 and up (it overflows and wraps around)
						const v128_t uppercaseInLowest25 = wasm_i8x16_add(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'A' to 'Z' (now 0 to 25) zero and everything else non-zero
						const v128_t lowest25IsZero = wasm_u8x16_sub_sat(uppercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values are now zero
						const v128_t maskUppercase = wasm_i8x16_eq(lowest25IsZero, zero);
						// For the masked chars a lowercase bit is set, and the bit is then added to the original chars, making the uppercase chars lowercase
						wasm_v128_store(data, wasm_i8x16_add(chars, wasm_v128_and(maskUppercase, lowercaseBit)));
					};

					// Unconditionally convert the first unaligned vector
					lowercaseOneVectorInPlace(reinterpret_cast<v128_t*>(data));

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll convert some bytes twice. Which is fine,
					// lowercasing already-lowercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);

					// Convert all aligned vectors
					for (; i + 16 <= end; i += 16)
						lowercaseOneVectorInPlace(reinterpret_cast<v128_t*>(i));

					// Handle remaining less than a vector, again overlapping back with the previous
					// already-converted elements, in an unaligned way
					if (i < end) {
						i = end - 16;
						lowercaseOneVectorInPlace(reinterpret_cast<v128_t*>(i));
					}
				};
			}

			// Again just a trivial port of the SSE2 code to WASM SIMD, with the same "aligned load/store is the same as unaligned"
			// simplification as the lowercase variant above
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SIMD128 typename std::decay<decltype(uppercaseInPlace)>::type uppercaseInPlaceImplementation(Cpu::Simd128T) {
				return [](char* data, const std::size_t size) DEATH_ENABLE_SIMD128 {
					char* const end = data + size;

					// If we have less than 16 bytes, do it the stupid way, equivalent to the scalar variant and just unrolled.
					{
						char* j = data;
						switch (size) {
							case 15: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 14: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 13: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 12: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 11: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case 10: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  9: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  8: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  7: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  6: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  5: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  4: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  3: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  2: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  1: *j -= (std::uint8_t(*j - 'a') < 26) << 5; ++j; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					// Core algorithm
					const v128_t aAndAbove = wasm_i8x16_const_splat(char(256u - std::uint8_t('a')));
					const v128_t lowest25 = wasm_i8x16_const_splat(25);
					const v128_t lowercaseBit = wasm_i8x16_const_splat(0x20);
					const v128_t zero = wasm_i8x16_const_splat(0);
					const auto uppercaseOneVectorInPlace = [&](v128_t* const data) DEATH_ENABLE_SIMD128 {
						const v128_t chars = wasm_v128_load(data);
						// Moves 'a' and everything above to 0 and up (it overflows and wraps around)
						const v128_t lowercaseInLowest25 = wasm_i8x16_add(chars, aAndAbove);
						// Subtracts 25 with saturation, which makes the original 'a' to 'z' (now 0 to 25) zero and everything else non-zero
						const v128_t lowest25IsZero = wasm_u8x16_sub_sat(lowercaseInLowest25, lowest25);
						// Mask indicating where uppercase letters where, i.e. which values are now zero
						const v128_t maskUppercase = wasm_i8x16_eq(lowest25IsZero, zero);
						// For the masked chars a lowercase bit is set, and the bit is then subtracted from the original
						// chars, making the lowercase chars uppercase
						wasm_v128_store(data, wasm_i8x16_sub(chars, wasm_v128_and(maskUppercase, lowercaseBit)));
					};

					// Unconditionally convert the first unaligned vector. WASM doesn't differentiate between aligned
					// and unaligned load, it's always unaligned, however even if just for memory access patterns I think
					// it still makes sense to do as much as possible aligned, so this matches what the SSE2 code does.
					uppercaseOneVectorInPlace(reinterpret_cast<v128_t*>(data));

					// Go to the next aligned position. If the pointer was already aligned, we'll go to the next aligned vector;
					// if not, there will be an overlap and we'll convert some bytes twice. Which is fine, uppercasing
					// already-uppercased data is a no-op.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);

					// Convert all aligned vectors
					for (; i + 16 <= end; i += 16)
						uppercaseOneVectorInPlace(reinterpret_cast<v128_t*>(i));

					// Handle remaining less than a vector with an unaligned load & store, again overlapping back
					// with the previous already-converted elements
					if (i < end) {
						i = end - 16;
						uppercaseOneVectorInPlace(reinterpret_cast<v128_t*>(i));
					}
				};
			}
			#endif
		}

		DEATH_CPU_DISPATCHER_BASE(lowercaseInPlaceImplementation)
		DEATH_CPU_DISPATCHED(lowercaseInPlaceImplementation, void DEATH_CPU_DISPATCHED_DECLARATION(lowercaseInPlace)(char* data, std::size_t size))({
			return lowercaseInPlaceImplementation(Cpu::DefaultBase)(data, size);
		})
		DEATH_CPU_DISPATCHER_BASE(uppercaseInPlaceImplementation)
		DEATH_CPU_DISPATCHED(uppercaseInPlaceImplementation, void DEATH_CPU_DISPATCHED_DECLARATION(uppercaseInPlace)(char* data, std::size_t size))({
			return uppercaseInPlaceImplementation(Cpu::DefaultBase)(data, size);
		})
		DEATH_CPU_DISPATCHER_BASE(equalsIgnoreCaseImplementation)
		DEATH_CPU_DISPATCHED(equalsIgnoreCaseImplementation, bool DEATH_CPU_DISPATCHED_DECLARATION(equalsIgnoreCase)(const char* data1, const char* data2, std::size_t size))({
			return equalsIgnoreCaseImplementation(Cpu::DefaultBase)(data1, data2, size);
		})
	}

	String lowercase(StringView string) {
		// Theoretically doing the copy in the same loop as case change could be faster for *really long* strings due
		// to cache reuse, but until that proves to be a bottleneck I'll go with the simpler solution.
		// Not implementing through lowercase(String) as the call stack is deep enough already and we don't
		// need the extra checks there.
		String out{string};
		lowercaseInPlace(out);
		return out;
	}

	String lowercase(String string) {
		// In the rare scenario where we'd get a non-owned string (such as String::nullTerminatedView() passed right
		// into the function), make it owned first. Usually it'll get copied however, which already makes it owned.
		if (!string.isSmall() && string.deleter()) string = String{string};

		lowercaseInPlace(string);
		return string;
	}

	String uppercase(StringView string) {
		// Theoretically doing the copy in the same loop as case change could be faster for *really long* strings due
		// to cache reuse, but until that proves to be a bottleneck I'll go with the simpler solution.
		// Not implementing through uppercase(String) as the call stack is deep enough already and we don't
		// need the extra checks there.
		String out{string};
		uppercaseInPlace(out);
		return out;
	}

	String uppercase(String string) {
		// In the rare scenario where we'd get a non-owned string (such as String::nullTerminatedView() passed right
		// into the function), make it owned first. Usually it'll get copied however, which already makes it owned.
		if (!string.isSmall() && string.deleter()) string = String{string};

		uppercaseInPlace(string);
		return string;
	}

	String lowercaseUnicode(StringView string) {
		static const char32_t u2l[1411] = {
			0x00041, 0x00042, 0x00043, 0x00044, 0x00045, 0x00046, 0x00047, 0x00048, 0x00049, 0x0004a, 0x0004b, 0x0004c, 0x0004d, 0x0004e, 0x0004f, 0x00050,
			0x00051, 0x00052, 0x00053, 0x00054, 0x00055, 0x00056, 0x00057, 0x00058, 0x00059, 0x0005a, 0x000b5, 0x000c0, 0x000c1, 0x000c2, 0x000c3, 0x000c4,
			0x000c5, 0x000c6, 0x000c7, 0x000c8, 0x000c9, 0x000ca, 0x000cb, 0x000cc, 0x000cd, 0x000ce, 0x000cf, 0x000d0, 0x000d1, 0x000d2, 0x000d3, 0x000d4,
			0x0010e, 0x00110, 0x00112, 0x00114, 0x00116, 0x00118, 0x0011a, 0x0011c, 0x0011e, 0x00120, 0x00122, 0x00124, 0x00126, 0x00128, 0x0012a, 0x0012c,
			0x0012e, 0x00132, 0x00134, 0x00136, 0x00139, 0x0013b, 0x0013d, 0x0013f, 0x00141, 0x00143, 0x00145, 0x00147, 0x0014a, 0x0014c, 0x0014e, 0x00150,
			0x00152, 0x00154, 0x00156, 0x00158, 0x0015a, 0x0015c, 0x0015e, 0x00160, 0x00162, 0x00164, 0x00166, 0x00168, 0x0016a, 0x0016c, 0x0016e, 0x00170,
			0x00172, 0x00174, 0x00176, 0x00178, 0x00179, 0x0017b, 0x0017d, 0x0017f, 0x00181, 0x00182, 0x00184, 0x00186, 0x00187, 0x00189, 0x0018a, 0x0018b,
			0x0018e, 0x0018f, 0x00190, 0x00191, 0x00193, 0x00194, 0x00196, 0x00197, 0x00198, 0x0019c, 0x0019d, 0x0019f, 0x001a0, 0x001a2, 0x001a4, 0x001a6,
			0x001a7, 0x001a9, 0x001ac, 0x001ae, 0x001af, 0x001b1, 0x001b2, 0x001b3, 0x001b5, 0x001b7, 0x001b8, 0x001bc, 0x001c4, 0x001c5, 0x001c7, 0x001c8,
			0x001ca, 0x001cb, 0x001cd, 0x001cf, 0x001d1, 0x001d3, 0x001d5, 0x001d7, 0x001d9, 0x001db, 0x001de, 0x001e0, 0x001e2, 0x001e4, 0x001e6, 0x001e8,
			0x001ea, 0x001ec, 0x001ee, 0x001f1, 0x001f2, 0x001f4, 0x001f6, 0x001f7, 0x001f8, 0x001fa, 0x001fc, 0x001fe, 0x00200, 0x00202, 0x00204, 0x00206,
			0x00208, 0x0020a, 0x0020c, 0x0020e, 0x00210, 0x00212, 0x00214, 0x00216, 0x00218, 0x0021a, 0x0021c, 0x0021e, 0x00220, 0x00222, 0x00224, 0x00226,
			0x00228, 0x0022a, 0x0022c, 0x0022e, 0x00230, 0x00232, 0x0023a, 0x0023b, 0x0023d, 0x0023e, 0x00241, 0x00243, 0x00244, 0x00245, 0x00246, 0x00248,
			0x0024a, 0x0024c, 0x0024e, 0x00345, 0x00370, 0x00372, 0x00376, 0x0037f, 0x00386, 0x00388, 0x00389, 0x0038a, 0x0038c, 0x0038e, 0x0038f, 0x00391,
			0x00392, 0x00393, 0x00394, 0x00395, 0x00396, 0x00397, 0x00398, 0x00399, 0x0039a, 0x0039b, 0x0039c, 0x0039d, 0x0039e, 0x0039f, 0x003a0, 0x003a1,
			0x003a3, 0x003a4, 0x003a5, 0x003a6, 0x003a7, 0x003a8, 0x003a9, 0x003aa, 0x003ab, 0x003c2, 0x003cf, 0x003d0, 0x003d1, 0x003d5, 0x003d6, 0x003d8,
			0x003da, 0x003dc, 0x003de, 0x003e0, 0x003e2, 0x003e4, 0x003e6, 0x003e8, 0x003ea, 0x003ec, 0x003ee, 0x003f0, 0x003f1, 0x003f4, 0x003f5, 0x003f7,
			0x003f9, 0x003fa, 0x003fd, 0x003fe, 0x003ff, 0x00400, 0x00401, 0x00402, 0x00403, 0x00404, 0x00405, 0x00406, 0x00407, 0x00408, 0x00409, 0x0040a,
			0x0040b, 0x0040c, 0x0040d, 0x0040e, 0x0040f, 0x00410, 0x00411, 0x00412, 0x00413, 0x00414, 0x00415, 0x00416, 0x00417, 0x00418, 0x00419, 0x0041a,
			0x0041b, 0x0041c, 0x0041d, 0x0041e, 0x0041f, 0x00420, 0x00421, 0x00422, 0x00423, 0x00424, 0x00425, 0x00426, 0x00427, 0x00428, 0x00429, 0x0042a,
			0x0042b, 0x0042c, 0x0042d, 0x0042e, 0x0042f, 0x00460, 0x00462, 0x00464, 0x00466, 0x00468, 0x0046a, 0x0046c, 0x0046e, 0x00470, 0x00472, 0x00474,
			0x00476, 0x00478, 0x0047a, 0x0047c, 0x0047e, 0x00480, 0x0048a, 0x0048c, 0x0048e, 0x00490, 0x00492, 0x00494, 0x00496, 0x00498, 0x0049a, 0x0049c,
			0x0049e, 0x004a0, 0x004a2, 0x004a4, 0x004a6, 0x004a8, 0x004aa, 0x004ac, 0x004ae, 0x004b0, 0x004b2, 0x004b4, 0x004b6, 0x004b8, 0x004ba, 0x004bc,
			0x004be, 0x004c0, 0x004c1, 0x004c3, 0x004c5, 0x004c7, 0x004c9, 0x004cb, 0x004cd, 0x004d0, 0x004d2, 0x004d4, 0x004d6, 0x004d8, 0x004da, 0x004dc,
			0x004de, 0x004e0, 0x004e2, 0x004e4, 0x004e6, 0x004e8, 0x004ea, 0x004ec, 0x004ee, 0x004f0, 0x004f2, 0x004f4, 0x004f6, 0x004f8, 0x004fa, 0x004fc,
			0x004fe, 0x00500, 0x00502, 0x00504, 0x00506, 0x00508, 0x0050a, 0x0050c, 0x0050e, 0x00510, 0x00512, 0x00514, 0x00516, 0x00518, 0x0051a, 0x0051c,
			0x0051e, 0x00520, 0x00522, 0x00524, 0x00526, 0x00528, 0x0052a, 0x0052c, 0x0052e, 0x00531, 0x00532, 0x00533, 0x00534, 0x00535, 0x00536, 0x00537,
			0x00538, 0x00539, 0x0053a, 0x0053b, 0x0053c, 0x0053d, 0x0053e, 0x0053f, 0x00540, 0x00541, 0x00542, 0x00543, 0x00544, 0x00545, 0x00546, 0x00547,
			0x00548, 0x00549, 0x0054a, 0x0054b, 0x0054c, 0x0054d, 0x0054e, 0x0054f, 0x00550, 0x00551, 0x00552, 0x00553, 0x00554, 0x00555, 0x00556, 0x010a0,
			0x010a1, 0x010a2, 0x010a3, 0x010a4, 0x010a5, 0x010a6, 0x010a7, 0x010a8, 0x010a9, 0x010aa, 0x010ab, 0x010ac, 0x010ad, 0x010ae, 0x010af, 0x010b0,
			0x010b1, 0x010b2, 0x010b3, 0x010b4, 0x010b5, 0x010b6, 0x010b7, 0x010b8, 0x010b9, 0x010ba, 0x010bb, 0x010bc, 0x010bd, 0x010be, 0x010bf, 0x010c0,
			0x010c1, 0x010c2, 0x010c3, 0x010c4, 0x010c5, 0x010c7, 0x010cd, 0x013f8, 0x013f9, 0x013fa, 0x013fb, 0x013fc, 0x013fd, 0x01c80, 0x01c81, 0x01c82,
			0x01c83, 0x01c84, 0x01c85, 0x01c86, 0x01c87, 0x01c88, 0x01c90, 0x01c91, 0x01c92, 0x01c93, 0x01c94, 0x01c95, 0x01c96, 0x01c97, 0x01c98, 0x01c99,
			0x01c9a, 0x01c9b, 0x01c9c, 0x01c9d, 0x01c9e, 0x01c9f, 0x01ca0, 0x01ca1, 0x01ca2, 0x01ca3, 0x01ca4, 0x01ca5, 0x01ca6, 0x01ca7, 0x01ca8, 0x01ca9,
			0x01caa, 0x01cab, 0x01cac, 0x01cad, 0x01cae, 0x01caf, 0x01cb0, 0x01cb1, 0x01cb2, 0x01cb3, 0x01cb4, 0x01cb5, 0x01cb6, 0x01cb7, 0x01cb8, 0x01cb9,
			0x01cba, 0x01cbd, 0x01cbe, 0x01cbf, 0x01e00, 0x01e02, 0x01e04, 0x01e06, 0x01e08, 0x01e0a, 0x01e0c, 0x01e0e, 0x01e10, 0x01e12, 0x01e14, 0x01e16,
			0x01e18, 0x01e1a, 0x01e1c, 0x01e1e, 0x01e20, 0x01e22, 0x01e24, 0x01e26, 0x01e28, 0x01e2a, 0x01e2c, 0x01e2e, 0x01e30, 0x01e32, 0x01e34, 0x01e36,
			0x01e38, 0x01e3a, 0x01e3c, 0x01e3e, 0x01e40, 0x01e42, 0x01e44, 0x01e46, 0x01e48, 0x01e4a, 0x01e4c, 0x01e4e, 0x01e50, 0x01e52, 0x01e54, 0x01e56,
			0x01e58, 0x01e5a, 0x01e5c, 0x01e5e, 0x01e60, 0x01e62, 0x01e64, 0x01e66, 0x01e68, 0x01e6a, 0x01e6c, 0x01e6e, 0x01e70, 0x01e72, 0x01e74, 0x01e76,
			0x01e78, 0x01e7a, 0x01e7c, 0x01e7e, 0x01e80, 0x01e82, 0x01e84, 0x01e86, 0x01e88, 0x01e8a, 0x01e8c, 0x01e8e, 0x01e90, 0x01e92, 0x01e94, 0x01e9b,
			0x01e9e, 0x01ea0, 0x01ea2, 0x01ea4, 0x01ea6, 0x01ea8, 0x01eaa, 0x01eac, 0x01eae, 0x01eb0, 0x01eb2, 0x01eb4, 0x01eb6, 0x01eb8, 0x01eba, 0x01ebc,
			0x01ebe, 0x01ec0, 0x01ec2, 0x01ec4, 0x01ec6, 0x01ec8, 0x01eca, 0x01ecc, 0x01ece, 0x01ed0, 0x01ed2, 0x01ed4, 0x01ed6, 0x01ed8, 0x01eda, 0x01edc,
			0x01ede, 0x01ee0, 0x01ee2, 0x01ee4, 0x01ee6, 0x01ee8, 0x01eea, 0x01eec, 0x01eee, 0x01ef0, 0x01ef2, 0x01ef4, 0x01ef6, 0x01ef8, 0x01efa, 0x01efc,
			0x01efe, 0x01f08, 0x01f09, 0x01f0a, 0x01f0b, 0x01f0c, 0x01f0d, 0x01f0e, 0x01f0f, 0x01f18, 0x01f19, 0x01f1a, 0x01f1b, 0x01f1c, 0x01f1d, 0x01f28,
			0x01f29, 0x01f2a, 0x01f2b, 0x01f2c, 0x01f2d, 0x01f2e, 0x01f2f, 0x01f38, 0x01f39, 0x01f3a, 0x01f3b, 0x01f3c, 0x01f3d, 0x01f3e, 0x01f3f, 0x01f48,
			0x01f6f, 0x01f88, 0x01f89, 0x01f8a, 0x01f8b, 0x01f8c, 0x01f8d, 0x01f8e, 0x01f8f, 0x01f98, 0x01f99, 0x01f9a, 0x01f9b, 0x01f9c, 0x01f9d, 0x01f9e,
			0x01f9f, 0x01fa8, 0x01fa9, 0x01faa, 0x01fab, 0x01fac, 0x01fad, 0x01fae, 0x01faf, 0x01fb8, 0x01fb9, 0x01fba, 0x01fbb, 0x01fbc, 0x01fbe, 0x01fc8,
			0x01fc9, 0x01fca, 0x01fcb, 0x01fcc, 0x01fd8, 0x01fd9, 0x01fda, 0x01fdb, 0x01fe8, 0x01fe9, 0x01fea, 0x01feb, 0x01fec, 0x01ff8, 0x01ff9, 0x01ffa,
			0x01ffb, 0x01ffc, 0x02126, 0x0212a, 0x0212b, 0x02132, 0x02160, 0x02161, 0x02162, 0x02163, 0x02164, 0x02165, 0x02166, 0x02167, 0x02168, 0x02169,
			0x0216a, 0x0216b, 0x0216c, 0x0216d, 0x0216e, 0x0216f, 0x02183, 0x024b6, 0x024b7, 0x024b8, 0x024b9, 0x024ba, 0x024bb, 0x024bc, 0x024bd, 0x024be,
			0x024bf, 0x024c0, 0x024c1, 0x024c2, 0x024c3, 0x024c4, 0x024c5, 0x024c6, 0x024c7, 0x024c8, 0x024c9, 0x024ca, 0x024cb, 0x024cc, 0x024cd, 0x024ce,
			0x024cf, 0x02c00, 0x02c01, 0x02c02, 0x02c03, 0x02c04, 0x02c05, 0x02c06, 0x02c07, 0x02c08, 0x02c09, 0x02c0a, 0x02c0b, 0x02c0c, 0x02c0d, 0x02c0e,
			0x02c0f, 0x02c10, 0x02c11, 0x02c12, 0x02c13, 0x02c14, 0x02c15, 0x02c16, 0x02c17, 0x02c18, 0x02c19, 0x02c1a, 0x02c1b, 0x02c1c, 0x02c1d, 0x02c1e,
			0x02c1f, 0x02c20, 0x02c21, 0x02c22, 0x02c23, 0x02c24, 0x02c25, 0x02c26, 0x02c27, 0x02c28, 0x02c29, 0x02c2a, 0x02c2b, 0x02c2c, 0x02c2d, 0x02c2e,
			0x02c60, 0x02c62, 0x02c63, 0x02c64, 0x02c67, 0x02c69, 0x02c6b, 0x02c6d, 0x02c6e, 0x02c6f, 0x02c70, 0x02c72, 0x02c75, 0x02c7e, 0x02c7f, 0x02c80,
			0x02c82, 0x02c84, 0x02c86, 0x02c88, 0x02c8a, 0x02c8c, 0x02c8e, 0x02c90, 0x02c92, 0x02c94, 0x02c96, 0x02c98, 0x02c9a, 0x02c9c, 0x02c9e, 0x02ca0,
			0x02ca2, 0x02ca4, 0x02ca6, 0x02ca8, 0x02caa, 0x02cac, 0x02cae, 0x02cb0, 0x02cb2, 0x02cb4, 0x02cb6, 0x02cb8, 0x02cba, 0x02cbc, 0x02cbe, 0x02cc0,
			0x02ce2, 0x02ceb, 0x02ced, 0x02cf2, 0x0a640, 0x0a642, 0x0a644, 0x0a646, 0x0a648, 0x0a64a, 0x0a64c, 0x0a64e, 0x0a650, 0x0a652, 0x0a654, 0x0a656,
			0x0a658, 0x0a65a, 0x0a65c, 0x0a65e, 0x0a660, 0x0a662, 0x0a664, 0x0a666, 0x0a668, 0x0a66a, 0x0a66c, 0x0a680, 0x0a682, 0x0a684, 0x0a686, 0x0a688,
			0x0a68a, 0x0a68c, 0x0a68e, 0x0a690, 0x0a692, 0x0a694, 0x0a696, 0x0a698, 0x0a69a, 0x0a722, 0x0a724, 0x0a726, 0x0a728, 0x0a72a, 0x0a72c, 0x0a72e,
			0x0a732, 0x0a734, 0x0a736, 0x0a738, 0x0a73a, 0x0a73c, 0x0a73e, 0x0a740, 0x0a742, 0x0a744, 0x0a746, 0x0a748, 0x0a74a, 0x0a74c, 0x0a74e, 0x0a750,
			0x0a752, 0x0a754, 0x0a756, 0x0a758, 0x0a75a, 0x0a75c, 0x0a75e, 0x0a760, 0x0a762, 0x0a764, 0x0a766, 0x0a768, 0x0a76a, 0x0a76c, 0x0a76e, 0x0a779,
			0x0a77b, 0x0a77d, 0x0a77e, 0x0a780, 0x0a782, 0x0a784, 0x0a786, 0x0a78b, 0x0a78d, 0x0a790, 0x0a792, 0x0a796, 0x0a798, 0x0a79a, 0x0a79c, 0x0a79e,
			0x0a7a0, 0x0a7a2, 0x0a7a4, 0x0a7a6, 0x0a7a8, 0x0a7aa, 0x0a7ab, 0x0a7ac, 0x0a7ad, 0x0a7ae, 0x0a7b0, 0x0a7b1, 0x0a7b2, 0x0a7b3, 0x0a7b4, 0x0a7b6,
			0x0a7b8, 0x0a7ba, 0x0a7bc, 0x0a7be, 0x0a7c2, 0x0a7c4, 0x0a7c5, 0x0a7c6, 0x0ab70, 0x0ab71, 0x0ab72, 0x0ab73, 0x0ab74, 0x0ab75, 0x0ab76, 0x0ab77,
			0x0ab78, 0x0ab79, 0x0ab7a, 0x0ab7b, 0x0ab7c, 0x0ab7d, 0x0ab7e, 0x0ab7f, 0x0ab80, 0x0ab81, 0x0ab82, 0x0ab83, 0x0ab84, 0x0ab85, 0x0ab86, 0x0ab87,
			0x0ab88, 0x0ab89, 0x0ab8a, 0x0ab8b, 0x0ab8c, 0x0ab8d, 0x0ab8e, 0x0ab8f, 0x0ab90, 0x0ab91, 0x0ab92, 0x0ab93, 0x0ab94, 0x0ab95, 0x0ab96, 0x0ab97,
			0x0ab98, 0x0ab99, 0x0ab9a, 0x0ab9b, 0x0ab9c, 0x0ab9d, 0x0ab9e, 0x0ab9f, 0x0aba0, 0x0aba1, 0x0aba2, 0x0aba3, 0x0aba4, 0x0aba5, 0x0aba6, 0x0aba7,
			0x0aba8, 0x0aba9, 0x0abaa, 0x0abab, 0x0abac, 0x0abad, 0x0abae, 0x0abaf, 0x0abb0, 0x0abb1, 0x0abb2, 0x0abb3, 0x0abb4, 0x0abb5, 0x0abb6, 0x0abb7,
			0x0abb8, 0x0abb9, 0x0abba, 0x0abbb, 0x0abbc, 0x0abbd, 0x0abbe, 0x0abbf, 0x0ff21, 0x0ff22, 0x0ff23, 0x0ff24, 0x0ff25, 0x0ff26, 0x0ff27, 0x0ff28,
			0x0ff29, 0x0ff2a, 0x0ff2b, 0x0ff2c, 0x0ff2d, 0x0ff2e, 0x0ff2f, 0x0ff30, 0x0ff31, 0x0ff32, 0x0ff33, 0x0ff34, 0x0ff35, 0x0ff36, 0x0ff37, 0x0ff38,
			0x0ff39, 0x0ff3a, 0x10400, 0x10401, 0x10402, 0x10403, 0x10404, 0x10405, 0x10406, 0x10407, 0x10408, 0x10409, 0x1040a, 0x1040b, 0x1040c, 0x1040d,
			0x1040e, 0x1040f, 0x10410, 0x10411, 0x10412, 0x10413, 0x10414, 0x10415, 0x10416, 0x10417, 0x10418, 0x10419, 0x1041a, 0x1041b, 0x1041c, 0x1041d,
			0x1041e, 0x1041f, 0x10420, 0x10421, 0x10422, 0x10423, 0x10424, 0x10425, 0x10426, 0x10427, 0x104b0, 0x104b1, 0x104b2, 0x104b3, 0x104b4, 0x104b5,
			0x104b6, 0x104b7, 0x104b8, 0x104b9, 0x104ba, 0x104bb, 0x104bc, 0x104bd, 0x104be, 0x104bf, 0x104c0, 0x104c1, 0x104c2, 0x104c3, 0x104c4, 0x104c5,
			0x104c6, 0x104c7, 0x104c8, 0x104c9, 0x104ca, 0x104cb, 0x104cc, 0x104cd, 0x104ce, 0x104cf, 0x104d0, 0x104d1, 0x104d2, 0x104d3, 0x10c80, 0x10c81,
			0x10c82, 0x10c83, 0x10c84, 0x10c85, 0x10c86, 0x10c87, 0x10c88, 0x10c89, 0x10c8a, 0x10c8b, 0x10c8c, 0x10c8d, 0x10c8e, 0x10c8f, 0x10c90, 0x10c91,
			0x10c92, 0x10c93, 0x10c94, 0x10c95, 0x10c96, 0x10c97, 0x10c98, 0x10c99, 0x10c9a, 0x10c9b, 0x10c9c, 0x10c9d, 0x10c9e, 0x10c9f, 0x10ca0, 0x10ca1,
			0x10ca2, 0x10ca3, 0x10ca4, 0x10ca5, 0x10ca6, 0x10ca7, 0x10ca8, 0x10ca9, 0x10caa, 0x10cab, 0x10cac, 0x10cad, 0x10cae, 0x10caf, 0x10cb0, 0x10cb1,
			0x10cb2, 0x118a0, 0x118a1, 0x118a2, 0x118a3, 0x118a4, 0x118a5, 0x118a6, 0x118a7, 0x118a8, 0x118a9, 0x118aa, 0x118ab, 0x118ac, 0x118ad, 0x118ae,
			0x118af, 0x118b0, 0x118b1, 0x118b2, 0x118b3, 0x118b4, 0x118b5, 0x118b6, 0x118b7, 0x118b8, 0x118b9, 0x118ba, 0x118bb, 0x118bc, 0x118bd, 0x118be,
			0x118bf, 0x16e40, 0x16e41, 0x16e42, 0x16e43, 0x16e44, 0x16e45, 0x16e46, 0x16e47, 0x16e48, 0x16e49, 0x16e4a, 0x16e4b, 0x16e4c, 0x16e4d, 0x16e4e,
			0x16e4f, 0x16e50, 0x16e51, 0x16e52, 0x16e53, 0x16e54, 0x16e55, 0x16e56, 0x16e57, 0x16e58, 0x16e59, 0x16e5a, 0x16e5b, 0x16e5c, 0x16e5d, 0x16e5e,
			0x16e5f, 0x1e900, 0x1e901, 0x1e902, 0x1e903, 0x1e904, 0x1e905, 0x1e906, 0x1e907, 0x1e908, 0x1e909, 0x1e90a, 0x1e90b, 0x1e90c, 0x1e90d, 0x1e90e,
			0x1e90f, 0x1e910, 0x1e911, 0x1e912, 0x1e913, 0x1e914, 0x1e915, 0x1e916, 0x1e917, 0x1e918, 0x1e919, 0x1e91a, 0x1e91b, 0x1e91c, 0x1e91d, 0x1e91e,
			0x1e91f, 0x1e920, 0x1e921
		};

		static const char32_t lc[1411] = {
			0x00061, 0x00062, 0x00063, 0x00064, 0x00065, 0x00066, 0x00067, 0x00068, 0x00069, 0x0006a, 0x0006b, 0x0006c, 0x0006d, 0x0006e, 0x0006f, 0x00070,
			0x00071, 0x00072, 0x00073, 0x00074, 0x00075, 0x00076, 0x00077, 0x00078, 0x00079, 0x0007a, 0x003bc, 0x000e0, 0x000e1, 0x000e2, 0x000e3, 0x000e4,
			0x000e5, 0x000e6, 0x000e7, 0x000e8, 0x000e9, 0x000ea, 0x000eb, 0x000ec, 0x000ed, 0x000ee, 0x000ef, 0x000f0, 0x000f1, 0x000f2, 0x000f3, 0x000f4,
			0x000f5, 0x000f6, 0x000f8, 0x000f9, 0x000fa, 0x000fb, 0x000fc, 0x000fd, 0x000fe, 0x00101, 0x00103, 0x00105, 0x00107, 0x00109, 0x0010b, 0x0010d,
			0x0010f, 0x00111, 0x00113, 0x00115, 0x00117, 0x00119, 0x0011b, 0x0011d, 0x0011f, 0x00121, 0x00123, 0x00125, 0x00127, 0x00129, 0x0012b, 0x0012d,
			0x0012f, 0x00133, 0x00135, 0x00137, 0x0013a, 0x0013c, 0x0013e, 0x00140, 0x00142, 0x00144, 0x00146, 0x00148, 0x0014b, 0x0014d, 0x0014f, 0x00151,
			0x00153, 0x00155, 0x00157, 0x00159, 0x0015b, 0x0015d, 0x0015f, 0x00161, 0x00163, 0x00165, 0x00167, 0x00169, 0x0016b, 0x0016d, 0x0016f, 0x00171,
			0x00173, 0x00175, 0x00177, 0x000ff, 0x0017a, 0x0017c, 0x0017e, 0x00073, 0x00253, 0x00183, 0x00185, 0x00254, 0x00188, 0x00256, 0x00257, 0x0018c,
			0x001dd, 0x00259, 0x0025b, 0x00192, 0x00260, 0x00263, 0x00269, 0x00268, 0x00199, 0x0026f, 0x00272, 0x00275, 0x001a1, 0x001a3, 0x001a5, 0x00280,
			0x001a8, 0x00283, 0x001ad, 0x00288, 0x001b0, 0x0028a, 0x0028b, 0x001b4, 0x001b6, 0x00292, 0x001b9, 0x001bd, 0x001c6, 0x001c6, 0x001c9, 0x001c9,
			0x001cc, 0x001cc, 0x001ce, 0x001d0, 0x001d2, 0x001d4, 0x001d6, 0x001d8, 0x001da, 0x001dc, 0x001df, 0x001e1, 0x001e3, 0x001e5, 0x001e7, 0x001e9,
			0x001eb, 0x001ed, 0x001ef, 0x001f3, 0x001f3, 0x001f5, 0x00195, 0x001bf, 0x001f9, 0x001fb, 0x001fd, 0x001ff, 0x00201, 0x00203, 0x00205, 0x00207,
			0x00209, 0x0020b, 0x0020d, 0x0020f, 0x00211, 0x00213, 0x00215, 0x00217, 0x00219, 0x0021b, 0x0021d, 0x0021f, 0x0019e, 0x00223, 0x00225, 0x00227,
			0x00229, 0x0022b, 0x0022d, 0x0022f, 0x00231, 0x00233, 0x02c65, 0x0023c, 0x0019a, 0x02c66, 0x00242, 0x00180, 0x00289, 0x0028c, 0x00247, 0x00249,
			0x0024b, 0x0024d, 0x0024f, 0x003b9, 0x00371, 0x00373, 0x00377, 0x003f3, 0x003ac, 0x003ad, 0x003ae, 0x003af, 0x003cc, 0x003cd, 0x003ce, 0x003b1,
			0x003b2, 0x003b3, 0x003b4, 0x003b5, 0x003b6, 0x003b7, 0x003b8, 0x003b9, 0x003ba, 0x003bb, 0x003bc, 0x003bd, 0x003be, 0x003bf, 0x003c0, 0x003c1,
			0x003c3, 0x003c4, 0x003c5, 0x003c6, 0x003c7, 0x003c8, 0x003c9, 0x003ca, 0x003cb, 0x003c3, 0x003d7, 0x003b2, 0x003b8, 0x003c6, 0x003c0, 0x003d9,
			0x003db, 0x003dd, 0x003df, 0x003e1, 0x003e3, 0x003e5, 0x003e7, 0x003e9, 0x003eb, 0x003ed, 0x003ef, 0x003ba, 0x003c1, 0x003b8, 0x003b5, 0x003f8,
			0x003f2, 0x003fb, 0x0037b, 0x0037c, 0x0037d, 0x00450, 0x00451, 0x00452, 0x00453, 0x00454, 0x00455, 0x00456, 0x00457, 0x00458, 0x00459, 0x0045a,
			0x0045b, 0x0045c, 0x0045d, 0x0045e, 0x0045f, 0x00430, 0x00431, 0x00432, 0x00433, 0x00434, 0x00435, 0x00436, 0x00437, 0x00438, 0x00439, 0x0043a,
			0x0043b, 0x0043c, 0x0043d, 0x0043e, 0x0043f, 0x00440, 0x00441, 0x00442, 0x00443, 0x00444, 0x00445, 0x00446, 0x00447, 0x00448, 0x00449, 0x0044a,
			0x0044b, 0x0044c, 0x0044d, 0x0044e, 0x0044f, 0x00461, 0x00463, 0x00465, 0x00467, 0x00469, 0x0046b, 0x0046d, 0x0046f, 0x00471, 0x00473, 0x00475,
			0x00477, 0x00479, 0x0047b, 0x0047d, 0x0047f, 0x00481, 0x0048b, 0x0048d, 0x0048f, 0x00491, 0x00493, 0x00495, 0x00497, 0x00499, 0x0049b, 0x0049d,
			0x0049f, 0x004a1, 0x004a3, 0x004a5, 0x004a7, 0x004a9, 0x004ab, 0x004ad, 0x004af, 0x004b1, 0x004b3, 0x004b5, 0x004b7, 0x004b9, 0x004bb, 0x004bd,
			0x004bf, 0x004cf, 0x004c2, 0x004c4, 0x004c6, 0x004c8, 0x004ca, 0x004cc, 0x004ce, 0x004d1, 0x004d3, 0x004d5, 0x004d7, 0x004d9, 0x004db, 0x004dd,
			0x004df, 0x004e1, 0x004e3, 0x004e5, 0x004e7, 0x004e9, 0x004eb, 0x004ed, 0x004ef, 0x004f1, 0x004f3, 0x004f5, 0x004f7, 0x004f9, 0x004fb, 0x004fd,
			0x004ff, 0x00501, 0x00503, 0x00505, 0x00507, 0x00509, 0x0050b, 0x0050d, 0x0050f, 0x00511, 0x00513, 0x00515, 0x00517, 0x00519, 0x0051b, 0x0051d,
			0x0051f, 0x00521, 0x00523, 0x00525, 0x00527, 0x00529, 0x0052b, 0x0052d, 0x0052f, 0x00561, 0x00562, 0x00563, 0x00564, 0x00565, 0x00566, 0x00567,
			0x00568, 0x00569, 0x0056a, 0x0056b, 0x0056c, 0x0056d, 0x0056e, 0x0056f, 0x00570, 0x00571, 0x00572, 0x00573, 0x00574, 0x00575, 0x00576, 0x00577,
			0x00578, 0x00579, 0x0057a, 0x0057b, 0x0057c, 0x0057d, 0x0057e, 0x0057f, 0x00580, 0x00581, 0x00582, 0x00583, 0x00584, 0x00585, 0x00586, 0x02d00,
			0x02d01, 0x02d02, 0x02d03, 0x02d04, 0x02d05, 0x02d06, 0x02d07, 0x02d08, 0x02d09, 0x02d0a, 0x02d0b, 0x02d0c, 0x02d0d, 0x02d0e, 0x02d0f, 0x02d10,
			0x02d11, 0x02d12, 0x02d13, 0x02d14, 0x02d15, 0x02d16, 0x02d17, 0x02d18, 0x02d19, 0x02d1a, 0x02d1b, 0x02d1c, 0x02d1d, 0x02d1e, 0x02d1f, 0x02d20,
			0x02d21, 0x02d22, 0x02d23, 0x02d24, 0x02d25, 0x02d27, 0x02d2d, 0x013f0, 0x013f1, 0x013f2, 0x013f3, 0x013f4, 0x013f5, 0x00432, 0x00434, 0x0043e,
			0x00441, 0x00442, 0x00442, 0x0044a, 0x00463, 0x0a64b, 0x010d0, 0x010d1, 0x010d2, 0x010d3, 0x010d4, 0x010d5, 0x010d6, 0x010d7, 0x010d8, 0x010d9,
			0x010da, 0x010db, 0x010dc, 0x010dd, 0x010de, 0x010df, 0x010e0, 0x010e1, 0x010e2, 0x010e3, 0x010e4, 0x010e5, 0x010e6, 0x010e7, 0x010e8, 0x010e9,
			0x010ea, 0x010eb, 0x010ec, 0x010ed, 0x010ee, 0x010ef, 0x010f0, 0x010f1, 0x010f2, 0x010f3, 0x010f4, 0x010f5, 0x010f6, 0x010f7, 0x010f8, 0x010f9,
			0x010fa, 0x010fd, 0x010fe, 0x010ff, 0x01e01, 0x01e03, 0x01e05, 0x01e07, 0x01e09, 0x01e0b, 0x01e0d, 0x01e0f, 0x01e11, 0x01e13, 0x01e15, 0x01e17,
			0x01e19, 0x01e1b, 0x01e1d, 0x01e1f, 0x01e21, 0x01e23, 0x01e25, 0x01e27, 0x01e29, 0x01e2b, 0x01e2d, 0x01e2f, 0x01e31, 0x01e33, 0x01e35, 0x01e37,
			0x01e39, 0x01e3b, 0x01e3d, 0x01e3f, 0x01e41, 0x01e43, 0x01e45, 0x01e47, 0x01e49, 0x01e4b, 0x01e4d, 0x01e4f, 0x01e51, 0x01e53, 0x01e55, 0x01e57,
			0x01e59, 0x01e5b, 0x01e5d, 0x01e5f, 0x01e61, 0x01e63, 0x01e65, 0x01e67, 0x01e69, 0x01e6b, 0x01e6d, 0x01e6f, 0x01e71, 0x01e73, 0x01e75, 0x01e77,
			0x01e79, 0x01e7b, 0x01e7d, 0x01e7f, 0x01e81, 0x01e83, 0x01e85, 0x01e87, 0x01e89, 0x01e8b, 0x01e8d, 0x01e8f, 0x01e91, 0x01e93, 0x01e95, 0x01e61,
			0x000df, 0x01ea1, 0x01ea3, 0x01ea5, 0x01ea7, 0x01ea9, 0x01eab, 0x01ead, 0x01eaf, 0x01eb1, 0x01eb3, 0x01eb5, 0x01eb7, 0x01eb9, 0x01ebb, 0x01ebd,
			0x01ebf, 0x01ec1, 0x01ec3, 0x01ec5, 0x01ec7, 0x01ec9, 0x01ecb, 0x01ecd, 0x01ecf, 0x01ed1, 0x01ed3, 0x01ed5, 0x01ed7, 0x01ed9, 0x01edb, 0x01edd,
			0x01edf, 0x01ee1, 0x01ee3, 0x01ee5, 0x01ee7, 0x01ee9, 0x01eeb, 0x01eed, 0x01eef, 0x01ef1, 0x01ef3, 0x01ef5, 0x01ef7, 0x01ef9, 0x01efb, 0x01efd,
			0x01eff, 0x01f00, 0x01f01, 0x01f02, 0x01f03, 0x01f04, 0x01f05, 0x01f06, 0x01f07, 0x01f10, 0x01f11, 0x01f12, 0x01f13, 0x01f14, 0x01f15, 0x01f20,
			0x01f21, 0x01f22, 0x01f23, 0x01f24, 0x01f25, 0x01f26, 0x01f27, 0x01f30, 0x01f31, 0x01f32, 0x01f33, 0x01f34, 0x01f35, 0x01f36, 0x01f37, 0x01f40,
			0x01f41, 0x01f42, 0x01f43, 0x01f44, 0x01f45, 0x01f51, 0x01f53, 0x01f55, 0x01f57, 0x01f60, 0x01f61, 0x01f62, 0x01f63, 0x01f64, 0x01f65, 0x01f66,
			0x01f67, 0x01f80, 0x01f81, 0x01f82, 0x01f83, 0x01f84, 0x01f85, 0x01f86, 0x01f87, 0x01f90, 0x01f91, 0x01f92, 0x01f93, 0x01f94, 0x01f95, 0x01f96,
			0x01f97, 0x01fa0, 0x01fa1, 0x01fa2, 0x01fa3, 0x01fa4, 0x01fa5, 0x01fa6, 0x01fa7, 0x01fb0, 0x01fb1, 0x01f70, 0x01f71, 0x01fb3, 0x003b9, 0x01f72,
			0x01f73, 0x01f74, 0x01f75, 0x01fc3, 0x01fd0, 0x01fd1, 0x01f76, 0x01f77, 0x01fe0, 0x01fe1, 0x01f7a, 0x01f7b, 0x01fe5, 0x01f78, 0x01f79, 0x01f7c,
			0x01f7d, 0x01ff3, 0x003c9, 0x0006b, 0x000e5, 0x0214e, 0x02170, 0x02171, 0x02172, 0x02173, 0x02174, 0x02175, 0x02176, 0x02177, 0x02178, 0x02179,
			0x0217a, 0x0217b, 0x0217c, 0x0217d, 0x0217e, 0x0217f, 0x02184, 0x024d0, 0x024d1, 0x024d2, 0x024d3, 0x024d4, 0x024d5, 0x024d6, 0x024d7, 0x024d8,
			0x024d9, 0x024da, 0x024db, 0x024dc, 0x024dd, 0x024de, 0x024df, 0x024e0, 0x024e1, 0x024e2, 0x024e3, 0x024e4, 0x024e5, 0x024e6, 0x024e7, 0x024e8,
			0x024e9, 0x02c30, 0x02c31, 0x02c32, 0x02c33, 0x02c34, 0x02c35, 0x02c36, 0x02c37, 0x02c38, 0x02c39, 0x02c3a, 0x02c3b, 0x02c3c, 0x02c3d, 0x02c3e,
			0x02c3f, 0x02c40, 0x02c41, 0x02c42, 0x02c43, 0x02c44, 0x02c45, 0x02c46, 0x02c47, 0x02c48, 0x02c49, 0x02c4a, 0x02c4b, 0x02c4c, 0x02c4d, 0x02c4e,
			0x02c4f, 0x02c50, 0x02c51, 0x02c52, 0x02c53, 0x02c54, 0x02c55, 0x02c56, 0x02c57, 0x02c58, 0x02c59, 0x02c5a, 0x02c5b, 0x02c5c, 0x02c5d, 0x02c5e,
			0x02c61, 0x0026b, 0x01d7d, 0x0027d, 0x02c68, 0x02c6a, 0x02c6c, 0x00251, 0x00271, 0x00250, 0x00252, 0x02c73, 0x02c76, 0x0023f, 0x00240, 0x02c81,
			0x02c83, 0x02c85, 0x02c87, 0x02c89, 0x02c8b, 0x02c8d, 0x02c8f, 0x02c91, 0x02c93, 0x02c95, 0x02c97, 0x02c99, 0x02c9b, 0x02c9d, 0x02c9f, 0x02ca1,
			0x02ca3, 0x02ca5, 0x02ca7, 0x02ca9, 0x02cab, 0x02cad, 0x02caf, 0x02cb1, 0x02cb3, 0x02cb5, 0x02cb7, 0x02cb9, 0x02cbb, 0x02cbd, 0x02cbf, 0x02cc1,
			0x02cc3, 0x02cc5, 0x02cc7, 0x02cc9, 0x02ccb, 0x02ccd, 0x02ccf, 0x02cd1, 0x02cd3, 0x02cd5, 0x02cd7, 0x02cd9, 0x02cdb, 0x02cdd, 0x02cdf, 0x02ce1,
			0x02ce3, 0x02cec, 0x02cee, 0x02cf3, 0x0a641, 0x0a643, 0x0a645, 0x0a647, 0x0a649, 0x0a64b, 0x0a64d, 0x0a64f, 0x0a651, 0x0a653, 0x0a655, 0x0a657,
			0x0a659, 0x0a65b, 0x0a65d, 0x0a65f, 0x0a661, 0x0a663, 0x0a665, 0x0a667, 0x0a669, 0x0a66b, 0x0a66d, 0x0a681, 0x0a683, 0x0a685, 0x0a687, 0x0a689,
			0x0a68b, 0x0a68d, 0x0a68f, 0x0a691, 0x0a693, 0x0a695, 0x0a697, 0x0a699, 0x0a69b, 0x0a723, 0x0a725, 0x0a727, 0x0a729, 0x0a72b, 0x0a72d, 0x0a72f,
			0x0a733, 0x0a735, 0x0a737, 0x0a739, 0x0a73b, 0x0a73d, 0x0a73f, 0x0a741, 0x0a743, 0x0a745, 0x0a747, 0x0a749, 0x0a74b, 0x0a74d, 0x0a74f, 0x0a751,
			0x0a753, 0x0a755, 0x0a757, 0x0a759, 0x0a75b, 0x0a75d, 0x0a75f, 0x0a761, 0x0a763, 0x0a765, 0x0a767, 0x0a769, 0x0a76b, 0x0a76d, 0x0a76f, 0x0a77a,
			0x0a77c, 0x01d79, 0x0a77f, 0x0a781, 0x0a783, 0x0a785, 0x0a787, 0x0a78c, 0x00265, 0x0a791, 0x0a793, 0x0a797, 0x0a799, 0x0a79b, 0x0a79d, 0x0a79f,
			0x0a7a1, 0x0a7a3, 0x0a7a5, 0x0a7a7, 0x0a7a9, 0x00266, 0x0025c, 0x00261, 0x0026c, 0x0026a, 0x0029e, 0x00287, 0x0029d, 0x0ab53, 0x0a7b5, 0x0a7b7,
			0x0a7b9, 0x0a7bb, 0x0a7bd, 0x0a7bf, 0x0a7c3, 0x0a794, 0x00282, 0x01d8e, 0x013a0, 0x013a1, 0x013a2, 0x013a3, 0x013a4, 0x013a5, 0x013a6, 0x013a7,
			0x013a8, 0x013a9, 0x013aa, 0x013ab, 0x013ac, 0x013ad, 0x013ae, 0x013af, 0x013b0, 0x013b1, 0x013b2, 0x013b3, 0x013b4, 0x013b5, 0x013b6, 0x013b7,
			0x013b8, 0x013b9, 0x013ba, 0x013bb, 0x013bc, 0x013bd, 0x013be, 0x013bf, 0x013c0, 0x013c1, 0x013c2, 0x013c3, 0x013c4, 0x013c5, 0x013c6, 0x013c7,
			0x013c8, 0x013c9, 0x013ca, 0x013cb, 0x013cc, 0x013cd, 0x013ce, 0x013cf, 0x013d0, 0x013d1, 0x013d2, 0x013d3, 0x013d4, 0x013d5, 0x013d6, 0x013d7,
			0x013d8, 0x013d9, 0x013da, 0x013db, 0x013dc, 0x013dd, 0x013de, 0x013df, 0x013e0, 0x013e1, 0x013e2, 0x013e3, 0x013e4, 0x013e5, 0x013e6, 0x013e7,
			0x013e8, 0x013e9, 0x013ea, 0x013eb, 0x013ec, 0x013ed, 0x013ee, 0x013ef, 0x0ff41, 0x0ff42, 0x0ff43, 0x0ff44, 0x0ff45, 0x0ff46, 0x0ff47, 0x0ff48,
			0x0ff49, 0x0ff4a, 0x0ff4b, 0x0ff4c, 0x0ff4d, 0x0ff4e, 0x0ff4f, 0x0ff50, 0x0ff51, 0x0ff52, 0x0ff53, 0x0ff54, 0x0ff55, 0x0ff56, 0x0ff57, 0x0ff58,
			0x0ff59, 0x0ff5a, 0x10428, 0x10429, 0x1042a, 0x1042b, 0x1042c, 0x1042d, 0x1042e, 0x1042f, 0x10430, 0x10431, 0x10432, 0x10433, 0x10434, 0x10435,
			0x10436, 0x10437, 0x10438, 0x10439, 0x1043a, 0x1043b, 0x1043c, 0x1043d, 0x1043e, 0x1043f, 0x10440, 0x10441, 0x10442, 0x10443, 0x10444, 0x10445,
			0x10446, 0x10447, 0x10448, 0x10449, 0x1044a, 0x1044b, 0x1044c, 0x1044d, 0x1044e, 0x1044f, 0x104d8, 0x104d9, 0x104da, 0x104db, 0x104dc, 0x104dd,
			0x104de, 0x104df, 0x104e0, 0x104e1, 0x104e2, 0x104e3, 0x104e4, 0x104e5, 0x104e6, 0x104e7, 0x104e8, 0x104e9, 0x104ea, 0x104eb, 0x104ec, 0x104ed,
			0x104ee, 0x104ef, 0x104f0, 0x104f1, 0x104f2, 0x104f3, 0x104f4, 0x104f5, 0x104f6, 0x104f7, 0x104f8, 0x104f9, 0x104fa, 0x104fb, 0x10cc0, 0x10cc1,
			0x10cc2, 0x10cc3, 0x10cc4, 0x10cc5, 0x10cc6, 0x10cc7, 0x10cc8, 0x10cc9, 0x10cca, 0x10ccb, 0x10ccc, 0x10ccd, 0x10cce, 0x10ccf, 0x10cd0, 0x10cd1,
			0x10cd2, 0x10cd3, 0x10cd4, 0x10cd5, 0x10cd6, 0x10cd7, 0x10cd8, 0x10cd9, 0x10cda, 0x10cdb, 0x10cdc, 0x10cdd, 0x10cde, 0x10cdf, 0x10ce0, 0x10ce1,
			0x10ce2, 0x10ce3, 0x10ce4, 0x10ce5, 0x10ce6, 0x10ce7, 0x10ce8, 0x10ce9, 0x10cea, 0x10ceb, 0x10cec, 0x10ced, 0x10cee, 0x10cef, 0x10cf0, 0x10cf1,
			0x10cf2, 0x118c0, 0x118c1, 0x118c2, 0x118c3, 0x118c4, 0x118c5, 0x118c6, 0x118c7, 0x118c8, 0x118c9, 0x118ca, 0x118cb, 0x118cc, 0x118cd, 0x118ce,
			0x118cf, 0x118d0, 0x118d1, 0x118d2, 0x118d3, 0x118d4, 0x118d5, 0x118d6, 0x118d7, 0x118d8, 0x118d9, 0x118da, 0x118db, 0x118dc, 0x118dd, 0x118de,
			0x118df, 0x16e60, 0x16e61, 0x16e62, 0x16e63, 0x16e64, 0x16e65, 0x16e66, 0x16e67, 0x16e68, 0x16e69, 0x16e6a, 0x16e6b, 0x16e6c, 0x16e6d, 0x16e6e,
			0x16e6f, 0x16e70, 0x16e71, 0x16e72, 0x16e73, 0x16e74, 0x16e75, 0x16e76, 0x16e77, 0x16e78, 0x16e79, 0x16e7a, 0x16e7b, 0x16e7c, 0x16e7d, 0x16e7e,
			0x16e7f, 0x1e922, 0x1e923, 0x1e924, 0x1e925, 0x1e926, 0x1e927, 0x1e928, 0x1e929, 0x1e92a, 0x1e92b, 0x1e92c, 0x1e92d, 0x1e92e, 0x1e92f, 0x1e930,
			0x1e931, 0x1e932, 0x1e933, 0x1e934, 0x1e935, 0x1e936, 0x1e937, 0x1e938, 0x1e939, 0x1e93a, 0x1e93b, 0x1e93c, 0x1e93d, 0x1e93e, 0x1e93f, 0x1e940,
			0x1e941, 0x1e942, 0x1e943
		};

		Array<char> output;
		std::size_t stringLength = string.size();
		std::size_t idx = 0;

		arrayReserve(output, stringLength + 1);

		do {
			auto [c, nextIdx] = Utf8::NextChar(string, idx);

			const char32_t* f = std::lower_bound(u2l, u2l + arraySize(u2l), c);
			if (f != u2l + arraySize(u2l) && *f == c) {
				c = lc[f - u2l];
			}

			if (c < 0x7f) {
				arrayAppend(output, (char)c);
			} else if (c < 0x7ff) {
				arrayAppend(output, 0xC0 | (c >> 6));
				arrayAppend(output, 0x80 | (c & 0x3f));
			} else if (c < 0xFFFF) {
				arrayAppend(output, 0xE0 | (c >> 12));
				arrayAppend(output, 0x80 | ((c >> 6) & 0x3f));
				arrayAppend(output, 0x80 | (c & 0x3f));
			} else {
				arrayAppend(output, 0xF0 | (c >> 18));
				arrayAppend(output, 0x80 | ((c >> 12) & 0x3f));
				arrayAppend(output, 0x80 | ((c >> 6) & 0x3f));
				arrayAppend(output, 0x80 | (c & 0x3f));
			}

			idx = nextIdx;
		} while (idx < stringLength);

		arrayAppend(output, '\0');
		const std::size_t size = output.size();
		// This assumes that the growable array uses std::malloc() (which has to be std::free()'d later) in order to be
		// able to std::realloc(). The deleter doesn't use the size argument so it should be fine to transfer it over
		// to a String with the size excluding the null terminator.
		void(*const deleter)(char*, std::size_t) = output.deleter();
		DEATH_DEBUG_ASSERT(deleter, "Invalid deleter used", {});
		return String{output.release(), size - 1, deleter};
	}

	String uppercaseUnicode(StringView string) {
		static const char32_t l2u[1384] = {
			0x00061, 0x00062, 0x00063, 0x00064, 0x00065, 0x00066, 0x00067, 0x00068, 0x00069, 0x0006a, 0x0006b, 0x0006c, 0x0006d, 0x0006e, 0x0006f, 0x00070,
			0x00071, 0x00072, 0x00073, 0x00074, 0x00075, 0x00076, 0x00077, 0x00078, 0x00079, 0x0007a, 0x000df, 0x000e0, 0x000e1, 0x000e2, 0x000e3, 0x000e4,
			0x000e5, 0x000e6, 0x000e7, 0x000e8, 0x000e9, 0x000ea, 0x000eb, 0x000ec, 0x000ed, 0x000ee, 0x000ef, 0x000f0, 0x000f1, 0x000f2, 0x000f3, 0x000f4,
			0x000f5, 0x000f6, 0x000f8, 0x000f9, 0x000fa, 0x000fb, 0x000fc, 0x000fd, 0x000fe, 0x000ff, 0x00101, 0x00103, 0x00105, 0x00107, 0x00109, 0x0010b,
			0x0010d, 0x0010f, 0x00111, 0x00113, 0x00115, 0x00117, 0x00119, 0x0011b, 0x0011d, 0x0011f, 0x00121, 0x00123, 0x00125, 0x00127, 0x00129, 0x0012b,
			0x0012d, 0x0012f, 0x00133, 0x00135, 0x00137, 0x0013a, 0x0013c, 0x0013e, 0x00140, 0x00142, 0x00144, 0x00146, 0x00148, 0x0014b, 0x0014d, 0x0014f,
			0x00151, 0x00153, 0x00155, 0x00157, 0x00159, 0x0015b, 0x0015d, 0x0015f, 0x00161, 0x00163, 0x00165, 0x00167, 0x00169, 0x0016b, 0x0016d, 0x0016f,
			0x00171, 0x00173, 0x00175, 0x00177, 0x0017a, 0x0017c, 0x0017e, 0x00180, 0x00183, 0x00185, 0x00188, 0x0018c, 0x00192, 0x00195, 0x00199, 0x0019a,
			0x0019e, 0x001a1, 0x001a3, 0x001a5, 0x001a8, 0x001ad, 0x001b0, 0x001b4, 0x001b6, 0x001b9, 0x001bd, 0x001bf, 0x001c6, 0x001c9, 0x001cc, 0x001ce,
			0x001d0, 0x001d2, 0x001d4, 0x001d6, 0x001d8, 0x001da, 0x001dc, 0x001dd, 0x001df, 0x001e1, 0x001e3, 0x001e5, 0x001e7, 0x001e9, 0x001eb, 0x001ed,
			0x001ef, 0x001f3, 0x001f5, 0x001f9, 0x001fb, 0x001fd, 0x001ff, 0x00201, 0x00203, 0x00205, 0x00207, 0x00209, 0x0020b, 0x0020d, 0x0020f, 0x00211,
			0x00213, 0x00215, 0x00217, 0x00219, 0x0021b, 0x0021d, 0x0021f, 0x00223, 0x00225, 0x00227, 0x00229, 0x0022b, 0x0022d, 0x0022f, 0x00231, 0x00233,
			0x0023c, 0x0023f, 0x00240, 0x00242, 0x00247, 0x00249, 0x0024b, 0x0024d, 0x0024f, 0x00250, 0x00251, 0x00252, 0x00253, 0x00254, 0x00256, 0x00257,
			0x00259, 0x0025b, 0x0025c, 0x00260, 0x00261, 0x00263, 0x00265, 0x00266, 0x00268, 0x00269, 0x0026a, 0x0026b, 0x0026c, 0x0026f, 0x00271, 0x00272,
			0x00275, 0x0027d, 0x00280, 0x00282, 0x00283, 0x00287, 0x00288, 0x00289, 0x0028a, 0x0028b, 0x0028c, 0x00292, 0x0029d, 0x0029e, 0x00371, 0x00373,
			0x00377, 0x0037b, 0x0037c, 0x0037d, 0x003ac, 0x003ad, 0x003ae, 0x003af, 0x003b1, 0x003b2, 0x003b3, 0x003b4, 0x003b5, 0x003b6, 0x003b7, 0x003b8,
			0x003b8, 0x003b9, 0x003b9, 0x003ba, 0x003bb, 0x003bc, 0x003bd, 0x003be, 0x003bf, 0x003c0, 0x003c1, 0x003c3, 0x003c4, 0x003c5, 0x003c6, 0x003c7,
			0x003c8, 0x003c9, 0x003ca, 0x003cb, 0x003cc, 0x003cd, 0x003ce, 0x003d7, 0x003d9, 0x003db, 0x003dd, 0x003df, 0x003e1, 0x003e3, 0x003e5, 0x003e7,
			0x003e9, 0x003eb, 0x003ed, 0x003ef, 0x003f2, 0x003f3, 0x003f8, 0x003fb, 0x00430, 0x00431, 0x00432, 0x00433, 0x00434, 0x00435, 0x00436, 0x00437,
			0x00438, 0x00439, 0x0043a, 0x0043b, 0x0043c, 0x0043d, 0x0043e, 0x0043f, 0x00440, 0x00441, 0x00442, 0x00442, 0x00443, 0x00444, 0x00445, 0x00446,
			0x00447, 0x00448, 0x00449, 0x0044a, 0x0044b, 0x0044c, 0x0044d, 0x0044e, 0x0044f, 0x00450, 0x00451, 0x00452, 0x00453, 0x00454, 0x00455, 0x00456,
			0x00457, 0x00458, 0x00459, 0x0045a, 0x0045b, 0x0045c, 0x0045d, 0x0045e, 0x0045f, 0x00461, 0x00463, 0x00465, 0x00467, 0x00469, 0x0046b, 0x0046d,
			0x0046f, 0x00471, 0x00473, 0x00475, 0x00477, 0x00479, 0x0047b, 0x0047d, 0x0047f, 0x00481, 0x0048b, 0x0048d, 0x0048f, 0x00491, 0x00493, 0x00495,
			0x00497, 0x00499, 0x0049b, 0x0049d, 0x0049f, 0x004a1, 0x004a3, 0x004a5, 0x004a7, 0x004a9, 0x004ab, 0x004ad, 0x004af, 0x004b1, 0x004b3, 0x004b5,
			0x004b7, 0x004b9, 0x004bb, 0x004bd, 0x004bf, 0x004c2, 0x004c4, 0x004c6, 0x004c8, 0x004ca, 0x004cc, 0x004ce, 0x004cf, 0x004d1, 0x004d3, 0x004d5,
			0x004d7, 0x004d9, 0x004db, 0x004dd, 0x004df, 0x004e1, 0x004e3, 0x004e5, 0x004e7, 0x004e9, 0x004eb, 0x004ed, 0x004ef, 0x004f1, 0x004f3, 0x004f5,
			0x004f7, 0x004f9, 0x004fb, 0x004fd, 0x004ff, 0x00501, 0x00503, 0x00505, 0x00507, 0x00509, 0x0050b, 0x0050d, 0x0050f, 0x00511, 0x00513, 0x00515,
			0x00517, 0x00519, 0x0051b, 0x0051d, 0x0051f, 0x00521, 0x00523, 0x00525, 0x00527, 0x00529, 0x0052b, 0x0052d, 0x0052f, 0x00561, 0x00562, 0x00563,
			0x00564, 0x00565, 0x00566, 0x00567, 0x00568, 0x00569, 0x0056a, 0x0056b, 0x0056c, 0x0056d, 0x0056e, 0x0056f, 0x00570, 0x00571, 0x00572, 0x00573,
			0x00574, 0x00575, 0x00576, 0x00577, 0x00578, 0x00579, 0x0057a, 0x0057b, 0x0057c, 0x0057d, 0x0057e, 0x0057f, 0x00580, 0x00581, 0x00582, 0x00583,
			0x00584, 0x00585, 0x00586, 0x010d0, 0x010d1, 0x010d2, 0x010d3, 0x010d4, 0x010d5, 0x010d6, 0x010d7, 0x010d8, 0x010d9, 0x010da, 0x010db, 0x010dc,
			0x010dd, 0x010de, 0x010df, 0x010e0, 0x010e1, 0x010e2, 0x010e3, 0x010e4, 0x010e5, 0x010e6, 0x010e7, 0x010e8, 0x010e9, 0x010ea, 0x010eb, 0x010ec,
			0x010ed, 0x010ee, 0x010ef, 0x010f0, 0x010f1, 0x010f2, 0x010f3, 0x010f4, 0x010f5, 0x010f6, 0x010f7, 0x010f8, 0x010f9, 0x010fa, 0x010fd, 0x010fe,
			0x010ff, 0x013a0, 0x013a1, 0x013a2, 0x013a3, 0x013a4, 0x013a5, 0x013a6, 0x013a7, 0x013a8, 0x013a9, 0x013aa, 0x013ab, 0x013ac, 0x013ad, 0x013ae,
			0x013af, 0x013b0, 0x013b1, 0x013b2, 0x013b3, 0x013b4, 0x013b5, 0x013b6, 0x013b7, 0x013b8, 0x013b9, 0x013ba, 0x013bb, 0x013bc, 0x013bd, 0x013be,
			0x013bf, 0x013c0, 0x013c1, 0x013c2, 0x013c3, 0x013c4, 0x013c5, 0x013c6, 0x013c7, 0x013c8, 0x013c9, 0x013ca, 0x013cb, 0x013cc, 0x013cd, 0x013ce,
			0x013cf, 0x013d0, 0x013d1, 0x013d2, 0x013d3, 0x013d4, 0x013d5, 0x013d6, 0x013d7, 0x013d8, 0x013d9, 0x013da, 0x013db, 0x013dc, 0x013dd, 0x013de,
			0x013df, 0x013e0, 0x013e1, 0x013e2, 0x013e3, 0x013e4, 0x013e5, 0x013e6, 0x013e7, 0x013e8, 0x013e9, 0x013ea, 0x013eb, 0x013ec, 0x013ed, 0x013ee,
			0x013ef, 0x013f0, 0x013f1, 0x013f2, 0x013f3, 0x013f4, 0x013f5, 0x01d79, 0x01d7d, 0x01d8e, 0x01e01, 0x01e03, 0x01e05, 0x01e07, 0x01e09, 0x01e0b,
			0x01e0d, 0x01e0f, 0x01e11, 0x01e13, 0x01e15, 0x01e17, 0x01e19, 0x01e1b, 0x01e1d, 0x01e1f, 0x01e21, 0x01e23, 0x01e25, 0x01e27, 0x01e29, 0x01e2b,
			0x01e2d, 0x01e2f, 0x01e31, 0x01e33, 0x01e35, 0x01e37, 0x01e39, 0x01e3b, 0x01e3d, 0x01e3f, 0x01e41, 0x01e43, 0x01e45, 0x01e47, 0x01e49, 0x01e4b,
			0x01e4d, 0x01e4f, 0x01e51, 0x01e53, 0x01e55, 0x01e57, 0x01e59, 0x01e5b, 0x01e5d, 0x01e5f, 0x01e61, 0x01e63, 0x01e65, 0x01e67, 0x01e69, 0x01e6b,
			0x01e6d, 0x01e6f, 0x01e71, 0x01e73, 0x01e75, 0x01e77, 0x01e79, 0x01e7b, 0x01e7d, 0x01e7f, 0x01e81, 0x01e83, 0x01e85, 0x01e87, 0x01e89, 0x01e8b,
			0x01e8d, 0x01e8f, 0x01e91, 0x01e93, 0x01e95, 0x01ea1, 0x01ea3, 0x01ea5, 0x01ea7, 0x01ea9, 0x01eab, 0x01ead, 0x01eaf, 0x01eb1, 0x01eb3, 0x01eb5,
			0x01eb7, 0x01eb9, 0x01ebb, 0x01ebd, 0x01ebf, 0x01ec1, 0x01ec3, 0x01ec5, 0x01ec7, 0x01ec9, 0x01ecb, 0x01ecd, 0x01ecf, 0x01ed1, 0x01ed3, 0x01ed5,
			0x01ed7, 0x01ed9, 0x01edb, 0x01edd, 0x01edf, 0x01ee1, 0x01ee3, 0x01ee5, 0x01ee7, 0x01ee9, 0x01eeb, 0x01eed, 0x01eef, 0x01ef1, 0x01ef3, 0x01ef5,
			0x01ef7, 0x01ef9, 0x01efb, 0x01efd, 0x01eff, 0x01f00, 0x01f01, 0x01f02, 0x01f03, 0x01f04, 0x01f05, 0x01f06, 0x01f07, 0x01f10, 0x01f11, 0x01f12,
			0x01f13, 0x01f14, 0x01f15, 0x01f20, 0x01f21, 0x01f22, 0x01f23, 0x01f24, 0x01f25, 0x01f26, 0x01f27, 0x01f30, 0x01f31, 0x01f32, 0x01f33, 0x01f34,
			0x01f35, 0x01f36, 0x01f37, 0x01f40, 0x01f41, 0x01f42, 0x01f43, 0x01f44, 0x01f45, 0x01f51, 0x01f53, 0x01f55, 0x01f57, 0x01f60, 0x01f61, 0x01f62,
			0x01f63, 0x01f64, 0x01f65, 0x01f66, 0x01f67, 0x01f70, 0x01f71, 0x01f72, 0x01f73, 0x01f74, 0x01f75, 0x01f76, 0x01f77, 0x01f78, 0x01f79, 0x01f7a,
			0x01f7b, 0x01f7c, 0x01f7d, 0x01f80, 0x01f81, 0x01f82, 0x01f83, 0x01f84, 0x01f85, 0x01f86, 0x01f87, 0x01f90, 0x01f91, 0x01f92, 0x01f93, 0x01f94,
			0x01f95, 0x01f96, 0x01f97, 0x01fa0, 0x01fa1, 0x01fa2, 0x01fa3, 0x01fa4, 0x01fa5, 0x01fa6, 0x01fa7, 0x01fb0, 0x01fb1, 0x01fb3, 0x01fc3, 0x01fd0,
			0x01fd1, 0x01fe0, 0x01fe1, 0x01fe5, 0x01ff3, 0x0214e, 0x02170, 0x02171, 0x02172, 0x02173, 0x02174, 0x02175, 0x02176, 0x02177, 0x02178, 0x02179,
			0x0217a, 0x0217b, 0x0217c, 0x0217d, 0x0217e, 0x0217f, 0x02184, 0x024d0, 0x024d1, 0x024d2, 0x024d3, 0x024d4, 0x024d5, 0x024d6, 0x024d7, 0x024d8,
			0x024d9, 0x024da, 0x024db, 0x024dc, 0x024dd, 0x024de, 0x024df, 0x024e0, 0x024e1, 0x024e2, 0x024e3, 0x024e4, 0x024e5, 0x024e6, 0x024e7, 0x024e8,
			0x024e9, 0x02c30, 0x02c31, 0x02c32, 0x02c33, 0x02c34, 0x02c35, 0x02c36, 0x02c37, 0x02c38, 0x02c39, 0x02c3a, 0x02c3b, 0x02c3c, 0x02c3d, 0x02c3e,
			0x02c3f, 0x02c40, 0x02c41, 0x02c42, 0x02c43, 0x02c44, 0x02c45, 0x02c46, 0x02c47, 0x02c48, 0x02c49, 0x02c4a, 0x02c4b, 0x02c4c, 0x02c4d, 0x02c4e,
			0x02c4f, 0x02c50, 0x02c51, 0x02c52, 0x02c53, 0x02c54, 0x02c55, 0x02c56, 0x02c57, 0x02c58, 0x02c59, 0x02c5a, 0x02c5b, 0x02c5c, 0x02c5d, 0x02c5e,
			0x02c61, 0x02c65, 0x02c66, 0x02c68, 0x02c6a, 0x02c6c, 0x02c73, 0x02c76, 0x02c81, 0x02c83, 0x02c85, 0x02c87, 0x02c89, 0x02c8b, 0x02c8d, 0x02c8f,
			0x02c91, 0x02c93, 0x02c95, 0x02c97, 0x02c99, 0x02c9b, 0x02c9d, 0x02c9f, 0x02ca1, 0x02ca3, 0x02ca5, 0x02ca7, 0x02ca9, 0x02cab, 0x02cad, 0x02caf,
			0x02cb1, 0x02cb3, 0x02cb5, 0x02cb7, 0x02cb9, 0x02cbb, 0x02cbd, 0x02cbf, 0x02cc1, 0x02cc3, 0x02cc5, 0x02cc7, 0x02cc9, 0x02ccb, 0x02ccd, 0x02ccf,
			0x02cd1, 0x02cd3, 0x02cd5, 0x02cd7, 0x02cd9, 0x02cdb, 0x02cdd, 0x02cdf, 0x02ce1, 0x02ce3, 0x02cec, 0x02cee, 0x02cf3, 0x02d00, 0x02d01, 0x02d02,
			0x02d03, 0x02d04, 0x02d05, 0x02d06, 0x02d07, 0x02d08, 0x02d09, 0x02d0a, 0x02d0b, 0x02d0c, 0x02d0d, 0x02d0e, 0x02d0f, 0x02d10, 0x02d11, 0x02d12,
			0x02d13, 0x02d14, 0x02d15, 0x02d16, 0x02d17, 0x02d18, 0x02d19, 0x02d1a, 0x02d1b, 0x02d1c, 0x02d1d, 0x02d1e, 0x02d1f, 0x02d20, 0x02d21, 0x02d22,
			0x02d23, 0x02d24, 0x02d25, 0x02d27, 0x02d2d, 0x0a641, 0x0a643, 0x0a645, 0x0a647, 0x0a649, 0x0a64b, 0x0a64d, 0x0a64f, 0x0a651, 0x0a653, 0x0a655,
			0x0a657, 0x0a659, 0x0a65b, 0x0a65d, 0x0a65f, 0x0a661, 0x0a663, 0x0a665, 0x0a667, 0x0a669, 0x0a66b, 0x0a66d, 0x0a681, 0x0a683, 0x0a685, 0x0a687,
			0x0a689, 0x0a68b, 0x0a68d, 0x0a68f, 0x0a691, 0x0a693, 0x0a695, 0x0a697, 0x0a699, 0x0a69b, 0x0a723, 0x0a725, 0x0a727, 0x0a729, 0x0a72b, 0x0a72d,
			0x0a72f, 0x0a733, 0x0a735, 0x0a737, 0x0a739, 0x0a73b, 0x0a73d, 0x0a73f, 0x0a741, 0x0a743, 0x0a745, 0x0a747, 0x0a749, 0x0a74b, 0x0a74d, 0x0a74f,
			0x0a751, 0x0a753, 0x0a755, 0x0a757, 0x0a759, 0x0a75b, 0x0a75d, 0x0a75f, 0x0a761, 0x0a763, 0x0a765, 0x0a767, 0x0a769, 0x0a76b, 0x0a76d, 0x0a76f,
			0x0a77a, 0x0a77c, 0x0a77f, 0x0a781, 0x0a783, 0x0a785, 0x0a787, 0x0a78c, 0x0a791, 0x0a793, 0x0a794, 0x0a797, 0x0a799, 0x0a79b, 0x0a79d, 0x0a79f,
			0x0a7a1, 0x0a7a3, 0x0a7a5, 0x0a7a7, 0x0a7a9, 0x0a7b5, 0x0a7b7, 0x0a7b9, 0x0a7bb, 0x0a7bd, 0x0a7bf, 0x0a7c3, 0x0ab53, 0x0ff41, 0x0ff42, 0x0ff43,
			0x0ff44, 0x0ff45, 0x0ff46, 0x0ff47, 0x0ff48, 0x0ff49, 0x0ff4a, 0x0ff4b, 0x0ff4c, 0x0ff4d, 0x0ff4e, 0x0ff4f, 0x0ff50, 0x0ff51, 0x0ff52, 0x0ff53,
			0x0ff54, 0x0ff55, 0x0ff56, 0x0ff57, 0x0ff58, 0x0ff59, 0x0ff5a, 0x10428, 0x10429, 0x1042a, 0x1042b, 0x1042c, 0x1042d, 0x1042e, 0x1042f, 0x10430,
			0x10431, 0x10432, 0x10433, 0x10434, 0x10435, 0x10436, 0x10437, 0x10438, 0x10439, 0x1043a, 0x1043b, 0x1043c, 0x1043d, 0x1043e, 0x1043f, 0x10440,
			0x10441, 0x10442, 0x10443, 0x10444, 0x10445, 0x10446, 0x10447, 0x10448, 0x10449, 0x1044a, 0x1044b, 0x1044c, 0x1044d, 0x1044e, 0x1044f, 0x104d8,
			0x104d9, 0x104da, 0x104db, 0x104dc, 0x104dd, 0x104de, 0x104df, 0x104e0, 0x104e1, 0x104e2, 0x104e3, 0x104e4, 0x104e5, 0x104e6, 0x104e7, 0x104e8,
			0x104e9, 0x104ea, 0x104eb, 0x104ec, 0x104ed, 0x104ee, 0x104ef, 0x104f0, 0x104f1, 0x104f2, 0x104f3, 0x104f4, 0x104f5, 0x104f6, 0x104f7, 0x104f8,
			0x104f9, 0x104fa, 0x104fb, 0x10cc0, 0x10cc1, 0x10cc2, 0x10cc3, 0x10cc4, 0x10cc5, 0x10cc6, 0x10cc7, 0x10cc8, 0x10cc9, 0x10cca, 0x10ccb, 0x10ccc,
			0x10ccd, 0x10cce, 0x10ccf, 0x10cd0, 0x10cd1, 0x10cd2, 0x10cd3, 0x10cd4, 0x10cd5, 0x10cd6, 0x10cd7, 0x10cd8, 0x10cd9, 0x10cda, 0x10cdb, 0x10cdc,
			0x10cdd, 0x10cde, 0x10cdf, 0x10ce0, 0x10ce1, 0x10ce2, 0x10ce3, 0x10ce4, 0x10ce5, 0x10ce6, 0x10ce7, 0x10ce8, 0x10ce9, 0x10cea, 0x10ceb, 0x10cec,
			0x10ced, 0x10cee, 0x10cef, 0x10cf0, 0x10cf1, 0x10cf2, 0x118c0, 0x118c1, 0x118c2, 0x118c3, 0x118c4, 0x118c5, 0x118c6, 0x118c7, 0x118c8, 0x118c9,
			0x118ca, 0x118cb, 0x118cc, 0x118cd, 0x118ce, 0x118cf, 0x118d0, 0x118d1, 0x118d2, 0x118d3, 0x118d4, 0x118d5, 0x118d6, 0x118d7, 0x118d8, 0x118d9,
			0x118da, 0x118db, 0x118dc, 0x118dd, 0x118de, 0x118df, 0x16e60, 0x16e61, 0x16e62, 0x16e63, 0x16e64, 0x16e65, 0x16e66, 0x16e67, 0x16e68, 0x16e69,
			0x16e6a, 0x16e6b, 0x16e6c, 0x16e6d, 0x16e6e, 0x16e6f, 0x16e70, 0x16e71, 0x16e72, 0x16e73, 0x16e74, 0x16e75, 0x16e76, 0x16e77, 0x16e78, 0x16e79,
			0x16e7a, 0x16e7b, 0x16e7c, 0x16e7d, 0x16e7e, 0x16e7f, 0x1e922, 0x1e923, 0x1e924, 0x1e925, 0x1e926, 0x1e927, 0x1e928, 0x1e929, 0x1e92a, 0x1e92b,
			0x1e92c, 0x1e92d, 0x1e92e, 0x1e92f, 0x1e930, 0x1e931, 0x1e932, 0x1e933, 0x1e934, 0x1e935, 0x1e936, 0x1e937, 0x1e938, 0x1e939, 0x1e93a, 0x1e93b,
			0x1e93c, 0x1e93d, 0x1e93e, 0x1e93f, 0x1e940, 0x1e941, 0x1e942, 0x1e943
		};

		static const char32_t uc[1384] = {
			0x00041, 0x00042, 0x00043, 0x00044, 0x00045, 0x00046, 0x00047, 0x00048, 0x00049, 0x0004a, 0x0004b, 0x0004c, 0x0004d, 0x0004e, 0x0004f, 0x00050,
			0x00051, 0x00052, 0x00053, 0x00054, 0x00055, 0x00056, 0x00057, 0x00058, 0x00059, 0x0005a, 0x01e9e, 0x000c0, 0x000c1, 0x000c2, 0x000c3, 0x000c4,
			0x000c5, 0x000c6, 0x000c7, 0x000c8, 0x000c9, 0x000ca, 0x000cb, 0x000cc, 0x000cd, 0x000ce, 0x000cf, 0x000d0, 0x000d1, 0x000d2, 0x000d3, 0x000d4,
			0x000d5, 0x000d6, 0x000d8, 0x000d9, 0x000da, 0x000db, 0x000dc, 0x000dd, 0x000de, 0x00178, 0x00100, 0x00102, 0x00104, 0x00106, 0x00108, 0x0010a,
			0x0010c, 0x0010e, 0x00110, 0x00112, 0x00114, 0x00116, 0x00118, 0x0011a, 0x0011c, 0x0011e, 0x00120, 0x00122, 0x00124, 0x00126, 0x00128, 0x0012a,
			0x0012c, 0x0012e, 0x00132, 0x00134, 0x00136, 0x00139, 0x0013b, 0x0013d, 0x0013f, 0x00141, 0x00143, 0x00145, 0x00147, 0x0014a, 0x0014c, 0x0014e,
			0x00150, 0x00152, 0x00154, 0x00156, 0x00158, 0x0015a, 0x0015c, 0x0015e, 0x00160, 0x00162, 0x00164, 0x00166, 0x00168, 0x0016a, 0x0016c, 0x0016e,
			0x00170, 0x00172, 0x00174, 0x00176, 0x00179, 0x0017b, 0x0017d, 0x00243, 0x00182, 0x00184, 0x00187, 0x0018b, 0x00191, 0x001f6, 0x00198, 0x0023d,
			0x00220, 0x001a0, 0x001a2, 0x001a4, 0x001a7, 0x001ac, 0x001af, 0x001b3, 0x001b5, 0x001b8, 0x001bc, 0x001f7, 0x001c4, 0x001c7, 0x001ca, 0x001cd,
			0x001cf, 0x001d1, 0x001d3, 0x001d5, 0x001d7, 0x001d9, 0x001db, 0x0018e, 0x001de, 0x001e0, 0x001e2, 0x001e4, 0x001e6, 0x001e8, 0x001ea, 0x001ec,
			0x001ee, 0x001f1, 0x001f4, 0x001f8, 0x001fa, 0x001fc, 0x001fe, 0x00200, 0x00202, 0x00204, 0x00206, 0x00208, 0x0020a, 0x0020c, 0x0020e, 0x00210,
			0x00212, 0x00214, 0x00216, 0x00218, 0x0021a, 0x0021c, 0x0021e, 0x00222, 0x00224, 0x00226, 0x00228, 0x0022a, 0x0022c, 0x0022e, 0x00230, 0x00232,
			0x0023b, 0x02c7e, 0x02c7f, 0x00241, 0x00246, 0x00248, 0x0024a, 0x0024c, 0x0024e, 0x02c6f, 0x02c6d, 0x02c70, 0x00181, 0x00186, 0x00189, 0x0018a,
			0x0018f, 0x00190, 0x0a7ab, 0x00193, 0x0a7ac, 0x00194, 0x0a78d, 0x0a7aa, 0x00197, 0x00196, 0x0a7ae, 0x02c62, 0x0a7ad, 0x0019c, 0x02c6e, 0x0019d,
			0x0019f, 0x02c64, 0x001a6, 0x0a7c5, 0x001a9, 0x0a7b1, 0x001ae, 0x00244, 0x001b1, 0x001b2, 0x00245, 0x001b7, 0x0a7b2, 0x0a7b0, 0x00370, 0x00372,
			0x00376, 0x003fd, 0x003fe, 0x003ff, 0x00386, 0x00388, 0x00389, 0x0038a, 0x00391, 0x00392, 0x00393, 0x00394, 0x00395, 0x00396, 0x00397, 0x00398,
			0x003f4, 0x00345, 0x01fbe, 0x0039a, 0x0039b, 0x0039c, 0x0039d, 0x0039e, 0x0039f, 0x003d6, 0x003f1, 0x003a3, 0x003a4, 0x003a5, 0x003a6, 0x003a7,
			0x003a8, 0x02126, 0x003aa, 0x003ab, 0x0038c, 0x0038e, 0x0038f, 0x003cf, 0x003d8, 0x003da, 0x003dc, 0x003de, 0x003e0, 0x003e2, 0x003e4, 0x003e6,
			0x003e8, 0x003ea, 0x003ec, 0x003ee, 0x003f9, 0x0037f, 0x003f7, 0x003fa, 0x00410, 0x00411, 0x01c80, 0x00413, 0x00414, 0x00415, 0x00416, 0x00417,
			0x00418, 0x00419, 0x0041a, 0x0041b, 0x0041c, 0x0041d, 0x0041e, 0x0041f, 0x00420, 0x00421, 0x00422, 0x01c85, 0x00423, 0x00424, 0x00425, 0x00426,
			0x00427, 0x00428, 0x00429, 0x01c86, 0x0042b, 0x0042c, 0x0042d, 0x0042e, 0x0042f, 0x00400, 0x00401, 0x00402, 0x00403, 0x00404, 0x00405, 0x00406,
			0x00407, 0x00408, 0x00409, 0x0040a, 0x0040b, 0x0040c, 0x0040d, 0x0040e, 0x0040f, 0x00460, 0x00462, 0x00464, 0x00466, 0x00468, 0x0046a, 0x0046c,
			0x0046e, 0x00470, 0x00472, 0x00474, 0x00476, 0x00478, 0x0047a, 0x0047c, 0x0047e, 0x00480, 0x0048a, 0x0048c, 0x0048e, 0x00490, 0x00492, 0x00494,
			0x00496, 0x00498, 0x0049a, 0x0049c, 0x0049e, 0x004a0, 0x004a2, 0x004a4, 0x004a6, 0x004a8, 0x004aa, 0x004ac, 0x004ae, 0x004b0, 0x004b2, 0x004b4,
			0x004b6, 0x004b8, 0x004ba, 0x004bc, 0x004be, 0x004c1, 0x004c3, 0x004c5, 0x004c7, 0x004c9, 0x004cb, 0x004cd, 0x004c0, 0x004d0, 0x004d2, 0x004d4,
			0x004d6, 0x004d8, 0x004da, 0x004dc, 0x004de, 0x004e0, 0x004e2, 0x004e4, 0x004e6, 0x004e8, 0x004ea, 0x004ec, 0x004ee, 0x004f0, 0x004f2, 0x004f4,
			0x004f6, 0x004f8, 0x004fa, 0x004fc, 0x004fe, 0x00500, 0x00502, 0x00504, 0x00506, 0x00508, 0x0050a, 0x0050c, 0x0050e, 0x00510, 0x00512, 0x00514,
			0x00516, 0x00518, 0x0051a, 0x0051c, 0x0051e, 0x00520, 0x00522, 0x00524, 0x00526, 0x00528, 0x0052a, 0x0052c, 0x0052e, 0x00531, 0x00532, 0x00533,
			0x00534, 0x00535, 0x00536, 0x00537, 0x00538, 0x00539, 0x0053a, 0x0053b, 0x0053c, 0x0053d, 0x0053e, 0x0053f, 0x00540, 0x00541, 0x00542, 0x00543,
			0x00544, 0x00545, 0x00546, 0x00547, 0x00548, 0x00549, 0x0054a, 0x0054b, 0x0054c, 0x0054d, 0x0054e, 0x0054f, 0x00550, 0x00551, 0x00552, 0x00553,
			0x00554, 0x00555, 0x00556, 0x01c90, 0x01c91, 0x01c92, 0x01c93, 0x01c94, 0x01c95, 0x01c96, 0x01c97, 0x01c98, 0x01c99, 0x01c9a, 0x01c9b, 0x01c9c,
			0x01c9d, 0x01c9e, 0x01c9f, 0x01ca0, 0x01ca1, 0x01ca2, 0x01ca3, 0x01ca4, 0x01ca5, 0x01ca6, 0x01ca7, 0x01ca8, 0x01ca9, 0x01caa, 0x01cab, 0x01cac,
			0x01cad, 0x01cae, 0x01caf, 0x01cb0, 0x01cb1, 0x01cb2, 0x01cb3, 0x01cb4, 0x01cb5, 0x01cb6, 0x01cb7, 0x01cb8, 0x01cb9, 0x01cba, 0x01cbd, 0x01cbe,
			0x01cbf, 0x0ab70, 0x0ab71, 0x0ab72, 0x0ab73, 0x0ab74, 0x0ab75, 0x0ab76, 0x0ab77, 0x0ab78, 0x0ab79, 0x0ab7a, 0x0ab7b, 0x0ab7c, 0x0ab7d, 0x0ab7e,
			0x0ab7f, 0x0ab80, 0x0ab81, 0x0ab82, 0x0ab83, 0x0ab84, 0x0ab85, 0x0ab86, 0x0ab87, 0x0ab88, 0x0ab89, 0x0ab8a, 0x0ab8b, 0x0ab8c, 0x0ab8d, 0x0ab8e,
			0x0ab8f, 0x0ab90, 0x0ab91, 0x0ab92, 0x0ab93, 0x0ab94, 0x0ab95, 0x0ab96, 0x0ab97, 0x0ab98, 0x0ab99, 0x0ab9a, 0x0ab9b, 0x0ab9c, 0x0ab9d, 0x0ab9e,
			0x0ab9f, 0x0aba0, 0x0aba1, 0x0aba2, 0x0aba3, 0x0aba4, 0x0aba5, 0x0aba6, 0x0aba7, 0x0aba8, 0x0aba9, 0x0abaa, 0x0abab, 0x0abac, 0x0abad, 0x0abae,
			0x0abaf, 0x0abb0, 0x0abb1, 0x0abb2, 0x0abb3, 0x0abb4, 0x0abb5, 0x0abb6, 0x0abb7, 0x0abb8, 0x0abb9, 0x0abba, 0x0abbb, 0x0abbc, 0x0abbd, 0x0abbe,
			0x0abbf, 0x013f8, 0x013f9, 0x013fa, 0x013fb, 0x013fc, 0x013fd, 0x0a77d, 0x02c63, 0x0a7c6, 0x01e00, 0x01e02, 0x01e04, 0x01e06, 0x01e08, 0x01e0a,
			0x01e0c, 0x01e0e, 0x01e10, 0x01e12, 0x01e14, 0x01e16, 0x01e18, 0x01e1a, 0x01e1c, 0x01e1e, 0x01e20, 0x01e22, 0x01e24, 0x01e26, 0x01e28, 0x01e2a,
			0x01e2c, 0x01e2e, 0x01e30, 0x01e32, 0x01e34, 0x01e36, 0x01e38, 0x01e3a, 0x01e3c, 0x01e3e, 0x01e40, 0x01e42, 0x01e44, 0x01e46, 0x01e48, 0x01e4a,
			0x01e4c, 0x01e4e, 0x01e50, 0x01e52, 0x01e54, 0x01e56, 0x01e58, 0x01e5a, 0x01e5c, 0x01e5e, 0x01e60, 0x01e62, 0x01e64, 0x01e66, 0x01e68, 0x01e6a,
			0x01e6c, 0x01e6e, 0x01e70, 0x01e72, 0x01e74, 0x01e76, 0x01e78, 0x01e7a, 0x01e7c, 0x01e7e, 0x01e80, 0x01e82, 0x01e84, 0x01e86, 0x01e88, 0x01e8a,
			0x01e8c, 0x01e8e, 0x01e90, 0x01e92, 0x01e94, 0x01ea0, 0x01ea2, 0x01ea4, 0x01ea6, 0x01ea8, 0x01eaa, 0x01eac, 0x01eae, 0x01eb0, 0x01eb2, 0x01eb4,
			0x01eb6, 0x01eb8, 0x01eba, 0x01ebc, 0x01ebe, 0x01ec0, 0x01ec2, 0x01ec4, 0x01ec6, 0x01ec8, 0x01eca, 0x01ecc, 0x01ece, 0x01ed0, 0x01ed2, 0x01ed4,
			0x01ed6, 0x01ed8, 0x01eda, 0x01edc, 0x01ede, 0x01ee0, 0x01ee2, 0x01ee4, 0x01ee6, 0x01ee8, 0x01eea, 0x01eec, 0x01eee, 0x01ef0, 0x01ef2, 0x01ef4,
			0x01ef6, 0x01ef8, 0x01efa, 0x01efc, 0x01efe, 0x01f08, 0x01f09, 0x01f0a, 0x01f0b, 0x01f0c, 0x01f0d, 0x01f0e, 0x01f0f, 0x01f18, 0x01f19, 0x01f1a,
			0x01f1b, 0x01f1c, 0x01f1d, 0x01f28, 0x01f29, 0x01f2a, 0x01f2b, 0x01f2c, 0x01f2d, 0x01f2e, 0x01f2f, 0x01f38, 0x01f39, 0x01f3a, 0x01f3b, 0x01f3c,
			0x01f3d, 0x01f3e, 0x01f3f, 0x01f48, 0x01f49, 0x01f4a, 0x01f4b, 0x01f4c, 0x01f4d, 0x01f59, 0x01f5b, 0x01f5d, 0x01f5f, 0x01f68, 0x01f69, 0x01f6a,
			0x01f6b, 0x01f6c, 0x01f6d, 0x01f6e, 0x01f6f, 0x01fba, 0x01fbb, 0x01fc8, 0x01fc9, 0x01fca, 0x01fcb, 0x01fda, 0x01fdb, 0x01ff8, 0x01ff9, 0x01fea,
			0x01feb, 0x01ffa, 0x01ffb, 0x01f88, 0x01f89, 0x01f8a, 0x01f8b, 0x01f8c, 0x01f8d, 0x01f8e, 0x01f8f, 0x01f98, 0x01f99, 0x01f9a, 0x01f9b, 0x01f9c,
			0x01f9d, 0x01f9e, 0x01f9f, 0x01fa8, 0x01fa9, 0x01faa, 0x01fab, 0x01fac, 0x01fad, 0x01fae, 0x01faf, 0x01fb8, 0x01fb9, 0x01fbc, 0x01fcc, 0x01fd8,
			0x01fd9, 0x01fe8, 0x01fe9, 0x01fec, 0x01ffc, 0x02132, 0x02160, 0x02161, 0x02162, 0x02163, 0x02164, 0x02165, 0x02166, 0x02167, 0x02168, 0x02169,
			0x0216a, 0x0216b, 0x0216c, 0x0216d, 0x0216e, 0x0216f, 0x02183, 0x024b6, 0x024b7, 0x024b8, 0x024b9, 0x024ba, 0x024bb, 0x024bc, 0x024bd, 0x024be,
			0x024bf, 0x024c0, 0x024c1, 0x024c2, 0x024c3, 0x024c4, 0x024c5, 0x024c6, 0x024c7, 0x024c8, 0x024c9, 0x024ca, 0x024cb, 0x024cc, 0x024cd, 0x024ce,
			0x024cf, 0x02c00, 0x02c01, 0x02c02, 0x02c03, 0x02c04, 0x02c05, 0x02c06, 0x02c07, 0x02c08, 0x02c09, 0x02c0a, 0x02c0b, 0x02c0c, 0x02c0d, 0x02c0e,
			0x02c0f, 0x02c10, 0x02c11, 0x02c12, 0x02c13, 0x02c14, 0x02c15, 0x02c16, 0x02c17, 0x02c18, 0x02c19, 0x02c1a, 0x02c1b, 0x02c1c, 0x02c1d, 0x02c1e,
			0x02c1f, 0x02c20, 0x02c21, 0x02c22, 0x02c23, 0x02c24, 0x02c25, 0x02c26, 0x02c27, 0x02c28, 0x02c29, 0x02c2a, 0x02c2b, 0x02c2c, 0x02c2d, 0x02c2e,
			0x02c60, 0x0023a, 0x0023e, 0x02c67, 0x02c69, 0x02c6b, 0x02c72, 0x02c75, 0x02c80, 0x02c82, 0x02c84, 0x02c86, 0x02c88, 0x02c8a, 0x02c8c, 0x02c8e,
			0x02c90, 0x02c92, 0x02c94, 0x02c96, 0x02c98, 0x02c9a, 0x02c9c, 0x02c9e, 0x02ca0, 0x02ca2, 0x02ca4, 0x02ca6, 0x02ca8, 0x02caa, 0x02cac, 0x02cae,
			0x02cb0, 0x02cb2, 0x02cb4, 0x02cb6, 0x02cb8, 0x02cba, 0x02cbc, 0x02cbe, 0x02cc0, 0x02cc2, 0x02cc4, 0x02cc6, 0x02cc8, 0x02cca, 0x02ccc, 0x02cce,
			0x02cd0, 0x02cd2, 0x02cd4, 0x02cd6, 0x02cd8, 0x02cda, 0x02cdc, 0x02cde, 0x02ce0, 0x02ce2, 0x02ceb, 0x02ced, 0x02cf2, 0x010a0, 0x010a1, 0x010a2,
			0x010a3, 0x010a4, 0x010a5, 0x010a6, 0x010a7, 0x010a8, 0x010a9, 0x010aa, 0x010ab, 0x010ac, 0x010ad, 0x010ae, 0x010af, 0x010b0, 0x010b1, 0x010b2,
			0x010b3, 0x010b4, 0x010b5, 0x010b6, 0x010b7, 0x010b8, 0x010b9, 0x010ba, 0x010bb, 0x010bc, 0x010bd, 0x010be, 0x010bf, 0x010c0, 0x010c1, 0x010c2,
			0x010c3, 0x010c4, 0x010c5, 0x010c7, 0x010cd, 0x0a640, 0x0a642, 0x0a644, 0x0a646, 0x0a648, 0x01c88, 0x0a64c, 0x0a64e, 0x0a650, 0x0a652, 0x0a654,
			0x0a656, 0x0a658, 0x0a65a, 0x0a65c, 0x0a65e, 0x0a660, 0x0a662, 0x0a664, 0x0a666, 0x0a668, 0x0a66a, 0x0a66c, 0x0a680, 0x0a682, 0x0a684, 0x0a686,
			0x0a688, 0x0a68a, 0x0a68c, 0x0a68e, 0x0a690, 0x0a692, 0x0a694, 0x0a696, 0x0a698, 0x0a69a, 0x0a722, 0x0a724, 0x0a726, 0x0a728, 0x0a72a, 0x0a72c,
			0x0a72e, 0x0a732, 0x0a734, 0x0a736, 0x0a738, 0x0a73a, 0x0a73c, 0x0a73e, 0x0a740, 0x0a742, 0x0a744, 0x0a746, 0x0a748, 0x0a74a, 0x0a74c, 0x0a74e,
			0x0a750, 0x0a752, 0x0a754, 0x0a756, 0x0a758, 0x0a75a, 0x0a75c, 0x0a75e, 0x0a760, 0x0a762, 0x0a764, 0x0a766, 0x0a768, 0x0a76a, 0x0a76c, 0x0a76e,
			0x0a779, 0x0a77b, 0x0a77e, 0x0a780, 0x0a782, 0x0a784, 0x0a786, 0x0a78b, 0x0a790, 0x0a792, 0x0a7c4, 0x0a796, 0x0a798, 0x0a79a, 0x0a79c, 0x0a79e,
			0x0a7a0, 0x0a7a2, 0x0a7a4, 0x0a7a6, 0x0a7a8, 0x0a7b4, 0x0a7b6, 0x0a7b8, 0x0a7ba, 0x0a7bc, 0x0a7be, 0x0a7c2, 0x0a7b3, 0x0ff21, 0x0ff22, 0x0ff23,
			0x0ff24, 0x0ff25, 0x0ff26, 0x0ff27, 0x0ff28, 0x0ff29, 0x0ff2a, 0x0ff2b, 0x0ff2c, 0x0ff2d, 0x0ff2e, 0x0ff2f, 0x0ff30, 0x0ff31, 0x0ff32, 0x0ff33,
			0x0ff34, 0x0ff35, 0x0ff36, 0x0ff37, 0x0ff38, 0x0ff39, 0x0ff3a, 0x10400, 0x10401, 0x10402, 0x10403, 0x10404, 0x10405, 0x10406, 0x10407, 0x10408,
			0x10409, 0x1040a, 0x1040b, 0x1040c, 0x1040d, 0x1040e, 0x1040f, 0x10410, 0x10411, 0x10412, 0x10413, 0x10414, 0x10415, 0x10416, 0x10417, 0x10418,
			0x10419, 0x1041a, 0x1041b, 0x1041c, 0x1041d, 0x1041e, 0x1041f, 0x10420, 0x10421, 0x10422, 0x10423, 0x10424, 0x10425, 0x10426, 0x10427, 0x104b0,
			0x104b1, 0x104b2, 0x104b3, 0x104b4, 0x104b5, 0x104b6, 0x104b7, 0x104b8, 0x104b9, 0x104ba, 0x104bb, 0x104bc, 0x104bd, 0x104be, 0x104bf, 0x104c0,
			0x104c1, 0x104c2, 0x104c3, 0x104c4, 0x104c5, 0x104c6, 0x104c7, 0x104c8, 0x104c9, 0x104ca, 0x104cb, 0x104cc, 0x104cd, 0x104ce, 0x104cf, 0x104d0,
			0x104d1, 0x104d2, 0x104d3, 0x10c80, 0x10c81, 0x10c82, 0x10c83, 0x10c84, 0x10c85, 0x10c86, 0x10c87, 0x10c88, 0x10c89, 0x10c8a, 0x10c8b, 0x10c8c,
			0x10c8d, 0x10c8e, 0x10c8f, 0x10c90, 0x10c91, 0x10c92, 0x10c93, 0x10c94, 0x10c95, 0x10c96, 0x10c97, 0x10c98, 0x10c99, 0x10c9a, 0x10c9b, 0x10c9c,
			0x10c9d, 0x10c9e, 0x10c9f, 0x10ca0, 0x10ca1, 0x10ca2, 0x10ca3, 0x10ca4, 0x10ca5, 0x10ca6, 0x10ca7, 0x10ca8, 0x10ca9, 0x10caa, 0x10cab, 0x10cac,
			0x10cad, 0x10cae, 0x10caf, 0x10cb0, 0x10cb1, 0x10cb2, 0x118a0, 0x118a1, 0x118a2, 0x118a3, 0x118a4, 0x118a5, 0x118a6, 0x118a7, 0x118a8, 0x118a9,
			0x118aa, 0x118ab, 0x118ac, 0x118ad, 0x118ae, 0x118af, 0x118b0, 0x118b1, 0x118b2, 0x118b3, 0x118b4, 0x118b5, 0x118b6, 0x118b7, 0x118b8, 0x118b9,
			0x118ba, 0x118bb, 0x118bc, 0x118bd, 0x118be, 0x118bf, 0x16e40, 0x16e41, 0x16e42, 0x16e43, 0x16e44, 0x16e45, 0x16e46, 0x16e47, 0x16e48, 0x16e49,
			0x16e4a, 0x16e4b, 0x16e4c, 0x16e4d, 0x16e4e, 0x16e4f, 0x16e50, 0x16e51, 0x16e52, 0x16e53, 0x16e54, 0x16e55, 0x16e56, 0x16e57, 0x16e58, 0x16e59,
			0x16e5a, 0x16e5b, 0x16e5c, 0x16e5d, 0x16e5e, 0x16e5f, 0x1e900, 0x1e901, 0x1e902, 0x1e903, 0x1e904, 0x1e905, 0x1e906, 0x1e907, 0x1e908, 0x1e909,
			0x1e90a, 0x1e90b, 0x1e90c, 0x1e90d, 0x1e90e, 0x1e90f, 0x1e910, 0x1e911, 0x1e912, 0x1e913, 0x1e914, 0x1e915, 0x1e916, 0x1e917, 0x1e918, 0x1e919,
			0x1e91a, 0x1e91b, 0x1e91c, 0x1e91d, 0x1e91e, 0x1e91f, 0x1e920, 0x1e921
		};

		Array<char> output;
		std::size_t stringLength = string.size();
		std::size_t idx = 0;

		arrayReserve(output, stringLength + 1);

		do {
			auto [c, nextIdx] = Utf8::NextChar(string, idx);

			const char32_t* f = std::lower_bound(l2u, l2u + arraySize(l2u), c);
			if (f != l2u + arraySize(l2u) && *f == c) {
				c = uc[f - l2u];
			}

			if (c < 0x7f) {
				arrayAppend(output, (char)c);
			} else if (c < 0x7ff) {
				arrayAppend(output, 0xC0 | (c >> 6));
				arrayAppend(output, 0x80 | (c & 0x3f));
			} else if (c < 0xFFFF) {
				arrayAppend(output, 0xE0 | (c >> 12));
				arrayAppend(output, 0x80 | ((c >> 6) & 0x3f));
				arrayAppend(output, 0x80 | (c & 0x3f));
			} else {
				arrayAppend(output, 0xF0 | (c >> 18));
				arrayAppend(output, 0x80 | ((c >> 12) & 0x3f));
				arrayAppend(output, 0x80 | ((c >> 6) & 0x3f));
				arrayAppend(output, 0x80 | (c & 0x3f));
			}

			idx = nextIdx;
		} while (idx < stringLength);

		arrayAppend(output, '\0');
		const std::size_t size = output.size();
		// This assumes that the growable array uses std::malloc() (which has to be std::free()'d later) in order to be
		// able to std::realloc(). The deleter doesn't use the size argument so it should be fine to transfer it over
		// to a String with the size excluding the null terminator.
		void(*const deleter)(char*, std::size_t) = output.deleter();
		DEATH_DEBUG_ASSERT(deleter, "Invalid deleter used", {});
		return String{output.release(), size - 1, deleter};
	}
	
	String replaceFirst(StringView string, StringView search, StringView replace) {
		// Handle also the case when the search string is empty - find() returns (empty) begin in that case and we just
		// prepend the replace string.
		const StringView found = string.find(search);
		if (!search || found) {
			String output{NoInit, string.size() + replace.size() - found.size()};
			const std::size_t begin = found.begin() - string.begin();
			std::memcpy(output.data(), string.data(), begin);
			std::memcpy(output.data() + begin, replace.data(), replace.size());
			const std::size_t end = begin + search.size();
			std::memcpy(output.data() + begin + replace.size(), string.data() + end, string.size() - end);
			return output;
		}

		return string;
	}

	String replaceAll(StringView string, StringView search, StringView replace) {
		DEATH_ASSERT(!search.empty(), "Empty search string would cause an infinite loop", {});
		Array<char> output;
		while (const StringView found = string.find(search)) {
			arrayAppend(output, string.prefix(found.begin()));
			arrayAppend(output, replace);
			string = string.slice(found.end(), string.end());
		}
		arrayAppend(output, string);
		arrayAppend(output, '\0');
		const std::size_t size = output.size();
		// This assumes that the growable array uses std::malloc() (which has to be std::free()'d later) in order to be
		// able to std::realloc(). The deleter doesn't use the size argument so it should be fine to transfer it over
		// to a String with the size excluding the null terminator.
		void(*const deleter)(char*, std::size_t) = output.deleter();
		DEATH_DEBUG_ASSERT(deleter, "Invalid deleter used", {});
		return String{output.release(), size - 1, deleter};
	}

	String replaceAll(String string, char search, char replace) {
		// If not even a single character is found, pass the argument through unchanged
		const MutableStringView found = string.find(search);
		if (!found) return string;

		// Convert the found pointer to an index to be able to replace even after a potential reallocation below
		const std::size_t firstFoundPosition = found.begin() - string.begin();

		// Otherwise, in the rare scenario where we'd get a non-owned string (such as String::nullTerminatedView() passed
		// right into the function), make it owned first. Usually it'll get copied however, which already makes it owned.
		if (!string.isSmall() && string.deleter()) string = String{string};

		// Replace the already-found occurence and delegate the rest further
		string[firstFoundPosition] = replace;
		replaceAllInPlace(string.exceptPrefix(firstFoundPosition + 1), search, replace);
		return string;
	}

	namespace Implementation
	{
		namespace
		{
			// Has to be first because the Avx2 variant may delegate to it if DEATH_ENABLE_SSE41 isn't defined due to compiler warts
			DEATH_CPU_MAYBE_UNUSED typename std::decay<decltype(replaceAllInPlaceCharacter)>::type replaceAllInPlaceCharacterImplementation(Cpu::ScalarT) {
				return [](char* const data, const std::size_t size, const char search, const char replace) {
					for (char* i = data, *end = data + size; i != end; ++i)
						if (*i == search) *i = replace;
				};
			}

			// SIMD implementation of character replacement. All tricks inherited from stringFindCharacterImplementation(),
			// in particular the unaligned preamble and postamble, as well as reducing the branching overhead by going through
			// four vectors at a time. See its documentation for more background info.
#if defined(DEATH_ENABLE_SSE41)
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SSE41 typename std::decay<decltype(replaceAllInPlaceCharacter)>::type replaceAllInPlaceCharacterImplementation(Cpu::Sse41T) {
				return [](char* const data, const std::size_t size, const char search, const char replace) DEATH_ENABLE_SSE41 {
					// If we have less than 16 bytes, do it the stupid way
					{
						char* j = data;
						switch (size) {
							case 15: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 14: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 13: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 12: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 11: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 10: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  9: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  8: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  7: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  6: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  5: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  4: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  3: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  2: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  1: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					const __m128i vsearch = _mm_set1_epi8(search);
					const __m128i vreplace = _mm_set1_epi8(replace);

					// Calculate the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll process some bytes twice.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);
					DEATH_DEBUG_ASSERT(i > data && reinterpret_cast<std::uintptr_t>(i) % 16 == 0);

					// Unconditionally process the first vector a slower, unaligned way. Do the replacement unconditionally
					// because it's faster than checking first.
					{
						const __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data));
						const __m128i out = _mm_blendv_epi8(in, vreplace, _mm_cmpeq_epi8(in, vsearch));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(data), out);
					}

					// Go four aligned vectors at a time. Bytes overlapping with the previous unaligned load will be processed
					// twice, but as everything is already replaced there, it'll be a no-op for those. Similarly to the find()
					// implementation, this reduces the branching overhead compared to branching on every vector, making
					// it comparable to an unconditional replace with a character that occurs often, but significantly faster
					// for characters that are rare.
					char* const end = data + size;
					for (; i + 4 * 16 <= end; i += 4 * 16) {
						const __m128i inA = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 0);
						const __m128i inB = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 1);
						const __m128i inC = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 2);
						const __m128i inD = _mm_load_si128(reinterpret_cast<const __m128i*>(i) + 3);

						const __m128i eqA = _mm_cmpeq_epi8(inA, vsearch);
						const __m128i eqB = _mm_cmpeq_epi8(inB, vsearch);
						const __m128i eqC = _mm_cmpeq_epi8(inC, vsearch);
						const __m128i eqD = _mm_cmpeq_epi8(inD, vsearch);

						const __m128i or1 = _mm_or_si128(eqA, eqB);
						const __m128i or2 = _mm_or_si128(eqC, eqD);
						const __m128i or3 = _mm_or_si128(or1, or2);
						// If any of the four vectors contained the character, replace all of them -- branching again
						// on each would hurt the "common character" case
						if (_mm_movemask_epi8(or3)) {
							const __m128i outA = _mm_blendv_epi8(inA, vreplace, eqA);
							const __m128i outB = _mm_blendv_epi8(inB, vreplace, eqB);
							const __m128i outC = _mm_blendv_epi8(inC, vreplace, eqC);
							const __m128i outD = _mm_blendv_epi8(inD, vreplace, eqD);

							_mm_store_si128(reinterpret_cast<__m128i*>(i) + 0, outA);
							_mm_store_si128(reinterpret_cast<__m128i*>(i) + 1, outB);
							_mm_store_si128(reinterpret_cast<__m128i*>(i) + 2, outC);
							_mm_store_si128(reinterpret_cast<__m128i*>(i) + 3, outD);
						}
					}

					// Handle remaining less than four aligned vectors. Again do the replacement unconditionally.
					for (; i + 16 <= end; i += 16) {
						const __m128i in = _mm_load_si128(reinterpret_cast<const __m128i*>(i));
						const __m128i out = _mm_blendv_epi8(in, vreplace, _mm_cmpeq_epi8(in, vsearch));
						_mm_store_si128(reinterpret_cast<__m128i*>(i), out);
					}

					// Handle remaining less than a vector in an unaligned way, again unconditionally and again overlapping bytes are no-op.
					if (i < end) {
						DEATH_DEBUG_ASSERT(i + 16 > end);
						i = end - 16;
						const __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(i));
						const __m128i out = _mm_blendv_epi8(in, vreplace, _mm_cmpeq_epi8(in, vsearch));
						_mm_storeu_si128(reinterpret_cast<__m128i*>(i), out);
					}
				};
			}
#endif

#if defined(DEATH_ENABLE_AVX2)
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_AVX2 typename std::decay<decltype(replaceAllInPlaceCharacter)>::type replaceAllInPlaceCharacterImplementation(Cpu::Avx2T) {
				return [](char* const data, const std::size_t size, const char search, const char replace) DEATH_ENABLE_AVX2 {
					// If we have less than 32 bytes, fall back to the SSE variant
					if (size < 32)
						return replaceAllInPlaceCharacterImplementation(Cpu::Sse41)(data, size, search, replace);

					const __m256i vsearch = _mm256_set1_epi8(search);
					const __m256i vreplace = _mm256_set1_epi8(replace);

					// Calculate the next aligned position. If the pointer was already aligned, we'll go to the next aligned vector;
					// if not, there will be an overlap and we'll process some bytes twice.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 32) & ~0x1f);
					DEATH_DEBUG_ASSERT(i > data && reinterpret_cast<std::uintptr_t>(i) % 32 == 0);

					// Unconditionally process the first vector a slower, unaligned way. Do the replacement unconditionally
					// because it's faster than checking first.
					{
						// _mm256_lddqu_si256 is just an alias to _mm256_loadu_si256, no reason to use it: https://stackoverflow.com/a/47426790
						const __m256i in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data));
						const __m256i out = _mm256_blendv_epi8(in, vreplace, _mm256_cmpeq_epi8(in, vsearch));
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(data), out);
					}

					// Go four aligned vectors at a time. Bytes overlapping with the previous unaligned load will be processed twice,
					// but as everything is already replaced there, it'll be a no-op for those. Similarly to the SSE2 implementation,
					// this reduces the branching overhead compared to branching on every vector, making it comparable to an unconditional
					// replace with a character that occurs often, but significantly faster for characters that are rare.
					char* const end = data + size;
					for (; i + 4 * 32 <= end; i += 4 * 32) {
						const __m256i inA = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 0);
						const __m256i inB = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 1);
						const __m256i inC = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 2);
						const __m256i inD = _mm256_load_si256(reinterpret_cast<const __m256i*>(i) + 3);

						const __m256i eqA = _mm256_cmpeq_epi8(inA, vsearch);
						const __m256i eqB = _mm256_cmpeq_epi8(inB, vsearch);
						const __m256i eqC = _mm256_cmpeq_epi8(inC, vsearch);
						const __m256i eqD = _mm256_cmpeq_epi8(inD, vsearch);

						const __m256i or1 = _mm256_or_si256(eqA, eqB);
						const __m256i or2 = _mm256_or_si256(eqC, eqD);
						const __m256i or3 = _mm256_or_si256(or1, or2);
						// If any of the four vectors contained the character, replace all of them -- branching again
						// on each would hurt the "common character" case
						if (_mm256_movemask_epi8(or3)) {
							const __m256i outA = _mm256_blendv_epi8(inA, vreplace, eqA);
							const __m256i outB = _mm256_blendv_epi8(inB, vreplace, eqB);
							const __m256i outC = _mm256_blendv_epi8(inC, vreplace, eqC);
							const __m256i outD = _mm256_blendv_epi8(inD, vreplace, eqD);

							_mm256_store_si256(reinterpret_cast<__m256i*>(i) + 0, outA);
							_mm256_store_si256(reinterpret_cast<__m256i*>(i) + 1, outB);
							_mm256_store_si256(reinterpret_cast<__m256i*>(i) + 2, outC);
							_mm256_store_si256(reinterpret_cast<__m256i*>(i) + 3, outD);
						}
					}

					// Handle remaining less than four aligned vectors. Again do the replacement unconditionally.
					for (; i + 32 <= end; i += 32) {
						const __m256i in = _mm256_load_si256(reinterpret_cast<const __m256i*>(i));
						const __m256i out = _mm256_blendv_epi8(in, vreplace, _mm256_cmpeq_epi8(in, vsearch));
						_mm256_store_si256(reinterpret_cast<__m256i*>(i), out);
					}

					// Handle remaining less than a vector in an unaligned way, again unconditionally and again overlapping bytes are no-op.
					if (i < end) {
						DEATH_DEBUG_ASSERT(i + 32 > end);
						i = end - 32;
						// _mm256_lddqu_si256 is just an alias to _mm256_loadu_si256, no reason to use it: https://stackoverflow.com/a/47426790
						const __m256i in = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(i));
						const __m256i out = _mm256_blendv_epi8(in, vreplace, _mm256_cmpeq_epi8(in, vsearch));
						_mm256_storeu_si256(reinterpret_cast<__m256i*>(i), out);
					}
				};
			}
#endif

			// Just a direct translation of the SSE4.1 code
#if defined(DEATH_ENABLE_SIMD128)
			DEATH_CPU_MAYBE_UNUSED DEATH_ENABLE_SIMD128 typename std::decay<decltype(replaceAllInPlaceCharacter)>::type replaceAllInPlaceCharacterImplementation(Cpu::Simd128T) {
				return [](char* const data, const std::size_t size, const char search, const char replace) DEATH_ENABLE_SIMD128 {
					// If we have less than 16 bytes, do it the stupid way
					{
						char* j = data;
						switch (size) {
							case 15: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 14: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 13: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 12: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 11: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case 10: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  9: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  8: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  7: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  6: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  5: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  4: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  3: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  2: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  1: if (*j++ == search) *(j - 1) = replace; DEATH_FALLTHROUGH
							case  0: return;
						}
					}

					const v128_t vsearch = wasm_i8x16_splat(search);
					const v128_t vreplace = wasm_i8x16_splat(replace);

					// Calculate the next aligned position. If the pointer was already aligned, we'll go to the next aligned
					// vector; if not, there will be an overlap and we'll process some bytes twice.
					char* i = reinterpret_cast<char*>(reinterpret_cast<std::uintptr_t>(data + 16) & ~0xf);
					DEATH_DEBUG_ASSERT(i > data && reinterpret_cast<std::uintptr_t>(i) % 16 == 0);

					// Unconditionally process the first vector a slower, unaligned way. WASM doesn't differentiate between
					// aligned and unaligned load/store, it's always unaligned, but the hardware might behave better if we
					// try to avoid unaligned operations.
					{
						const v128_t in = wasm_v128_load(reinterpret_cast<const v128_t*>(data));
						const v128_t out = wasm_v128_bitselect(vreplace, in, wasm_i8x16_eq(in, vsearch));
						wasm_v128_store(reinterpret_cast<v128_t*>(data), out);
					}

					// Go four aligned vectors at a time. Bytes overlapping with the previous unaligned load will be processed
					// twice, but as everything is already replaced there, it'll be a no-op for those. Similarly to the SSE2 / AVX2
					// implementation, this reduces the branching overhead compared to branching on every vector, making it
					// comparable to an unconditional replace with a character that occurs often, but significantly faster for
					// characters that are rare, on x86 at least. Elsewhere it *can* be slower due to the slow movemask emulation.
					char* const end = data + size;
					for (; i + 4 * 16 <= end; i += 4 * 16) {
						const v128_t inA = wasm_v128_load(reinterpret_cast<const v128_t*>(i) + 0);
						const v128_t inB = wasm_v128_load(reinterpret_cast<const v128_t*>(i) + 1);
						const v128_t inC = wasm_v128_load(reinterpret_cast<const v128_t*>(i) + 2);
						const v128_t inD = wasm_v128_load(reinterpret_cast<const v128_t*>(i) + 3);

						const v128_t eqA = wasm_i8x16_eq(inA, vsearch);
						const v128_t eqB = wasm_i8x16_eq(inB, vsearch);
						const v128_t eqC = wasm_i8x16_eq(inC, vsearch);
						const v128_t eqD = wasm_i8x16_eq(inD, vsearch);

						const v128_t or1 = wasm_v128_or(eqA, eqB);
						const v128_t or2 = wasm_v128_or(eqC, eqD);
						const v128_t or3 = wasm_v128_or(or1, or2);
						// If any of the four vectors contained the character, replace all of them -- branching again
						// on each would hurt the "common character" case
						if (wasm_i8x16_bitmask(or3)) {
							const v128_t outA = wasm_v128_bitselect(vreplace, inA, eqA);
							const v128_t outB = wasm_v128_bitselect(vreplace, inB, eqB);
							const v128_t outC = wasm_v128_bitselect(vreplace, inC, eqC);
							const v128_t outD = wasm_v128_bitselect(vreplace, inD, eqD);

							wasm_v128_store(reinterpret_cast<v128_t*>(i) + 0, outA);
							wasm_v128_store(reinterpret_cast<v128_t*>(i) + 1, outB);
							wasm_v128_store(reinterpret_cast<v128_t*>(i) + 2, outC);
							wasm_v128_store(reinterpret_cast<v128_t*>(i) + 3, outD);
						}
					}

					// Handle remaining less than four aligned vectors. Again do the replacement unconditionally.
					for (; i + 16 <= end; i += 16) {
						const v128_t in = wasm_v128_load(reinterpret_cast<const v128_t*>(i));
						const v128_t out = wasm_v128_bitselect(vreplace, in, wasm_i8x16_eq(in, vsearch));
						wasm_v128_store(reinterpret_cast<v128_t*>(i), out);
					}

					// Handle remaining less than a vector in an unaligned way. Overlapping bytes are again no-op.
					// Again WASM doesn't have any dedicated unaligned load/store instruction.
					if (i < end) {
						DEATH_DEBUG_ASSERT(i + 16 > end);
						i = end - 16;
						const v128_t in = wasm_v128_load(reinterpret_cast<const v128_t*>(i));
						const v128_t out = wasm_v128_bitselect(vreplace, in, wasm_i8x16_eq(in, vsearch));
						wasm_v128_store(reinterpret_cast<v128_t*>(i), out);
					}
				};
			}
#endif
		}

		DEATH_CPU_DISPATCHER_BASE(replaceAllInPlaceCharacterImplementation)
		DEATH_CPU_DISPATCHED(replaceAllInPlaceCharacterImplementation, void DEATH_CPU_DISPATCHED_DECLARATION(replaceAllInPlaceCharacter)(char* data, std::size_t size, char search, char replace))({
			return replaceAllInPlaceCharacterImplementation(Cpu::DefaultBase)(data, size, search, replace);
		})
	}

}}}