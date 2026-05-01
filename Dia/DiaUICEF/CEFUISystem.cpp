////////////////////////////////////////////////////////////////////////////////
// Filename: CEFUISystem.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFUISystem.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/FilePath/Path.h>
#include <DiaUI/IPage.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>
#include <DiaWindow/Interface/IWindow.h>
 
#include "CEFPage.h"
#include "CEFClientHandler.h"
#include "CEFProcessHandler.h"
#include "CEFJavaScriptBridge.h"

#include <include/cef_app.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <memory>
#include <string>
#include <thread>

namespace Dia
{
	namespace UICEF
	{
		class CEFUISystemImpl
		{
		public:
			CEFUISystemImpl(const Window::IWindow* windowContext)
				: mWindowContext(windowContext)
				, mIsInitialized(false)
				, mRemoteDebuggingPort(9222)
				, mCachePath(".cef_cache")
				, mAssetBasePath("./UI/")
				, mSubprocessPath("DiaUICEF_TestSubprocess.exe")
				, mWindowedRendering(false)
				, mPrimaryPage(nullptr)
				, mNextPageId(0)
				, mJSBridge(new CEFJavaScriptBridge())
			{
			}

			CEFJavaScriptBridge* GetJSBridge() { return mJSBridge.get(); }
			CEFPage* GetPrimaryPage() { return mPrimaryPage; }

			void RegisterJSHandler(const char* name, UI::IUISystem::JSHandler handler)
			{
				if (!name || !handler)
					return;
				mJSBridge->RegisterFunction(name, handler);
			}

			void CallJSFunction(const char* functionName, const char* argsJson)
			{
				if (!mPrimaryPage)
					return;
				CefRefPtr<CefBrowser> browser = mPrimaryPage->GetBrowser();
				if (browser)
					mJSBridge->CallJavaScript(browser, functionName, argsJson);
			}

			~CEFUISystemImpl()
			{
				if (mIsInitialized)
					Shutdown();
			}

			static std::string ResolveToAbsolute(const std::string& path)
			{
				if (path.size() >= 2 && (path[1] == ':' || path[0] == '\\' || path[0] == '/'))
					return path;
				std::string exeDir;
				Dia::Core::Path::ExePath(exeDir);
				if (path.empty())
					return exeDir + "\\";
				return exeDir + "\\" + path;
			}

			void Initialize()
			{
				if (mIsInitialized)
					return;

				CefMainArgs main_args(GetModuleHandle(nullptr));

				CefSettings settings;
				settings.no_sandbox = true;
				settings.windowless_rendering_enabled = mWindowedRendering ? false : true;
				std::string resolvedSubprocess = ResolveToAbsolute(mSubprocessPath);
				std::string resolvedCache = ResolveToAbsolute(mCachePath);
				DIA_LOG_INFO("UI", "DiaUICEF: subprocess_path = %s", resolvedSubprocess.c_str());
				DIA_LOG_INFO("UI", "DiaUICEF: cache_path = %s", resolvedCache.c_str());
				CefString(&settings.cache_path).FromASCII(resolvedCache.c_str());
				CefString(&settings.browser_subprocess_path).FromASCII(resolvedSubprocess.c_str());
				CefString(&settings.resources_dir_path).FromASCII(ResolveToAbsolute("cef").c_str());
				CefString(&settings.locales_dir_path).FromASCII(ResolveToAbsolute("cef/locales").c_str());

#ifdef _DEBUG
				settings.remote_debugging_port = mRemoteDebuggingPort;
				DIA_LOG_INFO("UI", "DiaUICEF: Remote debugging at http://localhost:%d", mRemoteDebuggingPort);
#else
				settings.remote_debugging_port = 0;
#endif

				std::string resolvedAssetBase = ResolveToAbsolute(mAssetBasePath);
				mApp = new CEFProcessHandler(resolvedAssetBase);

				bool result = CefInitialize(main_args, settings, mApp.get(), nullptr);
				DIA_ASSERT(result, "CefInitialize failed");

				mIsInitialized = result;
				if (mIsInitialized)
					DIA_LOG_INFO("UI", "DiaUICEF: Initialized successfully");
			}

