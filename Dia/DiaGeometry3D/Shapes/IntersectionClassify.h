#pragma once
#ifndef DIA_GEOMETRY3D_INTERSECTIONCLASSIFY_H
#define DIA_GEOMETRY3D_INTERSECTIONCLASSIFY_H

namespace Dia::Geometry3D
{
    enum class IntersectionClassify
    {
        kNoIntersection,
        kPenetrating,
        kAContainsB,
        kBContainsA,
    };
}

#endif
