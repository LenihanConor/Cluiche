# DiaSFML API

**Last Updated:** 2026-04-01

SFML backend implementation API providing graphics, window, input, and audio integration.

---

## Overview

**DiaSFML** provides SFML (Simple and Fast Multimedia Library) backend implementations for Dia subsystems.

**Location:** `Dia/DiaSFML/`

**Namespace:** `Dia::SFML::`

**Key Components:**
- **DiaSFMLRenderWindow** - Graphics and window backend
- **DiaSFMLInputSource** - Input backend
- **DiaSFMLSoundManager** - Audio backend

**SFML Version:** 2.5.1

---

## Graphics and Window

### DiaSFMLRenderWindow

**Header:** `Dia/DiaSFML/DiaSFMLRenderWindow.h`

**Purpose:** SFML implementation of ICanvas (graphics) and IWindow (window)

#### Key Methods

```cpp
class DiaSFMLRenderWindow : 
    public Dia::Graphics::ICanvas,
    public Dia::Window::IWindow
{
public:
    DiaSFMLRenderWindow();
    ~DiaSFMLRenderWindow();
    
    // IWindow implementation
    void Create(unsigned int width, unsigned int height, const char* title) override;
    void Close() override;
    bool IsOpen() const override;
    
    void Display() override;
    void Clear() override;
    
    unsigned int GetWidth() const override;
    unsigned int GetHeight() const override;
    void SetTitle(const char* title) override;
    
    void SetFullscreen(bool fullscreen) override;
    bool IsFullscreen() const override;
    
    bool HasFocus() const override;
    void RequestFocus() override;
    
    void SetVSync(bool enabled) override;
    void SetFramerateLimit(unsigned int limit) override;
    
    // ICanvas implementation
    void DrawLine(
        const Dia::Maths::Vector2D& start,
        const Dia::Maths::Vector2D& end,
        const Dia::Graphics::Color& color) override;
    
    void DrawCircle(
        const Dia::Maths::Vector2D& center,
        float radius,
        const Dia::Graphics::Color& color) override;
    
    void DrawRectangle(
        const Dia::Maths::Vector2D& position,
        const Dia::Maths::Vector2D& size,
        const Dia::Graphics::Color& color) override;
    
    void DrawSprite(
        const Texture& texture,
        const Dia::Maths::Vector2D& position,
        const Dia::Maths::Vector2D& size,
        const Dia::Graphics::Color& tint) override;
    
    void DrawText(
        const char* text,
        const Dia::Maths::Vector2D& position,
        const Dia::Graphics::Color& color,
        unsigned int fontSize) override;
    
    // SFML-specific
    sf::RenderWindow& GetSFMLWindow();
    const sf::RenderWindow& GetSFMLWindow() const;
    
private:
    sf::RenderWindow mWindow;
    bool mIsFullscreen;
    sf::Font mDefaultFont;
    
    // Conversion helpers
    sf::Color ToSFMLColor(const Dia::Graphics::Color& color);
    sf::Vector2f ToSFMLVector(const Dia::Maths::Vector2D& vec);
};
```

#### Usage Example

```cpp
using namespace Dia::SFML;

// Create window
DiaSFMLRenderWindow* window = new DiaSFMLRenderWindow();
window->Create(1920, 1080, "Game");

// Configure
window->SetVSync(true);
window->SetFullscreen(false);

// Main loop
while (window->IsOpen())
{
    // Clear
    window->Clear(Dia::Graphics::Color::Black);
    
    // Draw (via ICanvas)
    window->DrawCircle(
        Dia::Maths::Vector2D(100.0f, 100.0f),
        50.0f,
        Dia::Graphics::Color::Red);
    
    // Present
    window->Display();
}

// Cleanup
window->Close();
delete window;
```

---

### Conversion Functions

**Purpose:** Convert between Dia and SFML types

