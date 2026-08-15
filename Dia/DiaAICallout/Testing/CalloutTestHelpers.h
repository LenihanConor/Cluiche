#pragma once

#include <DiaAICallout/Callout.h>
#include <DiaAICallout/CalloutHandle.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaCore/Json/external/json/json.h>

#include <gtest/gtest.h>

namespace Dia::AICallout::Testing {

    //-------------------------------------------------------------------------------------------
    // CalloutTestHelpers
    //
    // Utility functions for testing CalloutRegistry behaviour. Designed for use inside GoogleTests.
    // Header-only (all inline) so consumers opt in by including this header — no gtest
    // dependency is introduced into DiaAICallout.lib itself.
    //
    // SD-007: Lives in DiaAICallout/Testing/.
    //-------------------------------------------------------------------------------------------

    // Emit a callout with a fixed TTL and return its handle.
    // Creates a Callout with kind, position, radius, and ttl; faction is kZero (any),
    // payload is empty. Returns the handle from registry.Emit().
    inline CalloutHandle EmitTestCallout(CalloutRegistry& registry,
                                         Dia::Core::StringCRC kind,
                                         Dia::Maths::Vector2D position,
                                         float radius = 100.0f,
                                         float ttl = 10.0f)
    {
        Callout callout{
            kind,                             // kind
            position,                         // position
            radius,                           // radius
            Dia::Core::StringCRC::kZero,      // faction = any
            ttl,                              // ttl
            Json::Value()                     // payload = empty
        };

        return registry.Emit(callout);
    }

    // Assert a given handle is in the query results.
    // Loops over results and checks if any handle's index and generation match expected.
    // Uses gtest EXPECT_TRUE so failures are non-fatal.
    template <unsigned int N>
    inline void AssertInQueryResults(const Dia::Core::Containers::DynamicArrayC<CalloutHandle, N>& results,
                                     const CalloutHandle& expected)
    {
        bool found = false;

        for (unsigned int i = 0u; i < results.Size(); ++i)
        {
            const CalloutHandle& candidate = results[i];
            // Compare via public interface: both must have the same index and generation
            // (which means they refer to the same slot at the same generation)
            if (candidate.GetIndex() == expected.GetIndex() &&
                candidate.GetGeneration() == expected.GetGeneration())
            {
                found = true;
                break;
            }
        }

        EXPECT_TRUE(found) << "Expected callout handle was not found in query results. "
                           << "Index=" << expected.GetIndex()
                           << " Generation=" << expected.GetGeneration();
    }

    // Assert a handle is absent from query results.
    // Loops over results and fails if the handle is found.
    // Uses gtest EXPECT_TRUE so failures are non-fatal.
    template <unsigned int N>
    inline void AssertNotInQueryResults(const Dia::Core::Containers::DynamicArrayC<CalloutHandle, N>& results,
                                        const CalloutHandle& absent)
    {
        for (unsigned int i = 0u; i < results.Size(); ++i)
        {
            const CalloutHandle& candidate = results[i];
            if (candidate.GetIndex() == absent.GetIndex() &&
                candidate.GetGeneration() == absent.GetGeneration())
            {
                EXPECT_TRUE(false) << "Callout handle was not expected to be in query results but was found. "
                                   << "Index=" << absent.GetIndex()
                                   << " Generation=" << absent.GetGeneration();
                return;
            }
        }
    }

} // namespace Dia::AICallout::Testing
