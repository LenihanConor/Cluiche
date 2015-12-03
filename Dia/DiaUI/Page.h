//////////////////
#pragma once

#include <DiaCore/Strings/String256.h>
#include <DiaUI/BoundMethod.h>

namespace Dia
{
	namespace UI
	{
		//------------------------------------------------------
		// A page is the C++ link/representation of a UI screen. 
		class Page
		{
		public:
			Page() {};
			Page(const Dia::Core::Containers::String256& url)
				: mUrl(url)
			{}

			const Dia::Core::Containers::String256& GetUrl()const { return mUrl; };
			BoundMethodList& GetBoundMenthods() { return mBoundMethodList; }
			
			void BindMethod(BoundMethod& method)
			{
				mBoundMethodList.Add(method);
			}

		private:
			Dia::Core::Containers::String256 mUrl;		// Address for page, this is used to load the page.
			BoundMethodList mBoundMethodList;			// List of all bound messages from UI -> C++
		};
	}
}