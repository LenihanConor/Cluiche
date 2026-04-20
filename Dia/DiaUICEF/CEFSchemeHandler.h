////////////////////////////////////////////////////////////////////////////////
// Filename: CEFSchemeHandler.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_scheme.h>
#include <include/cef_resource_handler.h>

#include <string>
#include <vector>

namespace Dia
{
	namespace UICEF
	{
		class CEFSchemeHandlerFactory : public CefSchemeHandlerFactory
		{
		public:
			CEFSchemeHandlerFactory(const std::string& basePath);

			CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
				CefRefPtr<CefFrame> frame, const CefString& scheme_name,
				CefRefPtr<CefRequest> request) override;

		private:
			std::string mBasePath;

			IMPLEMENT_REFCOUNTING(CEFSchemeHandlerFactory);
		};

		class CEFResourceHandler : public CefResourceHandler
		{
		public:
			CEFResourceHandler(const std::string& filePath);

			bool Open(CefRefPtr<CefRequest> request, bool& handle_request,
				CefRefPtr<CefCallback> callback) override;
			void GetResponseHeaders(CefRefPtr<CefResponse> response,
				int64_t& response_length, CefString& redirectUrl) override;
			bool Read(void* data_out, int bytes_to_read, int& bytes_read,
				CefRefPtr<CefResourceReadCallback> callback) override;
			void Cancel() override;

		private:
			bool LoadFile();

			std::string mFilePath;
			std::vector<unsigned char> mFileData;
			size_t mReadOffset;

			IMPLEMENT_REFCOUNTING(CEFResourceHandler);
		};

		void RegisterDiaScheme(CefRawPtr<CefSchemeRegistrar> registrar);
		void RegisterDiaSchemeHandlerFactory(const std::string& basePath);
	}
}
