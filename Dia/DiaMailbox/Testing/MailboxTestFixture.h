#pragma once
#include <gtest/gtest.h>
#include <DiaMailbox/Mailbox.h>
#include <string>
#include <vector>

namespace Dia::Mailbox {

    // -----------------------------------------------------------------------
    // MailboxFixture
    //
    // GoogleTest fixture that owns a Mailbox and captures any warnings it
    // emits.  Uses a static pointer trick so the raw function-pointer
    // callback can reach the current test instance.
    //
    // STL (std::string, std::vector) is intentionally used here — this is a
    // test utility, not a public API header.
    // -----------------------------------------------------------------------
    class MailboxFixture : public ::testing::Test {
    public:
        Dia::Mailbox::Mailbox mailbox;
        std::vector<std::string> capturedWarnings;

        static MailboxFixture* sCurrentFixture;

        static void WarnSpy(const char* msg) {
            if (sCurrentFixture != nullptr) {
                sCurrentFixture->capturedWarnings.push_back(msg);
            }
        }

        bool HasWarningContaining(const char* fragment) const {
            for (const auto& w : capturedWarnings) {
                if (w.find(fragment) != std::string::npos) {
                    return true;
                }
            }
            return false;
        }

    protected:
        void SetUp() override {
            sCurrentFixture = this;
            capturedWarnings.clear();
            mailbox.SetWarnCallback(&WarnSpy);
        }

        void TearDown() override {
            mailbox.SetWarnCallback(nullptr);
            sCurrentFixture = nullptr;
        }
    };

    // Definition of the static member — include this header in exactly one
    // .cpp per test binary (TypedQueueTests.cpp defines it).
    // Declared here so the linker can find it.

} // namespace Dia::Mailbox
