#include "BehaviourTreeAsset.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Dia
{
    namespace BehaviourTree
    {
        // -----------------------------------------------------------------------
        // Internal Impl — confined entirely to this .cpp
        // -----------------------------------------------------------------------

        struct BehaviourTreeAsset::Impl
        {
            Dia::Core::StringCRC rootId;
            std::unordered_map<unsigned int, NodeDescriptor> nodes; // keyed by nodeId.Value()
        };

        // -----------------------------------------------------------------------
        // Constructor / Destructor / Move
        // -----------------------------------------------------------------------

        BehaviourTreeAsset::BehaviourTreeAsset()
            : mImpl(new Impl())
            , mValid(false)
        {
        }

        BehaviourTreeAsset::~BehaviourTreeAsset()
        {
            delete mImpl;
        }

        BehaviourTreeAsset::BehaviourTreeAsset(BehaviourTreeAsset&& other) noexcept
            : mImpl(other.mImpl)
            , mValid(other.mValid)
        {
            other.mImpl  = nullptr;
            other.mValid = false;
        }

        BehaviourTreeAsset& BehaviourTreeAsset::operator=(BehaviourTreeAsset&& other) noexcept
        {
            if (this != &other)
            {
                delete mImpl;
                mImpl        = other.mImpl;
                mValid       = other.mValid;
                other.mImpl  = nullptr;
                other.mValid = false;
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // LoadFromJson
        // -----------------------------------------------------------------------

        BehaviourTreeAsset BehaviourTreeAsset::LoadFromJson(
            const Json::Value& root,
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
        {
            BehaviourTreeAsset asset;

            // --- Validate "root" field ---
            if (!root.isMember("root") || !root["root"].isString())
            {
                if (!outErrors.IsFull())
                    outErrors.Add("BehaviourTreeAsset: missing or non-string 'root' field");
                return asset;
            }

            const std::string rootName = root["root"].asString();
            asset.mImpl->rootId = Dia::Core::StringCRC(rootName.c_str());

            // --- Validate "nodes" field ---
            const Json::Value& nodesObj = root["nodes"];
            if (!nodesObj.isObject())
            {
                if (!outErrors.IsFull())
                    outErrors.Add("BehaviourTreeAsset: missing or non-object 'nodes' field");
                return asset;
            }

            // --- Parse each node ---
            for (const std::string& nodeName : nodesObj.getMemberNames())
            {
                const Json::Value& nodeJson = nodesObj[nodeName];
                if (!nodeJson.isObject())
                    continue;

                const Dia::Core::StringCRC nodeId(nodeName.c_str());
                NodeDescriptor desc;
                desc.id = nodeId;

                // type
                if (nodeJson.isMember("type") && nodeJson["type"].isString())
                    desc.type = Dia::Core::StringCRC(nodeJson["type"].asString().c_str());

                const std::string typeStr = nodeJson.isMember("type") ? nodeJson["type"].asString() : "";

                if (typeStr == "sequence" || typeStr == "selector")
                {
                    // children array
                    if (nodeJson.isMember("children") && nodeJson["children"].isArray())
                    {
                        const Json::Value& children = nodeJson["children"];
                        for (Json::ArrayIndex i = 0; i < children.size(); ++i)
                        {
                            if (children[i].isString())
                                desc.children.push_back(Dia::Core::StringCRC(children[i].asString().c_str()));
                        }
                    }
                }
                else if (typeStr == "parallel")
                {
                    if (nodeJson.isMember("policy") && nodeJson["policy"].isString())
                        desc.policy = Dia::Core::StringCRC(nodeJson["policy"].asString().c_str());

                    if (nodeJson.isMember("children") && nodeJson["children"].isArray())
                    {
                        const Json::Value& children = nodeJson["children"];
                        for (Json::ArrayIndex i = 0; i < children.size(); ++i)
                        {
                            if (children[i].isString())
                                desc.children.push_back(Dia::Core::StringCRC(children[i].asString().c_str()));
                        }
                    }
                }
                else if (typeStr == "decorator")
                {
                    if (nodeJson.isMember("decorator") && nodeJson["decorator"].isString())
                        desc.decoratorType = Dia::Core::StringCRC(nodeJson["decorator"].asString().c_str());

                    if (nodeJson.isMember("child") && nodeJson["child"].isString())
                        desc.childId = Dia::Core::StringCRC(nodeJson["child"].asString().c_str());

                    if (nodeJson.isMember("repeat_count") && nodeJson["repeat_count"].isInt())
                        desc.repeatCount = nodeJson["repeat_count"].asInt();

                    if (nodeJson.isMember("break_on_failure") && nodeJson["break_on_failure"].isBool())
                        desc.breakOnFailure = nodeJson["break_on_failure"].asBool();

                    if (nodeJson.isMember("cooldown_seconds") && nodeJson["cooldown_seconds"].isNumeric())
                        desc.cooldownSeconds = nodeJson["cooldown_seconds"].asFloat();

                    if (nodeJson.isMember("blackboard_key") && nodeJson["blackboard_key"].isString())
                        desc.guardKey = Dia::Core::StringCRC(nodeJson["blackboard_key"].asString().c_str());
                }
                else if (typeStr == "action")
                {
                    if (nodeJson.isMember("action_id") && nodeJson["action_id"].isString())
                        desc.actionId = Dia::Core::StringCRC(nodeJson["action_id"].asString().c_str());

                    if (nodeJson.isMember("params") && nodeJson["params"].isArray())
                    {
                        const Json::Value& params = nodeJson["params"];
                        for (Json::ArrayIndex i = 0; i < params.size(); ++i)
                        {
                            if (params[i].isString())
                                desc.params.push_back(Dia::Core::StringCRC(params[i].asString().c_str()));
                        }
                    }
                }
                else if (typeStr == "condition")
                {
                    if (nodeJson.isMember("blackboard_key") && nodeJson["blackboard_key"].isString())
                        desc.blackboardKey = Dia::Core::StringCRC(nodeJson["blackboard_key"].asString().c_str());
                }

                asset.mImpl->nodes.emplace(nodeId.Value(), std::move(desc));
            }

            // --- Validate: root node must exist in nodes ---
            if (asset.mImpl->nodes.count(asset.mImpl->rootId.Value()) == 0)
            {
                if (!outErrors.IsFull())
                    outErrors.Add("BehaviourTreeAsset: root node not found in nodes map");
                return asset;
            }

            // --- DFS from root: validate child references and detect cycles ---
            // Only root-reachable nodes are checked; unreachable nodes are ignored.
            {
                std::unordered_set<unsigned int> grey;
                std::unordered_set<unsigned int> black;
                bool failed = false;

                std::function<void(Dia::Core::StringCRC)> dfs = [&](Dia::Core::StringCRC id)
                {
                    if (failed) return;
                    const unsigned int k = id.Value();
                    if (black.count(k)) return;
                    if (grey.count(k))
                    {
                        if (!outErrors.IsFull())
                            outErrors.Add("BehaviourTreeAsset: cycle detected in node graph");
                        failed = true;
                        return;
                    }
                    grey.insert(k);

                    auto it = asset.mImpl->nodes.find(k);
                    if (it != asset.mImpl->nodes.end())
                    {
                        const NodeDescriptor& desc = it->second;

                        // Check and recurse into multi-children (sequence/selector/parallel)
                        for (const Dia::Core::StringCRC& childId : desc.children)
                        {
                            if (asset.mImpl->nodes.count(childId.Value()) == 0)
                            {
                                if (!outErrors.IsFull())
                                    outErrors.Add("BehaviourTreeAsset: node child reference not found");
                                failed = true;
                                return;
                            }
                            dfs(childId);
                        }

                        // Check and recurse into decorator single child
                        if (desc.childId.Value() != 0)
                        {
                            if (asset.mImpl->nodes.count(desc.childId.Value()) == 0)
                            {
                                if (!outErrors.IsFull())
                                    outErrors.Add("BehaviourTreeAsset: decorator child reference not found");
                                failed = true;
                                return;
                            }
                            dfs(desc.childId);
                        }
                    }

                    grey.erase(k);
                    black.insert(k);
                };

                dfs(asset.mImpl->rootId);

                if (failed)
                    return asset;
            }

            asset.mValid = true;
            return asset;
        }

        // -----------------------------------------------------------------------
        // Validate
        // -----------------------------------------------------------------------

        bool BehaviourTreeAsset::Validate(
            Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors) const
        {
            if (!mImpl || !mValid)
                return false;

            bool valid = true;

            // Check all child references
            for (auto& [key, desc] : mImpl->nodes)
            {
                for (const Dia::Core::StringCRC& childId : desc.children)
                {
                    if (mImpl->nodes.count(childId.Value()) == 0)
                    {
                        if (!outErrors.IsFull())
                            outErrors.Add("BehaviourTreeAsset: unresolved child reference");
                        valid = false;
                    }
                }
            }

            // Cycle detection from root via DFS
            std::unordered_set<unsigned int> grey;
            std::unordered_set<unsigned int> black;

            std::function<bool(Dia::Core::StringCRC)> dfs = [&](Dia::Core::StringCRC id) -> bool
            {
                const unsigned int k = id.Value();
                if (black.count(k)) return false;
                if (grey.count(k))
                {
                    if (!outErrors.IsFull())
                        outErrors.Add("BehaviourTreeAsset: cycle detected in node graph");
                    return true;
                }
                grey.insert(k);

                auto it = mImpl->nodes.find(k);
                if (it != mImpl->nodes.end())
                {
                    const NodeDescriptor& desc = it->second;
                    for (const Dia::Core::StringCRC& childId : desc.children)
                        if (dfs(childId)) return true;
                    if (desc.childId.Value() != 0)
                        if (dfs(desc.childId)) return true;
                }

                grey.erase(k);
                black.insert(k);
                return false;
            };

            if (dfs(mImpl->rootId))
                valid = false;

            return valid;
        }

        // -----------------------------------------------------------------------
        // Accessors
        // -----------------------------------------------------------------------

        bool BehaviourTreeAsset::IsValid() const { return mValid; }

        Dia::Core::StringCRC BehaviourTreeAsset::GetRootNodeId() const
        {
            if (!mImpl) return Dia::Core::StringCRC();
            return mImpl->rootId;
        }

        int BehaviourTreeAsset::GetNodeCount() const
        {
            if (!mImpl) return 0;
            return static_cast<int>(mImpl->nodes.size());
        }

        const BehaviourTreeAsset::NodeDescriptor* BehaviourTreeAsset::GetNode(
            Dia::Core::StringCRC nodeId) const
        {
            if (!mImpl) return nullptr;
            auto it = mImpl->nodes.find(nodeId.Value());
            if (it == mImpl->nodes.end()) return nullptr;
            return &it->second;
        }

        bool BehaviourTreeAsset::HasNode(Dia::Core::StringCRC nodeId) const
        {
            if (!mImpl) return false;
            return mImpl->nodes.count(nodeId.Value()) != 0;
        }

    } // namespace BehaviourTree
} // namespace Dia
