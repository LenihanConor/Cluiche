#include "DiaProtobuf/ProtoJsonCodec.h"

#include <google/protobuf/util/json_util.h>
#include <string>
#include <cstring>

namespace Dia
{
	namespace Proto
	{
		bool ToJson(const google::protobuf::Message& msg, char* outBuffer, unsigned int bufferSize, unsigned int* outLength)
		{
			if (outBuffer == nullptr || bufferSize == 0)
				return false;

			std::string output;
			google::protobuf::util::JsonPrintOptions options;
			options.preserve_proto_field_names = true;
			auto status = google::protobuf::util::MessageToJsonString(msg, &output, options);
			if (!status.ok())
			{
				outBuffer[0] = '\0';
				if (outLength) *outLength = 0;
				return false;
			}

			if (output.size() >= bufferSize)
			{
				outBuffer[0] = '\0';
				if (outLength) *outLength = 0;
				return false;
			}

			memcpy(outBuffer, output.c_str(), output.size() + 1);
			if (outLength) *outLength = static_cast<unsigned int>(output.size());
			return true;
		}

		bool FromJson(const char* json, google::protobuf::Message* msg)
		{
			if (json == nullptr || msg == nullptr)
				return false;

			google::protobuf::util::JsonParseOptions options;
			options.ignore_unknown_fields = true;
			auto status = google::protobuf::util::JsonStringToMessage(json, msg, options);
			return status.ok();
		}
	}
}
