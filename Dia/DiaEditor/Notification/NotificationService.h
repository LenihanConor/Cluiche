#pragma once

#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Editor {

    class WebUIBridge;

    enum class NotificationLevel { kInfo, kSuccess, kWarning, kError };

    struct NotificationRequest {
        NotificationLevel level;
        const char* title;
        const char* message;
        float durationSeconds;
    };

    class NotificationService {
    public:
        static const Dia::Core::StringCRC kUniqueId;

        void Initialize(WebUIBridge* bridge);
        void Push(const NotificationRequest& request);
        void DismissAll();

    private:
        WebUIBridge* mBridge;
    };

}
