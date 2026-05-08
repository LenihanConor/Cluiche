#pragma once

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			class SessionContext
			{
			public:
				SessionContext();

				void Load(const char* outputDir);
				void Save(const char* outputDir) const;

				const char* GetLastManifestPath() const;
				void SetLastManifestPath(const char* path);
				void ClearLastManifestPath();

				bool HasLastManifestPath() const;

			private:
				static const unsigned int kMaxPathLength = 512;
				char mLastManifestPath[kMaxPathLength];
			};
		}
	}
}
