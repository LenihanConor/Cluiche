////////////////////////////////////////////////////////////////////////////////
// Filename: SfmlUIRenderOverlay.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/SfmlUIRenderOverlay.h"

#include <DiaUI/UIDataBuffer.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/FilePath/FilePath.h>
#include <DiaCore/Core/Log.h>

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>

namespace Dia
{
	namespace SFML
	{
		SfmlUIRenderOverlay::SfmlUIRenderOverlay(sf::RenderWindow* windowContext, sf::RenderTexture* backBuffer)
			: mWindowContext(windowContext)
			, mBackBuffer(backBuffer)
			, mUIShader(nullptr)
			, mUIOverlayTexture(nullptr)
		{
			DIA_ASSERT(mWindowContext != nullptr, "SfmlUIRenderOverlay: windowContext is NULL");
			DIA_ASSERT(mBackBuffer != nullptr, "SfmlUIRenderOverlay: backBuffer is NULL");
			DIA_ASSERT(sf::Shader::isAvailable(), "SfmlUIRenderOverlay: shaders not available on this platform");

			mUIShader = DIA_NEW(sf::Shader());
			const sf::Vector2u windowSize = mWindowContext->getSize();
			mUIOverlayTexture = DIA_NEW(sf::Texture(windowSize));
			DIA_ASSERT(mUIOverlayTexture != nullptr, "Could not create UI overlay texture");
			mClearPixels.assign(windowSize.x * windowSize.y * 4, 0);

			Dia::Core::FilePath uiShaderFile("root", "global/Presentation/", "ui.frag");
			Dia::Core::FilePath::ResoledFilePath resolvedUIShaderFile;
			uiShaderFile.Resolve(resolvedUIShaderFile);

			bool isLoaded = mUIShader->loadFromFile(resolvedUIShaderFile.AsCStr(), sf::Shader::Type::Fragment);
			DIA_ASSERT(isLoaded, "Could not load ui.frag from %s", resolvedUIShaderFile.AsCStr());
			if (!isLoaded)
				Dia::Core::Log::OutputVaradicLine("[ERROR][Graphics] Failed to load shader: %s", resolvedUIShaderFile.AsCStr());

			mUIShader->setUniform("uiOverlayTex", *mUIOverlayTexture);
			mUIShader->setUniform("backBufferTex", mBackBuffer->getTexture());
		}

		SfmlUIRenderOverlay::~SfmlUIRenderOverlay()
		{
			DIA_ASSERT(mUIShader, "mUIShader is NULL");
			DIA_DELETE(mUIShader);

			DIA_ASSERT(mUIOverlayTexture, "mUIOverlayTexture is NULL");
			DIA_DELETE(mUIOverlayTexture);
		}

		void SfmlUIRenderOverlay::OnCanvasSizeChanged(const Dia::Maths::Vector2D& size)
		{
			// Reallocate overlay texture to match new canvas size.
			DIA_DELETE(mUIOverlayTexture);
			const sf::Vector2u newSize{
				static_cast<unsigned int>(size.x),
				static_cast<unsigned int>(size.y)
			};
			mUIOverlayTexture = DIA_NEW(sf::Texture(newSize));
			mClearPixels.assign(newSize.x * newSize.y * 4, 0);

			mUIShader->setUniform("uiOverlayTex", *mUIOverlayTexture);
			mUIShader->setUniform("backBufferTex", mBackBuffer->getTexture());
		}

		void SfmlUIRenderOverlay::Composite(const Dia::UI::UIDataBuffer& buffer)
		{
			DIA_ASSERT(mUIShader, "mUIShader is NULL");
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			sf::Sprite uiSprite(*mUIOverlayTexture);

			if (buffer.GetBufferSize() > 0)
			{
				mUIOverlayTexture->update(buffer.GetBuffer());

				// TODO This should be part of a debug enum
				// TODO Hide this behind a real save file
				static bool debugUIRendertexture = false;
				if (debugUIRendertexture)
					bool isSuccessful = mUIOverlayTexture->copyToImage().saveToFile("debugUIRender.png");
			}
			else
			{
				// No UI active — clear stale texture so previous stage UI doesn't bleed through.
				mUIOverlayTexture->update(mClearPixels.data());
			}

			mWindowContext->pushGLStates();
			mWindowContext->draw(uiSprite, mUIShader);
			mWindowContext->popGLStates();
		}
	}
}
