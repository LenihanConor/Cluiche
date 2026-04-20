////////////////////////////////////////////////////////////////////////////////
// Filename: CEFUtils.h
// Utility functions extracted for testability (no CEF header dependencies).
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <string>

namespace Dia
{
	namespace UICEF
	{
		namespace Utils
		{
			inline const char* GetMimeType(const std::string& filePath)
			{
				auto endsWith = [](const std::string& str, const std::string& suffix) -> bool
				{
					if (suffix.size() > str.size()) return false;
					return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
				};

				if (endsWith(filePath, ".html") || endsWith(filePath, ".htm")) return "text/html";
				if (endsWith(filePath, ".css")) return "text/css";
				if (endsWith(filePath, ".js")) return "application/javascript";
				if (endsWith(filePath, ".json")) return "application/json";
				if (endsWith(filePath, ".png")) return "image/png";
				if (endsWith(filePath, ".jpg") || endsWith(filePath, ".jpeg")) return "image/jpeg";
				if (endsWith(filePath, ".gif")) return "image/gif";
				if (endsWith(filePath, ".svg")) return "image/svg+xml";
				if (endsWith(filePath, ".woff")) return "font/woff";
				if (endsWith(filePath, ".woff2")) return "font/woff2";
				if (endsWith(filePath, ".ttf")) return "font/ttf";
				if (endsWith(filePath, ".eot")) return "application/vnd.ms-fontobject";
				if (endsWith(filePath, ".ico")) return "image/x-icon";
				return "application/octet-stream";
			}

			inline bool IsPathTraversal(const std::string& path)
			{
				return path.find("..") != std::string::npos;
			}

			inline std::string ExtractPathFromDiaURL(const std::string& url)
			{
				// dia://app/index.html -> app/index.html
				const std::string prefix = "dia://";
				if (url.compare(0, prefix.size(), prefix) == 0)
					return url.substr(prefix.size());

				return url;
			}

			inline void ConvertBGRAtoRGBA(unsigned char* buffer, int width, int height,
				int startX, int startY, int rectWidth, int rectHeight)
			{
				for (int y = startY; y < startY + rectHeight && y < height; ++y)
				{
					for (int x = startX; x < startX + rectWidth && x < width; ++x)
					{
						int offset = (y * width + x) * 4;
						unsigned char b = buffer[offset + 0];
						unsigned char r = buffer[offset + 2];
						buffer[offset + 0] = r;
						buffer[offset + 2] = b;
					}
				}
			}
		}
	}
}
