import core;
import util;
using namespace util;

int FindChar(const char* ptr, const char* end, char c)
{
	auto allC = _mm256_set1_epi8(c);
	constexpr uint RegSize = sizeof(allC);
	uint max = end - ptr;

	for (uint offset = 0; offset < max; offset += RegSize)
	{
		auto chars = _mm256_cmpeq_epi8(_mm256_loadu_epi8(ptr + offset), allC);
		if (uint mvmask = _mm256_movemask_epi8(chars))
			return std::countr_zero(mvmask) + offset;
	}

	return max;
}

enum TokenType
{
	TokenList,
	TokenListEnd,
	TokenNumber,
};

struct Token
{
	int value;
	TokenType type;

	Token(TokenType t) : type(t) { }
	Token(int v) : value(v), type(TokenNumber) { }
};


Token GetToken(const char*& _ptr)
{
	auto ptr = _ptr;
	if (*ptr == ',')
		ptr++;
	else if (*ptr == ']')
		return _ptr++, TokenList;

	if (*ptr == '[')
		return _ptr = ++ptr, TokenList;

	int n = *ptr - '0';
	while (*ptr != ',' && *ptr != ']')
	{
		n = n * 10 + *ptr - '0';
		ptr++;
	}
	_ptr = ptr;
	return n;
}

bool Smaller(const char* p1, const char* p2)
{
	int level = 1;
	p1++;	// skip first '['
	p2++;

	while (level)
	{
		//static const char* tokenNames[] = { "list", "list-end", "number" };
		//std::cout << std::format("p1: {}\np2: {}\n", std::string_view{ p1, strcspn(p1, "\n") }, std::string_view{ p2, strcspn(p2, "\n") });
		auto t1 = GetToken(p1);
		auto t2 = GetToken(p2);
		//std::cout << std::format("t1: {}, t2: {}\n", tokenNames[t1.index()], tokenNames[t2.index()]);


		if (t1.type == TokenList)
		{
			if (t2.type == TokenList)
			{
				level++;
				continue;
			}

			if (t2.type == TokenListEnd)
				return false;

			// t2 is a number, p1 should parse to a (nested) empty list or a (nested) single item list with number <= t2
			int nested = 1;
			t1 = GetToken(p1);
			while (t1.type == TokenList)
			{
				nested++;
				t1 = GetToken(p1);
			}
			if (t1.type == TokenListEnd)	// (nested) empty list < any number
				return true;

			if (t1.value != t2.value)
				return t1.value < t2.value;

			while (nested)	// undo nesting
			{
				t1 = GetToken(p1);
				if (t1.type != TokenListEnd)
					return false;
				nested--;
			}
		}
		else if (t1.type == TokenListEnd)
		{
			if (t2.type != TokenListEnd)
				return true;
			level--;
		}
		else
		{
			// t1 is number
			if (t2.type == TokenListEnd)
				return false;
			if (t2.type == TokenNumber)
			{
				if (t1.value != t2.value)
					return t1.value < t2.value;
				continue;
			}

			// t2 is a list, p2 should parse to a (nested) list with one number or more, where the first number >= t1
			int nested = 1;
			t2 = GetToken(p2);
			while (t2.type == TokenList)
			{
				nested++;
				t2 = GetToken(p2);
			}
			if (t2.type == TokenListEnd)	// any number > (nested) empty list
				return false;

			if (t1.value != t2.value)
				return t1.value < t2.value;

			while (nested)	// undo nesting
			{
				t2 = GetToken(p2);
				if (t2.type != TokenListEnd)
					return true;
				nested--;
			}
		}
	}

	return false;
}

std::pair<int, int> CheckKeys(const char* p1, const char* p2)	// assumes Smaller(p1, p2)
{
	constexpr auto Key1 = "[[2]]", Key2 = "[[6]]";
	int nkey1 = 0, nkey2 = 0;
	if (Smaller(p1, Key1))
	{
		//std::cout << std::string_view{ p1 - 1, strcspn(p1, "\n") + 1 } << std::endl;
		nkey1++, nkey2++;
		if (Smaller(p2, Key1))
		{
			//std::cout << std::string_view{ p2 - 1, strcspn(p2, "\n") + 1 } << std::endl;
			nkey1++, nkey2++;
		}
		else if (Smaller(p2, Key2))
			nkey2++;
	}
	else if (Smaller(p1, Key2))
	{
		nkey2++;
		if (Smaller(p2, Key2))
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

		if (Smaller(ptr, ptr2))
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
