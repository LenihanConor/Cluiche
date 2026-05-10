#pragma once

#include <DiaApplicationFlow/Module.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaLogger/DebugOutputSink.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorModelModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorModelModule(const Dia::Core::StringCRC& instanceId);

			Dia::Editor::EditorModel& GetModel() { return mModel; }
			const Dia::Editor::EditorModel& GetModel() const { return mModel; }

			void SetProjectPath(const char* path);
			const char* GetProjectPath() const;

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			Dia::Editor::EditorModel       mModel;
			Dia::Logger::DebugOutputSink   mDebugOutputSink;

			static const unsigned int kMaxProjectPathLength = 260;
			char mProjectPath[kMaxProjectPathLength];
		};
	}
}
