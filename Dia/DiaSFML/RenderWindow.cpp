////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/RenderWindow.h"

#include "DiaSFML/Conversion.h"
#include "DiaSFML/DebugFrameRendererVisitor.h"

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Strings/stringutils.h>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#pragma warning( disable : 4800 )

namespace Dia
{
	namespace SFML
	{
		//-------------------------------------------------------------------------------------
		RenderWindow::RenderWindow()
			: mWindowContext(nullptr)
			, mBackBuffer(nullptr)
			, mUIShader(nullptr)
			, mUIOverlayTexture(nullptr)
		{}

		//-------------------------------------------------------------------------------------
		RenderWindow::RenderWindow(const Window::IWindow::Settings& windowSetting, const Graphics::ICanvas::Settings& canvasSettings)
		{
			// Extract the SF settings for the window
			wchar_t titleBuffer[1024];
			Dia::Core::StringToWString(windowSetting.GetTitle().AsCStr(), &titleBuffer[0]);
			std::wstring titleTempWString(&titleBuffer[0]);
		
			unsigned int style = windowSetting.GetStyle().GetAllBits();

			sf::VideoMode videoMode(windowSetting.GetDimensions().GetWidth(), windowSetting.GetDimensions().GetHeight(), windowSetting.GetDimensions().GetBitsPerPixel());
			sf::ContextSettings context(canvasSettings.GetDepth(), canvasSettings.GetStencil(), canvasSettings.GetAntialiasing(), canvasSettings.GetOpenGLMajor(), canvasSettings.GetOpenGLMinor());

			mWindowContext = DIA_NEW(sf::RenderWindow(videoMode, sf::String(titleTempWString), style, context));
			mBackBuffer = DIA_NEW(sf::RenderTexture());

			bool isBackBufferCreated = mBackBuffer->create(windowSetting.GetDimensions().GetWidth(), windowSetting.GetDimensions().GetHeight());
			
			DIA_ASSERT(isBackBufferCreated, "Rendering backbuffer is not allocated");
			DIA_ASSERT(sf::Shader::isAvailable(), "Shaders are not available on this platform");

			mUIShader = DIA_NEW(sf::Shader());
			mUIOverlayTexture = DIA_NEW(sf::Texture());
			
			//TODO: Replace with a better file load system
			//TODO: Move this shader to a centralized place
			bool isLoadedUIShader = mUIShader->loadFromFile("ui.frag", sf::Shader::Fragment); 
			DIA_ASSERT(isLoadedUIShader, "Could not load ui.frag");

			bool isTextureCreatedCorrectly = mUIOverlayTexture->create(mWindowContext->getSize().x, mWindowContext->getSize().y);
			DIA_ASSERT(isTextureCreatedCorrectly, "Could not create ui texture");

			mUIShader->setParameter("uiOverlayTex", *mUIOverlayTexture);
			mUIShader->setParameter("backBufferTex", mBackBuffer->getTexture());

			InputSource::SetWindowContext(mWindowContext);
		}

		//-------------------------------------------------------------------------------------
		RenderWindow::~RenderWindow()
		{
			DIA_ASSERT(mWindowContext, "Window is NULL");	
			DIA_DELETE(mWindowContext);

			DIA_ASSERT(mBackBuffer, "mBackBuffer is NULL");
			DIA_DELETE(mBackBuffer);

			DIA_ASSERT(mUIShader, "mUIShader is NULL");
			DIA_DELETE(mUIShader);

			DIA_ASSERT(mUIOverlayTexture, "mUIOverlayTexture is NULL");
			DIA_DELETE(mUIOverlayTexture);
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::Initialize(const Graphics::ICanvas::Settings& settings)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->setVerticalSyncEnabled(static_cast<bool>(settings.GetEnableVerticalSync()));

				// Make it the active window for OpenGL calls
				mWindowContext->setActive();

				// TODO: Allow this to be set from our settings not hardcoded

				// Enable Z-buffer read and write
				glEnable(GL_DEPTH_TEST);
				glDepthMask(GL_TRUE);
				glClearDepth(1.f);

				// Disable lighting
				glDisable(GL_LIGHTING);

				// Configure the viewport (the same size as the window)
				glViewport(0, 0, mWindowContext->getSize().x, mWindowContext->getSize().y);

				// Setup a perspective projection`
				glMatrixMode(GL_PROJECTION);
				glLoadIdentity();
				GLfloat ratio = static_cast<float>(mWindowContext->getSize().x) / mWindowContext->getSize().y;
				glFrustum(-ratio, ratio, -1.f, 1.f, 1.f, 500.f);
			}
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetCanvasSize(const Dia::Maths::Vector2D& size)
		{
			glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetActiveContext(bool active)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->setActive(false);
			}
		}

