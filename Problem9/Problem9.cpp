import core;
import util;
using namespace util;

enum MoveDir : char
{
	UpLeft,
	Up,
	UpRight,
	Left,
	None,
	Right,
	DownLeft,
	Down,
	DownRight,
};

constexpr MoveDir DirFromDeltas(int dx, int dy)
{
	return (MoveDir)(dy * 3 + dx + 4);
}

constexpr std::pair<int, int> DeltasFromDir(int dir)
{
	return { dir % 3 - 1, dir / 3 - 1 };
}

constexpr auto GenerateDirLookup()
{
	std::array<MoveDir, 256> l = {};
	l['L'] = Left;
	l['R'] = Right;
	l['U'] = Up;
	l['D'] = Down;
	return l;
}
constexpr std::array<MoveDir, 256> DirLookup = GenerateDirLookup();

struct Move
{
	char nextState;
	char dx;
	char dy;
	char moveDir;
};

constexpr int Sign(int i)
{
	return i < 0 ? -1 : i > 0 ? 1 : 0;
}

constexpr auto GenerateStates()
{
	std::array<std::array<Move, 9>, 9> s{};

	for (int old = 0; old < 9; old++)
	{
		auto [x, y] = DeltasFromDir(old);

		for (int n = 0; n < 9; n++)
		{
			auto [dx, dy] = DeltasFromDir(n);

			int nx = x - dx;	// tails moves relatively in opposite direction
			int ny = y - dy;

			if (nx < -1 || nx > 1 || ny < -1 || ny > 1)
			{
				dx = -Sign(nx);
				dy = -Sign(ny);
				nx += dx;
				ny += dy;
			}
			else
			{
				dx = 0;
				dy = 0;
			}

			int newStateIdx = DirFromDeltas(nx, ny);
			s[old][n] = { char(newStateIdx), char(dx), char(dy), DirFromDeltas(dx, dy) };
		}
	}

	return s;
}
constexpr std::array<std::array<Move, 9>, 9> States = GenerateStates();

namespace std
{
	template<class T>
	struct std::hash<std::pair<T, T>>
	{
		size_t operator()(const std::pair<T, T>& p) const
		{
			std::hash<T> hasher;
			return hasher(p.first) ^ hasher(p.second);
		}
	};
}


constexpr uint BitsPerLevel = 16;
constexpr uint MaxLevel = (64 + BitsPerLevel - 1) / BitsPerLevel - 1;

template<uint Level>
struct BitPage;

template<>
struct BitPage<0>
{
	static constexpr ullong WordSize = 64;
	static constexpr ullong WordShift = 6;
	static constexpr ullong NumWords = (1ll << BitsPerLevel) / WordSize;
	static constexpr ullong LevelMask = NumWords - 1;

	bool Set(ullong b)
	{
		ullong idx = (b >> WordShift) & LevelMask;
		ullong m = 1ull << (b & WordSize - 1);

		auto old = words[idx];
		words[idx] |= m;
		return !(old & m);
	}

	std::array<ullong, NumWords> words{};
};

template<uint Level>
struct BitPage
{
	using PageType = BitPage<Level - 1>;
	using PagePtr = std::unique_ptr<PageType>;
	static constexpr ullong ElemsPerPage = 1ull << BitsPerLevel;
	static constexpr ullong LevelShift = Level * BitsPerLevel;
	static constexpr ullong LevelMask = ElemsPerPage - 1;

	bool Set(ullong b)
	{
		ullong idx = (b >> LevelShift) & LevelMask;
		if (!pages[idx])
			pages[idx] = std::make_unique<PageType>();
		return pages[idx]->Set(b);
	}

	std::array<PagePtr, ElemsPerPage> pages;
};

class BitGrid
{
public:
	bool Set(int x, int y)
	{
		if (!m_topPage)
			m_topPage = std::make_unique<BitPage<MaxLevel>>();

		auto v = _mm_setr_epi32(x, y, 0, 0);
		v = _mm_shuffle_epi8(v, _mm_setr_epi8(0, 4, 1, 5, 2, 6, 3, 7, 8, 8, 8, 8, 8, 8, 8, 8));
		ullong b = _mm_extract_epi64(v, 0);
		return m_topPage->Set(b);
	}

	void Clear()
	{
		m_topPage.reset();
	}

private:
	std::unique_ptr<BitPage<MaxLevel>> m_topPage;
};


bool Run(const wchar_t* file)
{
	Timer total(AutoStart);
	Timer tload(AutoStart);
	MemoryMappedFile mmap(file);
	if (!mmap)
		return false;
	int size = (int)mmap.GetSize();
	auto data = mmap.GetSpan<const char>();
	const char* ptr = (const char*)data.data();
	const char* end = ptr + size;
	tload.Stop();

	GenerateStates();

	Timer tsimulate(AutoStart);
	std::vector<ullong> coords;
	//std::vector<std::pair<int, int>> coords;
	constexpr int NumKnots = 9;
	int x = 0, y = 0;
	int knotStates[NumKnots] = { };
	std::ranges::fill(knotStates, 4);
	//int minx = 0, miny = 0, maxx = 0, maxy = 0;

	BitGrid grid;
	int numCells = 0;

	while (ptr < end)
	{
		char d = *ptr;
		ptr += 2;
		int n = int(std::find(ptr, end, '\n') - ptr);
		int num = conv(ptr, n);
		ptr += n + 1;

		MoveDir dir = DirLookup[d & 0xff];
		for (int i = 0; i < num; i++)
		{
			int d = dir;
			Move lastMove;
			for (int k = 0; k < NumKnots; k++)
			{
				auto& state = knotStates[k];
				lastMove = States[state][d];
				state = lastMove.nextState;
				d = lastMove.moveDir;
			}
			x += lastMove.dx;
			y += lastMove.dy;
			//minx = std::min(minx, x);
			//miny = std::min(miny, y);
			//maxx = std::max(maxx, x);
			//maxy = std::max(maxy, y);
			//std::cout << std::format("({}, {})\n", x, y);

			//coords.emplace_back((ullong(y) << 32) | (uint)x);
			//coords.emplace_back(x, y);
			
			numCells += grid.Set(x, y);
		}
	}
	tsimulate.Stop();

	Timer tsort(AutoStart);
	//std::ranges::sort(coords);
	//auto numCells = std::ranges::unique(coords).begin() - coords.begin();
	tsort.Stop();

	total.Stop();
	std::cout << std::format(std::locale(""), "Time: {:L}us (load:{}us, sim:{:L}us, sort:{:L}us)\n{}\n", total.GetTime(), tload.GetTime(), tsimulate.GetTime(), tsort.GetTime(), numCells);
	//std::cout << std::format("({}, {}) - ({}, {})\n", minx, miny, maxx, maxy);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"example.txt",
		//L"example-2.txt",
		L"input.txt",
		L"aoc_2022_day09_large-1.txt",
		L"aoc_2022_day09_large-2.txt",
	};

	constexpr int NumRuns = 1;
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