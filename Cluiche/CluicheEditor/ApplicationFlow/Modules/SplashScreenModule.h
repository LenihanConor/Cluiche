#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <DiaApplication/ApplicationModule.h>

namespace Cluiche
{
	namespace Editor
	{
		class SplashScreenModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit SplashScreenModule(Dia::Application::ProcessingUnit* pu);

			void Dismiss();

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoStop() override;

		private:
			HWND mHwnd;

			static const int kWidth  = 560;
			static const int kHeight = 323;
		};
	}
}