			void Shutdown()
			{
				if (!mIsInitialized)
					return;

				// Request browser close and let CEF deliver OnBeforeClose before we destroy the page.
				for (unsigned int i = 0; i < mPages.Size(); ++i)
				{
					if (mPages[i])
						mPages[i]->Close();
				}

				// Pump the CEF message loop until all browsers have been destroyed.
				// OnBeforeClose will clear each page's mBrowser via SetBrowser(nullptr).
				for (int i = 0; i < 50; ++i)
					CefDoMessageLoopWork();

				// Detach pages from client handlers so any late callbacks are safe,
				// then delete.
				for (unsigned int i = 0; i < mPages.Size(); ++i)
				{
					if (mPages[i])
					{
						if (CefRefPtr<CEFClientHandler> ch = mPages[i]->GetClientHandler())
							ch->DetachPage();
						delete mPages[i];
					}
				}
				mPages.RemoveAll();
				mPrimaryPage = nullptr;

				CefShutdown();
				mIsInitialized = false;
				DIA_LOG_INFO("UI", "DiaUICEF: Shutdown complete");
			}

			void Update()
			{
				if (!mIsInitialized)
					return;

				CefDoMessageLoopWork();
			}

			void LoadPage(UI::Page& newPage)
			{
				Dia::Core::FilePath::ResoledFilePath resolvedPath;
				newPage.GetUrl().Resolve(resolvedPath);

				int width = static_cast<int>(mWindowContext->GetSize().X());
				int height = static_cast<int>(mWindowContext->GetSize().Y());

				CEFPage* page = new CEFPage(mNextPageId++, resolvedPath.AsCStr(), width, height);
				if (page->Create())
				{
					mPages.Add(page);
					mPrimaryPage = page;

					while (page->IsLoading())
					{
						CefDoMessageLoopWork();
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
				}
				else
				{
					delete page;
					DIA_ASSERT(0, "DiaUICEF: Failed to create page");
				}
			}

			void UnloadPage()
			{
				if (mPrimaryPage)
				{
					for (unsigned int i = 0; i < mPages.Size(); ++i)
					{
						if (mPages[i] == mPrimaryPage)
						{
							mPages[i]->Close();
							delete mPages[i];
							mPages.RemoveAt(i);
							break;
						}
					}
					mPrimaryPage = nullptr;
				}
			}

			bool IsPageLoaded() const
			{
				return mPrimaryPage != nullptr;
			}

			void FetchUIDataBuffer(UI::UIDataBuffer& outBuffer) const
			{
				if (mPrimaryPage)
					mPrimaryPage->GetTextureData(outBuffer);
			}

			UI::IPage* CreatePage(const char* url, int width, int height)
			{
				CEFPage* page = new CEFPage(mNextPageId++, url, width, height);
				page->SetJSBridge(mJSBridge.get());

				bool ok = false;
				if (mWindowedRendering && mWindowContext)
				{
					void* hwnd = mWindowContext->GetSystemHandle();
					ok = page->CreateWindowed(hwnd);
				}
				else
				{
					ok = page->Create();
				}

				if (ok)
				{
					mPages.Add(page);
					if (mPrimaryPage == nullptr)
						mPrimaryPage = page;
					return page;
				}

				delete page;
				return nullptr;
			}

			void DestroyPage(UI::IPage* page)
			{
				if (!page)
					return;

				for (unsigned int i = 0; i < mPages.Size(); ++i)
				{
					if (mPages[i] == page)
					{
						if (mPrimaryPage == mPages[i])
							mPrimaryPage = nullptr;

						mPages[i]->Close();
						delete mPages[i];
						mPages.RemoveAt(i);
						return;
					}
				}
			}

			int GetPageCount() const
			{
				return static_cast<int>(mPages.Size());
			}

			void InjectMouseMove(int x, int y) { if (mPrimaryPage) mPrimaryPage->InjectMouseMove(x, y); }
			void InjectMouseDown(Dia::Input::EMouseButton b, int x, int y) { if (mPrimaryPage) mPrimaryPage->InjectMouseDown(static_cast<int>(b), x, y); }
			void InjectMouseUp(Dia::Input::EMouseButton b, int x, int y) { if (mPrimaryPage) mPrimaryPage->InjectMouseUp(static_cast<int>(b), x, y); }
			void InjectMouseClick(Dia::Input::EMouseButton b, int x, int y) { if (mPrimaryPage) mPrimaryPage->InjectMouseClick(static_cast<int>(b), x, y); }
			void InjectMouseWheel(int sv, int sh) { if (mPrimaryPage) mPrimaryPage->InjectMouseWheel(sv, sh); }

			void SetRemoteDebuggingPort(int port) { mRemoteDebuggingPort = port; }
			void SetCachePath(const char* path) { mCachePath = path; }
			void SetAssetBasePath(const char* path) { mAssetBasePath = path; }
			void SetSubprocessPath(const char* path) { mSubprocessPath = path; }
			void SetWindowedRendering(bool windowed) { mWindowedRendering = windowed; }

		private:
			const Window::IWindow* mWindowContext;
			bool mIsInitialized;
			int mRemoteDebuggingPort;
			std::string mCachePath;
			std::string mAssetBasePath;
			std::string mSubprocessPath;
			bool mWindowedRendering;

			CefRefPtr<CEFProcessHandler> mApp;
			Dia::Core::Containers::DynamicArrayC<CEFPage*, 16> mPages;
			CEFPage* mPrimaryPage;
			int mNextPageId;
			std::unique_ptr<CEFJavaScriptBridge> mJSBridge;
		};

