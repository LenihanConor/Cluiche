//////////////////
#pragma once

#include "DiaUI/UIDataBuffer.h"

#include <DiaInput/EMouseButton.h>
#include <DiaCore/Strings/String256.h>
#include <DiaCore/Strings/String64.h>
#include <DiaCore/Strings/String32.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
namespace Dia
{
	namespace UI
	{
		class BoundMethod
		{
		public:
			class Args
			{
			public:

			};

			typedef void(*FunctionPtr)(const Args *);

			BoundMethod(const char* name, FunctionPtr functionPtr)
				: mName(name)
				, mFunctionPtr(functionPtr)
			{}

			const Dia::Core::Containers::String32& GetName()const { return mName; };

			Dia::Core::Containers::String32 mName;
			FunctionPtr mFunctionPtr;
		};
		

		static const int kMaxBoundMethod = 32;
		
		typedef Dia::Core::Containers::DynamicArrayC<BoundMethod, kMaxBoundMethod> BoundMethodList;

		class Page
		{
		public:
			

			Page() {};
			Page(const Dia::Core::Containers::String256& url)
				: mUrl(url)
			{}

			virtual ~Page() {};

		//	virtual void BindMethods(IUISystem* parentSystem) = 0;
			const Dia::Core::Containers::String256& GetUrl()const { return mUrl;  };
		/*	const BoundMethodList& GetBoundMenthods()const { return mBoundMethodList; }
			void BindMethod(BoundMethod& pMethod)
			{
				mBoundMethodList.Add(pMethod);
			}*/


		private:
			Dia::Core::Containers::String256 mUrl;
		//	BoundMethodList mBoundMethodList;
		};

		class IUISystem
		{
		public:
			IUISystem() {};
			virtual ~IUISystem() {};

			virtual void Initialize() = 0;

			virtual void LoadPage(const Page& newPage) = 0;
			virtual void OnLoadedPage() = 0;
			virtual void IsLoadingPage() = 0;
			virtual void UnloadPage() = 0;

			virtual void Update() = 0;

			virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const = 0;

			virtual void BindMethod(const Dia::Core::Containers::String64& methodName, BoundMethod* pMethod) = 0;

			//Input
			virtual void InjectMouseMove(int x, int y) = 0;
			virtual void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y) = 0;
			virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) = 0;
		};
	}
}