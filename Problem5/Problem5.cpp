#define CV_ENABLE		0	// whether the concurrency visualizer code is enabled

#if CV_ENABLE
#include <cvmarkersobj.h>
using namespace Concurrency::diagnostic;

marker_series g_markerSeries(L"Problem1");

#define CV_MARKER(x) g_markerSeries.write_flag(x)
#define CV_SCOPED_SPAN(x) span myspan ## __COUNTER__(g_markerSeries, x)

union delayed_span
{
	delayed_span(const wchar_t* x) : s(g_markerSeries, x) { }
	delayed_span(const delayed_span&) = delete;
	~delayed_span() { }
	span s;
};

#define CV_SPAN_START(name, x) delayed_span cvspan_ ## name(x)
#define CV_SPAN_STOP(name) cvspan_ ## name.s.~span()

#else

#define CV_MARKER(x) do;while(0)
#define CV_SCOPED_SPAN(x) do;while(0)
#define CV_SPAN_START(name, x) do;while(0)
#define CV_SPAN_STOP(name) do;while(0)

#endif


using uint = unsigned int;
using ullong = unsigned long long;
static constexpr size_t RegSize = sizeof(__m256i);

uint conv(const char* str, int size)
{
	__assume(size >= 0 && size <= 8);
	int sizeBits = size * 8;

	ullong l = *(const ullong*)str;
	l ^= 0x30303030'30303030;
	l <<= 64 - sizeBits;

	__m128i v8 = _mm_cvtsi64_si128(l);
	__m256i v = _mm256_cvtepu8_epi32(v8); // 8x 32 bit digits
	v = _mm256_mullo_epi32(v, _mm256_setr_epi32(10'000'000, 1'000'000, 100'000, 10'000, 1'000, 100, 10, 1));
	v = _mm256_hadd_epi32(v, v); // 4 combined numbers
	v = _mm256_hadd_epi32(v, v); // 2 combined numbers
	uint r =  _mm_cvtsi128_si32(_mm_add_epi32(_mm256_extracti128_si256(v, 0), _mm256_extracti128_si256(v, 1)));
	return r;
}

uint conv(const char* str, const char* end)
{
	return conv(str, int(end - str));
}

int main()
{
	CV_SPAN_START(init, L"Init");
	auto start = std::chrono::high_resolution_clock::now();

	auto filename = L"aoc_2022_day05_large_input.txt";
	HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return (std::wcerr << std::format(L"Can't open file `{}`\n", filename)), 1;
	ullong fileSize = 0;
	GetFileSizeEx(hFile, (LARGE_INTEGER*)&fileSize);

	HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (hMap == INVALID_HANDLE_VALUE)
		return (std::cerr << "Error creating file mapping\n"), 1;

	const char* basePtr = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	const char* endPtr = basePtr + fileSize;

	std::vector<std::span<const char>> stateLines;
	stateLines.reserve(10000);
	int numStacks = 0;

	CV_SPAN_STOP(init);
	CV_SPAN_START(state, L"Parse state");

	const char* lineStart = basePtr;
	for(;;)
	{
		auto line = lineStart;
		auto nl = std::find(line, endPtr, '\n');
		lineStart = nl + 1;

		int len = int(nl - line);
		if (len < 2 || line[1] == '0' || line[1] == '1')
			break;

		stateLines.push_back(std::span{ line, nl });
		int stacks = (len + 1) / 4;
		numStacks = std::max(numStacks, stacks);
	}

	CV_SPAN_STOP(state);

	std::vector<int> stackStart(numStacks);
	std::jthread findStackStarts([&, numStacks]
	{
		CV_SCOPED_SPAN(L"find stack starts");
		for (int i = 0; i < numStacks; i++)
		{
			int p = i * 4 + 1;
			for (int j = 0; j < stateLines.size(); j++)
			{
				if (stateLines[j].size() > p && stateLines[j][p] != ' ')
				{
					stackStart[i] = j;
					break;
				}
			}
		}
	});

	CV_SPAN_START(moves, L"Parse moves");
	constexpr uint MaxThreads = 32;
	static std::array<uint, 3> threadTotals[MaxThreads] = { };
	int numThreads = std::max(1u, std::min(std::thread::hardware_concurrency(), MaxThreads) - 2);

	struct Move { int n, from, to; };
	std::vector<Move> threadMoves[MaxThreads];
	std::vector<std::jthread> threads;
	threads.reserve(numThreads);

	for (int threadIdx = numThreads - 1; threadIdx >= 0; threadIdx--)
	{
		threads.emplace_back([&](int idx)
		{
			int total = endPtr - lineStart;
			const char* start = lineStart + (total * idx / numThreads);
			const char* end = lineStart + (total * (idx + 1) / numThreads);

			if (idx > 0)
				start = std::find(start, end, '\n') + 1;
			end = std::find(end, endPtr, '\n');

			std::vector<Move> mymoves;
			mymoves.reserve(1000);

			while (start < end)
			{
				// "move a from b to c\n"

				auto line = start;
				auto nl = std::find(line, endPtr, '\n');
				start = nl + 1;

				int len = int(nl - line);
				if (len < 8)
					continue;

				auto a = line + 5;
				auto a_end = std::find(a + 1, nl, ' ');
				auto b = a_end + 6;
				auto b_end = std::find(b + 1, nl, ' ');
				auto c = b_end + 4;
				auto c_end = std::find(c + 1, nl, ' ');

				mymoves.emplace_back(conv(a, a_end), conv(b, b_end) - 1, conv(c, c_end) - 1);
			}

			threadMoves[idx] = std::move(mymoves);
		}, threadIdx);
	}

	CV_SPAN_STOP(moves);

	CV_SPAN_START(process, L"Process");
	struct Pile { int junk, column; };
	std::vector<std::vector<Pile>> stacks(numStacks);
	for (int i = 0; i < numStacks; i++)
	{
		stacks[i].reserve(1000);
		stacks[i].emplace_back(0, i);
	}

	for (int threadIdx = numThreads - 1; threadIdx >= 0; threadIdx--)
	{
		threads[threadIdx].join();
		auto& moves = threadMoves[threadIdx];
		for (auto [n, to, from] : std::ranges::reverse_view(moves))
		{
			auto& src = stacks[from];
			auto& dest = stacks[to];

			while (n)
			{
				if (dest.empty())
				{
					if (src.empty())
						break;

					auto& srcPile = src.back();
					int amount = std::min(srcPile.junk, n);
					srcPile.junk -= amount;
					n -= amount;
					if (n)
					{
						n--;
						dest.emplace_back(0, srcPile.column);
						src.pop_back();
					}
				}
				else
				{
					if (src.empty())
					{
						dest.back().junk += n;
						break;
					}

					auto& srcPile = src.back();
					auto& destPile = dest.back();
					int amount = std::min(srcPile.junk, n);
					srcPile.junk -= amount;
					destPile.junk += amount;
					n -= amount;
					if (n)
					{
						n--;
						dest.emplace_back(0, srcPile.column);
						src.pop_back();
					}
				}
			}
		}
	}
	CV_SPAN_STOP(process);
	CV_SPAN_START(result, L"Get result");

	findStackStarts.join();
	std::vector<char> result(numStacks + 1);
	for (int i = 0; i < numStacks; i++)
	{
		auto& s = stacks[i];
		int top = stackStart[i];
		for (auto& p : std::ranges::reverse_view(s))
		{
			top += p.junk;
			result[p.column] = stateLines[top][i * 4 + 1];
			top++;
		}
	}
	CV_SPAN_STOP(result);

	auto d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
	std::cout << std::format("Time: {} us\n", d);
	//std::cout << result.data() << std::endl;
}
