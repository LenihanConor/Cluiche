#include "DiaEditor/Plugin/LiveConnectionPluginBase.h"
#include <DiaEditor/Plugin/PluginServiceLocator.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia { namespace Editor {

LiveConnectionPluginBase::LiveConnectionPluginBase(const EditorPluginMetadata& meta, const char* connectionPrefix)
    : EditorPluginBase(meta)
    , mPrefix(connectionPrefix)
{}

void LiveConnectionPluginBase::OnPluginLoad()
{
    mGameConnection = (GetServices() != nullptr)
        ? GetServices()->GetService<GameConnectionManager>()
        : nullptr;

    if (mGameConnection == nullptr)
    {
        DIA_LOG_WARNING("LiveConnectionPluginBase", "GameConnectionManager service not found");
    }

    char handlerTopic[Dia::Core::StringCRC::kStringLength];
    snprintf(handlerTopic, sizeof(handlerTopic), "%s.get_connection_state", mPrefix);
    RegisterHandler(Dia::Core::StringCRC(handlerTopic),
        [this](const Json::Value& /*data*/) -> Json::Value
        {
            Json::Value result;
            result["connected"] = IsGameConnected();
            return result;
        });

    OnLivePluginLoad();

    if (mGameConnection != nullptr && mGameConnection->IsConnected())
    {
        mWasConnected = true;
        SubscribeAllTopics();

        Json::Value state;
        state["connected"] = true;
        char stateTopic[Dia::Core::StringCRC::kStringLength];
        snprintf(stateTopic, sizeof(stateTopic), "%s.connection_state", mPrefix);
        if (GetBridge() != nullptr)
        {
            GetBridge()->NotifyUIDataChanged(stateTopic, state);
        }
        OnGameConnected();
    }
}

void LiveConnectionPluginBase::OnUpdate(float deltaTime)
{
    if (mGameConnection != nullptr)
    {
        const bool nowConnected = mGameConnection->IsConnected();
        if (nowConnected != mWasConnected)
        {
            HandleConnectionStateChange(nowConnected);
            mWasConnected = nowConnected;
        }
    }

    OnLiveUpdate(deltaTime);
}

void LiveConnectionPluginBase::OnPluginUnload()
{
    if (mWasConnected)
    {
        UnsubscribeAllTopics();
    }

    OnLivePluginUnload();

    mGameConnection = nullptr;
    mWasConnected = false;
}

void LiveConnectionPluginBase::HandleConnectionStateChange(bool connected)
{
    char stateTopic[Dia::Core::StringCRC::kStringLength];
    snprintf(stateTopic, sizeof(stateTopic), "%s.connection_state", mPrefix);

    if (connected)
    {
        SubscribeAllTopics();

        Json::Value state;
        state["connected"] = true;
        if (GetBridge() != nullptr)
        {
            GetBridge()->NotifyUIDataChanged(stateTopic, state);
        }

        OnGameConnected();
    }
    else
    {
        UnsubscribeAllTopics();

        Json::Value state;
        state["connected"] = false;
        if (GetBridge() != nullptr)
        {
            GetBridge()->NotifyUIDataChanged(stateTopic, state);
        }

        OnGameDisconnected();
    }
}

void LiveConnectionPluginBase::SubscribeAllTopics()
{
    for (unsigned int i = 0; i < mRegisteredTopics.Size(); ++i)
    {
        mGameConnection->Subscribe(mRegisteredTopics[i].topic, mRegisteredTopics[i].callback);
    }
}

void LiveConnectionPluginBase::UnsubscribeAllTopics()
{
    for (unsigned int i = 0; i < mRegisteredTopics.Size(); ++i)
    {
        mGameConnection->Unsubscribe(mRegisteredTopics[i].topic);
    }
}

void LiveConnectionPluginBase::RegisterGameTopic(const Dia::Core::StringCRC& topic,
                                                  GameConnectionManager::DataCallback callback)
{
    TopicRegistration reg;
    reg.topic = topic;
    reg.callback = callback;
    mRegisteredTopics.Add(reg);

    if (mWasConnected && mGameConnection != nullptr)
    {
        mGameConnection->Subscribe(topic, callback);
    }
}

GameConnectionManager* LiveConnectionPluginBase::GetGameConnection() const
{
    return mGameConnection;
}

bool LiveConnectionPluginBase::IsGameConnected() const
{
    return mGameConnection != nullptr && mGameConnection->IsConnected();
}

}} // namespace Dia::Editor
