////////////////////////////////////////////////////////////////////////////////
// Filename: MeshRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

namespace Dia { namespace Graphics3D { class Mesh3DFrameData; struct Mesh3DDrawCommand; } }
namespace Dia { namespace Mesh3D { class Mesh3DAssetHandler; } }

namespace Dia
{
    namespace Bgfx3D
    {
        class MeshGpuCache;
        class MaterialRegistry;

        class MeshRenderer
        {
        public:
            MeshRenderer(unsigned short viewId, MeshGpuCache* cache,
                         MaterialRegistry* materials,
                         Dia::Mesh3D::Mesh3DAssetHandler* meshHandler);

            void Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData);

        private:
            void DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd);

            unsigned short                   mViewId;
            MeshGpuCache*                    mCache;
            MaterialRegistry*                mMaterials;
            Dia::Mesh3D::Mesh3DAssetHandler* mMeshHandler;
        };

    } // namespace Bgfx3D
} // namespace Dia
