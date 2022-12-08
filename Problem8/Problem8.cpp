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

template<class C>
void Process(const char* data, long long* treeScore, int majorMax, int majorStride, int minorMin, int minorMax, int minorStride)
{
	int nmajor = majorMax / majorStride;
	auto algo = [=](int threadIdx, int numThreads)
	{
		int from = threadIdx * nmajor / numThreads;
		int to = (threadIdx + 1) * nmajor / numThreads;
		from *= majorStride;
		to *= majorStride;

		C comparator;
		for (int major = from; major < to; major += majorStride)
		{
			int treeDists[10] = { };
			for (int minor = minorMin, d = 0; comparator(minor, minorMax); minor += minorStride, d++)
			{
				int offset = major + minor;
				int height = data[offset] - '0';
				int distance = d - treeDists[height];
				treeScore[offset] *= distance;
				std::fill_n(treeDists, height + 1, d);
			}
		}
	};

	if (nmajor < 200)
		algo(0, 1);
	else
	{
		auto futures = DoParallel(algo);
		for (auto& f : futures)
			f.wait();
	}

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

	Timer part1(AutoStart);
	std::vector<bool> bits(stride * height);

	int numTrees = 0;
	for (int y = 0; y < height; y++)
	{
		int max = 0;
		int offset = y * stride;
		for (int x = 0; x < width; x++, offset++)
		{
			if (data[offset] > max)
			{
				if (!bits[offset])
					numTrees++;
				bits[offset] = 1;
				max = data[offset];
			}
		}
		int maxHeight = max;
		max = 0;
		offset--;
		for (int x = width - 1; x >= 0; x--, offset--)
		{
			char c = data[offset];
			if (c > max)
			{
				if (!bits[offset])
					numTrees++;
				bits[offset] = 1;
				max = data[offset];
			}
			if (c >= maxHeight)
				break;
		}
	}

	for (int x = 0; x < width; x++)
	{
		int max = 0;
		int offset = x;
		for (int y = 0; y < height; y++, offset += stride)
		{
			if (data[offset] > max)
			{
				if (!bits[offset])
					numTrees++;
				bits[offset] = 1;
				max = data[offset];
			}
		}
		int maxHeight = max;
		max = 0;
		offset -= stride;
		for (int y = height - 1; y >= 0; y--, offset -= stride)
		{
			char c = data[offset];

			if (c > max)
			{
				if (!bits[offset])
					numTrees++;
				bits[offset] = 1;
				max = data[offset]; 
			}
			if (c >= maxHeight)
				break;
		}
	}
	part1.Stop();


	Timer part2(AutoStart);
	std::vector<long long> treeScore(height * stride, 1);
	Process<std::less<>>(data.data(), treeScore.data(), height * stride, stride, 0, width, 1);				   // left
	Process<std::greater_equal<>>(data.data(), treeScore.data(), height * stride, stride, width - 1, 0, -1);   // right
	Process<std::less<>>(data.data(), treeScore.data(), width, 1, 0, height * stride, stride);				   // up
	Process<std::greater_equal<>>(data.data(), treeScore.data(), width, 1, (height - 1) * stride, 0, -stride); // down
	auto maxScore = std::ranges::max(treeScore);
 	part2.Stop();

	total.Stop();
	std::cout << std::format("Time: {}us (load:{}us, part1:{}us, part2:{}us)\n{}\n{}\n", total.GetTime(), load.GetTime(), part1.GetTime(), part2.GetTime(), numTrees, maxScore);

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
