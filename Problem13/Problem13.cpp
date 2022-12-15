import core;
import util;
using namespace util;

int FindChar(const char* ptr, const char* end, char c)
{
	auto allC = _mm256_set1_epi8(c);
	constexpr uint RegSize = sizeof(allC);
	uint max = uint(end - ptr);

	for (uint offset = 0; offset < max; offset += RegSize)
	{
		auto chars = _mm256_cmpeq_epi8(_mm256_loadu_epi8(ptr + offset), allC);
		if (uint mvmask = _mm256_movemask_epi8(chars))
			return std::countr_zero(mvmask) + offset;
	}

	return max;
}


constexpr bool Digit(char c)
{
	return (c & 0b0011'0000) == 0b0011'0000;
}

int GetNumber(const char*& ptr)
{
	int n = *ptr++ - '0';
	while (Digit(*ptr)) // (*ptr != ',' && *ptr != ']')
		n = n * 10 + *ptr++ - '0';
	return n;
}

bool ComesBefore(const char* p1, const char* p2)
{
	p1++;	// skip first '['
	p2++;

	for(;;)
	{
		while (*p1 == *p2 && !Digit(*p1))
			p1++, p2++;

		if (*p1 == ']')
			return true;
		if (*p2 == ']')
			return false;

		if (*p1 == '[')
		{
			// p2 is a number, p1 should parse to a (nested) empty list or a (nested) single item list with number <= p2
			auto pstart = p1;
			while (*++p1 == '[');
			if (*p1 == ']')	// (nested) empty list < any number
				return true;
			int nested = int(p1 - pstart);

			int n1 = GetNumber(p1);
			int n2 = GetNumber(p2);
			if (n1 != n2)
				return n1 < n2;

			while (nested--)	// undo nesting
				if (*p1++ != ']')
					return false;
		}
		else if (*p2 == '[')
		{
			// p1 is a number, p2 should parse to a (nested) list with one number or more, where the first number >= p1
			auto pstart = p2;
			while (*++p2 == '[');
			if (*p2 == ']')	// (nested) empty list > any number
				return false;
			int nested = int(p2 - pstart);

			int n1 = GetNumber(p1);
			int n2 = GetNumber(p2);
			if (n1 != n2)
				return n1 < n2;

			while (nested--)	// undo nesting
				if (*p2++ != ']')
					return true;
		}
		else
		{
			// both numbers
			int n1 = GetNumber(p1);
			int n2 = GetNumber(p2);
			if (n1 != n2)
				return n1 < n2;
		}

		while (*p1 == ']')
		{
			if (*p2++ != ']')
				return true;
			p1++;
		}

		if (*p2 == ']')
			return false;

		p1++;	// both ','
		p2++;
	}
}

bool ComesBeforeKey(const char* p, int k)
{
	char c = k + '0';

	while (*++p == '[');
	if (*p != ']' && *p++ >= c)
		return false;
	return !Digit(*p);
}

std::pair<int, int> CheckKeys(const char* p1, const char* p2)	// assumes Smaller(p1, p2)
{
	constexpr auto Key1 = 2, Key2 = 6;
	int nkey1 = 0, nkey2 = 0;

	if (ComesBeforeKey(p1, Key1))
	{
		//std::cout << std::string_view{ p1 - 1, strcspn(p1, "\n") + 1 } << std::endl;
		nkey1++, nkey2++;
		if (ComesBeforeKey(p2, Key1))
		{
			//std::cout << std::string_view{ p2 - 1, strcspn(p2, "\n") + 1 } << std::endl;
			nkey1++, nkey2++;
		}
		else if (ComesBeforeKey(p2, Key2))
			nkey2++;
	}
	else if (ComesBeforeKey(p1, Key2))
	{
		nkey2++;
		if (ComesBeforeKey(p2, Key2))
			nkey2++;
	}

	return { nkey1, nkey2 };
}

bool Run(const wchar_t* file)
{
	Timer total(AutoStart);
	Timer tload(AutoStart);
	MemoryMappedFile mmap(file);
	if (!mmap)
		return false;
	int size = (int)mmap.GetSize();
	auto data = mmap.GetSpan<const char>();
	const char* ptr = data.data();
	const char* endPtr = ptr + size;

	tload.Stop();

	Timer talgo(AutoStart);
	int pair = 1;
	int result = 0;
	int nkey1 = 1, nkey2 = 2;
	while (ptr < endPtr)
	{
		int nl = FindChar(ptr, endPtr, '\n');
		auto ptr2 = ptr + nl + 1;

		if (ComesBefore(ptr, ptr2))
		{
			//std::cout << pair << std::endl;
			result += pair;
			auto [n1, n2] = CheckKeys(ptr, ptr2);
			nkey1 += n1;
			nkey2 += n2;
		}
		else
		{
			auto [n1, n2] = CheckKeys(ptr2, ptr);
			nkey1 += n1;
			nkey2 += n2;
		}

		ptr = ptr2 + FindChar(ptr2, endPtr, '\n') + 2;
		pair++;
	}

	talgo.Stop();
	total.Stop();
	std::cout << std::format(std::locale("en-US"), "Time: {:L}us (load:{:L}us, algo:{:L}us)\nPart 1: {}\nPart 2: {}\n", total.GetTime(), tload.GetTime(), talgo.GetTime(), result, nkey1*nkey2);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"example.txt",
		L"input.txt",
		L"aoc_2022_day13_large-1.txt",
		L"aoc_2022_day13_large-2.txt",
		L"aoc_2022_day13_large-3.txt",
	};

	constexpr int NumRuns = 5;
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
