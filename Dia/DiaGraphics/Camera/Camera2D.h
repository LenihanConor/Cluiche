////////////////////////////////////////////////////////////////////////////////
// Filename: Camera2D.h — Forwarding header (Camera2D moved to DiaCamera2D)
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaCamera2D/Camera2D.h>

// Backward-compatibility alias so existing code using Dia::Graphics::Camera2D
// continues to compile while it migrates to Dia::Camera2D::Camera2D.
namespace Dia { namespace Graphics { using Camera2D = Dia::Camera2D::Camera2D; } }
