////////////////////////////////////////////////////////////////////////////////
// Filename: UltralightUISystem.cpp
////////////////////////////////////////////////////////////////////////////////
#include "UltralightUISystem.h"

// DiaWindow must be included before Ultralight to avoid Win32 macro conflicts
// (Ultralight pulls in <windows.h> which redefines symbols like GetSystemHandle)
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaUI/IPage.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>
#include <DiaWindow/Interface/IWindow.h>
#include <DiaInput/EKeyModifiers.h>
#include <DiaInput/InputRouter.h>

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
#include <Ultralight/KeyEvent.h>
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
			// Maps Dia EKey integer value to Ultralight GK_ virtual key code.
			// EKey values: A=0..Z=25, Num0=26..Num9=35, Escape=36, LControl=37,
			// LShift=38, LAlt=39, LSystem=40, RControl=41, RShift=42, RAlt=43,
			// RSystem=44, Menu=45, LBracket=46, RBracket=47, SemiColon=48, Comma=49,
			// Period=50, Quote=51, Slash=52, BackSlash=53, Tilde=54, Equal=55, Dash=56,
			// Space=57, Return=58, BackSpace=59, Tab=60, PageUp=61, PageDown=62,
			// End=63, Home=64, Insert=65, Delete=66, Add=67, Subtract=68, Multiply=69,
			// Divide=70, Left=71, Right=72, Up=73, Down=74, Numpad0=75..Numpad9=84,
			// F1=85..F15=99, Pause=100. Unknown=1 (alias for B — treated as GK_B).
			static int ToUltralightVirtualKeyCode(Dia::Input::EKey key)
			{
				using namespace ::ultralight::KeyCodes;
				const int v = key.m_IntValue;
				if (v >= 0  && v <= 25) return GK_A + v;
				if (v >= 26 && v <= 35) return GK_0 + (v - 26);
				if (v >= 75 && v <= 84) return GK_NUMPAD0 + (v - 75);
				if (v >= 85 && v <= 99) return GK_F1 + (v - 85);
				switch (v)
				{
				case 36: return GK_ESCAPE;
				case 37: return GK_LCONTROL;
				case 38: return GK_LSHIFT;
				case 39: return GK_LMENU;
				case 40: return GK_LWIN;
				case 41: return GK_RCONTROL;
				case 42: return GK_RSHIFT;
				case 43: return GK_RMENU;
				case 44: return GK_RWIN;
				case 45: return GK_MENU;
				case 46: return GK_OEM_4;
				case 47: return GK_OEM_6;
				case 48: return GK_OEM_1;
				case 49: return GK_OEM_COMMA;
				case 50: return GK_OEM_PERIOD;
				case 51: return GK_OEM_7;
				case 52: return GK_OEM_2;
				case 53: return GK_OEM_5;
				case 54: return GK_OEM_3;
				case 55: return GK_OEM_PLUS;
				case 56: return GK_OEM_MINUS;
				case 57: return GK_SPACE;
				case 58: return GK_RETURN;
				case 59: return GK_BACK;
				case 60: return GK_TAB;
				case 61: return GK_PRIOR;
				case 62: return GK_NEXT;
				case 63: return GK_END;
				case 64: return GK_HOME;
				case 65: return GK_INSERT;
				case 66: return GK_DELETE;
				case 67: return GK_ADD;
				case 68: return GK_SUBTRACT;
				case 69: return GK_MULTIPLY;
				case 70: return GK_DIVIDE;
				case 71: return GK_LEFT;
				case 72: return GK_RIGHT;
				case 73: return GK_UP;
				case 74: return GK_DOWN;
				case 100: return GK_PAUSE;
				default:
					DIA_ASSERT(0, "Unmapped EKey value %d — keyboard event is a no-op", v);
					return 0;
				}
			}

			// Translates Dia EKeyModifiers flags to Ultralight KeyEvent modifier flags.
			static unsigned int ToUltralightModifiers(int diaModifiers)
			{
				unsigned int mods = 0;
				if (diaModifiers & Dia::Input::kModAlt)     mods |= ::ultralight::KeyEvent::kMod_AltKey;
				if (diaModifiers & Dia::Input::kModControl) mods |= ::ultralight::KeyEvent::kMod_CtrlKey;
				if (diaModifiers & Dia::Input::kModSystem)  mods |= ::ultralight::KeyEvent::kMod_MetaKey;
				if (diaModifiers & Dia::Input::kModShift)   mods |= ::ultralight::KeyEvent::kMod_ShiftKey;
				return mods;
			}

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
					, mInputRouter(nullptr)
				{
				}

				~UISystemImpl()
				{
					Shutdown();
				}

				void Shutdown()
				{
					if (!mIsInitialized)
						return;

					mView = nullptr;
					mRenderer = nullptr;

					auto& platform = ::ultralight::Platform::instance();
					platform.set_file_system(nullptr);
					platform.set_logger(nullptr);

					mFileSystem.reset();
					mLogger.reset();

					mIsInitialized = false;
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
					config.resource_path_prefix = "assets/resources/";
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

				void InjectKeyDown(Dia::Input::EKey key, int diaModifiers)
				{
					if (!mView) return;
					const int vk = ToUltralightVirtualKeyCode(key);
					if (vk == 0) return;
					::ultralight::KeyEvent evt;
					evt.type             = ::ultralight::KeyEvent::kType_RawKeyDown;
					evt.virtual_key_code = vk;
					evt.native_key_code  = vk;
					evt.modifiers        = ToUltralightModifiers(diaModifiers);
					evt.is_keypad        = false;
					evt.is_auto_repeat   = false;
					evt.is_system_key    = false;
					::ultralight::GetKeyIdentifierFromVirtualKeyCode(vk, evt.key_identifier);
					mView->FireKeyEvent(evt);
				}

				void InjectKeyUp(Dia::Input::EKey key, int diaModifiers)
				{
					if (!mView) return;
					const int vk = ToUltralightVirtualKeyCode(key);
					if (vk == 0) return;
					::ultralight::KeyEvent evt;
					evt.type             = ::ultralight::KeyEvent::kType_KeyUp;
					evt.virtual_key_code = vk;
					evt.native_key_code  = vk;
					evt.modifiers        = ToUltralightModifiers(diaModifiers);
					evt.is_keypad        = false;
					evt.is_auto_repeat   = false;
					evt.is_system_key    = false;
					::ultralight::GetKeyIdentifierFromVirtualKeyCode(vk, evt.key_identifier);
					mView->FireKeyEvent(evt);
				}

				void InjectCharacterInput(uint32_t codepoint)
				{
					if (!mView) return;
					// Encode codepoint to UTF-8
					char buf[5] = {};
					if (codepoint < 0x80u)
					{
						buf[0] = static_cast<char>(codepoint);
					}
					else if (codepoint < 0x800u)
					{
						buf[0] = static_cast<char>(0xC0u | (codepoint >> 6));
						buf[1] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
					}
					else if (codepoint < 0x10000u)
					{
						buf[0] = static_cast<char>(0xE0u | (codepoint >> 12));
						buf[1] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
						buf[2] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
					}
					else
					{
						buf[0] = static_cast<char>(0xF0u | (codepoint >> 18));
						buf[1] = static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu));
						buf[2] = static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu));
						buf[3] = static_cast<char>(0x80u | (codepoint & 0x3Fu));
					}
					::ultralight::KeyEvent evt;
					evt.type             = ::ultralight::KeyEvent::kType_Char;
					evt.text             = ::ultralight::String(buf);
					evt.unmodified_text  = evt.text;
					evt.virtual_key_code = 0;
					evt.native_key_code  = 0;
					evt.modifiers        = 0;
					evt.is_keypad        = false;
					evt.is_auto_repeat   = false;
					evt.is_system_key    = false;
					mView->FireKeyEvent(evt);
				}

				void SetInputRouter(Dia::Input::InputRouter* router)
				{
					mInputRouter = router;
				}

				void CallJSFunction(const char* fnName, const char* argsJson)
				{
					if (!mView || !mIsInitialized || !fnName) return;
					auto jsCtx = mView->LockJSContext();
					::ultralight::SetJSContext(jsCtx->ctx());
					std::string script(fnName);
					script += "(";
					if (argsJson && argsJson[0] != '\0')
						script += argsJson;
					script += ")";
					mView->EvaluateScript(::ultralight::String(script.c_str()));
				}

				// LoadListener

				// Fires before any page scripts run. Install a queuing proxy for `app` so
				// that calls made from DOMContentLoaded handlers don't throw "app is not
				// defined". Any void call made before OnDOMReady is queued in app.__q and
				// replayed once the real C++ bindings are attached.
				// Return-value calls (GetTestValue etc.) made before OnDOMReady return
				// undefined — queue them after window.onload if you need the return value.
				virtual void OnWindowObjectReady(::ultralight::View* caller, uint64_t /*frame_id*/,
					bool is_main_frame, const ::ultralight::String& /*url*/) override
				{
					if (!is_main_frame) return;

					auto jsCtx = caller->LockJSContext();
					::ultralight::SetJSContext(jsCtx->ctx());

					// Install a Proxy that queues any property access as a callable stub.
					// Queued entries: { n: methodName, a: argsArray }
					// OnDOMReady will replace window.app with the real object and drain __q.
					const char* kProxyScript =
						"window.app = new Proxy({__q:[]}, {"
						"  get: function(t,p) {"
						"    if (p === '__q') return t.__q;"
						"    return function() {"
						"      t.__q.push({n:p, a:Array.prototype.slice.call(arguments)});"
						"      return undefined;"
						"    };"
						"  }"
						"});";

					caller->EvaluateScript(::ultralight::String(kProxyScript));
				}

				// Fires after DOM is parsed and before window.onload. Replace the queuing
				// proxy with the real `app` object bound to C++ methods, then replay any
				// void calls that were queued during DOMContentLoaded.
				virtual void OnDOMReady(::ultralight::View* caller, uint64_t /*frame_id*/,
					bool is_main_frame, const ::ultralight::String& /*url*/) override
				{
					if (!is_main_frame) return;

					auto jsCtx = caller->LockJSContext();
					::ultralight::SetJSContext(jsCtx->ctx());

					// Capture the pending queue before replacing app
					::ultralight::JSObject global = ::ultralight::JSGlobalObject();
					::ultralight::JSArray pendingQueue;
					{
						::ultralight::JSValue existing = global["app"];
						if (existing.IsObject())
						{
							::ultralight::JSObject existingObj = existing.ToObject();
							::ultralight::JSValue q = existingObj["__q"];
							if (q.IsArray())
								pendingQueue = q.ToArray();
						}
					}

					// Build the real app object with all C++ bindings
					::ultralight::JSObject appObj;
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

					// Auto-bind app.PushInputMode / app.PopInputMode if InputRouter is wired.
					if (mInputRouter != nullptr)
					{
						Dia::Input::InputRouter* router = mInputRouter;
						appObj["PushInputMode"] = ::ultralight::JSCallbackWithRetval(
							[router](const ::ultralight::JSObject& /*thisObj*/,
								const ::ultralight::JSArgs& args) mutable -> ::ultralight::JSValue
							{
								if (args.size() >= 1 && args[0].IsString())
								{
									::ultralight::String s = args[0].ToString();
									std::string modeStr = s.utf8().data();
									if (modeStr == "ui_only")
										router->PushInputMode(Dia::Input::EInputRouting::kUIOnly);
									else if (modeStr == "game_only")
										router->PushInputMode(Dia::Input::EInputRouting::kGameOnly);
									else if (modeStr == "game_and_ui")
										router->PushInputMode(Dia::Input::EInputRouting::kGameAndUI);
								}
								return ::ultralight::JSValue();
							});

						appObj["PopInputMode"] = ::ultralight::JSCallbackWithRetval(
							[router](const ::ultralight::JSObject& /*thisObj*/,
								const ::ultralight::JSArgs& /*args*/) mutable -> ::ultralight::JSValue
							{
								router->PopInputMode();
								return ::ultralight::JSValue();
							});
					}

					// Drain the queue: replay any void calls made before bindings were ready.
					// Return-value calls are skipped (their return was already undefined).
					unsigned qLen = pendingQueue.length();
					for (unsigned i = 0; i < qLen; ++i)
					{
						::ultralight::JSValue entry = pendingQueue[i];
						if (!entry.IsObject()) continue;
						::ultralight::JSObject entryObj = entry.ToObject();

						::ultralight::JSValue nameVal = entryObj["n"];
						::ultralight::JSValue argsVal = entryObj["a"];
						if (!nameVal.IsString()) continue;

						::ultralight::String methodNameStr = nameVal.ToString();
						std::string methodName = methodNameStr.utf8().data();
						::ultralight::JSValue methodVal = appObj[methodName.c_str()];
						if (!methodVal.IsObject()) continue;

						// Build JSArgs from the stored array
						::ultralight::JSArgs callArgs;
						if (argsVal.IsArray())
						{
							::ultralight::JSArray arr = argsVal.ToArray();
							unsigned aLen = arr.length();
							for (unsigned j = 0; j < aLen; ++j)
								callArgs.push_back(arr[j]);
						}

						// JSFunction (not JSObject) has operator() for invocation
						::ultralight::JSFunction fn = methodVal;
						fn(appObj, callArgs);
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
				Dia::Input::InputRouter*                   mInputRouter;

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
				mUISystemImpl->Shutdown();
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

			//-------------------------------------------------------------------
			void UISystem::InjectKeyDown(Dia::Input::EKey key, int modifiers)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->InjectKeyDown(key, modifiers);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectKeyUp(Dia::Input::EKey key, int modifiers)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->InjectKeyUp(key, modifiers);
			}

			//-------------------------------------------------------------------
			void UISystem::InjectCharacterInput(uint32_t codepoint)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->InjectCharacterInput(codepoint);
			}

			//-------------------------------------------------------------------
			void UISystem::SetInputRouter(Dia::Input::InputRouter* router)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->SetInputRouter(router);
			}

			//-------------------------------------------------------------------
			void UISystem::CallJSFunction(const char* functionName, const char* argsJson)
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");
				std::lock_guard<std::mutex> lock(mSystemMutex);
				mUISystemImpl->CallJSFunction(functionName, argsJson);
			}
		}
	}
}
