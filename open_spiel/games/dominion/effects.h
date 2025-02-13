#pragma once

#include <coroutine>

#include "open_spiel/spiel.h"

namespace open_spiel {
namespace dominion {

class DominionState;  // Forward declaration
class PlayerState;    // Forward declaration
struct DominionAction;

struct Promise;

// template <typename T = void>
struct Coroutine : std::coroutine_handle<Promise> {
  using promise_type = Promise;

  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<Promise> handle);
  void await_resume() {};
};

struct Promise {
  std::coroutine_handle<> continuation_;

  Coroutine get_return_object() { return {Coroutine::from_promise(*this)}; }
  std::suspend_never initial_suspend() { return {}; }
  std::suspend_always final_suspend() noexcept {
    if (continuation_) {
      continuation_.resume();
    }
    return {};
  }
  void return_void() {}
  void unhandled_exception() {}
};

struct ActionAwaiterBase {
  DominionState &state;
  std::vector<Action> legal_actions;

  bool await_ready() const noexcept { return false; }
  void await_suspend(std::coroutine_handle<> handle);
};

struct ActionAwaiter : ActionAwaiterBase {
  ActionAwaiter(DominionState &state, std::vector<Action> legal_actions)
      : ActionAwaiterBase(state, legal_actions) {}
  Action await_resume();
};

struct DominionActionAwaiter : ActionAwaiterBase {
  DominionActionAwaiter(DominionState &state, std::vector<Action> legal_actions)
      : ActionAwaiterBase(state, legal_actions) {}
  DominionAction await_resume();
};

ActionAwaiter getAction(DominionState &state,
                        std::vector<Action> legal_actions);
DominionActionAwaiter getDominionAction(DominionState &state,
                                        std::vector<Action> legal_actions);

}  // namespace dominion
}  // namespace open_spiel
