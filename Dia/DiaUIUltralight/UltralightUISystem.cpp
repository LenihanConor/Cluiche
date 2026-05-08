////////////////////////////////////////////////////////////////////////////////
// Filename: UltralightUISystem.cpp
////////////////////////////////////////////////////////////////////////////////
#include "UltralightUISystem.h"

// DiaWindow must be included before Ultralight to avoid Win32 macro conflicts
// (Ultralight pulls in <windows.h> which redefines symbols like GetSystemHandle)
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaLogger/DiaLog.h>
#include <DiaUI/IPage.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>
#include <DiaWindow/Interface/IWindow.h>

// Undefine Win32 macros that clash with DiaWindow before pulling in Ultralight
#ifdef GetSystemHandle
#undef GetSystemHandle
#endif

#include <Ultralight/Ultralight.h>
#include <Ultralight/platform/Platform.h>
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/FileSystem.h>
#include <Ultralight/platform/FontLoader.h>
#include <Ultralight/platform/Surface.h>
#include <AppCore/Platform.h>
#include <AppCore/JSHelpers.h>

#include <JavaScriptCore/JavaScript.h>

#include <thread>
#include <string>
#include <vector>
#include <functional>

namespace Dia
{
	namespace UI
	{
		namespace Ultralight
		{
			// Minimal FileSystem implementation that maps file:// URLs to disk paths
			class DiaFileSystem : public ::ultralight::FileSystem
			{
			public:
				virtual bool FileExists(const ::ultralight::String& path) override
				{
					std::string p = path.utf8().data();
					FILE* f = nullptr;
					fopen_s(&f, p.c_str(), "rb");
					if (f) { fclose(f); return true; }
					return false;
				}

				virtual ::ultralight::String GetFileMimeType(const ::ultralight::String& path) override
				{
					std::string p = path.utf8().data();
					if (p.size() >= 5 && p.substr(p.size() - 5) == ".html") return "text/html";
					if (p.size() >= 4 && p.substr(p.size() - 4) == ".css")  return "text/css";
					if (p.size() >= 3 && p.substr(p.size() - 3) == ".js")   return "text/javascript";
					if (p.size() >= 4 && p.substr(p.size() - 4) == ".png")  return "image/png";
					if (p.size() >= 4 && p.substr(p.size() - 4) == ".jpg")  return "image/jpeg";
					return "application/octet-stream";
				}

				virtual ::ultralight::String GetFileCharset(const ::ultralight::String& /*path*/) override
				{
					return "utf-8";
				}

				virtual ::ultralight::RefPtr<::ultralight::Buffer> OpenFile(const ::ultralight::String& path) override
				{
					std::string p = path.utf8().data();
					FILE* f = nullptr;
					fopen_s(&f, p.c_str(), "rb");
					if (!f) return nullptr;

					fseek(f, 0, SEEK_END);
					long size = ftell(f);
					fseek(f, 0, SEEK_SET);

					void* data = malloc(size);
					fread(data, 1, size, f);
					fclose(f);

					return ::ultralight::Buffer::Create(data, size, nullptr,
						[](void* d, void*) { free(d); });
				}
			};

			// Minimal Logger that forwards to DiaCore logging
			class DiaLogger : public ::ultralight::Logger
			{
			public:
				virtual void LogMessage(::ultralight::LogLevel level, const ::ultralight::String& message) override
				{
					DIA_LOG_DEBUG("UI", "DiaUltralightUI: %s", message.utf8().data());
				}
			};

			// Holds a pending bound method callback to register on DOMReady
			struct PendingBinding
			{
				std::string name;
				BoundMethod method;
			};

			class UISystemImpl : public ::ultralight::LoadListener, public ::ultralight::ViewListener
			{
			public:
				UISystemImpl(const Window::IWindow* windowContext)
					: mWindowContext(windowContext)
					, mIsInitialized(false)
					, mWidth(0)
					, mHeight(0)
				{
				}

				~UISystemImpl()
				{
					mView = nullptr;
					mRenderer = nullptr;
				}

