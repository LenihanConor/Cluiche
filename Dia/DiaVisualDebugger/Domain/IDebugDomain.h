////////////////////////////////////////////////////////////////////////////////
// Filename: IDebugDomain.h
// Description: Abstract interface for a visual debugger domain. All debug
//              subsystems (Physics, AI, Rendering, etc.) implement this to
//              register their drawers and expose panel data.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia
{
    namespace Debug
    {
        class DebugLayerManager;
    }
}

namespace Dia
{
    namespace VisualDebugger
    {
        ////////////////////////////////////////////////////////////////////////////////
        // IDebugDomain
        //
        // Abstract base for every visual debugger domain. Each subsystem
        // (Physics, Animation, AI, …) derives from this, bulk-registers its
        // world-space drawers via Register(), and exposes frame state to the
        // DiaDebugPanel via GetJSONState() / OnCommand().
        //
        // Panel-only domains (no world drawers) should override HasWorldDrawers()
        // to return false and leave Register/Unregister/GetDrawer as no-ops.
        ////////////////////////////////////////////////////////////////////////////////
        class IDebugDomain
        {
        public:
            virtual ~IDebugDomain() = default;

            // ----------------------------------------------------------------
            // Identity
            // ----------------------------------------------------------------

            /// Stable machine identifier (e.g. StringCRC("Physics")).
            virtual Dia::Core::StringCRC GetDomainId()    const = 0;

            /// Short human-readable label shown in the panel header.
            virtual const char*          GetDisplayName() const = 0;

            /// One-sentence description of the domain. Must be ≤80 characters.
            virtual const char*          GetDescription() const = 0;

            /// Group key used to cluster domains in the panel sidebar.
            /// Should be one of the canonical DebugGroupAccents keys
            /// (e.g. StringCRC("Physics"), StringCRC("AI")).
            virtual Dia::Core::StringCRC GetGroup()       const = 0;

            /// Accent colour for the panel card header tint.
            /// Implementations MUST return a DebugGroupAccents constant —
            /// never an inline literal.
            virtual Dia::Core::RGBA      GetAccentColour() const = 0;

            // ----------------------------------------------------------------
            // World-space drawer capability
            // ----------------------------------------------------------------

            /// Returns true when this domain contributes world-space drawers.
            /// Panel-only domains override to return false.
            virtual bool HasWorldDrawers() const { return true; }

            // ----------------------------------------------------------------
            // Lifecycle
            // ----------------------------------------------------------------

            /// Bulk-registers all world-space drawers with the layer manager.
            /// Default no-op for panel-only domains.
            virtual void Register(Dia::Debug::DebugLayerManager& mgr)   {}

            /// Bulk-unregisters all world-space drawers from the layer manager.
            /// Default no-op for panel-only domains.
            virtual void Unregister(Dia::Debug::DebugLayerManager& mgr) {}

            // ----------------------------------------------------------------
            // Data bridge to DiaDebugPanel
            // ----------------------------------------------------------------

            /// Called per-frame to write domain state for the panel.
            /// Must write at minimum:
            ///   { "drawers": [{"name": "Shapes", "enabled": true}, ...], "stats": {} }
            virtual void GetJSONState(Json::Value& out) = 0;

            /// Handles commands dispatched from the panel.
            /// Required handlers:
            ///   "toggle"   — args: { "drawer": <name> }
            ///   "setScale" — args: { "key": <name>, "value": <float> }
            virtual void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) = 0;

            // ----------------------------------------------------------------
            // World-space drawer access
            // ----------------------------------------------------------------

            /// Returns the number of world-space drawers owned by this domain.
            /// Only called when HasWorldDrawers() returns true.
            virtual int GetDrawerCount() const { return 0; }

            /// Returns the drawer at the given index, or nullptr if out of range.
            /// Only called when HasWorldDrawers() returns true.
            virtual Dia::Debug::IVisualDebugger* GetDrawer(int index) { return nullptr; }
        };

    } // namespace VisualDebugger
} // namespace Dia

#endif // DIA_DEBUG
