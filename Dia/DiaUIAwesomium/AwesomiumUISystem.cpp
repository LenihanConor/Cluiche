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
#include <DiaCore/Core/Log.h>

#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/Page.h>

#include  <DiaWindow/SystemHandle.h>
#include  <DiaWindow/Interface/IWindow.h>

#include <Awesomium/WebCore.h>
#include <Awesomium/BitmapSurface.h>
#include <Awesomium/STLHelpers.h>

#include <thread>
#include <map>
#include <utility>

namespace Dia
{
	namespace UI
	{
		namespace Awesomium
		{
			// Key/value pair of error code/{error name&error description}
			std::map<int, std::pair<const char*, const char*>> sErrorDetailMap = { 
				{-2, {"FAILED","A generic failure occurred."}},
				{-3, {"ABORTED","An operation was aborted (due to user action)."}},
				{-4, {"INVALID_ARGUMENT","An argument to the function is incorrect."}},
				{-5, {"INVALID_HANDLE","The handle or file descriptor is invalid."}},
				{-6, {"FILE_NOT_FOUND","The file or directory cannot be found."}},
				{-7, {"TIMED_OUT","An operation timed out."}},
				{-8, {"FILE_TOO_BIG","The file is too large."}},
				{-9, {"UNEXPECTED","An unexpected error. This may be caused by a programming mistake or an invalid assumption."}},
				{-10, {"ACCESS_DENIED","Permission to access a resource, other than the network, was denied."}},
				{-11, {"NOT_IMPLEMENTED","The operation failed because of unimplemented functionality."}},
				{-12, {"INSUFFICIENT_RESOURCES","There were not enough resources to complete the operation."}},
				{-13, {"OUT_OF_MEMORY","Memory allocation failed."}},
				{-14, {"UPLOAD_FILE_CHANGED","The file upload failed because the file's modification time was different from the expectation."}},
				{-15, {"SOCKET_NOT_CONNECTED","The socket is not connected."}},
				{-16, {"FILE_EXISTS","The file already exists."}},
				{-17, {"FILE_PATH_TOO_LONG","The path or file name is too long."}},
				{-18, {"FILE_NO_SPACE","Not enough room left on the disk."}},
				{-19, {"FILE_VIRUS_INFECTED","The file has a virus."}},
				{-20, {"BLOCKED_BY_CLIENT","The client chose to block the request."}},
				{-100,{"CONNECTION_CLOSED","A connection was closed (corresponding to a TCP FIN)." } },
				{-101,{"CONNECTION_RESET","A connection was reset (corresponding to a TCP RST)." } },
				{-102,{"CONNECTION_REFUSED","A connection attempt was refused." } },
				{-103,{"CONNECTION_ABORTED","This can include a FIN packet that did not get ACK'd." } },
				{-104,{"CONNECTION_FAILED","A connection attempt failed." } },
				{-105,{"NAME_NOT_RESOLVED","The host name could not be resolved." } },
				{-106,{"INTERNET_DISCONNECTED","The Internet connection has been lost." } },
				{-107,{"SSL_PROTOCOL_ERROR","An SSL protocol error occurred." } },
				{-108,{"ADDRESS_INVALID","The IP address or port number is invalid (e.g., cannot connect to the IP address 0 or the port 0)." } },
				{-109,{"ADDRESS_UNREACHABLE","The IP address is unreachable. This usually means that there is no route to the specified host or network." } },
				{-110,{"SSL_CLIENT_AUTH_CERT_NEEDED","The server requested a client certificate for SSL client authentication." } },
				{-111,{"TUNNEL_CONNECTION_FAILED","A tunnel connection through the proxy could not be established." } },
				{-112,{"NO_SSL_VERSIONS_ENABLED","No SSL protocol versions are enabled." } },
				{-113,{"SSL_VERSION_OR_CIPHER_MISMATCH","cipher suite." } },
				{-114,{"SSL_RENEGOTIATION_REQUESTED","The server requested a renegotiation (rehandshake)." } },
				{-115,{"PROXY_AUTH_UNSUPPORTED","The proxy requested authentication (for tunnel establishment) with an unsupported method." } },
				{-116,{"CERT_ERROR_IN_SSL_RENEGOTIATION","During SSL renegotiation (rehandshake), the server sent a certificate with an error." } },
				{-117,{"BAD_SSL_CLIENT_AUTH_CERT","The SSL handshake failed because of a bad or missing client certificate." } },
				{-118,{"CONNECTION_TIMED_OUT","A connection attempt timed out." } },
				{-119,{"HOST_RESOLVER_QUEUE_TOO_LARGE","There are too many pending DNS resolves, so a request in the queue was aborted." } },
				{-120,{"SOCKS_CONNECTION_FAILED","Failed establishing a connection to the SOCKS proxy server for a target host." } },
				{-121,{"SOCKS_CONNECTION_HOST_UNREACHABLE"," The SOCKS proxy server failed establishing connection to the target host because that host is unreachable." } },
				{-122,{"NPN_NEGOTIATION_FAILED","The request to negotiate an alternate protocol failed." } },
				{-123,{"SSL_NO_RENEGOTIATION","The peer sent an SSL no_renegotiation alert message." } },
				{-124,{"WINSOCK_UNEXPECTED_WRITTEN_BYTES","Winsock sometimes reports more data written than passed.This is probably due to a broken LSP." } },
				{-125,{"SSL_DECOMPRESSION_FAILURE_ALERT","An SSL peer sent us a fatal decompression_failure alert. This typically occurs when a peer selects DEFLATE compression in the mistaken belief that it supports it." } },
				{-126,{"SSL_BAD_RECORD_MAC_ALERT","An SSL peer sent us a fatal bad_record_mac alert. This has been observed from servers with buggy DEFLATE support." } },
				{-127,{"PROXY_AUTH_REQUESTED","The proxy requested authentication (for tunnel establishment)." } },
				{-128,{"SSL_UNSAFE_NEGOTIATION","A known TLS strict server didn't offer the renegotiation extension." } },
				{-129,{"SSL_WEAK_SERVER_EPHEMERAL_DH_KEY"," The SSL server attempted to use a weak ephemeral Diffie-Hellman key." } },
				{-130,{"PROXY_CONNECTION_FAILED","Could not create a connection to the proxy server. An error occurred either in resolving its name, or in connecting a socket to it. Note that this does NOT include failures during the actual \"CONNECT\" method of an HTTP proxy." } },
				{-131,{"MANDATORY_PROXY_CONFIGURATION_FAILED","A mandatory proxy configuration could not be used. Currently this mean that a mandatory PAC script could not be fetched, parsed or executed." } },
				{-133,{"PRECONNECT_MAX_SOCKET_LIMIT","We've hit the max socket limit for the socket pool while pre-connecting. We don't bother trying to pre-connect more sockets." } },
				{-134,{"SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED","The permission to use the SSL client certificate's private key was denied."}},
				{-135,{"SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY","The SSL client certificate has no private key." }},
				{-136,{"PROXY_CERTIFICATE_INVALID","The certificate presented by the HTTPS Proxy was invalid." } },
				{-137,{"NAME_RESOLUTION_FAILED","An error occurred when trying to do a name resolution (DNS)." } },
				{-138,{"NETWORK_ACCESS_DENIED","Permission to access the network was denied. This is used to distinguish errors that were most likely caused by a firewall from other access denied errors. See also ERR_ACCESS_DENIED."}},
				{-139,{"TEMPORARILY_THROTTLED","The request throttler module canceled this request to avoid DDOS." }},
				{-140,{"HTTPS_PROXY_TUNNEL_RESPONSE","A request to create an SSL tunnel connection through the HTTPS proxy received a non-200 (OK) and non-407 (Proxy Auth) response. The responsebody might include a description of why the request failed." } },
				{-141,{"SSL_CLIENT_AUTH_SIGNATURE_FAILED","We were unable to sign the CertificateVerify data of an SSL client auth handshake with the client certificate's private key." } },
				{-142,{"MSG_TOO_BIG","The message was too large for the transport.(for example a UDP message which exceeds size threshold)." } },
				{-143,{"SPDY_SESSION_ALREADY_EXISTS","A SPDY session already exists, and should be used instead of this connection." } },
				{-146,{"PROTOCOL_SWITCHED","Connection was aborted for switching to another protocol. WebSocket abort SocketStream connection when alternate protocol is found." } },
				{-147,{"ADDRESS_IN_USE","Returned when attempting to bind an address that is already in use." } },
				{-148,{"SSL_HANDSHAKE_NOT_COMPLETED","An operation failed because the SSL handshake has not completed." } },
				{-149,{"SSL_BAD_PEER_PUBLIC_KEY","SSL peer's public key is invalid." } },
				{-150,{"SSL_PINNED_KEY_NOT_IN_CERT_CHAIN","The certificate didn't match the built-in public key pins for the host name. The pins are set in net/base/transport_security_state. cc and require that one of a set of public keys exist on the path from the leaf to the root."}},
				{-151,{"CLIENT_AUTH_CERT_TYPE_UNSUPPORTED","Server request for client certificate did not contain any types we support." }},
				{-152,{"ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH","Server requested one type of cert, then requested a different type while the first was still being generated." } }
				};

