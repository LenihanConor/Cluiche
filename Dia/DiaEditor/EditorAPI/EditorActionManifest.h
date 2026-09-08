#pragma once

#include <DiaEditor/EditorAPI/EditorActionDescriptor.h>

namespace Dia
{
	namespace Editor
	{
		// Read-only view of a registered action (stored by value in the manifest)
		// Stores everything except the handler (not needed for manifest queries/serialisation)
		struct EditorActionEntry
		{
			Dia::Core::StringCRC    name;
			const char*             description    = nullptr;
			const char*             category       = nullptr;
			const char*             owner          = nullptr;
			EditorActionParamSchema params;
			DispatchThread          dispatchThread = DispatchThread::kMainThread;
		};

		// Full manifest: all registered actions, queryable by name or owner
		class EditorActionManifest
		{
		public:
			static const unsigned int kMaxActions = 128;

			// Add an entry (called by registry on RegisterAction)
			void Add(const EditorActionEntry& entry);

			// Remove all entries for a given owner
			void RemoveByOwner(Dia::Core::StringCRC owner);

			// Query
			const EditorActionEntry* FindByName(Dia::Core::StringCRC name) const;
			unsigned int             GetCount() const;
			const EditorActionEntry& GetAt(unsigned int index) const;

		private:
			Dia::Core::Containers::DynamicArrayC<EditorActionEntry, kMaxActions> mEntries;
		};

	} // namespace Editor
} // namespace Dia