```cpp
// Dia::Graphics::Color → sf::Color
sf::Color ToSFMLColor(const Dia::Graphics::Color& color)
{
    return sf::Color(color.r, color.g, color.b, color.a);
}

// Dia::Maths::Vector2D → sf::Vector2f
sf::Vector2f ToSFMLVector(const Dia::Maths::Vector2D& vec)
{
    return sf::Vector2f(vec.x, vec.y);
}

// sf::Vector2i → Dia::Maths::Vector2D
Dia::Maths::Vector2D FromSFMLVector(const sf::Vector2i& vec)
{
    return Dia::Maths::Vector2D(static_cast<float>(vec.x), static_cast<float>(vec.y));
}
```

---

## Input

### DiaSFMLInputSource

**Header:** `Dia/DiaSFML/DiaSFMLInputSource.h`

**Purpose:** SFML implementation of IInputSource (input events)

#### Key Methods

```cpp
class DiaSFMLInputSource : public Dia::Input::IInputSource
{
public:
    DiaSFMLInputSource(sf::Window* window);
    
    // IInputSource implementation
    bool PollEvent(Dia::Input::InputEvent& outEvent) override;
    
private:
    sf::Window* mWindow;
    
    // Conversion helpers
    Dia::Input::KeyCode ConvertKey(sf::Keyboard::Key key);
    Dia::Input::MouseButton ConvertMouseButton(sf::Mouse::Button button);
    Dia::Input::InputEventType ConvertEventType(sf::Event::EventType type);
};
```

#### Usage Example

```cpp
using namespace Dia::SFML;

// Create input source (needs SFML window)
sf::RenderWindow sfmlWindow;
DiaSFMLInputSource* inputSource = new DiaSFMLInputSource(&sfmlWindow);

// Create input manager
Dia::Input::InputSourceManager inputManager;
inputManager.AddSource(inputSource);

// Poll events
Dia::Input::InputEvent event;
while (inputManager.PollEvent(event))
{
    ProcessInput(event);
}
```

---

### Key Conversion

**Purpose:** Convert SFML keyboard keys to Dia KeyCode

```cpp
Dia::Input::KeyCode ConvertKey(sf::Keyboard::Key key)
{
    using namespace Dia::Input;
    
    switch (key)
    {
        case sf::Keyboard::A: return KeyCode::A;
        case sf::Keyboard::B: return KeyCode::B;
        // ... (full mapping)
        case sf::Keyboard::Space: return KeyCode::Space;
        case sf::Keyboard::Enter: return KeyCode::Enter;
        case sf::Keyboard::Escape: return KeyCode::Escape;
        default: return KeyCode::Unknown;
    }
}
```

---

### Mouse Button Conversion

**Purpose:** Convert SFML mouse buttons to Dia MouseButton

```cpp
Dia::Input::MouseButton ConvertMouseButton(sf::Mouse::Button button)
{
    using namespace Dia::Input;
    
    switch (button)
    {
        case sf::Mouse::Left: return MouseButton::Left;
        case sf::Mouse::Right: return MouseButton::Right;
        case sf::Mouse::Middle: return MouseButton::Middle;
        case sf::Mouse::XButton1: return MouseButton::XButton1;
        case sf::Mouse::XButton2: return MouseButton::XButton2;
        default: return MouseButton::Left;  // Fallback
    }
}
```

---

## Audio

### DiaSFMLSoundManager

**Header:** `Dia/DiaSFML/DiaSFMLSoundManager.h`

**Purpose:** SFML audio backend for sound playback

#### Key Methods

```cpp
class DiaSFMLSoundManager
{
public:
    DiaSFMLSoundManager();
    ~DiaSFMLSoundManager();
    
    // Sound loading
    bool LoadSound(const char* name, const char* filePath);
    void UnloadSound(const char* name);
    
    // Sound playback
    void PlaySound(const char* name);
    void StopSound(const char* name);
    void PauseSound(const char* name);
    
    // Music playback
    bool LoadMusic(const char* filePath);
    void PlayMusic(bool loop = true);
    void StopMusic();
    void PauseMusic();
    
    // Volume control
    void SetSoundVolume(float volume);  // 0.0 - 1.0
    void SetMusicVolume(float volume);  // 0.0 - 1.0
    
    // Master volume
    void SetMasterVolume(float volume);
    
private:
    std::map<std::string, sf::SoundBuffer> mSoundBuffers;
    std::map<std::string, sf::Sound> mSounds;
    sf::Music mMusic;
    
    float mSoundVolume;
    float mMusicVolume;
    float mMasterVolume;
};
```

