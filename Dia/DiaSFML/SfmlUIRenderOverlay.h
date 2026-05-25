////////////////////////////////////////////////////////////////////////////////
// Filename: SfmlUIRenderOverlay.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IUIRenderOverlay.h>

#include <cstdint>
#include <vector>

namespace sf
{
	class RenderWindow;
	class RenderTexture;
	class Shader;
	class Texture;
}

namespace Dia
{
	namespace SFML
	{
		class SfmlUIRenderOverlay : public Dia::UI::IUIRenderOverlay
		{
		public:
			// windowContext and backBuffer are not owned; both must outlive this object.
			SfmlUIRenderOverlay(sf::RenderWindow* windowContext, sf::RenderTexture* backBuffer);
			~SfmlUIRenderOverlay() override;

			void OnCanvasSizeChanged(const Dia::Maths::Vector2D& size) override;
			void Composite(const Dia::UI::UIDataBuffer& buffer) override;

		private:
			sf::RenderWindow*         mWindowContext;    // not owned
			sf::RenderTexture*        mBackBuffer;       // not owned
			sf::Shader*               mUIShader;         // owned
			sf::Texture*              mUIOverlayTexture; // owned
			std::vector<uint8_t>      mClearPixels;      // cached zero buffer for clearing stale UI
		};
	}
}
