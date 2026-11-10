#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace Dia::ValueSchema
{
	namespace fs = std::filesystem;

	namespace
	{
		bool HasSuffix(const std::string& name, const std::string& suffix)
		{
			const size_t n = suffix.size();
			return name.size() > n && name.compare(name.size() - n, n, suffix) == 0;
		}

		// Absolute, canonical (symlinks / "..", real case on Windows), '/' separators.
		// Falls back to the lexical absolute path if canonicalisation fails.
		bool Normalise(const fs::path& p, std::string& out)
		{
			std::error_code ec;
			fs::path c = fs::canonical(p, ec);
			if (ec) { c = fs::absolute(p, ec).lexically_normal(); }
			if (ec) { return false; }
			try { out = c.generic_string(); }
			catch (const std::exception&) { return false; }	// not representable in the narrow code page
			return true;
		}

		bool ReadText(const std::string& path, std::string& out)
		{
			std::ifstream in(fs::path(path), std::ios::binary);
			if (!in) { return false; }
			out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return !in.bad();
		}
	}

	struct SchemaDirectoryLoader::Impl
	{
		std::string					mSuffix;
		std::vector<std::string>	mRoots;
		std::vector<std::string>	mFiles;
		unsigned int				mRootsMissing = 0;
	};

	SchemaDirectoryLoader::SchemaDirectoryLoader(const char* suffix) : mImpl(new Impl()) { mImpl->mSuffix = suffix ? suffix : ".schema.json"; }
	SchemaDirectoryLoader::~SchemaDirectoryLoader() { delete mImpl; }

	void SchemaDirectoryLoader::AddRoot(const char* directory)
	{
		mImpl->mRoots.push_back(directory ? directory : "");
	}

	unsigned int SchemaDirectoryLoader::RootCount() const
	{
		return static_cast<unsigned int>(mImpl->mRoots.size());
	}

	unsigned int SchemaDirectoryLoader::Discover()
	{
		mImpl->mFiles.clear();
		mImpl->mRootsMissing = 0;

		for (const std::string& root : mImpl->mRoots)
		{
			std::error_code ec;
			const fs::path rootPath = fs::path(root);
			if (root.empty() || !fs::is_directory(rootPath, ec))
			{
				DIA_LOG_WARNING("VSC", "Schema root '%s' is not an existing directory; skipped", root.c_str());
				++mImpl->mRootsMissing;
				continue;
			}

			fs::recursive_directory_iterator it(rootPath, fs::directory_options::skip_permission_denied, ec);
			const fs::recursive_directory_iterator end;
			for (; !ec && it != end; it.increment(ec))
			{
				std::error_code fec;
				if (!it->is_regular_file(fec)) { continue; }
				std::string name;
				try { name = it->path().filename().generic_string(); }
				catch (const std::exception&) { continue; }
				if (!HasSuffix(name, mImpl->mSuffix)) { continue; }

				std::string full;
				if (Normalise(it->path(), full)) { mImpl->mFiles.push_back(full); }
			}
			if (ec)
			{
				DIA_LOG_WARNING("VSC", "Schema root '%s' walk stopped early: %s", root.c_str(), ec.message().c_str());
			}
		}

		// Deterministic: sort the whole combined list, then drop files reached from more than one root.
		std::sort(mImpl->mFiles.begin(), mImpl->mFiles.end());
		mImpl->mFiles.erase(std::unique(mImpl->mFiles.begin(), mImpl->mFiles.end()), mImpl->mFiles.end());
		return FileCount();
	}

	unsigned int SchemaDirectoryLoader::FileCount() const
	{
		return static_cast<unsigned int>(mImpl->mFiles.size());
	}

	const char* SchemaDirectoryLoader::FileAt(unsigned int index) const
	{
		return index < mImpl->mFiles.size() ? mImpl->mFiles[index].c_str() : "";
	}

	SchemaLoadStats SchemaDirectoryLoader::LoadInto(ValueSchemaRegistry& registry)
	{
		Discover();

		SchemaLoadStats stats;
		stats.mFilesFound = FileCount();
		stats.mRootsMissing = mImpl->mRootsMissing;

		std::string text;
		for (const std::string& file : mImpl->mFiles)
		{
			DIA_PROFILE_SCOPE("ValueSchema.LoadFile", Dia::Observation::Profile::Category::kDiaAssetRuntime);

			if (!ReadText(file, text))
			{
				DIA_LOG_ERROR("VSC", "Schema file '%s' skipped: could not be read", file.c_str());
				++stats.mFilesFailed;
				continue;
			}

			const LoadResult result = ValueSchemaLoader::LoadFromText(text.c_str(), file.c_str());
			if (!result.mOk)
			{
				DIA_LOG_ERROR("VSC", "Schema file '%s' skipped (entry '%s'): %s",
					result.mFailure.mFile.AsCStr(), result.mFailure.mEntry.AsCStr(), result.mFailure.mReason.AsCStr());
				++stats.mFilesFailed;
				continue;
			}

			++stats.mFilesLoaded;
			for (unsigned int i = 0; i < result.mEntries.Size(); ++i)
			{
				if (registry.Register(result.mEntries[i]) == RegisterResult::Ok) { ++stats.mEntriesLoaded; }
				else { ++stats.mEntriesRejected; }
			}
		}
		return stats;
	}
}
