#include "DiaBlueprintEditor/SchemaReader.h"
#include <DiaCore/Json/external/json/json.h>
#include <stdio.h>
#include <string.h>

namespace Dia { namespace BlueprintEditor {

    void SchemaReader::LoadFromFile(const char* schemaPath)
    {
        Clear();

        if (!schemaPath || schemaPath[0] == '\0')
            return;

        FILE* f = nullptr;
        fopen_s(&f, schemaPath, "rb");
        if (!f)
            return;

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            fclose(f);
            return;
        }

        char* buf = new char[static_cast<unsigned int>(fileSize) + 1];
        size_t bytesRead = fread(buf, 1, static_cast<size_t>(fileSize), f);
        fclose(f);
        buf[bytesRead] = '\0';

        Json::Value root;
        Json::Reader reader;
        bool ok = reader.parse(buf, buf + bytesRead, root);
        delete[] buf;

        if (!ok || !root.isObject())
            return;

        // Read version
        if (root.isMember("version") && root["version"].isObject())
        {
            mVersion.major = root["version"].get("major", 0).asInt();
            mVersion.minor = root["version"].get("minor", 0).asInt();
        }

        // Read components
        const Json::Value& components = root["components"];
        if (!components.isArray())
        {
            mLoaded = true;
            return;
        }

        for (unsigned int i = 0; i < components.size(); ++i)
        {
            if (mComponents.IsFull())
                break;

            const Json::Value& comp = components[i];
            if (!comp.isObject())
                continue;

            SchemaComponentEntry entry;
            entry.typeId = Dia::Core::StringCRC(comp.get("type_id", "").asCString());
            strncpy_s(entry.debugName, sizeof(entry.debugName),
                      comp.get("debug_name", "").asCString(), _TRUNCATE);

            const Json::Value& fields = comp["fields"];
            if (fields.isArray())
            {
                for (unsigned int f = 0; f < fields.size(); ++f)
                {
                    if (entry.fields.IsFull())
                        break;

                    const Json::Value& field = fields[f];
                    if (!field.isObject())
                        continue;

                    SchemaFieldEntry fieldEntry;
                    strncpy_s(fieldEntry.name, sizeof(fieldEntry.name),
                              field.get("name", "").asCString(), _TRUNCATE);
                    strncpy_s(fieldEntry.kind, sizeof(fieldEntry.kind),
                              field.get("kind", "primitive").asCString(), _TRUNCATE);
                    entry.fields.Add(fieldEntry);
                }
            }

            mComponents.Add(entry);
        }

        mLoaded = true;
    }

    void SchemaReader::Clear()
    {
        mComponents.RemoveAll();
        mVersion = Version{};
        mLoaded = false;
    }

    bool SchemaReader::IsLoaded() const
    {
        return mLoaded;
    }

    unsigned int SchemaReader::GetComponentCount() const
    {
        return mComponents.Size();
    }

    const SchemaComponentEntry& SchemaReader::GetComponent(unsigned int i) const
    {
        return mComponents[i];
    }

    SchemaReader::Version SchemaReader::GetVersion() const
    {
        return mVersion;
    }

}} // namespace Dia::BlueprintEditor
