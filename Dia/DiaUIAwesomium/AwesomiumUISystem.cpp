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



#define CREATE_AWESOMIUM_BOUND(_methodName, _ptrToMethod)\
	void _methodName(WebView* caller, const JSArray& args)\
	{\
		Dia::UI::BoundMethodArgs diaArgs;\
		Convert(args, diaArgs);\
		(*_ptrToMethod)(diaArgs);\
	}


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
					{
						mPlatformAbstractedUIApplicaton->Load();
					}
				}

				void LoadPage(const Page& newPage)
				{
					const Dia::Core::Containers::String256& newPageUrl = newPage.GetUrl();
					::Awesomium::WebURL url(::Awesomium::WSLit(newPageUrl.AsCStr()));




				//	Dia::UI::BoundMethodList boundMethodList = newPage.GetBoundMenthods();

	/*				::Awesomium::JSValue result = mView->CreateGlobalJavascriptObject(::Awesomium::WSLit("app"));
					if (result.IsObject()) 
					{
						// Bind our custom method to it.
						::Awesomium::JSObject& app_object = result.ToObject();

						
					//	const Dia::Core::Containers::String32* name = &boundMethodList[0].GetName();

						mMethodDispatcher.Bind(app_object,
							::Awesomium::WSLit("functionRouter"),
							JSDelegate(this, &UISystemImpl::BoundMethodRouter));
					}

					// Bind our method dispatcher to the WebView
					mView->set_js_method_handler(&mMethodDispatcher);*/
				

					mView->LoadURL(url);
			
					while (mView->IsLoading())
						mPlatformAbstractedUIApplicaton->web_core()->Update();
				}

				void BoundMethodRouter(::Awesomium::WebView* caller,
					const ::Awesomium::JSArray& args)
				{

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

		//		 = 1920000;
				

				void FetchUIDataBuffer(Dia::UI::UIDataBuffer& outBuffer)const
				{
					if (IsInitialized())
					{
					//	static unsigned char buffer[1920000];

						::Awesomium::BitmapSurface* bitmap = static_cast<::Awesomium::BitmapSurface*>(mView->surface());


						//TODO: This is crap that i need to assign to a local buffer to just memcopy below
						int buffersize = bitmap->width() * bitmap->height() * 4;

						DIA_ASSERT(buffersize > sBuffersize); // Need to increase size of UI buffer
					//	unsigned char *buffer = new unsigned char[buffersize];
					//	bitmap->CopyTo(buffer, bitmap->width() * 4, 4, false, false);


						bitmap->CopyTo(&mBuffer[0], bitmap->width() * 4, 4, false, false);

						// We do not delete the memory, the buffer will take care of it.
						outBuffer.CreateFromPreallocatedBuffer(bitmap->width(), bitmap->height(), &mBuffer[0], sBuffersize, false);
					}
				}

				void BindMethod(const Dia::Core::Containers::String64& methodName, BoundMethod* pMethod)
				{

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
			void UISystem::BindMethod(const Dia::Core::Containers::String64& methodName, BoundMethod* pMethod)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mUISystemImpl->BindMethod(methodName, pMethod);
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