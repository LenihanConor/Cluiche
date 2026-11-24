#include <DiaValueSchema/RampSetLoader.h>
#include <DiaValueSchema/RampLoader.h>
#include <DiaValueSchema/SchemaDirectoryLoader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Profile/DiaProfile.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <system_error>
#include <vector>

namespace Dia::ValueSchema
{
	namespace fs = std::filesystem;

	namespace
	{
		bool ReadText(const std::string& path, std::string& out)
		{
			std::ifstream in(fs::path(path), std::ios::binary);
			if (!in) { return false; }
			out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
			return !in.bad();
		}

		// Canonical absolute path ('/' separators) used to recognise the same Dia file reached through two roots.
		std::string CanonicalKey(const fs::path& p)
		{
			std::error_code ec;
			fs::path c = fs::canonical(p, ec);
			if (ec) { c = fs::absolute(p, ec).lexically_normal(); }
			try { return c.generic_string(); }
			catch (const std::exception&) { return p.generic_string(); }
		}

		void LoadFile(const std::string& file, bool isDia, RampRegistry& registry, RampLoadStats& stats)
		{
			DIA_PROFILE_SCOPE("ValueSchema.LoadRamps", Dia::Observation::Profile::Category::kDiaAssetRuntime);

			std::string text;
			if (!ReadText(file, text))
			{
				DIA_LOG_ERROR("VSC", "Ramp file '%s' skipped: could not be read", file.c_str());
				++stats.mFilesFailed;
				return;
			}
			const RampLoadResult result = RampLoader::LoadFromText(text.c_str(), file.c_str());
			if (!result.mOk)
			{
				DIA_LOG_ERROR("VSC", "Ramp file '%s' skipped: %s", file.c_str(), result.mFileReason.AsCStr());
				++stats.mFilesFailed;
				return;
			}
			++stats.mFilesLoaded;
			stats.mRampsRejected += result.mRampsRejected;
			for (unsigned int i = 0; i < result.mRamps.Size(); ++i)
			{
				if (registry.Register(result.mRamps[i], isDia, file.c_str()) == RampRegisterResult::Ok) { ++stats.mRampsLoaded; }
				else { ++stats.mRampsRejected; }
			}
		}
	}

	struct RampSetLoader::Impl
	{
		std::vector<std::string>	mDiaRoots;
		SchemaDirectoryLoader		mGameLoader{ ".ramps.json" };
	};

	RampSetLoader::RampSetLoader() : mImpl(new Impl()) {}
	RampSetLoader::~RampSetLoader() { delete mImpl; }

	void RampSetLoader::AddDiaRoot(const char* directory) { mImpl->mDiaRoots.push_back(directory ? directory : ""); }
	void RampSetLoader::AddGameRoot(const char* directory) { mImpl->mGameLoader.AddRoot(directory); }

	RampLoadStats RampSetLoader::Load(RampRegistry& registry)
	{
		RampLoadStats stats;

		// Dia set first so it always wins id clashes.
		// A Dia file reachable from two roots (same root twice, or spelled differently) loads once.
		std::set<std::string> seenDiaFiles;
		for (const std::string& root : mImpl->mDiaRoots)
		{
			std::error_code ec;
			const fs::path file = fs::path(root) / "ramps.json";
			if (root.empty() || !fs::is_regular_file(file, ec)) { continue; }
			if (!seenDiaFiles.insert(CanonicalKey(file)).second) { continue; }
			++stats.mFilesFound;
			LoadFile(file.generic_string(), true, registry, stats);
		}

		const unsigned int gameFiles = mImpl->mGameLoader.Discover();
		stats.mFilesFound += gameFiles;
		for (unsigned int i = 0; i < gameFiles; ++i)
		{
			LoadFile(mImpl->mGameLoader.FileAt(i), false, registry, stats);
		}
		return stats;
	}
}
