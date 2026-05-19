#pragma once

#include <string.h>

namespace Dia
{
	namespace Editor
	{
		enum class ProjectSource
		{
			kManual,
			kLive
		};

		struct ProjectContext
		{
			static constexpr int kMaxPath = 512;

			char diagamePath[kMaxPath];
			char applicationManifestPath[kMaxPath];
			char assetCataloguePath[kMaxPath];
			char assetRoot[kMaxPath];
			ProjectSource source;

			ProjectContext()
				: source(ProjectSource::kManual)
			{
				diagamePath[0]              = '\0';
				applicationManifestPath[0]  = '\0';
				assetCataloguePath[0]       = '\0';
				assetRoot[0]                = '\0';
			}

			bool IsValid() const { return diagamePath[0] != '\0'; }
		};

		using ProjectChangedCallback = void(*)(const ProjectContext&, void* userData);
	}
}
