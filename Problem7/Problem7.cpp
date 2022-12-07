import core;
import util;
using namespace util;


int findchar(const char* ptr, char c)
{
	auto s = _mm256_loadu_epi8(ptr);
	auto chars = _mm256_cmpeq_epi8(s, _mm256_set1_epi8(c));
	uint mvmask = _mm256_movemask_epi8(chars);
	if (!mvmask)
		return 0x4000'0000;
	return std::countr_zero(mvmask);
}


bool run(const wchar_t* file)
{
	MemoryMappedFile mmap(file);
	if (!mmap)
		return (std::wcerr << std::format(L"Error opening `{}`\n", file)), false;

	Timer t(AutoStart);
	Timer parse(AutoStart);
	//Splitter lines(mmap.GetSpan<const char>(), '\n');
	std::vector<uint> dirSizes;
	std::vector<uint> stack;
	uint current = 0;
	uint r1 = 0;

	dirSizes.reserve(200000);
	stack.reserve(100000);
	auto data = mmap.GetSpan<const char>().data();
	auto dataEnd = data + mmap.GetSize();

	while(data < dataEnd)
	{
		if (data[0] == '$')
		{
			if (data[2] == 'c')
			{
				if (data[5] == '/')
				{
					while (!stack.empty())
					{
						dirSizes.push_back(current);
						if (current <= 100000)
							r1 += current;
						current += stack.back();
						stack.pop_back();
					}
					data += 7; // "$ cd /\n"
					continue;
				}
				if (data[5] == '.')
				{
					dirSizes.push_back(current);
					if (current <= 100000)
						r1 += current;
					current += stack.back();
					stack.pop_back();
					data += 8; // "$ cd ..\n"
					continue;
				}

				stack.push_back(current);
				current = 0;
				data += findchar(data, '\n') + 1; // "$ cd <dirname>\n"
				continue;
			}

			data += 5; // "$ ls\n"
			continue;
		}

		if (data[0] != 'd')
			current += conv(data, findchar(data, ' '));

		data += findchar(data, '\n') + 1; // "dir <name>\n" or "<size> <name>\n"
	}

	while (!stack.empty())
	{
		dirSizes.push_back(current);
		if (current <= 100000)
			r1 += current;
		current += stack.back();
		stack.pop_back();
	}
	dirSizes.push_back(current);

	parse.Stop();

	uint min = dirSizes.back() - 40'000'000;
	uint r2 = ~0u;
	for (auto d : dirSizes)
	{
		if (d >= min && d < r2)
			r2 = d;
	}

	t.Stop();
	std::wcout << std::format(L"==[ {} ]==========\nTime: {}us (Parse: {}us)\n{}\n{}\n\n", file, t.GetTime(), parse.GetTime(), r1, r2);
	return true;
}

int main()
{
	run(L"input.txt");
	run(L"aoc_2022_day07_deep.txt");
	run(L"aoc_2022_day07_deep-2.txt");
	run(L"aoc_2022_day07_large.txt");
}
