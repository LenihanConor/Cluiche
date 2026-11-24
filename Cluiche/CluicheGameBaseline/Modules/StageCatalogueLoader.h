#pragma once

#include <Types/StageCatalogue.h>

namespace Cluiche { namespace AppFlow {

enum class StageCatalogueResult : uint8_t { kOk, kMissingDiagame, kMalformed, kNoRootManifest };
const char* StageCatalogueResultName(StageCatalogueResult result);

// Pure loader: reads a .diagame, its root .diaapp (Boot's transitions give order) and imported .diastage files.
// Behaviourally identical to Dia/DiaCLI/dia_cli/utils/stage_catalogue.py (shared fixtures).
class StageCatalogueLoader {
public:
    // `out` is cleared first and only populated when the result is kOk.
    static StageCatalogueResult Load(const char* diagamePath, StageCatalogue& out);
};

}} // namespace Cluiche::AppFlow
