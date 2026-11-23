#include <Shared/WorldStep/WorldSimSaveCodec.h>

namespace CoW {
namespace WorldStep {

bool ParamsHashMatches(const Json::Value& record, uint64_t currentHash)
{
    if (!record.isObject() || !record.isMember("params_hash") || !record["params_hash"].isUInt64()) { return false; }
    return record["params_hash"].asUInt64() == currentHash;
}

}  // namespace WorldStep
}  // namespace CoW
