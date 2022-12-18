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

constexpr bool Verbose = false;

template<class... T>
void vprintln(std::_Fmt_string<T...> fmt, T&&... args)
{
	if constexpr (Verbose)
	{
		static std::locale loc("en-US");
		static Timer lastTime;
		static bool running = false;

		if (running)
		{
			lastTime.Stop();
			println("-- {:L}us", lastTime.GetTime());
		}
		println(fmt, std::forward<T>(args)...);
		running = true;
		lastTime.Restart();
	}
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

	struct Valve
	{
		int rate = 0;
		int relIdx = -1;
		std::vector<int> tunnels;
		std::vector<std::pair<int, int>> valveCosts;
	};
	constexpr int MaxValves = 26 * 26;
	std::vector<Valve> valves(MaxValves);
	std::vector<int> relevantValves;
	int totalFlowRate = 0;


	vprintln("Reading file");

	while (ptr < endPtr)
	{
		ptr += 6;
		int valveIdx = (ptr[0] - 'A') * 26 + ptr[1] - 'A';
		auto& valve = valves[valveIdx];

		ptr += 17;
		int n = FindChar(ptr, ';');
		valve.rate = conv(ptr, n);
		totalFlowRate += valve.rate;
		ptr += n + 24;
		if (*ptr == ' ')
			ptr++;

		int tunnel = (ptr[0] - 'A') * 26 + ptr[1] - 'A';
		valve.tunnels.push_back(tunnel);
		while (ptr[2] == ',')
		{
			ptr += 4;
			tunnel = (ptr[0] - 'A') * 26 + ptr[1] - 'A';
			valve.tunnels.push_back(tunnel);
		}
		ptr += 3;

		if (valve.rate)
		{
			valve.relIdx = int(relevantValves.size());
			relevantValves.push_back(valveIdx);
		}
	}

	tload.Stop();

	Timer talgo1(AutoStart);
	int maxRate = 0;
	std::unordered_map<ullong, int> maxScores;
	{
		vprintln("Calculating distances");

		// find shortest path from each relevant valve to all other relevant valves
		std::deque<int> queue;
		std::vector<int> costs(MaxValves);
		if (!valves[0].rate)
			relevantValves.push_back(0);
		for (auto valveIdx : relevantValves)
		{
			queue.clear();
			queue.push_back(valveIdx);
			std::ranges::fill(costs, std::numeric_limits<int>::max());
			costs[valveIdx] = 0;

			while (!queue.empty())
			{
				int v = queue.front();
				queue.pop_front();

				int c = costs[v] + 1;
				if ((!valveIdx || valves[valveIdx].rate) && valves[v].rate)
					valves[valveIdx].valveCosts.emplace_back(v, c);
				for (auto t : valves[v].tunnels)
				{
					if (costs[t] > c)
					{
						costs[t] = c;
						queue.push_back(t);
					}
				}
			}

			std::ranges::sort(valves[valveIdx].valveCosts, [&](auto& a, auto& b) { return valves[a.first].rate > valves[b.first].rate; });
		}
		if (!valves[0].rate)
			relevantValves.pop_back();

		vprintln("Running part 1");
		struct State { int rate, time, valveIdx; ullong usedValves; int pureRate; };

		constexpr int MaxTime = 30;
		std::vector<State> stateQueue;
		stateQueue.emplace_back(0, MaxTime);
		if (valves[0].rate)	// if we can open the start valve, also try that as first action
			stateQueue.emplace_back((MaxTime - 1) * valves[0].rate, MaxTime - 1, 0, 1);

		int statePos = 0;
		while (statePos < (int)stateQueue.size())
		{
			auto state = stateQueue[statePos++];

			auto& valve = valves[state.valveIdx];

			for (auto [v, c] : valve.valveCosts)
			{
				ullong valveMask = 1ull << valves[v].relIdx;
				if (state.usedValves & valveMask)
					continue;

				auto newState = state;
				newState.time -= c;
				if (newState.time < 0)
					continue;
				newState.rate += newState.time * valves[v].rate;
				newState.pureRate += valves[v].rate;
				maxRate = std::max(maxRate, newState.rate);
				newState.valveIdx = v;
				newState.usedValves |= valveMask;

				if (newState.time > 1 && newState.rate + (totalFlowRate - newState.pureRate) * (newState.time - 1) > maxRate)
					stateQueue.push_back(newState);

				if (newState.time >= 4)
				{
					int rate = newState.rate - 4 * newState.pureRate;
					auto& score = maxScores[newState.usedValves];
					if (score < rate)
						score = rate;
				}
			}
		}
	} 
	talgo1.Stop();

	Timer talgo2(AutoStart);
	vprintln("Running part 2");
	int maxRate2 = 0;
	{
		int num = 0;
		for (auto it = maxScores.begin(); it != maxScores.end(); ++it)
		{
			auto [m, s] = *it;
			for (auto it2 = std::next(it); it2 != maxScores.end(); ++it2)
			{
				auto [m2, s2] = *it2;
				if (m & m2)
					continue;
				//if (s + s2 > maxRate2)
				//	std::cout << std::format("{:b} + {:b} -> {}+{}={}\n", m, m2, s, s2, s + s2);
				maxRate2 = std::max(maxRate2, s + s2);
			}
		}
	}

	talgo2.Stop();

	total.Stop();
	println("Time: {:L}us (load:{:L}us, part1:{:L}us, part2:{:L}us)\nPart 1: {}\nPart 2: {}", total.GetTime(), tload.GetTime(), talgo1.GetTime(), talgo2.GetTime(), maxRate, maxRate2);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		//L"example.txt",
		L"input.txt",
		//L"aoc_2022_day16_large-1.txt",
		//L"aoc_2022_day16_large-2.txt",
		//L"aoc_2022_day16_large-3.txt",
		//L"aoc_2022_day16_large-4.txt",
		//L"aoc_2022_day16_large-5.txt",
		//L"aoc_2022_day16_large-6.txt",
		//L"aoc_2022_day16_large-7.txt",
	};

	constexpr int NumRuns = 1;
	for (auto f : inputs)
	{
		println(L"\n===[ {} ]==========", f);
		for (int i = 0; i < NumRuns; i++)
		{
			if (!Run(f))
				eprintln(L"Can't open `{}`", f);
		}
	}
}
