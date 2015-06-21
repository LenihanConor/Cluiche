////////////////////////////////////////////////////////////////////////////////
// Filename: ICanvas.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaGraphics/Frame/FrameData.h"

#include <DiaCore/Core/EnumClass.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: ICanvas
		////////////////////////////////////////////////////////////////////////////////
		class ICanvas
		{
		public:
			class Settings
			{
			public:
				CLASSEDENUM(VSyncEnum, \
				CE_ITEMVAL(kEnable, static_cast<int>(true))\
				CE_ITEMVAL(kDisable, static_cast<int>(false))\
				, kEnable \
				);

				Settings(VSyncEnum vsync = VSyncEnum::kEnable, unsigned int depth = 0, unsigned int stencil = 0, unsigned int antialiasing = 0, unsigned int openGLMajor = 2, unsigned int openGLMinor = 0)
					: mDepth(depth)
					, mStencil(stencil)
					, mAntialiasing(antialiasing)
					, mOpenGLMajor(openGLMajor)
					, mOpenGLMinor(openGLMinor)
				{}

				unsigned int GetDepth()const { return mDepth; }
				unsigned int GetStencil()const { return mStencil; }
				unsigned int GetAntialiasing()const { return mAntialiasing; }
				unsigned int GetOpenGLMajor()const { return mOpenGLMajor; }
				unsigned int GetOpenGLMinor()const { return mOpenGLMinor; }
				VSyncEnum GetEnableVerticalSync()const { return mEnableVerticalSync; }

			private:
				unsigned int mDepth;			// Bits of the depth buffer
				unsigned int mStencil;			// Bits of the stencil buffer
				unsigned int mAntialiasing;		// Level of antialiasing
				unsigned int mOpenGLMajor;		// Major number of the context version to create
				unsigned int mOpenGLMinor;		// Minor number of the context version to create
				VSyncEnum mEnableVerticalSync;	// What is the inital value for vsync
			};

			ICanvas(){}
			virtual ~ICanvas(){};
			
			virtual void Initialize(const Settings& settings) = 0;
			virtual void SetCanvasSize(const Dia::Maths::Vector2D& size) = 0;
			virtual void SetActiveContext(bool active)=0;

			virtual void StartFrame(const FrameData& nextFrame) = 0;
			virtual void ProcessFrame(const FrameData& nextFrame) = 0;
			virtual void EndFrame(const FrameData& nextFrame) = 0;
		};
	}
}