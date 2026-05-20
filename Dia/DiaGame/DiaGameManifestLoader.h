#pragma once

#include "DiaGameManifest.h"

namespace Dia
{
	namespace Game
	{
		class DiaGameManifestLoader
		{
		public:
			static bool LoadGameFile(const char* path, DiaGameManifest& outManifest);
			static bool LoadStageFile(const char* path, DiaStageManifest& outManifest);
		};
	}
}
