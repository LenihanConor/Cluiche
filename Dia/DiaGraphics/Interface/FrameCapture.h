////////////////////////////////////////////////////////////////////////////////
// Filename: FrameCapture.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

namespace Dia
{
	namespace Graphics
	{
		struct FrameCaptureToken
		{
			unsigned int id;	// Encodes ring slot index (lower bits) + generation (upper bits). 0 = invalid.
			bool IsValid() const { return id != 0; }
		};

		struct FrameCaptureResult
		{
			enum class Status : unsigned char
			{
				kPending,		// Readback not yet complete
				kReady,			// Pixel data available in data pointer
				kFailed,		// Capture failed (bgfx error or ring overflow)
				kInvalidToken	// Token was never issued or ring slot was recycled
			};

			Status		status	= Status::kInvalidToken;
			const void*	data	= nullptr;	// RGBA8 pixel buffer; valid only when status == kReady
			unsigned int	width	= 0;
			unsigned int	height	= 0;
			unsigned int	pitch	= 0;		// Bytes per row
		};
	}
}
