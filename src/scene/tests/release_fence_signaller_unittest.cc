// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "escher/impl/command_buffer_sequencer.h"
#include "gtest/gtest.h"

#include "apps/mozart/src/scene/fence.h"
#include "apps/mozart/src/scene/release_fence_signaller.h"
#include "apps/mozart/src/scene/tests/util.h"

namespace mozart {
namespace scene {
namespace test {

using ReleaseFenceSignallerTest = ::testing::Test;

TEST_F(ReleaseFenceSignallerTest, FencesSignalledProperly) {
  escher::impl::CommandBufferSequencer sequencer;
  ReleaseFenceSignaller release_fence_signaler(&sequencer);

  // Create two fences.
  uint64_t seq_num1 = sequencer.GetNextCommandBufferSequenceNumber();
  mx::event fence1;
  ASSERT_EQ(mx::event::create(0, &fence1), MX_OK);
  release_fence_signaler.AddCPUReleaseFence(CopyEvent(fence1));

  uint64_t seq_num2 = sequencer.GetNextCommandBufferSequenceNumber();
  mx::event fence2;
  ASSERT_EQ(mx::event::create(0, &fence2), MX_OK);
  release_fence_signaler.AddCPUReleaseFence(CopyEvent(fence2));

  // Create a third fence that will not be signaled initially.
  uint64_t seq_num3 = sequencer.GetNextCommandBufferSequenceNumber();
  mx::event fence3;
  ASSERT_EQ(mx::event::create(0, &fence3), MX_OK);
  release_fence_signaler.AddCPUReleaseFence(CopyEvent(fence3));

  // Assert that none of the fences are signalled.
  ASSERT_FALSE(IsEventSignalled(fence1, kFenceSignalled));
  ASSERT_FALSE(IsEventSignalled(fence2, kFenceSignalled));

  // Mark the sequence numbers so far as finished. (Do it out of order for fun).
  sequencer.CommandBufferFinished(seq_num2);
  sequencer.CommandBufferFinished(seq_num1);

  ASSERT_TRUE(IsEventSignalled(fence1, kFenceSignalled));
  ASSERT_TRUE(IsEventSignalled(fence2, kFenceSignalled));
  ASSERT_FALSE(IsEventSignalled(fence3, kFenceSignalled));

  sequencer.CommandBufferFinished(seq_num3);

  ASSERT_TRUE(IsEventSignalled(fence1, kFenceSignalled));
  ASSERT_TRUE(IsEventSignalled(fence2, kFenceSignalled));
  ASSERT_TRUE(IsEventSignalled(fence3, kFenceSignalled));
}

}  // namespace test
}  // namespace scene
}  // namespace mozart
