////////////////////////////////////////////////////////////////////////////////
// Filename: CEFSchemeHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFSchemeHandler.h"
#include "CEFUtils.h"

#include <DiaObservation/Log/DiaLog.h>

#include <include/cef_scheme.h>

#include <cstdio>

namespace Dia
{
	namespace UICEF
	{
		//-------------------------------------------------------------------
		// CEFSchemeHandlerFactory
		//-------------------------------------------------------------------

		CEFSchemeHandlerFactory::CEFSchemeHandlerFactory(const std::string& basePath)
			: mBasePath(basePath)
		{
		}

		CefRefPtr<CefResourceHandler> CEFSchemeHandlerFactory::Create(
			CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
			const CefString& /*scheme_name*/, CefRefPtr<CefRequest> request)
		{
			std::string url = request->GetURL().ToString();
			std::string path = Utils::ExtractPathFromDiaURL(url);

			if (Utils::IsPathTraversal(path))
			{
				DIA_LOG_ERROR("UI", "DiaUICEF: Path traversal rejected: %s", path.c_str());
				return nullptr;
			}

			std::string filePath = mBasePath + path;
			return new CEFResourceHandler(filePath);
		}

		//-------------------------------------------------------------------
		// CEFResourceHandler
		//-------------------------------------------------------------------

		CEFResourceHandler::CEFResourceHandler(const std::string& filePath)
			: mFilePath(filePath)
			, mReadOffset(0)
		{
		}

		bool CEFResourceHandler::Open(CefRefPtr<CefRequest> /*request*/,
			bool& handle_request, CefRefPtr<CefCallback> /*callback*/)
		{
			handle_request = true;
			return LoadFile();
		}

		void CEFResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
			int64_t& response_length, CefString& /*redirectUrl*/)
		{
			if (mFileData.empty())
			{
				response->SetStatus(404);
				response->SetStatusText("Not Found");
				response_length = 0;
				return;
			}

			response->SetStatus(200);
			response->SetStatusText("OK");
			response->SetMimeType(Utils::GetMimeType(mFilePath));
			response->SetHeaderByName("Access-Control-Allow-Origin", "*", true);
			response_length = static_cast<int64_t>(mFileData.size());
		}

		bool CEFResourceHandler::Read(void* data_out, int bytes_to_read, int& bytes_read,
			CefRefPtr<CefResourceReadCallback> /*callback*/)
		{
			if (mReadOffset >= mFileData.size())
			{
				bytes_read = 0;
				return false;
			}

			size_t remaining = mFileData.size() - mReadOffset;
			size_t toRead = (static_cast<size_t>(bytes_to_read) < remaining)
				? static_cast<size_t>(bytes_to_read) : remaining;

			memcpy(data_out, &mFileData[mReadOffset], toRead);
			mReadOffset += toRead;
			bytes_read = static_cast<int>(toRead);
			return true;
		}

		void CEFResourceHandler::Cancel()
		{
			mFileData.clear();
			mReadOffset = 0;
		}

		bool CEFResourceHandler::LoadFile()
		{
			FILE* f = nullptr;
			fopen_s(&f, mFilePath.c_str(), "rb");
			if (!f)
			{
				DIA_LOG_ERROR("UI", "DiaUICEF: File not found: %s", mFilePath.c_str());
				return false;
			}

			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);

			std::string raw(static_cast<size_t>(size), '\0');
			fread(&raw[0], 1, static_cast<size_t>(size), f);
			fclose(f);

			if (IsPluginHtmlPath(mFilePath))
			{
				std::string injected = InjectThemeLinks(raw);
				mFileData.assign(injected.begin(), injected.end());
			}
			else
			{
				mFileData.assign(raw.begin(), raw.end());
			}

			mReadOffset = 0;
			return true;
		}

		bool CEFResourceHandler::IsPluginHtmlPath(const std::string& filePath)
		{
			auto hasExt = [&](const std::string& ext) {
				return filePath.size() >= ext.size() &&
					filePath.compare(filePath.size() - ext.size(), ext.size(), ext) == 0;
			};
			if (!hasExt(".html") && !hasExt(".htm"))
				return false;

			auto containsSlash = [&](const std::string& seg) {
				std::string fwd = filePath;
				for (char& c : fwd) if (c == '\\') c = '/';
				return fwd.find("/" + seg + "/") != std::string::npos ||
					fwd.find("\\" + seg + "\\") != std::string::npos;
			};
			return containsSlash("plugins");
		}

		std::string CEFResourceHandler::InjectThemeLinks(const std::string& html)
		{
			// Script runs before CSS loads, so data-theme="dark" is set when Pico applies its selectors.
			static const std::string kLinks =
				"<script>document.documentElement.setAttribute('data-theme','dark');</script>\n"
				"<link rel=\"stylesheet\" href=\"dia://theme/pico.min.css\">\n"
				"<link rel=\"stylesheet\" href=\"dia://theme/dia-overrides.css\">\n";

			// Insert after <head> if present, otherwise before <body>
			auto findCaseInsensitive = [](const std::string& haystack, const std::string& needle) -> size_t {
				if (needle.empty()) return 0;
				for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
				{
					bool match = true;
					for (size_t j = 0; j < needle.size(); ++j)
					{
						if (tolower((unsigned char)haystack[i + j]) != tolower((unsigned char)needle[j]))
						{
							match = false;
							break;
						}
					}
					if (match) return i;
				}
				return std::string::npos;
			};

			size_t headPos = findCaseInsensitive(html, "<head>");
			if (headPos != std::string::npos)
			{
				size_t insertPos = headPos + 6; // after <head>
				return html.substr(0, insertPos) + "\n" + kLinks + html.substr(insertPos);
			}

			size_t bodyPos = findCaseInsensitive(html, "<body");
			if (bodyPos != std::string::npos)
			{
				return html.substr(0, bodyPos) + "<head>\n" + kLinks + "</head>\n" + html.substr(bodyPos);
			}

			return html;
		}

		//-------------------------------------------------------------------
		// Registration
		//-------------------------------------------------------------------

		void RegisterDiaScheme(CefRawPtr<CefSchemeRegistrar> registrar)
		{
			registrar->AddCustomScheme("dia",
				CEF_SCHEME_OPTION_STANDARD |
				CEF_SCHEME_OPTION_CORS_ENABLED |
				CEF_SCHEME_OPTION_SECURE);
		}

		void RegisterDiaSchemeHandlerFactory(const std::string& basePath)
		{
			CefRefPtr<CEFSchemeHandlerFactory> factory = new CEFSchemeHandlerFactory(basePath);
			CefRegisterSchemeHandlerFactory("dia", "", factory);
		}
	}
}
