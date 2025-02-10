#include "effects.h"

#include <algorithm>
#include <coroutine>

#include "dominion.h"

namespace open_spiel {
namespace dominion {
void ActionAwaiter::await_suspend(std::coroutine_handle<> handle) {
  state.continuation_ = handle;
}

void Coroutine::await_suspend(std::coroutine_handle<Promise> handle) {
  this->promise().continuation_ = handle;
}

Action ActionAwaiter::await_resume() { return state.pending_action.value(); }
}  // namespace dominion
}  // namespace open_spiel