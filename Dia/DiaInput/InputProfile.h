////////////////////////////////////////////////////////////////////////////////
// Filename: InputProfile.h - Save/load ActionMap bindings to JSON
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <json/json.h>
#include <fstream>
#include "DiaInput/ActionMap.h"
#include "DiaInput/EKey.h"
#include "DiaInput/EMouseButton.h"
#include "DiaInput/EJoystick.h"
#include "DiaInput/ConsoleGamepad.h"

namespace Dia
{
	namespace Input
	{
		/// @brief Input profile for saving/loading ActionMap bindings
		///
		/// InputProfile serializes ActionMap bindings to JSON format, allowing
		/// users to save custom key bindings and load them across sessions.
		///
		/// **JSON Format:**
		/// @code
		/// {
		///     "profile_name": "Player1",
		///     "bindings": [
		///         {
		///             "action": "Jump",
		///             "type": "Key",
		///             "code": 57,  // EKey::Space
		///             "device": 0
		///         },
		///         {
		///             "action": "Jump",
		///             "type": "GamepadButton",
		///             "code": 0,  // ConsoleGamepad::EButtonID::A
		///             "device": 0
		///         }
		///     ]
		/// }
		/// @endcode
		///
		/// **Usage:**
		/// @code
		/// ActionMap actionMap;
		/// actionMap.BindKey(ActionID("Jump"), EKey::Space);
		///
		/// // Save profile
		/// InputProfile::SaveProfile(actionMap, "player1.json", "Player1");
		///
		/// // Load profile
		/// ActionMap loadedMap;
		/// InputProfile::LoadProfile(loadedMap, "player1.json");
		/// @endcode
		////////////////////////////////////////////////////////////////////////////////
		class InputProfile
		{
		public:
			/// @brief Save ActionMap bindings to JSON file
			///
			/// @param actionMap ActionMap to save
			/// @param filePath File path to save to
			/// @param profileName Optional profile name (default: "Default")
			/// @return true if successful, false on error
			static bool SaveProfile(const ActionMap& actionMap, const char* filePath, const char* profileName = "Default")
			{
				using namespace Dia::Core;

				// Create root JSON object
				Json::Value root;
				root["profile_name"] = profileName;

				// Create bindings array
				Json::Value bindingsArray(Json::arrayValue);

				// Iterate through all bindings
				const auto& bindings = actionMap.GetBindings();
				for (auto it = bindings.Begin(); it != bindings.End(); ++it)
				{
					ActionID action = it.Key();
					const auto& bindingList = it.Value();

					for (unsigned int i = 0; i < bindingList.Size(); i++)
					{
						const ActionMap::Binding& binding = bindingList[i];

						// Create binding JSON object
						Json::Value bindingObj;
						bindingObj["action"] = action.AsChar();
						bindingObj["type"] = BindingTypeToString(binding.type);
						bindingObj["code"] = binding.code;
						bindingObj["device"] = static_cast<int>(binding.deviceIndex);

						bindingsArray.append(bindingObj);
					}
				}

				root["bindings"] = bindingsArray;

				// Write JSON to file using standard jsoncpp
				std::ofstream file(filePath);
				if (!file.is_open())
					return false;

				Json::StyledWriter writer;
				file << writer.write(root);
				file.close();
				return true;
			}

			/// @brief Load ActionMap bindings from JSON file
			///
			/// @param actionMap ActionMap to populate (cleared first)
			/// @param filePath File path to load from
			/// @return true if successful, false on error
			static bool LoadProfile(ActionMap& actionMap, const char* filePath)
			{
				using namespace Dia::Core;

				// Read JSON from file using standard jsoncpp
				std::ifstream file(filePath);
				if (!file.is_open())
				{
					return false;
				}

				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(file, root))
				{
					file.close();
					return false;
				}
				file.close();

				// Clear existing bindings
				actionMap.ClearAllBindings();

				// Read bindings array
				if (!root.isMember("bindings") || !root["bindings"].isArray())
				{
					return false;
				}

