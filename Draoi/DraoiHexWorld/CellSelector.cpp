#include <DraoiHexWorld/CellSelector.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DraoiHexWorld/WorldCellFields.h>

#include <cstring>
#include <string>
#include <utility>

namespace Draoi {
namespace HexWorld {

using Dia::Core::StringCRC;
using Dia::Core::Containers::String64;

namespace {

const StringCRC kDerivedSlot("derived");

// Wraps the base fields and additionally answers Has for slot "derived" names declared in the block.
class ValidationContext : public Dia::Condition::IConditionContext
{
public:
    ValidationContext(const WorldCellFields& base, const StringCRC* derived, uint32_t count)
        : mBase(base), mDerived(derived), mCount(count) {}

    float GetFloat(StringCRC slot, StringCRC field) const override { return slot == kDerivedSlot ? 0.0f : mBase.GetFloat(slot, field); }
    bool  GetBool (StringCRC slot, StringCRC field) const override { return slot == kDerivedSlot ? false : mBase.GetBool(slot, field); }
    bool  Has     (StringCRC slot, StringCRC field) const override
    {
        if (slot == kDerivedSlot)
        {
            for (uint32_t i = 0; i < mCount; ++i)
                if (mDerived[i] == field) return true;
            return false;
        }
        return mBase.Has(slot, field);
    }

private:
    const WorldCellFields& mBase;
    const StringCRC*       mDerived;
    uint32_t               mCount;
};

bool Fail(SelectorError& err, const char* kind, const char* item, const std::string& reason)
{
    err.kind.SetText(kind);
    err.item.SetText(item);
    err.reason.SetText(reason.c_str());
    return false;
}

// Collects the "field" of every leaf (any object with slot == "derived") under node, however it is nested.
void CollectDerivedLeaves(const Json::Value& node, std::string* names, uint32_t& count, uint32_t cap)
{
    if (node.isArray())
    {
        for (const Json::Value& e : node) CollectDerivedLeaves(e, names, count, cap);
    }
    else if (node.isObject())
    {
        if (node["slot"].isString() && node["field"].isString() && std::strcmp(node["slot"].asCString(), "derived") == 0 && count < cap)
            names[count++] = node["field"].asString();
        for (const std::string& key : node.getMemberNames())
            CollectDerivedLeaves(node[key], names, count, cap);
    }
}

} // namespace

CellSelectorSet::CellSelectorSet()
{
    mSelectors[0].id = StringCRC("all");
    mSelectors[0].name.SetText("all");
}

int CellSelectorSet::FindSelector(StringCRC id) const
{
    for (uint32_t i = 0; i < mCount; ++i)
        if (mSelectors[i].id == id) return static_cast<int>(i);
    return -1;
}

int CellSelectorSet::FindDerived(StringCRC id) const
{
    for (uint32_t i = 0; i < mDerivedCount; ++i)
        if (mDerived[i].id == id) return static_cast<int>(i);
    return -1;
}

bool CellSelectorSet::Load(const Json::Value& block, const WorldCellFields& baseFields, SelectorError& err)
{
    if (!block.isObject())
        return Fail(err, "block", "block", "selector block must be a JSON object");

    const Json::Value& jd = block["derived"];
    const Json::Value& js = block["selectors"];
    if (!jd.isNull() && !jd.isObject())
        return Fail(err, "derived", "derived", "'derived' must be an object");
    if (!js.isNull() && !js.isObject())
        return Fail(err, "selectors", "selectors", "'selectors' must be an object");
    if (js.isObject() && js.size() > static_cast<Json::ArrayIndex>(kMaxSelectors))
        return Fail(err, "selectors", "selectors", "too many selectors: " + std::to_string(js.size()) + " (max " + std::to_string(kMaxSelectors) + ")");
    if (jd.isObject() && jd.size() > static_cast<Json::ArrayIndex>(kMaxDerived))
        return Fail(err, "derived", "derived", "too many derived fields: " + std::to_string(jd.size()) + " (max " + std::to_string(kMaxDerived) + ")");

    CellSelectorSet tmp;

    // Derived definitions first, so selector validation can see the declared names.
    StringCRC derivedIds[kMaxDerived];
    if (jd.isObject())
    {
        for (const std::string& name : jd.getMemberNames())
        {
            if (name.empty() || name.size() >= String64::Capacity())
                return Fail(err, "derived", name.c_str(), "derived field name must be 1-63 characters");
            const Json::Value& def = jd[name];
            if (!def.isObject() || def.size() != 1)
                return Fail(err, "derived", name.c_str(), "derived field must be an object with exactly one kind (distance or upstream)");

            DerivedField& d = tmp.mDerived[tmp.mDerivedCount];
            d.name.SetText(name.c_str());
            d.id = StringCRC(name.c_str());

            const char* sourceKey = nullptr;
            if (def.isMember("distance")) { d.kind = DerivedKind::Distance; sourceKey = "from"; }
            else if (def.isMember("upstream")) { d.kind = DerivedKind::Upstream; sourceKey = "of"; }
            else return Fail(err, "derived", name.c_str(), "unsupported derived kind '" + def.getMemberNames()[0] + "'");

            const Json::Value& body = def[d.kind == DerivedKind::Distance ? "distance" : "upstream"];
            if (!body.isObject() || !body[sourceKey].isString() || body[sourceKey].asString().empty())
                return Fail(err, "derived", name.c_str(), std::string("missing selector name '") + sourceKey + "'");
            if (body[sourceKey].asString().size() >= String64::Capacity())
                return Fail(err, "derived", name.c_str(), std::string("'") + sourceKey + "' selector name too long");
            d.sourceName.SetText(body[sourceKey].asCString());

            if (d.kind == DerivedKind::Upstream)
            {
                const Json::Value& ms = body["maxSteps"];
                if (!ms.isIntegral())
                    return Fail(err, "derived", name.c_str(), "upstream requires an integer 'maxSteps'");
                const Json::Int64 v = ms.asInt64();
                if (v < 1 || v > 0x7fffffff)
                    return Fail(err, "derived", name.c_str(), "maxSteps must be >= 1, got " + std::to_string(v));
                d.maxSteps = static_cast<int32_t>(v);
            }
            derivedIds[tmp.mDerivedCount] = d.id;
            ++tmp.mDerivedCount;
        }
    }

    // Selectors.
    ValidationContext ctx(baseFields, derivedIds, tmp.mDerivedCount);
    std::string usedNames[kMaxDerived * 4];
    uint16_t selectorUses[kMaxSelectors + 1] = {};   // per selector: mask of derived fields its leaves read
    if (js.isObject())
    {
        for (const std::string& name : js.getMemberNames())
        {
            if (name.empty() || name.size() >= String64::Capacity())
                return Fail(err, "selector", name.c_str(), "selector name must be 1-63 characters");
            if (name == "all")
                return Fail(err, "selector", name.c_str(), "'all' is a reserved selector name");

            Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
            Selector& s = tmp.mSelectors[tmp.mCount];
            s.expr = Dia::Condition::ConditionExpr::LoadFromJson(js[name], errors);
            if (!s.expr.IsValid())
                return Fail(err, "selector", name.c_str(), errors.Size() > 0 ? errors[0u] : "invalid condition expression");
            if (!s.expr.Validate(ctx, errors))
                return Fail(err, "selector", name.c_str(), errors.Size() > 0 ? errors[0u] : "unresolvable slot/field");
            s.name.SetText(name.c_str());
            s.id = StringCRC(name.c_str());

            uint32_t used = 0;
            CollectDerivedLeaves(js[name], usedNames, used, kMaxDerived * 4);
            for (uint32_t u = 0; u < used; ++u)
                for (uint32_t i = 0; i < tmp.mDerivedCount; ++i)
                    if (tmp.mDerived[i].id == StringCRC(usedNames[u].c_str()))
                        selectorUses[tmp.mCount] |= static_cast<uint16_t>(1u << i);
            s.usesDerived = used > 0;
            ++tmp.mCount;
        }
    }

    // Resolve each derived field's source selector and its derived-field dependencies.
    for (uint32_t i = 0; i < tmp.mDerivedCount; ++i)
    {
        DerivedField& d = tmp.mDerived[i];
        const int sel = tmp.FindSelector(StringCRC(d.sourceName.AsCStr()));
        if (sel < 0)
            return Fail(err, "derived", d.name.AsCStr(), std::string("unknown selector '") + d.sourceName.AsCStr() + "'");
        d.sourceSelector = static_cast<uint32_t>(sel);
        d.dependsMask = selectorUses[sel];
    }

    // Cycle detection + topological order (dependencies first) by DFS over derived -> (selector) -> derived.
    enum : uint8_t { White, Grey, Black };
    uint8_t  color[kMaxDerived] = {};
    uint32_t stack[kMaxDerived + 1];
    uint32_t nextBit[kMaxDerived + 1];
    uint32_t orderCount = 0;
    for (uint32_t root = 0; root < tmp.mDerivedCount; ++root)
    {
        if (color[root] != White) continue;
        uint32_t depth = 0;
        stack[0] = root; nextBit[0] = 0; color[root] = Grey;
        while (true)
        {
            const uint32_t cur = stack[depth];
            uint32_t& bit = nextBit[depth];
            while (bit < tmp.mDerivedCount && !tmp.mDerived[cur].DependsOn(bit)) ++bit;
            if (bit < tmp.mDerivedCount)
            {
                const uint32_t dep = bit++;
                if (color[dep] == Grey)
                {
                    std::string path;
                    uint32_t k = 0;
                    while (stack[k] != dep) ++k;
                    for (; k <= depth; ++k)
                        path += std::string(tmp.mDerived[stack[k]].name.AsCStr()) + " -> (" + tmp.mDerived[stack[k]].sourceName.AsCStr() + ") -> ";
                    path += tmp.mDerived[dep].name.AsCStr();
                    return Fail(err, "derived", tmp.mDerived[dep].name.AsCStr(), "cycle: " + path);
                }
                if (color[dep] == White)
                {
                    ++depth; stack[depth] = dep; nextBit[depth] = 0; color[dep] = Grey;
                }
            }
            else
            {
                color[cur] = Black;
                tmp.mOrder[orderCount++] = cur;
                if (depth == 0) break;
                --depth;
            }
        }
    }

    *this = std::move(tmp);
    return true;
}

void CellSelectorSet::ClearResolved()
{
    for (Resolved& r : mResolved) r = Resolved{};
    mPool.reset();
}

bool CellSelectorSet::Resolve(WorldCellFields& fields, uint32_t cellCount, SelectorError& err)
{
    ClearResolved();
    for (uint32_t s = 1; s < mCount; ++s)
        if (mSelectors[s].usesDerived)
            return Fail(err, "resolve", mSelectors[s].name.AsCStr(), "selector reads derived fields; derived values are not computed by this Resolve overload");

    // Value-initialised so unused tail bytes are deterministic (byte-identical pools). Never null, so count-0 lists are safe.
    mPool.reset(new uint32_t[static_cast<size_t>(mCount) * cellCount + 1]());

    const uint32_t savedCell = fields.GetCell();
    for (uint32_t s = 0; s < mCount; ++s)
    {
        uint32_t* out = mPool.get() + static_cast<size_t>(s) * cellCount;
        uint32_t n = 0;
        if (s == 0)
        {
            for (uint32_t i = 0; i < cellCount; ++i) out[n++] = i;
        }
        else
        {
            const Dia::Condition::ConditionExpr& expr = mSelectors[s].expr;
            for (uint32_t i = 0; i < cellCount; ++i)
            {
                fields.SetCell(i);
                if (expr.Evaluate(fields)) out[n++] = i;
            }
        }
        mResolved[s].offset = static_cast<uint32_t>(static_cast<size_t>(s) * cellCount);
        mResolved[s].count = n;
    }
    fields.SetCell(savedCell);
    return true;
}

} // namespace HexWorld
} // namespace Draoi
