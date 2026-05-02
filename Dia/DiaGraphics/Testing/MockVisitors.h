#pragma once
#ifndef DIA_GRAPHICS_TESTING_MOCKVISITORS_H
#define DIA_GRAPHICS_TESTING_MOCKVISITORS_H

#include <DiaGraphics/Frame/EntityFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugFrameDataCircle2D.h>
#include <DiaGraphics/Frame/DebugFrameDataLine2D.h>
#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaGraphics/Frame/EntityFrameData.h>

namespace Dia { namespace Graphics { namespace Testing {

// ---------------------------------------------------------------------------
// RecordingEntityVisitor — counts how many times Visit is called
// ---------------------------------------------------------------------------

struct RecordingEntityVisitor : public EntityFrameDataVisitor
{
    mutable int visitCount = 0;

    void Visit(const EntityFrameData& /*data*/) const override { ++visitCount; }
};

// ---------------------------------------------------------------------------
// RecordingDebugVisitor — counts circle, line, and frame Visit calls
// ---------------------------------------------------------------------------

struct RecordingDebugVisitor : public DebugFrameDataVisitor
{
    mutable int circleCount = 0;
    mutable int lineCount   = 0;
    mutable int frameCount  = 0;

    void Visit(const DebugFrameDataCircle2D& /*obj*/) const override { ++circleCount; }
    void Visit(const DebugFrameDataLine2D&   /*obj*/) const override { ++lineCount;   }
    void Visit(const DebugFrameData&         /*obj*/) const override { ++frameCount;  }

    void Reset() { circleCount = lineCount = frameCount = 0; }
};

}}} // namespace Dia::Graphics::Testing

#endif // DIA_GRAPHICS_TESTING_MOCKVISITORS_H
