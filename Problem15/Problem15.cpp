import core;
import util;
using namespace util;

int FindChar(const char* ptr, char c)
{
	auto allC = _mm256_set1_epi8(c);
	constexpr uint RegSize = sizeof(allC);

	auto chars = _mm256_cmpeq_epi8(_mm256_loadu_epi8(ptr), allC);
	if (uint mvmask = _mm256_movemask_epi8(chars))
		return std::countr_zero(mvmask);

	return -1;
}

int convn(const char* ptr, int n)
{
	if (*ptr == '-')
		return -int(conv(ptr + 1, n - 1));
	return conv(ptr, n);
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

	struct Sensor
	{
		int x, y;
		int minDist;
	};
	std::vector<Sensor> sensors;
	std::vector<int> beaconsOnRow;
	constexpr int Row = 2'000'000;

	while (ptr < endPtr)
	{
		ptr += 12;
		int n = FindChar(ptr, ',');
		int x = convn(ptr, n);
		ptr += n + 4;
		n = FindChar(ptr, ':');
		int y = convn(ptr, n);
		ptr += n + 25;
		n = FindChar(ptr, ',');
		int bx = convn(ptr, n);
		ptr += n + 4;

		n = FindChar(ptr, '\n');
		if (n == -1)
			n = FindChar(ptr, 0);
		int by = convn(ptr, n);
		ptr += n + 1;

		if (by == Row && !std::ranges::contains(beaconsOnRow, bx))
			beaconsOnRow.push_back(bx);

		int minDist = std::abs(bx - x) + std::abs(by - y);
		sensors.emplace_back(x, y, minDist);
	}
	tload.Stop();

	Timer talgo1(AutoStart);
	std::vector<std::pair<int, int>> spans;
	spans.reserve(1000);
	for (auto sensor : sensors)
	{
		int rowDist = std::abs(sensor.y - Row);
		if (rowDist <= sensor.minDist)
		{
			int delta = sensor.minDist - rowDist;
			int minx = sensor.x - delta;
			int maxx = sensor.x + delta;

			auto it = std::lower_bound(spans.begin(), spans.end(), minx, [](auto& span, int x) { return span.second < x; });
			if (it == spans.end())
				spans.emplace_back(minx, maxx);
			else if (it->first > maxx + 1)
				spans.emplace(it, minx, maxx);
			else
			{
				it->first = std::min(it->first, minx);
				auto next = std::next(it), last = it;
				while (next != spans.end() && next->first <= maxx)
					last = next++;
				it->second = std::max(maxx, last->second);
				spans.erase(std::next(it), next);
			}
		}
	}

	int result1 = 0;
	for (auto& s : spans)
		result1 += s.second - s.first + 1;
	result1 -= (int)beaconsOnRow.size();
	talgo1.Stop();

	Timer talgo2(AutoStart);
	constexpr int BoxMin = 0, BoxMax = 4'000'000;
	constexpr ullong TuningMul = 4'000'000;
	ullong result2 = 0;
	auto CheckSensors = [](auto& sensors, int x, int y)
	{
		for (auto sensor : sensors)
		{
			int d = std::abs(x - sensor.x) + std::abs(y - sensor.y);
			if (d <= sensor.minDist)
				return false;
		}
		return true;
	};

	struct Line { int x0, y0, x1; };

	std::vector<Line> linesDown, linesUp;
	linesDown.reserve(sensors.size());
	linesUp.reserve(sensors.size());
	for (auto i : std::views::iota(0, (int)sensors.size() - 1))
	{
		for (auto j : std::views::iota(i + 1, (int)sensors.size()))
		{
			if (std::abs(sensors[i].x - sensors[j].x) + std::abs(sensors[i].y - sensors[j].y) == sensors[i].minDist + sensors[j].minDist + 2)
			{
				//std::cout << std::format("({}, {}) - ({}, {})\n", sensors[i].x, sensors[i].y, sensors[j].x, sensors[j].y);

				auto p = i, q = j;
				if (sensors[i].x > sensors[j].x)
					std::swap(p, q);
				int pr = sensors[p].minDist + 1;
				int qr = sensors[q].minDist + 1;
				if (sensors[p].y < sensors[q].y)
				{	// diagonal up with +x
					Line l;
					l.x0 = std::max(sensors[p].x, sensors[q].x - qr);
					l.y0 = std::min(sensors[p].y + pr, sensors[q].y);
					l.x1 = std::min(sensors[p].x + pr, sensors[q].x);
					linesUp.push_back(l);
				}
				else
				{	// diagonal down with +x
					Line l;
					l.x0 = std::max(sensors[p].x, sensors[q].x - qr);
					l.y0 = std::max(sensors[p].y - pr, sensors[q].y);
					l.x1 = std::min(sensors[p].x + pr, sensors[q].x);
					linesDown.push_back(l);
				}
			}
		}
	}

	for (auto& p : linesUp)
	{
		for (auto& q : linesDown)
		{
			int dx = p.x0 - q.x0;
			int dy = p.y0 - q.y0;
			if ((dy + dx) & 1)
				continue;	// should be even
			int d = (dx + dy) / 2;
			int x = q.x0 + d;
			int y = q.y0 + d;
			if (x < q.x0 || x > q.x1 || x < p.x0 || x > p.x1)
				continue;

			if (CheckSensors(sensors, x, y))
			{
				result2 = x * TuningMul + y;
				goto done;
			}
		}
	}

done:
	talgo2.Stop();
	
	total.Stop();
	std::cout << std::format(std::locale("en-US"), "Time: {:L}us (load:{:L}us, algo1:{:L}us, algo2:{:L}us)\nPart 1: {}\nPart 2: {}\n", total.GetTime(), tload.GetTime(), talgo1.GetTime(), talgo2.GetTime(), result1, result2);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"example.txt",
		//L"input.txt",
		//L"input-soultaker.txt",
		//L"input-frankmennink.txt",
		//L"aoc_2022_day15_large-1.txt",
		//L"aoc_2022_day15_large-2.txt",
		L"aoc_2022_day15_large-3.txt",
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
