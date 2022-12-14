module;

#pragma warning(disable:4005)
#pragma warning(disable:5106)
#define WINDOWS_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <immintrin.h>

module util;

namespace util
{


////////////////////////////////////////////////////////////////////////////////////////////////
MemoryMappedFile::MemoryMappedFile()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////
MemoryMappedFile::MemoryMappedFile(const wchar_t* path, bool copyOnWrite)
{
	Open(path, copyOnWrite);
}

////////////////////////////////////////////////////////////////////////////////////////////////
MemoryMappedFile::MemoryMappedFile(MemoryMappedFile&& other)
{
	*this = std::move(other);
}

////////////////////////////////////////////////////////////////////////////////////////////////
MemoryMappedFile::~MemoryMappedFile()
{
	Close();
}

////////////////////////////////////////////////////////////////////////////////////////////////
MemoryMappedFile& MemoryMappedFile::operator=(MemoryMappedFile&& other)
{
	Close();
	m_hFile = std::exchange(other.m_hFile, INVALID_HANDLE_VALUE);
	m_hMap = std::exchange(other.m_hMap, INVALID_HANDLE_VALUE);
	m_size = std::exchange(other.m_size, 0);
	m_pData = std::exchange(other.m_pData, nullptr);
	return *this;
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool MemoryMappedFile::Open(const wchar_t* path, bool copyOnWrite)
{
	m_hFile = CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
	if (m_hFile == INVALID_HANDLE_VALUE)
		return false;
	GetFileSizeEx(m_hFile, (LARGE_INTEGER*)&m_size);

	m_hMap = CreateFileMapping(m_hFile, nullptr, copyOnWrite ? PAGE_WRITECOPY : PAGE_READONLY, 0, 0, nullptr);
	if (m_hMap == INVALID_HANDLE_VALUE)
		return false;

	m_pData = MapViewOfFile(m_hMap, copyOnWrite ? FILE_MAP_COPY : FILE_MAP_READ, 0, 0, 0);
	return (bool)m_pData;
}

////////////////////////////////////////////////////////////////////////////////////////////////
void MemoryMappedFile::Close()
{
	if (!IsOpen())
		return;

	UnmapViewOfFile(m_pData);
	CloseHandle(m_hMap);
	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
	m_hMap = INVALID_HANDLE_VALUE;
	m_size = 0;
	m_pData = nullptr;
}

}