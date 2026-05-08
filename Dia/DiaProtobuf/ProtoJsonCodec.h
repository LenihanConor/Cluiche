#ifndef DIA_PROTOBUF_PROTO_JSON_CODEC_H
#define DIA_PROTOBUF_PROTO_JSON_CODEC_H

namespace google { namespace protobuf { class Message; } }

namespace Dia
{
	namespace Proto
	{
		bool ToJson(const google::protobuf::Message& msg, char* outBuffer, unsigned int bufferSize, unsigned int* outLength = nullptr);
		bool FromJson(const char* json, google::protobuf::Message* msg);
	}
}

#endif // DIA_PROTOBUF_PROTO_JSON_CODEC_H
