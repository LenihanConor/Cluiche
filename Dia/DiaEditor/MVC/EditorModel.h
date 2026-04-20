#pragma once

#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		namespace ObserverMessage
		{
			enum Type
			{
				kProjectChanged = 0,
				kDirtyStateChanged,
				kCloseRequested,
				kCount
			};
		}

		class EditorModel : public Dia::Core::ObserverSubject
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			EditorModel();

			void LoadProject(const char* path);
			void MarkDirty();
			void ClearDirty();
			void RequestClose();
			void Reset();

			bool HasOpenProject() const;
			bool IsDirty() const;
			bool IsCloseRequested() const;
			const char* GetProjectPath() const;

			unsigned int GetManifestCount() const;
			const char* GetManifestPath(unsigned int index) const;

		private:
			static const unsigned int kMaxPathLength = 256;
			static const unsigned int kMaxManifests = 16;

			char mProjectPath[kMaxPathLength];
			bool mHasOpenProject;
			bool mIsDirty;
			bool mCloseRequested;

			char mManifestPaths[kMaxManifests][kMaxPathLength];
			unsigned int mManifestCount;
		};
	}
}
