#pragma once

#include <coroutine>

namespace open_spiel
{
    namespace dominion
    {

        class DominionState; // Forward declaration
        class PlayerState;   // Forward declaration

        // template <typename T = void>
        class Coroutine
        {
        public:
            struct Promise
            {
                std::coroutine_handle<> continuation_;

                Coroutine get_return_object() { return Coroutine{std::coroutine_handle<Promise>::from_promise(*this)}; }
                std::suspend_never initial_suspend() { return {}; }
                std::suspend_always final_suspend() noexcept { return {}; }
                void return_void()
                {
                    if (continuation_)
                        continuation_.resume();
                }
                void unhandled_exception() {}
            };

            using promise_type = Promise;
            using handle_type = std::coroutine_handle<Promise>;

            Coroutine(handle_type handle) : handle_(handle) {}
            ~Coroutine()
            {
                if (handle_)
                    handle_.destroy();
            }

            bool Done() const { return handle_.done(); }

            void Resume() { handle_.resume(); }

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> h)
            {
                handle_.promise().continuation_ = h;
            }
            void await_resume() {};

        private:
            handle_type handle_;
        };

        struct ActionAwaiter
        {
            DominionState &state;

            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> handle);
            void await_resume() {};
        };

    } // namespace dominion
} // namespace open_spiel
