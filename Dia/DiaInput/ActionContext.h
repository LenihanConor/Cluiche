////////////////////////////////////////////////////////////////////////////////
// Filename: ActionContext.h - Context-sensitive input management
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTable/HashTable.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include "DiaInput/ActionMap.h"

namespace Dia
{
	namespace Input
	{
		using ContextID = Core::StringCRC;

		/// @brief Action context for context-sensitive input
		///
		/// ActionContext represents a named input context (e.g., "Menu", "Gameplay", "Inventory")
		/// with its own set of action bindings. Contexts can be stacked, allowing hierarchical
		/// input handling where higher contexts override lower contexts.
		///
		/// **Use Cases:**
		/// - Menu overlays that capture input before gameplay
		/// - Different control schemes for different game modes
		/// - Modal dialogs that temporarily block gameplay input
		///
		/// **Example:**
		/// @code
		/// ActionContext gameplayContext(ContextID("Gameplay"));
		/// gameplayContext.BindKey(ActionID("Jump"), EKey::Space);
		/// gameplayContext.BindKey(ActionID("Fire"), EKey::MouseLeft);
		///
		/// ActionContext menuContext(ContextID("Menu"));
		/// menuContext.BindKey(ActionID("Navigate"), EKey::Up);
		/// menuContext.BindKey(ActionID("Select"), EKey::Enter);
		///
		/// ActionContextManager manager;
		/// manager.PushContext(gameplayContext);  // Base context
		/// manager.PushContext(menuContext);      // Menu overrides gameplay
		///
		/// // Process events (menu context is checked first)
		/// manager.ProcessEvents(events);
		///
		/// manager.PopContext();  // Back to gameplay
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class ActionContext
		{
		public:
			/// @brief Constructor with context ID
			///
			/// @param contextId Unique identifier for this context
			/// @param blockLower If true, prevents lower contexts from processing input
			ActionContext(ContextID contextId, bool blockLower = false)
				: mContextId(contextId)
				, mBlockLowerContexts(blockLower)
			{}

			/// @brief Get context identifier
			ContextID GetContextId() const { return mContextId; }

			/// @brief Check if this context blocks lower contexts
			bool BlocksLowerContexts() const { return mBlockLowerContexts; }

			/// @brief Set whether this context blocks lower contexts
			void SetBlockLowerContexts(bool block) { mBlockLowerContexts = block; }

			/// @brief Get the action map for this context
			ActionMap& GetActionMap() { return mActionMap; }
			const ActionMap& GetActionMap() const { return mActionMap; }

			// Convenience methods that forward to internal ActionMap
			void BindKey(ActionID action, EKey key)
			{
				mActionMap.BindKey(action, key);
			}

			void BindMouseButton(ActionID action, EMouseButton button)
			{
				mActionMap.BindMouseButton(action, button);
			}

			void BindGamepadButton(ActionID action, unsigned int gamepadIndex, ConsoleGamepad::EButtonID button)
			{
				mActionMap.BindGamepadButton(action, gamepadIndex, button);
			}

			void BindJoystickButton(ActionID action, unsigned int joystickIndex, unsigned int button)
			{
				mActionMap.BindJoystickButton(action, joystickIndex, button);
			}

			void BindJoystickAxis(ActionID action, unsigned int joystickIndex, EJoystickAxis axis)
			{
				mActionMap.BindJoystickAxis(action, joystickIndex, axis);
			}

			void UnbindAction(ActionID action)
			{
				mActionMap.UnbindAction(action);
			}

		private:
			ContextID mContextId;
			bool mBlockLowerContexts;
			ActionMap mActionMap;
		};

