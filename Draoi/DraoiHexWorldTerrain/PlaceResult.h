// S-1 Regions, Nodes and Site Hook: result of a placer / RegionsAndSites::Place.
#pragma once

#include <cstdint>
#include <cstring>

namespace Draoi {
namespace HexWorld {

struct PlaceResult
{
    enum class Code : uint8_t { Ok, Retryable, TerrainCause };

    Code code = Code::Ok;
    char reason[96] = {};

    bool IsOk() const { return code == Code::Ok; }

    static PlaceResult Ok() { return PlaceResult{}; }
    static PlaceResult Retryable(const char* why) { return Make(Code::Retryable, why); }
    static PlaceResult TerrainCause(const char* why) { return Make(Code::TerrainCause, why); }

private:
    static PlaceResult Make(Code c, const char* why)
    {
        PlaceResult r;
        r.code = c;
        if (why)
        {
            std::strncpy(r.reason, why, sizeof(r.reason) - 1);
            r.reason[sizeof(r.reason) - 1] = '\0';
        }
        return r;
    }
};

} // namespace HexWorld
} // namespace Draoi
