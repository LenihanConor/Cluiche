#pragma once
#include <DiaEditor/Plugin/EditorPluginBase.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace Editor {

class LiveConnectionPluginBase : public EditorPluginBase
{
public:
    explicit LiveConnectionPluginBase(const EditorPluginMetadata& meta, const char* connectionPrefix);

protected:
    virtual void OnLivePluginLoad() = 0;
    virtual void OnLivePluginUnload() = 0;

    virtual void OnGameConnected() = 0;
    virtual void OnGameDisconnected() = 0;

    virtual void OnLiveUpdate(float deltaTime) { (void)deltaTime; }

    // Declarative topic registration. Callable at any time (including OnLivePluginLoad).
    // Buffered when disconnected; subscribed immediately when connected.
    // All registered topics auto-unsubscribe on disconnect.
    void RegisterGameTopic(const Dia::Core::StringCRC& topic,
                           GameConnectionManager::DataCallback callback);

    GameConnectionManager* GetGameConnection() const;
    bool IsGameConnected() const;

private:
    void OnPluginLoad() override final;
    void OnPluginUnload() override final;
    void OnUpdate(float deltaTime) override final;

    void HandleConnectionStateChange(bool connected);
    void SubscribeAllTopics();
    void UnsubscribeAllTopics();

    GameConnectionManager* mGameConnection = nullptr;
    bool mWasConnected = false;
    const char* mPrefix;

    struct TopicRegistration {
        Dia::Core::StringCRC topic;
        GameConnectionManager::DataCallback callback;
    };
    static const unsigned int kMaxTopics = 16;
    Dia::Core::Containers::DynamicArrayC<TopicRegistration, kMaxTopics> mRegisteredTopics;
};

}} // namespace Dia::Editor
