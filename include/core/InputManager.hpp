#pragma once
#ifndef VELOX_INPUT_MANAGER_HPP
#define VELOX_INPUT_MANAGER_HPP
#include "InputEvent.hpp"
#include <ranges>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace velox
{
class Application;

template<typename T>
concept InputEventType = requires(const InputEvent& e) { std::get<T>(e); };

class InputManager
{
public:
    InputManager(Application* _app) noexcept;
    ~InputManager();
    // platformWindow is a GLFWwindow* on desktop, nullptr on Emscripten (we'll use the default window)
    void Initialize([[maybe_unused]] void* platformWindow);
    // will forward/move, whoever is posting this should be done with it after posting
    void PostEvent(InputEvent event, double timestamp_ms);
    // Clients can retrieve this and process the events they care about for the frame
    std::span<const ReceivedEvent> GetEventsForFrame() const noexcept;
    std::span<const GestureEvent> GetGesturesForFrame() const noexcept;
    // Coalesces incoming events and evaluates gesture recognizers; call once per frame
    void PrepareFrame() noexcept;

    template<typename... Ts>
        requires((sizeof...(Ts) >= 1) && (InputEventType<Ts> && ...))
    auto GetEventsOfType() const noexcept
    {
        if constexpr (sizeof...(Ts) == 1)
        {
            using T = std::tuple_element_t<0, std::tuple<Ts...>>;
            return coalescedEventsForFrame |
                   std::views::filter(
                       [](const ReceivedEvent& e)
                       {
                           return std::holds_alternative<T>(e.EventData);
                       }) |
                   std::views::transform(
                       [](const ReceivedEvent& e) -> const T&
                       {
                           return std::get<T>(e.EventData);
                       });
        }
        else
        {
            return coalescedEventsForFrame | std::views::filter(
                                                 [](const ReceivedEvent& e)
                                                 {
                                                     return (std::holds_alternative<Ts>(e.EventData) || ...);
                                                 });
        }
    }

private:
    // todo: this could also be replaced with a series of coroutines, one per gesture recognizer, which would
    // be a more elegant solution than the FSM backing all of this. less buggy, too!
    void evaluateGestures();
    void processCursorPositionEvent(const CursorPositionEvent& cursorPosEvent);
    void setMouseDragging(const PointerState& state) noexcept;
    void processPointerPressEvent(const double eventTime, const PointerEvent& pointerEvent);
    void processPointerReleaseOrCancelEvent(const double eventTime, const PointerEvent& pointerEvent);
    void processPointerMoveEvent(const double eventTime, const PointerEvent& pointerEvent);

    // mutuates gestureState.LastTapTime... another instance of why FSMs make me feel icky. 
    TapGestureEvent evaluateTapGestureEvent(const float posX, const float posY, const double eventTime) noexcept;

    struct TouchState
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float StartX{ 0.0f };
        float StartY{ 0.0f };
        double StartTime{ 0.0 };
    };

    Application* application{ nullptr };
    std::vector<ReceivedEvent> incomingEvents;
    std::vector<ReceivedEvent> coalescedEventsForFrame;
    std::vector<GestureEvent> gestureEvents;
    std::unordered_map<uint32_t, TouchState> activeTouchPositions;

    // encapsulate state into a struct - makes it a lot easier to reset, and will help refactoring
    // full reset is needed on focus change, for example, and this makes that trivial to implement
    struct GestureRecognizerState
    {
        float LastMouseX{ 0.0f };
        float LastMouseY{ 0.0f };
        bool MouseDragging{ false };
        uint32_t MaxSimultaneousTouches{ 0 };
        double GestureStartTime{ 0.0 };
        double LastTapTime{ 0.0 };
        bool GestureCouldBeTap{ true };
    };

    GestureRecognizerState gestureState;

    // these were previously constant statics in the body of evaluateGestures(), but encapsulating them here
    // makes it easier to drive them via configuration or from experimentation to see what works
    struct GestureRecognizerConfig
    {
        double TapMaxDurationMs{ 300.0 };
        double DoubleTapWindowMs{ 400.0 };
        double SwipeMaxDurationMs{ 500.0 };
        float TapMaxDistancePx{ 15.0f };
        float SwipeMinDistancePx{ 30.0f };
    };

    GestureRecognizerConfig gestureConfig;
};
} // namespace velox

#endif // !VELOX_INPUT_MANAGER_HPP
