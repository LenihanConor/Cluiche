//////////////////
#pragma once

#include <DiaCore/Core/System.h>

namespace Dia
{
	namespace UI
	{
		class UIDataBuffer
		{
		public:
			UIDataBuffer();
			UIDataBuffer(int width, int height, const unsigned char* data, int datasize);
			UIDataBuffer(const UIDataBuffer&);
			UIDataBuffer& operator=(const UIDataBuffer&);
			~UIDataBuffer();
			
			void CreateFromPreallocatedBuffer(int width, int height, unsigned char* data, int datasize, bool deleteWhenDone = true);
			void Create(int width, int height, const unsigned char* data, int datasize);
			void Destroy();

			int GetWidth()const;
			int GetHeight()const;
			int GetBufferSize()const;
			const unsigned char* GetBuffer()const;

		private:			
			int mWidth;
			int mHeight;
			int mBufferSize;
			unsigned char* mBuffer;
			bool mDeleteWhenDone;
		};
	}
}