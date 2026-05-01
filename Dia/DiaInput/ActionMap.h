////////////////////////////////////////////////////////////////////////////////
// Filename: ActionMap.h - Bind inputs to abstract actions
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Architecture/Events/Delegate.h>
#include "DiaInput/Event.h"
#include "DiaInput/EventData.h"
#include "DiaInput/EKey.h"
#include "DiaInput/EMouseButton.h"
#include "DiaInput/ConsoleGamepad.h"

namespace Dia
{
	namespace Input
	{
		using ActionID = Core::StringCRC;

		/// @brief Action mapping system for binding inputs to abstract actions
		///
		/// ActionMap allows binding multiple inputs (keys, mouse buttons, gamepad buttons)
		/// to named actions ("Jump", "Fire", "MoveForward"). This decouples gameplay
		/// logic from specific input devices.
		///
		/// **Usage:**
		/// @code
		/// ActionMap actionMap;
		///
		/// // Bind inputs to actions
		/// actionMap.BindKey(ActionID("Jump"), EKey::Space);
		/// actionMap.BindGamepadButton(ActionID("Jump"), 0, ConsoleGamepad::EButtonID::A);
		///
		/// // Subscribe to action callbacks
		/// actionMap.OnActionActivated([](ActionID action, float value) {
		///     if (action == ActionID("Jump") && value > 0.5f)
		///         player.Jump();
		/// });
		///
		/// // Each frame:
		/// EventData events;
		/// inputSourceManager.Update(events);
		/// actionMap.ProcessEvents(events);
		/// @endcode
		///
		/// **Action Values:**
		/// - Digital inputs (keys, buttons): 0.0 (released) or 1.0 (pressed)
		/// - Analog inputs (sticks, triggers): 0.0 to 1.0 range
		////////////////////////////////////////////////////////////////////////////////
		class ActionMap
		{
		public:
			using ActionCallback = Core::Events::Delegate<ActionID, float>;

			/// @brief Input binding entry
			struct Binding
			{
				enum class Type
				{
					Key,
					MouseButton,
					GamepadButton,
					GamepadAxis,
					JoystickButton,
					JoystickAxis
				};

				Type type;
				int code;           // Key code, mouse button, gamepad/joystick button, or axis index
				unsigned int deviceIndex;  // Gamepad/joystick index (0 for keyboard/mouse)

				Binding() : type(Type::Key), code(0), deviceIndex(0) {}

				Binding(Type t, int c, unsigned int devIdx = 0)
					: type(t), code(c), deviceIndex(devIdx) {}
			};

			ActionMap()
			{
				mBindings.SetSize(32, 32);      // Initialize with reasonable default size
				mActionValues.SetSize(32, 32);  // Initialize with reasonable default size
			}

			/// @brief Bind a keyboard key to an action
			///
			/// @param action Action identifier (e.g., ActionID("Jump"))
			/// @param key Key to bind
			void BindKey(ActionID action, EKey key)
			{
				Binding binding(Binding::Type::Key, static_cast<int>(key));
				AddBinding(action, binding);
			}

			/// @brief Bind a mouse button to an action
			///
			/// @param action Action identifier
			/// @param button Mouse button to bind
			void BindMouseButton(ActionID action, EMouseButton button)
			{
				Binding binding(Binding::Type::MouseButton, static_cast<int>(button));
				AddBinding(action, binding);
			}

			/// @brief Bind a gamepad button to an action
			///
			/// @param action Action identifier
			/// @param gamepadIndex Gamepad index (0-7)
			/// @param button Gamepad button to bind
			void BindGamepadButton(ActionID action, unsigned int gamepadIndex, ConsoleGamepad::EButtonID button)
			{
				Binding binding(Binding::Type::GamepadButton, static_cast<int>(button), gamepadIndex);
				AddBinding(action, binding);
			}

			/// @brief Bind a joystick button to an action
			///
			/// @param action Action identifier
			/// @param joystickIndex Joystick index (0-7)
			/// @param button Joystick button index
			void BindJoystickButton(ActionID action, unsigned int joystickIndex, unsigned int button)
			{
				Binding binding(Binding::Type::JoystickButton, button, joystickIndex);
				AddBinding(action, binding);
			}

			/// @brief Bind a joystick axis to an action
			///
			/// @param action Action identifier
			/// @param joystickIndex Joystick index (0-7)
			/// @param axis Joystick axis to bind
			void BindJoystickAxis(ActionID action, unsigned int joystickIndex, EJoystickAxis axis)
			{
				Binding binding(Binding::Type::JoystickAxis, static_cast<int>(axis), joystickIndex);
				AddBinding(action, binding);
			}

