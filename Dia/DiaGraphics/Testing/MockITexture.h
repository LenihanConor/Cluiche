////////////////////////////////////////////////////////////////////////////////
// Filename: MockITexture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Assets/ITexture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		namespace Testing
		{
			class MockITexture : public ITexture
			{
			public:
				MockITexture()
					: mAssetId(0)
					, mSize(1.0f, 1.0f)
					, mState(State::Ready)
				{}

				MockITexture(Dia::Core::StringCRC assetId, Maths::Vector2D size = Maths::Vector2D(1.0f, 1.0f), State state = State::Ready)
					: mAssetId(assetId)
					, mSize(size)
					, mState(state)
				{}

				Dia::Core::StringCRC GetAssetId() const override { return mAssetId; }
				Maths::Vector2D     GetSize()    const override { return mSize; }
				State               GetState()   const override { return mState; }

				void SetState(State s) { mState = s; }

			private:
				Dia::Core::StringCRC mAssetId;
				Maths::Vector2D     mSize;
				State               mState;
			};
		}
	}
}
