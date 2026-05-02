#pragma once
#ifndef DIA_GRAPHICS_TESTING_MOCKVISITORS_H
#define DIA_GRAPHICS_TESTING_MOCKVISITORS_H

#include <DiaGraphics/Frame/EntityFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>
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
// RecordingDebugVisitor — counts per-primitive-type and frame Visit calls
// ---------------------------------------------------------------------------

struct RecordingDebugVisitor : public DebugFrameDataVisitor
{
	mutable int primitiveCount[7] = {};  // indexed by DebugPrimitiveType
	mutable int frameCount = 0;

	void Visit(const DebugPrimitive& p) const override
	{
		++primitiveCount[static_cast<int>(p.type)];
	}

	void Visit(const DebugFrameData& /*obj*/) const override { ++frameCount; }

	int CircleCount()   const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Circle2D)];   }
	int LineCount()     const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Line2D)];     }
	int PointCount()    const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Point2D)];    }
	int RectCount()     const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Rect2D)];     }
	int ArcCount()      const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Arc2D)];      }
	int RayCount()      const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Ray2D)];      }
	int TriangleCount() const { return primitiveCount[static_cast<int>(DebugPrimitiveType::Triangle2D)]; }

	int TotalCount() const
	{
		int total = 0;
		for (int i = 0; i < 7; ++i) total += primitiveCount[i];
		return total;
	}

	void Reset()
	{
		for (int i = 0; i < 7; ++i) primitiveCount[i] = 0;
		frameCount = 0;
	}
};

// ---------------------------------------------------------------------------
// InspectingDebugVisitor — records the last visited primitive for field checks
// ---------------------------------------------------------------------------

struct InspectingDebugVisitor : public DebugFrameDataVisitor
{
	mutable DebugPrimitive lastPrimitive = {};
	mutable int            visitCount    = 0;
	mutable int            frameCount    = 0;

	void Visit(const DebugPrimitive& p) const override
	{
		lastPrimitive = p;
		++visitCount;
	}

	void Visit(const DebugFrameData& /*obj*/) const override { ++frameCount; }

	void Reset() { visitCount = 0; frameCount = 0; }
};

// ---------------------------------------------------------------------------
// OrderRecordingDebugVisitor — records insertion order of primitive types
// ---------------------------------------------------------------------------

struct OrderRecordingDebugVisitor : public DebugFrameDataVisitor
{
	static constexpr int kMaxRecorded = 64;
	mutable DebugPrimitiveType order[kMaxRecorded] = {};
	mutable int                count = 0;
	mutable int                frameCount = 0;

	void Visit(const DebugPrimitive& p) const override
	{
		if (count < kMaxRecorded)
			order[count++] = p.type;
	}

	void Visit(const DebugFrameData& /*obj*/) const override { ++frameCount; }

	void Reset() { count = 0; frameCount = 0; }
};

}}} // namespace Dia::Graphics::Testing

#endif // DIA_GRAPHICS_TESTING_MOCKVISITORS_H
