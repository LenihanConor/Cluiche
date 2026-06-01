////////////////////////////////////////////////////////////////////////////////
// Filename: SceneLoader2D.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaScene2D/SceneLoader2D.h"
#include "DiaScene2D/Scene2D.h"
#include "DiaScene2D/DiaScene2DSerializers.h"

#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaCamera2D/Camera2D.h>
#include <DiaCamera2D/Registry/CameraRegistry2D.h>
#include <DiaLighting2D/PointLight2D.h>
#include <DiaLighting2D/Registry/LightRegistry2D.h>
#include <DiaEntity/Domain.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
    namespace Scene2D
    {
        SceneLoader2D::SceneLoader2D()
        {
        }

        bool SceneLoader2D::Load(const char*       filePath,
                                 SceneLoadContext& context,
                                 LayerTable&       outLayers,
                                 SceneLoadErrors*  outErrors)
        {
            // --- Parse JSON ---
            Json::Value   root;
            Json::Reader  reader;

            FILE* f = fopen(filePath, "rb");
            if (!f)
            {
                if (outErrors) outErrors->hasErrors = true;
                return false;
            }
            fseek(f, 0, SEEK_END);
            long fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fileSize <= 0)
            {
                fclose(f);
                if (outErrors) outErrors->hasErrors = true;
                return false;
            }

            char* buf = new char[static_cast<size_t>(fileSize) + 1];
            fread(buf, 1, static_cast<size_t>(fileSize), f);
            buf[fileSize] = '\0';
            fclose(f);

            bool parsed = reader.parse(buf, root, false);
            delete[] buf;

            if (!parsed || !root.isMember("scene2d"))
            {
                if (outErrors) outErrors->hasErrors = true;
                return false;
            }

            // --- Deserialize Scene2D struct ---
            Scene2D scene;
            {
                Dia::Reflect::JsonReadArchive ar(root["scene2d"]);
                serialize(ar, scene, 1u);
                if (ar.GetResult().HasErrors())
                {
                    if (outErrors) outErrors->hasErrors = true;
                    // Non-fatal: continue with partial data
                }
            }

            // --- Build LayerTable ---
            outLayers.Build(scene.layers);

            // --- Validate cameras ---
            int activeCameraCount = 0;
            for (unsigned int i = 0; i < scene.cameras.Size(); ++i)
            {
                if (scene.cameras.At(i).active)
                    ++activeCameraCount;
            }
            if (activeCameraCount != 1)
            {
                if (outErrors) outErrors->hasErrors = true;
                return false;
            }

            // --- Hydrate cameras ---
            for (unsigned int i = 0; i < scene.cameras.Size(); ++i)
            {
                const CameraEntry& entry = scene.cameras.At(i);

                // v1: construct Camera2D with defaults; apply instanceData patches
                Dia::Camera2D::Camera2D cam;

                if (entry.instanceData.isObject())
                {
                    // Patch Camera2D fields from instanceData "Camera2D.fieldName"
                    for (const auto& key : entry.instanceData.getMemberNames())
                    {
                        const std::string& keyStr = key;
                        if (keyStr.rfind("Camera2D.", 0) == 0)
                        {
                            const std::string field = keyStr.substr(9);
                            const Json::Value& val  = entry.instanceData[key];
                            if (field == "position" && val.isArray() && val.size() >= 2)
                                cam.SetPosition(Dia::Maths::Vector2D(val[0].asFloat(), val[1].asFloat()));
                            else if (field == "zoom" && val.isNumeric())
                                cam.SetZoom(val.asFloat());
                            else if (field == "rotation" && val.isNumeric())
                                cam.SetRotation(val.asFloat());
                        }
                    }
                }

                context.cameraRegistry.Register(entry.id, cam);
                if (entry.active)
                    context.cameraRegistry.SetActive(entry.id);

                mRegisteredCameras.Add(entry.id);
            }

            // --- Hydrate lights ---
            for (unsigned int i = 0; i < scene.lights.Size(); ++i)
            {
                const LightEntry& entry = scene.lights.At(i);

                Dia::Lighting2D::PointLight2D light;
                light.enabled   = entry.enabled;
                light.layerMask = outLayers.ResolveMask(entry.affectsLayers);

                if (entry.instanceData.isObject())
                {
                    for (const auto& key : entry.instanceData.getMemberNames())
                    {
                        const std::string& keyStr = key;
                        if (keyStr.rfind("PointLight2D.", 0) == 0)
                        {
                            const std::string field = keyStr.substr(13);
                            const Json::Value& val  = entry.instanceData[key];
                            if (field == "position" && val.isArray() && val.size() >= 2)
                                light.position = Dia::Maths::Vector2D(val[0].asFloat(), val[1].asFloat());
                            else if (field == "radius" && val.isNumeric())
                                light.radius = val.asFloat();
                            else if (field == "intensity" && val.isNumeric())
                                light.intensity = val.asFloat();
                            else if (field == "colour" && val.isArray() && val.size() >= 4)
                            {
                                light.colour[0] = val[0].asFloat();
                                light.colour[1] = val[1].asFloat();
                                light.colour[2] = val[2].asFloat();
                                light.colour[3] = val[3].asFloat();
                            }
                            else if (field == "enabled" && val.isBool())
                                light.enabled = val.asBool();
                        }
                    }
                }

                context.lightRegistry.Register(entry.id, light);
                mRegisteredLights.Add(entry.id);
            }

            // --- Spawn entities ---
            for (unsigned int i = 0; i < scene.entities.Size(); ++i)
            {
                const EntityInstance& entry = scene.entities.At(i);
                if (!entry.enabled)
                    continue;

                const char* debugName = (entry.name.AsChar() && entry.name.AsChar()[0] != '\0')
                                       ? entry.name.AsChar() : nullptr;
                Dia::Entity::Entity entity = context.entityDomain.CreateEntity(debugName);

                if (!entity.IsValid())
                {
                    if (outErrors) outErrors->hasErrors = true;
                    continue;
                }

                ApplyInstanceData(context.entityDomain, entity, entry.instanceData, outErrors);
                mSpawnedEntities.Add(entity);
            }

            context.entityDomain.EndOfFrame();
            return true;
        }

        void SceneLoader2D::Unload(SceneLoadContext& context)
        {
            for (unsigned int i = 0; i < mRegisteredCameras.Size(); ++i)
                context.cameraRegistry.Unregister(mRegisteredCameras.At(i));
            mRegisteredCameras.RemoveAll();

            for (unsigned int i = 0; i < mRegisteredLights.Size(); ++i)
                context.lightRegistry.Unregister(mRegisteredLights.At(i));
            mRegisteredLights.RemoveAll();

            for (unsigned int i = 0; i < mSpawnedEntities.Size(); ++i)
                context.entityDomain.QueueDestroy(mSpawnedEntities.At(i));
            context.entityDomain.EndOfFrame();
            mSpawnedEntities.RemoveAll();
        }

        void SceneLoader2D::ApplyInstanceData(Dia::Entity::Domain&    domain,
                                              Dia::Entity::Entity      entity,
                                              const Json::Value&       instanceData,
                                              SceneLoadErrors*         outErrors)
        {
            if (!instanceData.isObject())
                return;

            // instanceData keys are "ComponentType.fieldName" — split and call Domain::WriteField
            for (const auto& key : instanceData.getMemberNames())
            {
                const char*  keyStr   = key.c_str();
                const char*  dotPos   = strchr(keyStr, '.');
                if (!dotPos)
                    continue;

                // Extract component type ID and field name
                char compTypeBuf[128] = {};
                size_t compLen = static_cast<size_t>(dotPos - keyStr);
                if (compLen >= sizeof(compTypeBuf))
                    continue;
                memcpy(compTypeBuf, keyStr, compLen);

                const char* fieldName = dotPos + 1;
                Dia::Core::StringCRC typeId(compTypeBuf);

                if (!domain.WriteField(entity, typeId, fieldName, instanceData[key]))
                {
                    if (outErrors) outErrors->hasErrors = true;
                }
            }
        }

    } // namespace Scene2D
} // namespace Dia
