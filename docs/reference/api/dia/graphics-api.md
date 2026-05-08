# DiaGraphics API

**Last Updated:** 2026-04-01

Graphics abstraction API providing platform-agnostic rendering interface.

---

## Overview

**DiaGraphics** provides a platform-agnostic rendering interface for 2D graphics.

**Location:** `Dia/DiaGraphics/`

**Namespace:** `Dia::Graphics::`

**Key Concepts:**
- **ICanvas** - Abstract rendering interface
- **Frame** - Frame data (color, transform)
- **Platform-agnostic** - Backend implementation in DiaSFML

**Implementation:** `Dia::SFML::DiaSFMLRenderWindow` (SFML backend)

---

## Core Classes

### ICanvas

**Header:** `Dia/DiaGraphics/Interface/ICanvas.h`

**Purpose:** Abstract rendering interface implemented by backend

#### Key Methods

```cpp
class ICanvas
{
public:
    virtual ~ICanvas() = default;
    
    // Primitives
    virtual void DrawLine(
        const Dia::Maths::Vector2D& start,
        const Dia::Maths::Vector2D& end,
        const Color& color) = 0;
    
    virtual void DrawCircle(
        const Dia::Maths::Vector2D& center,
        float radius,
        const Color& color) = 0;
    
    virtual void DrawRectangle(
        const Dia::Maths::Vector2D& position,
        const Dia::Maths::Vector2D& size,
        const Color& color) = 0;
    
    virtual void DrawSprite(
        const Texture& texture,
        const Dia::Maths::Vector2D& position,
        const Dia::Maths::Vector2D& size,
        const Color& tint = Color::White) = 0;
    
    virtual void DrawText(
        const char* text,
        const Dia::Maths::Vector2D& position,
        const Color& color,
        unsigned int fontSize = 12) = 0;
    
    // Frame control
    virtual void Clear(const Color& color = Color::Black) = 0;
    virtual void Present() = 0;
    
    // Query
    virtual Dia::Maths::Vector2D GetSize() const = 0;
};
```

#### Usage Example

```cpp
// Get canvas (usually from RenderProcessingUnit)
Dia::Graphics::ICanvas* canvas = GetCanvas();

// Clear background
canvas->Clear(Dia::Graphics::Color::Black);

// Draw line
Dia::Maths::Vector2D start(100.0f, 100.0f);
Dia::Maths::Vector2D end(200.0f, 200.0f);
canvas->DrawLine(start, end, Dia::Graphics::Color::Red);

// Draw circle
Dia::Maths::Vector2D center(300.0f, 300.0f);
canvas->DrawCircle(center, 50.0f, Dia::Graphics::Color::Green);

// Draw rectangle
Dia::Maths::Vector2D pos(400.0f, 400.0f);
Dia::Maths::Vector2D size(100.0f, 50.0f);
canvas->DrawRectangle(pos, size, Dia::Graphics::Color::Blue);

// Draw text
canvas->DrawText("Hello World", Dia::Maths::Vector2D(10.0f, 10.0f),
    Dia::Graphics::Color::White, 16);

// Present frame
canvas->Present();
```

---

### Color

**Header:** `Dia/DiaGraphics/Interface/ICanvas.h`

**Purpose:** RGBA color representation

#### Definition

```cpp
struct Color
{
    unsigned char r, g, b, a;
    
    Color();
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
    
    // Predefined colors
    static const Color Black;
    static const Color White;
    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Yellow;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Transparent;
};
```

#### Usage Example

```cpp
using namespace Dia::Graphics;

// Create colors
Color red(255, 0, 0, 255);
Color semiTransparent(255, 255, 255, 128);

// Use predefined colors
Color white = Color::White;
Color black = Color::Black;

// Alpha blending
Color fadeOut(255, 0, 0, 128);  // 50% transparent red
```

---

### Frame

**Header:** `Dia/DiaGraphics/Interface/Frame.h`

**Purpose:** Frame data structure (color, transform)

#### Definition

```cpp
struct Frame
{
    Color backgroundColor;
    Dia::Maths::Matrix33 viewTransform;
    
    Frame();
    Frame(const Color& bgColor);
};
```

#### Usage Example

