#include "DiaEditor/Project/GamePath.h"

#include <filesystem>
#include <string>
#include <string.h>

namespace Dia
{
	namespace Editor
	{
		void NormalizeGamePath(const char* in, char* out, unsigned int outSize)
		{
			if (out == nullptr || outSize == 0)
			{
				return;
			}
			out[0] = '\0';
			if (in == nullptr || in[0] == '\0')
			{
				return;
			}

			std::error_code ec;
			std::filesystem::path p = std::filesystem::absolute(std::filesystem::path(in), ec);
			if (ec)
			{
				p = std::filesystem::path(in);
			}
			std::error_code ec2;
			std::filesystem::path canon = std::filesystem::weakly_canonical(p, ec2);
			if (!ec2)
			{
				p = canon;
			}
			else
			{
				p = p.lexically_normal();
			}

			std::string s = p.generic_string();
			for (size_t i = 0; i < s.size(); ++i)
			{
				char c = s[i];
				if (c >= 'A' && c <= 'Z')
				{
					s[i] = static_cast<char>(c - 'A' + 'a');
				}
			}
			strncpy_s(out, outSize, s.c_str(), _TRUNCATE);
		}

		bool IsSameGamePath(const char* a, const char* b)
		{
			if (a == nullptr || b == nullptr || a[0] == '\0' || b[0] == '\0')
			{
				return false;
			}
			char ka[1024];
			char kb[1024];
			NormalizeGamePath(a, ka, sizeof(ka));
			NormalizeGamePath(b, kb, sizeof(kb));
			return ka[0] != '\0' && strcmp(ka, kb) == 0;
		}
	}
}
