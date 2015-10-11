////////////////////////////////////////////////////////////////////////////////
// Filename: DiaAwesomiumUISystem
////////////////////////////////////////////////////////////////////////////////
#include "AwesomiumUISystem.h"

#include "DiaUIAwesomium/External/application.h"
#include "DiaUIAwesomium/External/view.h"
#include "DiaUIAwesomium/External/method_dispatcher.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Memory/Memory.h>

#include <DiaUI/UIDataBuffer.h>

#include  <DiaWindow/SystemHandle.h>
#include  <DiaWindow/Interface/IWindow.h>

#include <Awesomium/WebCore.h>
#include <Awesomium/BitmapSurface.h>
#include <Awesomium/STLHelpers.h>

#include <thread>

namespace Dia
{
	namespace UI
	{
		namespace Awesomium
		{
			class UISystemImpl: public Application::Listener
			{
			public:
				UISystemImpl(const Window::IWindow* windowContext)
					: mPlatformAbstractedUIApplicaton(Application::Create())
					, view_(nullptr)
					, mSystemHandle(windowContext->GetSystemHandle())
					, mIsInitialized(false)
				{
					mPlatformAbstractedUIApplicaton->set_listener(this);
				}

				virtual ~UISystemImpl()
				{
					DIA_ASSERT(mPlatformAbstractedUIApplicaton, "mPlatformAbstractedUIApplicaton is NULL");
					if (mPlatformAbstractedUIApplicaton)
						delete mPlatformAbstractedUIApplicaton;
				}

				void Initialize()
				{
					if (!mIsInitialized)	// only initialize once
						mPlatformAbstractedUIApplicaton->Load();
				}

				bool IsInitialized()const
				{
					return mIsInitialized;
				}

				void Update()
				{
					mPlatformAbstractedUIApplicaton->Run();
				}

				// Inherited from Application::Listener
				virtual void OnLoaded()
				{
					view_ = mPlatformAbstractedUIApplicaton->web_core()->CreateWebView(800, 600);
					view_->SetTransparent(true);

				//	BindMethods(view_);

					::Awesomium::WebURL url(::Awesomium::WSLit("file:///Z:/GitHub/Cluiche/Cluiche/WebixTest/app.html"));
					view_->LoadURL(url);

					while (view_->IsLoading())
						mPlatformAbstractedUIApplicaton->web_core()->Update();

					Sleep(300);
					mPlatformAbstractedUIApplicaton->web_core()->Update();

					mIsInitialized = true;
				}

				// Inherited from Application::Listener
				virtual void OnUpdate()
				{
				}

				// Inherited from Application::Listener
				virtual void OnShutdown() {
				}

				void FetchUIDataBuffer(Dia::UI::UIDataBuffer& outBuffer)const
				{
					::Awesomium::BitmapSurface* bitmap = static_cast<::Awesomium::BitmapSurface*>(view_->surface());


					// REMOVE
					bitmap->SaveToJPEG(::Awesomium::WSLit("./AwesomiumTest.jpg"));

					//TODO: This is crap that i need to assign to a local buffer to just memcopy below
					int buffersize = bitmap->width() * bitmap->height() * 4;
					unsigned char *buffer = new unsigned char[buffersize];
					bitmap->CopyTo(buffer, bitmap->width() * 4, 4, false, false);

					outBuffer.Create(bitmap->width(), bitmap->height(), buffer, buffersize);
					
					delete[] buffer;
				}
			private:
				Application* mPlatformAbstractedUIApplicaton;
				::Awesomium::WebView* view_;
				MethodDispatcher method_dispatcher_;
				HWND mSystemHandle;
				bool mIsInitialized;
			};

			//-------------------------------------------------------------------
			UISystem::UISystem(const Window::IWindow* windowContext)
				: IUISystem()
				, mUISystemImpl(nullptr)
			{
				mUISystemImpl = DIA_NEW(UISystemImpl(windowContext));
			}

			//-------------------------------------------------------------------
			UISystem::~UISystem()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				DIA_DELETE(mUISystemImpl);
			}

			//-------------------------------------------------------------------
			void UISystem::Initialize()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				mUISystemImpl->Initialize();
				
				while (!mUISystemImpl->IsInitialized())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));	// Wait for ms before trying again
				}
			}

			//-------------------------------------------------------------------
			void UISystem::Update()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				mUISystemImpl->Update();
			}

			//-------------------------------------------------------------------
			void UISystem::FetchUIDataBuffer(UIDataBuffer& outBuffer)const
			{

				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				mUISystemImpl->FetchUIDataBuffer(outBuffer);
			}
		}
	}
}