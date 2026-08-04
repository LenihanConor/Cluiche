#ifdef DIA_DEBUG

#include "Modules/TestStages/Drawers/PathfindingTestDrawer.h"
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaFlowField/FlowCell.h>
#include <DiaPathfinding/CellCoord.h>
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace CluicheTest {

// ---------------------------------------------------------------------------
// Agent colors
// ---------------------------------------------------------------------------
static const Dia::Core::RGBA kAgentColors[PathfindingTestDrawer::kAgentCount] = {
    Dia::Core::RGBA(60,  140, 255, 255),   // Agent 0 — blue
    Dia::Core::RGBA(255, 140, 40,  255),   // Agent 1 — orange
    Dia::Core::RGBA(180, 80,  255, 255),   // Agent 2 — purple
};
static const Dia::Core::RGBA kAgentColorsBright[PathfindingTestDrawer::kAgentCount] = {
    Dia::Core::RGBA(120, 190, 255, 255),
    Dia::Core::RGBA(255, 200, 100, 255),
    Dia::Core::RGBA(220, 140, 255, 255),
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
PathfindingTestDrawer::PathfindingTestDrawer(
    const Dia::Pathfinding::SquarePathGrid&  grid,
    const Dia::FlowField::FlowField*&        currentField,
    const Dia::Steering::SteeringAgent*      agents,
    const Dia::Maths::Vector2D (*trails)[kTrailLen],
    const int*                               trailHeads,
    const bool*                              agentArrived,
    const Dia::Maths::Vector2D&              goalWorld,
    const bool&                              pathComputed,
    const bool&                              flowFieldReady,
    const bool&                              recomputedAfterBlock,
    const bool&                              firstArrived,
    const bool&                              allArrived,
    const int&                               recomputeCount,
    float                                    cellSize,
    const float&                             stageTime,
    const Dia::Debug::DebugLayerManager&     /*layerManager*/)
    : mGrid(grid)
    , mCurrentField(currentField)
    , mAgents(agents)
    , mTrails(trails)
    , mTrailHeads(trailHeads)
    , mAgentArrived(agentArrived)
    , mGoalWorld(goalWorld)
    , mPathComputed(pathComputed)
    , mFlowFieldReady(flowFieldReady)
    , mRecomputedAfterBlock(recomputedAfterBlock)
    , mFirstArrived(firstArrived)
    , mAllArrived(allArrived)
    , mRecomputeCount(recomputeCount)
    , mCellSize(cellSize)
    , mStageTime(stageTime)
{}

Dia::Core::StringCRC PathfindingTestDrawer::GetLayerName() const
{
    return Dia::Core::StringCRC("PathfindingTest");
}

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------
Dia::Maths::Vector2D PathfindingTestDrawer::CellCenter(int col, int row) const
{
    return Dia::Maths::Vector2D(
        col * mCellSize + mCellSize * 0.5f,
        row * mCellSize + mCellSize * 0.5f);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------
void PathfindingTestDrawer::Draw(Dia::Core::IDebugDraw& draw)
{
    const int W = mGrid.GetWidth();
    const int H = mGrid.GetHeight();

    // --- 1. Grid cells ---
    for (int col = 0; col < W; ++col)
    {
        for (int row = 0; row < H; ++row)
        {
            Dia::Maths::Vector2D tl(col * mCellSize,             row * mCellSize);
            Dia::Maths::Vector2D br(col * mCellSize + mCellSize, row * mCellSize + mCellSize);

            if (!mGrid.IsPassable({ col, row }))
            {
                // Dynamic obstacle (row 10, cols 6-9, added at frame 60)
                bool isDynamic = (mRecomputedAfterBlock && row == 10 && col >= 6 && col <= 9);
                Dia::Core::RGBA fillCol = isDynamic
                    ? Dia::Core::RGBA(80, 20, 20, 255)
                    : Dia::Core::RGBA(30, 30, 40, 255);
                draw.RequestDrawRect(tl, br, Dia::Core::RGBA(0, 0, 0, 0), fillCol);
            }
            else
            {
                draw.RequestDrawRect(tl, br, Dia::Core::RGBA(60, 60, 70, 80));
            }
        }
    }

    // --- 2. Flow field arrows ---
    if (mCurrentField)
    {
        for (int col = 0; col < W; ++col)
        {
            for (int row = 0; row < H; ++row)
            {
                if (!mGrid.IsPassable({ col, row })) continue;

                const auto& cell = mCurrentField->Sample({ col, row });
                if (!cell.reachable) continue;

                float dx = cell.direction.X();
                float dy = cell.direction.Y();
                float len = sqrtf(dx * dx + dy * dy);
                if (len < 0.0001f) continue; // goal cell — zero direction

                // Normalised distance from goal: approximate using Manhattan from goal (16,7)
                float normDist = static_cast<float>(
                    std::abs(col - 16) + std::abs(row - 7)) / static_cast<float>(W + H);
                if (normDist > 1.0f) normDist = 1.0f;

                // Green (near) → Red (far)
                Dia::Core::RGBA arrowCol(
                    static_cast<unsigned char>(normDist * 220),
                    static_cast<unsigned char>((1.0f - normDist) * 200),
                    40,
                    180);

                Dia::Maths::Vector2D center = CellCenter(col, row);
                float arrowLen = mCellSize * 0.38f;
                Dia::Maths::Vector2D dir(dx / len, dy / len);
                draw.RequestDrawRay(center, dir, arrowLen, arrowCol);
            }
        }
    }

    // --- 3. Start markers (X cross per agent) ---
    static const Dia::Pathfinding::CellCoord kStarts[kAgentCount] = {{1,1},{1,13},{18,13}};
    for (int i = 0; i < kAgentCount; ++i)
    {
        Dia::Maths::Vector2D c = CellCenter(kStarts[i].x, kStarts[i].y);
        float s = mCellSize * 0.25f;
        Dia::Core::RGBA xCol(120, 120, 120, 160);
        draw.RequestDraw(Dia::Maths::Vector2D(c.X()-s, c.Y()-s), Dia::Maths::Vector2D(c.X()+s, c.Y()+s), xCol);
        draw.RequestDraw(Dia::Maths::Vector2D(c.X()+s, c.Y()-s), Dia::Maths::Vector2D(c.X()-s, c.Y()+s), xCol);
    }

    // --- 4. Goal pulse ---
    float pulse = 8.0f + 6.0f * sinf(mStageTime * 4.0f);
    draw.RequestDraw(mGoalWorld, pulse, Dia::Core::RGBA(255, 255, 255, 220));

    // --- 5. Agent trails, circles, velocity arrows ---
    for (int i = 0; i < kAgentCount; ++i)
    {
        // Trail dots (fading)
        for (int t = 0; t < kTrailLen; ++t)
        {
            int idx = (mTrailHeads[i] - 1 - t + kTrailLen * 100) % kTrailLen;
            const Dia::Maths::Vector2D& tp = mTrails[i][idx];
            if (tp.X() == 0.0f && tp.Y() == 0.0f) continue;
            float alpha = 1.0f - static_cast<float>(t) / static_cast<float>(kTrailLen);
            Dia::Core::RGBA trailCol(
                kAgentColors[i].R(),
                kAgentColors[i].G(),
                kAgentColors[i].B(),
                static_cast<unsigned char>(alpha * 140.0f));
            draw.RequestDrawPoint(tp, trailCol);
        }

        // Agent circle
        draw.RequestDraw(mAgents[i].position, 10.0f, kAgentColors[i]);

        // Velocity arrow
        float vx = mAgents[i].velocity.X();
        float vy = mAgents[i].velocity.Y();
        float vlen = sqrtf(vx * vx + vy * vy);
        if (vlen > 0.5f)
        {
            Dia::Maths::Vector2D vdir(vx / vlen, vy / vlen);
            draw.RequestDrawRay(mAgents[i].position, vdir, 16.0f, kAgentColorsBright[i]);
        }
    }
}

// ---------------------------------------------------------------------------
// DrawImGui
// ---------------------------------------------------------------------------
void PathfindingTestDrawer::DrawImGui()
{
    // --- Nav Stack panel ---
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260, 180), ImGuiCond_Always);
    ImGui::Begin("Navigation Stack", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    if (ImGui::TreeNodeEx("Path", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("computed: %s", mPathComputed ? "YES" : "pending");
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Flow Field", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("ready: %s", mFlowFieldReady ? "YES" : "pending");
        ImGui::Text("recomputes: %d", mRecomputeCount);
        ImGui::Text("rerouted: %s", mRecomputedAfterBlock ? "YES" : "pending");
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Agents", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static const char* kAgentNames[kAgentCount] = {"Agent0 (blue)", "Agent1 (orange)", "Agent2 (purple)"};
        for (int i = 0; i < kAgentCount; ++i)
        {
            float dx = mAgents[i].position.X() - mGoalWorld.X();
            float dy = mAgents[i].position.Y() - mGoalWorld.Y();
            float dist = sqrtf(dx * dx + dy * dy);
            ImGui::Text("%s  dist:%.1f  %s",
                kAgentNames[i], dist, mAgentArrived[i] ? "ARRIVED" : "steering");
        }
        ImGui::TreePop();
    }
    ImGui::End();

    // --- Checkpoints panel ---
    ImGui::SetNextWindowPos(ImVec2(280, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(260, 160), ImGuiCond_Always);
    ImGui::Begin("Checkpoints", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    auto CheckRow = [](const char* name, bool passed) {
        ImGui::TextColored(
            passed ? ImVec4(0.2f,1.0f,0.2f,1.0f) : ImVec4(0.8f,0.8f,0.2f,1.0f),
            "%s  %s", passed ? "[PASS]" : "[ -- ]", name);
    };
    CheckRow("path_computed",          mPathComputed);
    CheckRow("flow_field_ready",       mFlowFieldReady);
    CheckRow("recomputed_after_block", mRecomputedAfterBlock);
    CheckRow("first_agent_arrived",    mFirstArrived);
    CheckRow("all_agents_arrived",     mAllArrived);
    ImGui::End();
}

} // namespace CluicheTest
#endif // DIA_DEBUG
