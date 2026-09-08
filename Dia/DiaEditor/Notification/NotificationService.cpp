#include <DiaEditor/Notification/NotificationService.h>
#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Log/DiaLog.h>

#include <cstdio>

namespace Dia::Editor {

    const Dia::Core::StringCRC NotificationService::kUniqueId("NotificationService");

    void NotificationService::Initialize(WebUIBridge* bridge) {
        mBridge = bridge;
    }

    void NotificationService::Push(const NotificationRequest& request) {
        if (mBridge == nullptr) {
            return;
        }

        static unsigned int sNextId = 0;
        char idBuf[16];
        snprintf(idBuf, sizeof(idBuf), "t%u", sNextId++);

        const char* levelStr = "info";
        switch (request.level) {
            case NotificationLevel::kInfo:    levelStr = "info";    break;
            case NotificationLevel::kSuccess: levelStr = "success"; break;
            case NotificationLevel::kWarning: levelStr = "warning"; break;
            case NotificationLevel::kError:   levelStr = "error";   break;
        }

        float duration = request.durationSeconds;
        if (duration <= 0.0f) {
            switch (request.level) {
                case NotificationLevel::kInfo:    duration = 4.0f; break;
                case NotificationLevel::kSuccess: duration = 3.0f; break;
                case NotificationLevel::kWarning: duration = 6.0f; break;
                case NotificationLevel::kError:   duration = 0.0f; break;
            }
        }

        Json::Value payload;
        payload["id"]       = idBuf;
        payload["level"]    = levelStr;
        payload["title"]    = request.title;
        if (request.message != nullptr) {
            payload["message"] = request.message;
        }
        payload["duration"] = duration;

        mBridge->NotifyUIDataChanged("editor.notification", payload);
        DIA_LOG_INFO("Editor", "NotificationService: [%s] %s — %s", levelStr, request.title, idBuf);
    }

    void NotificationService::DismissAll() {
        if (mBridge == nullptr) {
            return;
        }

        Json::Value payload;
        payload["dismissAll"] = true;

        mBridge->NotifyUIDataChanged("editor.notification.dismiss_all", payload);
        DIA_LOG_INFO("Editor", "NotificationService: DismissAll");
    }

}
