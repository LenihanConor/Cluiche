#include "DiaProtobuf/ProtoStructConverter.h"

#include <DiaCore/Json/external/json/json.h>

#pragma warning(push)
#pragma warning(disable: 4018)
#include <google/protobuf/struct.pb.h>
#pragma warning(pop)

namespace Dia
{
	namespace Proto
	{
		static void JsonValueToProtoValue(const Json::Value& src, google::protobuf::Value* dst);
		static Json::Value ProtoValueToJsonValue(const google::protobuf::Value& src);

		static void JsonArrayToProtoListValue(const Json::Value& arr, google::protobuf::ListValue* list)
		{
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
				JsonValueToProtoValue(arr[i], list->add_values());
		}

		static void JsonValueToProtoValue(const Json::Value& src, google::protobuf::Value* dst)
		{
			if (src.isNull())
			{
				dst->set_null_value(google::protobuf::NULL_VALUE);
			}
			else if (src.isBool())
			{
				dst->set_bool_value(src.asBool());
			}
			else if (src.isNumeric())
			{
				dst->set_number_value(src.asDouble());
			}
			else if (src.isString())
			{
				dst->set_string_value(src.asString());
			}
			else if (src.isObject())
			{
				JsonValueToProtoStruct(src, dst->mutable_struct_value());
			}
			else if (src.isArray())
			{
				JsonArrayToProtoListValue(src, dst->mutable_list_value());
			}
		}

		bool JsonValueToProtoStruct(const Json::Value& src, google::protobuf::Struct* dst)
		{
			if (dst == nullptr)
				return false;
			if (src.isNull())
				return true;
			if (!src.isObject())
				return false;

			auto& fields = *dst->mutable_fields();
			Json::Value::Members members = src.getMemberNames();
			for (unsigned int i = 0; i < members.size(); ++i)
				JsonValueToProtoValue(src[members[i]], &fields[members[i]]);

			return true;
		}

		static Json::Value ProtoValueToJsonValue(const google::protobuf::Value& src)
		{
			switch (src.kind_case())
			{
			case google::protobuf::Value::kNullValue:
				return Json::Value(Json::nullValue);
			case google::protobuf::Value::kBoolValue:
				return Json::Value(src.bool_value());
			case google::protobuf::Value::kNumberValue:
				return Json::Value(src.number_value());
			case google::protobuf::Value::kStringValue:
				return Json::Value(src.string_value());
			case google::protobuf::Value::kStructValue:
				return ProtoStructToJsonValue(src.struct_value());
			case google::protobuf::Value::kListValue:
			{
				Json::Value arr(Json::arrayValue);
				const auto& list = src.list_value();
				for (int i = 0; i < list.values_size(); ++i)
					arr.append(ProtoValueToJsonValue(list.values(i)));
				return arr;
			}
			default:
				return Json::Value(Json::nullValue);
			}
		}

		Json::Value ProtoStructToJsonValue(const google::protobuf::Struct& src)
		{
			Json::Value result(Json::objectValue);
			const auto& fields = src.fields();
			for (auto it = fields.begin(); it != fields.end(); ++it)
				result[it->first] = ProtoValueToJsonValue(it->second);
			return result;
		}
	}
}