				void Initialize()
				{
					if (mIsInitialized)
						return;

					mWidth  = static_cast<uint32_t>(mWindowContext->GetSize().X());
					mHeight = static_cast<uint32_t>(mWindowContext->GetSize().Y());

					auto& platform = ::ultralight::Platform::instance();

					::ultralight::Config config;
					config.cache_path = ".ultralight_cache";
					platform.set_config(config);

					mFileSystem = std::make_unique<DiaFileSystem>();
					mLogger     = std::make_unique<DiaLogger>();

					platform.set_file_system(mFileSystem.get());
					platform.set_logger(mLogger.get());

					// Use the platform's built-in font loader (provided by Ultralight on Windows)
					::ultralight::FontLoader* fontLoader = ::ultralight::GetPlatformFontLoader();
					DIA_ASSERT(fontLoader, "Ultralight platform font loader unavailable");
					platform.set_font_loader(fontLoader);

					mRenderer = ::ultralight::Renderer::Create();
					DIA_ASSERT(mRenderer, "Failed to create Ultralight Renderer");

					::ultralight::ViewConfig viewConfig;
					viewConfig.is_accelerated  = false;
					viewConfig.is_transparent  = true;
					viewConfig.enable_javascript = true;

					mView = mRenderer->CreateView(mWidth, mHeight, viewConfig, nullptr);
					DIA_ASSERT(mView, "Failed to create Ultralight View");

					mView->set_load_listener(this);
					mView->set_view_listener(this);

					mIsInitialized = true;
				}

				bool IsInitialized() const { return mIsInitialized; }

				void LoadPage(Page& newPage)
				{
					mPendingBindings.clear();

					BoundMethodList& boundMethods = newPage.GetBoundMenthods();
					for (unsigned int i = 0; i < boundMethods.Size(); i++)
					{
						PendingBinding binding;
						binding.name   = boundMethods[i].GetName().AsCStr();
						binding.method = boundMethods[i];
						mPendingBindings.push_back(binding);
					}

					// Build file:// URL from the resolved path
					Dia::Core::FilePath::ResoledFilePath resolvedPath;
					newPage.GetUrl().Resolve(resolvedPath);

					std::string url = "file:///";
					url += resolvedPath.AsCStr();
					// Normalise backslashes to forward slashes for the URL
					for (char& c : url)
						if (c == '\\') c = '/';

					mView->LoadURL(::ultralight::String(url.c_str()));

					// Pump until the page finishes loading
					while (mView->is_loading())
					{
						mRenderer->Update();
						std::this_thread::sleep_for(std::chrono::milliseconds(1));
					}
					mRenderer->Render();
				}

				void Update()
				{
					if (mRenderer)
					{
						mRenderer->Update();
						mRenderer->Render();
					}
				}

				void FetchUIDataBuffer(Dia::UI::UIDataBuffer& outBuffer) const
				{
					if (!mIsInitialized || !mView)
						return;

					auto* surface = static_cast<::ultralight::BitmapSurface*>(mView->surface());
					if (!surface)
						return;

					auto bitmap = surface->bitmap();
					if (!bitmap)
						return;

					void* pixels = bitmap->LockPixels();
					if (!pixels)
						return;

					uint32_t w    = bitmap->width();
					uint32_t h    = bitmap->height();
					uint32_t size = static_cast<uint32_t>(bitmap->size());

					// Copy to our staging buffer (avoids holding the lock during render)
					if (size > static_cast<uint32_t>(sBufferSize))
					{
						bitmap->UnlockPixels();
						DIA_ASSERT(0, "Ultralight bitmap exceeds staging buffer size");
						return;
					}

					memcpy(mStagingBuffer, pixels, size);
					bitmap->UnlockPixels();
					surface->ClearDirtyBounds();

					outBuffer.CreateFromPreallocatedBuffer(
						static_cast<int>(w),
						static_cast<int>(h),
						mStagingBuffer,
						static_cast<int>(size),
						false);
				}

				void InjectMouseMove(int x, int y)
				{
					if (!mView) return;
					::ultralight::MouseEvent evt;
					evt.type    = ::ultralight::MouseEvent::kType_MouseMoved;
					evt.x       = x;
					evt.y       = y;
					evt.button  = ::ultralight::MouseEvent::kButton_None;
					mView->FireMouseEvent(evt);
				}

				void InjectMouseDown(Dia::Input::EMouseButton button, int x, int y)
				{
					if (!mView) return;
					::ultralight::MouseEvent evt;
					evt.type   = ::ultralight::MouseEvent::kType_MouseDown;
					evt.x      = x;
					evt.y      = y;
					evt.button = ToUltralightButton(button);
					mView->FireMouseEvent(evt);
				}

