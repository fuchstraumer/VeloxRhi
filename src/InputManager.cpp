#include "InputManager.hpp"
#include <algorithm>
#include <cmath>
#include <magic_enum/magic_enum.hpp>
#include <numbers>
#include <print>

#ifndef __EMSCRIPTEN__
namespace
{
struct GLFWwindow;
void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
void CursorEnterCallback(GLFWwindow* window, int entered);
void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void CharCallback(GLFWwindow* window, unsigned int codepoint);
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void KeyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void FocusCallback(GLFWwindow* window, int focused);
} // namespace
#else
#include <emscripten/html5.h>
namespace
{
EM_BOOL EmMouseMoveCallback(int event_type, const EmscriptenMouseEvent* mouse_event, void* user_data);
EM_BOOL EmMouseButtonCallback(int event_type, const EmscriptenMouseEvent* mouse_event, void* user_data);
EM_BOOL EmWheelCallback(int event_type, const EmscriptenWheelEvent* wheel_event, void* user_data);
EM_BOOL EmKeyCallback(int event_type, const EmscriptenKeyboardEvent* key_event, void* user_data);
EM_BOOL EmTouchCallback(int event_type, const EmscriptenTouchEvent* touch_event, void* user_data);
EM_BOOL EmFocusCallback(int event_type, const EmscriptenFocusEvent* focus_event, void* user_data);
} // namespace
#endif