		/// @brief Manager for context stack
		///
		/// ActionContextManager maintains a stack of ActionContexts and processes
		/// input events from top to bottom, respecting blocking behavior.
		///
		/// **Context Priority:**
		/// - Top context is processed first (highest priority)
		/// - If a context has `blockLower = true`, lower contexts are skipped
		/// - If an action is handled in a higher context, it won't reach lower contexts
		///
		/// **Example:**
		/// @code
		/// ActionContextManager manager;
		///
		/// // Setup contexts
		/// ActionContext* gameplay = manager.CreateContext(ContextID("Gameplay"), false);
		/// gameplay->BindKey(ActionID("Jump"), EKey::Space);
		///
		/// ActionContext* menu = manager.CreateContext(ContextID("Menu"), true);  // Blocks gameplay
		/// menu->BindKey(ActionID("Navigate"), EKey::Up);
		///
		/// // Start in gameplay mode
		/// manager.PushContext(gameplay);
		///
		/// // Open menu (blocks gameplay input)
		/// manager.PushContext(menu);
		///
		/// // Each frame:
		/// EventData events;
		/// inputSourceManager.Update(events);
		/// manager.ProcessEvents(events);
		///
		/// // Query actions from active context
		/// if (manager.IsActionActive(ActionID("Navigate")))
		///     MoveMenuCursor();
		///
		/// // Close menu
		/// manager.PopContext();
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class ActionContextManager
		{
		public:
			ActionContextManager() {}

			/// @brief Create a new context (managed by this manager)
			///
			/// @param contextId Unique identifier for the context
			/// @param blockLower Whether this context blocks lower contexts
			/// @return Pointer to created context (owned by manager)
			ActionContext* CreateContext(ContextID contextId, bool blockLower = false)
			{
				ActionContext context(contextId, blockLower);
				mContexts.Insert(contextId, context);
				return &mContexts[contextId];
			}

			/// @brief Get a context by ID
			///
			/// @param contextId Context identifier
			/// @return Pointer to context, or nullptr if not found
			ActionContext* GetContext(ContextID contextId)
			{
				if (mContexts.Contains(contextId))
				{
					return &mContexts[contextId];
				}
				return nullptr;
			}

			/// @brief Push a context onto the stack (becomes active)
			///
			/// @param context Pointer to context (must be managed by this manager)
			void PushContext(ActionContext* context)
			{
				if (context)
				{
					mContextStack.Add(context);
				}
			}

			/// @brief Pop the top context from the stack
			///
			/// @return Pointer to popped context, or nullptr if stack is empty
			ActionContext* PopContext()
			{
				if (mContextStack.Size() > 0)
				{
					ActionContext* top = mContextStack[mContextStack.Size() - 1];
					mContextStack.RemoveAt(mContextStack.Size() - 1);
					return top;
				}
				return nullptr;
			}

			/// @brief Get the current active context (top of stack)
			///
			/// @return Pointer to active context, or nullptr if stack is empty
			ActionContext* GetActiveContext()
			{
				if (mContextStack.Size() > 0)
				{
					return mContextStack[mContextStack.Size() - 1];
				}
				return nullptr;
			}

			/// @brief Process input events through context stack
			///
			/// @param events EventData from InputSourceManager
			///
			/// Processes events from top to bottom of context stack, respecting blocking.
			void ProcessEvents(const EventData& events)
			{
				// Process from top to bottom of stack
				for (int i = mContextStack.Size() - 1; i >= 0; i--)
				{
					ActionContext* context = mContextStack[i];
					context->GetActionMap().ProcessEvents(events);

					// If this context blocks lower contexts, stop here
					if (context->BlocksLowerContexts())
					{
						break;
					}
				}
			}

			/// @brief Check if an action is active in the current context stack
			///
			/// @param action Action identifier
			/// @return true if action is active in any non-blocked context
			bool IsActionActive(ActionID action) const
			{
				// Check from top to bottom of stack
				for (int i = mContextStack.Size() - 1; i >= 0; i--)
				{
					const ActionContext* context = mContextStack[i];
					if (context->GetActionMap().IsActionActive(action))
					{
						return true;
					}

					// If this context blocks lower contexts, stop here
					if (context->BlocksLowerContexts())
					{
						break;
					}
				}
				return false;
			}

			/// @brief Get action value from current context stack
			///
			/// @param action Action identifier
			/// @return Action value (0.0-1.0), or 0.0 if not active
			float GetActionValue(ActionID action) const
			{
				// Check from top to bottom of stack
				for (int i = mContextStack.Size() - 1; i >= 0; i--)
				{
					const ActionContext* context = mContextStack[i];
					float value = context->GetActionMap().GetActionValue(action);
					if (value > 0.0f)
					{
						return value;
					}

					// If this context blocks lower contexts, stop here
					if (context->BlocksLowerContexts())
					{
						break;
					}
				}
				return 0.0f;
			}

			/// @brief Clear the context stack
			void ClearStack()
			{
				mContextStack.RemoveAll();
			}

			/// @brief Get stack size
			unsigned int GetStackSize() const
			{
				return mContextStack.Size();
			}

		private:
			Core::Containers::HashTable<ContextID, ActionContext> mContexts;
			Core::Containers::DynamicArrayC<ActionContext*, 8> mContextStack;
		};
	}
}
