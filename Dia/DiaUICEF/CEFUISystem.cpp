////////////////////////////////////////////////////////////////////////////////
// Filename: CEFUISystem.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFUISystem.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaUI/IPage.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>
#include <DiaWindow/Interface/IWindow.h>

#include "CEFPage.h"
#include "CEFProcessHandler.h"

#include <include/cef_app.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
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
				, mPrimaryPage(nullptr)
				, mNextPageId(0)
			{
			}

			~CEFUISystemImpl()
			{
				if (mIsInitialized)
					Shutdown();
			}

			void Initialize()
			{
				if (mIsInitialized)
					return;

				CefMainArgs main_args(GetModuleHandle(nullptr));

				CefSettings settings;
				settings.no_sandbox = true;
				settings.windowless_rendering_enabled = true;
				CefString(&settings.cache_path).FromString(mCachePath);
				CefString(&settings.browser_subprocess_path).FromASCII("DiaUICEF_subprocess.exe");

#ifdef _DEBUG
				settings.remote_debugging_port = mRemoteDebuggingPort;
				Dia::Core::Log::OutputVaradicLine("DiaUICEF: Remote debugging at http://localhost:%d", mRemoteDebuggingPort);
#else
				settings.remote_debugging_port = 0;
#endif

				mApp = new CEFProcessHandler(mAssetBasePath);

				bool result = CefInitialize(main_args, settings, mApp.get(), nullptr);
				DIA_ASSERT(result, "CefInitialize failed");

				mIsInitialized = result;
				if (mIsInitialized)
					Dia::Core::Log::OutputVaradicLine("DiaUICEF: Initialized successfully");
			}

			void Shutdown()
			{
				if (!mIsInitialized)
					return;

				for (unsigned int i = 0; i < mPages.Size(); ++i)
				{
					if (mPages[i])
					{
						mPages[i]->Close();
						delete mPages[i];
					}
				}
				mPages.RemoveAll();
				mPrimaryPage = nullptr;

				CefShutdown();
				mIsInitialized = false;
				Dia::Core::Log::OutputVaradicLine("DiaUICEF: Shutdown complete");
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
				if (page->Create())
				{
					mPages.Add(page);
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

		private:
			const Window::IWindow* mWindowContext;
			bool mIsInitialized;
			int mRemoteDebuggingPort;
			std::string mCachePath;
			std::string mAssetBasePath;

			CefRefPtr<CEFProcessHandler> mApp;
			Dia::Core::Containers::DynamicArrayC<CEFPage*, 16> mPages;
			CEFPage* mPrimaryPage;
			int mNextPageId;
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
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->Initialize();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::Shutdown()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->Shutdown();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::LoadPage(UI::Page& newPage)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->LoadPage(newPage);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::UnloadPage()
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
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
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->Update();
		}

		//-------------------------------------------------------------------
		void CEFUISystem::FetchUIDataBuffer(UI::UIDataBuffer& outBuffer) const
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->FetchUIDataBuffer(outBuffer);
		}

		//-------------------------------------------------------------------
		UI::IPage* CEFUISystem::CreatePage(const char* url, int width, int height)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			return mImpl->CreatePage(url, width, height);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::DestroyPage(UI::IPage* page)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
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
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->InjectMouseMove(x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->InjectMouseDown(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->InjectMouseUp(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
			mImpl->InjectMouseClick(button, x, y);
		}

		//-------------------------------------------------------------------
		void CEFUISystem::InjectMouseWheel(int scroll_vert, int scroll_horz)
		{
			DIA_ASSERT(mImpl, "mImpl is NULL");
			std::lock_guard<std::mutex> lock(mSystemMutex);
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
	}
}
