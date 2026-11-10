#pragma once

// Test helper: a scratch directory under the system temp dir for schema discovery tests.
// Ships with DiaValueSchema (Testing/ convention). Created empty on construction, removed on destruction.

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace Dia::ValueSchema::Testing
{
	class SchemaTempDirectory
	{
	public:
		explicit SchemaTempDirectory(const char* name)
		{
			mRoot = std::filesystem::temp_directory_path() / "DiaValueSchemaTests" / name;
			std::error_code ec;
			std::filesystem::remove_all(mRoot, ec);
			std::filesystem::create_directories(mRoot);
		}
		~SchemaTempDirectory()
		{
			std::error_code ec;
			std::filesystem::remove_all(mRoot, ec);
		}
		SchemaTempDirectory(const SchemaTempDirectory&) = delete;
		SchemaTempDirectory& operator=(const SchemaTempDirectory&) = delete;

		// Writes text to <root>/<relPath>, creating parent folders. Returns the full path ('/' separators).
		std::string Write(const char* relPath, const std::string& text) const
		{
			const std::filesystem::path p = mRoot / relPath;
			std::filesystem::create_directories(p.parent_path());
			std::ofstream out(p, std::ios::binary | std::ios::trunc);
			out << text;
			return p.generic_string();
		}

		// Full path of <root>/<relPath> ('/' separators); relPath may be empty.
		std::string Path(const char* relPath = "") const
		{
			return (relPath && relPath[0]) ? (mRoot / relPath).generic_string() : mRoot.generic_string();
		}

	private:
		std::filesystem::path mRoot;
	};
}
