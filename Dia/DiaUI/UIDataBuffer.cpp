////////////////////////////////////////////////////////////////////////////////
// Filename: UIDataBuffer
////////////////////////////////////////////////////////////////////////////////
#include "DiaUI/UIDataBuffer.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Memory/Memory.h>

namespace Dia
{
	namespace UI
	{
		UIDataBuffer::UIDataBuffer()
			: mWidth(0)
			, mHeight(0)
			, mBufferSize(0)
			, mBuffer(nullptr)
			, mDeleteWhenDone(true)
		{}

		UIDataBuffer::UIDataBuffer(int width, int height, const unsigned char* data, int datasize)
		{
			
			Create(width, height, data, datasize);
		}

		UIDataBuffer::~UIDataBuffer()
		{
			Destroy();
		}

		UIDataBuffer::UIDataBuffer(const UIDataBuffer& rhs)
			: mWidth(0)
			, mHeight(0)
			, mBufferSize(0)
			, mBuffer(nullptr)
			, mDeleteWhenDone(true)
		{
			// Only copy if this is already been set
			if (rhs.mBuffer != nullptr)
				Create(rhs.GetWidth(), rhs.GetHeight(), rhs.GetBuffer(), rhs.GetBufferSize());
		}

		UIDataBuffer& UIDataBuffer::operator=(const UIDataBuffer& rhs)
		{
			mWidth = 0;
			mHeight = 0;
			mBufferSize = 0;
			mBuffer = nullptr;
			mDeleteWhenDone = true;
			// Only copy if this is already been set
			if (rhs.mBuffer != nullptr)
				Create(rhs.GetWidth(), rhs.GetHeight(), rhs.GetBuffer(), rhs.GetBufferSize());
			
			return *this;
		}

		void UIDataBuffer::Create(int width, int height, const unsigned char* data, int datasize)
		{
			DIA_ASSERT(mWidth > 0, "Buffer width is set to zero");
			DIA_ASSERT(mHeight > 0, "Buffer height is set to zero");
			DIA_ASSERT(data != nullptr, "Buffer data is null");

			mWidth = width;
			mHeight = height;
			mBufferSize = datasize;
			mBuffer = DIA_NEW_ARRAY(datasize, unsigned char);
			Dia::Core::MemoryCopy(static_cast<void*>(mBuffer), data, datasize);
			mDeleteWhenDone = true;
		}

		void UIDataBuffer::CreateFromPreallocatedBuffer(int width, int height, unsigned char* data, int datasize, bool deleteWhenDone)
		{
			DIA_ASSERT(mWidth > 0, "Buffer width is set to zero");
			DIA_ASSERT(mHeight > 0, "Buffer height is set to zero");
			DIA_ASSERT(data != nullptr, "Buffer data is null");

			mWidth = width;
			mHeight = height;
			mBufferSize = datasize;
			mBuffer = data;
			Dia::Core::MemoryCopy(static_cast<void*>(mBuffer), data, datasize);

			mDeleteWhenDone = deleteWhenDone;
		}

		void UIDataBuffer::Destroy()
		{
			if (mBuffer != nullptr && mDeleteWhenDone)
			{
				DIA_DELETE_ARRAY(mBuffer);
			}

			mWidth = 0;
			mHeight = 0;
			mBufferSize = 0;
		}

		int UIDataBuffer::GetWidth()const { return mWidth; }
		int UIDataBuffer::GetHeight()const { return mHeight; }
		int UIDataBuffer::GetBufferSize()const { return mBufferSize;  }
		const unsigned char* UIDataBuffer::GetBuffer()const { return mBuffer; }
	}
}