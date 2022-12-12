import core;
import util;
using namespace util;

#define VIS		0

int FindChar(const char* ptr, char c)
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

int MinPath(int fromX, int fromY, int fromHeight, int toX, int toY, int toHeight)
{
	// disabled because it's slower
	return 0; // std::abs(fromX - fromY) + std::abs(fromX - toX) + std::max(toHeight - fromHeight, 0);
}

bool Run(const wchar_t* file)
{
	Timer total(AutoStart);
	Timer tload(AutoStart);
	MemoryMappedFile mmap(file, true);
	if (!mmap)
		return false;
	int size = (int)mmap.GetSize();
	auto data = mmap.GetSpan<char>();
	char* ptr = data.data();
	char* endPtr = ptr + size;

	int width = FindChar(ptr, '\n');
	int stride = width + 1;
	int height = int(mmap.GetSize() + 1) / stride;

	int startPos = FindChar(ptr, 'S');
	int startX = startPos % stride;
	int startY = startPos / stride;
	ptr[startPos] = 'a';
	int endPos = FindChar(ptr, 'E');
	int endX = endPos % stride;
	int endY = endPos / stride;
	ptr[endPos] = 'z';

	std::vector<int> leastPath(stride * height, std::numeric_limits<int>::max());
	leastPath[startPos] = 0;
	tload.Stop();

	Timer talgo[2];
	int result[2];

	for (int part = 0; part < 2; part++)
	{
		talgo[part].Start();
		struct Node
		{
			int minPath;
			int offset;
			int x, y;
		};
		using NodeGreater = decltype([](auto& a, auto& b) { return a.minPath > b.minPath; });
		std::priority_queue<Node, std::vector<Node>, NodeGreater> queue;

		if (part == 0)
			queue.emplace(MinPath(startX, startY, 'a', endX, endY, 'z'), startPos, startX, startY);
		else
		{
			std::ranges::fill(leastPath, std::numeric_limits<int>::max());
			for (int y = 0, o = 0; y < height; y++, o++)
				for (int x = 0; x < width; x++, o++)
					if (ptr[o] == 'a')
						leastPath[o] = 0, queue.emplace(MinPath(x, y, 'a', endX, endY, 'z'), o, x, y);
		}

		std::vector<char> field(stride * height + 1);

		while (!queue.empty())
		{
		#if VIS
			for (int i = 0; i < size; i++)
				field[i] = (leastPath[i] < 0x7fffffff) ? '.' : ptr[i];
			std::cout << "\x1b[0;0H" << field.data();
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		#endif

			auto n = queue.top();
			queue.pop();
			if (n.offset == endPos)
			{
				result[part] = leastPath[n.offset];
				break;
			}

			int h = data[n.offset];
			int p = leastPath[n.offset] + 1;
			if (n.x > 0 && leastPath[n.offset - 1] > p && data[n.offset - 1] <= h + 1)
			{
				leastPath[n.offset - 1] = p;
				queue.emplace(p + MinPath(n.x, n.y, h, n.x - 1, n.y, data[n.offset - 1]), n.offset - 1, n.x - 1, n.y);
			}
			if (n.x + 1 < width && leastPath[n.offset + 1] > p && data[n.offset + 1] <= h + 1)
			{
				leastPath[n.offset + 1] = p;
				queue.emplace(p + MinPath(n.x, n.y, h, n.x + 1, n.y, data[n.offset + 1]), n.offset + 1, n.x + 1, n.y);
			}
			if (n.y > 0 && leastPath[n.offset - stride] > p && data[n.offset - stride] <= h + 1)
			{
				leastPath[n.offset - stride] = p;
				queue.emplace(p + MinPath(n.x, n.y, h, n.x, n.y - 1, data[n.offset - stride]), n.offset - stride, n.x, n.y - 1);
			}
			if (n.y + 1 < height && leastPath[n.offset + stride] > p && data[n.offset + stride] <= h + 1)
			{
				leastPath[n.offset + stride] = p;
				queue.emplace(p + MinPath(n.x, n.y, h, n.x, n.y + 1, data[n.offset + stride]), n.offset + stride, n.x, n.y + 1);
			}
		}

	#if VIS
		if (part == 0)
		{
			for (int i = 0; i < size; i++)
				field[i] = (leastPath[i] < 0x7fffffff) ? '.' : ptr[i];
			int offset = endPos;
			while (offset != startPos)
			{
				int s = leastPath[offset];
				if (offset > 0 && leastPath[offset - 1] == s - 1)
					offset--;
				else if (offset + 1 < size && leastPath[offset + 1] == s - 1)
					offset++;
				else if (offset >= stride && leastPath[offset - stride] == s - 1)
					offset -= stride;
				else if (offset + stride < size && leastPath[offset + stride] == s - 1)
					offset += stride;
				field[offset] = '#';
			}
			std::cout << "\x1b[0;0H" << field.data();
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
	#endif


		talgo[part].Stop();
	}

	total.Stop();
	std::cout << std::format(std::locale(""), "Time: {:L}us (load:{:L}us, part1:{:L}us, part2:{:L}us)\nPart 1: {}\nPart 2: {}\n", total.GetTime(), tload.GetTime(), talgo[0].GetTime(), talgo[1].GetTime(), result[0], result[1]);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"input.txt",
		L"aoc_2022_day12_large-1.txt",
	};

	constexpr int NumRuns = 10;
	for (auto f : inputs)
	{
		std::wcout << std::format(L"\n===[ {} ]==========\n", f);
		for (int i = 0; i < NumRuns; i++)
		{
			if (!Run(f))
				std::wcerr << std::format(L"Can't open `{}`\n", f);
		}
	}
}
