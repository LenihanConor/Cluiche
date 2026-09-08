#include <DiaBehaviourTree/BehaviourTreeSystem.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>

#include <vector>
#include <chrono>

namespace Dia
{
    namespace BehaviourTree
    {

const Dia::Core::StringCRC BehaviourTreeSystem::kUniqueId{"behaviour-tree-system"};

struct BehaviourTreeSystem::Impl
{
    std::vector<BehaviourTreeComponent*> components;
    int   roundRobinIndex = 0;
    float lastDeltaTime   = 0.0f;
};

BehaviourTreeSystem::BehaviourTreeSystem()
    : mImpl(new Impl())
{
}

BehaviourTreeSystem::~BehaviourTreeSystem()
{
    delete mImpl;
    mImpl = nullptr;
}

void BehaviourTreeSystem::Register(BehaviourTreeComponent* component)
{
    mImpl->components.push_back(component);
}

void BehaviourTreeSystem::Unregister(BehaviourTreeComponent* component)
{
    auto& v = mImpl->components;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        if (*it == component)
        {
            const int removedIdx = static_cast<int>(it - v.begin());
            v.erase(it);
            // Keep round-robin index valid after removal
            if (!v.empty() && mImpl->roundRobinIndex > removedIdx)
                --mImpl->roundRobinIndex;
            mImpl->roundRobinIndex %= static_cast<int>(v.size() == 0 ? 1 : v.size());
            break;
        }
    }
}

int BehaviourTreeSystem::GetRegisteredCount() const
{
    return static_cast<int>(mImpl->components.size());
}

void BehaviourTreeSystem::Update(float budgetMs, float deltaTime)
{
    const int count = static_cast<int>(mImpl->components.size());
    if (count == 0 || budgetMs <= 0.0f)
        return;

    float elapsedMs = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        if (elapsedMs >= budgetMs)
            break;

        const int idx = mImpl->roundRobinIndex;
        mImpl->roundRobinIndex = (mImpl->roundRobinIndex + 1) % count;

        BehaviourTreeComponent* comp = mImpl->components[idx];
        if (!comp->HasAsset())
            continue;

        auto t0 = std::chrono::steady_clock::now();
        comp->Tick(deltaTime);
        auto t1 = std::chrono::steady_clock::now();
        elapsedMs += std::chrono::duration<float, std::milli>(t1 - t0).count();
    }
}

Dia::Core::StringCRC BehaviourTreeSystem::GetSystemId() const
{
    return kUniqueId;
}

void BehaviourTreeSystem::UpdateBudgeted(float budgetMs)
{
    Update(budgetMs, mImpl->lastDeltaTime);
}

void BehaviourTreeSystem::SetDeltaTime(float dt)
{
    mImpl->lastDeltaTime = dt;
}

    } // namespace BehaviourTree
} // namespace Dia
