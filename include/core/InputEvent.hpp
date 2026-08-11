#pragma once
#ifndef VELOX_INPUT_EVENT_HPP
#define VELOX_INPUT_EVENT_HPP
#include <cstdint>
#include <limits>
#include <variant>
#include <string_view>
#include <span>

// Encapsulated and dependency-free abstractions for input events, to unify platform-specific events
namespace velox
{

enum class PointerType : uint8_t
{
    Invalid = 0,
    Mouse,
    Touch,
    Pen
};

enum class PointerState : uint8_t
{
    Invalid = 0,
    Pressed,
    Released,
    Moved,
    Entered,
    Exited,
    Cancelled
};

struct CursorPositionEvent
{
    float X{ 0.0f };
    float Y{ 0.0f };
};

struct PointerEvent
{
    uint32_t ID{ std::numeric_limits<uint32_t>::max() };
    PointerType Type{ PointerType::Invalid };
    PointerState State{ PointerState::Invalid };
    float X{ 0.0f };
    float Y{ 0.0f };
};

struct ScrollEvent
{
    float DeltaX{ 0.0f };
    float DeltaY{ 0.0f };
};

enum class KeyState : uint8_t
{
    Invalid = 0,
    Pressed,
    Held,
    Released
};

struct KeyEvent
{
    uint16_t KeyCode{ 0 };
    uint8_t Modifiers{ 0 }; // Bitmask for modifier keys (Shift, Ctrl, Alt, etc.)
    KeyState State{ KeyState::Invalid };
};

struct FocusEvent
{
    int32_t Focused{ 0 }; // 1 for focused, 0 for unfocused
};

struct PathDropEvent
{
    std::span<const std::string_view> Paths;
};

struct PanGestureEvent
{
    float DeltaX{ 0.0f };
    float DeltaY{ 0.0f };
};

struct PinchGestureEvent
{
    float Scale{ 1.0f };
    float CenterX{ 0.0f };
    float CenterY{ 0.0f };
};

struct RotationGestureEvent
{
    float AngleDelta{ 0.0f }; // radians, positive = CCW
    float CenterX{ 0.0f };
    float CenterY{ 0.0f };
};

struct TextInputEvent
{
    uint32_t Codepoint{ 0 };
};

using InputEvent =
    std::variant<CursorPositionEvent, PointerEvent, ScrollEvent, KeyEvent, FocusEvent, PathDropEvent, TextInputEvent>;

struct ReceivedEvent
{
    double Time{ 0.0 }; // ms, high-res platform time
    InputEvent EventData;
};


struct SwipeGestureEvent
{
    float VelocityX{ 0.0f }; // pixels/ms
    float VelocityY{ 0.0f };
    float X{ 0.0f };
    float Y{ 0.0f };
    uint32_t FingerCount{ 1 };
};

struct TapGestureEvent
{
    float X{ 0.0f };
    float Y{ 0.0f };
    uint32_t FingerCount{ 1 };
    uint32_t TapCount{ 1 }; // 2 = double-tap
};

using GestureEvent = std::variant<PanGestureEvent, PinchGestureEvent, RotationGestureEvent, SwipeGestureEvent, TapGestureEvent>;

}; // namespace velox

#endif // !VELOX_INPUT_EVENT_HPP
