////////////////////////////////////////////////////////////////////////////////
// Filename: ITexture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		class ITexture
		{
		public:
			enum class State : unsigned char
			{
				Pending = 0,
				Ready   = 1,
				Failed  = 2
			};

			virtual ~ITexture() {}

			virtual Dia::Core::StringCRC GetAssetId() const = 0;
			virtual Maths::Vector2D     GetSize()    const = 0;
			virtual State               GetState()   const = 0;

			bool IsReady() const { return GetState() == State::Ready; }

		protected:
			ITexture() {}

		private:
			ITexture(const ITexture&);
			ITexture& operator=(const ITexture&);
		};
	}
}