		//-------------------------------------------------------------------------------------
		// Get the frame ready for rendering
		void RenderWindow::StartFrame(const Dia::Graphics::FrameData& nextFrame)
		{
			// clear the buffers
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			mWindowContext->clear();
		}

		//-------------------------------------------------------------------------------------
		// Handle a frame buffer
		void RenderWindow::ProcessFrame(const Dia::Graphics::FrameData& nextFrame)
		{
			DIA_ASSERT(mBackBuffer, "mBackBuffer is NULL");

			if (mBackBuffer)
			{			
				DebugFrameRendererVisitor renderDebugObjects(mBackBuffer);
				
				renderDebugObjects.Visit(nextFrame);	
			}
		}

		//-------------------------------------------------------------------------------------
		// Finish the frame
		void RenderWindow::EndFrame(const Dia::Graphics::FrameData& nextFrame)
		{			
			DIA_ASSERT(mUIShader, "mUIShader is NULL");
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				// Push the ui overlay texture to a sprite for rendering
				sf::Sprite uiSprite;
				uiSprite.setTexture(*mUIOverlayTexture);

				mUIOverlayTexture->update(nextFrame.GetUIData().GetBuffer());	

				mWindowContext->pushGLStates();
				mWindowContext->draw(uiSprite, mUIShader);
				mWindowContext->popGLStates();

				mBackBuffer->clear();

				// end the current frame (internally swaps the front and back buffers)
				mWindowContext->display();
			}
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::Initialize(const Window::IWindow::Settings& settings)
		{}

		//-------------------------------------------------------------------------------------
		void RenderWindow::Close()
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->close();
			}
		}

		//-------------------------------------------------------------------------------------
		bool RenderWindow::IsOpen() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				return mWindowContext->isOpen();
			}

			return false;
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D RenderWindow::GetPosition() const
		{
			Maths::Vector2D result = Maths::Vector2D::Zero();

			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				Dia::SFML::Convert(result, mWindowContext->getPosition());
			}

			return result;
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetPosition(const Maths::Vector2D& position)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				sf::Vector2i sfPosition;

				Dia::SFML::Convert(sfPosition, position);

				mWindowContext->setPosition(sfPosition);
			}
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D RenderWindow::GetSize() const
		{
			Maths::Vector2D result = Maths::Vector2D::Zero();

			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				Dia::SFML::Convert(result, mWindowContext->getSize());
			}

			return result;
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetSize(const Maths::Vector2D& size)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				sf::Vector2u sfPosition;

				Dia::SFML::Convert(sfPosition, size);

				mWindowContext->setSize(sfPosition);
			}
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetTitle(const Core::Containers::String64& title)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				sf::String str(title.AsCStr());

				mWindowContext->setTitle(str);
			}
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->setIcon(width, height, pixels);
			}
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetVisible(bool visible)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->setVisible(visible);
			}
		}

		//-------------------------------------------------------------------------------------
		bool RenderWindow::SetActive(bool active) const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				return mWindowContext->setActive(active);
			}

			return false;
		}

		//-------------------------------------------------------------------------------------
		void RenderWindow::SetMouseCursorVisible(bool visible)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			if (mWindowContext)
			{
				mWindowContext->setMouseCursorVisible(visible);
			}
		}

		//-------------------------------------------------------------------------------------
		Window::SystemHandle RenderWindow::GetSystemHandle() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");

			return mWindowContext->getSystemHandle();
		}
	}
}