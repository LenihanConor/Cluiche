//////////////////
#pragma once


namespace Dia
{
	namespace UI
	{
		class UIDataBuffer;

		// Inteface for rendering to interact with UI
		class IUIRendering
		{
		public:
			virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer)const = 0;
		};
	}
}