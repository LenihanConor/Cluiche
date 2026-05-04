////////////////////////////////////////////////////////////////////////////////
// Filename: SoftConstraintsDrawer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSoftBody2DVisualDebugger/SoftConstraintsDrawer.h"

#ifdef DIA_DEBUG

#include "DiaSoftBody2D/SoftBodyWorld.h"
#include "DiaSoftBody2D/SoftBody.h"
#include "DiaSoftBody2D/Rope.h"
#include "DiaSoftBody2D/Cloth.h"
#include "DiaSoftBody2D/Particle.h"
#include "DiaSoftBody2D/Constraints/DistanceConstraint.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaVisualDebugger/DebugLayerManager.h"
#include "DiaVisualDebugger/DebugColourPalette.h"
#include "DiaVisualDebugger/DebugLayerNames.h"
#include "DiaCore/Core/Assert.h"

namespace Dia::SoftBody2D
{

SoftConstraintsDrawer::SoftConstraintsDrawer(const SoftBodyWorld&                world,
                                             const Dia::Debug::DebugLayerManager& manager)
    : mWorld(world)
    , mManager(manager)
{}

Dia::Core::StringCRC SoftConstraintsDrawer::GetLayerName() const
{
    return Dia::Debug::LayerNames::kSoftConstraints;
}

static Dia::Graphics::RGBA ConstraintColour(ConstraintType type)
{
    switch (type)
    {
        case ConstraintType::kRope:       return Dia::Debug::DebugColourPalette::kActive;
        case ConstraintType::kStructural: return Dia::Debug::DebugColourPalette::kActive;
        case ConstraintType::kShear:      return Dia::Debug::DebugColourPalette::kGoal;
        case ConstraintType::kBend:       return Dia::Debug::DebugColourPalette::kInactive;
    }
    return Dia::Debug::DebugColourPalette::kActive;
}

static void DrawConstraintsFromRope(const Rope* rope, Dia::Graphics::FrameData& frameData)
{
    const int count = rope->GetConstraintCount();
    for (int i = 0; i < count; ++i)
    {
        const DistanceConstraint& c = rope->GetConstraint(i);
        if (!c.active) continue;

        const Dia::Maths::Vector2D& posA = rope->GetParticle(c.indexA).position;
        const Dia::Maths::Vector2D& posB = rope->GetParticle(c.indexB).position;
        frameData.RequestDraw(posA, posB, ConstraintColour(c.type));
    }
}

static void DrawConstraintsFromCloth(const Cloth* cloth, Dia::Graphics::FrameData& frameData)
{
    const int resX  = cloth->GetResX();
    const int resY  = cloth->GetResY();
    const int count = cloth->GetConstraintCount();

    for (int i = 0; i < count; ++i)
    {
        const DistanceConstraint& c = cloth->GetConstraint(i);
        if (!c.active) continue;

        // Flat index to 2D grid
        const int axA = c.indexA % resX;  const int ayA = c.indexA / resX;
        const int axB = c.indexB % resX;  const int ayB = c.indexB / resX;

        // Guard against indices outside grid bounds
        if (axA >= resX || ayA >= resY || axB >= resX || ayB >= resY) continue;

        const Dia::Maths::Vector2D& posA = cloth->GetParticle(axA, ayA).position;
        const Dia::Maths::Vector2D& posB = cloth->GetParticle(axB, ayB).position;
        frameData.RequestDraw(posA, posB, ConstraintColour(c.type));
    }
}

void SoftConstraintsDrawer::Draw(Dia::Graphics::FrameData& frameData)
{
    const auto& bodies = mWorld.GetBodies();

    for (unsigned int b = 0; b < bodies.Size(); ++b)
    {
        const SoftBody* body = bodies[b];
        DIA_ASSERT(body != nullptr, "SoftBodyWorld contains null body pointer");

        switch (body->GetBodyType())
        {
            case BodyType::kRope:
                DrawConstraintsFromRope(static_cast<const Rope*>(body), frameData);
                break;
            case BodyType::kCloth:
                DrawConstraintsFromCloth(static_cast<const Cloth*>(body), frameData);
                break;
        }
    }
}

} // namespace Dia::SoftBody2D

#endif // DIA_DEBUG
