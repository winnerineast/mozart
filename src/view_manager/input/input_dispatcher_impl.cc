// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/mozart/src/view_manager/input/input_dispatcher_impl.h"

#include <queue>

#include "apps/mozart/services/composition/cpp/formatting.h"
#include "apps/mozart/services/geometry/cpp/geometry_util.h"
#include "apps/mozart/services/input/cpp/formatting.h"
#include "apps/mozart/services/views/cpp/formatting.h"
#include "apps/mozart/src/view_manager/internal/input_owner.h"
#include "apps/mozart/src/view_manager/internal/view_inspector.h"
#include "lib/ftl/functional/make_copyable.h"
#include "lib/mtl/tasks/message_loop.h"

namespace view_manager {
namespace {
void TransformEvent(const mozart::Transform& transform,
                    mozart::InputEvent* event) {
  if (!event->is_pointer())
    return;
  const mozart::PointerEventPtr& pointer = event->get_pointer();
  mozart::PointF point;
  point.x = pointer->x;
  point.y = pointer->y;
  point = TransformPoint(transform, point);
  pointer->x = point.x;
  pointer->y = point.y;
}

// The input event fidl is currently defined to expect some number
// of milliseconds.
int64_t InputEventTimestampNow() {
  return ftl::TimePoint::Now().ToEpochDelta().ToNanoseconds();
}
}  // namespace

InputDispatcherImpl::InputDispatcherImpl(
    ViewInspector* inspector,
    InputOwner* owner,
    mozart::ViewTreeTokenPtr view_tree_token,
    fidl::InterfaceRequest<mozart::InputDispatcher> request)
    : inspector_(inspector),
      owner_(owner),
      view_tree_token_(std::move(view_tree_token)),
      hit_tester_(
          new ViewTreeHitTesterClient(inspector_, view_tree_token_.Clone())),
      view_hit_resolver_(new ViewHitResolver(owner_)),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  FTL_DCHECK(inspector_);
  FTL_DCHECK(view_tree_token_);

  binding_.set_connection_error_handler(
      [this] { owner_->OnInputDispatcherDied(this); });
}

InputDispatcherImpl::~InputDispatcherImpl() {}

void InputDispatcherImpl::DispatchEvent(mozart::InputEventPtr event) {
  FTL_DCHECK(event);
  FTL_VLOG(1) << "DispatchEvent: " << *event;

  pending_events_.push(std::move(event));
  if (pending_events_.size() == 1u)
    ProcessNextEvent();
}

void InputDispatcherImpl::ProcessNextEvent() {
  FTL_DCHECK(!pending_events_.empty());

  do {
    const mozart::InputEvent* event = pending_events_.front().get();
    FTL_VLOG(1) << "ProcessNextEvent: " << *event;

    if (event->is_pointer()) {
      const mozart::PointerEventPtr& pointer = event->get_pointer();
      if (pointer->phase == mozart::PointerEvent::Phase::DOWN) {
        auto point = mozart::PointF::New();
        point->x = pointer->x;
        point->y = pointer->y;
        FTL_VLOG(1) << "HitTest: point=" << point;
        auto hit_result_callback = ftl::MakeCopyable([
          pt = point.Clone(), weak = weak_factory_.GetWeakPtr()
        ](std::unique_ptr<ResolvedHits> resolved_hits) mutable {
          if (weak)
            weak->OnHitTestResult(std::move(pt), std::move(resolved_hits));
        });
        hit_tester_->HitTest(std::move(point), hit_result_callback);
        return;
      }
    } else if (event->is_keyboard()) {
      inspector_->ResolveFocusChain(
          view_tree_token_.Clone(), [weak = weak_factory_.GetWeakPtr()](
                                        std::unique_ptr<FocusChain>
                                            focus_chain) {
            if (weak) {
              // Make sure to keep processing events when no focus is defined
              if (focus_chain) {
                weak->OnFocusResult(std::move(focus_chain));
              } else {
                weak->PopAndScheduleNextEvent();
              }
            }
          });
      return;
    }
    DeliverEvent(std::move(pending_events_.front()));
    pending_events_.pop();
  } while (!pending_events_.empty());
}

void InputDispatcherImpl::DeliverEvent(uint64_t event_path_propagation_id,
                                       const EventPath* event_path,
                                       mozart::InputEventPtr event) {
  FTL_VLOG(1) << "DeliverEvent " << event_path_propagation_id << " " << *event;
  // TODO(jpoichet) when the chain is changed, we might need to cancel events
  // that have not progagated fully through the chain.
  if (event_path && event_path_propagation_id_ == event_path_propagation_id) {
    FTL_DCHECK(event_path->token);
    FTL_DCHECK(event_path->transform);
    mozart::InputEventPtr cloned_event = event.Clone();
    // TODO(jpoichet) once input arena is in place, we won't need the "handled"
    // boolean on the callback anymore.
    owner_->DeliverEvent(
        event_path->token.get(), std::move(event), ftl::MakeCopyable([
          this, event_path_propagation_id, event_path,
          cloned_event = std::move(cloned_event)
        ](bool handled) mutable {
          if (!handled &&
              event_path_propagation_id_ == event_path_propagation_id &&
              event_path && event_path->next) {
            // Avoid re-entrance on DeliverEvent
            mtl::MessageLoop::GetCurrent()->task_runner()->PostTask(
                ftl::MakeCopyable([
                  weak = weak_factory_.GetWeakPtr(), event_path_propagation_id,
                  event_path = std::move(event_path),
                  cloned_event = std::move(cloned_event)
                ]() mutable {
                  if (weak)
                    weak->DeliverEvent(event_path_propagation_id,
                                       event_path->next.get(),
                                       std::move(cloned_event));

                }));
          }
        }));
  }
}

