////////////////////////////////////////////////////////////////////////////////
// Filename: EntitySpatialVisualDebugger.h
// Description: IDebugDomain implementation for DiaEntitySpatial. World-space domain
//              with three drawers: Grid, Entities, Query.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <atomic>
#include <memory>

namespace Dia { namespace Entity { class Domain; } }
namespace Dia { namespace Debug  { class IDebugLayerRegistry; } }

namespace Dia
{
    namespace EntitySpatial
    {
        class EntitySpatialModule;

        namespace Adaptors
        {
            class EntitySpatialGridOverlay;
            class EntitySpatialEntityOverlay;
            class EntitySpatialQueryOverlay;
        }

        class EntitySpatialVisualDebugger : public Dia::VisualDebugger::IDebugDomain
        {
        public:
            EntitySpatialVisualDebugger(const EntitySpatialModule&           module,
                                         const Dia::Entity::Domain&           domain,
                                         const EntitySpatialIndex::SquareDef& def);
            ~EntitySpatialVisualDebugger() override;

            Dia::Core::StringCRC GetDomainId()     const override;
            const char*          GetDisplayName()  const override;
            const char*          GetDescription()  const override;
            Dia::Core::StringCRC GetGroup()        const override;
            Dia::Core::RGBA      GetAccentColour() const override;
            bool                 HasWorldDrawers() const override { return true; }

            void Register  (Dia::Debug::IDebugLayerRegistry& mgr) override;
            void Unregister(Dia::Debug::IDebugLayerRegistry& mgr) override;

            int                          GetDrawerCount() const override { return 3; }
            Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

            void GetJSONState(Json::Value& out) override;
            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

        private:
            const EntitySpatialModule&     mModule;
            const Dia::Entity::Domain&     mDomain;
            EntitySpatialIndex::SquareDef  mDef;
            Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

            float mLabelSize = 12.0f;

            std::atomic<bool> mGridEnabled{true};
            std::atomic<bool> mEntitiesEnabled{true};
            std::atomic<bool> mQueryEnabled{true};

            std::unique_ptr<Adaptors::EntitySpatialGridOverlay>   mGrid;
            std::unique_ptr<Adaptors::EntitySpatialEntityOverlay> mEntities;
            std::unique_ptr<Adaptors::EntitySpatialQueryOverlay>  mQuery;
        };

    } // namespace EntitySpatial
} // namespace Dia

#endif // DIA_DEBUG
