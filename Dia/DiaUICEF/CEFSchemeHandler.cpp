////////////////////////////////////////////////////////////////////////////////
// Filename: CEFSchemeHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFSchemeHandler.h"
#include "CEFUtils.h"

#include <DiaLogger/DiaLog.h>

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

			mFileData.resize(static_cast<size_t>(size));
			fread(mFileData.data(), 1, static_cast<size_t>(size), f);
			fclose(f);

			mReadOffset = 0;
			return true;
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