			/// @brief Unbind all inputs from an action
			///
			/// @param action Action identifier
			void UnbindAction(ActionID action)
			{
				mBindings.Erase(action);
			}

			/// @brief Clear all bindings
			///
			/// Removes all action bindings. Use before loading a profile.
			void ClearAllBindings()
			{
				mBindings.Clear();
				mActionValues.Clear();
			}

			/// @brief Get all bindings (for serialization)
			///
			/// @return Reference to bindings hash table
			const Core::Containers::HashTable<ActionID, Core::Containers::DynamicArrayC<Binding, 4>>& GetBindings() const
			{
				return mBindings;
			}

			/// @brief Save bindings to JSON profile file
			///
			/// @param filePath File path to save to
			/// @param profileName Optional profile name (default: "Default")
			/// @return true if successful, false on error
			///
			/// Convenience method that wraps InputProfile::SaveProfile().
			bool SaveProfile(const char* filePath, const char* profileName = "Default") const;

			/// @brief Load bindings from JSON profile file
			///
			/// @param filePath File path to load from
			/// @return true if successful, false on error
			///
			/// Clears existing bindings before loading.
			/// Convenience method that wraps InputProfile::LoadProfile().
			bool LoadProfile(const char* filePath);

			/// @brief Check if an action is currently active
			///
			/// @param action Action identifier
			/// @return true if any bound input is active, false otherwise
			bool IsActionActive(ActionID action) const
			{
				float value = GetActionValue(action);
				return value > 0.0f;
			}

			/// @brief Get action value (analog)
			///
			/// @param action Action identifier
			/// @return Value in range [0.0, 1.0], or 0.0 if inactive
			float GetActionValue(ActionID action) const
			{
				if (!mActionValues.Contains(action))
				{
					return 0.0f;
				}

				return mActionValues[action];
			}

			/// @brief Process input events to update action state
			///
			/// @param events EventData from InputSourceManager
			///
			/// Call this each frame after polling input sources.
			void ProcessEvents(const EventData& events)
			{
				// Clear all action values
				for (auto it = mActionValues.Begin(); it != mActionValues.End(); ++it)
				{
					it.Value() = 0.0f;
				}

				// Process events and update action values
				for (unsigned int i = 0; i < events.Size(); i++)
				{
					const Event& evt = events[i];

					switch (evt.type.m_Value)
					{
					case Event::EType::kKeyPressed:
					case Event::EType::kKeyReleased:
					{
						EKey key = evt.key.AsKey();
						float value = (evt.type.m_Value == Event::EType::kKeyPressed) ? 1.0f : 0.0f;
						UpdateActionsForKey(key, value);
						break;
					}

					case Event::EType::kMouseButtonPressed:
					case Event::EType::kMouseButtonReleased:
					{
						EMouseButton button = evt.mouseButton.AsMouseButton();
						float value = (evt.type.m_Value == Event::EType::kMouseButtonPressed) ? 1.0f : 0.0f;
						UpdateActionsForMouseButton(button, value);
						break;
					}

					case Event::EType::kConsoleGamepadButtonPressed:
					case Event::EType::kConsoleGamepadButtonReleased:
					{
						unsigned int gamepadIdx = evt.consoleGamepadButtonEvent.gamepadIndex;
						ConsoleGamepad::EButtonID button = evt.consoleGamepadButtonEvent.AsButtonId();
						float value = (evt.type.m_Value == Event::EType::kConsoleGamepadButtonPressed) ? 1.0f : 0.0f;
						UpdateActionsForGamepadButton(gamepadIdx, button, value);
						break;
					}

					case Event::EType::kJoystickButtonPressed:
					case Event::EType::kJoystickButtonReleased:
					{
						unsigned int joystickIdx = evt.joystickButton.joystickId;
						unsigned int button = evt.joystickButton.button;
						float value = (evt.type.m_Value == Event::EType::kJoystickButtonPressed) ? 1.0f : 0.0f;
						UpdateActionsForJoystickButton(joystickIdx, button, value);
						break;
					}

					case Event::EType::kJoystickMoved:
					{
						unsigned int joystickIdx = evt.joystickMove.joystickId;
						EJoystickAxis axis = evt.joystickMove.AsJoystickAxis();
						float value = evt.joystickMove.position / 100.0f;  // SFML returns -100 to 100, normalize to -1 to 1
						UpdateActionsForJoystickAxis(joystickIdx, axis, value);
						break;
					}

					default:
						break;
					}
				}

				// Fire callbacks for active actions
				for (auto it = mActionValues.Begin(); it != mActionValues.End(); ++it)
				{
					if (it.Value() > 0.0f)
					{
						mActionActivatedCallback.Invoke(it.Key(), it.Value());
					}
				}
			}

