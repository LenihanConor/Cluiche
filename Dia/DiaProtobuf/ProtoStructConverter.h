#ifndef DIA_PROTOBUF_PROTO_STRUCT_CONVERTER_H
#define DIA_PROTOBUF_PROTO_STRUCT_CONVERTER_H

namespace Json { class Value; }
namespace google { namespace protobuf { class Struct; } }

namespace Dia
{
	namespace Proto
	{
		// Converts a jsoncpp Value (must be objectValue or nullValue) into a
		// google::protobuf::Struct. Arrays within the object become ListValue entries.
		// Returns false if src is not an object or null — top-level arrays are not
		// representable as Struct.
		//
		// PRECISION NOTE: All numbers are stored as double. Integers > 2^53
		// (9,007,199,254,740,992) will lose precision silently.
		bool JsonValueToProtoStruct(const Json::Value& src, google::protobuf::Struct* dst);

		// Converts a google::protobuf::Struct back to a jsoncpp Value (objectValue).
		Json::Value ProtoStructToJsonValue(const google::protobuf::Struct& src);
	}
}

#endif // DIA_PROTOBUF_PROTO_STRUCT_CONVERTER_H
