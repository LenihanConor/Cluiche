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
					, mView(nullptr)
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

				void LoadPage(const Page& newPage)
				{
					const Dia::Core::Containers::String256& newPageUrl = newPage.GetUrl();
					::Awesomium::WebURL url(::Awesomium::WSLit(newPageUrl.AsCStr()));




					TODO - REMOVE THIS
//					newPage.BindMethods();
					newPage.GetBoundMenthods();

					JSValue result = web_view->CreateGlobalJavascriptObject(WSLit("app"));
					if (result.IsObject()) {
						// Bind our custom method to it.
						JSObject& app_object = result.ToObject();
						method_dispatcher_.Bind(app_object,
							WSLit("backgroundGrey"),
							JSDelegate(this, &TutorialApp::BackgroundGrey));

						method_dispatcher_.Bind(app_object,
							WSLit("backgroundWhite"),
							JSDelegate(this, &TutorialApp::BackgroundWhite));

						method_dispatcher_.Bind(app_object,
							WSLit("backgroundBluish"),
							JSDelegate(this, &TutorialApp::BackgroundBluish));
					}

					// Bind our method dispatcher to the WebView
					web_view->set_js_method_handler(&method_dispatcher_);
					TODO - REMOVE THIS








					mView->LoadURL(url);
			
					while (mView->IsLoading())
						mPlatformAbstractedUIApplicaton->web_core()->Update();
				}

				bool IsInitialized()const
				{
					return mIsInitialized;
				}

				void Update()
				{
					mPlatformAbstractedUIApplicaton->UpdateCore();
				}

				// Inherited from Application::Listener
				virtual void OnLoaded()
				{
					mView = mPlatformAbstractedUIApplicaton->web_core()->CreateWebView(800, 600);
					mView->SetTransparent(true);
				
					mIsInitialized = true;
				}

				// Inherited from Application::Listener
				virtual void OnUpdate()
				{}

				// Inherited from Application::Listener
				virtual void OnShutdown()
				{}

				void FetchUIDataBuffer(Dia::UI::UIDataBuffer& outBuffer)const
				{
					::Awesomium::BitmapSurface* bitmap = static_cast<::Awesomium::BitmapSurface*>(mView->surface());

					//TODO: This is crap that i need to assign to a local buffer to just memcopy below
					int buffersize = bitmap->width() * bitmap->height() * 4;
					unsigned char *buffer = new unsigned char[buffersize];
					bitmap->CopyTo(buffer, bitmap->width() * 4, 4, false, false);


					// We do not delete the memory, the buffer will take care of it.
					outBuffer.CreateFromPreallocatedBuffer(bitmap->width(), bitmap->height(), buffer, buffersize);
				}

				//-------------------------------------------------------------------
				void InjectMouseMove(int x, int y)
				{
					if (mView)
						mView->InjectMouseMove(x, y);
				}

				//-------------------------------------------------------------------
				void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)
				{			
					if (mView)
					{
						::Awesomium::MouseButton button = ::Awesomium::kMouseButton_Left;
						switch (button)
						{
						case Dia::Input::EMouseButton::kLeft: button = ::Awesomium::kMouseButton_Left; break;
						case Dia::Input::EMouseButton::kMiddle: button = ::Awesomium::kMouseButton_Middle; break;
						case Dia::Input::EMouseButton::kRight: button = ::Awesomium::kMouseButton_Right; break;
						default: 
							break;
						}

						mView->InjectMouseMove(x, y);
						mView->InjectMouseDown(button);
					}
				}

				//-------------------------------------------------------------------
				void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)
				{
					if (mView)
					{
						::Awesomium::MouseButton button = ::Awesomium::kMouseButton_Left;
						switch (button)
						{
						case Dia::Input::EMouseButton::kLeft: button = ::Awesomium::kMouseButton_Left; break;
						case Dia::Input::EMouseButton::kMiddle: button = ::Awesomium::kMouseButton_Middle; break;
						case Dia::Input::EMouseButton::kRight: button = ::Awesomium::kMouseButton_Right; break;
						default:
							break;
						}

						mView->InjectMouseMove(x, y);
						mView->InjectMouseUp(button);
					}
				}

				//-------------------------------------------------------------------
				void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)
				{
					if (mView)
					{
						::Awesomium::MouseButton button = ::Awesomium::kMouseButton_Left;
						switch (button)
						{
						case Dia::Input::EMouseButton::kLeft: button = ::Awesomium::kMouseButton_Left; break;
						case Dia::Input::EMouseButton::kMiddle: button = ::Awesomium::kMouseButton_Middle; break;
						case Dia::Input::EMouseButton::kRight: button = ::Awesomium::kMouseButton_Right; break;
						default:
							break;
						}

						mView->InjectMouseMove(x, y);
						mView->InjectMouseDown(button);
						mView->InjectMouseUp(button);
					}
				}

				//-------------------------------------------------------------------
				void InjectMouseWheel(int scroll_vert, int scroll_horz)
				{
					if (mView)
						mView->InjectMouseWheel(scroll_vert, scroll_horz);
				}

			private:
				Application* mPlatformAbstractedUIApplicaton;
				::Awesomium::WebView* mView;
				MethodDispatcher mMethodDispatcher;
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

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->Initialize();
				
				while (!mUISystemImpl->IsInitialized())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));	// Wait for ms before trying again
				}
			}

			//-------------------------------------------------------------------
			void UISystem::LoadPage(const Page& newPage)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->LoadPage(newPage);
			}

			//-------------------------------------------------------------------
			void UISystem::Update()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->Update();
			}

			//-------------------------------------------------------------------
			void UISystem::FetchUIDataBuffer(UIDataBuffer& outBuffer)const
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->FetchUIDataBuffer(outBuffer);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectMouseMove(int x, int y)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->InjectMouseMove(x,y);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->InjectMouseDown(button, x, y);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->InjectMouseUp(button, x, y);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->InjectMouseClick(button, x, y);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectMouseWheel(int scroll_vert, int scroll_horz)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->InjectMouseWheel(scroll_vert, scroll_horz);
			}
		}
	}
}