void InputDispatcherImpl::DeliverEvent(mozart::InputEventPtr event) {
  if (event_path_) {
    TransformEvent(*(event_path_->transform), event.get());
    DeliverEvent(event_path_propagation_id_, event_path_.get(),
                 std::move(event));
  }
}

void InputDispatcherImpl::DeliverKeyEvent(
    std::unique_ptr<FocusChain> focus_chain,
    uint64_t propagation_index,
    mozart::InputEventPtr event) {
  FTL_DCHECK(propagation_index < focus_chain->chain.size());
  FTL_VLOG(1) << "DeliverKeyEvent " << focus_chain->version << " "
              << (1 + propagation_index) << "/" << focus_chain->chain.size()
              << " " << *(focus_chain->chain[propagation_index]) << " "
              << *event;

  auto cloned_event = event.Clone();
  owner_->DeliverEvent(
      focus_chain->chain[propagation_index].get(), std::move(event),
      ftl::MakeCopyable([
        this, focus_chain = std::move(focus_chain), propagation_index,
        cloned_event = std::move(cloned_event)
      ](bool handled) mutable {
        FTL_VLOG(2) << "Event " << *cloned_event << (handled ? "" : " Not")
                    << " Handled by "
                    << *(focus_chain->chain[propagation_index]);

        if (!handled && propagation_index + 1 < focus_chain->chain.size()) {
          // Avoid re-entrance on DeliverKeyEvent
          mtl::MessageLoop::GetCurrent()->task_runner()->PostTask(
              ftl::MakeCopyable([
                weak = weak_factory_.GetWeakPtr(),
                focus_chain = std::move(focus_chain), propagation_index,
                cloned_event = std::move(cloned_event)
              ]() mutable {
                FTL_VLOG(2) << "Propagating event to "
                            << *(focus_chain->chain[propagation_index + 1]);

                if (weak)
                  weak->DeliverKeyEvent(std::move(focus_chain),
                                        propagation_index + 1,
                                        std::move(cloned_event));

              }));
        }
      }));
}

void InputDispatcherImpl::PopAndScheduleNextEvent() {
  if (!pending_events_.empty()) {
    pending_events_.pop();
    if (!pending_events_.empty()) {
      // Prevent reentrance from ProcessNextEvent.
      auto process_next_event = [weak = weak_factory_.GetWeakPtr()] {
        if (weak)
          weak->ProcessNextEvent();
      };
      mtl::MessageLoop::GetCurrent()->task_runner()->PostTask(
          process_next_event);
    }
  }
}

void InputDispatcherImpl::OnFocusResult(
    std::unique_ptr<FocusChain> focus_chain) {
  FTL_VLOG(1) << "OnFocusResult " << focus_chain->version << " "
              << focus_chain->chain.size();
  if (focus_chain->chain.size() > 0) {
    DeliverKeyEvent(std::move(focus_chain), 0,
                    std::move(pending_events_.front()));
  }
  PopAndScheduleNextEvent();
}

void InputDispatcherImpl::OnHitTestResult(
    mozart::PointFPtr point,
    std::unique_ptr<ResolvedHits> resolved_hits) {
  FTL_DCHECK(!pending_events_.empty());
  FTL_VLOG(1) << "OnHitTestResult: resolved_hits=" << resolved_hits.get();

  if (resolved_hits && resolved_hits->result()->root) {
    mozart::HitTestResultPtr result = resolved_hits->TakeResult();
    const mozart::SceneHit* root_scene = result->root.get();
    view_hit_resolver_->Resolve(
        root_scene, std::move(point), std::move(resolved_hits),
        [this](std::vector<std::unique_ptr<EventPath>> views) {
          if (views.empty()) {
            return;
          }

          // FIXME(jpoichet) This should be done somewhere else.
          inspector_->ActivateFocusChain(
              views.back()->token->Clone(),
              [this](std::unique_ptr<FocusChain> new_chain) {
                if (!active_focus_chain_ ||
                    active_focus_chain_->chain.front()->value !=
                        new_chain->chain.front()->value) {
                  if (active_focus_chain_) {
                    FTL_VLOG(1) << "Input focus lost by "
                                << *(active_focus_chain_->chain.front().get());
                    mozart::InputEventPtr event = mozart::InputEvent::New();
                    mozart::FocusEventPtr focus = mozart::FocusEvent::New();
                    focus->event_time = InputEventTimestampNow();
                    focus->focused = false;
                    event->set_focus(std::move(focus));
                    owner_->DeliverEvent(
                        active_focus_chain_->chain.front().get(),
                        std::move(event), nullptr);
                  }

                  FTL_VLOG(1) << "Input focus gained by "
                              << *(new_chain->chain.front().get());
                  mozart::InputEventPtr event = mozart::InputEvent::New();
                  mozart::FocusEventPtr focus = mozart::FocusEvent::New();
                  focus->event_time = InputEventTimestampNow();
                  focus->focused = true;
                  event->set_focus(std::move(focus));
                  owner_->DeliverEvent(new_chain->chain.front().get(),
                                       std::move(event), nullptr);

                  active_focus_chain_ = std::move(new_chain);
                }
              });

          // TODO(jpoichet) Implement Input Arena
          event_path_propagation_id_++;
          event_path_ = std::move(views.back());

          FTL_VLOG(1) << "OnViewHitResolved: view_token_=" << event_path_->token
                      << ", view_transform_=" << event_path_->transform
                      << ", event_path_propagation_id_="
                      << event_path_propagation_id_;

          DeliverEvent(std::move(pending_events_.front()));
          PopAndScheduleNextEvent();
        });
  } else {
    PopAndScheduleNextEvent();
  }
}

}  // namespace view_manager