#### Usage Example

```cpp
using namespace Dia::SFML;

// Create sound manager
DiaSFMLSoundManager soundManager;

// Load sounds
soundManager.LoadSound("jump", "Audio/jump.wav");
soundManager.LoadSound("shoot", "Audio/shoot.wav");

// Play sounds
soundManager.PlaySound("jump");
soundManager.PlaySound("shoot");

// Load and play music
soundManager.LoadMusic("Audio/music.ogg");
soundManager.PlayMusic(true);  // Loop

// Volume control
soundManager.SetSoundVolume(0.8f);
soundManager.SetMusicVolume(0.5f);
soundManager.SetMasterVolume(1.0f);

// Stop music
soundManager.StopMusic();
```

---

## Common Patterns

### Window Creation with Input

```cpp
class SFMLBackend
{
public:
    void Initialize()
    {
        // Create window
        mWindow = new DiaSFMLRenderWindow();
        mWindow->Create(1920, 1080, "Game");
        mWindow->SetVSync(true);
        
        // Create input source
        mInputSource = new DiaSFMLInputSource(&mWindow->GetSFMLWindow());
        
        // Register with input manager
        mInputManager.AddSource(mInputSource);
    }
    
    void Update()
    {
        // Poll input
        Dia::Input::InputEvent event;
        while (mInputManager.PollEvent(event))
        {
            ProcessInput(event);
        }
        
        // Render
        mWindow->Clear();
        DrawFrame();
        mWindow->Display();
    }
    
    void Shutdown()
    {
        mInputManager.RemoveSource(mInputSource);
        delete mInputSource;
        
        mWindow->Close();
        delete mWindow;
    }
    
private:
    DiaSFMLRenderWindow* mWindow;
    DiaSFMLInputSource* mInputSource;
    Dia::Input::InputSourceManager mInputManager;
};
```

---

### Audio System

```cpp
class AudioModule : public Dia::Application::Module
{
public:
    void OnConstruct() override
    {
        mSoundManager = new DiaSFMLSoundManager();
    }
    
    void OnStart() override
    {
        // Load audio assets
        mSoundManager->LoadSound("jump", "Audio/jump.wav");
        mSoundManager->LoadSound("hit", "Audio/hit.wav");
        mSoundManager->LoadMusic("Audio/background.ogg");
        
        // Set volumes
        mSoundManager->SetSoundVolume(0.7f);
        mSoundManager->SetMusicVolume(0.5f);
        
        // Start music
        mSoundManager->PlayMusic(true);
    }
    
    void OnUpdate() override
    {
        // Process audio events
        ProcessAudioEvents();
    }
    
    void OnStop() override
    {
        mSoundManager->StopMusic();
    }
    
    void OnDestruct() override
    {
        delete mSoundManager;
    }
    
    void PlaySound(const char* name)
    {
        mSoundManager->PlaySound(name);
    }
    
private:
    DiaSFMLSoundManager* mSoundManager;
};
```

---

### Texture Loading

```cpp
class TextureManager
{
public:
    sf::Texture* LoadTexture(const char* filePath)
    {
        // Check if already loaded
        auto it = mTextures.find(filePath);
        if (it != mTextures.end())
        {
            return it->second;
        }
        
        // Load new texture
        sf::Texture* texture = new sf::Texture();
        if (texture->loadFromFile(filePath))
        {
            mTextures[filePath] = texture;
            return texture;
        }
        
        // Failed to load
        delete texture;
        return nullptr;
    }
    
    void UnloadAll()
    {
        for (auto& pair : mTextures)
        {
            delete pair.second;
        }
        mTextures.clear();
    }
    
private:
    std::map<std::string, sf::Texture*> mTextures;
};
```

