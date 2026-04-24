---
schema: dia.module.v1
module_id: dia.geometry2d
name: DiaGeometry2D
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaGeometry2D
language: cpp
parent_module_id: dia.root

summary: >
  2D geometry library — shapes, intersection tests, transforms, and spatial acceleration structures.

intent: >
  Owns all 2D geometric primitives migrated from DiaMaths, plus new types. Provides full
  pairwise intersection tests (GJK+EPA for convex pairs; fast paths for common cases),
  2D transform with parent-child hierarchy, and spatial structures (grid, quadtree, BVH).
  DiaMaths retains only pure linear algebra after this migration.

responsibilities:
  - Own all 2D geometric primitive types (Circle, AARect, OORect, Line, Ray, Triangle, Arc, Capsule, Point, Plane, ConvexPolygon, Sector, AnnularSector)
  - Provide full pairwise intersection tests returning IntersectionClassify
  - Provide ContactResult struct for normals, depth, and contact point
  - Provide closest-point queries between shapes
  - Own 2D transform with local/world space and parent-child hierarchy
  - Provide ISpatialStructure<T> interface and three implementations: SpatialGrid, Quadtree, BVH

non_responsibilities:
  - Pure math (vectors, matrices, angles, trig) — DiaMaths
  - 3D geometry — future DiaGeometry3D
  - Physics simulation (forces, rigid bodies, collision response) — DiaRigidBody2D
  - Rendering or debug drawing — DiaGraphics
  - Scene management beyond Transform — future DiaScene
  - Serialization of geometry data

dependent_modules:
  - dia.maths
  - dia.core

public_api:
  headers:
    - DiaGeometry2D/Shapes/Circle.h
    - DiaGeometry2D/Shapes/AARect.h
    - DiaGeometry2D/Shapes/OORect.h
    - DiaGeometry2D/Shapes/Line.h
    - DiaGeometry2D/Shapes/Ray.h
    - DiaGeometry2D/Shapes/Triangle.h
    - DiaGeometry2D/Shapes/Arc.h
    - DiaGeometry2D/Shapes/Capsule.h
    - DiaGeometry2D/Shapes/Point.h
    - DiaGeometry2D/Shapes/Plane.h
    - DiaGeometry2D/Shapes/ConvexPolygon.h
    - DiaGeometry2D/Shapes/Sector.h
    - DiaGeometry2D/Shapes/AnnularSector.h
    - DiaGeometry2D/Shapes/IntersectionClassify.h
    - DiaGeometry2D/Shapes/ContactResult.h
    - DiaGeometry2D/Intersection/IntersectionTests.h
    - DiaGeometry2D/Transform/Transform.h
  namespaces:
    - Dia::Geometry2D
  entry_points:
    - Circle
    - AARect
    - OORect
    - Line
    - Ray
    - Triangle
    - Arc
    - Capsule
    - Point
    - Plane
    - ConvexPolygon
    - Sector
    - AnnularSector
    - IntersectionClassify
    - ContactResult
    - IntersectionTests
    - Transform
    - ISpatialStructure
    - SpatialGrid
    - Quadtree
    - BVH

dependencies:
  required:
    - dia.maths
    - dia.core
  forbidden:
    - dia.graphics
    - dia.application
---