		//-------------------------------------------------------------------
		CEFUISystem::CEFUISystem(const Window::IWindow* windowContext)
			: IUISystem()
			, mImpl(nullptr)
		{
			mImpl = DIA_NEW(CEFUISystemImpl(windowContext));
		}

		//-------------------------------------------------------------------
		CEFUISystem::~CEFUISystem()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			DIA_DELETE(mImpl);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::Initialize()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->Initialize();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::Shutdown()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->Shutdown();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::LoadPage(UI::Page& newPage)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->LoadPage(newPage);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::UnloadPage()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->UnloadPage();
		}

		//-------------------------------------------------------------------
		bool CEFUISystem::IsPageLoaded() const
		{
			return mImpl ? mImpl->IsPageLoaded() : false;
		}

		//-------------------------------------------------------------------
		void CEFUISystem::Update()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->Update();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::FetchUIDataBuffer(UI::UIDataBuffer& outBuffer) const
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->FetchUIDataBuffer(outBuffer);
		}

		//-------------------------------------------------------------------
		UI::IPage* CEFUISystem::CreatePage(const char* url, int width, int height)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			return mImpl->CreatePage(url, width, height);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::DestroyPage(UI::IPage* page)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->DestroyPage(page);
		}

		//-------------------------------------------------------------------
		int CEFUISystem::GetPageCount() const
		{
			return mImpl ? mImpl->GetPageCount() : 0;
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseMove(int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->InjectMouseMove(x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->InjectMouseDown(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->InjectMouseUp(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->InjectMouseClick(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseWheel(int scroll_vert, int scroll_horz)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->InjectMouseWheel(scroll_vert, scroll_horz);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::SetRemoteDebuggingPort(int port)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			mImpl->SetRemoteDebuggingPort(port);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::SetCachePath(const char* path)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			mImpl->SetCachePath(path);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::SetAssetBasePath(const char* path)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			mImpl->SetAssetBasePath(path);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::SetSubprocessPath(const char* path)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			mImpl->SetSubprocessPath(path);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::SetWindowedRendering(bool windowed)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			mImpl->SetWindowedRendering(windowed);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::RegisterJSHandler(const char* name, JSHandler handler)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->RegisterJSHandler(name, handler);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::CallJSFunction(const char* functionName, const char* argsJson)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::recursive_mutex> lock(mSystemMutex);
			mImpl->CallJSFunction(functionName, argsJson);
		}
	}
}
