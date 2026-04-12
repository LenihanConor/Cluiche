////////////////////////////////////////////////////////////////////////////////
// Filename: Frame.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/FrameData.h"

namespace Dia
{
	namespace Graphics
	{
		FrameData::FrameData()
		{}

		FrameData& FrameData::operator=(const FrameData& rhs)
		{
			Copy(rhs);

			return *this;
		}

		void FrameData::Clear()
		{
			DebugFrameData::ClearDebugBuffer();
			UIFrameData::ClearUIBuffer();
			EntityFrameData::Clear();
		}

		void FrameData::Copy(const FrameData& rhs)
		{
			DebugFrameData::CopyDebugBuffer(rhs);
			UIFrameData::CopyUIBuffer(rhs);

			// Copy entity frame data (sprites)
			const Core::Containers::DynamicArrayC<SpriteDrawCommand, 256>& sprites = rhs.GetSprites();
			for (unsigned int i = 0; i < sprites.Size(); ++i)
			{
				RequestDrawSprite(sprites[i]);
			}
		}
	}
}