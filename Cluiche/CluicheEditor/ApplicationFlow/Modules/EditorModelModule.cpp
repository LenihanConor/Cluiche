#include "EditorModelModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaLogger/Logger.h>

#include <string.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorModelModule::kTypeId("EditorModelModule");

		EditorModelModule::EditorModelModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
		{
			mProjectPath[0] = '\0';
		}

		void EditorModelModule::SetProjectPath(const char* path)
		{
			if (path == nullptr)
			{
				mProjectPath[0] = '\0';
				return;
			}
			strncpy_s(mProjectPath, kMaxProjectPathLength, path, _TRUNCATE);
		}

		const char* EditorModelModule::GetProjectPath() const
		{
			return mProjectPath;
		}

		Dia::ApplicationFlow::StartResult EditorModelModule::DoStart()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.RegisterThreadBuffer();
			logger.RegisterSink(&mDebugOutputSink);
			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorModelModule::DoUpdate(float /*deltaTime*/)
		{
			Dia::Logger::Logger::Instance().FlushBuffers();
		}

		Dia::ApplicationFlow::StopResult EditorModelModule::DoStop()
		{
			Dia::Logger::Logger& logger = Dia::Logger::Logger::Instance();
			logger.UnregisterSink(&mDebugOutputSink);
			logger.UnregisterThreadBuffer();
			mModel.Reset();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using EditorModelModule_ = Cluiche::Editor::EditorModelModule; }
DIA_MODULE(EditorModelModule_);
