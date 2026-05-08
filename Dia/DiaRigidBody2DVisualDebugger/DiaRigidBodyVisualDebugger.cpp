#include "DiaRigidBody2DVisualDebugger/DiaRigidBodyVisualDebugger.h"

#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/Bodies/BodyType.h"
#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"
#include "DiaGraphics/Frame/FrameData.h"
#include "DiaGraphics/Misc/RGBA.h"

#include <cmath>

namespace Dia::RigidBody2D {

static constexpr float kContactNormalLength = 0.3f;
static const Dia::Graphics::RGBA kSleepColour{ 0, 0, 80, 255 };

static Dia::Graphics::RGBA BodyColour(const Body2DBase* body)
{
    if (!body->IsAwake()) return kSleepColour;
    switch (body->GetBodyType())
    {
        case BodyType::kDynamic:   return Dia::Graphics::RGBA::White;
        case BodyType::kStatic:    return Dia::Graphics::RGBA(128, 128, 128, 255);
        case BodyType::kKinematic: return Dia::Graphics::RGBA::Cyan;
    }
    return Dia::Graphics::RGBA::White;
}

static Dia::Maths::Vector2D RotateVec(const Dia::Maths::Vector2D& v, float rad)
{
    const float c = std::cos(rad);
    const float s = std::sin(rad);
    return Dia::Maths::Vector2D(c * v.x - s * v.y, s * v.x + c * v.y);
}

// ---------------------------------------------------------------------------

DiaRigidBodyVisualDebugger::DiaRigidBodyVisualDebugger()
    : mEnabled(false)
    , mOptions()
{}

void DiaRigidBodyVisualDebugger::SetEnabled(bool enabled)                    { mEnabled = enabled; }
bool DiaRigidBodyVisualDebugger::IsEnabled() const                           { return mEnabled; }
void DiaRigidBodyVisualDebugger::SetOptions(const VisualDebuggerOptions& o)  { mOptions = o; }
const VisualDebuggerOptions& DiaRigidBodyVisualDebugger::GetOptions() const  { return mOptions; }

void DiaRigidBodyVisualDebugger::Draw(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData)
{
    if (!mEnabled) return;
    DrawBodies(world, frameData);
    DrawVelocityArrows(world, frameData);
    DrawContactNormals(world, frameData);
    DrawConstraints(world, frameData);
}

// ---------------------------------------------------------------------------

static void DrawBody(const Body2DBase* body, Dia::Graphics::FrameData& frameData)
{
    const Dia::Geometry2D::Transform* t = body->GetTransform();
    if (!t) return;

    const Dia::Maths::Vector2D pos = t->GetWorldPosition();
    const float                rot = t->GetLocalRotation().AsRadians();
    const Dia::Graphics::RGBA  col = BodyColour(body);

    switch (body->GetShapeKind())
    {
        case ShapeKind::kCircle:
        {
            const Dia::Geometry2D::Circle* c = body->GetCircleShape();
            if (c)
                frameData.RequestDraw(pos, c->GetRadius(), col);
            break;
        }
        case ShapeKind::kPoly:
        {
            const Dia::Geometry2D::ConvexPolygon* poly = body->GetPolyShape();
            if (poly && poly->GetVertexCount() >= 2)
            {
                const int n = poly->GetVertexCount();
                for (int i = 0; i < n; ++i)
                {
                    const Dia::Maths::Vector2D lv0 = poly->GetVertex(i);
                    const Dia::Maths::Vector2D lv1 = poly->GetVertex((i + 1) % n);
                    const Dia::Maths::Vector2D wv0 = pos + RotateVec(lv0, rot);
                    const Dia::Maths::Vector2D wv1 = pos + RotateVec(lv1, rot);
                    frameData.RequestDraw(wv0, wv1, col);
                }
            }
            break;
        }
        default:
            frameData.RequestDrawPoint(pos, col);
            break;
    }
}

void DiaRigidBodyVisualDebugger::DrawBodies(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData)
{
    const auto& pointBodies  = world.GetPointBodies();
    const auto& rigidBodies  = world.GetRigidBodies();

    for (unsigned int i = 0; i < pointBodies.Size(); ++i)
        DrawBody(pointBodies[i], frameData);

    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
        DrawBody(rigidBodies[i], frameData);
}

// ---------------------------------------------------------------------------

void DiaRigidBodyVisualDebugger::DrawVelocityArrows(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData)
{
    static const Dia::Graphics::RGBA kVelColour{ 255, 255, 0, 255 };

    auto drawArrow = [&](const Body2DBase* body)
    {
        if (!body->IsAwake()) return;
        if (body->GetBodyType() == BodyType::kStatic) return;

        const Dia::Geometry2D::Transform* t = body->GetTransform();
        if (!t) return;

        const Dia::Maths::Vector2D pos = t->GetWorldPosition();
        const Dia::Maths::Vector2D vel = body->GetVelocity();
        const float speed = std::sqrt(vel.x * vel.x + vel.y * vel.y);
        if (speed < 1e-4f) return;

        float len = speed * mOptions.velocityArrowScale;
        if (len > mOptions.velocityArrowMaxLen) len = mOptions.velocityArrowMaxLen;

        const Dia::Maths::Vector2D dir{ vel.x / speed, vel.y / speed };
        frameData.RequestDrawRay(pos, dir, len, kVelColour);
    };

    const auto& pointBodies = world.GetPointBodies();
    const auto& rigidBodies = world.GetRigidBodies();

    for (unsigned int i = 0; i < pointBodies.Size(); ++i)
        drawArrow(pointBodies[i]);
    for (unsigned int i = 0; i < rigidBodies.Size(); ++i)
        drawArrow(rigidBodies[i]);
}

// ---------------------------------------------------------------------------

void DiaRigidBodyVisualDebugger::DrawContactNormals(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData)
{
    static const Dia::Graphics::RGBA kContactColour{ 255, 128, 0, 255 };

    const auto& contacts = world.GetLastContacts();
    for (unsigned int i = 0; i < contacts.Size(); ++i)
    {
        const Contact& c = contacts[i];
        frameData.RequestDrawRay(c.point, c.normal, kContactNormalLength, kContactColour);
    }
}

// ---------------------------------------------------------------------------

void DiaRigidBodyVisualDebugger::DrawConstraints(const PhysicsWorld& world, Dia::Graphics::FrameData& frameData)
{
    static const Dia::Graphics::RGBA kConstraintColour{ 0, 200, 255, 255 };

    const auto& constraints = world.GetConstraints();
    for (unsigned int i = 0; i < constraints.Size(); ++i)
    {
        const IConstraint* c = constraints[i];
        const Dia::Maths::Vector2D anchorA = c->GetWorldAnchorA();
        const Dia::Maths::Vector2D anchorB = c->GetWorldAnchorB();
        frameData.RequestDraw(anchorA, anchorB, kConstraintColour);
    }
}

} // namespace Dia::RigidBody2D
