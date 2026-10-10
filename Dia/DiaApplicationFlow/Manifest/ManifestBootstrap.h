#pragma once
#include <DiaApplicationFlow/Manifest/ApplicationManifestV3.h>

namespace Dia { namespace ApplicationFlow {

    class TypeRegistry;

    // Registers diagameConfig["path_aliases"] into PathStore, resolving each path
    // relative to the directory containing diagamePath. No-op if the manifest has
    // no diagameConfig or no path_aliases object. Must run before any module
    // resolves a FilePath.
    void RegisterPathAliases(const ApplicationManifestV3& manifest, const char* diagamePath);

    // Compose the .diagame (with stage imports), register its path aliases, and
    // validate against the registry. Prints errors and returns false on failure.
    // Frame rate, loop and Application construction stay with the calling app.
    bool LoadManifest(const char* diagamePath, ApplicationManifestV3& outManifest, TypeRegistry& registry);

}} // namespace Dia::ApplicationFlow
