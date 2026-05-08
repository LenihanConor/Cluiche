#pragma once

namespace Dia
{
	namespace Serializer
	{
		struct SerializeResult
		{
			bool		ok;
			const char*	error;  // static string literal; null on success

			explicit operator bool() const { return ok; }

			static SerializeResult Success()
			{
				SerializeResult r;
				r.ok = true;
				r.error = nullptr;
				return r;
			}

			static SerializeResult Failure(const char* errorMsg)
			{
				SerializeResult r;
				r.ok = false;
				r.error = errorMsg;
				return r;
			}
		};
	}
}
