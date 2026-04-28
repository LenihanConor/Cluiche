#include "DiaSoftBody2D/SoftBodyWorld.h"

#include "DiaCore/Core/Assert.h"
#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/World/PhysicsWorld.h"
#include "DiaRigidBody2D/WorldShapeUtil.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/Line.h"

#include <cmath>

namespace Dia::SoftBody2D {

SoftBodyWorld::SoftBodyWorld(const WorldDef& def)
    : mDef(def)
    , mAccumulator(0.0f)
{
}

SoftBodyWorld::~SoftBodyWorld()
{
    for (unsigned int i = 0; i < mBodies.Size(); ++i)
    {
        delete mBodies[i];
    }
}

Rope* SoftBodyWorld::AddRope(const RopeDef& def)
{
    Rope* rope = new Rope(def);
    mBodies.Add(rope);
    return rope;
}

Cloth* SoftBodyWorld::AddCloth(const ClothDef& def)
{
    Cloth* cloth = new Cloth(def);
    mBodies.Add(cloth);
    return cloth;
}

void SoftBodyWorld::RemoveBody(SoftBody* body)
{
    for (unsigned int i = 0; i < mBodies.Size(); ++i)
    {
        if (mBodies[i] == body)
        {
            delete mBodies[i];
            mBodies.RemoveAt(i);
            return;
        }
    }
    DIA_ASSERT(false, "RemoveBody: body not found in world");
}

int SoftBodyWorld::GetBodyCount() const
{
    return static_cast<int>(mBodies.Size());
}

void SoftBodyWorld::AddStaticShape(const Dia::Geometry2D::AARect* shape)
{
    mStaticRects.Add(shape);
}

void SoftBodyWorld::AddStaticShape(const Dia::Geometry2D::Circle* shape)
{
    mStaticCircles.Add(shape);
}

void SoftBodyWorld::AddStaticShape(const Dia::Geometry2D::Line* shape)
{
    mStaticLines.Add(shape);
}

void SoftBodyWorld::RemoveStaticShape(const void* shapePtr)
{
    for (unsigned int i = 0; i < mStaticRects.Size(); ++i)
    {
        if (mStaticRects[i] == shapePtr)
        {
            mStaticRects.RemoveAt(i);
            return;
        }
    }
    for (unsigned int i = 0; i < mStaticCircles.Size(); ++i)
    {
        if (mStaticCircles[i] == shapePtr)
        {
            mStaticCircles.RemoveAt(i);
            return;
        }
    }
    for (unsigned int i = 0; i < mStaticLines.Size(); ++i)
    {
        if (mStaticLines[i] == shapePtr)
        {
            mStaticLines.RemoveAt(i);
            return;
        }
    }
    DIA_ASSERT(false, "RemoveStaticShape: shape not found");
}

void SoftBodyWorld::Update(float deltaTime)
{
    mAccumulator += deltaTime;
    int steps = 0;
    while (mAccumulator >= mDef.fixedTimestep && steps < mDef.maxSubSteps)
    {
        StepOnce();
        mAccumulator -= mDef.fixedTimestep;
        ++steps;
    }

    if (steps >= mDef.maxSubSteps)
    {
        mAccumulator = 0.0f;
    }
}

void SoftBodyWorld::SetGravity(const Dia::Maths::Vector2D& gravity)
{
    mDef.gravity = gravity;
}

const Dia::Maths::Vector2D& SoftBodyWorld::GetGravity() const
{
    return mDef.gravity;
}

const Dia::Core::Containers::DynamicArray<SoftBody*>& SoftBodyWorld::GetBodies() const
{
    return mBodies;
}

void SoftBodyWorld::StepOnce()
{
    ApplyExternalForces();
    ProjectConstraints();
    ResolveGeometryCollision();
    ResolveRigidBodyCollision();
    FinalizeVelocities();
    CheckTearing();
}

void SoftBodyWorld::ApplyExternalForces()
{
    float dt = mDef.fixedTimestep;

    for (unsigned int b = 0; b < mBodies.Size(); ++b)
    {
        SoftBody* body = mBodies[b];
        int particleCount = 0;

        if (body->GetBodyType() == BodyType::kRope)
        {
            Rope* rope = static_cast<Rope*>(body);
            particleCount = rope->GetParticleCount();

            for (int i = 0; i < particleCount; ++i)
            {
                Particle& p = rope->GetParticle(i);
                if (p.invMass <= 0.0f)
                    continue;

                Dia::Maths::Vector2D velocity = DeriveVelocity(p, dt);
                Dia::Maths::Vector2D predictedPos = p.position + velocity * dt + mDef.gravity * (dt * dt);
                p.prevPosition = p.position;
                p.position = predictedPos;
            }

            // Anchor overwrites
            if (rope->GetStartAnchor() != nullptr)
            {
                rope->GetParticle(0).position = rope->GetStartAnchor()->GetTransform()->GetWorldPosition();
            }
            if (rope->GetEndAnchor() != nullptr)
            {
                rope->GetParticle(rope->GetParticleCount() - 1).position =
                    rope->GetEndAnchor()->GetTransform()->GetWorldPosition();
            }
        }
        else if (body->GetBodyType() == BodyType::kCloth)
        {
            Cloth* cloth = static_cast<Cloth*>(body);
            particleCount = cloth->GetParticleCount();

            for (int y = 0; y < cloth->GetResY(); ++y)
            {
                for (int x = 0; x < cloth->GetResX(); ++x)
                {
                    Particle& p = cloth->GetParticle(x, y);
                    if (p.invMass <= 0.0f)
                        continue;

                    Dia::Maths::Vector2D velocity = DeriveVelocity(p, dt);
                    Dia::Maths::Vector2D predictedPos = p.position + velocity * dt + mDef.gravity * (dt * dt);
                    p.prevPosition = p.position;
                    p.position = predictedPos;
                }
            }
        }
    }
}

void SoftBodyWorld::ProjectConstraints()
{
    float dt = mDef.fixedTimestep;

    for (int iter = 0; iter < mDef.solverIterations; ++iter)
    {
        for (unsigned int b = 0; b < mBodies.Size(); ++b)
        {
            SoftBody* body = mBodies[b];

            if (body->GetBodyType() == BodyType::kRope)
            {
                Rope* rope = static_cast<Rope*>(body);
                for (int c = 0; c < rope->GetConstraintCount(); ++c)
                {
                    DistanceConstraint& con = rope->GetConstraint(c);
                    if (!con.active)
                        continue;

                    Particle& pA = rope->GetParticle(con.indexA);
                    Particle& pB = rope->GetParticle(con.indexB);

                    Dia::Maths::Vector2D delta = pB.position - pA.position;
                    float dist = delta.Magnitude();
                    if (dist < 1e-7f)
                        continue;

                    float correction = (dist - con.restLength) / dist;
                    float alpha = (1.0f - con.stiffness) / (dt * dt);
                    float wA = pA.invMass;
                    float wB = pB.invMass;
                    float denom = wA + wB + alpha;
                    if (denom < 1e-7f)
                        continue;

                    float lambda = correction / denom;
                    Dia::Maths::Vector2D corrVec = delta * lambda;
                    pA.position = pA.position + corrVec * wA;
                    pB.position = pB.position - corrVec * wB;
                }
            }
            else if (body->GetBodyType() == BodyType::kCloth)
            {
                Cloth* cloth = static_cast<Cloth*>(body);
                for (int c = 0; c < cloth->GetConstraintCount(); ++c)
                {
                    DistanceConstraint& con = cloth->GetConstraint(c);
                    if (!con.active)
                        continue;

                    Particle& pA = cloth->GetParticle(con.indexA % cloth->GetResX(), con.indexA / cloth->GetResX());
                    Particle& pB = cloth->GetParticle(con.indexB % cloth->GetResX(), con.indexB / cloth->GetResX());

                    Dia::Maths::Vector2D delta = pB.position - pA.position;
                    float dist = delta.Magnitude();
                    if (dist < 1e-7f)
                        continue;

                    float correction = (dist - con.restLength) / dist;
                    float alpha = (1.0f - con.stiffness) / (dt * dt);
                    float wA = pA.invMass;
                    float wB = pB.invMass;
                    float denom = wA + wB + alpha;
                    if (denom < 1e-7f)
                        continue;

                    float lambda = correction / denom;
                    Dia::Maths::Vector2D corrVec = delta * lambda;
                    pA.position = pA.position + corrVec * wA;
                    pB.position = pB.position - corrVec * wB;
                }
            }
        }
    }
}

static void ResolveParticleVsAARect(Particle& p, const Dia::Geometry2D::AARect& rect, float dt)
{
    Dia::Maths::Vector2D closest;
    rect.ClosestPointOnAARectTo(p.position, closest);

    Dia::Maths::Vector2D delta = p.position - closest;
    float distSq = delta.x * delta.x + delta.y * delta.y;

    if (distSq >= p.radius * p.radius)
        return;

    float dist = std::sqrt(distSq);
    Dia::Maths::Vector2D normal;
    if (dist < 1e-6f)
    {
        Dia::Maths::Vector2D center = rect.CalculateCenter();
        normal = p.position - center;
        float nLen = normal.Magnitude();
        if (nLen < 1e-6f)
            normal = Dia::Maths::Vector2D(0.0f, 1.0f);
        else
            normal = normal * (1.0f / nLen);
        dist = 0.0f;
    }
    else
    {
        normal = delta * (1.0f / dist);
    }

    float penetration = p.radius - dist;
    p.position = p.position + normal * penetration;

    Dia::Maths::Vector2D vel = DeriveVelocity(p, dt);
    float normalVel = vel.Dot(normal);
    if (normalVel < 0.0f)
    {
        p.prevPosition = p.prevPosition - normal * (normalVel * dt);
    }
}

static void ResolveParticleVsCircle(Particle& p, const Dia::Geometry2D::Circle& circle, float dt)
{
    Dia::Maths::Vector2D delta = p.position - circle.GetCenter();
    float distSq = delta.x * delta.x + delta.y * delta.y;
    float radSum = p.radius + circle.GetRadius();

    if (distSq >= radSum * radSum)
        return;

    float dist = std::sqrt(distSq);
    Dia::Maths::Vector2D normal;
    if (dist < 1e-6f)
    {
        normal = Dia::Maths::Vector2D(0.0f, 1.0f);
        dist = 0.0f;
    }
    else
    {
        normal = delta * (1.0f / dist);
    }

    float penetration = radSum - dist;
    p.position = p.position + normal * penetration;

    Dia::Maths::Vector2D vel = DeriveVelocity(p, dt);
    float normalVel = vel.Dot(normal);
    if (normalVel < 0.0f)
    {
        p.prevPosition = p.prevPosition - normal * (normalVel * dt);
    }
}

static void ResolveParticleVsLine(Particle& p, const Dia::Geometry2D::Line& line, float dt)
{
    Dia::Maths::Vector2D closest;
    line.ClosestPointOnLineTo(p.position, closest);

    Dia::Maths::Vector2D delta = p.position - closest;
    float distSq = delta.x * delta.x + delta.y * delta.y;

    if (distSq >= p.radius * p.radius)
        return;

    float dist = std::sqrt(distSq);
    Dia::Maths::Vector2D normal;
    if (dist < 1e-6f)
    {
        Dia::Maths::Vector2D lineDir = line.Pt1ToPt2Vector();
        normal = Dia::Maths::Vector2D(-lineDir.y, lineDir.x);
        float nLen = normal.Magnitude();
        if (nLen > 1e-6f)
            normal = normal * (1.0f / nLen);
        else
            normal = Dia::Maths::Vector2D(0.0f, 1.0f);
        dist = 0.0f;
    }
    else
    {
        normal = delta * (1.0f / dist);
    }

    float penetration = p.radius - dist;
    p.position = p.position + normal * penetration;

    Dia::Maths::Vector2D vel = DeriveVelocity(p, dt);
    float normalVel = vel.Dot(normal);
    if (normalVel < 0.0f)
    {
        p.prevPosition = p.prevPosition - normal * (normalVel * dt);
    }
}

static void ResolveParticleGeometry(Particle& p,
    const Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::AARect*>& rects,
    const Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::Circle*>& circles,
    const Dia::Core::Containers::DynamicArray<const Dia::Geometry2D::Line*>& lines,
    float dt)
{
    for (unsigned int i = 0; i < rects.Size(); ++i)
        ResolveParticleVsAARect(p, *rects[i], dt);

    for (unsigned int i = 0; i < circles.Size(); ++i)
        ResolveParticleVsCircle(p, *circles[i], dt);

    for (unsigned int i = 0; i < lines.Size(); ++i)
        ResolveParticleVsLine(p, *lines[i], dt);
}

void SoftBodyWorld::ResolveGeometryCollision()
{
    if (mStaticRects.Size() == 0 && mStaticCircles.Size() == 0 && mStaticLines.Size() == 0)
        return;

    float dt = mDef.fixedTimestep;

    for (unsigned int b = 0; b < mBodies.Size(); ++b)
    {
        SoftBody* body = mBodies[b];

        if (body->GetBodyType() == BodyType::kRope)
        {
            Rope* rope = static_cast<Rope*>(body);
            for (int i = 0; i < rope->GetParticleCount(); ++i)
            {
                ResolveParticleGeometry(rope->GetParticle(i),
                    mStaticRects, mStaticCircles, mStaticLines, dt);
            }
        }
        else if (body->GetBodyType() == BodyType::kCloth)
        {
            Cloth* cloth = static_cast<Cloth*>(body);
            for (int y = 0; y < cloth->GetResY(); ++y)
            {
                for (int x = 0; x < cloth->GetResX(); ++x)
                {
                    ResolveParticleGeometry(cloth->GetParticle(x, y),
                        mStaticRects, mStaticCircles, mStaticLines, dt);
                }
            }
        }
    }
}

static bool ParticleVsBodyNarrow(const Particle& p, const Dia::RigidBody2D::Body2DBase* rb,
    Dia::Maths::Vector2D& outNormal, float& outDepth)
{
    Dia::Maths::Vector2D bodyPos(0.0f, 0.0f);
    const Dia::Geometry2D::Transform* t = rb->GetTransform();
    if (t) bodyPos = t->GetWorldPosition();

    const Dia::Geometry2D::Circle* circShape = rb->GetCircleShape();
    if (circShape)
    {
        Dia::Geometry2D::Circle worldCirc(circShape->GetRadius(), bodyPos);
        Dia::Maths::Vector2D delta = p.position - worldCirc.GetCenter();
        float distSq = delta.x * delta.x + delta.y * delta.y;
        float radSum = p.radius + worldCirc.GetRadius();
        if (distSq >= radSum * radSum)
            return false;

        float dist = std::sqrt(distSq);
        if (dist < 1e-6f)
        {
            outNormal = Dia::Maths::Vector2D(0.0f, 1.0f);
        }
        else
        {
            outNormal = delta * (1.0f / dist);
        }
        outDepth = radSum - dist;
        return true;
    }

    Dia::Geometry2D::AARect worldAABB = Dia::RigidBody2D::ComputeWorldAABB(rb);
    Dia::Maths::Vector2D closest;
    worldAABB.ClosestPointOnAARectTo(p.position, closest);

    Dia::Maths::Vector2D delta = p.position - closest;
    float distSq = delta.x * delta.x + delta.y * delta.y;
    if (distSq >= p.radius * p.radius)
        return false;

    float dist = std::sqrt(distSq);
    if (dist < 1e-6f)
    {
        Dia::Maths::Vector2D center = worldAABB.CalculateCenter();
        Dia::Maths::Vector2D toParticle = p.position - center;
        float nLen = toParticle.Magnitude();
        if (nLen < 1e-6f)
            outNormal = Dia::Maths::Vector2D(0.0f, 1.0f);
        else
            outNormal = toParticle * (1.0f / nLen);
    }
    else
    {
        outNormal = delta * (1.0f / dist);
    }
    outDepth = p.radius - dist;
    return true;
}

static void ResolveParticleVsRigidBody(Particle& p, Dia::RigidBody2D::Body2DBase* rb, float dt)
{
    Dia::Maths::Vector2D normal;
    float depth;
    if (!ParticleVsBodyNarrow(p, rb, normal, depth))
        return;

    p.position = p.position + normal * depth;

    Dia::Maths::Vector2D vel = DeriveVelocity(p, dt);
    float normalVel = vel.Dot(normal);
    if (normalVel < 0.0f)
    {
        p.prevPosition = p.prevPosition - normal * (normalVel * dt);
    }

    float particleMass = 1.0f / p.invMass;
    Dia::Maths::Vector2D backImpulse = normal * (depth * particleMass / dt);
    rb->ApplyImpulse(backImpulse * -1.0f);
}

void SoftBodyWorld::ResolveRigidBodyCollision()
{
    if (mDef.rigidBodyWorld == nullptr)
        return;

    float dt = mDef.fixedTimestep;

    // Part A — Anchor back-impulse
    for (unsigned int b = 0; b < mBodies.Size(); ++b)
    {
        SoftBody* body = mBodies[b];
        if (body->GetBodyType() != BodyType::kRope)
            continue;

        Rope* rope = static_cast<Rope*>(body);

        if (rope->GetStartAnchor() != nullptr)
        {
            Particle& p = rope->GetParticle(0);
            if (p.invMass > 0.0f)
            {
                float particleMass = 1.0f / p.invMass;
                Dia::Maths::Vector2D displacement = p.position - p.prevPosition;
                Dia::Maths::Vector2D impulse = displacement * (particleMass / dt);
                rope->GetStartAnchor()->ApplyImpulse(impulse * -1.0f);
            }
        }

        if (rope->GetEndAnchor() != nullptr)
        {
            Particle& p = rope->GetParticle(rope->GetParticleCount() - 1);
            if (p.invMass > 0.0f)
            {
                float particleMass = 1.0f / p.invMass;
                Dia::Maths::Vector2D displacement = p.position - p.prevPosition;
                Dia::Maths::Vector2D impulse = displacement * (particleMass / dt);
                rope->GetEndAnchor()->ApplyImpulse(impulse * -1.0f);
            }
        }
    }

    // Part B — Particle vs rigid body collision
    Dia::Core::Containers::DynamicArrayC<const Dia::RigidBody2D::Body2DBase*, Dia::RigidBody2D::kMaxQueryResults> candidates;

    for (unsigned int b = 0; b < mBodies.Size(); ++b)
    {
        SoftBody* body = mBodies[b];

        if (body->GetBodyType() == BodyType::kRope)
        {
            Rope* rope = static_cast<Rope*>(body);
            for (int i = 0; i < rope->GetParticleCount(); ++i)
            {
                Particle& p = rope->GetParticle(i);
                if (p.invMass <= 0.0f)
                    continue;

                Dia::Geometry2D::Circle particleCircle(p.radius, p.position);
                candidates.RemoveAll();
                mDef.rigidBodyWorld->QueryCircle(particleCircle, candidates);

                for (unsigned int c = 0; c < candidates.Size(); ++c)
                {
                    Dia::RigidBody2D::Body2DBase* rb = const_cast<Dia::RigidBody2D::Body2DBase*>(candidates[c]);
                    if (rb == rope->GetStartAnchor() || rb == rope->GetEndAnchor())
                        continue;
                    ResolveParticleVsRigidBody(p, rb, dt);
                }
            }
        }
        else if (body->GetBodyType() == BodyType::kCloth)
        {
            Cloth* cloth = static_cast<Cloth*>(body);
            for (int y = 0; y < cloth->GetResY(); ++y)
            {
                for (int x = 0; x < cloth->GetResX(); ++x)
                {
                    Particle& p = cloth->GetParticle(x, y);
                    if (p.invMass <= 0.0f)
                        continue;

                    Dia::Geometry2D::Circle particleCircle(p.radius, p.position);
                    candidates.RemoveAll();
                    mDef.rigidBodyWorld->QueryCircle(particleCircle, candidates);

                    for (unsigned int c = 0; c < candidates.Size(); ++c)
                    {
                        Dia::RigidBody2D::Body2DBase* rb = const_cast<Dia::RigidBody2D::Body2DBase*>(candidates[c]);
                        ResolveParticleVsRigidBody(p, rb, dt);
                    }
                }
            }
        }
    }
}

void SoftBodyWorld::FinalizeVelocities()
{
    // PBD: velocity is derived from (position - prevPosition) / dt.
    // prevPosition was set in ApplyExternalForces. No explicit update needed.
    // This phase exists for optional damping — no damping in v1.
}

void SoftBodyWorld::CheckTearing()
{
    for (unsigned int b = 0; b < mBodies.Size(); ++b)
    {
        SoftBody* body = mBodies[b];
        if (body->GetBodyType() == BodyType::kRope)
        {
            static_cast<Rope*>(body)->CheckTearing();
        }
        else if (body->GetBodyType() == BodyType::kCloth)
        {
            static_cast<Cloth*>(body)->CheckTearing();
        }
    }
}

} // namespace Dia::SoftBody2D
