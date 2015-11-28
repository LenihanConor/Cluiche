////////////////////////////////////////////////////////////////////////////////
// Filename: IWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Strings/String64.h>
#include <DiaCore/Containers/BitFlag/BitArray8.h>
#include <DiaCore/Core/EnumClass.h>

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaWindow/SystemHandle.h>

namespace Dia
{

	namespace Window
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: IWindow
		////////////////////////////////////////////////////////////////////////////////
		class IWindow 
		{
		public:
			class Settings
			{
			public:
				////////////////////////////////////////////////////////////////////////////////
				// Class name: IWindow::Settings::Dimensions: Size of the Canvas
				////////////////////////////////////////////////////////////////////////////////
				class Dimensions
				{
				public:
					Dimensions(unsigned int width, unsigned int height, unsigned int bitsPerPixel = 32)
						: mWidth(width)
						, mHeight(height)
						, mBitsPerPixel(bitsPerPixel)
					{}

					unsigned int GetWidth()const { return mWidth; }
					unsigned int GetHeight()const { return mHeight; }
					unsigned int GetBitsPerPixel()const { return mBitsPerPixel; }

				private:
					unsigned int mWidth;			// Width of the window
					unsigned int mHeight;			// Height of the window
					unsigned int mBitsPerPixel;		// Bits per pixel of the window
				};

				////////////////////////////////////////////////////////////////////////////////
				// Class name: IWindow::Settings::Style: Size of the Canvas
				////////////////////////////////////////////////////////////////////////////////
				class Style : public Core::BitArray8
				{
				public:
					CLASSEDENUM(EStyle, \
						CE_ITEMVAL(kTitlebar, 0)\
						CE_ITEM(kResize)\
						CE_ITEM(kClose)\
						CE_ITEM(kFullscreen)\
						, kTitlebar \
						);

					Style()
						: Core::BitArray8()
					{
						SetBit(EStyle::kTitlebar, true);
						SetBit(EStyle::kClose, true);
					}

					Style(const unsigned char value)
						: Core::BitArray8(value)
					{}
				};

				Settings(const Core::Containers::String64& title, const Dimensions& dimensions, const Style& style)
					: mTitle(title)
					, mDimensions(dimensions)
					, mStyle(style)
				{}

				const Core::Containers::String64& GetTitle()const { return mTitle; }
				const Dimensions& GetDimensions()const { return mDimensions; }
				const Style& GetStyle()const { return mStyle; }

			private:
				Core::Containers::String64 mTitle;			// Name of the title displayed in the title bar
				Dimensions mDimensions;						// Size fo the window
				Style mStyle;								// Look of the window
			};

			virtual ~IWindow(){};
			////////////////////////////////////////////////////////////
			///  Called after creation
			virtual void Initialize(const Settings& settings) = 0;

			////////////////////////////////////////////////////////////
			///  Close the window and destroy all the attached resources
			virtual void Close() = 0;

			////////////////////////////////////////////////////////////
			/// Tell whether or not the window is open
			virtual bool IsOpen() const = 0;

			////////////////////////////////////////////////////////////
			/// Get the position of the window
			virtual Maths::Vector2D GetPosition() const = 0;

			////////////////////////////////////////////////////////////
			/// Change the position of the window on screen
			virtual void SetPosition(const Maths::Vector2D& position) = 0;

			////////////////////////////////////////////////////////////
			/// Get the size of the rendering region of the window
			virtual Maths::Vector2D GetSize() const = 0;

			////////////////////////////////////////////////////////////
			///  Change the size of the rendering region of the window
			virtual void SetSize(const Maths::Vector2D& size) = 0;

			////////////////////////////////////////////////////////////
			/// Change the title of the window
			virtual void SetTitle(const Core::Containers::String64& title) = 0;

			////////////////////////////////////////////////////////////
			/// Change the window's icon
			virtual void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) = 0;

			////////////////////////////////////////////////////////////
			/// Show or hide the window
			virtual void SetVisible(bool visible) = 0;
	
			////////////////////////////////////////////////////////////
			/// Activate or deactivate the window as the current target
			///        for OpenGL rendering
			virtual bool SetActive(bool active = true) const = 0;

			////////////////////////////////////////////////////////////
			/// Show or hide the mouse cursor
			virtual void SetMouseCursorVisible(bool visible) = 0;

			////////////////////////////////////////////////////////////
			///  Get the OS-specific handle of the window
			///
			virtual SystemHandle GetSystemHandle() const = 0;
		};
	}
}