namespace velox
{

// small epsilon to avoid jittery pinch gestures. may need tuning - i'm sure we could find examples though
constexpr float k_PinchGestureEpsilon = 0.01f;

InputManager::InputManager(Application* _app) noexcept
    : application{ _app }
{
}

InputManager::~InputManager()
{
#ifdef __EMSCRIPTEN__
    emscripten_set_mousemove_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_mousedown_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_mouseup_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_wheel_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_keydown_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_keyup_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_keypress_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_touchstart_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_touchend_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_touchmove_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_touchcancel_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_focus_callback("#canvas", nullptr, false, nullptr);
    emscripten_set_blur_callback("#canvas", nullptr, false, nullptr);
#endif
}

void InputManager::Initialize(void* platform_window)
{
#ifndef __EMSCRIPTEN__
    auto* Window = reinterpret_cast<GLFWwindow*>(platform_window);
    glfwSetWindowUserPointer(Window, this);
    glfwSetCursorPosCallback(Window, CursorPositionCallback);
    glfwSetCursorEnterCallback(Window, CursorEnterCallback);
    glfwSetScrollCallback(Window, ScrollCallback);
    glfwSetCharCallback(Window, CharCallback);
    glfwSetMouseButtonCallback(Window, MouseButtonCallback);
    glfwSetKeyCallback(Window, KeyboardCallback);
    glfwSetWindowFocusCallback(Window, FocusCallback);
#else
    emscripten_set_mousemove_callback("#canvas", this, false, EmMouseMoveCallback);
    emscripten_set_mousedown_callback("#canvas", this, false, EmMouseButtonCallback);
    emscripten_set_mouseup_callback("#canvas", this, false, EmMouseButtonCallback);
    emscripten_set_wheel_callback("#canvas", this, false, EmWheelCallback);
    emscripten_set_keydown_callback("#canvas", this, false, EmKeyCallback);
    emscripten_set_keyup_callback("#canvas", this, false, EmKeyCallback);
    emscripten_set_keypress_callback("#canvas", this, false, EmKeyCallback);
    emscripten_set_touchstart_callback("#canvas", this, false, EmTouchCallback);
    emscripten_set_touchend_callback("#canvas", this, false, EmTouchCallback);
    emscripten_set_touchmove_callback("#canvas", this, false, EmTouchCallback);
    emscripten_set_touchcancel_callback("#canvas", this, false, EmTouchCallback);
    emscripten_set_focus_callback("#canvas", this, false, EmFocusCallback);
    emscripten_set_blur_callback("#canvas", this, false, EmFocusCallback);
#endif
}

void InputManager::PostEvent(InputEvent event, double timestamp_ms)
{
    incomingEvents.push_back(ReceivedEvent{ timestamp_ms, std::move(event) });
}

std::span<const ReceivedEvent> InputManager::GetEventsForFrame() const noexcept
{
    return coalescedEventsForFrame;
}

std::span<const GestureEvent> InputManager::GetGesturesForFrame() const noexcept
{
    return gestureEvents;
}

void InputManager::PrepareFrame() noexcept
{
    evaluateGestures();
}

void InputManager::processCursorPositionEvent(const CursorPositionEvent& cursorPosEvent)
{
    if (gestureState.MouseDragging)
    {
        PanGestureEvent panEvent{ cursorPosEvent.X - gestureState.LastMouseX,
                                  cursorPosEvent.Y - gestureState.LastMouseY };
        gestureEvents.emplace_back(std::move(panEvent));
    }

    gestureState.LastMouseX = cursorPosEvent.X;
    gestureState.LastMouseY = cursorPosEvent.Y;
}

void InputManager::setMouseDragging(const PointerState& state) noexcept
{
    if (state == PointerState::Pressed)
    {
        gestureState.MouseDragging = true;
    }
    else if (state == PointerState::Released)
    {
        gestureState.MouseDragging = false;
    }
}

void InputManager::processPointerPressEvent(const double eventTime, const PointerEvent& pointerEvent)
{
    if (activeTouchPositions.empty())
    {
        gestureState.GestureStartTime = eventTime;
        gestureState.GestureCouldBeTap = true;
    }

    activeTouchPositions[pointerEvent.ID] =
        TouchState{ pointerEvent.X, pointerEvent.Y, pointerEvent.X, pointerEvent.Y, eventTime };
    gestureState.MaxSimultaneousTouches =
        std::max(gestureState.MaxSimultaneousTouches, static_cast<uint32_t>(activeTouchPositions.size()));
}

void InputManager::processPointerReleaseOrCancelEvent(const double eventTime,
                                                      const PointerEvent& pointerEvent)
{
    auto prevTouchIter = activeTouchPositions.find(pointerEvent.ID);
    if (prevTouchIter != activeTouchPositions.end())
    {
        if (pointerEvent.State == PointerState::Released)
        {
            const double Duration = eventTime - prevTouchIter->second.StartTime;
            const float Dx = pointerEvent.X - prevTouchIter->second.StartX;
            const float Dy = pointerEvent.Y - prevTouchIter->second.StartY;
            const float TotalDist = std::hypot(Dx, Dy);

            if (TotalDist > gestureConfig.TapMaxDistancePx)
            {
                gestureState.GestureCouldBeTap = false;
            }

            if (Duration > 0.0 && TotalDist > gestureConfig.SwipeMinDistancePx &&
                Duration < gestureConfig.SwipeMaxDurationMs)
            {
                gestureState.GestureCouldBeTap = false;
                SwipeGestureEvent swipeEvent{ static_cast<float>(Dx / Duration),
                                              static_cast<float>(Dy / Duration),
                                              pointerEvent.X,
                                              pointerEvent.Y,
                                              gestureState.MaxSimultaneousTouches };
                gestureEvents.emplace_back(std::move(swipeEvent));
            }
        }

        activeTouchPositions.erase(prevTouchIter);
    }

    if (activeTouchPositions.empty())
    {
        const double GestureDuration = eventTime - gestureState.GestureStartTime;
        if (gestureState.GestureCouldBeTap &&
            pointerEvent.State == PointerState::Released &&
            GestureDuration < gestureConfig.TapMaxDurationMs)
        {
            gestureEvents.emplace_back(evaluateTapGestureEvent(pointerEvent.X, pointerEvent.Y, eventTime));
        }

        gestureState.MaxSimultaneousTouches = 0;
        gestureState.GestureCouldBeTap = true;
    }
}

void InputManager::processPointerMoveEvent(const double eventTime, const PointerEvent& pointerEvent)
{
    auto prevIter = activeTouchPositions.find(pointerEvent.ID);
    if (prevIter == activeTouchPositions.end())
    {
        return;
    }

    // mutable as we update the last known position of touch point before returning
    TouchState& prevTouchState = prevIter->second;
    const float totalDx = pointerEvent.X - prevTouchState.StartX;
    const float totalDy = pointerEvent.Y - prevTouchState.StartY;

    // if the total distance moved exceeds the tap threshold, we can no longer consider this a tap gesture
    if (std::hypot(totalDx, totalDy) > gestureConfig.TapMaxDistancePx)
    {
        gestureState.GestureCouldBeTap = false;
    }

    if (activeTouchPositions.size() == 1)
    {
        PanGestureEvent panEvent{ pointerEvent.X - prevTouchState.X, pointerEvent.Y - prevTouchState.Y };
        gestureEvents.emplace_back(std::move(panEvent));
    }
    else if (activeTouchPositions.size() == 2)
    {
        // find the other touch point, so we can calculate the pinch gesture
        auto touchBeginIter = activeTouchPositions.begin();
        // if the first touch in the map is the one that moved, get the other one
        if (touchBeginIter->first == pointerEvent.ID)
        {
            ++touchBeginIter;
        }

        const TouchState& otherTouchState = touchBeginIter->second;
        // sign won't matter here because we're using hypot (like using distance() in shaders)
        const float prevDeltaX = prevTouchState.X - otherTouchState.X;
        const float prevDeltaY = prevTouchState.Y - otherTouchState.Y;
        const float prevDist = std::hypot(prevDeltaX, prevDeltaY);
        const float currDeltaX = pointerEvent.X - otherTouchState.X;
        const float currDeltaY = pointerEvent.Y - otherTouchState.Y;
        const float currDist = std::hypot(currDeltaX, currDeltaY);

        const float scale = prevDist > k_PinchGestureEpsilon ? currDist / prevDist : 1.0f;
        const float centerX = (pointerEvent.X + otherTouchState.X) * 0.5f;
        const float centerY = (pointerEvent.Y + otherTouchState.Y) * 0.5f;
        gestureEvents.emplace_back(PinchGestureEvent{ scale, centerX, centerY });

        // now evaluate for a rotation gesture
        const float prevAngle = std::atan2(prevDeltaX, prevDeltaY);
        const float currAngle = std::atan2(currDeltaX, currDeltaY);
        const float angleDelta = currAngle - prevAngle;
        // wrap angleDelta to [-pi, pi] range
        const float wrappedAngleDelta = std::fmod(angleDelta + std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>) -
                                        std::numbers::pi_v<float>;
        gestureEvents.emplace_back(RotationGestureEvent{ wrappedAngleDelta, centerX, centerY });
        // (yells internally: i wish this was implemented as a coroutine instead of fsm!)
    }

    // update the last known position of this touch point
    prevTouchState.X = pointerEvent.X;
    prevTouchState.Y = pointerEvent.Y;

}

TapGestureEvent InputManager::evaluateTapGestureEvent(const float posX,
                                                      const float posY,
                                                      const double eventTime) noexcept
{
    const bool posLastTapTime = gestureState.LastTapTime > 0.0;
    const double timeSinceLastTap = eventTime - gestureState.LastTapTime;
    const bool withinDoubleTapWindow = timeSinceLastTap < gestureConfig.DoubleTapWindowMs;
    const uint32_t TapCount = (posLastTapTime && withinDoubleTapWindow) ? 2u : 1u;
    const double newLastTapTime = (TapCount == 2u) ? 0.0 : eventTime;
    gestureState.LastTapTime = newLastTapTime;
    return std::move(TapGestureEvent{ posX, posY, gestureState.MaxSimultaneousTouches, TapCount });
}

void InputManager::evaluateGestures()
{
    coalescedEventsForFrame = std::move(incomingEvents);
    incomingEvents.clear();
    gestureEvents.clear();

    for (const ReceivedEvent& Ev : coalescedEventsForFrame)
    {
        if (const CursorPositionEvent* Cur = std::get_if<CursorPositionEvent>(&Ev.EventData))
        {
            processCursorPositionEvent(*Cur);
            continue;
        }

        const auto* Ptr = std::get_if<PointerEvent>(&Ev.EventData);
        if (!Ptr)
        {
            continue;
        }

        if (Ptr->Type == PointerType::Mouse)
        {
            setMouseDragging(Ptr->State);
            continue;
        }

        // logic from here on out is only used if the pointer type is a touch event
        if (Ptr->Type != PointerType::Touch)
        {
            continue;
        }

        switch (Ptr->State)
        {
        case PointerState::Pressed:
            processPointerPressEvent(Ev.Time, *Ptr);
            break;
        case PointerState::Released:
            [[fallthrough]];
        case PointerState::Cancelled:
            processPointerReleaseOrCancelEvent(Ev.Time, *Ptr);
            break;
        case PointerState::Moved:
            processPointerMoveEvent(Ev.Time, *Ptr);
            break;
        default:
            break;
        }
    }
}

} // namespace velox

