#include "DiaGameManifest.h"

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Game
	{
		DiaGameManifest::DiaGameManifest()
			: rawConfig(nullptr)
		{
		}

		DiaGameManifest::~DiaGameManifest()
		{
			delete rawConfig;
		}

		DiaGameManifest::DiaGameManifest(DiaGameManifest&& other) noexcept
			: name(other.name)
			, version(other.version)
			, imports(other.imports)
			, config(other.config)
			, rawConfig(other.rawConfig)
		{
			other.rawConfig = nullptr;
		}

		DiaGameManifest& DiaGameManifest::operator=(DiaGameManifest&& other) noexcept
		{
			if (this != &other)
			{
				delete rawConfig;
				name = other.name;
				version = other.version;
				imports = other.imports;
				config = other.config;
				rawConfig = other.rawConfig;
				other.rawConfig = nullptr;
			}
			return *this;
		}
	}
}