```cpp
Dia::Graphics::Frame frame;
frame.backgroundColor = Dia::Graphics::Color::Black;
frame.viewTransform = Dia::Maths::Matrix33::Identity();
```

---

## Drawing Primitives

### Lines

```cpp
// Single line
canvas->DrawLine(
    Dia::Maths::Vector2D(0.0f, 0.0f),
    Dia::Maths::Vector2D(100.0f, 100.0f),
    Dia::Graphics::Color::Red);

// Connected lines (polyline)
std::vector<Dia::Maths::Vector2D> points = {
    Dia::Maths::Vector2D(0.0f, 0.0f),
    Dia::Maths::Vector2D(50.0f, 100.0f),
    Dia::Maths::Vector2D(100.0f, 0.0f)
};

for (size_t i = 0; i < points.size() - 1; ++i)
{
    canvas->DrawLine(points[i], points[i + 1], Dia::Graphics::Color::Blue);
}
```

---

### Circles

```cpp
// Filled circle
canvas->DrawCircle(
    Dia::Maths::Vector2D(200.0f, 200.0f),
    30.0f,
    Dia::Graphics::Color::Green);

// Ring (draw two circles)
canvas->DrawCircle(center, outerRadius, Color::White);
canvas->DrawCircle(center, innerRadius, Color::Black);
```

---

### Rectangles

```cpp
// Rectangle
canvas->DrawRectangle(
    Dia::Maths::Vector2D(100.0f, 100.0f),  // Position
    Dia::Maths::Vector2D(50.0f, 30.0f),    // Size
    Dia::Graphics::Color::Blue);

// Outline (draw 4 lines)
Dia::Maths::Vector2D pos(100.0f, 100.0f);
Dia::Maths::Vector2D size(50.0f, 30.0f);
Color color = Color::Red;

canvas->DrawLine(pos, pos + Dia::Maths::Vector2D(size.x, 0.0f), color);
canvas->DrawLine(pos + Dia::Maths::Vector2D(size.x, 0.0f), pos + size, color);
canvas->DrawLine(pos + size, pos + Dia::Maths::Vector2D(0.0f, size.y), color);
canvas->DrawLine(pos + Dia::Maths::Vector2D(0.0f, size.y), pos, color);
```

---

### Sprites

```cpp
// Load texture (backend-specific)
Texture texture = LoadTexture("sprite.png");

// Draw sprite
canvas->DrawSprite(
    texture,
    Dia::Maths::Vector2D(100.0f, 100.0f),  // Position
    Dia::Maths::Vector2D(64.0f, 64.0f),    // Size
    Dia::Graphics::Color::White);           // Tint

// Tinted sprite (colored)
canvas->DrawSprite(
    texture,
    position,
    size,
    Dia::Graphics::Color(255, 0, 0, 255));  // Red tint
```

---

### Text

```cpp
// Simple text
canvas->DrawText(
    "Score: 100",
    Dia::Maths::Vector2D(10.0f, 10.0f),
    Dia::Graphics::Color::White,
    16);  // Font size

// Formatted text
char buffer[64];
sprintf(buffer, "FPS: %.1f", fps);
canvas->DrawText(buffer, position, Color::Yellow, 14);
```

---

## Common Patterns

### Clear and Present

```cpp
void RenderFrame()
{
    // Clear background
    canvas->Clear(Dia::Graphics::Color(20, 20, 20, 255));
    
    // Draw everything
    DrawGame();
    DrawUI();
    DrawDebug();
    
    // Present to screen
    canvas->Present();
}
```

---

### Debug Rendering

```cpp
void DrawDebugCircle(const Dia::Maths::Vector2D& center, float radius)
{
    // Draw circle outline (multiple lines)
    const int segments = 32;
    for (int i = 0; i < segments; ++i)
    {
        float angle1 = (i / (float)segments) * Dia::Maths::kTwoPi;
        float angle2 = ((i + 1) / (float)segments) * Dia::Maths::kTwoPi;
        
        Dia::Maths::Vector2D p1(
            center.x + Dia::Maths::Cos(angle1) * radius,
            center.y + Dia::Maths::Sin(angle1) * radius);
        
        Dia::Maths::Vector2D p2(
            center.x + Dia::Maths::Cos(angle2) * radius,
            center.y + Dia::Maths::Sin(angle2) * radius);
        
        canvas->DrawLine(p1, p2, Dia::Graphics::Color::Green);
    }
}
```

