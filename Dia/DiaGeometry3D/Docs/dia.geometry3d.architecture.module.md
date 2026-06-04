---
schema: dia.module.v1
module_id: dia.geometry3d
name: DiaGeometry3D
layer: foundation/maths
path: Dia/DiaGeometry3D

dependent_modules:
  - dia.core
  - dia.maths

public_api:
  entry_points:
    - Dia::Geometry3D::IntersectionClassify
    - Dia::Geometry3D::AABB
    - Dia::Geometry3D::OOBB
    - Dia::Geometry3D::Sphere
    - Dia::Geometry3D::Capsule
    - Dia::Geometry3D::Triangle
    - Dia::Geometry3D::Cylinder
    - Dia::Geometry3D::Ray
    - Dia::Geometry3D::Plane
    - Dia::Geometry3D::PlaneSide
    - Dia::Geometry3D::Frustum
    - Dia::Geometry3D::FrustumPlane
    - Dia::Geometry3D::IntersectionTests
    - Dia::Geometry3D::ISpatialStructure3D
    - Dia::Geometry3D::SpatialGrid3D
    - Dia::Geometry3D::kMaxQueryResults

responsibilities:
  - Own all 3D geometric primitive types (9 shapes)
  - Pairwise intersection tests via IntersectionTests static API
  - Point containment and closest-point queries
  - ISpatialStructure3D interface and SpatialGrid3D uniform grid implementation
  - Test utilities in Testing/ subdirectory

non_responsibilities:
  - Pure linear algebra (DiaMaths)
  - 2D geometry (DiaGeometry2D)
  - 2D-to-3D conversions (DiaGeometryBridge)
  - Visual debug rendering (DiaGeometry3DVisualDebugger)
  - Physics simulation (DiaRigidBody3D)
---
