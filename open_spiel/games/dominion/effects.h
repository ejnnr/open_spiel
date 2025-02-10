#pragma once

#include "open_spiel/spiel.h"

#include <coroutine>

namespace open_spiel
{
    namespace dominion
    {

        class DominionState; // Forward declaration
        class PlayerState;   // Forward declaration

        struct Promise;

        // template <typename T = void>
        struct Coroutine : std::coroutine_handle<Promise>
        {
            using promise_type = Promise;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<Promise> handle);
            void await_resume() {};
        };

        struct Promise
        {
            std::coroutine_handle<> continuation_;

            Coroutine get_return_object() { return {Coroutine::from_promise(*this)}; }
            std::suspend_never initial_suspend() { return {}; }
            std::suspend_always final_suspend() noexcept
            {
                if (continuation_)
                {
                    continuation_.resume();
                }
                return {};
            }
            void return_void() {}
            void unhandled_exception() {}
        };

        struct ActionAwaiter
        {
            DominionState &state;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle);
            Action await_resume();
        };

    } // namespace dominion
} // namespace open_spiel