---

### Coordinate System

```cpp
// Screen space (origin top-left)
// X: 0 (left) → width (right)
// Y: 0 (top) → height (bottom)

Dia::Maths::Vector2D screenSize = canvas->GetSize();

// Top-left corner
Dia::Maths::Vector2D topLeft(0.0f, 0.0f);

// Top-right corner
Dia::Maths::Vector2D topRight(screenSize.x, 0.0f);

// Bottom-right corner
Dia::Maths::Vector2D bottomRight(screenSize.x, screenSize.y);

// Bottom-left corner
Dia::Maths::Vector2D bottomLeft(0.0f, screenSize.y);

// Center
Dia::Maths::Vector2D center = screenSize * 0.5f;
```

---

### View Transform (Camera)

```cpp
// Not directly supported by ICanvas
// Apply transform to world coordinates before drawing

Dia::Maths::Matrix33 cameraTransform = 
    Dia::Maths::Matrix33::Translation(-cameraPos.x, -cameraPos.y) *
    Dia::Maths::Matrix33::Scale(cameraZoom, cameraZoom);

// Transform world position to screen position
Dia::Maths::Vector2D worldPos(100.0f, 100.0f);
Dia::Maths::Vector2D screenPos = cameraTransform * worldPos;

canvas->DrawCircle(screenPos, 10.0f, Color::Red);
```

---

## Backend Implementation

### DiaSFML Implementation

**Class:** `Dia::SFML::DiaSFMLRenderWindow`

**Header:** `Dia/DiaSFML/DiaSFMLRenderWindow.h`

```cpp
class DiaSFMLRenderWindow : public Dia::Graphics::ICanvas
{
public:
    DiaSFMLRenderWindow(unsigned int width, unsigned int height);
    
    // ICanvas implementation
    void DrawLine(const Vector2D& start, const Vector2D& end, const Color& color) override;
    void DrawCircle(const Vector2D& center, float radius, const Color& color) override;
    void DrawRectangle(const Vector2D& position, const Vector2D& size, const Color& color) override;
    void DrawSprite(const Texture& texture, const Vector2D& position, const Vector2D& size, const Color& tint) override;
    void DrawText(const char* text, const Vector2D& position, const Color& color, unsigned int fontSize) override;
    
    void Clear(const Color& color) override;
    void Present() override;
    
    Vector2D GetSize() const override;
    
private:
    sf::RenderWindow mWindow;
};
```

**[→ DiaSFML API Details](sfml-api.md)**

---

## Dependencies

**Required:**
- `Dia/DiaMaths/` - Vector2D

**Implementation:**
- `Dia/DiaSFML/` - SFML backend

---

## Thread Safety

| Method | Thread Safety |
|--------|---------------|
| `DrawLine/Circle/Rectangle/Text` | ❌ Call from Render thread only |
| `Clear/Present` | ❌ Call from Render thread only |

**Important:** All ICanvas methods must be called from the **Render thread**.

---

## Best Practices

### 1. Clear Before Drawing

```cpp
// ✅ Good: Clear each frame
canvas->Clear(Color::Black);
DrawFrame();
canvas->Present();

// ❌ Bad: No clear (accumulates previous frames)
DrawFrame();
canvas->Present();
```

---

### 2. Present After Drawing

```cpp
// ✅ Good: Present after all drawing
DrawBackground();
DrawGameObjects();
DrawUI();
canvas->Present();

// ❌ Bad: Multiple presents (flicker)
DrawBackground();
canvas->Present();
DrawGameObjects();
canvas->Present();
```

---

### 3. Use Color Constants

```cpp
// ✅ Good: Use predefined colors
canvas->Clear(Color::Black);

// ❌ Bad: Magic numbers
canvas->Clear(Color(0, 0, 0, 255));
```

---

### 4. Batch Similar Draw Calls

