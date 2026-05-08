#pragma once

#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class FileDialogHandler
		{
		public:
			static Json::Value HandleOpenFileDialog(const Json::Value& data);
			static Json::Value HandleSaveFileDialog(const Json::Value& data);
		};
	}
}
