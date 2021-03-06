////////////////////////////////////////////////////////////////////////////////
// Filename: DiaAwesomiumUISystem
////////////////////////////////////////////////////////////////////////////////
#include "AwesomiumUISystem.h"

#include "DiaUIAwesomium/External/application.h"
#include "DiaUIAwesomium/External/view.h"
#include "DiaUIAwesomium/External/method_dispatcher.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Strings/String256.h>

#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>

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
			class UISystemImpl: public Application::Listener,
										::Awesomium::WebViewListener::Load,
										::Awesomium::WebViewListener::Process

			{
			public:
				UISystemImpl(const Window::IWindow* windowContext)
					: mPlatformAbstractedUIApplicaton(Application::Create())
					, mView(nullptr)
					, mSystemHandle(windowContext->GetSystemHandle())
					, mIsInitialized(false)
					, mWindowContext(windowContext)
				{
					mPlatformAbstractedUIApplicaton->set_listener(this);
				}

				virtual ~UISystemImpl()
				{
					DIA_ASSERT(mPlatformAbstractedUIApplicaton, "mPlatformAbstractedUIApplicaton is NULL");
					if (mPlatformAbstractedUIApplicaton)
					{
						delete mPlatformAbstractedUIApplicaton;
					}
				}

				void Initialize()
				{
					if (!mIsInitialized)	// only initialize once
					{
						mPlatformAbstractedUIApplicaton->Load();
					}
				}

				void LoadPage(Page& newPage)
				{						 
					const Dia::Core::Containers::String256& newPageUrl = newPage.GetUrl();
					::Awesomium::WebURL url(::Awesomium::WSLit(newPageUrl.AsCStr()));

					mView->set_js_method_handler(&mMethodDispatcher);

					Dia::UI::BoundMethodList boundMethodList = newPage.GetBoundMenthods();

					::Awesomium::JSValue result = mView->CreateGlobalJavascriptObject(::Awesomium::WSLit("app"));
					if (result.IsObject())
					{
						::Awesomium::JSObject& app_object = result.ToObject();

						for (unsigned int i = 0; i < boundMethodList.Size(); i++)
						{
							const Dia::Core::Containers::String32& name = boundMethodList[0].GetName();

							Dia::UI::BoundMethod::MethodPtr func = boundMethodList[0].GetMethodPtr();

							mMethodDispatcher.Bind(app_object,
								::Awesomium::WSLit(name.AsCStr()),
								JSDelegate(func));
						}
					}

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
					mView = mPlatformAbstractedUIApplicaton->web_core()->CreateWebView(mWindowContext->GetSize().X(), mWindowContext->GetSize().Y());
					mView->SetTransparent(true);
					mView->set_load_listener(this);
					mView->set_process_listener(this);

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
					if (IsInitialized())
					{
						::Awesomium::BitmapSurface* bitmap = static_cast<::Awesomium::BitmapSurface*>(mView->surface());

						int buffersize = bitmap->width() * bitmap->height() * 4;
						DIA_ASSERT(buffersize > sBuffersize); // Need to increase size of UI buffer

						bitmap->CopyTo(&mBuffer[0], bitmap->width() * 4, 4, false, false);

						// We do not delete the memory, the buffer will take care of it.
						outBuffer.CreateFromPreallocatedBuffer(bitmap->width(), bitmap->height(), &mBuffer[0], sBuffersize, false);
					}
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

				//-------------------------------------------------------------------
				// This event occurs when the page begins loading a frame.
				virtual void OnBeginLoadingFrame(::Awesomium::WebView* caller,
					int64 frame_id,
					bool is_main_frame,
					const ::Awesomium::WebURL& url,
					bool is_error_page)override
				{}

				//-------------------------------------------------------------------
				// This event occurs when a frame fails to load. See error_desc for additional information.
				virtual void OnFailLoadingFrame(::Awesomium::WebView* caller,
					int64 frame_id,
					bool is_main_frame,
					const ::Awesomium::WebURL& url,
					int error_code,
					const ::Awesomium::WebString& error_desc)override
				{
					Dia::Core::Containers::String256 str;
					url.filename().ToUTF8(str.AsCStr(), str.Size());
					DIA_ASSERT(0, "Failed to load %s", str.AsCStr());
				}

				//-------------------------------------------------------------------
				// This event occurs when the page finishes loading a frame.
				// The main frame always finishes loading last for a given page load.
				virtual void OnFinishLoadingFrame(::Awesomium::WebView* caller,
					int64 frame_id,
					bool is_main_frame,
					const ::Awesomium::WebURL& url)override
				{}

				//-------------------------------------------------------------------
				// This event occurs when the DOM has finished parsing and the
				// window object is available for JavaScript execution.
				virtual void OnDocumentReady(::Awesomium::WebView* caller,
					const ::Awesomium::WebURL& url)override
				{}

				//-------------------------------------------------------------------
				// This even occurs when a new WebView render process is launched.
				virtual void OnLaunch(::Awesomium::WebView* caller)override{}

				/// This event occurs when the render process hangs.
				virtual void OnUnresponsive(::Awesomium::WebView* caller)override
				{
					DIA_ASSERT(0, "Awesomium rendering is unresponsive");
				}

				/// This event occurs when the render process becomes responsive after a hang.
				virtual void OnResponsive(::Awesomium::WebView* caller)override{}

				/// This event occurs when the render process crashes.
				virtual void OnCrashed(::Awesomium::WebView* caller, ::Awesomium::TerminationStatus status)override
				{
					DIA_ASSERT(0, "Awesomium rendering has crashed");
				}

			private:
				Application* mPlatformAbstractedUIApplicaton;
				::Awesomium::WebView* mView;
				MethodDispatcher mMethodDispatcher;
				const Window::IWindow* mWindowContext;
				HWND mSystemHandle;
				bool mIsInitialized;

				static const int sBuffersize = 7000000;
				mutable unsigned char mBuffer[sBuffersize];
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
			void UISystem::LoadPage(Page& newPage)
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