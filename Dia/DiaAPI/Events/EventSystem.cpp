////////////////////////////////////////////////////////////////////////////////
// Filename: EventSystem.cpp
// Description: Event system implementation
// Feature spec: docs/specs/features/dia/diacli/event-system.md
////////////////////////////////////////////////////////////////////////////////
#include "EventSystem.h"
#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace API
	{
		////////////////////////////////////////////////////////////////////////////////
		// Thread-local storage for event data (safe for multi-threaded event firing)
		////////////////////////////////////////////////////////////////////////////////

		namespace Internal
		{
			// Thread-local storage for each event type
			thread_local const CommandRegisteredEvent* gCurrentCommandRegisteredEvent = nullptr;
			thread_local const CommandExecutingEvent* gCurrentCommandExecutingEvent = nullptr;
			thread_local const CommandExecutedEvent* gCurrentCommandExecutedEvent = nullptr;
			thread_local const CommandErrorEvent* gCurrentCommandErrorEvent = nullptr;
			thread_local const HelpRequestedEvent* gCurrentHelpRequestedEvent = nullptr;

			// Accessors for thread-local storage
			const CommandRegisteredEvent* GetCurrentCommandRegisteredEvent()
			{
				return gCurrentCommandRegisteredEvent;
			}

			const CommandExecutingEvent* GetCurrentCommandExecutingEvent()
			{
				return gCurrentCommandExecutingEvent;
			}

			const CommandExecutedEvent* GetCurrentCommandExecutedEvent()
			{
				return gCurrentCommandExecutedEvent;
			}

			const CommandErrorEvent* GetCurrentCommandErrorEvent()
			{
				return gCurrentCommandErrorEvent;
			}

			const HelpRequestedEvent* GetCurrentHelpRequestedEvent()
			{
				return gCurrentHelpRequestedEvent;
			}

			void SetCurrentCommandRegisteredEvent(const CommandRegisteredEvent* event)
			{
				gCurrentCommandRegisteredEvent = event;
			}

			void SetCurrentCommandExecutingEvent(const CommandExecutingEvent* event)
			{
				gCurrentCommandExecutingEvent = event;
			}

			void SetCurrentCommandExecutedEvent(const CommandExecutedEvent* event)
			{
				gCurrentCommandExecutedEvent = event;
			}

			void SetCurrentCommandErrorEvent(const CommandErrorEvent* event)
			{
				gCurrentCommandErrorEvent = event;
			}

			void SetCurrentHelpRequestedEvent(const HelpRequestedEvent* event)
			{
				gCurrentHelpRequestedEvent = event;
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Observer<T>::ObserverNotification specializations
		////////////////////////////////////////////////////////////////////////////////

		template<>
		void Observer<CommandRegisteredEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			const CommandRegisteredEvent* eventData = Internal::GetCurrentCommandRegisteredEvent();
			if (eventData)
			{
				Notify(*eventData);
			}
		}

		template<>
		void Observer<CommandExecutingEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			const CommandExecutingEvent* eventData = Internal::GetCurrentCommandExecutingEvent();
			if (eventData)
			{
				Notify(*eventData);
			}
		}

		template<>
		void Observer<CommandExecutedEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			const CommandExecutedEvent* eventData = Internal::GetCurrentCommandExecutedEvent();
			if (eventData)
			{
				Notify(*eventData);
			}
		}

		template<>
		void Observer<CommandErrorEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			const CommandErrorEvent* eventData = Internal::GetCurrentCommandErrorEvent();
			if (eventData)
			{
				Notify(*eventData);
			}
		}

		template<>
		void Observer<HelpRequestedEvent>::ObserverNotification(const Dia::Core::ObserverSubject* subject, int message)
		{
			const HelpRequestedEvent* eventData = Internal::GetCurrentHelpRequestedEvent();
			if (eventData)
			{
				Notify(*eventData);
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// ObserverSubject<T>::Notify specializations
		////////////////////////////////////////////////////////////////////////////////

		template<>
		void ObserverSubject<CommandRegisteredEvent>::Notify(const CommandRegisteredEvent& event)
		{
			Internal::SetCurrentCommandRegisteredEvent(&event);
			mSubject.NotifyObservers(0);
			Internal::SetCurrentCommandRegisteredEvent(nullptr);
		}

		template<>
		void ObserverSubject<CommandExecutingEvent>::Notify(const CommandExecutingEvent& event)
		{
			Internal::SetCurrentCommandExecutingEvent(&event);
			mSubject.NotifyObservers(0);
			Internal::SetCurrentCommandExecutingEvent(nullptr);
		}

		template<>
		void ObserverSubject<CommandExecutedEvent>::Notify(const CommandExecutedEvent& event)
		{
			Internal::SetCurrentCommandExecutedEvent(&event);
			mSubject.NotifyObservers(0);
			Internal::SetCurrentCommandExecutedEvent(nullptr);
		}

		template<>
		void ObserverSubject<CommandErrorEvent>::Notify(const CommandErrorEvent& event)
		{
			Internal::SetCurrentCommandErrorEvent(&event);
			mSubject.NotifyObservers(0);
			Internal::SetCurrentCommandErrorEvent(nullptr);
		}

		template<>
		void ObserverSubject<HelpRequestedEvent>::Notify(const HelpRequestedEvent& event)
		{
			Internal::SetCurrentHelpRequestedEvent(&event);
			mSubject.NotifyObservers(0);
			Internal::SetCurrentHelpRequestedEvent(nullptr);
		}

		////////////////////////////////////////////////////////////////////////////////
		// Event Subject Singletons
		////////////////////////////////////////////////////////////////////////////////

		namespace Internal
		{
			struct EventSystemState
			{
				ObserverSubject<CommandRegisteredEvent> commandRegisteredSubject;
				ObserverSubject<CommandExecutingEvent> commandExecutingSubject;
				ObserverSubject<CommandExecutedEvent> commandExecutedSubject;
				ObserverSubject<CommandErrorEvent> commandErrorSubject;
				ObserverSubject<HelpRequestedEvent> helpRequestedSubject;
			};

			EventSystemState gEventSystemState;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Public API: Get Event Subjects
		////////////////////////////////////////////////////////////////////////////////

		ObserverSubject<CommandRegisteredEvent>& GetCommandRegisteredSubject()
		{
			return Internal::gEventSystemState.commandRegisteredSubject;
		}

		ObserverSubject<CommandExecutingEvent>& GetCommandExecutingSubject()
		{
			return Internal::gEventSystemState.commandExecutingSubject;
		}

		ObserverSubject<CommandExecutedEvent>& GetCommandExecutedSubject()
		{
			return Internal::gEventSystemState.commandExecutedSubject;
		}

		ObserverSubject<CommandErrorEvent>& GetCommandErrorSubject()
		{
			return Internal::gEventSystemState.commandErrorSubject;
		}

		ObserverSubject<HelpRequestedEvent>& GetHelpRequestedSubject()
		{
			return Internal::gEventSystemState.helpRequestedSubject;
		}

		////////////////////////////////////////////////////////////////////////////////
		// Internal API: Fire Events
		////////////////////////////////////////////////////////////////////////////////

		namespace Internal
		{
			void FireCommandRegistered(const Dia::Core::StringCRC& name, const char* description)
			{
				CommandRegisteredEvent event;
				event.commandName = name;
				event.description = description;

				gEventSystemState.commandRegisteredSubject.Notify(event);
			}

			void FireCommandExecuting(const Dia::Core::StringCRC& name, const CommandArgs* args)
			{
				CommandExecutingEvent event;
				event.commandName = name;
				event.args = args;

				gEventSystemState.commandExecutingSubject.Notify(event);
			}

			void FireCommandExecuted(const Dia::Core::StringCRC& name, int exitCode, float duration)
			{
				CommandExecutedEvent event;
				event.commandName = name;
				event.exitCode = exitCode;
				event.durationSeconds = duration;

				gEventSystemState.commandExecutedSubject.Notify(event);
			}

			void FireCommandError(const Dia::Core::StringCRC& name, const char* errorMessage, int exitCode)
			{
				CommandErrorEvent event;
				event.commandName = name;
				event.errorMessage = errorMessage;
				event.exitCode = exitCode;

				gEventSystemState.commandErrorSubject.Notify(event);
			}

			void FireHelpRequested(const Dia::Core::StringCRC& commandName, bool isGlobalHelp)
			{
				HelpRequestedEvent event;
				event.commandName = commandName;
				event.isGlobalHelp = isGlobalHelp;

				gEventSystemState.helpRequestedSubject.Notify(event);
			}
		}
	}
}
