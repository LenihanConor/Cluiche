

#include <Awesomium/WebCore.h>
#include <DiaUI/BoundMethod.h>
#include <DiaCore/Containers/Misc/FastDelegate.h>

// Callbacks wrapped via JSDelegate should have the following function signature:
typedef Dia::UI::BoundMethod::MethodPtr JSDelegate;

// Callbacks wrapped via JSDelegateWithRetval should have the following function signature:
typedef Dia::UI::BoundMethod::MethodPtrWithRetVal JSDelegateWithRetval;


