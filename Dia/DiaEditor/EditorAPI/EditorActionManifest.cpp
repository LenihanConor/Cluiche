#include <DiaEditor/EditorAPI/EditorActionManifest.h>
#include <DiaCore/Core/Assert.h>
#include <cstring>

namespace Dia
{
	namespace Editor
	{
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
