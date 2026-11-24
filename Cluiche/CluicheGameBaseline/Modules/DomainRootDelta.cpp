#include "Modules/DomainRootDelta.h"

namespace Cluiche { namespace AppFlow {

DomainRootDelta ComputeDomainRootDelta(const Json::Value& prev, const Json::Value& next)
{
    DomainRootDelta delta;
    if (!prev.isObject() || !next.isObject())
    {
        delta.structureChanged = true;
        return delta;
    }

    for (Json::ValueConstIterator it = next.begin(); it != next.end(); ++it)
    {
        const std::string name = it.name();
        if (name == "simFrame")
            continue;
        if (!prev.isMember(name))
        {
            delta.structureChanged = true;
            return delta;
        }
        if (!(prev[name] == *it))
            delta.changed.push_back(name);
    }
    for (Json::ValueConstIterator it = prev.begin(); it != prev.end(); ++it)
    {
        const std::string name = it.name();
        if (name != "simFrame" && !next.isMember(name))
        {
            delta.structureChanged = true;
            delta.changed.clear();
            return delta;
        }
    }
    return delta;
}

Json::Value BuildPartialRoot(const Json::Value& next, const std::vector<std::string>& names)
{
    Json::Value partial(Json::objectValue);
    partial["__partial"] = true;
    for (const std::string& name : names)
    {
        if (next.isMember(name))
            partial[name] = next[name];
    }
    return partial;
}

} } // namespace Cluiche::AppFlow
