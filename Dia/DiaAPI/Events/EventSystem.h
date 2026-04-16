////////////////////////////////////////////////////////////////////////////////
// Filename: EventSystem.h
// Description: Event system for DiaAPI command lifecycle events
// Feature spec: docs/specs/features/dia/diacli/event-system.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

namespace Dia
{
	namespace API
	{
		////////////////////////////////////////////////////////////////////////////////
		// Event Data Structures
		////////////////////////////////////////////////////////////////////////////////

		// Event: Command registered
		struct CommandRegisteredEvent
		{
			Dia::Core::StringCRC commandName;
			const char* description;
		};

		// Event: Command about to execute
		struct CommandExecutingEvent
		{
			Dia::Core::StringCRC commandName;
			const CommandArgs* args;  // Pointer to arguments (not owned)
		};

		// Event: Command execution completed
		struct CommandExecutedEvent
		{
			Dia::Core::StringCRC commandName;
			int exitCode;
			float durationSeconds;
		};

		// Event: Command execution error
		struct CommandErrorEvent
		{
			Dia::Core::StringCRC commandName;
			const char* errorMessage;
			int exitCode;
		};

		// Event: Help requested
		struct HelpRequestedEvent
		{
			Dia::Core::StringCRC commandName;  // Empty if global help
			bool isGlobalHelp;
		};

		////////////////////////////////////////////////////////////////////////////////
		// Templated Observer Pattern (type-safe wrapper over DiaCore Observer)
		////////////////////////////////////////////////////////////////////////////////

		// Forward declarations for event data access
		namespace Internal
		{
			// Thread-local storage accessors (specialized in .cpp)
			const CommandRegisteredEvent* GetCurrentCommandRegisteredEvent();
			const CommandExecutingEvent* GetCurrentCommandExecutingEvent();
			const CommandExecutedEvent* GetCurrentCommandExecutedEvent();
			const CommandErrorEvent* GetCurrentCommandErrorEvent();
			const HelpRequestedEvent* GetCurrentHelpRequestedEvent();

			void SetCurrentCommandRegisteredEvent(const CommandRegisteredEvent* event);
			void SetCurrentCommandExecutingEvent(const CommandExecutingEvent* event);
			void SetCurrentCommandExecutedEvent(const CommandExecutedEvent* event);
			void SetCurrentCommandErrorEvent(const CommandErrorEvent* event);
			void SetCurrentHelpRequestedEvent(const HelpRequestedEvent* event);
		}

		// Type-safe observer base class
		template<typename EventType>
		class Observer : public Dia::Core::Observer
		{
		public:
			// Type-safe notification method (override this)
			virtual void Notify(const EventType& event) = 0;

			// DiaCore Observer interface implementation
			void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;
		};

		// Explicit specializations (implemented in .cpp)
		template<> void Observer<CommandRegisteredEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message);
		template<> void Observer<CommandExecutingEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message);
		template<> void Observer<CommandExecutedEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message);
		template<> void Observer<CommandErrorEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message);
		template<> void Observer<HelpRequestedEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message);

		// Type-safe observer subject (wraps DiaCore ObserverSubject)
		template<typename EventType>
		class ObserverSubject
		{
		public:
			ObserverSubject() {}

			void Attach(Observer<EventType>* observer)
			{
				mSubject.AttachToObserver(observer);
			}

			void Detach(Observer<EventType>* observer)
			{
				mSubject.DetachFromObserver(observer);
			}

			void Notify(const EventType& event);

		private:
			Dia::Core::ObserverSubject mSubject;
		};

		// Explicit specializations (implemented in .cpp)
		template<> void ObserverSubject<CommandRegisteredEvent>::Notify(const CommandRegisteredEvent& event);
		template<> void ObserverSubject<CommandExecutingEvent>::Notify(const CommandExecutingEvent& event);
		template<> void ObserverSubject<CommandExecutedEvent>::Notify(const CommandExecutedEvent& event);
		template<> void ObserverSubject<CommandErrorEvent>::Notify(const CommandErrorEvent& event);
		template<> void ObserverSubject<HelpRequestedEvent>::Notify(const HelpRequestedEvent& event);

		////////////////////////////////////////////////////////////////////////////////
		// Public API: Get Event Subjects
		////////////////////////////////////////////////////////////////////////////////

		ObserverSubject<CommandRegisteredEvent>& GetCommandRegisteredSubject();
		ObserverSubject<CommandExecutingEvent>& GetCommandExecutingSubject();
		ObserverSubject<CommandExecutedEvent>& GetCommandExecutedSubject();
		ObserverSubject<CommandErrorEvent>& GetCommandErrorSubject();
		ObserverSubject<HelpRequestedEvent>& GetHelpRequestedSubject();

		////////////////////////////////////////////////////////////////////////////////
		// Internal API: Fire Events
		////////////////////////////////////////////////////////////////////////////////

		namespace Internal
		{
			void FireCommandRegistered(const Dia::Core::StringCRC& name, const char* description);
			void FireCommandExecuting(const Dia::Core::StringCRC& name, const CommandArgs* args);
			void FireCommandExecuted(const Dia::Core::StringCRC& name, int exitCode, float duration);
			void FireCommandError(const Dia::Core::StringCRC& name, const char* errorMessage, int exitCode);
			void FireHelpRequested(const Dia::Core::StringCRC& commandName, bool isGlobalHelp);
		}
	}
}
