#include "pch.h"

using uint = unsigned int;
using ullong = unsigned long long;
static constexpr size_t RegSize = sizeof(__m256i);

static const alignas(16) unsigned char s_mask[48] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const alignas(16) uint s_mul[32] = { 1'000'000'000, 100'000'000, 10'000'000, 1'000'000, 100'000, 10'000, 1'000, 100, 10, 1 };

static const alignas(16) unsigned char s_masks[10][16] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
};

uint conv(const char* str, int size)
{
	__m128i v8 = _mm_loadu_epi8(str);
	v8 = _mm_sub_epi8(v8, _mm_set1_epi8('0'));
	v8 = _mm_and_si128(v8, _mm_loadu_epi8(s_mask + 32 - size));	// 16x 8 bit digits

	__m256i vlo = _mm256_cvtepi8_epi32(v8); // lower 8x 32 bit digits
	__m256i vhi = _mm256_cvtepi8_epi32(_mm_shuffle_epi32(v8, _MM_SHUFFLE(1, 0, 3, 2))); // upper 8x 32 bit digits
	vlo = _mm256_mullo_epi32(vlo, _mm256_loadu_si256((__m256i*)(s_mul + 10 - size)));
	vhi = _mm256_mullo_epi32(vhi, _mm256_loadu_si256((__m256i*)(s_mul + 18 - size)));

	__m256i v = _mm256_add_epi32(vlo, vhi);	// 8 combined numbers
	v = _mm256_hadd_epi32(v, v); // 4 combined numbers
	v = _mm256_hadd_epi32(v, v); // 2 combined numbers
	uint r = _mm256_cvtsi256_si32(v) + _mm256_extract_epi32(v, 4);
	return r;
}

__m256i conv_1(const char* str, int size)
{
	__m128i v8 = _mm_loadu_epi8(str - 10 + size);
	v8 = _mm_sub_epi8(v8, _mm_set1_epi8('0'));
	//v8 = _mm_and_si128(v8, _mm_loadu_epi8(s_mask + 6 + size));
	//v8 = _mm_and_si128(v8, _mm_loadu_epi8(s_mask + 16 + 6));
	v8 = _mm_and_si128(v8, _mm_load_si128((const __m128i*)s_masks[size-1]));

	return _mm256_cvtepi8_epi16(v8); // lower 16x 16 bit digits
}

uint conv_2(__m256i v)
{
	__m256i vlo = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 0)); // lower 8x 32 bit digits
	__m256i vhi = _mm256_cvtepi16_epi32(_mm256_extracti128_si256(v, 1)); // upper 8x 32 bit digits
	vlo = _mm256_mullo_epi32(vlo, _mm256_loadu_si256((__m256i*)(s_mul)));
	vhi = _mm256_mullo_epi32(vhi, _mm256_loadu_si256((__m256i*)(s_mul + 8)));

	v = _mm256_add_epi32(vlo, vhi);	// 8 combined numbers
	v = _mm256_hadd_epi32(v, v); // 4 combined numbers
	v = _mm256_hadd_epi32(v, v); // 2 combined numbers
	uint r = _mm256_cvtsi256_si32(v) + _mm256_extract_epi32(v, 4);
	return r;
}

int main()
{
	auto start = std::chrono::high_resolution_clock::now();

	HANDLE hFile = CreateFile(L"aoc_2022_day01_large_input.txt", GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return (std::cerr << "Can't open file `aoc_2022_day01_large_input.txt`\n"), 1;
	ullong fileSize = 0;
	GetFileSizeEx(hFile, (LARGE_INTEGER*)&fileSize);

	HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (hMap == INVALID_HANDLE_VALUE)
		return (std::cerr << "Error creating file mapping\n"), 1;

	const char* basePtr = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);

	// create small shadow region where we can do misaligned reads starting before the data
	char* shadow = new char[64];
	std::copy_n(basePtr, 64 - 16, shadow + 16);

	constexpr uint MaxThreads = 32;
	static std::array<uint, 3> threadTotals[MaxThreads] = { };
	int numThreads = std::min(std::thread::hardware_concurrency(), MaxThreads);
	const ullong numRegs = (fileSize + RegSize - 1) / RegSize;

	auto ThreadFunc = [&](const int threadIdx)
	{
		std::array<uint, 3> totals = { };
		auto insert = [](uint l, std::array<uint, 3>& totals)
		{
			if (l < totals[2])
				return;
			if (l < totals[1])
			{
				totals[2] = l;
				return;
			}

			if (l < totals[0])
			{
				totals[2] = totals[1];
				totals[1] = l;
				return;
			}

			totals[2] = totals[1];
			totals[1] = totals[0];
			totals[0] = l;
		};

		ullong startReg = numRegs * threadIdx / numThreads;
		ullong endOffset = (numRegs * (threadIdx + 1) / numThreads - startReg) * RegSize;
		const char* ptr = basePtr + (startReg * RegSize);
		std::span<const __m256i> data((const __m256i*)ptr, (const __m256i*)basePtr + numRegs);

		ullong offset = 0 - RegSize, lastpos = 0;
		__m256i subtotalv = _mm256_setzero_si256();

		__m256i allNl = _mm256_set1_epi8('\n');
		bool skipFirst = threadIdx > 0;

		for (auto c : data)
		{
			offset += RegSize;
			__m256i nls = _mm256_cmpeq_epi8(c, allNl);
			uint mvmask = _mm256_movemask_epi8(nls);
			while (mvmask)
			{
				ullong npos = std::countr_zero(mvmask) + offset;
				if (npos == lastpos)
				{	// double newline
					if (skipFirst)
						skipFirst = false;
					else
					{
						uint subtotal = conv_2(subtotalv);
						insert(subtotal, totals);
						subtotalv = _mm256_setzero_si256();
					}

					if (offset >= endOffset)
					{
						threadTotals[threadIdx] = totals;
						return;
					}
				}
				else if (!skipFirst)
				{
					if (threadIdx == 0 && lastpos < 16)
					{
						auto v = conv_1(shadow + 16 + lastpos, int(npos - lastpos));
						subtotalv = _mm256_add_epi16(subtotalv, v);
					}
					else
					{
						auto v = conv_1(ptr + lastpos, int(npos - lastpos));
						subtotalv = _mm256_add_epi16(subtotalv, v);
					}
				}
				lastpos = npos + 1;
				mvmask &= mvmask - 1;
			}
		}

		uint subtotal = conv_2(subtotalv);
		insert(subtotal, totals);
		threadTotals[threadIdx] = totals;
	};


	std::vector<std::jthread> threads;
	std::array<uint, 3> grandTotals = { };
	threads.reserve(numThreads);
	for (int n = 0; n < numThreads; n++)
		threads.emplace_back(ThreadFunc, n);
	int threadIdx = 0;
	for (auto& t : threads)
	{
		t.join();

		auto old = grandTotals;
		int l = 0, r = 0;
		for (int i = 0; i < 3; i++)
		{
			if (old[l] < threadTotals[threadIdx][r])
				grandTotals[i] = threadTotals[threadIdx][r++];
			else
				grandTotals[i] = old[l++];
		}

		threadIdx++;
	}

	auto d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
	std::cout << std::format("Time: {} us\n", d);

	std::cout << std::format("Max: {}\n", grandTotals[0]);
	std::cout << std::format("Top 3: {} ({}, {}, {})\n", grandTotals[0] + grandTotals[1] + grandTotals[2], grandTotals[0], grandTotals[1], grandTotals[2]);

}