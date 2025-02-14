#include "effects.h"

#include <algorithm>
#include <coroutine>

#include "dominion.h"

namespace open_spiel {
namespace dominion {
void ActionAwaiterBase::await_suspend(std::coroutine_handle<> handle) {
  state.continuation_ = handle;
  state.pending_legal_actions = legal_actions;
}

void Coroutine::await_suspend(std::coroutine_handle<Promise> handle) {
  this->promise().continuation_ = handle;
}

Action ActionAwaiter::await_resume() { return state.pending_action.value(); }

DominionAction DominionActionAwaiter::await_resume() {
  return DominionAction(state.pending_action.value());
}

ActionAwaiter getAction(DominionState &state,
                        std::vector<Action> legal_actions) {
  return ActionAwaiter(state, legal_actions);
}

DominionActionAwaiter getDominionAction(DominionState &state,
                                        std::vector<Action> legal_actions) {
  return DominionActionAwaiter(state, legal_actions);
}

}  // namespace dominion
}  // namespace open_spiel