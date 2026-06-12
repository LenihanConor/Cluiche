////////////////////////////////////////////////////////////////////////////////
// Filename: MeshRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaBgfx3D/Renderers/MeshRenderer.h"

#include <bgfx/bgfx.h>

#include "DiaBgfx3D/Resources/MeshGpuCache.h"
#include "DiaBgfx3D/Resources/MaterialRegistry.h"
#include <DiaBgfx/Resources/ShaderProgram.h>
#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaMaths/Matrix/Matrix44.h>

namespace Dia
{
    namespace Bgfx3D
    {
        MeshRenderer::MeshRenderer(unsigned short viewId, MeshGpuCache* cache,
                                   MaterialRegistry* materials,
                                   Dia::Mesh3D::Mesh3DAssetHandler* meshHandler)
            : mViewId(viewId)
            , mCache(cache)
            , mMaterials(materials)
            , mMeshHandler(meshHandler)
        {
        }

        void MeshRenderer::Draw(const Dia::Graphics3D::Mesh3DFrameData& frameData)
        {
            const auto& draws = frameData.GetMeshDraws();
            for (unsigned int i = 0; i < draws.Size(); ++i)
            {
                const auto& cmd = draws[i];
                if (cmd.skinningPaletteIndex != 0)
                    continue; // Skip skinned draws
                DrawCommand(cmd);
            }
        }

        void MeshRenderer::DrawCommand(const Dia::Graphics3D::Mesh3DDrawCommand& cmd)
        {
            // Look up the mesh asset
            Dia::Mesh3D::Mesh3DAsset* asset = mMeshHandler->LookupMesh(cmd.meshId);
            if (!asset || !asset->IsReady())
                return;

            // Get GPU buffers
            const GpuMesh* gpu = mCache->GetOrUpload(*asset);
            if (!gpu)
                return;

            // Set transform (row-major → column-major for bgfx)
            float mtx[16];
            cmd.transform.GetColumnMajor(mtx);
            bgfx::setTransform(mtx);

            // Bind vertex + index buffers
            bgfx::VertexBufferHandle vbh{ gpu->vertexBuffer };
            bgfx::IndexBufferHandle  ibh{ gpu->indexBuffer };
            bgfx::setVertexBuffer(0, vbh);
            bgfx::setIndexBuffer(ibh);

            // Set render state
            const uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A
                                 | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
                                 | BGFX_STATE_CULL_CW;
            bgfx::setState(state);

            // Resolve material; use default if not found
            const MaterialDescriptor* mat = mMaterials->Resolve(cmd.materialId);
            if (!mat)
                mat = &mMaterials->GetDefault();

            // Submit — extract program handle from material's ShaderProgram
            unsigned short progIdx = mat->program ? mat->program->GetProgramHandle() : 0;
            bgfx::ProgramHandle prog{ progIdx };
            bgfx::submit(mViewId, prog);
        }

    } // namespace Bgfx3D
} // namespace Dia
