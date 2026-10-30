#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include "DiaEditor/Plugin/IEditorPlugin.h"

namespace Dia
{
	namespace Editor
	{
		class EditorPluginRegistry
		{
		public:
			static const unsigned int kMaxPlugins = 32;
			static const unsigned int kMaxManifests = 16;

			static EditorPluginRegistry& Instance();

			void RegisterPlugin(const Dia::Core::StringCRC& typeId, IEditorPluginFactory* factory);
			IEditorPlugin* CreatePlugin(const Dia::Core::StringCRC& typeId);
			bool IsPluginRegistered(const Dia::Core::StringCRC& typeId) const;

			unsigned int GetRegisteredCount() const;
			const Dia::Core::StringCRC& GetRegisteredTypeId(unsigned int index) const;
			IEditorPluginFactory* GetFactory(unsigned int index) const;

			void TagPluginManifest(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& manifestId);
			void SetActiveManifests(const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests>& manifests);
			void ClearActiveManifests();
			bool IsInScopeFilter(const Dia::Core::StringCRC& typeId) const;

			// Game scope: a type tagged for a game is available only while that game is active.
			// A game-scoped tag takes precedence over a .cluicheproj manifest tag. An empty gameId clears the tag.
			typedef void (*GameScopeChangedCallback)(void* userData, const Dia::Core::StringCRC& oldGame, const Dia::Core::StringCRC& newGame);
			static const unsigned int kMaxGameScopeListeners = 8;

			void TagGameScoped(const Dia::Core::StringCRC& typeId, const Dia::Core::StringCRC& gameId);
			void SetGameScope(const Dia::Core::StringCRC& gameId, const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxPlugins>& scopedTypeIds);
			void ClearGameScope();
			const Dia::Core::StringCRC& GetActiveGameId() const;
			bool IsGameScoped(const Dia::Core::StringCRC& typeId) const;
			bool IsAvailable(const Dia::Core::StringCRC& typeId) const;
			// Registered, game-scoped, and its game is not the active one. Ignores the .cluicheproj display filter.
			bool IsOutOfGameScope(const Dia::Core::StringCRC& typeId) const;

			void AddGameScopeListener(GameScopeChangedCallback callback, void* userData);
			void RemoveGameScopeListener(GameScopeChangedCallback callback, void* userData);

		private:
			void ChangeActiveGame(const Dia::Core::StringCRC& newGame);

			EditorPluginRegistry() = default;
			~EditorPluginRegistry() = default;
			EditorPluginRegistry(const EditorPluginRegistry&) = delete;
			EditorPluginRegistry& operator=(const EditorPluginRegistry&) = delete;

			struct PluginEntry
			{
				Dia::Core::StringCRC typeId;
				IEditorPluginFactory* factory;
				Dia::Core::StringCRC manifestId;  // empty = built-in
				Dia::Core::StringCRC gameId;      // empty = not game-scoped
			};

			struct GameScopeListener
			{
				GameScopeChangedCallback callback;
				void* userData;
			};

			Dia::Core::Containers::DynamicArrayC<PluginEntry, kMaxPlugins> mEntries;
			Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxManifests> mActiveManifests;
			Dia::Core::StringCRC mActiveGameId;
			GameScopeListener mListeners[kMaxGameScopeListeners] = {};
			unsigned int mListenerCount = 0;
		};
	}
}
