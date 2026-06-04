#pragma once

#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaEditor/MVC/IEditorContext.h>

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

		class EditorModel : public Dia::Core::ObserverSubject, public IEditorContext
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			EditorModel();

			// .cluicheproj loading
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

			// IEditorContext — .diagame game-project API
			bool LoadDiagameProject(const char* diagamePath) override;
			void ClearDiagameProject() override;
			const ProjectContext& GetDiagameProject() const override;
			void OnDiagameProjectChanged(ProjectChangedCallback callback, void* userData) override;
			unsigned int GetRecentProjectCount() const override;
			const char* GetRecentProject(unsigned int index) const override;

			// Called by EditorModelModule to keep the recent list in sync after
			// each LoadDiagameProject / ClearDiagameProject.
			void SetRecentProjects(const char* const* paths, unsigned int count);

		private:
			static const unsigned int kMaxPathLength = 256;
			static const unsigned int kMaxManifests = 16;

			char mProjectPath[kMaxPathLength];
			bool mHasOpenProject;
			bool mIsDirty;
			bool mCloseRequested;

			char mManifestPaths[kMaxManifests][kMaxPathLength];
			unsigned int mManifestCount;

			// Game-project context
			struct CallbackEntry
			{
				ProjectChangedCallback callback;
				void* userData;
			};

			static const unsigned int kMaxCallbacks = 16;
			static const unsigned int kMaxRecent    = 5;

			ProjectContext  mDiagameContext;
			CallbackEntry   mCallbacks[kMaxCallbacks];
			unsigned int    mCallbackCount;

			char         mRecentPaths[kMaxRecent][kMaxPathLength];
			unsigned int mRecentCount;

			void FireProjectCallbacks();
		};
	}
}
