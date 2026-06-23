////////////////////////////////////////////////////////////////////////////////
// Filename: LightPathBehaviour3D.h
////////////////////////////////////////////////////////////////////////////////
#ifndef DIA_LIGHTING3D_LIGHT_PATH_BEHAVIOUR3D_H
#define DIA_LIGHTING3D_LIGHT_PATH_BEHAVIOUR3D_H

#include <DiaLighting3D/Behaviours/ILightBehaviour3D.h>
#include <DiaLighting3D/PointLight3D.h>
#include <DiaLighting3D/SpotLight3D.h>
#include <DiaGeometry3D/Shapes/Spline3D.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Lighting3D {

class LightPathBehaviour3D : public ILightBehaviour3D
{
public:
    enum class LoopMode { Loop, PingPong, Once };

    struct Config
    {
        Dia::Geometry3D::Spline3D spline;
        float                     speed    = 0.5f;  // t units per second
        LoopMode                  loopMode = LoopMode::Loop;
    };

    // Constructors for supported light types only — no default constructor
    explicit LightPathBehaviour3D(PointLight3D* light, const Config& config);
    explicit LightPathBehaviour3D(SpotLight3D*  light, const Config& config);

    // ILightBehaviour3D
    Dia::Core::StringCRC GetTypeId() const override;
    void                 Update(float dt) override;

    float    GetT()        const;
    LoopMode GetLoopMode() const;

    static const Dia::Core::StringCRC kTypeId;

private:
    enum class LightType { Point, Spot };

    LightType                 mLightType;
    PointLight3D*             mPointLight  = nullptr;
    SpotLight3D*              mSpotLight   = nullptr;
    Dia::Geometry3D::Spline3D mSpline;
    float                     mSpeed;
    LoopMode                  mLoopMode;
    float                     mT         = 0.0f;
    float                     mDirection = 1.0f;  // 1.0 or -1.0 for PingPong
};

} }

#endif // DIA_LIGHTING3D_LIGHT_PATH_BEHAVIOUR3D_H
