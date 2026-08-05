#pragma once
#ifndef DIA_ENTITYSPATIAL_TESTING_SPATIALTESTHELPERS_H
#define DIA_ENTITYSPATIAL_TESTING_SPATIALTESTHELPERS_H

#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace Dia { namespace EntitySpatial { namespace Testing {

    // -------------------------------------------------------------------------
    // SpawnSpatialEntity
    //
    // Creates an entity in `domain` with a SpatialComponent configured at the
    // given position, radius, and layerMask. The entity is queued but not yet
    // live — call FlushDomain() to apply.
    //
    // layerMask must use only bits 0–30; bit 31 is reserved for the engine dirty
    // flag. Values with bit 31 set are silently masked down to bits 0–30.
    // -------------------------------------------------------------------------
    inline Dia::Entity::Entity SpawnSpatialEntity(
        Dia::Entity::Domain&          domain,
        const Dia::Maths::Vector2D&   position,
        float                         radius    = 1.0f,
        uint32_t                      layerMask = 0x7FFFFFFFu)
    {
        Dia::Entity::Entity e = domain.CreateEntity();

        Json::Value cfg;
        cfg["position"]["x"] = position.x;
        cfg["position"]["y"] = position.y;
        cfg["radius"]        = radius;
        cfg["layerMask"]     = layerMask & 0x7FFFFFFFu;

        domain.QueueAddComponent<Dia::EntitySpatial::SpatialComponent>(e, cfg);
        return e;
    }

    // -------------------------------------------------------------------------
    // FlushDomain
    //
    // Calls Domain::EndOfFrame() to commit all queued mutations (entity creates,
    // component adds/removes, entity destroys).
    // -------------------------------------------------------------------------
    inline void FlushDomain(Dia::Entity::Domain& domain)
    {
        domain.EndOfFrame();
    }

    // -------------------------------------------------------------------------
    // AssertExactEntitySet
    //
    // Asserts that `result` contains exactly the same entities as `expected`,
    // regardless of order. Both sets are sorted by entity index before
    // comparison so that results are deterministic.
    //
    // `label` is prepended to each EXPECT_EQ failure message to aid diagnosis.
    //
    // std::vector and std::sort are intentionally used here: this is test
    // utility code and those dependencies are acceptable in that context.
    // -------------------------------------------------------------------------
    template<unsigned int N, unsigned int M>
    inline void AssertExactEntitySet(
        const Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, N>& result,
        const Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, M>& expected,
        const char* label = "")
    {
        std::vector<Dia::Entity::Entity> rVec;
        std::vector<Dia::Entity::Entity> eVec;

        for (uint32_t i = 0; i < result.Size(); ++i)
            rVec.push_back(result[i]);
        for (uint32_t i = 0; i < expected.Size(); ++i)
            eVec.push_back(expected[i]);

        auto cmp = [](const Dia::Entity::Entity& a, const Dia::Entity::Entity& b) {
            return a.GetIndex() < b.GetIndex();
        };
        std::sort(rVec.begin(), rVec.end(), cmp);
        std::sort(eVec.begin(), eVec.end(), cmp);

        EXPECT_EQ(rVec.size(), eVec.size()) << label << ": result count mismatch";

        const size_t compareCount = std::min(rVec.size(), eVec.size());
        for (size_t i = 0; i < compareCount; ++i)
            EXPECT_EQ(rVec[i], eVec[i]) << label << ": entity mismatch at index " << i;
    }

}}} // namespace Dia::EntitySpatial::Testing

#endif // DIA_ENTITYSPATIAL_TESTING_SPATIALTESTHELPERS_H
