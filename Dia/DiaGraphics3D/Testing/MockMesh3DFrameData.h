#pragma once
#ifndef DIA_GRAPHICS3D_TESTING_MOCKMESH3DFRAMEDATA_H
#define DIA_GRAPHICS3D_TESTING_MOCKMESH3DFRAMEDATA_H

#include "DiaGraphics3D/Mesh3DFrameData.h"

namespace Dia { namespace Graphics3D { namespace Testing {

// ---------------------------------------------------------------------------
// MockMesh3DFrameData — test stub for Mesh3DFrameData
//
// Inherits Mesh3DFrameData unchanged. All public API is inherited.
// Use this type in GoogleTest fixtures that need a concrete Mesh3DFrameData;
// it signals test intent clearly without adding complexity.
// ---------------------------------------------------------------------------

struct MockMesh3DFrameData : public Mesh3DFrameData
{
    MockMesh3DFrameData() = default;
};

}}} // namespace Dia::Graphics3D::Testing

#endif // DIA_GRAPHICS3D_TESTING_MOCKMESH3DFRAMEDATA_H
