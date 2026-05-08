////////////////////////////////////////////////////////////////////////////////
// Filename: EntityFrameData.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/EntityFrameData.h"
#include "DiaGraphics/Frame/EntityFrameDataVisitor.h"
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		EntityFrameData::EntityFrameData()
		{
		}

		////////////////////////////////////////////////////////////
		void EntityFrameData::RequestDrawSprite(const SpriteDrawCommand& cmd)
		{
			if (mSprites.IsFull())
			{
				DIA_LOG_WARNING("Graphics", "EntityFrameData: sprite budget exceeded (%u). Draw calls will be dropped.", mSprites.Capacity());
				return;
			}
			mSprites.Add(cmd);
		}

		////////////////////////////////////////////////////////////
		void EntityFrameData::Clear()
		{
			mSprites.RemoveAll();
		}

		////////////////////////////////////////////////////////////
		const Core::Containers::DynamicArrayC<SpriteDrawCommand, 256>& EntityFrameData::GetSprites() const
		{
			return mSprites;
		}

		////////////////////////////////////////////////////////////
		void EntityFrameData::AcceptVisitor(const EntityFrameDataVisitor& visitor) const
		{
			visitor.Visit(*this);
		}
	}
}
