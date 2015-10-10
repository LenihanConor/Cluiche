////////////////////////////////////////////////////////////////////////////////
// Filename: RenderWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/IWindow.h>

#include "DiaSFML/InputSource.h"

// Forward Declare
namespace sf
{
	class RenderWindow;
	class RenderTexture;
	class Shader;
	class Sprite;
	class Texture;
	class Context;
}

namespace Dia
{
	namespace SFML
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: RenderWindow
		////////////////////////////////////////////////////////////////////////////////
		class RenderWindow : public Graphics::ICanvas, public Window::IWindow, public InputSource
		{
		public:
			friend class RenderWindowFactory;

			virtual ~RenderWindow();

			// Inherited from ICanvas
			virtual void Initialize(const Graphics::ICanvas::Settings& settings)override;
			virtual void SetCanvasSize(const Dia::Maths::Vector2D& size)override;
			virtual void SetActiveContext(bool active)override;

			virtual void StartFrame(const Dia::Graphics::FrameData& nextFrame)override;
			virtual void ProcessFrame(const Dia::Graphics::FrameData& nextFrame)override;
			virtual void EndFrame(const Dia::Graphics::FrameData& nextFrame)override;

			// Inherited from IWindow
			virtual void Initialize(const Window::IWindow::Settings& settings) override;
			virtual void Close() override;
			virtual bool IsOpen() const override;
			virtual Maths::Vector2D GetPosition() const override;
			virtual void SetPosition(const Maths::Vector2D& position) override;
			virtual Maths::Vector2D GetSize() const override;
			virtual void SetSize(const Maths::Vector2D& size) override;
			virtual void SetTitle(const Core::Containers::String64& title) override;
			virtual void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) override;
			virtual void SetVisible(bool visible) override;
			virtual bool SetActive(bool active = true) const override;
			virtual void SetMouseCursorVisible(bool visible) override;

		private:
			RenderWindow();
			RenderWindow(const Window::IWindow::Settings& windowSetting, const Graphics::ICanvas::Settings& canvasSettings);

			sf::RenderWindow* mWindowContext;	// Window context used to render too
			sf::RenderTexture* mBackBuffer;		// Texture we rednder too that will be used to render to final window context
			
			// UI Variables
			sf::Shader* mUIShader;				// Shader used to merge the backbuffer and the UI sprite before pushing to window context
			sf::Texture* mUIOverlayTexture;		//TODO: Replace this with a DIA texture when i create one
		};
	}
}