				void InjectMouseUp(Dia::Input::EMouseButton button, int x, int y)
				{
					if (!mView) return;
					::ultralight::MouseEvent evt;
					evt.type   = ::ultralight::MouseEvent::kType_MouseUp;
					evt.x      = x;
					evt.y      = y;
					evt.button = ToUltralightButton(button);
					mView->FireMouseEvent(evt);
				}

				void InjectMouseClick(Dia::Input::EMouseButton button, int x, int y)
				{
					InjectMouseMove(x, y);
					InjectMouseDown(button, x, y);
					InjectMouseUp(button, x, y);
				}

				void InjectMouseWheel(int scroll_vert, int scroll_horz)
				{
					if (!mView) return;
					::ultralight::ScrollEvent evt;
					evt.type        = ::ultralight::ScrollEvent::kType_ScrollByPixel;
					evt.delta_x     = scroll_horz;
					evt.delta_y     = scroll_vert;
					mView->FireScrollEvent(evt);
				}

				// LoadListener
				virtual void OnDOMReady(::ultralight::View* caller, uint64_t /*frame_id*/,
					bool is_main_frame, const ::ultralight::String& /*url*/) override
				{
					if (!is_main_frame) return;

					auto jsCtx = caller->LockJSContext();
					::ultralight::SetJSContext(jsCtx->ctx());

					// Create the global 'app' object and bind all methods
					::ultralight::JSObject global = ::ultralight::JSGlobalObject();

					::ultralight::JSObject appObj;
					// JSPropertyValue doesn't accept JSObject directly — assign via JSObjectRef cast to JSValue
					global["app"] = ::ultralight::JSValue(static_cast<JSObjectRef>(appObj));

					for (auto& binding : mPendingBindings)
					{
						BoundMethod method = binding.method;
						std::string name   = binding.name;

						appObj[name.c_str()] = ::ultralight::JSCallbackWithRetval(
							[method](const ::ultralight::JSObject& /*thisObj*/,
								const ::ultralight::JSArgs& args) mutable -> ::ultralight::JSValue
							{
								BoundMethodArgs diaArgs;
								for (uint32_t i = 0; i < static_cast<uint32_t>(args.size()); ++i)
								{
									const ::ultralight::JSValue& v = args[i];
									if (v.IsBoolean())
										diaArgs.Add(BoundMethodValue(v.ToBoolean()));
									else if (v.IsNumber())
										diaArgs.Add(BoundMethodValue(static_cast<double>(v.ToNumber())));
									else if (v.IsString())
									{
										::ultralight::String s = v.ToString();
										Dia::Core::Containers::String64 str64(s.utf8().data());
										diaArgs.Add(BoundMethodValue(str64));
									}
								}

								if (method.GetReturnValueFlag() == BoundMethod::ReturnValueFlag::kEnabled)
								{
									BoundMethodValue retVal = method.GetMethodReturnPtr()(diaArgs);
									switch (retVal.GetType())
									{
									case BoundMethodValue::EType::kBoolean:
										return ::ultralight::JSValue(retVal.GetBoolean());
									case BoundMethodValue::EType::kInteger:
										return ::ultralight::JSValue(static_cast<int32_t>(retVal.GetInteger()));
									case BoundMethodValue::EType::kDouble:
										return ::ultralight::JSValue(retVal.GetDouble());
									case BoundMethodValue::EType::kString:
										return ::ultralight::JSValue(retVal.GetString().AsCStr());
									default:
										return ::ultralight::JSValue();
									}
								}
								else
								{
									method.GetMethodPtr()(diaArgs);
									return ::ultralight::JSValue();
								}
							});
					}
				}

				virtual void OnFailLoading(::ultralight::View* /*caller*/, uint64_t /*frame_id*/,
					bool is_main_frame, const ::ultralight::String& url,
					const ::ultralight::String& description,
					const ::ultralight::String& /*error_domain*/, int error_code) override
				{
					if (is_main_frame)
					{
						DIA_ASSERT(0, "Ultralight failed to load page. URL: %s  Error %d: %s",
							url.utf8().data(), error_code, description.utf8().data());
					}
					else
					{
						DIA_LOG_ERROR("UI", "Ultralight sub-resource load failed. URL: %s  Error %d: %s",
							url.utf8().data(), error_code, description.utf8().data());
					}
				}