---

## Dependencies

**Required:**
- SFML 2.5.1 (graphics, window, audio, system)
- `Dia/DiaGraphics/` - ICanvas interface
- `Dia/DiaWindow/` - IWindow interface
- `Dia/DiaInput/` - IInputSource interface
- `Dia/DiaMaths/` - Vector2D

**SFML Modules:**
- `sfml-graphics` - Rendering
- `sfml-window` - Window management
- `sfml-audio` - Sound and music
- `sfml-system` - Base utilities

---

## Thread Safety

| Component | Thread Safety |
|-----------|---------------|
| `DiaSFMLRenderWindow` | ❌ Call from Main/Render thread only |
| `DiaSFMLInputSource` | ❌ Call from Main thread only |
| `DiaSFMLSoundManager` | ❌ Not thread-safe |

**Important:** All SFML operations must occur on the **same thread** (typically Main or Render thread).

---

## Best Practices

### 1. Initialize SFML on Main Thread

```cpp
// ✅ Good: Create on Main thread
DiaSFMLRenderWindow* window = new DiaSFMLRenderWindow();
window->Create(1920, 1080, "Game");

// ❌ Bad: Create on worker thread (OpenGL context issues)
std::thread worker([&]() {
    DiaSFMLRenderWindow* window = new DiaSFMLRenderWindow();
    window->Create(1920, 1080, "Game");  // ERROR!
});
```

---

### 2. Load Assets Before Use

```cpp
// ✅ Good: Load before playing
soundManager->LoadSound("jump", "jump.wav");
soundManager->PlaySound("jump");

// ❌ Bad: Play without loading
soundManager->PlaySound("jump");  // Nothing happens
```

---

### 3. Check Window Validity

```cpp
// ✅ Good: Check IsOpen
if (window->IsOpen())
{
    window->Display();
}

// ❌ Bad: Assume always open
window->Display();  // Crash if closed
```

---

### 4. Cache SFML Resources

```cpp
// ✅ Good: Cache textures
class TextureCache
{
private:
    std::map<std::string, sf::Texture> mTextures;
};

// ❌ Bad: Load repeatedly
for (int i = 0; i < 100; ++i)
{
    sf::Texture texture;
    texture.loadFromFile("sprite.png");  // Loads 100 times!
}
```

---

## Gotchas

### Gotcha 1: SFML Window Must Stay Alive

`DiaSFMLInputSource` holds a pointer to `sf::Window`. The window must outlive the input source:

```cpp
// ✅ Good: Window outlives input source
DiaSFMLRenderWindow window;
DiaSFMLInputSource inputSource(&window.GetSFMLWindow());

// ❌ Bad: Window destroyed before input source
{
    DiaSFMLRenderWindow window;
    inputSource = new DiaSFMLInputSource(&window.GetSFMLWindow());
}  // window destroyed, inputSource has dangling pointer!
```

---

### Gotcha 2: VSync Blocks Display()

`Display()` with VSync enabled will **block** until vertical refresh:

```cpp
// This may take 16ms (60 Hz) or 8ms (120 Hz)
window->SetVSync(true);
window->Display();  // Blocks
```

---

### Gotcha 3: Sound vs Music

SFML differentiates between **sounds** (short, loaded into memory) and **music** (streamed from disk):

```cpp
// ✅ Sounds: Short effects (< 1 sec)
soundManager->LoadSound("jump", "jump.wav");  // Loaded into memory

// ✅ Music: Long tracks (> 10 sec)
soundManager->LoadMusic("music.ogg");  // Streamed from file
```

---

### Gotcha 4: Coordinate System

SFML uses **top-left origin** with Y **increasing downward**:

```cpp
// Top-left corner
sf::Vector2f topLeft(0.0f, 0.0f);

// Bottom-right corner
sf::Vector2f bottomRight(width, height);

// Y increases downward
```

This matches Dia's coordinate system.

---

## Limitations

### Current Limitations

