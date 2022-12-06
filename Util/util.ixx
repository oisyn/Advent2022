module;

#pragma warning(disable:4005)
#pragma warning(disable:5106)
#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <immintrin.h>

export module util;

import core;

export
{

using uchar = unsigned char;
using ushort = unsigned short;
using uint = unsigned;
using ullong = unsigned long long;

}

export namespace util
{

class MemoryMappedFile
{
public:
	MemoryMappedFile();
	MemoryMappedFile(const wchar_t* path);
	MemoryMappedFile(MemoryMappedFile&& other);
	~MemoryMappedFile();

	MemoryMappedFile& operator=(MemoryMappedFile&& other);

	bool Open(const wchar_t* path);
	void Close();
	bool IsOpen() const { return m_pData != nullptr; }
	ullong GetSize() const { return m_size; }

	template<class T>
	std::span<T> GetSpan() const { return std::span<T>((T*)m_pData, (T*)(const char*)m_pData + m_size); }

private:
	HANDLE m_hFile = INVALID_HANDLE_VALUE;
	HANDLE m_hMap = INVALID_HANDLE_VALUE;
	ullong m_size = 0;
	const void* m_pData = nullptr;
};


inline constexpr int MaxThreads = 32;

template<class F> auto DoParallel(F&& func)
{
	int numThreads = std::clamp((int)std::thread::hardware_concurrency() - 2, 1, MaxThreads);

	using result_t = std::invoke_result_t<std::decay_t<F>, int, int>;
	std::vector<std::future<result_t>> results;
	results.reserve(numThreads);
	for (int i = 0; i < numThreads; i++)
		results.push_back(std::async(std::launch::async, std::forward<F>(func), i, numThreads));

	return results;
}

}