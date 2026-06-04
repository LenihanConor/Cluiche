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
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Metric/MetricRegistry.h>

#include <cstdio>
#include <cstring>

namespace Dia
{
    namespace Scene2D
    {
        SceneLoader2D::SceneLoader2D()
        {
            auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
            mMetricLoadsTotal    = reg.RegisterCounter(Dia::Core::StringCRC("dia.scene2d.loads_total"));
            mMetricLoadFailures  = reg.RegisterCounter(Dia::Core::StringCRC("dia.scene2d.load_failures"));
            mMetricCamerasLoaded = reg.RegisterGauge  (Dia::Core::StringCRC("dia.scene2d.cameras_loaded"));
            mMetricLightsLoaded  = reg.RegisterGauge  (Dia::Core::StringCRC("dia.scene2d.lights_loaded"));
            mMetricEntitiesLoaded= reg.RegisterGauge  (Dia::Core::StringCRC("dia.scene2d.entities_loaded"));
        }

        bool SceneLoader2D::Load(const char*       filePath,
                                 SceneLoadContext& context,
                                 LayerTable&       outLayers,
                                 SceneLoadErrors*  outErrors)
        {
            DIA_TRACE_ZONE("scene2d.load", Dia::Observation::Trace::Category::kDiaScene);
            DIA_PROFILE_SCOPE("scene2d.load", Dia::Observation::Profile::Category::kDiaScene);

            if (mMetricLoadsTotal) mMetricLoadsTotal->Inc();

            // --- Parse JSON ---
            Json::Value  root;
            Json::Reader reader;

            FILE* f = nullptr;
            #ifdef _MSC_VER
            fopen_s(&f, filePath, "rb");
            #else
            f = fopen(filePath, "rb");
            #endif
            if (!f)
            {
                DIA_LOG_ERROR("DiaScene2D", "SceneLoader2D::Load — cannot open file: %s", filePath);
                if (outErrors) outErrors->hasErrors = true;
                if (mMetricLoadFailures) mMetricLoadFailures->Inc();
                SetFailing(Dia::Core::StringCRC("file_not_found"));
                IncrementErrors();
                return false;
            }
            fseek(f, 0, SEEK_END);
            long fileSize = ftell(f);
            fseek(f, 0, SEEK_SET);

            if (fileSize <= 0)
            {
                fclose(f);
                DIA_LOG_ERROR("DiaScene2D", "SceneLoader2D::Load — empty file: %s", filePath);
                if (outErrors) outErrors->hasErrors = true;
                if (mMetricLoadFailures) mMetricLoadFailures->Inc();
                SetFailing(Dia::Core::StringCRC("empty_file"));
                IncrementErrors();
                return false;
            }

            char* buf = new char[static_cast<size_t>(fileSize) + 1];
            fread(buf, 1, static_cast<size_t>(fileSize), f);
            buf[fileSize] = '\0';
            fclose(f);

            bool parsed = reader.parse(buf, root, false);
            delete[] buf;

            if (!parsed)
            {
                DIA_LOG_ERROR("DiaScene2D", "SceneLoader2D::Load — JSON parse error in: %s", filePath);
                if (outErrors) outErrors->hasErrors = true;
                if (mMetricLoadFailures) mMetricLoadFailures->Inc();
                SetFailing(Dia::Core::StringCRC("parse_error"));
                IncrementErrors();
                return false;
            }

            if (!root.isMember("scene2d"))
            {
                DIA_LOG_ERROR("DiaScene2D", "SceneLoader2D::Load — missing 'scene2d' key in: %s", filePath);
                if (outErrors) outErrors->hasErrors = true;
                if (mMetricLoadFailures) mMetricLoadFailures->Inc();
                SetFailing(Dia::Core::StringCRC("missing_scene2d_key"));
                IncrementErrors();
                return false;
            }

            // --- Deserialize Scene2D struct ---
            Scene2D scene;
            {
                Dia::Reflect::JsonReadArchive ar(root["scene2d"]);
                serialize(ar, scene, 1u);
                if (ar.GetResult().HasErrors())
                {
                    DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D::Load — partial deserialization errors in: %s", filePath);
                    if (outErrors) outErrors->hasErrors = true;
                    IncrementWarnings();
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
                DIA_LOG_ERROR("DiaScene2D",
                    "SceneLoader2D::Load — expected exactly 1 active camera, found %d in: %s",
                    activeCameraCount, filePath);
                if (outErrors) outErrors->hasErrors = true;
                if (mMetricLoadFailures) mMetricLoadFailures->Inc();
                SetFailing(Dia::Core::StringCRC("camera_validation_failed"));
                IncrementErrors();
                return false;
            }

            // --- Hydrate cameras ---
            for (unsigned int i = 0; i < scene.cameras.Size(); ++i)
            {
                const CameraEntry& entry = scene.cameras.At(i);

                Dia::Camera2D::Camera2D cam;

                if (entry.instanceData.isObject())
                {
                    for (const auto& key : entry.instanceData.getMemberNames())
                    {
                        const std::string& keyStr = key;
                        if (keyStr.rfind("Camera2D.", 0) == 0)
                        {
                            const std::string  field = keyStr.substr(9);
                            const Json::Value& val   = entry.instanceData[key];
                            if (field == "position" && val.isArray() && val.size() >= 2)
                                cam.SetPosition(Dia::Maths::Vector2D(val[0].asFloat(), val[1].asFloat()));
                            else if (field == "zoom" && val.isNumeric())
                                cam.SetZoom(val.asFloat());
                            else if (field == "rotation" && val.isNumeric())
                                cam.SetRotation(val.asFloat());
                            else
                                DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — unknown Camera2D instanceData field: %s", field.c_str());
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
                            const std::string  field = keyStr.substr(13);
                            const Json::Value& val   = entry.instanceData[key];
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
                            else
                                DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — unknown PointLight2D instanceData field: %s", field.c_str());
                        }
                    }
                }

                context.lightRegistry.Register(entry.id, light);
                mRegisteredLights.Add(entry.id);
            }

            // --- Spawn entities ---
            unsigned int spawnedCount = 0;
            for (unsigned int i = 0; i < scene.entities.Size(); ++i)
            {
                const EntityInstance& entry = scene.entities.At(i);
                if (!entry.enabled)
                {
                    DIA_LOG_DEBUG("DiaScene2D", "SceneLoader2D — skipping disabled entity: %s", entry.id.AsChar());
                    continue;
                }

                const char* debugName = (entry.name.AsChar() && entry.name.AsChar()[0] != '\0')
                                       ? entry.name.AsChar() : nullptr;
                Dia::Entity::Entity entity = context.entityDomain.CreateEntity(debugName);

                if (!entity.IsValid())
                {
                    DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — Domain full, could not spawn entity: %s", entry.id.AsChar());
                    if (outErrors) outErrors->hasErrors = true;
                    IncrementWarnings();
                    continue;
                }

                ApplyInstanceData(context.entityDomain, entity, entry.instanceData, outErrors);
                mSpawnedEntities.Add(entity);
                ++spawnedCount;
            }

            context.entityDomain.EndOfFrame();

            // --- Update metrics ---
            if (mMetricCamerasLoaded)  mMetricCamerasLoaded->Set(static_cast<double>(mRegisteredCameras.Size()));
            if (mMetricLightsLoaded)   mMetricLightsLoaded->Set(static_cast<double>(mRegisteredLights.Size()));
            if (mMetricEntitiesLoaded) mMetricEntitiesLoaded->Set(static_cast<double>(spawnedCount));

            SetOK();
            DIA_LOG_INFO("DiaScene2D",
                "SceneLoader2D::Load — loaded '%s': %u cameras, %u lights, %u entities, %u layers",
                filePath,
                mRegisteredCameras.Size(),
                mRegisteredLights.Size(),
                spawnedCount,
                outLayers.GetCount());

            return true;
        }

        void SceneLoader2D::Unload(SceneLoadContext& context)
        {
            DIA_TRACE_ZONE("scene2d.unload", Dia::Observation::Trace::Category::kDiaScene);

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

            if (mMetricCamerasLoaded)  mMetricCamerasLoaded->Set(0.0);
            if (mMetricLightsLoaded)   mMetricLightsLoaded->Set(0.0);
            if (mMetricEntitiesLoaded) mMetricEntitiesLoaded->Set(0.0);

            DIA_LOG_INFO("DiaScene2D", "SceneLoader2D::Unload — scene unloaded");
        }

        void SceneLoader2D::ApplyInstanceData(Dia::Entity::Domain&    domain,
                                              Dia::Entity::Entity      entity,
                                              const Json::Value&       instanceData,
                                              SceneLoadErrors*         outErrors)
        {
            if (!instanceData.isObject())
                return;

            for (const auto& key : instanceData.getMemberNames())
            {
                const char*  keyStr   = key.c_str();
                const char*  dotPos   = strchr(keyStr, '.');
                if (!dotPos)
                {
                    DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — instanceData key missing '.': %s", keyStr);
                    continue;
                }

                char compTypeBuf[128] = {};
                size_t compLen = static_cast<size_t>(dotPos - keyStr);
                if (compLen >= sizeof(compTypeBuf))
                {
                    DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — instanceData component type name too long: %s", keyStr);
                    continue;
                }
                memcpy(compTypeBuf, keyStr, compLen);

                const char* fieldName = dotPos + 1;
                Dia::Core::StringCRC typeId(compTypeBuf);

                if (!domain.WriteField(entity, typeId, fieldName, instanceData[key]))
                {
                    DIA_LOG_WARNING("DiaScene2D", "SceneLoader2D — WriteField failed for %s.%s (component may not be registered)", compTypeBuf, fieldName);
                    if (outErrors) outErrors->hasErrors = true;
                }
            }
        }

        Dia::Core::StringCRC SceneLoader2D::GetReporterName() const
        {
            return Dia::Core::StringCRC("dia.scene2d.loader");
        }

    } // namespace Scene2D
} // namespace Dia
