////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrame.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Core/Assert.h>

#include <DiaUI/UIDataBuffer.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// UIFrameData - A single frame that stores all the info that the render that is for UI
		///
		class UIFrameData
		{
		public:
			UIFrameData();
			~UIFrameData();

			// Required by system
			void ClearUIBuffer();
			void CopyUIBuffer(const UIFrameData& rhs);

			void RequestDrawUI(const Dia::UI::UIDataBuffer& object);

			const Dia::UI::UIDataBuffer& GetUIData()const { return mData; }
	
		private:
			Dia::UI::UIDataBuffer mData;
		};
	}
}