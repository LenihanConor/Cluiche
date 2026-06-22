////////////////////////////////////////////////////////////////////////////////
// Filename: MaterialRegistry.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include <DiaCore/CRC/CRC.h>
#include <DiaObservation/Log/DiaLog.h>

namespace Dia
{
    namespace Bgfx3D
    {
        MaterialRegistry::MaterialRegistry()
        {
            mDefault.id               = Dia::Core::StringCRC("default");
            mDefault.program          = nullptr;
            mDefault.baseColourRGBA   = 0xFFFFFFFFu;
            mDefault.albedoTexture    = 0xFFFFu;
            mDefault.normalMapTexture = 0xFFFFu;
        }

        void MaterialRegistry::Register(const MaterialDescriptor& desc)
        {
            // Overwrite if ID already registered.
            for (unsigned int i = 0; i < mMaterials.Size(); ++i)
            {
                if (mMaterials[i].id == desc.id)
                {
                    mMaterials[i] = desc;
                    return;
                }
            }
            if (!mMaterials.IsFull())
            {
                mMaterials.Add(desc);
            }
            else
            {
                DIA_LOG_WARNING("DiaBgfx3D", "MaterialRegistry full (%u); dropping material '%s'",
                    kMaxMaterials, desc.id.AsChar());
            }
        }

        const MaterialDescriptor* MaterialRegistry::Resolve(Dia::Core::StringCRC id) const
        {
            for (unsigned int i = 0; i < mMaterials.Size(); ++i)
            {
                if (mMaterials[i].id == id)
                    return &mMaterials[i];
            }
            return nullptr;
        }

        const MaterialDescriptor* MaterialRegistry::Resolve(Dia::Core::CRC id) const
        {
            for (unsigned int i = 0; i < mMaterials.Size(); ++i)
            {
                if (mMaterials[i].id.Value() == id.Value())
                    return &mMaterials[i];
            }
            return nullptr;
        }

        const MaterialDescriptor& MaterialRegistry::GetDefault() const
        {
            return mDefault;
        }

    } // namespace Bgfx3D
} // namespace Dia
