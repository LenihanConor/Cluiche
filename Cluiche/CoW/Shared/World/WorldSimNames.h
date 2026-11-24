#pragma once

#include <stddef.h>

namespace CoW {
namespace World {

// Builds every world-sim name from a system token (worldgen, weather, hydro, ecology, succession).
// Results live in a fixed buffer (no STL). Any invalid input (unknown token, non-snake_case field,
// overflow past the 192-char name cap) yields an empty Name.
struct Name
{
    static const size_t kCapacity = 192;
    char text[kCapacity];

    Name() { text[0] = '\0'; }
    const char* c_str() const { return text; }
    bool IsEmpty() const { return text[0] == '\0'; }
};

constexpr const char* kWorldgen   = "worldgen";
constexpr const char* kWeather    = "weather";
constexpr const char* kHydro      = "hydro";
constexpr const char* kEcology    = "ecology";
constexpr const char* kSuccession = "succession";

// The WorldStepModule StepChainConfig order: stage ids are the system tokens.
constexpr const char* kStageOrder[] = { kWeather, kHydro, kEcology };
constexpr size_t kStageOrderCount = sizeof(kStageOrder) / sizeof(kStageOrder[0]);

namespace Detail {

inline bool StrEq(const char* a, const char* b)
{
    while (*a != '\0' && *a == *b) { ++a; ++b; }
    return *a == *b;
}

// Appends s to n; returns false (and leaves n empty) on overflow.
inline bool Append(Name& n, size_t& len, const char* s)
{
    while (*s != '\0')
    {
        if (len + 1 >= Name::kCapacity) { n.text[0] = '\0'; return false; }
        n.text[len++] = *s++;
    }
    n.text[len] = '\0';
    return true;
}

// Appends the token with its first letter capitalised (directory form).
inline bool AppendCapitalised(Name& n, size_t& len, const char* token)
{
    if (*token == '\0') { return true; }
    if (len + 1 >= Name::kCapacity) { n.text[0] = '\0'; return false; }
    const char c = *token;
    n.text[len++] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    n.text[len] = '\0';
    return Append(n, len, token + 1);
}

}  // namespace Detail

inline bool IsKnownToken(const char* token)
{
    if (token == nullptr) { return false; }
    return Detail::StrEq(token, kWorldgen) || Detail::StrEq(token, kWeather) || Detail::StrEq(token, kHydro)
        || Detail::StrEq(token, kEcology) || Detail::StrEq(token, kSuccession);
}

// Non-empty, lowercase letters, digits and underscores only.
inline bool IsSnakeCase(const char* s)
{
    if (s == nullptr || *s == '\0') { return false; }
    for (; *s != '\0'; ++s)
    {
        const char c = *s;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) { return false; }
    }
    return true;
}

namespace Detail {

// Concatenates up to five parts; part 0 sys is validated by callers.
inline Name Build(const char* a, const char* b = "", const char* c = "", const char* d = "", const char* e = "")
{
    Name n;
    size_t len = 0;
    if (!Append(n, len, a) || !Append(n, len, b) || !Append(n, len, c) || !Append(n, len, d) || !Append(n, len, e))
    {
        return Name();
    }
    return n;
}

}  // namespace Detail

inline Name LayerId(const char* sys, const char* field)
{
    if (!IsKnownToken(sys) || !IsSnakeCase(field)) { return Name(); }
    return Detail::Build(sys, ".", field);
}

inline Name LayerSchemaKey(const char* sys, const char* field)
{
    if (!IsKnownToken(sys) || !IsSnakeCase(field)) { return Name(); }
    return Detail::Build("cow.", sys, ".layers/", field);
}

// Param keys may be dotted (snow.melt_threshold); they must be non-empty.
inline Name ParamSchemaKey(const char* sys, const char* key)
{
    if (!IsKnownToken(sys) || key == nullptr || *key == '\0') { return Name(); }
    return Detail::Build("cow.", sys, ".params/", key);
}

inline Name CommandName(const char* sys, const char* verb)
{
    if (!IsKnownToken(sys) || !IsSnakeCase(verb)) { return Name(); }
    return Detail::Build("cow.", sys, ".", verb);
}

inline Name SaveId(const char* sys)
{
    if (!IsKnownToken(sys)) { return Name(); }
    return Detail::Build("cow.", sys);
}

inline Name StateFormat(const char* sys)
{
    if (!IsKnownToken(sys)) { return Name(); }
    return Detail::Build("cow.", sys, "_state");
}

inline Name ParamsFormat(const char* sys)
{
    if (!IsKnownToken(sys)) { return Name(); }
    return Detail::Build("cow.", sys, "_params");
}

inline Name PresetFormat(const char* sys)
{
    if (!IsKnownToken(sys)) { return Name(); }
    return Detail::Build("cow.", sys, "_preset");
}

inline Name PresetPath(const char* sys, const char* id)
{
    if (!IsKnownToken(sys) || id == nullptr || *id == '\0') { return Name(); }
    Name n;
    size_t len = 0;
    if (!Detail::Append(n, len, "Global/") || !Detail::AppendCapitalised(n, len, sys)
        || !Detail::Append(n, len, "/Presets/") || !Detail::Append(n, len, id)
        || !Detail::Append(n, len, ".") || !Detail::Append(n, len, sys) || !Detail::Append(n, len, "_preset.json"))
    {
        return Name();
    }
    return n;
}

inline Name StageId(const char* sys)
{
    if (!IsKnownToken(sys)) { return Name(); }
    return Detail::Build(sys);
}

inline Name MetricName(const char* sys, const char* metric)
{
    if (!IsKnownToken(sys) || !IsSnakeCase(metric)) { return Name(); }
    return Detail::Build("cow.", sys, ".", metric);
}

}  // namespace World
}  // namespace CoW
