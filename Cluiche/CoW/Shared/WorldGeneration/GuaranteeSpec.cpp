#include <Shared/WorldGeneration/GuaranteeSpec.h>

#include <DiaAttribute/ParameterFile.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>

namespace CoW {
namespace WorldGeneration {

namespace
{
    bool Fail(GuaranteeSpecError& err, const std::string& entry, const std::string& reason)
    {
        err.entry = entry;
        err.reason = reason;
        DIA_LOG_ERROR("CoW", "GuaranteeSpec: load failed at '%s': %s", entry.c_str(), reason.c_str());
        return false;
    }

    bool ParseShape(const std::string& s, SiteShape& out)
    {
        static const SiteShape all[] = { SiteShape::Dot, SiteShape::Diamond, SiteShape::Square, SiteShape::Triangle,
                                         SiteShape::Star, SiteShape::Ring };
        for (SiteShape shape : all)
        {
            if (s == SiteShapeName(shape)) { out = shape; return true; }
        }
        return false;
    }

    // "#RRGGBB" or "RRGGBB".
    bool ParseColour(const std::string& s, uint32_t& out)
    {
        const size_t at = (!s.empty() && s[0] == '#') ? 1 : 0;
        if (s.size() - at != 6) { return false; }
        uint32_t v = 0;
        for (size_t i = at; i < s.size(); ++i)
        {
            const char c = s[i];
            uint32_t d;
            if (c >= '0' && c <= '9') d = static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') d = static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') d = static_cast<uint32_t>(c - 'A' + 10);
            else return false;
            v = v * 16 + d;
        }
        out = v;
        return true;
    }

    // Rejects any member of 'obj' not in the allowed list.
    bool OnlyKeys(const Json::Value& obj, std::initializer_list<const char*> allowed, std::string& bad)
    {
        for (const std::string& name : obj.getMemberNames())
        {
            bool ok = false;
            for (const char* a : allowed) { if (name == a) { ok = true; break; } }
            if (!ok) { bad = name; return false; }
        }
        return true;
    }

    bool NonEmptyString(const Json::Value& obj, const char* key, std::string& out)
    {
        const Json::Value& v = obj[key];
        if (!v.isString() || v.asString().empty()) { return false; }
        out = v.asString();
        return true;
    }

    // Reads each required string field into its destination; names the first bad one.
    struct StringField { const char* key; std::string* dest; };
    bool ReadStrings(const Json::Value& obj, std::initializer_list<StringField> fields, std::string& bad)
    {
        for (const StringField& f : fields)
        {
            if (!NonEmptyString(obj, f.key, *f.dest)) { bad = f.key; return false; }
        }
        return true;
    }

