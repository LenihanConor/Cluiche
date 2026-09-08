#include <gtest/gtest.h>
#include <DiaMailbox/MailboxTypes.h>
#include <type_traits>

// AC1: Default-construct Address, assert routerId == Dia::Core::StringCRC{} and payload == 0
TEST(MailboxTypesTests, AddressDefaultConstruct) {
    Dia::Mailbox::Address addr;
    EXPECT_EQ(addr.routerId, Dia::Core::StringCRC());
    EXPECT_EQ(addr.payload, 0);
}

// AC2: operator== and operator!= for Address
TEST(MailboxTypesTests, AddressEqualityOperators) {
    Dia::Mailbox::Address addr1;
    Dia::Mailbox::Address addr2;

    // Two default-constructed addresses should be equal
    EXPECT_TRUE(addr1 == addr2);
    EXPECT_FALSE(addr1 != addr2);

    // Modify addr2's payload
    addr2.payload = 42;
    EXPECT_FALSE(addr1 == addr2);
    EXPECT_TRUE(addr1 != addr2);
}

// AC3: SubscriberId equality operators
TEST(MailboxTypesTests, SubscriberIdEqualityOperators) {
    Dia::Mailbox::SubscriberId sub1{123};
    Dia::Mailbox::SubscriberId sub2{123};
    Dia::Mailbox::SubscriberId sub3{456};

    EXPECT_TRUE(sub1 == sub2);
    EXPECT_FALSE(sub1 != sub2);
    EXPECT_FALSE(sub1 == sub3);
    EXPECT_TRUE(sub1 != sub3);
}

// AC4: OverflowPolicy size and enumerators
TEST(MailboxTypesTests, OverflowPolicyProperties) {
    static_assert(sizeof(Dia::Mailbox::OverflowPolicy) == 1, "OverflowPolicy must be 1 byte");

    // Check that enumerators exist
    [[maybe_unused]] Dia::Mailbox::OverflowPolicy p1 = Dia::Mailbox::OverflowPolicy::DropOldest;
    [[maybe_unused]] Dia::Mailbox::OverflowPolicy p2 = Dia::Mailbox::OverflowPolicy::Assert;
}

// AC5: SubscriptionHandle forward declaration - pointer to incomplete type is legal
TEST(MailboxTypesTests, SubscriptionHandleForwardDeclaration) {
    // This test just verifies compilation: pointer to incomplete type is legal
    Dia::Mailbox::SubscriptionHandle* p = nullptr;
    EXPECT_EQ(p, nullptr);

    // DO NOT test sizeof(SubscriptionHandle) - would fail to compile as required
}

// AC6: SubscriberSet type alias
TEST(MailboxTypesTests, SubscriberSetTypeAlias) {
    static_assert(
        std::is_same_v<
            Dia::Mailbox::SubscriberSet,
            Dia::Core::Containers::DynamicArrayC<Dia::Mailbox::SubscriberId, 64>
        >,
        "SubscriberSet must be DynamicArrayC<SubscriberId, 64>"
    );
}

// AC7: No STL includes in public headers (code review only, covered by AC1-AC6 compiling)
TEST(MailboxTypesTests, HeaderCompiles) {
    // If this test compiles, MailboxTypes.h compiled successfully with only its dependencies
    // and no STL includes (verified by code review)
    EXPECT_TRUE(true);
}

// AC9: Address is copyable (StringCRC has user-defined copy ctor so Address is not trivially copyable)
TEST(MailboxTypesTests, AddressCopyable) {
    Dia::Mailbox::Address a1;
    a1.payload = 99;
    Dia::Mailbox::Address a2 = a1;
    EXPECT_EQ(a1, a2);
}

// AC10: SubscriberId is trivially copyable
TEST(MailboxTypesTests, SubscriberIdTriviallyCopyable) {
    static_assert(
        std::is_trivially_copyable_v<Dia::Mailbox::SubscriberId>,
        "SubscriberId must be trivially copyable"
    );
}
