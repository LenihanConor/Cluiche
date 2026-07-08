////////////////////////////////////////////////////////////////////////////////
// Filename: IObjectRenderer.h
// Description: Type-erased renderer interface for fixed-topology debug objects.
//              Game code subclasses TypedObjectRenderer<T> — never writes const void*.
// Feature spec: docs/specs/features/dia/diavisualdebugger/fixed-draw-layer.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaCore/Core/Assert.h>

namespace Dia
{
    namespace Debug
    {
        class IFixedPrimitiveBuffer;

        ////////////////////////////////////////////////////////////////////////
        // IObjectRenderer
        //
        // Called once at RegisterFixed() and once per InvalidateFixed() to fill
        // a FixedPrimitiveBuffer. The source object pointer is owned by the caller
        // and must remain valid for the lifetime of the registration.
        ////////////////////////////////////////////////////////////////////////
        class IObjectRenderer
        {
        public:
            virtual ~IObjectRenderer() = default;
            virtual void BuildPrimitives(const void* sourceObject,
                                         IFixedPrimitiveBuffer& out) const = 0;
        };

        ////////////////////////////////////////////////////////////////////////
        // TypedObjectRenderer<T>
        //
        // Public-facing base class. Game code overrides DoBuild(const T&, ...).
        // The const void* cast is isolated here and final prevents accidental
        // override of BuildPrimitives.
        ////////////////////////////////////////////////////////////////////////
        template<typename T>
        class TypedObjectRenderer : public IObjectRenderer
        {
        public:
            void BuildPrimitives(const void* src,
                                  IFixedPrimitiveBuffer& out) const override final
            {
                DoBuild(*static_cast<const T*>(src), out);
            }

        protected:
            virtual void DoBuild(const T& object, IFixedPrimitiveBuffer& out) const = 0;
        };

    } // namespace Debug
} // namespace Dia

#endif // DIA_DEBUG