    bool KindDeclared(const std::vector<SiteKindDecl>& kinds, const std::string& id)
    {
        for (const SiteKindDecl& k : kinds) { if (k.id == id) { return true; } }
        return false;
    }
}

SiteKindInfo SiteKindDecl::ToInfo() const
{
    return MakeSiteKindInfo(Dia::Core::StringCRC(id.c_str()), displayName.c_str(), colour, shape, variantLabel.c_str());
}

const char* GuaranteeRuleTypeName(GuaranteeRuleType type)
{
    switch (type)
    {
    case GuaranteeRuleType::ReachAny:     return "reach_any";
    case GuaranteeRuleType::MinSpacing:   return "min_spacing";
    case GuaranteeRuleType::VariantCover: return "variant_cover";
    }
    return "";
}

const SiteKindDecl* GuaranteeSpec::FindKind(const std::string& id) const
{
    for (const SiteKindDecl& k : siteKinds) { if (k.id == id) { return &k; } }
    return nullptr;
}

const GuaranteeEntry* GuaranteeSpec::FindGuarantee(const std::string& id) const
{
    for (const GuaranteeEntry& g : guarantees) { if (g.id == id) { return &g; } }
    return nullptr;
}

bool LoadGuaranteeSpec(const char* text, const char* sidecarText, const Draoi::HexWorld::StratumTable& strata, GuaranteeSpec& out,
                       GuaranteeSpecError& err)
{
    err = GuaranteeSpecError();
    if (text == nullptr) { return Fail(err, "document", "no guarantees text"); }
    if (sidecarText == nullptr) { return Fail(err, "sidecar", "no guarantees sidecar text"); }

    Json::Value root;
    {
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["rejectDupKeys"] = true;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string errors;
        bool ok = false;
        try { ok = reader->parse(text, text + std::strlen(text), &root, &errors); }
        catch (const std::exception&) { ok = false; }
        if (!ok) { return Fail(err, "document", "not valid JSON"); }
    }
    if (!root.isObject() || !root["format"].isString() || root["format"].asString() != "cow.guarantees")
    {
        return Fail(err, "format", "format must be \"cow.guarantees\"");
    }
    if (!root["version"].isUInt() || root["version"].asUInt() != 1) { return Fail(err, "version", "unsupported version"); }
    {
        std::string bad;
        if (!OnlyKeys(root, { "format", "version", "site_kinds", "guarantees", "marker_overlay" }, bad))
        {
            return Fail(err, "document", "unknown field '" + bad + "'");
        }
    }

    Dia::Attribute::ParameterFile file;
    {
        Dia::Attribute::ParamError perr;
        if (!file.Load(sidecarText, "guarantees.schema.json", nullptr, perr))
        {
            return Fail(err, "sidecar", perr.reason.AsCStr());
        }
    }

    GuaranteeSpec cand;

    // ---- site kinds ----
    const Json::Value& kinds = root["site_kinds"];
    if (!kinds.isNull() && !kinds.isArray()) { return Fail(err, "site_kinds", "site_kinds must be an array"); }
    for (Json::ArrayIndex i = 0; i < kinds.size(); ++i)
    {
        const Json::Value& k = kinds[i];
        const std::string where = "site_kinds[" + std::to_string(i) + "]";
        if (!k.isObject()) { return Fail(err, where, "each site kind must be an object"); }
        SiteKindDecl decl;
        if (!NonEmptyString(k, "id", decl.id)) { return Fail(err, where, "id must be a non-empty string"); }
        const std::string name = "site_kinds[" + decl.id + "]";
        std::string bad;
        if (!OnlyKeys(k, { "id", "display_name", "colour", "shape", "variant_label", "variant_count" }, bad))
        {
            return Fail(err, name, "unknown field '" + bad + "'");
        }
        if (decl.id.size() >= sizeof(SiteKindInfo::id)) { return Fail(err, name, "id is too long"); }
        if (cand.FindKind(decl.id) != nullptr) { return Fail(err, name, "duplicate kind id"); }
        if (!NonEmptyString(k, "display_name", decl.displayName)) { return Fail(err, name, "display_name must be a non-empty string"); }
        if (decl.displayName.size() >= sizeof(SiteKindInfo::displayName)) { return Fail(err, name, "display_name is too long"); }
        if (!k["colour"].isString() || !ParseColour(k["colour"].asString(), decl.colour))
        {
            return Fail(err, name, "colour must be a \"#RRGGBB\" string");
        }
        if (!k["shape"].isString() || !ParseShape(k["shape"].asString(), decl.shape))
        {
            return Fail(err, name, "bad shape (Dot, Diamond, Square, Triangle, Star, Ring)");
        }
        if (!k["variant_label"].isNull())
        {
            if (!k["variant_label"].isString()) { return Fail(err, name, "variant_label must be a string"); }
            decl.variantLabel = k["variant_label"].asString();
            if (decl.variantLabel.size() >= sizeof(SiteKindInfo::variantLabel)) { return Fail(err, name, "variant_label is too long"); }
        }
        if (!k["variant_count"].isNull())
        {
            if (!k["variant_count"].isUInt() || k["variant_count"].asUInt() > 64) { return Fail(err, name, "variant_count must be a whole number 0..64"); }
            decl.variantCount = k["variant_count"].asUInt();
        }
        cand.siteKinds.push_back(decl);
    }

    // ---- guarantees ----
    const Json::Value& gs = root["guarantees"];
    if (!gs.isNull() && !gs.isArray()) { return Fail(err, "guarantees", "guarantees must be an array"); }
    for (Json::ArrayIndex i = 0; i < gs.size(); ++i)
    {
        const Json::Value& g = gs[i];
        const std::string where = "guarantees[" + std::to_string(i) + "]";
        if (!g.isObject()) { return Fail(err, where, "each guarantee must be an object"); }
        GuaranteeEntry e;
        if (!NonEmptyString(g, "id", e.id)) { return Fail(err, where, "id must be a non-empty string"); }
        const std::string name = "guarantees[" + e.id + "]";
        if (cand.FindGuarantee(e.id) != nullptr) { return Fail(err, name, "duplicate guarantee id"); }
        if (!NonEmptyString(g, "label", e.label)) { return Fail(err, name, "label must be a non-empty string"); }
        std::string typeText;
        if (!NonEmptyString(g, "type", typeText)) { return Fail(err, name, "type must be a non-empty string"); }

        std::string bad;
        auto needKind = [&](const std::string& id, const char* field) -> bool
        {
            if (!KindDeclared(cand.siteKinds, id))
            {
                Fail(err, name, std::string(field) + " '" + id + "' is not a declared site kind");
                return false;
            }
            return true;
        };
        auto resolveZone = [&]() -> bool
        {
            const int index = strata.IndexOf(e.toStratum.c_str());
            if (index < 1) { Fail(err, name, "unknown stratum '" + e.toStratum + "'"); return false; }
            e.toStratumIndex = index;
            return true;
        };
        auto badField = [&](const std::string& field) { return Fail(err, name, "missing or bad field '" + field + "'"); };

        if (typeText == "reach_any")
        {
            e.type = GuaranteeRuleType::ReachAny;
            if (!OnlyKeys(g, { "id", "label", "type", "from", "to", "to_stratum", "noun", "to_noun", "to_noun_plural" }, bad))
            {
                return Fail(err, name, "unknown field '" + bad + "'");
            }
            if (!ReadStrings(g, { { "from", &e.from }, { "noun", &e.noun }, { "to_noun", &e.toNoun }, { "to_noun_plural", &e.toNounPlural } }, bad))
            {
                return badField(bad);
            }
            const bool hasTo = !g["to"].isNull();
            const bool hasZone = !g["to_stratum"].isNull();
            if (hasTo == hasZone) { return Fail(err, name, "exactly one of 'to' and 'to_stratum' is required"); }
            if (!needKind(e.from, "from")) { return false; }
            if (hasTo)
            {
                if (!NonEmptyString(g, "to", e.to)) { return badField("to"); }
                if (!needKind(e.to, "to")) { return false; }
            }
            else
            {
                if (!NonEmptyString(g, "to_stratum", e.toStratum)) { return badField("to_stratum"); }
                if (!resolveZone()) { return false; }
            }
        }
        else if (typeText == "min_spacing")
        {
            e.type = GuaranteeRuleType::MinSpacing;
            if (!OnlyKeys(g, { "id", "label", "type", "kind", "minimum_param", "noun", "noun_plural", "minimum_key", "distance_key" }, bad))
            {
                return Fail(err, name, "unknown field '" + bad + "'");
            }
            std::string param;
            if (!ReadStrings(g, { { "kind", &e.kind }, { "minimum_param", &param }, { "noun", &e.noun }, { "noun_plural", &e.nounPlural },
                                  { "minimum_key", &e.minimumKey }, { "distance_key", &e.distanceKey } }, bad))
            {
                return badField(bad);
            }
            if (!needKind(e.kind, "kind")) { return false; }
            if (file.Schema().FindAttribute(Dia::Core::StringCRC(param.c_str())) == nullptr)
            {
                return Fail(err, name, "minimum_param '" + param + "' is not in the sidecar");
            }
            e.minimum = file.Values().GetBaseValue(Dia::Core::StringCRC(param.c_str()));
            if (!(e.minimum > 0.0f)) { return Fail(err, name, "minimum must be greater than zero"); }
        }
        else if (typeText == "variant_cover")
        {
            e.type = GuaranteeRuleType::VariantCover;
            if (!OnlyKeys(g, { "id", "label", "type", "kind", "variant_count", "to_stratum", "noun", "noun_plural", "variant_noun",
                               "target_noun", "count_key", "target_count_key" }, bad))
            {
                return Fail(err, name, "unknown field '" + bad + "'");
            }
            if (!ReadStrings(g, { { "kind", &e.kind }, { "to_stratum", &e.toStratum }, { "noun", &e.noun }, { "noun_plural", &e.nounPlural },
                                  { "variant_noun", &e.variantNoun }, { "target_noun", &e.targetNoun }, { "count_key", &e.countKey },
                                  { "target_count_key", &e.targetCountKey } }, bad))
            {
                return badField(bad);
            }
            if (!g["variant_count"].isUInt() || g["variant_count"].asUInt() < 1 || g["variant_count"].asUInt() > 64)
            {
                return Fail(err, name, "variant_count must be a whole number 1..64");
            }
            e.variantCount = g["variant_count"].asUInt();
            if (!needKind(e.kind, "kind")) { return false; }
            if (!resolveZone()) { return false; }
        }
        else
        {
            return Fail(err, name, "unknown rule type '" + typeText + "'");
        }
        cand.guarantees.push_back(e);
    }

    // ---- marker overlay ----
    const Json::Value& m = root["marker_overlay"];
    if (!m.isNull())
    {
        const std::string name = "marker_overlay";
        if (!m.isObject()) { return Fail(err, name, "marker_overlay must be an object"); }
        std::string bad;
        if (!OnlyKeys(m, { "overlay_id", "label", "kind", "spacing_guarantee", "fallback_ring" }, bad))
        {
            return Fail(err, name, "unknown field '" + bad + "'");
        }
        MarkerOverlaySpec o;
        if (!ReadStrings(m, { { "overlay_id", &o.overlayId }, { "label", &o.label }, { "kind", &o.kind },
                              { "spacing_guarantee", &o.spacingGuarantee } }, bad))
        {
            return Fail(err, name, "missing or bad field '" + bad + "'");
        }
        if (!KindDeclared(cand.siteKinds, o.kind)) { return Fail(err, name, "kind '" + o.kind + "' is not a declared site kind"); }
        const GuaranteeEntry* sp = cand.FindGuarantee(o.spacingGuarantee);
        if (sp == nullptr || sp->type != GuaranteeRuleType::MinSpacing)
        {
            return Fail(err, name, "spacing_guarantee '" + o.spacingGuarantee + "' is not a declared min_spacing guarantee");
        }
        if (!m["fallback_ring"].isNumeric() || !(m["fallback_ring"].asFloat() > 0.0f))
        {
            return Fail(err, name, "fallback_ring must be a number greater than zero");
        }
        o.fallbackRing = m["fallback_ring"].asFloat();
        cand.hasMarkerOverlay = true;
        cand.markerOverlay = o;
    }

    out = std::move(cand);
    DIA_LOG_INFO("CoW", "GuaranteeSpec: loaded %u site kinds, %u guarantees", static_cast<unsigned>(out.siteKinds.size()),
                 static_cast<unsigned>(out.guarantees.size()));
    return true;
}

bool InstallSiteKinds(const GuaranteeSpec& spec, SiteRegistry& registry, bool skipExisting)
{
    for (const SiteKindDecl& k : spec.siteKinds)
    {
        if (skipExisting && registry.FindInfo(Dia::Core::StringCRC(k.id.c_str())) != nullptr)
            continue;   // a placer (or an earlier install) already owns the id
        if (!registry.RegisterDeclaredKind(k.ToInfo()))
        {
            DIA_LOG_ERROR("CoW", "GuaranteeSpec: site kind '%s' is already registered", k.id.c_str());
            return false;
        }
    }
    return true;
}

} // namespace WorldGeneration
} // namespace CoW
