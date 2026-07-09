////////////////////////////////////////////////////////////////////////////////
// Filename: MaterialRegistry.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/CRC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Bgfx { class ShaderProgram; } }

namespace Dia
{
    namespace Bgfx3D
    {
        struct MaterialDescriptor
        {
            Dia::Core::StringCRC         id;
            Dia::Bgfx::ShaderProgram*    program          = nullptr;       // not owned; lives in Canvas3D
            uint32_t                     baseColourRGBA   = 0xFFFFFFFFu;  // 0xFFFFFFFF default
            unsigned short               albedoTexture    = 0xFFFF;       // bgfx::TextureHandle::idx; 0xFFFF = none
            unsigned short               normalMapTexture = 0xFFFF;       // bgfx::TextureHandle::idx; 0xFFFF = none
            unsigned short               ormTexture       = 0xFFFF;       // bgfx::TextureHandle::idx; 0xFFFF = none (R=occlusion, G=roughness, B=metallic)
            float                        metallic         = 0.0f;         // fallback scalar when ormTexture is 0xFFFF
            float                        roughness        = 0.5f;         // fallback scalar when ormTexture is 0xFFFF
        };

        // Maps StringCRC material IDs to shader programs and base colours.
        // Backed by DynamicArrayC — no STL in public surface (PD-004, BG3-008).
        class MaterialRegistry
        {
        public:
            static constexpr unsigned int kMaxMaterials = 256;

            MaterialRegistry();

            void                        Register(const MaterialDescriptor& desc);
            const MaterialDescriptor*   Resolve(Dia::Core::StringCRC id) const;  // nullptr if not found
            const MaterialDescriptor*   Resolve(Dia::Core::CRC id)       const;  // for Submesh::materialId (CRC, not StringCRC)
            const MaterialDescriptor&   GetDefault() const;

        private:
            Dia::Core::Containers::DynamicArrayC<MaterialDescriptor, kMaxMaterials> mMaterials;
            MaterialDescriptor                                                       mDefault;
        };

    } // namespace Bgfx3D
} // namespace Dia
