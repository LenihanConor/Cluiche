#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>
#include <cstring>

namespace Dia
{
	namespace Editor
	{
		Json::Value EditorActionParamToJson(const EditorActionParam& param)
		{
			Json::Value pj(Json::objectValue);
			pj["name"]        = param.name        ? param.name        : "";
			pj["type"]        = param.type        ? param.type        : "string";
			pj["required"]    = param.required;
			pj["description"] = param.description ? param.description : "";
			if (param.unit != nullptr && param.unit[0] != '\0') pj["unit"] = param.unit;
			// Range is omitted when either bound is NaN/inf: JSON has no representation for them
			// (jsoncpp would emit invalid text such as 1e+9999).
			if (param.hasRange && std::isfinite(param.min) && std::isfinite(param.max))
			{
				pj["min"] = param.min;
				pj["max"] = param.max;
			}
			if (param.enumValues != nullptr) pj["values"] = param.enumValues;
			return pj;
		}

		void EditorActionManifest::Add(const EditorActionEntry& entry)
		{
			DIA_ASSERT(entry.name != Dia::Core::StringCRC(), "EditorActionManifest::Add - entry.name must not be default StringCRC");
			mEntries.Add(entry);
		}

		void EditorActionManifest::RemoveByOwner(Dia::Core::StringCRC owner)
		{
			unsigned int i = 0;
			while (i < mEntries.Size())
			{
				const EditorActionEntry& entry = mEntries[i];
				if (entry.owner != nullptr && Dia::Core::StringCRC(entry.owner) == owner)
				{
					mEntries.RemoveAt(i);
					// Do not advance i — the next element shifted into position i
				}
				else
				{
					++i;
				}
			}
		}

		const EditorActionEntry* EditorActionManifest::FindByName(Dia::Core::StringCRC name) const
		{
			for (unsigned int i = 0; i < mEntries.Size(); ++i)
			{
				if (mEntries[i].name == name)
				{
					return &mEntries[i];
				}
			}
			return nullptr;
		}

		unsigned int EditorActionManifest::GetCount() const
		{
			return mEntries.Size();
		}

		const EditorActionEntry& EditorActionManifest::GetAt(unsigned int index) const
		{
			DIA_ASSERT(index < mEntries.Size(), "EditorActionManifest::GetAt - index out of range");
			return mEntries[index];
		}

	} // namespace Editor
} // namespace Dia
