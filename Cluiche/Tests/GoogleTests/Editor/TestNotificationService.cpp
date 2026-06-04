#include <gtest/gtest.h>
#include <DiaEditor/Notification/NotificationService.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::Editor;
using namespace Dia::Core;

// ---------------------------------------------------------------------------
// kUniqueId
// ---------------------------------------------------------------------------

TEST(NotificationService, UniqueId_MatchesExpectedString)
{
    EXPECT_EQ(NotificationService::kUniqueId, StringCRC("NotificationService"));
}

TEST(NotificationService, UniqueId_IsDistinctFromOtherServices)
{
    EXPECT_NE(NotificationService::kUniqueId, StringCRC("SomethingElse"));
    EXPECT_NE(NotificationService::kUniqueId, StringCRC("WebUIBridge"));
    EXPECT_NE(NotificationService::kUniqueId, StringCRC("PluginServiceLocator"));
}

// ---------------------------------------------------------------------------
// Null-bridge safety (no Initialize called — mBridge is uninitialised raw ptr)
// We call Initialize(nullptr) explicitly so mBridge is a known-null state.
// ---------------------------------------------------------------------------

TEST(NotificationService, Push_AfterInitializeNull_DoesNotCrash_InfoLevel)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kInfo, "Hello", nullptr, 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}

TEST(NotificationService, Push_AfterInitializeNull_DoesNotCrash_SuccessLevel)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kSuccess, "Done", "Operation complete", 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}

TEST(NotificationService, Push_AfterInitializeNull_DoesNotCrash_WarningLevel)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kWarning, "Caution", "Check your settings", 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}

TEST(NotificationService, Push_AfterInitializeNull_DoesNotCrash_ErrorLevel)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kError, "Failed", "Something went wrong", 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}

TEST(NotificationService, Push_AfterInitializeNull_WithExplicitDuration_DoesNotCrash)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kInfo, "Timed", "Will auto-dismiss", 5.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}

TEST(NotificationService, DismissAll_AfterInitializeNull_DoesNotCrash)
{
    NotificationService service;
    service.Initialize(nullptr);

    EXPECT_NO_FATAL_FAILURE(service.DismissAll());
}

// ---------------------------------------------------------------------------
// Multiple calls — counter advances without crash
// ---------------------------------------------------------------------------

TEST(NotificationService, MultiplePushCalls_DoNotCrash)
{
    NotificationService service;
    service.Initialize(nullptr);

    const NotificationLevel levels[] = {
        NotificationLevel::kInfo,
        NotificationLevel::kSuccess,
        NotificationLevel::kWarning,
        NotificationLevel::kError
    };

    for (int i = 0; i < 8; ++i)
    {
        NotificationRequest req{ levels[i % 4], "Title", "Message", static_cast<float>(i) };
        EXPECT_NO_FATAL_FAILURE(service.Push(req));
    }
}

TEST(NotificationService, InterleavedPushAndDismissAll_DoNotCrash)
{
    NotificationService service;
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kInfo, "A", nullptr, 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
    EXPECT_NO_FATAL_FAILURE(service.DismissAll());
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
    EXPECT_NO_FATAL_FAILURE(service.DismissAll());
}

// ---------------------------------------------------------------------------
// Re-initialize (second call to Initialize should silently overwrite, no crash)
// ---------------------------------------------------------------------------

TEST(NotificationService, InitializeTwice_DoesNotCrash)
{
    NotificationService service;
    service.Initialize(nullptr);
    service.Initialize(nullptr);

    NotificationRequest req{ NotificationLevel::kInfo, "Re-init test", nullptr, 0.0f };
    EXPECT_NO_FATAL_FAILURE(service.Push(req));
}