				virtual void OnAddConsoleMessage(::ultralight::View* /*caller*/,
					::ultralight::MessageSource /*source*/, ::ultralight::MessageLevel level,
					const ::ultralight::String& message, uint32_t line_number,
					uint32_t /*column_number*/, const ::ultralight::String& source_id) override
				{
					if (level == ::ultralight::kMessageLevel_Error)
					{
						DIA_LOG_ERROR("UI", "JS Error - Source: %s, Line: %d, Message: %s",
							source_id.utf8().data(), line_number, message.utf8().data());
					}
					else if (level == ::ultralight::kMessageLevel_Warning)
					{
						DIA_LOG_WARNING("UI", "JS Warning - Source: %s, Line: %d, Message: %s",
							source_id.utf8().data(), line_number, message.utf8().data());
					}
					else
					{
						DIA_LOG_DEBUG("UI", "JS Log - Source: %s, Line: %d, Message: %s",
							source_id.utf8().data(), line_number, message.utf8().data());
					}
				}

			private:
				static ::ultralight::MouseEvent::Button ToUltralightButton(Dia::Input::EMouseButton button)
				{
					switch (button)
					{
					case Dia::Input::EMouseButton::kLeft:   return ::ultralight::MouseEvent::kButton_Left;
					case Dia::Input::EMouseButton::kMiddle: return ::ultralight::MouseEvent::kButton_Middle;
					case Dia::Input::EMouseButton::kRight:  return ::ultralight::MouseEvent::kButton_Right;
					default:                                return ::ultralight::MouseEvent::kButton_None;
					}
				}

				const Window::IWindow*                     mWindowContext;
				bool                                       mIsInitialized;
				uint32_t                                   mWidth;
				uint32_t                                   mHeight;
				std::unique_ptr<DiaFileSystem>             mFileSystem;
				std::unique_ptr<DiaLogger>                 mLogger;
				::ultralight::RefPtr<::ultralight::Renderer> mRenderer;
				::ultralight::RefPtr<::ultralight::View>     mView;
				std::vector<PendingBinding>                mPendingBindings;

				static const int sBufferSize = (16 * 1024 * 1024);
				mutable unsigned char mStagingBuffer[sBufferSize];
			};

			//-------------------------------------------------------------------
			UISystem::UISystem(const Window::IWindow* windowContext)
				: IUISystem()
				, mIsPageLoaded(false)
				, mUISystemImpl(nullptr)
			{
				mUISystemImpl = DIA_NEW(UISystemImpl(windowContext));
				mUIHandler.SetUISystem(this);
			}

			//-------------------------------------------------------------------
			UISystem::~UISystem()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				DIA_DELETE(mUISystemImpl);
			}

			//-------------------------------------------------------------------
			UIHandler* UISystem::GetUIHandler()
			{
				return &mUIHandler;
			}

			//-------------------------------------------------------------------
			void UISystem::Initialize()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->Initialize();
				mIsPageLoaded = false;
			}

			//-------------------------------------------------------------------
			void UISystem::Shutdown()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mIsPageLoaded = false;
			}

			//-------------------------------------------------------------------
			IPage* UISystem::CreatePage(const char* /*url*/, int /*width*/, int /*height*/)
			{
				DIA_ASSERT(0, "CreatePage not supported by Ultralight backend");
				return nullptr;
			}

			//-------------------------------------------------------------------
			void UISystem::DestroyPage(IPage* /*page*/)
			{
				DIA_ASSERT(0, "DestroyPage not supported by Ultralight backend");
			}

			//-------------------------------------------------------------------
			int UISystem::GetPageCount() const
			{
				return mIsPageLoaded ? 1 : 0;
			}

			//-------------------------------------------------------------------
			void UISystem::LoadPage(Page& newPage)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->LoadPage(newPage);
				mIsPageLoaded = true;
			}

			//-------------------------------------------------------------------
			void UISystem::UnloadPage()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mIsPageLoaded = false;
			}

			//-------------------------------------------------------------------
			bool UISystem::IsPageLoaded() const
			{
				return mIsPageLoaded;
			}

			//-------------------------------------------------------------------
			void UISystem::Update()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->Update();
			}

			//-------------------------------------------------------------------
			void UISystem::FetchUIDataBuffer(UIDataBuffer& outBuffer) const
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
				mUISystemImpl->InjectMouseMove(x, y);
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