			/// @brief Subscribe to action activation events
			///
			/// @param callback Function to call when actions are activated
			/// @return Callback ID for later unsubscription
			ActionCallback::CallbackID OnActionActivated(ActionCallback::CallbackFunc callback)
			{
				return mActionActivatedCallback.Add(callback);
			}

			/// @brief Unsubscribe from action activation events
			///
			/// @param callbackId Callback ID returned by OnActionActivated()
			void RemoveCallback(ActionCallback::CallbackID callbackId)
			{
				mActionActivatedCallback.Remove(callbackId);
			}

		private:
			void AddBinding(ActionID action, const Binding& binding)
			{
				if (!mBindings.Contains(action))
				{
					mBindings.Insert(action, Core::Containers::DynamicArrayC<Binding, 4>());
				}

				mBindings[action].Add(binding);
			}

			void UpdateActionsForKey(EKey key, float value)
			{
				int keyCode = static_cast<int>(key);

				for (auto it = mBindings.Begin(); it != mBindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindings = it.Value();

					for (unsigned int i = 0; i < bindings.Size(); i++)
					{
						if (bindings[i].type == Binding::Type::Key &&
							bindings[i].code == keyCode)
						{
							SetActionValue(action, value);
							break;
						}
					}
				}
			}

			void UpdateActionsForMouseButton(EMouseButton button, float value)
			{
				int buttonCode = static_cast<int>(button);

				for (auto it = mBindings.Begin(); it != mBindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindings = it.Value();

					for (unsigned int i = 0; i < bindings.Size(); i++)
					{
						if (bindings[i].type == Binding::Type::MouseButton &&
							bindings[i].code == buttonCode)
						{
							SetActionValue(action, value);
							break;
						}
					}
				}
			}

			void UpdateActionsForGamepadButton(unsigned int gamepadIndex, ConsoleGamepad::EButtonID button, float value)
			{
				int buttonCode = static_cast<int>(button);

				for (auto it = mBindings.Begin(); it != mBindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindings = it.Value();

					for (unsigned int i = 0; i < bindings.Size(); i++)
					{
						if (bindings[i].type == Binding::Type::GamepadButton &&
							bindings[i].code == buttonCode &&
							bindings[i].deviceIndex == gamepadIndex)
						{
							SetActionValue(action, value);
							break;
						}
					}
				}
			}

			void UpdateActionsForJoystickButton(unsigned int joystickIndex, unsigned int button, float value)
			{
				for (auto it = mBindings.Begin(); it != mBindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindings = it.Value();

					for (unsigned int i = 0; i < bindings.Size(); i++)
					{
						if (bindings[i].type == Binding::Type::JoystickButton &&
							bindings[i].code == static_cast<int>(button) &&
							bindings[i].deviceIndex == joystickIndex)
						{
							SetActionValue(action, value);
							break;
						}
					}
				}
			}

			void UpdateActionsForJoystickAxis(unsigned int joystickIndex, EJoystickAxis axis, float value)
			{
				int axisCode = static_cast<int>(axis);

				for (auto it = mBindings.Begin(); it != mBindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindings = it.Value();

					for (unsigned int i = 0; i < bindings.Size(); i++)
					{
						if (bindings[i].type == Binding::Type::JoystickAxis &&
							bindings[i].code == axisCode &&
							bindings[i].deviceIndex == joystickIndex)
						{
							SetActionValue(action, value);
							break;
						}
					}
				}
			}

			void SetActionValue(ActionID action, float value)
			{
				if (!mActionValues.Contains(action))
				{
					mActionValues.Insert(action, value);
				}
				else
				{
					mActionValues[action] = value;
				}
			}

			// Bindings: ActionID -> List of input bindings
			Core::Containers::HashTable<ActionID, Core::Containers::DynamicArrayC<Binding, 4>> mBindings;

			// Current action values (updated each frame)
			Core::Containers::HashTable<ActionID, float> mActionValues;

			// Callback for action activation
			ActionCallback mActionActivatedCallback;
		};

	}
}
