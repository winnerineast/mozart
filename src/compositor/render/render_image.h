// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MOZART_SRC_COMPOSITOR_RENDER_RENDER_IMAGE_H_
#define APPS_MOZART_SRC_COMPOSITOR_RENDER_RENDER_IMAGE_H_

#include "apps/mozart/services/buffers/cpp/buffer_consumer.h"
#include "apps/mozart/services/composition/resources.fidl.h"
#include "lib/ftl/macros.h"
#include "lib/ftl/memory/ref_counted.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace compositor {

// Describes an image which can be rendered by the compositor.
//
// Render objects are thread-safe, immutable, and reference counted.
// They have no direct references to the scene graph.
class RenderImage : public ftl::RefCountedThreadSafe<RenderImage> {
 public:
  RenderImage(const sk_sp<SkImage>& image,
              std::unique_ptr<mozart::BufferFence> fence);

  // Creates a new image backed by a shared memory buffer.
  // Returns nullptr if the image is invalid.
  static ftl::RefPtr<RenderImage> CreateFromImage(
      mozart::ImagePtr image,
      mozart::BufferConsumer* consumer);

  uint32_t width() const { return image_->width(); }
  uint32_t height() const { return image_->height(); }

  // Gets the underlying image to rasterize, never null.
  const sk_sp<SkImage>& image() const { return image_; }

  // Gets the image buffer's fence, or null if none.
  mozart::BufferFence* fence() const { return fence_.get(); }

 private:
  FRIEND_REF_COUNTED_THREAD_SAFE(RenderImage);

  ~RenderImage();

  sk_sp<SkImage> image_;
  std::unique_ptr<mozart::BufferFence> fence_;

  FTL_DISALLOW_COPY_AND_ASSIGN(RenderImage);
};

}  // namespace compositor

#endif  // APPS_MOZART_SRC_COMPOSITOR_RENDER_RENDER_IMAGE_H_
