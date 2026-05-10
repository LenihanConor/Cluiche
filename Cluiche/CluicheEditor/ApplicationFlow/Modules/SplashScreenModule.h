#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <DiaApplicationFlow/Module.h>

namespace Cluiche
{
	namespace Editor
	{
		class SplashScreenModule : public Dia::ApplicationFlow::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit SplashScreenModule(const Dia::Core::StringCRC& instanceId);

			void Dismiss();

		protected:
			Dia::ApplicationFlow::StartResult DoStart() override;
			void DoUpdate(float deltaTime) override;
			Dia::ApplicationFlow::StopResult DoStop() override;

		private:
			HWND mHwnd;

			static const int kWidth  = 700;
			static const int kHeight = 404;
		};
	}
}
