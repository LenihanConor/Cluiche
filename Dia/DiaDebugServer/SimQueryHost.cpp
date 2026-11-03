#include "DiaDebugServer/SimQueryHost.h"

#include <DiaObservation/Log/DiaLog.h>

#include <cstring>
#include <memory>
#include <string>

namespace Dia
{
	namespace DebugServer
	{
		namespace
		{
			void SetReplyText(SimReplyEvent& reply, SimReplyStatus status, const char* text)
			{
				reply.status = static_cast<uint32_t>(status);
				std::strncpy(reply.json, text, kSimPayloadBytes - 1);
				reply.json[kSimPayloadBytes - 1] = '\0';
			}

			void SetFailure(SimReplyEvent& reply, SimReplyStatus status, const char* error)
			{
				std::string text = "{\"success\":false,\"error\":\"";
				text += error;
				text += "\"}";
				SetReplyText(reply, status, text.c_str());
			}
		}

		SimQueryHost::SimQueryHost() : mSource(nullptr), mSink(nullptr), mWarnedFull(false) {}

		void SimQueryHost::SetRequestSource(ISimRequestSource* source) { mSource = source; }
		void SimQueryHost::SetReplySink(ISimReplySink* sink) { mSink = sink; }

		void SimQueryHost::SetResyncHandler(std::function<void()> handler) { mResyncHandler = handler; }

		bool SimQueryHost::RegisterQuery(Dia::Core::StringCRC name, const SimHelpInfo& help, QueryHandler handler)
		{
			return Register(name, SimKind::Query, help, handler);
		}

		bool SimQueryHost::RegisterCommand(Dia::Core::StringCRC name, const SimHelpInfo& help, QueryHandler handler)
		{
			return Register(name, SimKind::Command, help, handler);
		}

		bool SimQueryHost::Register(Dia::Core::StringCRC name, SimKind kind, const SimHelpInfo& help, QueryHandler handler)
		{
			if (!handler)
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: null handler for '%s'", name.AsChar());
				return false;
			}
			if (Has(name))
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: handler already registered for '%s'", name.AsChar());
				return false;
			}
			if (mHandlers.IsFull())
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: max handler capacity reached, cannot register '%s'", name.AsChar());
				return false;
			}

			std::shared_ptr<SimHelpStored> stored(new SimHelpStored);
			const SimHelpResult helpResult = CopyHelp(help, *stored);
			if (helpResult != SimHelpResult::Ok)
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: invalid help for '%s' (%s), not registered", name.AsChar(), SimHelpResultText(helpResult));
				return false;
			}

			Entry entry;
			entry.name = name;
			entry.kind = kind;
			entry.handler = handler;
			entry.help = stored;
			mHandlers.Add(entry);
			return true;
		}

		bool SimQueryHost::UnregisterQuery(Dia::Core::StringCRC name)
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
				{
					mHandlers.RemoveAt(i);
					return true;
				}
			}
			return false;
		}

		bool SimQueryHost::Has(Dia::Core::StringCRC name) const
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
					return true;
			}
			return false;
		}

		const SimHelpStored* SimQueryHost::GetHelp(Dia::Core::StringCRC name) const
		{
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name == name)
					return mHandlers[i].help.get();
			}
			return nullptr;
		}

		void SimQueryHost::Handle(const SimRequestEvent& request, SimReplyEvent& reply) const
		{
			reply.requestId = request.requestId;

			const Entry* found = nullptr;
			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].name.Value() == request.nameCrc && static_cast<uint32_t>(mHandlers[i].kind) == request.kind)
				{
					found = &mHandlers[i];
					break;
				}
			}
			if (!found)
			{
				SetFailure(reply, SimReplyStatus::UnknownName, "unknown name");
				return;
			}

			const char* nul = static_cast<const char*>(std::memchr(request.json, 0, kSimPayloadBytes));
			Json::Value args;
			if (!nul)
			{
				SetFailure(reply, SimReplyStatus::HandlerFailed, "handler failed");
				return;
			}
			{
				Json::CharReaderBuilder builder;
				std::string errs;
				std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
				if (!reader->parse(request.json, nul, &args, &errs))
				{
					SetFailure(reply, SimReplyStatus::HandlerFailed, "handler failed");
					return;
				}
			}

			Json::Value result;
			try
			{
				result = found->handler(args);
			}
			catch (...)
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: handler for request %u threw an exception", request.requestId);
				SetFailure(reply, SimReplyStatus::HandlerFailed, "handler failed");
				return;
			}

			if (!result.isObject())
			{
				DIA_LOG_ERROR("DebugServer", "SimQueryHost: handler for request %u returned a non-object", request.requestId);
				SetFailure(reply, SimReplyStatus::HandlerFailed, "handler failed");
				return;
			}

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			const std::string text = Json::writeString(writer, result);
			if (text.size() >= kSimPayloadBytes)
			{
				SetFailure(reply, SimReplyStatus::TooLarge, "reply too large");
				return;
			}
			SetReplyText(reply, SimReplyStatus::Ok, text.c_str());
		}

		void SimQueryHost::Update()
		{
			if (!mSource)
				return;

			// Heap-allocated: events are ~16 KB each, too big to stack-allocate comfortably.
			std::unique_ptr<SimRequestEvent> request(new SimRequestEvent);
			std::unique_ptr<SimReplyEvent> reply(new SimReplyEvent);

			while (mSource->TryPop(*request))
			{
				// Control request: handled before any name lookup and never replied to.
				if (request->kind == static_cast<uint32_t>(SimKind::Resync))
				{
					if (mResyncHandler)
						mResyncHandler();
					continue;
				}

				std::memset(reply.get(), 0, sizeof(SimReplyEvent));
				Handle(*request, *reply);

				if (!mSink || !mSink->PostReply(*reply))
				{
					if (!mWarnedFull)
					{
						mWarnedFull = true;
						DIA_LOG_WARNING("DebugServer", "SimQueryHost: reply stream full, dropping reply (further drops not logged)");
					}
				}
			}
		}
	}
}
