////////////////////////////////////////////////////////////////////////////////
// Filename: ViewportTransform.h — Forwarding header (moved to DiaCamera2D)
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <DiaCamera2D/ViewportTransform.h>

// Backward-compatibility alias so existing code using Dia::Graphics::ViewportTransform
// continues to compile while it migrates to Dia::Camera2D::ViewportTransform.
namespace Dia { namespace Graphics { using ViewportTransform = Dia::Camera2D::ViewportTransform; } }