```cpp
// ✅ Good: Batch by type/color
for (auto& circle : circles)
{
    canvas->DrawCircle(circle.position, circle.radius, Color::Red);
}

// Less efficient: Interleaved draw calls
for (int i = 0; i < count; ++i)
{
    canvas->DrawCircle(circles[i].position, circles[i].radius, Color::Red);
    canvas->DrawLine(lines[i].start, lines[i].end, Color::Blue);
}
```

---

## Gotchas

### Gotcha 1: Coordinate System Origin

Origin is **top-left**, not bottom-left. Y increases **downward**.

```cpp
// Top-left corner
Vector2D topLeft(0.0f, 0.0f);

// Move down (increase Y)
Vector2D down = topLeft + Vector2D(0.0f, 10.0f);  // (0, 10)
```

---

### Gotcha 2: Size vs Bounds

`DrawRectangle(position, size)` takes **size**, not end position:

```cpp
// ✅ Good: Size
canvas->DrawRectangle(Vector2D(10.0f, 10.0f), Vector2D(100.0f, 50.0f), color);

// ❌ Bad: End position (wrong interpretation)
canvas->DrawRectangle(Vector2D(10.0f, 10.0f), Vector2D(110.0f, 60.0f), color);
```

---

### Gotcha 3: Alpha Blending

Alpha values affect transparency:
- `255` = fully opaque
- `0` = fully transparent

---

## Limitations

### Current Limitations

1. **No texture management** - Backend handles textures
2. **No camera/viewport** - Manual transform needed
3. **Basic primitives only** - No polygons, curves, etc.
4. **No render states** - No blend modes, depth, etc.
5. **No batching** - Each draw call is immediate

### Future Improvements

- Camera/viewport support
- Sprite batching
- More primitives (polygon, ellipse, arc)
- Render-to-texture
- Shader support

**[→ Future Directions](../../design-rationale/future-directions.md)**

---

## Examples

### Full Render Loop

```cpp
class RenderModule : public Dia::Application::Module
{
private:
    void DoUpdate() override
    {
        Dia::Graphics::ICanvas* canvas = GetCanvas();
        
        // Clear
        canvas->Clear(Dia::Graphics::Color(20, 20, 30, 255));
        
        // Draw background
        DrawBackground(canvas);
        
        // Draw game objects
        for (auto& obj : gameObjects)
        {
            obj.Draw(canvas);
        }
        
        // Draw UI
        DrawUI(canvas);
        
        // Draw debug info
        #ifdef DEBUG
        DrawDebug(canvas);
        #endif
        
        // Present
        canvas->Present();
    }
};
```

---

### Drawing Shapes

```cpp
void DrawShapes(Dia::Graphics::ICanvas* canvas)
{
    using namespace Dia::Maths;
    using namespace Dia::Graphics;
    
    // Line
    canvas->DrawLine(
        Vector2D(50.0f, 50.0f),
        Vector2D(150.0f, 150.0f),
        Color::Red);
    
    // Circle
    canvas->DrawCircle(
        Vector2D(200.0f, 200.0f),
        30.0f,
        Color::Green);
    
    // Rectangle
    canvas->DrawRectangle(
        Vector2D(300.0f, 300.0f),
        Vector2D(80.0f, 50.0f),
        Color::Blue);
    
    // Text
    canvas->DrawText(
        "Hello World",
        Vector2D(10.0f, 10.0f),
        Color::White,
        16);
}
```

---

## Summary

**Core Interface:**
- `ICanvas` - Abstract rendering interface

**Drawing Methods:**
- `DrawLine()` - Line segment
- `DrawCircle()` - Filled circle
- `DrawRectangle()` - Filled rectangle
- `DrawSprite()` - Textured quad
- `DrawText()` - Text rendering

**Frame Control:**
- `Clear()` - Clear background
- `Present()` - Present frame

**Color:**
- RGBA (0-255 per channel)
- Predefined colors (Black, White, Red, etc.)

**Coordinate System:**
- Origin: Top-left
- X: Left → Right
- Y: Top → Bottom

**Thread Safety:**
- ❌ All methods Render thread only

**Implementation:**
- DiaSFML (SFML backend)

**[→ API Overview](../api-overview.md)**  
**[→ DiaSFML API](sfml-api.md)**  
**[→ DiaMaths API](maths-api.md)**
