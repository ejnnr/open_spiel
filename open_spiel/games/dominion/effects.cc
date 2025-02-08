#include "effects.h"
#include "dominion.h"

#include <coroutine>
#include <algorithm>

namespace open_spiel
{
    namespace dominion
    {
        void ActionAwaiter::await_suspend(std::coroutine_handle<> handle)
        {
            state.continuation_ = handle;
        }
    }
}