#pragma once

#include <stdint.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaSaveGame/ISaveable.h>

namespace Dia::SaveGame
{
	class SaveRegistry;
	class SaveManager;
	class SaveContext;
	class LoadContext;
}

namespace Dia::DeferredStep
{
	class DeferredStepScheduler;

	// Save participant for the deferred-step scheduler. SaveStarted drains in-flight steps (so Serialize stays const);
	// LoadStarted abandons them. SimPU only.
	class DeferredStepSaveParticipant final : public Dia::SaveGame::ISaveable, public Dia::Core::Observer
	{
	public:
		static const uint32_t kFormatVersion = 1;
		static const Dia::Core::StringCRC kParticipantId;   // "DeferredStep"

		explicit DeferredStepSaveParticipant(DeferredStepScheduler& scheduler);
		~DeferredStepSaveParticipant() override;

		DeferredStepSaveParticipant(const DeferredStepSaveParticipant&) = delete;
		DeferredStepSaveParticipant& operator=(const DeferredStepSaveParticipant&) = delete;

		// Registers with the SaveRegistry and attaches to OnSaveStarted / OnLoadStarted.
		// Attach twice, or Detach without Attach, asserts in Debug and is a no-op in Release.
		void Attach(Dia::SaveGame::SaveRegistry& registry, Dia::SaveGame::SaveManager& manager);
		void Detach();

		// ISaveable
		void     Serialize  (Dia::SaveGame::SaveContext& ctx) const override;
		void     Deserialize(Dia::SaveGame::LoadContext& ctx) override;
		uint32_t GetVersion () const override;
		bool     Validate   (Dia::SaveGame::LoadContext& ctx) const override;

		// Observer: SaveEvent::SaveStarted drains, SaveEvent::LoadStarted abandons; other events are ignored.
		void ObserverNotification(const Dia::Core::ObserverSubject* theChangeSubject, int message) override;

	private:
		DeferredStepScheduler& mScheduler;
		Dia::SaveGame::SaveRegistry* mRegistry = nullptr;
		Dia::SaveGame::SaveManager* mManager = nullptr;
	};
}
