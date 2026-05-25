////////////////////////////////////////////////////////////////////////////////
// Filename: SfmlTexture.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/SfmlTexture.h"

#include <DiaCore/Memory/Memory.h>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace Dia
{
	namespace SFML
	{
		SfmlTexture::SfmlTexture(Dia::Core::StringCRC assetId)
			: mAssetId(assetId)
			, mTexture(nullptr)
			, mState(State::Pending)
		{}

		SfmlTexture::~SfmlTexture()
		{
			DIA_DELETE(mTexture);
		}

		Dia::Maths::Vector2D SfmlTexture::GetSize() const
		{
			if (!mTexture || GetState() != State::Ready)
				return Dia::Maths::Vector2D(0.0f, 0.0f);
			const sf::Vector2u sz = mTexture->getSize();
			return Dia::Maths::Vector2D(static_cast<float>(sz.x), static_cast<float>(sz.y));
		}

		bool SfmlTexture::UploadFromImage(const sf::Image& image)
		{
			sf::Texture* tex = DIA_NEW(sf::Texture());
			if (!tex->loadFromImage(image))
			{
				DIA_DELETE(tex);
				mState.store(State::Failed, std::memory_order_release);
				return false;
			}
			mTexture = tex;
			mState.store(State::Ready, std::memory_order_release);
			return true;
		}

		void SfmlTexture::MarkFailed(const char* /*reason*/)
		{
			mState.store(State::Failed, std::memory_order_release);
		}
	}
}
