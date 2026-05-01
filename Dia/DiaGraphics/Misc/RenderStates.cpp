////////////////////////////////////////////////////////////////////////////////
// Filename: RenderStates.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Misc/RenderStates.h"

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		const RenderStates RenderStates::Default;

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates()
			: blendMode(BlendMode::Alpha)
			, transform()
			, texture(nullptr)
			, shader(nullptr)
		{
		}

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates(BlendMode theBlendMode)
			: blendMode(theBlendMode)
			, transform()
			, texture(nullptr)
			, shader(nullptr)
		{
		}

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates(const Transform& theTransform)
			: blendMode(BlendMode::Alpha)
			, transform(theTransform)
			, texture(nullptr)
			, shader(nullptr)
		{
		}

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates(const ITexture* theTexture)
			: blendMode(BlendMode::Alpha)
			, transform()
			, texture(theTexture)
			, shader(nullptr)
		{
		}

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates(const IShader* theShader)
			: blendMode(BlendMode::Alpha)
			, transform()
			, texture(nullptr)
			, shader(theShader)
		{
		}

		////////////////////////////////////////////////////////////
		RenderStates::RenderStates(BlendMode theBlendMode, const Transform& theTransform,
								   const ITexture* theTexture, const IShader* theShader)
			: blendMode(theBlendMode)
			, transform(theTransform)
			, texture(theTexture)
			, shader(theShader)
		{
		}
	}
}
