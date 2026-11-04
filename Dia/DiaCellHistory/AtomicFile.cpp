#include <DiaCellHistory/AtomicFile.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace Dia::CellHistory
{
	bool AtomicFile::TempPath(const char* path, char* buf, uint32_t capacity)
	{
		if (path == nullptr || buf == nullptr || capacity == 0) { return false; }
		const int n = snprintf(buf, capacity, "%s.tmp", path);
		return n > 0 && uint32_t(n) < capacity;
	}

	bool AtomicFile::Commit(const char* tmp, const char* path, char* err, uint32_t errCapacity)
	{
		std::error_code ec;
		std::filesystem::rename(tmp, path, ec);
		if (!ec) { return true; }
		Discard(tmp);
		if (err != nullptr && errCapacity > 0) { snprintf(err, errCapacity, "cannot move temporary file into place: %s", ec.message().c_str()); }
		return false;
	}

	void AtomicFile::Discard(const char* tmp)
	{
		std::error_code ec;
		std::filesystem::remove(tmp, ec);
	}
}