			class UISystemImpl: public Application::Listener,
										::Awesomium::WebViewListener::View,
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
					// Get Url to load
					Dia::Core::FilePath::ResoledFilePath newPageUrl;
					newPage.GetUrl().Resolve(newPageUrl);

					::Awesomium::WebURL url(::Awesomium::WSLit(newPageUrl.AsCStr()));

					mView->set_js_method_handler(&mMethodDispatcher);

					Dia::UI::BoundMethodList boundMethodList = newPage.GetBoundMenthods();

					::Awesomium::JSValue result = mView->CreateGlobalJavascriptObject(::Awesomium::WSLit("app"));
					if (result.IsObject())
					{
						::Awesomium::JSObject& app_object = result.ToObject();

						for (unsigned int i = 0; i < boundMethodList.Size(); i++)
						{
							const Dia::Core::Containers::String32& name = boundMethodList[i].GetName();

							BoundMethod::ReturnValueFlag returnValueFlag = boundMethodList[i].GetReturnValueFlag();

							switch (returnValueFlag)
							{
							case BoundMethod::ReturnValueFlag::kDisabled:
								{
									Dia::UI::BoundMethod::MethodPtr func = boundMethodList[i].GetMethodPtr();
									mMethodDispatcher.Bind(app_object,
										::Awesomium::WSLit(name.AsCStr()),
										JSDelegate(func));
								}
								break;
							case BoundMethod::ReturnValueFlag::kEnabled:
								{
									Dia::UI::BoundMethod::MethodPtrWithRetVal func = boundMethodList[i].GetMethodReturnPtr();
									mMethodDispatcher.BindWithRetval(app_object,
										::Awesomium::WSLit(name.AsCStr()),
										JSDelegateWithRetval(func));
								}
							break;
							default:
								break;
							}

							
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
					mView = mPlatformAbstractedUIApplicaton->web_core()->CreateWebView(static_cast<int>(mWindowContext->GetSize().X()), static_cast<int>(mWindowContext->GetSize().Y()));
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
						DIA_ASSERT(buffersize < sBuffersize, "Awesomium failed to get allocate a large enough buffersize"); // Need to increase size of UI buffer

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
				// This event occurs when the page title has changed.
				virtual void OnChangeTitle(::Awesomium::WebView* caller,
					const ::Awesomium::WebString& title) override {}

				//-------------------------------------------------------------------
				// This event occurs when the page URL has changed.
				virtual void OnChangeAddressBar(::Awesomium::WebView* caller,
					const ::Awesomium::WebURL& url) override {}

				//-------------------------------------------------------------------
				// This event occurs when the tooltip text has changed. You
				// should hide the tooltip when the text is empty.
				virtual void OnChangeTooltip(::Awesomium::WebView* caller,
					const ::Awesomium::WebString& tooltip) override {}

				//-------------------------------------------------------------------
				// This event occurs when the target URL has changed. This
				// is usually the result of hovering over a link on a page.
				virtual void OnChangeTargetURL(::Awesomium::WebView* caller,
					const ::Awesomium::WebURL& url) override {}

				//-------------------------------------------------------------------
				// This event occurs when the cursor has changed. This is
				// is usually the result of hovering over different content.
				virtual void OnChangeCursor(::Awesomium::WebView* caller,
					::Awesomium::Cursor cursor) override {}

				//-------------------------------------------------------------------
				// This event occurs when the focused element changes on the page.
				// This is usually the result of textbox being focused or some other
				// user-interaction event.
				virtual void OnChangeFocus(::Awesomium::WebView* caller,
					::Awesomium::FocusedElementType focused_type)override {}

				//-------------------------------------------------------------------
				// This event occurs when a message is added to the console on the page.
				// This is usually the result of a JavaScript error being encountered
				// on a page.
				virtual void OnAddConsoleMessage(::Awesomium::WebView* caller,
					const ::Awesomium::WebString& message,
					int line_number,
					const ::Awesomium::WebString& source)override
				{
					Dia::Core::Log::OutputVaradicLine("DiaAwesomiumUI Log - Source: %s, Line: %d, Message: %s", source.data(), line_number, message.data());
				}

				//-------------------------------------------------------------------
				// This event occurs when a WebView creates a new child WebView
				// (usually the result of window.open or an external link). It
				// is your responsibility to display this child WebView in your
				// application. You should call Resize on the child WebView
				// immediately after this event to make it match your container
				// size.
				//
				// If this is a child of a Windowed WebView, you should call
				// WebView::set_parent_window on the new view immediately within
				// this event
				virtual void OnShowCreatedWebView(::Awesomium::WebView* caller,
					::Awesomium::WebView* new_view,
					const ::Awesomium::WebURL& opener_url,
					const ::Awesomium::WebURL& target_url,
					const ::Awesomium::Rect& initial_pos,
					bool is_popup) override {}

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
					Dia::Core::Containers::String256 strUrl;
					url.filename().ToUTF8(strUrl.AsCStr(), strUrl.Size());
					
					const char* errorName = "Unknown"; 
					const char* errorDesc = "Find error code at: http://docs.awesomium.net/html/T_Awesomium_Core_LoadingFrameFailedEventArgs.htm";
					auto it = sErrorDetailMap.find(error_code);
					if (it != sErrorDetailMap.end())
					{
						errorName = it->second.first;
						errorDesc = it->second.second;
					}

					DIA_ASSERT(0, "Failed to load %s. \n\tError Code: %d. \n\tName: %s\n\tDesc: %s", strUrl.AsCStr(), error_code, errorName, errorDesc);
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

				mIsPageLoaded = false;
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
			bool UISystem::IsPageLoaded()const
			{
				return mIsPageLoaded;
			}

			//-------------------------------------------------------------------
			void UISystem::UnloadPage()
			{
				DIA_ASSERT(mUISystemImpl, "mUISystemImpl is NULL");

				std::lock_guard<std::mutex> lock(mSystemMutex);

				mIsPageLoaded = false;

				DIA_ASSERT(0, "Currently cannot unload page");
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