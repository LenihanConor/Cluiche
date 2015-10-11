////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/UIFrameData.h"

namespace Dia
{
	namespace Graphics
	{
		//------------------------------------------------------------------------------
		UIFrameData::UIFrameData()
		{}
		
		//------------------------------------------------------------------------------
		UIFrameData::~UIFrameData()
		{}

		//------------------------------------------------------------------------------
		void UIFrameData::ClearUIBuffer()
		{
			mData.Destroy();
		}

		//------------------------------------------------------------------------------
		void UIFrameData::CopyUIBuffer(const UIFrameData& rhs)
		{
			mData = rhs.mData;
		}

		//------------------------------------------------------------------------------
		void UIFrameData::RequestDrawUI(const Dia::UI::UIDataBuffer& object)
		{
			mData = object;
		}
	}
}