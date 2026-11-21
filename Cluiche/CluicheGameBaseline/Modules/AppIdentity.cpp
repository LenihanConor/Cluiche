#include "Modules/AppIdentity.h"

#include <cctype>
#include <cstdlib>

namespace Cluiche { namespace AppFlow {

AppIdentity AppIdentityFromExePath(const char* exePath)
{
    std::string stem = exePath ? exePath : "";

    const size_t slash = stem.find_last_of("/\\");
    if (slash != std::string::npos)
        stem.erase(0, slash + 1);

    const size_t dot = stem.find_last_of('.');
    if (dot != std::string::npos && dot > 0)
        stem.erase(dot);

    if (stem.empty())
        stem = "CluicheTest";   // legacy fallback

    AppIdentity id;
    id.name = stem;
    id.lower = stem;
    for (char& c : id.lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    id.observationAssetRelPath = "../../../../Assets/" + id.name + "/" + id.lower + ".diaobservation";
    id.diagameFileName = id.lower + ".diagame";
    return id;
}

const AppIdentity& CurrentAppIdentity()
{
    // Function-local static (not a singleton service): computed once from argv[0]
    // so c_str() pointers stay valid for the process lifetime.
    static const AppIdentity sIdentity =
        AppIdentityFromExePath(__argc > 0 && __argv ? __argv[0] : nullptr);
    return sIdentity;
}

} } // namespace Cluiche::AppFlow
