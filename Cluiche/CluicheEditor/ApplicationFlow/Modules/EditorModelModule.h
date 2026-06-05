#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/Project/ProjectContext.h>

namespace Cluiche
{
	namespace Editor
	{
		struct WindowState
		{
			int x = 0;
			int y = 0;
			int width = 1280;
			int height = 720;
			bool maximized = false;
			bool hasStoredState = false;
		};

		class EditorModelModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorModelModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::EditorModel& GetModel() { return mModel; }
			const Dia::Editor::EditorModel& GetModel() const { return mModel; }

			void SetProjectPath(const char* path);
			const char* GetProjectPath() const;

			unsigned int GetRecentProjectCount() const;
			const char* GetRecentProject(unsigned int index) const;

			const WindowState& GetWindowState() const { return mWindowState; }
			void SaveWindowState(int x, int y, int width, int height, bool maximized);

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			void ReadEditorState(const char* cluicheprojPath);
			void WriteEditorState();
			void OnDiagameProjectChanged(const Dia::Editor::ProjectContext& ctx);

			Dia::Editor::EditorModel       mModel;

			static const unsigned int kMaxProjectPathLength = 512;
			static const unsigned int kMaxRecent = 5;

			char mProjectPath[kMaxProjectPathLength];    // .cluicheproj path (bare arg)
			char mDiagamePath[kMaxProjectPathLength];    // --project=<path> arg
			char mCluicheproj[kMaxProjectPathLength];    // path of loaded .cluicheproj for writeback
			char mRestoredLastProject[kMaxProjectPathLength];

			char mRecentProjects[kMaxRecent][kMaxProjectPathLength];
			unsigned int mRecentCount;

			WindowState mWindowState;
		};
	}
}
