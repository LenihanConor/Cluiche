#pragma once

#include <string>

namespace Cluiche { namespace AppFlow {

// Application identity derived by convention from the executable name.
// App "CoW" (CoW.exe) -> observation asset ".../Assets/CoW/cow.diaobservation", "cow.diagame".
struct AppIdentity
{
    std::string name;                   // exe stem, e.g. "CluicheTest"
    std::string lower;                  // lower-cased stem, e.g. "cluichetest"
    std::string observationAssetRelPath;
    std::string diagameFileName;
};

// Pure: stem of the path (either separator, extension stripped). Null/empty -> "CluicheTest".
AppIdentity AppIdentityFromExePath(const char* exePath);

// Identity of the running process, computed once from __argv[0]. Lives for the process lifetime,
// so c_str() pointers may be handed to long-lived consumers.
const AppIdentity& CurrentAppIdentity();

} } // namespace Cluiche::AppFlow
