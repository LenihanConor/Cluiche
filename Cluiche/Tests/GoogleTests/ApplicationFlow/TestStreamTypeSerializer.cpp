#include <gtest/gtest.h>
#include <DiaApplicationFlow/Streams/StreamTypeRegistry.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

struct SerializerTestPayload { int value; };

static Json::Value SerializeSerializerTestPayload(const void* bytes, size_t size)
{
    const auto* p = static_cast<const SerializerTestPayload*>(bytes);
    Json::Value v;
    v["value"] = p->value;
    return v;
}
DIA_STREAM_TYPE_WITH_SERIALIZER(SerializerTestPayload, SerializeSerializerTestPayload);

TEST(StreamTypeSerializer, RegisteredTypeSerializes)
{
    SerializerTestPayload p{42};
    Dia::Core::StringCRC typeId = Dia::ApplicationFlow::StreamTypeRegistry::GetTypeId<SerializerTestPayload>();
    Json::Value result = Dia::ApplicationFlow::StreamTypeRegistry::SerializeToJson(typeId, &p, sizeof(p));
    EXPECT_FALSE(result.isNull());
    EXPECT_EQ(result["value"].asInt(), 42);
}

TEST(StreamTypeSerializer, UnregisteredTypeReturnsNullValue)
{
    Dia::Core::StringCRC unknown("UnknownType_XYZ_999");
    SerializerTestPayload p{1};
    Json::Value result = Dia::ApplicationFlow::StreamTypeRegistry::SerializeToJson(unknown, &p, sizeof(p));
    EXPECT_TRUE(result.isNull());
}

TEST(StreamTypeSerializer, MacroRegistersTypeAndSerializer)
{
    // DIA_STREAM_TYPE_WITH_SERIALIZER registered both the type ID and serializer above.
    Dia::Core::StringCRC typeId = Dia::ApplicationFlow::StreamTypeRegistry::GetTypeId<SerializerTestPayload>();
    EXPECT_TRUE(Dia::ApplicationFlow::StreamTypeRegistry::IsRegistered(typeId));
    // Serializer must be callable
    SerializerTestPayload p{7};
    Json::Value v = Dia::ApplicationFlow::StreamTypeRegistry::SerializeToJson(typeId, &p, sizeof(p));
    EXPECT_EQ(v["value"].asInt(), 7);
}
