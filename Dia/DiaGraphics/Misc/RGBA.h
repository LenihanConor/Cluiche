////////////////////////////////////////////////////////////////////////////////
// Filename: RGBA.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/Type/TypeDeclarationMacros.h>

namespace Dia
{
	namespace Graphics
	{
		///
		/// Frame - A single framethat stores all the info that the render
		///
		class RGBA
		{
		public:
			DIA_TYPE_DECLARATION;

			////////////////////////////////////////////////////////////
			// Static member data
			////////////////////////////////////////////////////////////
			static const RGBA Black;       ///< Black predefined color
			static const RGBA White;       ///< White predefined color
			static const RGBA Red;         ///< Red predefined color
			static const RGBA Green;       ///< Green predefined color
			static const RGBA Blue;        ///< Blue predefined color
			static const RGBA Yellow;      ///< Yellow predefined color
			static const RGBA Magenta;     ///< Magenta predefined color
			static const RGBA Cyan;        ///< Cyan predefined color
			static const RGBA Transparent; ///< Transparent (black) predefined color

			RGBA();
			RGBA(const RGBA& rhs);
			RGBA(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha = 255);
			
			void SetR(unsigned char red);
			void SetG(unsigned char green);
			void SetB(unsigned char blue);
			void SetA(unsigned char alpha);

			unsigned char R()const { return r; }
			unsigned char G()const { return g; }
			unsigned char B()const { return b; }
			unsigned char A()const { return a; }

			RGBA& operator =(const RGBA& rhs);
			bool operator ==(const RGBA& rhs)const;
			bool operator !=(const RGBA& rhs)const;
			RGBA operator +(const RGBA& rhs)const;
			RGBA operator -(const RGBA& rhs)const;
			RGBA operator *(const RGBA& rhs)const;
			RGBA& operator +=(const RGBA& rhs);
			RGBA& operator -=(const RGBA& rhs);
			RGBA& operator *=(const RGBA& rhs);
			
		private:
			static const unsigned char kMin = 0;
			static const unsigned char kMax = 255;

			unsigned char r; ///< Red component
			unsigned char g; ///< Green component
			unsigned char b; ///< Blue component
			unsigned char a; ///< Alpha (opacity) component
		};
	}
}