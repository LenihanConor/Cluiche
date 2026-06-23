#pragma once
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/DirectionalLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaLighting3D/AmbientLight3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class ILightBehaviour3D;
class LightPathBehaviour3D;

class LightRegistry3D
{
public:
    static constexpr unsigned int kMaxPointLights       = 16;
    static constexpr unsigned int kMaxDirectionalLights =  4;
    static constexpr unsigned int kMaxSpotLights        = 16;
    static constexpr unsigned int kMaxBehaviours        =  4;

    LightRegistry3D();

    bool RegisterPoint      (Dia::Core::StringCRC id, const PointLight3D& light);
    bool RegisterDirectional(Dia::Core::StringCRC id, const DirectionalLight3D& light);
    bool RegisterSpot       (Dia::Core::StringCRC id, const SpotLight3D& light);
    void SetAmbient         (const AmbientLight3D& light);

    void Unregister(Dia::Core::StringCRC id);
    bool Has       (Dia::Core::StringCRC id) const;

    PointLight3D&       GetPoint      (Dia::Core::StringCRC id);
    DirectionalLight3D& GetDirectional(Dia::Core::StringCRC id);
    SpotLight3D&        GetSpot       (Dia::Core::StringCRC id);
    AmbientLight3D&     GetAmbient    ();

    unsigned int              GetPointCount()                        const;
    const PointLight3D&       GetPointByIndex(unsigned int i)       const;
    unsigned int              GetDirectionalCount()                  const;
    const DirectionalLight3D& GetDirectionalByIndex(unsigned int i) const;
    unsigned int              GetSpotCount()                        const;
    const SpotLight3D&        GetSpotByIndex(unsigned int i)        const;

    bool AttachBehaviour(Dia::Core::StringCRC lightId, ILightBehaviour3D* behaviour);
    void DetachBehaviour(Dia::Core::StringCRC lightId, Dia::Core::StringCRC behaviourTypeId);

    LightPathBehaviour3D* GetPathBehaviour(Dia::Core::StringCRC lightId);

    void UpdateAll(float dt);

private:
    struct PointSlot
    {
        Dia::Core::StringCRC id;
        PointLight3D         light;
        ILightBehaviour3D*   behaviours[kMaxBehaviours] = {};
        unsigned int         behaviourCount = 0;
    };
    struct DirectionalSlot
    {
        Dia::Core::StringCRC id;
        DirectionalLight3D   light;
        ILightBehaviour3D*   behaviours[kMaxBehaviours] = {};
        unsigned int         behaviourCount = 0;
    };
    struct SpotSlot
    {
        Dia::Core::StringCRC id;
        SpotLight3D          light;
        ILightBehaviour3D*   behaviours[kMaxBehaviours] = {};
        unsigned int         behaviourCount = 0;
    };

    int FindPointIndex      (Dia::Core::StringCRC id) const;
    int FindDirectionalIndex(Dia::Core::StringCRC id) const;
    int FindSpotIndex       (Dia::Core::StringCRC id) const;

    template<typename TSlot, unsigned int N>
    void DetachBehaviourFromSlots(TSlot (&slots)[N], unsigned int count,
                                  Dia::Core::StringCRC lightId, Dia::Core::StringCRC typeId);

    PointSlot       mPointSlots      [kMaxPointLights];
    unsigned int    mPointCount       = 0;
    DirectionalSlot mDirectionalSlots[kMaxDirectionalLights];
    unsigned int    mDirectionalCount = 0;
    SpotSlot        mSpotSlots       [kMaxSpotLights];
    unsigned int    mSpotCount        = 0;
    AmbientLight3D  mAmbient;
};

} }