				const Json::Value& bindingsArray = root["bindings"];
				for (unsigned int i = 0; i < bindingsArray.size(); i++)
				{
					const Json::Value& bindingObj = bindingsArray[i];

					if (!bindingObj.isObject())
						continue;

					// Read action name
					if (!bindingObj.isMember("action") || !bindingObj["action"].isString())
						continue;

					const std::string actionString = bindingObj["action"].asString();
					ActionID action(actionString.c_str());

					// Read binding type
					if (!bindingObj.isMember("type") || !bindingObj["type"].isString())
						continue;

					const std::string typeString = bindingObj["type"].asString();
					ActionMap::Binding::Type type = StringToBindingType(typeString.c_str());

					// Read code and device
					if (!bindingObj.isMember("code") || !bindingObj["code"].isInt())
						continue;
					if (!bindingObj.isMember("device") || !bindingObj["device"].isInt())
						continue;

					int code = bindingObj["code"].asInt();
					unsigned int device = static_cast<unsigned int>(bindingObj["device"].asInt());

					// Create binding based on type
					switch (static_cast<int>(type))
					{
					case static_cast<int>(ActionMap::Binding::Type::Key):
						actionMap.BindKey(action, EKey::CreateFromInt(code));
						break;

					case static_cast<int>(ActionMap::Binding::Type::MouseButton):
						actionMap.BindMouseButton(action, EMouseButton::CreateFromInt(code));
						break;

					case static_cast<int>(ActionMap::Binding::Type::GamepadButton):
						actionMap.BindGamepadButton(action, device, ConsoleGamepad::EButtonID::CreateFromInt(code));
						break;

					case static_cast<int>(ActionMap::Binding::Type::JoystickButton):
						actionMap.BindJoystickButton(action, device, static_cast<unsigned int>(code));
						break;

					case static_cast<int>(ActionMap::Binding::Type::JoystickAxis):
						actionMap.BindJoystickAxis(action, device, EJoystickAxis::CreateFromInt(code));
						break;

					default:
						break;
					}
				}

				return true;
			}

			/// @brief Get profile name from JSON file without loading full profile
			///
			/// @param filePath File path to read
			/// @param outProfileName Output buffer for profile name (must be at least 256 bytes)
			/// @return true if successful, false on error
			static bool GetProfileName(const char* filePath, char* outProfileName, size_t bufferSize = 256)
			{
				using namespace Dia::Core;

				// Read JSON from file
				std::ifstream file(filePath);
				if (!file.is_open())
				{
					return false;
				}

				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(file, root))
				{
					file.close();
					return false;
				}
				file.close();

				if (root.isMember("profile_name") && root["profile_name"].isString())
				{
					const std::string& profileName = root["profile_name"].asString();
					if (profileName.length() < bufferSize)
					{
						strncpy(outProfileName, profileName.c_str(), bufferSize - 1);
						outProfileName[bufferSize - 1] = '\0';
						return true;
					}
				}

				return false;
			}

		private:
			/// @brief Convert binding type enum to string
			static const char* BindingTypeToString(ActionMap::Binding::Type type)
			{
				switch (type)
				{
				case ActionMap::Binding::Type::Key: return "Key";
				case ActionMap::Binding::Type::MouseButton: return "MouseButton";
				case ActionMap::Binding::Type::GamepadButton: return "GamepadButton";
				case ActionMap::Binding::Type::GamepadAxis: return "GamepadAxis";
				case ActionMap::Binding::Type::JoystickButton: return "JoystickButton";
				case ActionMap::Binding::Type::JoystickAxis: return "JoystickAxis";
				default: return "Unknown";
				}
			}

			/// @brief Convert string to binding type enum
			static ActionMap::Binding::Type StringToBindingType(const char* str)
			{
				if (strcmp(str, "Key") == 0) return ActionMap::Binding::Type::Key;
				if (strcmp(str, "MouseButton") == 0) return ActionMap::Binding::Type::MouseButton;
				if (strcmp(str, "GamepadButton") == 0) return ActionMap::Binding::Type::GamepadButton;
				if (strcmp(str, "GamepadAxis") == 0) return ActionMap::Binding::Type::GamepadAxis;
				if (strcmp(str, "JoystickButton") == 0) return ActionMap::Binding::Type::JoystickButton;
				if (strcmp(str, "JoystickAxis") == 0) return ActionMap::Binding::Type::JoystickAxis;
				return ActionMap::Binding::Type::Key;
			}
		};
	}
}