1. **Single window** - SFML supports only one window per context
2. **Limited 3D** - SFML is primarily 2D (no 3D transforms)
3. **No shader pipeline** - Limited shader support
4. **Audio format support** - Depends on SFML's built-in codecs (WAV, OGG, FLAC)
5. **Font rendering** - Basic text rendering only

### SFML Alternative: SDL2

For projects needing more control or 3D, consider **SDL2** as an alternative backend:
- More low-level control
- Better 3D support (via OpenGL)
- More platform support
- Larger community

**[→ Future Directions](../../design-rationale/future-directions.md)**

---

## Examples

### Complete SFML Backend Module

```cpp
class SFMLModule : public Dia::Application::Module
{
public:
    void OnConstruct() override
    {
        mWindow = new DiaSFMLRenderWindow();
        mSoundManager = new DiaSFMLSoundManager();
    }
    
    void OnStart() override
    {
        // Initialize window
        mWindow->Create(1920, 1080, "Cluiche");
        mWindow->SetVSync(true);
        
        // Initialize input
        mInputSource = new DiaSFMLInputSource(&mWindow->GetSFMLWindow());
        mInputManager.AddSource(mInputSource);
        
        // Load audio
        mSoundManager->LoadSound("jump", "Audio/jump.wav");
        mSoundManager->LoadMusic("Audio/music.ogg");
        mSoundManager->PlayMusic(true);
    }
    
    void OnUpdate() override
    {
        // Check window
        if (!mWindow->IsOpen())
        {
            QueuePhaseTransition(Dia::Application::Phase::kShutdown);
            return;
        }
        
        // Poll input
        Dia::Input::InputEvent event;
        while (mInputManager.PollEvent(event))
        {
            ProcessInput(event);
        }
        
        // Render
        mWindow->Clear(Dia::Graphics::Color::Black);
        DrawFrame();
        mWindow->Display();
    }
    
    void OnStop() override
    {
        mSoundManager->StopMusic();
        
        mInputManager.RemoveSource(mInputSource);
        delete mInputSource;
        
        mWindow->Close();
    }
    
    void OnDestruct() override
    {
        delete mSoundManager;
        delete mWindow;
    }
    
    DiaSFMLRenderWindow* GetWindow() const { return mWindow; }
    DiaSFMLSoundManager* GetSoundManager() const { return mSoundManager; }
    
private:
    DiaSFMLRenderWindow* mWindow;
    DiaSFMLInputSource* mInputSource;
    DiaSFMLSoundManager* mSoundManager;
    Dia::Input::InputSourceManager mInputManager;
};
```

---

## Summary

**Core Classes:**
- `DiaSFMLRenderWindow` - Graphics + window backend (ICanvas + IWindow)
- `DiaSFMLInputSource` - Input backend (IInputSource)
- `DiaSFMLSoundManager` - Audio backend

**Graphics:**
- DrawLine/Circle/Rectangle/Sprite/Text
- Clear/Display for frame control
- SFML sf::RenderWindow wrapper

**Input:**
- Converts sf::Event → Dia::Input::InputEvent
- Keyboard, mouse, gamepad support
- Key and mouse button conversion

**Audio:**
- Sound effects (loaded into memory)
- Music (streamed from disk)
- Volume control (sound, music, master)

**Conversion:**
- Dia::Graphics::Color ↔ sf::Color
- Dia::Maths::Vector2D ↔ sf::Vector2f
- Dia::Input::KeyCode ↔ sf::Keyboard::Key

**Thread Safety:**
- ❌ All SFML operations Main/Render thread only

**Best Practices:**
- Initialize on Main thread
- Load assets before use
- Check window validity
- Cache SFML resources

**Limitations:**
- Single window
- 2D focused
- Limited shader support

**[→ API Overview](../api-overview.md)**  
**[→ DiaGraphics API](graphics-api.md)**  
**[→ DiaWindow API](window-api.md)**  
**[→ DiaInput API](input-api.md)**  
**[→ External Links](../../registry/external-links.md)**
