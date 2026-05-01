#ifndef DIA_WEBSOCKET_MESSAGE_H
#define DIA_WEBSOCKET_MESSAGE_H

#include "DiaWebSocket/Error.h"
#include "DiaCore/Core/Assert.h"
#include <cstddef>

namespace Dia
{
	namespace WebSocket
	{
		enum class MessageType
		{
			kText,
			kBinary
		};

		struct Message
		{
			MessageType type;
			const void* data;
			size_t length;
			int connectionId;

			const char* AsText() const
			{
				DIA_ASSERT(type == MessageType::kText, "Message is not text");
				return static_cast<const char*>(data);
			}
		};
	}
}

#endif // DIA_WEBSOCKET_MESSAGE_H
