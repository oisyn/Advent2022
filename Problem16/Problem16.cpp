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

	while (ptr < endPtr)
	{
		ptr += 6;
		int valveIdx = (ptr[0] - 'A') * 26 + ptr[1] - 'A';
		auto& valve = valves[valveIdx];

		ptr += 17;
		int n = FindChar(ptr, ';');
		valve.rate = conv(ptr, n);
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
	{
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
		}
		if (!valves[0].rate)
			relevantValves.pop_back();

		struct State { int rate, time, valveIdx; ullong usedValves; };

		constexpr int MaxTime = 30;
		std::deque<State> stateQueue;
		stateQueue.emplace_back(0, MaxTime);
		if (valves[0].rate)	// if we can open the start valve, also try that as first action
			stateQueue.emplace_back((MaxTime - 1) * valves[0].rate, MaxTime - 1, 0, 1);

		while (!stateQueue.empty())
		{
			auto state = stateQueue.front();
			stateQueue.pop_front();

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
				maxRate = std::max(maxRate, newState.rate);
				newState.valveIdx = v;
				newState.usedValves |= valveMask;
				stateQueue.push_back(newState);
			}
		}
	}
	std::cout << "Part1 done\n";
	talgo1.Stop();

	Timer talgo2(AutoStart);
	int maxRate2 = 0;
	{
		struct State { int rate, time, valveIdx, valveIdxOther, otherTimeLeft; ullong usedValves; };

		constexpr int MaxTime = 26;
		static std::deque<State> stateQueue;
		stateQueue.emplace_back(0, MaxTime);
		if (valves[0].rate)	// if we can open the start valve, also try that as first action
		{
			State newState = { valves[0].rate * (MaxTime - 1), MaxTime - 1 };
			newState.usedValves = 1;
			for (auto [v, c] : valves[0].valveCosts)
			{
				if (c >= MaxTime)
					continue;
				newState.rate += (MaxTime - c - 1) * valves[v].rate;
				maxRate2 = std::max(maxRate2, newState.rate);
				newState.valveIdxOther = v;
				newState.otherTimeLeft = c;
				newState.usedValves = 1 | (1ull << valves[v].relIdx);
				stateQueue.push_back(newState);
			}
		}

		int iterations = 0;
		while (!stateQueue.empty())
		{
			auto state = stateQueue.front();
			stateQueue.pop_front();

			//if (!(++iterations % 1000000))
			//	std::cout << std::format("\r{}      ", stateQueue.size());

			auto& valve = valves[state.valveIdx];
			auto usedValves = state.usedValves;

			for (auto [v, c] : valve.valveCosts)
			{
				ullong valveMask = 1ull << valves[v].relIdx;
				if (usedValves & valveMask)
					continue;

				if (c > state.time)
					continue;

				auto newState = state;
				newState.rate += (state.time - c) * valves[v].rate;
				maxRate2 = std::max(maxRate2, newState.rate);
				newState.valveIdx = v;
				newState.usedValves |= valveMask;

				if (newState.otherTimeLeft)
				{	// the other one is still busy, figure out which one finishes first
					if (newState.otherTimeLeft < c)
					{
						newState.time -= newState.otherTimeLeft;
						newState.otherTimeLeft = c - newState.otherTimeLeft;
						std::swap(newState.valveIdx, newState.valveIdxOther);
					}
					else
					{
						newState.time -= c;
						newState.otherTimeLeft -= c;
					}
					stateQueue.push_back(newState);
				}
				else
				{	// the other one finished as well
					usedValves |= valveMask; // make sure we don't revisit states that we already generated a state for

					auto& valve2 = valves[state.valveIdxOther];
					for (auto [v2, c2] : valve2.valveCosts)
					{
						ullong valveMask2 = 1ull << valves[v2].relIdx;
						if (usedValves & valveMask2)
							continue;

						if (c2 > state.time)
							continue;

						State newState2 = newState;
						newState2.rate += (state.time - c2) * valves[v2].rate;
						maxRate2 = std::max(maxRate2, newState2.rate);
						newState2.valveIdxOther = v2;
						newState2.usedValves |= valveMask2;
						usedValves |= valveMask2; // make sure we don't revisit states that we already generated a state for

						if (c2 < c)
						{
							newState2.time -= c2;
							newState2.otherTimeLeft = c - c2;
							std::swap(newState.valveIdx, newState2.valveIdxOther);
						}
						else
						{
							newState2.time -= c;
							newState2.otherTimeLeft = c2 - c;
						}
						stateQueue.push_back(newState2);
					}
				}

			}
		}

	}

	talgo2.Stop();

	total.Stop();
	std::cout << std::format(std::locale("en-US"), "Time: {:L}us (load:{:L}us, algo1:{:L}us, algo2:{:L}us)\nPart 1: {}\nPart 2: {}\n", total.GetTime(), tload.GetTime(), talgo1.GetTime(), talgo2.GetTime(), maxRate, maxRate2);

	return true;
}


int main()
{
	const wchar_t* inputs[] =
	{
		L"example.txt",
		L"input.txt",
		//L"aoc_2022_day15_large-1.txt",
		//L"aoc_2022_day15_large-2.txt",
		//L"aoc_2022_day15_large-3.txt",
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
