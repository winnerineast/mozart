// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/mozart/lib/tests/mocks/mock_view_listener.h"

namespace mozart {
namespace test {

MockViewListener::MockViewListener() : callback_(nullptr) {}

MockViewListener::MockViewListener(const OnMockInvalidationCallback& callback)
    : callback_(callback) {}

MockViewListener::~MockViewListener() {}

void MockViewListener::OnInvalidation(mozart::ViewInvalidationPtr invalidation,
                                      const OnInvalidationCallback& callback) {
  if (callback_) {
    callback_(std::move(invalidation));
  }
  callback();
}

}  // namespace test
}  // namespace mozart