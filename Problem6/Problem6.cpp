constexpr int WindowSize = 4;

using uint = unsigned int;
using ullong = unsigned long long;


template<uint WindowSize>
uint GetStart(std::span<const char> data)
{
	uint lastSeen[26] = { };
	uint* lastSeenO = lastSeen - (int)'a';
	uint nextOk = WindowSize;
	uint size = uint(data.size());
	for (uint i = 0; i < size; i++)
	{
		if (i == nextOk)
			return i;
		char c = data[i];
		nextOk = std::max(nextOk, std::exchange(lastSeenO[c], i) + WindowSize + 1);
	}

	return ~0u; // bad input!
}

int main()
{
	auto start = std::chrono::high_resolution_clock::now();

	auto filename = L"aoc22d6xxl.txt";
	HANDLE hFile = CreateFile(filename, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (hFile == INVALID_HANDLE_VALUE)
		return (std::wcerr << std::format(L"Can't open file `{}`\n", filename)), 1;
	ullong fileSize = 0;
	GetFileSizeEx(hFile, (LARGE_INTEGER*)&fileSize);

	HANDLE hMap = CreateFileMapping(hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (hMap == INVALID_HANDLE_VALUE)
		return (std::cerr << "Error creating file mapping\n"), 1;

	const char* basePtr = (const char*)MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
	const char* endPtr = basePtr + fileSize;

	constexpr uint MaxThreads = 32;
	int numThreads = std::max(1u, std::min(std::thread::hardware_concurrency() - 2, MaxThreads));
	std::vector<std::jthread> threads;
	threads.reserve(numThreads);
	uint results[MaxThreads] = { };

	for (int t = 0; t < numThreads; t++)
	{
		threads.emplace_back([=, &results](int threadIdx)
		{
			auto offset = uint(fileSize * threadIdx / numThreads);
			auto end = uint(fileSize * (threadIdx + 1) / numThreads);
			if (threadIdx)
				offset -= 20;
			std::span data{ basePtr + offset, basePtr + end };
			auto r = GetStart<14>(data);
			if (r != ~0u)
				r += offset;
			results[threadIdx] = r;
		}, t);
	}

	uint result = ~0u;
	for (int t = 0; t < numThreads; t++)
	{
		threads[t].join();
		if (results[t] != ~0u)
		{			
			result = results[t];
			break;
		}
	}

	auto d = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
	std::cout << std::format("Time: {} us\n", d);
	std::cout << result << std::endl;
}