////////////////////////////////////////////////////////////////////////////////
// Filename: RenderStates.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaGraphics/Misc/Transform.h"

namespace Dia
{
	namespace Graphics
	{
		class ITexture;
		class IShader;

		////////////////////////////////////////////////////////////
		/// \brief Define the states used for drawing to a render target
		///
		/// There are four global states that can be applied to
		/// the drawn objects:
		///
		/// \li the blend mode: how pixels of the object are blended with the background
		/// \li the transform: how the object is positioned/rotated/scaled
		/// \li the texture: what image is mapped to the object
		/// \li the shader: what custom effect is applied to the object
		///
		/// High-level objects such as sprites or text force some of
		/// these states. For example, a sprite will set its own texture,
		/// so that you don't have to care about it when drawing the sprite.
		///
		/// The transform is a special case: sprites, texts and shapes
		/// (and it's a good idea to do it with your own drawable classes
		/// too) combine their transform with the one that is passed in the
		/// RenderStates structure. So that you can use a "global" transform
		/// on top of each object's transform.
		///
		/// Most objects, especially high-level drawables, can be drawn
		/// directly without defining render states explicitly -- the default
		/// set of states is OK in most cases.
		/// \code
		/// window.Draw(sprite);
		/// \endcode
		///
		/// If you want to use a single specific render state,
		/// for example a shader, you can pass it directly to the Draw
		/// function: SFML will construct a RenderStates on the fly.
		/// \code
		/// window.Draw(sprite, shader);
		/// \endcode
		///
		/// When you're inside the Draw function of a drawable
		/// object (inherited from IDrawable), you can
		/// either pass the render states unmodified, or change
		/// some of them.
		/// For example, a transformable object will combine the
		/// current transform with its own transform. A sprite will
		/// set its texture. Etc.
		////////////////////////////////////////////////////////////
		class RenderStates
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Enumeration of the blending modes
			///
			/// The blending mode determines how the colors of the source
			/// are combined with the destination colors.
			////////////////////////////////////////////////////////////
			enum class BlendMode
			{
				Alpha,      ///< Blend source and dest according to dest alpha
				Add,        ///< Add source to dest
				Multiply,   ///< Multiply source and dest
				None        ///< Overwrite dest with source
			};

			////////////////////////////////////////////////////////////
			/// \brief Default constructor
			///
			/// Constructing a default set of render states is equivalent
			/// to using RenderStates::Default.
			/// The default set defines:
			/// \li the BlendAlpha blend mode
			/// \li the identity transform
			/// \li a null texture
			/// \li a null shader
			////////////////////////////////////////////////////////////
			RenderStates();

			////////////////////////////////////////////////////////////
			/// \brief Construct a default set of render states with a custom blend mode
			///
			/// \param theBlendMode Blend mode to use
			////////////////////////////////////////////////////////////
			explicit RenderStates(BlendMode theBlendMode);

			////////////////////////////////////////////////////////////
			/// \brief Construct a default set of render states with a custom transform
			///
			/// \param theTransform Transform to use
			////////////////////////////////////////////////////////////
			explicit RenderStates(const Transform& theTransform);

			////////////////////////////////////////////////////////////
			/// \brief Construct a default set of render states with a custom texture
			///
			/// \param theTexture Texture to use
			////////////////////////////////////////////////////////////
			explicit RenderStates(const ITexture* theTexture);

			////////////////////////////////////////////////////////////
			/// \brief Construct a default set of render states with a custom shader
			///
			/// \param theShader Shader to use
			////////////////////////////////////////////////////////////
			explicit RenderStates(const IShader* theShader);

			////////////////////////////////////////////////////////////
			/// \brief Construct a set of render states with all its attributes
			///
			/// \param theBlendMode Blend mode to use
			/// \param theTransform Transform to use
			/// \param theTexture   Texture to use
			/// \param theShader    Shader to use
			////////////////////////////////////////////////////////////
			RenderStates(BlendMode theBlendMode, const Transform& theTransform,
						 const ITexture* theTexture, const IShader* theShader);

			////////////////////////////////////////////////////////////
			// Static member data
			////////////////////////////////////////////////////////////
			static const RenderStates Default; ///< Special instance holding the default render states

			////////////////////////////////////////////////////////////
			// Member data
			////////////////////////////////////////////////////////////
			BlendMode       blendMode;  ///< Blending mode
			Transform       transform;  ///< Transform
			const ITexture* texture;    ///< Texture
			const IShader*  shader;     ///< Shader
		};
	}
}
