#include <DiaCellHistory/SnapshotNaming.h>
#include <DiaCellHistory/ISnapshotStore.h>
#include <DiaCellHistory/SnapshotCellFile.h>

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace Dia::CellHistory
{
	namespace
	{
		void SetError(char* err, uint32_t cap, const char* text)
		{
			if (err != nullptr && cap > 0) { snprintf(err, cap, "%s", text); }
		}
	}

	bool IsValidSnapshotName(const char* name)
	{
		if (name == nullptr || name[0] == '\0') { return false; }
		uint32_t n = 0;
		for (const char* p = name; *p != '\0'; ++p, ++n)
		{
			const char c = *p;
			const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
			if (!ok || n >= kMaxSnapshotNameChars) { return false; }
		}
		return true;
	}

	uint32_t CountSnapshots(const ISnapshotStore& store)
	{
		const char* folder = store.Folder();
		if (folder == nullptr || folder[0] == '\0') { return 0; }
		namespace fs = std::filesystem;
		static const char kPrefix[] = "snapshot_";
		static const char kSuffix[] = ".cells.json";
		const size_t prefixLen = sizeof(kPrefix) - 1, suffixLen = sizeof(kSuffix) - 1;
		uint32_t count = 0;
		std::error_code ec;
		for (fs::directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
		{
			if (!it->is_regular_file(ec)) { continue; }
			const std::string file = it->path().filename().string();
			if (file.size() > prefixLen + suffixLen && file.compare(0, prefixLen, kPrefix) == 0
				&& file.compare(file.size() - suffixLen, suffixLen, kSuffix) == 0)
			{
				++count;
			}
		}
		return count;
	}

	bool SnapshotGate::TryBegin(char* err, uint32_t errCapacity)
	{
		if (mInFlight)
		{
			SetError(err, errCapacity, "snapshot operation in progress");
			return false;
		}
		mInFlight = true;
		return true;
	}

	bool CheckTake(const char* name, bool overwrite, const ISnapshotStore& store, const SnapshotGate& gate, char* err, uint32_t errCapacity)
	{
		if (!IsValidSnapshotName(name))
		{
			if (err != nullptr && errCapacity > 0)
			{
				snprintf(err, errCapacity, "invalid snapshot name '%.40s': use 1 to %u characters from A-Z a-z 0-9 _ -",
					name != nullptr ? name : "", kMaxSnapshotNameChars);
			}
			return false;
		}
		if (gate.InFlight())
		{
			SetError(err, errCapacity, "snapshot operation in progress");
			return false;
		}
		char path[512];
		const bool havePath = SnapshotCellFile::Path(store.Folder(), name, path, sizeof(path));
		std::error_code ec;
		const bool cellFileExists = havePath && std::filesystem::exists(path, ec);
		if (!overwrite && (store.Exists(name) || cellFileExists))
		{
			if (err != nullptr && errCapacity > 0) { snprintf(err, errCapacity, "snapshot '%s' already exists (use overwrite)", name); }
			return false;
		}
		if (!cellFileExists && CountSnapshots(store) >= kMaxSnapshots)
		{
			if (err != nullptr && errCapacity > 0) { snprintf(err, errCapacity, "snapshot limit of %u reached; delete a snapshot first", kMaxSnapshots); }
			return false;
		}
		return true;
	}
}