#ifndef __EMSCRIPTEN__
namespace
{
#define GLFW_INCLUDE_NONE
#include "glfw/glfw3.h"
using namespace velox;

void CursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->PostEvent(
        InputEvent{ CursorPositionEvent{ static_cast<float>(xpos), static_cast<float>(ypos) } },
        glfwGetTime() * 1000.0);
}

void CursorEnterCallback(GLFWwindow* window, int entered)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    PointerEvent Event{};
    Event.Type = PointerType::Mouse;
    Event.State = entered ? PointerState::Entered : PointerState::Exited;
    inputManager->PostEvent(InputEvent{ Event }, glfwGetTime() * 1000.0);
}

void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->PostEvent(
        InputEvent{ ScrollEvent{ static_cast<float>(xoffset), static_cast<float>(yoffset) } },
        glfwGetTime() * 1000.0);
}

void CharCallback(GLFWwindow* window, unsigned int codepoint)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->PostEvent(InputEvent{ TextInputEvent{ codepoint } }, glfwGetTime() * 1000.0);
}

void MouseButtonCallback(GLFWwindow* window, int button, int action, int /*mods*/)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    PointerEvent Event{};
    Event.ID = static_cast<uint32_t>(button);
    Event.Type = PointerType::Mouse;
    Event.State = action ? PointerState::Pressed : PointerState::Released;
    inputManager->PostEvent(InputEvent{ Event }, glfwGetTime() * 1000.0);
}

