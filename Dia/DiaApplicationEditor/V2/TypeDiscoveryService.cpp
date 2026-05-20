// TypeDiscoveryService.cpp
// Reads types.json produced by DiaCLI and exposes the registered module/PU type lists.

#include "DiaApplicationEditor/V2/TypeDiscoveryService.h"

#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>
#include <string.h>

namespace Dia
{
    namespace ApplicationFlow
    {
        namespace Editor
        {
            // -----------------------------------------------------------------------
            // LoadFromFile
            // -----------------------------------------------------------------------
            void TypeDiscoveryService::LoadFromFile(const char* typesJsonPath)
            {
                Clear();

                if (!typesJsonPath)
                    return;

                // Open the file
                FILE* f = nullptr;
                fopen_s(&f, typesJsonPath, "rb");
                if (!f)
                    return;

                // Determine file size
                fseek(f, 0, SEEK_END);
                long fileSize = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (fileSize <= 0 || fileSize > 65536)
                {
                    fclose(f);
                    return;
                }

                // Read into heap buffer to avoid large stack allocation
                char* buffer = new char[static_cast<unsigned int>(fileSize) + 1];
                size_t bytesRead = fread(buffer, 1, static_cast<size_t>(fileSize), f);
                fclose(f);
                buffer[bytesRead] = '\0';

                // Parse JSON
                Json::Reader reader;
                Json::Value  root;
                bool parsed = reader.parse(buffer, root);
                delete[] buffer;

                if (!parsed)
                    return;

                // Populate module types
                const Json::Value& modules = root["modules"];
                if (modules.isArray())
                {
                    for (Json::ArrayIndex i = 0; i < modules.size() && mModuleTypes.Size() < kMaxModuleTypes; ++i)
                    {
                        const Json::Value& entry = modules[i];
                        if (!entry.isObject())
                            continue;

                        TypeInfo info;
                        info.typeId = Dia::Core::StringCRC(entry["type_id"].asCString());

                        const char* desc = entry["description"].asCString();
                        strncpy_s(info.description, sizeof(info.description), desc, _TRUNCATE);

                        mModuleTypes.Add(info);
                    }
                }

                // Populate processing unit types
                const Json::Value& pus = root["processing_units"];
                if (pus.isArray())
                {
                    for (Json::ArrayIndex i = 0; i < pus.size() && mPUTypes.Size() < kMaxPUTypes; ++i)
                    {
                        const Json::Value& entry = pus[i];
                        if (!entry.isObject())
                            continue;

                        TypeInfo info;
                        info.typeId = Dia::Core::StringCRC(entry["type_id"].asCString());

                        const char* desc = entry["description"].asCString();
                        strncpy_s(info.description, sizeof(info.description), desc, _TRUNCATE);

                        mPUTypes.Add(info);
                    }
                }
            }

            // -----------------------------------------------------------------------
            // Clear
            // -----------------------------------------------------------------------
            void TypeDiscoveryService::Clear()
            {
                mModuleTypes.RemoveAll();
                mPUTypes.RemoveAll();
            }

            // -----------------------------------------------------------------------
            // Refresh
            // -----------------------------------------------------------------------
            void TypeDiscoveryService::Refresh(const char* typesJsonPath)
            {
                Clear();
                LoadFromFile(typesJsonPath);
            }

            // -----------------------------------------------------------------------
            // Accessors
            // -----------------------------------------------------------------------
            unsigned int TypeDiscoveryService::GetModuleTypeCount() const
            {
                return mModuleTypes.Size();
            }

            const TypeDiscoveryService::TypeInfo* TypeDiscoveryService::GetModuleTypes() const
            {
                if (mModuleTypes.Size() == 0)
                    return nullptr;
                return &mModuleTypes[0u];
            }

            unsigned int TypeDiscoveryService::GetPUTypeCount() const
            {
                return mPUTypes.Size();
            }

            const TypeDiscoveryService::TypeInfo* TypeDiscoveryService::GetPUTypes() const
            {
                if (mPUTypes.Size() == 0)
                    return nullptr;
                return &mPUTypes[0u];
            }

            // -----------------------------------------------------------------------
            // Lookup
            // -----------------------------------------------------------------------
            bool TypeDiscoveryService::IsKnownModuleType(Dia::Core::StringCRC typeId) const
            {
                for (unsigned int i = 0; i < mModuleTypes.Size(); ++i)
                {
                    if (mModuleTypes[i].typeId == typeId)
                        return true;
                }
                return false;
            }

            bool TypeDiscoveryService::IsKnownPUType(Dia::Core::StringCRC typeId) const
            {
                for (unsigned int i = 0; i < mPUTypes.Size(); ++i)
                {
                    if (mPUTypes[i].typeId == typeId)
                        return true;
                }
                return false;
            }

            // -----------------------------------------------------------------------
            // IsLoaded
            // -----------------------------------------------------------------------
            bool TypeDiscoveryService::IsLoaded() const
            {
                return mModuleTypes.Size() > 0 || mPUTypes.Size() > 0;
            }

        } // namespace Editor
    } // namespace ApplicationFlow
} // namespace Dia
