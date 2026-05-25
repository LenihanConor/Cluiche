////////////////////////////////////////////////////////////////////////////////
// Filename: SfmlTexture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Assets/ITexture.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <atomic>

namespace sf
{
	class Image;
	class Texture;
}

namespace Dia
{
	namespace SFML
	{
		class SfmlTexture : public Dia::Graphics::ITexture
		{
		public:
			explicit SfmlTexture(Dia::Core::StringCRC assetId);
			~SfmlTexture() override;

			// ITexture
			Dia::Core::StringCRC    GetAssetId() const override { return mAssetId; }
			Dia::Maths::Vector2D    GetSize()    const override;
			State                   GetState()   const override { return mState.load(std::memory_order_acquire); }

			// Lifecycle controlled by TextureHandler::Tick() on the GL-context thread.
			bool UploadFromImage(const sf::Image& image);
			void MarkFailed(const char* reason);

			// Renderer-internal accessor — DiaSFML translation units only.
			const sf::Texture* GetSfTexture() const { return mTexture; }

		private:
			Dia::Core::StringCRC     mAssetId;
			sf::Texture*             mTexture;
			std::atomic<State>       mState;
		};
	}
}
