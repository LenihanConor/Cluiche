////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia
{
	namespace UI
	{
		class BoundMethod;
		class UIDataBuffer;

		class IPage
		{
		public:
			virtual ~IPage() {}

			virtual void LoadURL(const char* url) = 0;
			virtual void LoadHTML(const char* html, const char* baseURL) = 0;

			virtual void ExecuteJavaScript(const char* script) = 0;
			virtual void SetCallback(const char* callbackName, BoundMethod* method) = 0;
			virtual void RemoveCallback(const char* callbackName) = 0;

			virtual bool IsLoading() const = 0;
			virtual const char* GetURL() const = 0;

			virtual void Resize(int width, int height) = 0;
			virtual void GetTextureData(UIDataBuffer& outBuffer) const = 0;

			virtual int GetPageId() const = 0;
		};
	}
}
