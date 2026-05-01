#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaRigidBody2D/Triggers/TriggerVolume2D.h"
#include "DiaGeometry2D/Transform/Transform.h"
#include "DiaGeometry2D/Shapes/AARect.h"
#include "DiaGeometry2D/Shapes/Circle.h"
#include "DiaGeometry2D/Shapes/ConvexPolygon.h"

namespace Dia::RigidBody2D {

inline Dia::Geometry2D::AARect ComputeWorldAABB(const Body2DBase* body)
{
    Dia::Maths::Vector2D pos(0.0f, 0.0f);
    const Dia::Geometry2D::Transform* t = body->GetTransform();
    if (t) pos = t->GetWorldPosition();

    const Dia::Geometry2D::Circle* circle = body->GetCircleShape();
    if (circle)
    {
        float r = circle->GetRadius();
        return Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(pos.x - r, pos.y - r),
            Dia::Maths::Vector2D(pos.x + r, pos.y + r));
    }

    const Dia::Geometry2D::ConvexPolygon* poly = body->GetPolyShape();
    if (poly && poly->GetVertexCount() > 0)
    {
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (int i = 0; i < poly->GetVertexCount(); ++i)
        {
            float wx = pos.x + poly->GetVertex(i).x;
            float wy = pos.y + poly->GetVertex(i).y;
            if (wx < minX) minX = wx;
            if (wy < minY) minY = wy;
            if (wx > maxX) maxX = wx;
            if (wy > maxY) maxY = wy;
        }
        return Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(minX, minY),
            Dia::Maths::Vector2D(maxX, maxY));
    }

    return Dia::Geometry2D::AARect(pos, pos);
}

inline Dia::Geometry2D::Circle ComputeWorldCircle(const Body2DBase* body)
{
    Dia::Maths::Vector2D pos(0.0f, 0.0f);
    const Dia::Geometry2D::Transform* t = body->GetTransform();
    if (t) pos = t->GetWorldPosition();

    const Dia::Geometry2D::Circle* circle = body->GetCircleShape();
    float r = circle ? circle->GetRadius() : 0.0f;
    return Dia::Geometry2D::Circle(r, pos);
}

inline Dia::Geometry2D::AARect ComputeWorldAABB(const TriggerVolume2D* trigger)
{
    Dia::Maths::Vector2D pos(0.0f, 0.0f);
    const Dia::Geometry2D::Transform* t = trigger->GetTransform();
    if (t) pos = t->GetWorldPosition();

    const Dia::Geometry2D::Circle* circle = trigger->GetCircleShape();
    if (circle)
    {
        float r = circle->GetRadius();
        return Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(pos.x - r, pos.y - r),
            Dia::Maths::Vector2D(pos.x + r, pos.y + r));
    }

    const Dia::Geometry2D::ConvexPolygon* poly = trigger->GetPolyShape();
    if (poly && poly->GetVertexCount() > 0)
    {
        float minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        for (int i = 0; i < poly->GetVertexCount(); ++i)
        {
            float wx = pos.x + poly->GetVertex(i).x;
            float wy = pos.y + poly->GetVertex(i).y;
            if (wx < minX) minX = wx;
            if (wy < minY) minY = wy;
            if (wx > maxX) maxX = wx;
            if (wy > maxY) maxY = wy;
        }
        return Dia::Geometry2D::AARect(
            Dia::Maths::Vector2D(minX, minY),
            Dia::Maths::Vector2D(maxX, maxY));
    }

    return Dia::Geometry2D::AARect(pos, pos);
}

inline Dia::Geometry2D::Circle ComputeWorldCircle(const TriggerVolume2D* trigger)
{
    Dia::Maths::Vector2D pos(0.0f, 0.0f);
    const Dia::Geometry2D::Transform* t = trigger->GetTransform();
    if (t) pos = t->GetWorldPosition();

    const Dia::Geometry2D::Circle* circle = trigger->GetCircleShape();
    float r = circle ? circle->GetRadius() : 0.0f;
    return Dia::Geometry2D::Circle(r, pos);
}

} // namespace Dia::RigidBody2D
