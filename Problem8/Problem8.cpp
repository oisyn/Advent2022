import core;
import util;
using namespace util;

int findchar(const char* ptr, char c)
{
	auto allC = _mm256_set1_epi8(c);
	constexpr uint RegSize = sizeof(allC);

	for (uint offset = 0; ; offset += RegSize)
	{
		auto chars = _mm256_cmpeq_epi8(_mm256_loadu_epi8(ptr + offset), allC);
		if (uint mvmask = _mm256_movemask_epi8(chars))
			return std::countr_zero(mvmask) + offset;
	}
}

std::pair<long long, bool> GetScoreAndVisibility(const char* data, int x, int y, int width, int height, int stride)
{
	int offset = y * stride + x;
	int h = data[offset];

	long long l = 1;
	bool visible = false;
	int dx, dy, o;

	dx = x + 1;
	o = offset + 1;
	while (dx < width && data[o] < h)
		dx++, o++;
	if (dx >= width)
		dx--, visible = true;
	l *= dx - x;

	dx = x - 1;
	o = offset - 1;
	while (dx >= 0 && data[o] < h)
		dx--, o--;
	if (dx < 0)
		dx = 0, visible = true;
	l *= x - dx;

	dy = y + 1;
	o = offset + stride;
	while (dy < height  && data[o] < h)
		dy++, o += stride;
	if (dy >= height)
		dy--, visible = true;
	l *= dy - y;

	dy = y - 1;
	o = offset - stride;
	while (dy >= 0 && data[o] < h)
		dy--, o -= stride;
	if (dy < 0)
		dy = 0, visible = true;
	l *= y - dy;

	return { l, visible };
}

bool run(const wchar_t* file)
{
	Timer total(AutoStart);
	Timer load(AutoStart);
	MemoryMappedFile mmap(file);
	if (!mmap)
		return false;

	auto data = mmap.GetSpan<const char>();
	int width = findchar(data.data(), '\n');
	int stride = width + 1;
	int height = int(mmap.GetSize() + 1) / stride;

	load.Stop();

	std::atomic<int> nextOffset = 0;
	auto futures = DoParallel([=, &nextOffset](int threadIdx, int numThreads) -> std::pair<long long, int>
	{
		long long myMax = 0;
		int numVisible = 0;
		constexpr int WorkloadSize = 32;

		for (;;)
		{
			int myoffsets = (nextOffset += WorkloadSize) - WorkloadSize;
			for (int i = 0; i < WorkloadSize; i++)
			{
				int o = myoffsets + i;
				if (o >= width * height)
					return { myMax, numVisible };

				int y = o / width;
				int x = o % width;
				auto [s, v] = GetScoreAndVisibility(data.data(), x, y, width, height, stride);
				myMax = std::max(myMax, s);
				numVisible += v;
			}
		}
	});

	long long maxScore = 0;
	int numTrees = 0;
	for (auto& f : futures)
	{
		auto [s, n] = f.get();
		maxScore = std::max(maxScore, s);
		numTrees += n;
	}

	total.Stop();
	std::cout << std::format("Time: {}us (load:{}us)\n{}\n{}\n", total.GetTime(), load.GetTime(), numTrees, maxScore);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"example.txt",
		L"input.txt",
		L"aoc_2022_day08_rect.txt",
		L"aoc_2022_day08_sparse.txt",
		L"input-mrhaas.txt",
	};

	for (auto f : inputs)
	{
		std::wcout << std::format(L"\n===[ {} ]==========\n", f);
		if (!run(f))
			std::wcerr << std::format(L"Can't open `{}`\n", f);
	}
}
