#include <gtest/gtest.h>

#include <DiaProtobuf/ProtoStructConverter.h>
#include <DiaCore/Json/external/json/json.h>
#include <google/protobuf/struct.pb.h>

using Dia::Proto::JsonValueToProtoStruct;
using Dia::Proto::ProtoStructToJsonValue;

TEST(ProtoStructConverter, EmptyObject_RoundTrip)
{
	Json::Value src(Json::objectValue);
	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));
	EXPECT_TRUE(s.fields().empty());

	Json::Value result = ProtoStructToJsonValue(s);
	EXPECT_TRUE(result.isObject());
	EXPECT_EQ(result.size(), 0u);
}

TEST(ProtoStructConverter, StringField_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["key"] = "value";

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("key"));
	EXPECT_EQ(result["key"].asString(), "value");
}

TEST(ProtoStructConverter, BoolField_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["flag"] = true;

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	ASSERT_TRUE(s.fields().count("flag") > 0);
	EXPECT_EQ(s.fields().at("flag").kind_case(), google::protobuf::Value::kBoolValue);

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("flag"));
	EXPECT_TRUE(result["flag"].isBool());
	EXPECT_TRUE(result["flag"].asBool());
}

TEST(ProtoStructConverter, IntegerField_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["count"] = 42;

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	ASSERT_TRUE(s.fields().count("count") > 0);
	EXPECT_EQ(s.fields().at("count").kind_case(), google::protobuf::Value::kNumberValue);

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("count"));
	EXPECT_EQ(result["count"].asInt(), 42);
}

TEST(ProtoStructConverter, LargeInteger_PrecisionBoundary)
{
	// Integers > 2^53 lose precision when stored as double.
	// This test documents the known precision loss.
	const int64_t original = 9007199254740993LL; // 2^53 + 1
	Json::Value src(Json::objectValue);
	src["id"] = Json::Value::Int64(original);

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("id"));
	// Document that precision is lost: the recovered value is not equal to original
	EXPECT_NE(result["id"].asInt64(), original);
}

TEST(ProtoStructConverter, DoubleField_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["pi"] = 3.14159;

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("pi"));
	EXPECT_DOUBLE_EQ(result["pi"].asDouble(), 3.14159);
}

TEST(ProtoStructConverter, NullValue_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["x"] = Json::Value(Json::nullValue);

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	ASSERT_TRUE(s.fields().count("x") > 0);
	EXPECT_EQ(s.fields().at("x").kind_case(), google::protobuf::Value::kNullValue);

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("x"));
	EXPECT_TRUE(result["x"].isNull());
}

TEST(ProtoStructConverter, NestedObject_RoundTrip)
{
	Json::Value src(Json::objectValue);
	src["outer"]["inner"] = "val";

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("outer"));
	ASSERT_TRUE(result["outer"].isObject());
	EXPECT_EQ(result["outer"]["inner"].asString(), "val");
}

TEST(ProtoStructConverter, ArrayValue_RoundTrip)
{
	Json::Value src(Json::objectValue);
	Json::Value arr(Json::arrayValue);
	arr.append(1);
	arr.append("two");
	arr.append(true);
	arr.append(Json::Value(Json::nullValue));
	src["items"] = arr;

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result.isMember("items"));
	ASSERT_TRUE(result["items"].isArray());
	ASSERT_EQ(result["items"].size(), 4u);
	EXPECT_EQ(result["items"][0].asInt(), 1);
	EXPECT_EQ(result["items"][1].asString(), "two");
	EXPECT_TRUE(result["items"][2].isBool());
	EXPECT_TRUE(result["items"][2].asBool());
	EXPECT_TRUE(result["items"][3].isNull());
}

TEST(ProtoStructConverter, NestedArray_RoundTrip)
{
	Json::Value src(Json::objectValue);
	Json::Value matrix(Json::arrayValue);
	Json::Value row0(Json::arrayValue);
	row0.append(1); row0.append(2);
	Json::Value row1(Json::arrayValue);
	row1.append(3); row1.append(4);
	matrix.append(row0);
	matrix.append(row1);
	src["matrix"] = matrix;

	google::protobuf::Struct s;
	ASSERT_TRUE(JsonValueToProtoStruct(src, &s));

	Json::Value result = ProtoStructToJsonValue(s);
	ASSERT_TRUE(result["matrix"].isArray());
	ASSERT_EQ(result["matrix"].size(), 2u);
	ASSERT_TRUE(result["matrix"][0].isArray());
	EXPECT_EQ(result["matrix"][0][0].asInt(), 1);
	EXPECT_EQ(result["matrix"][1][1].asInt(), 4);
}

TEST(ProtoStructConverter, NullInput_ReturnsTrue_EmptyStruct)
{
	Json::Value src(Json::nullValue);
	google::protobuf::Struct s;
	EXPECT_TRUE(JsonValueToProtoStruct(src, &s));
	EXPECT_TRUE(s.fields().empty());
}

TEST(ProtoStructConverter, NonObjectInput_ReturnsFalse)
{
	Json::Value arr(Json::arrayValue);
	arr.append(1);
	google::protobuf::Struct s;
	EXPECT_FALSE(JsonValueToProtoStruct(arr, &s));
}

TEST(ProtoStructConverter, EmptyStruct_ProducesEmptyObject)
{
	google::protobuf::Struct s;
	Json::Value result = ProtoStructToJsonValue(s);
	EXPECT_TRUE(result.isObject());
	EXPECT_EQ(result.size(), 0u);
}
