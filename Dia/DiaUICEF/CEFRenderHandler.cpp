////////////////////////////////////////////////////////////////////////////////
// Filename: CEFRenderHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFRenderHandler.h"
#include "CEFUtils.h"

#include <cstring>

namespace Dia
{
	namespace UICEF
	{
		CEFRenderHandler::CEFRenderHandler(int width, int height)
			: mWidth(width)
			, mHeight(height)
			, mIsDirty(false)
			, mPopupVisible(false)
		{
			memset(mPixelBuffer, 0, sBufferSize);
		}

		CEFRenderHandler::~CEFRenderHandler()
		{
		}

		void CEFRenderHandler::GetViewRect(CefRefPtr<CefBrowser> /*browser*/, CefRect& rect)
		{
			rect = CefRect(0, 0, mWidth, mHeight);
		}

		void CEFRenderHandler::OnPaint(CefRefPtr<CefBrowser> /*browser*/, PaintElementType type,
			const RectList& dirtyRects, const void* buffer, int width, int height)
		{
			std::lock_guard<std::mutex> lock(mBufferMutex);

			int bufferSize = width * height * 4;
			if (bufferSize > sBufferSize)
				return;

			if (type == PET_VIEW)
			{
				memcpy(mPixelBuffer, buffer, bufferSize);

				for (size_t i = 0; i < dirtyRects.size(); ++i)
				{
					const CefRect& rect = dirtyRects[i];
					Utils::ConvertBGRAtoRGBA(mPixelBuffer, width, height,
						rect.x, rect.y, rect.width, rect.height);
				}

				mWidth = width;
				mHeight = height;
			}
			else if (type == PET_POPUP && mPopupVisible)
			{
				const unsigned char* popupBuffer = static_cast<const unsigned char*>(buffer);
				for (int y = 0; y < height && (y + mPopupRect.y) < mHeight; ++y)
				{
					int destY = y + mPopupRect.y;
					if (destY < 0) continue;
					for (int x = 0; x < width && (x + mPopupRect.x) < mWidth; ++x)
					{
						int destX = x + mPopupRect.x;
						if (destX < 0) continue;
						int srcOffset = (y * width + x) * 4;
						int dstOffset = (destY * mWidth + destX) * 4;
						// BGRA -> RGBA composite
						mPixelBuffer[dstOffset + 0] = popupBuffer[srcOffset + 2]; // R
						mPixelBuffer[dstOffset + 1] = popupBuffer[srcOffset + 1]; // G
						mPixelBuffer[dstOffset + 2] = popupBuffer[srcOffset + 0]; // B
						mPixelBuffer[dstOffset + 3] = 255; // A
					}
				}
			}

			mIsDirty = true;
		}

		void CEFRenderHandler::OnPopupShow(CefRefPtr<CefBrowser> /*browser*/, bool show)
		{
			mPopupVisible = show;
		}

		void CEFRenderHandler::OnPopupSize(CefRefPtr<CefBrowser> /*browser*/, const CefRect& rect)
		{
			mPopupRect = rect;
		}

		const unsigned char* CEFRenderHandler::GetPixelBuffer() const
		{
			return mPixelBuffer;
		}

		int CEFRenderHandler::GetBufferSize() const
		{
			return mWidth * mHeight * 4;
		}

		void CEFRenderHandler::Resize(int width, int height)
		{
			std::lock_guard<std::mutex> lock(mBufferMutex);
			mWidth = width;
			mHeight = height;
		}
	}
}