void KeyboardCallback(GLFWwindow* window, int key, int /*scancode*/, int action, int mods)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    KeyEvent Event{};
    Event.KeyCode = static_cast<uint16_t>(key);
    Event.Modifiers = static_cast<uint8_t>(mods);
    switch (action)
    {
    case 0: // GLFW_RELEASE
        Event.State = KeyState::Released;
        break;
    case 1: // GLFW_PRESS
        Event.State = KeyState::Pressed;
        break;
    case 2: // GLFW_REPEAT
        Event.State = KeyState::Held;
        break;
    default:
        break;
    }
    inputManager->PostEvent(InputEvent{ Event }, glfwGetTime() * 1000.0);
}

void FocusCallback(GLFWwindow* window, int focused)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(glfwGetWindowUserPointer(window));
    inputManager->PostEvent(InputEvent{ FocusEvent{ focused } }, glfwGetTime() * 1000.0);
}

} // namespace
#else
namespace
{
using namespace velox;

EM_BOOL EmMouseMoveCallback(int /*event_type*/, const EmscriptenMouseEvent* mouse_event, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    inputManager->PostEvent(InputEvent{ CursorPositionEvent{
                                static_cast<float>(mouse_event->clientX),
                                static_cast<float>(mouse_event->clientY),
                            } },
                            mouse_event->timestamp);
    return EM_FALSE;
}

EM_BOOL EmMouseButtonCallback(int event_type, const EmscriptenMouseEvent* mouse_event, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    PointerEvent Event{};
    Event.ID = static_cast<uint32_t>(mouse_event->button);
    Event.Type = PointerType::Mouse;
    Event.State = (event_type == EMSCRIPTEN_EVENT_MOUSEDOWN) ? PointerState::Pressed : PointerState::Released;
    Event.X = static_cast<float>(mouse_event->clientX);
    Event.Y = static_cast<float>(mouse_event->clientY);
    inputManager->PostEvent(InputEvent{ Event }, mouse_event->timestamp);
    return EM_FALSE;
}

EM_BOOL EmWheelCallback(int /*event_type*/, const EmscriptenWheelEvent* wheel_event, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    inputManager->PostEvent(InputEvent{ ScrollEvent{
                                static_cast<float>(wheel_event->deltaX),
                                static_cast<float>(wheel_event->deltaY),
                            } },
                            wheel_event->mouse.timestamp);
    return EM_FALSE;
}

EM_BOOL EmKeyCallback(int event_type, const EmscriptenKeyboardEvent* key_event, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    // keypress carries the Unicode code point for text input
    if (event_type == EMSCRIPTEN_EVENT_KEYPRESS && key_event->charCode != 0)
    {
        inputManager->PostEvent(InputEvent{ TextInputEvent{ static_cast<uint32_t>(key_event->charCode) } },
                                key_event->timestamp);
        return EM_FALSE;
    }
    KeyEvent Event{};
    Event.KeyCode = static_cast<uint16_t>(key_event->keyCode);
    Event.Modifiers = static_cast<uint8_t>((key_event->shiftKey ? 1u : 0u) | (key_event->ctrlKey ? 2u : 0u) |
                                           (key_event->altKey ? 4u : 0u) | (key_event->metaKey ? 8u : 0u));
    Event.State = (event_type == EMSCRIPTEN_EVENT_KEYDOWN)
                      ? (key_event->repeat ? KeyState::Held : KeyState::Pressed)
                      : KeyState::Released;
    inputManager->PostEvent(InputEvent{ Event }, key_event->timestamp);
    return EM_FALSE;
}

EM_BOOL EmTouchCallback(int event_type, const EmscriptenTouchEvent* touch_event, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    PointerState State = PointerState::Invalid;
    switch (event_type)
    {
    case EMSCRIPTEN_EVENT_TOUCHSTART:
        State = PointerState::Pressed;
        break;
    case EMSCRIPTEN_EVENT_TOUCHEND:
        State = PointerState::Released;
        break;
    case EMSCRIPTEN_EVENT_TOUCHMOVE:
        State = PointerState::Moved;
        break;
    case EMSCRIPTEN_EVENT_TOUCHCANCEL:
        State = PointerState::Cancelled;
        break;
    default:
        break;
    }
    for (int i = 0; i < touch_event->numTouches; ++i)
    {
        const EmscriptenTouchPoint& Point = touch_event->touches[i];
        if (!Point.isChanged)
        {
            continue;
        }
        PointerEvent Event{};
        Event.ID = static_cast<uint32_t>(Point.identifier);
        Event.Type = PointerType::Touch;
        Event.State = State;
        Event.X = static_cast<float>(Point.clientX);
        Event.Y = static_cast<float>(Point.clientY);
        inputManager->PostEvent(InputEvent{ Event }, touch_event->timestamp);
    }
    return EM_FALSE;
}

EM_BOOL EmFocusCallback(int event_type, const EmscriptenFocusEvent* /*focus_event*/, void* user_data)
{
    InputManager* inputManager = reinterpret_cast<InputManager*>(user_data);
    int32_t Focused =
        (event_type == EMSCRIPTEN_EVENT_FOCUS || event_type == EMSCRIPTEN_EVENT_FOCUSIN) ? 1 : 0;
    inputManager->PostEvent(InputEvent{ FocusEvent{ Focused } }, emscripten_get_now());
    return EM_FALSE;
}

} // namespace
#endif