import core;
import util;
using namespace util;


int findspace(std::span<const char> data)
{
	auto s = _mm256_loadu_epi8(data.data());
	auto spaces = _mm256_cmpeq_epi8(s, _mm256_set1_epi8(' '));
	uint mvmask = _mm256_movemask_epi8(spaces);
	return std::countr_zero(mvmask);
}


bool run(const wchar_t* file)
{
	Timer t(AutoStart);
	MemoryMappedFile mmap(file);
	if (!mmap)
		return (std::wcerr << std::format(L"Error opening `{}`\n", file)), false;

	Timer parse(AutoStart);
	Splitter lines(mmap.GetSpan<const char>(), '\n');
	std::vector<uint> dirSizes;
	std::vector<uint> stack;
	uint current = 0;
	uint r1 = 0;

	dirSizes.reserve(200000);
	stack.reserve(100000);

	for (auto l : lines)
	{
		if (l[0] == '$')
		{
			if (l[2] == 'c')
			{
				if (l[5] == '/')
				{
					while (!stack.empty())
					{
						dirSizes.push_back(current);
						if (current <= 100000)
							r1 += current;
						current += stack.back();
						stack.pop_back();
					}
					continue;
				}
				if (l[5] == '.')
				{
					dirSizes.push_back(current);
					if (current <= 100000)
						r1 += current;
					current += stack.back();
					stack.pop_back();

					continue;
				}
				stack.push_back(current);
				current = 0;
				continue;
			}

			continue;
		}

		if (l[0] == 'd')
			continue;

		current += conv(l.subspan(0, findspace(l)));

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
