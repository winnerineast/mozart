// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_MOZART_SERVICES_COMPOSITION_CPP_FORMATTING_H_
#define APPS_MOZART_SERVICES_COMPOSITION_CPP_FORMATTING_H_

#include "apps/mozart/services/composition/compositor.fidl.h"
#include "apps/mozart/services/composition/scheduling.fidl.h"
#include "apps/mozart/services/geometry/cpp/formatting.h"
#include "lib/fidl/cpp/bindings/formatting.h"

namespace mozart {

std::ostream& operator<<(std::ostream& os, const SceneToken& value);
std::ostream& operator<<(std::ostream& os, const SceneUpdate& value);
std::ostream& operator<<(std::ostream& os, const SceneMetadata& value);

std::ostream& operator<<(std::ostream& os, const Resource& value);
std::ostream& operator<<(std::ostream& os, const SceneResource& value);
std::ostream& operator<<(std::ostream& os, const ImageResource& value);

std::ostream& operator<<(std::ostream& os, const Image& value);
std::ostream& operator<<(std::ostream& os, const Image::PixelFormat* value);
std::ostream& operator<<(std::ostream& os, const Image::AlphaFormat* value);
std::ostream& operator<<(std::ostream& os, const Image::ColorSpace* value);

std::ostream& operator<<(std::ostream& os, const Node& value);
std::ostream& operator<<(std::ostream& os, const NodeOp& value);
std::ostream& operator<<(std::ostream& os, const RectNodeOp& value);
std::ostream& operator<<(std::ostream& os, const ImageNodeOp& value);
std::ostream& operator<<(std::ostream& os, const SceneNodeOp& value);
std::ostream& operator<<(std::ostream& os, const LayerNodeOp& value);

// FIXME(jeffbrown): Passing as a pointer to disambiguate with the operator<<
// generated by fidl for this enum, which unfortunately doesn't print names.
// We should improve fidl then get rid of this overload.
std::ostream& operator<<(std::ostream& os, const Node::Combinator* value);

std::ostream& operator<<(std::ostream& os, const Color& value);

std::ostream& operator<<(std::ostream& os, const Blend& value);

std::ostream& operator<<(std::ostream& os, const FrameInfo& value);

std::ostream& operator<<(std::ostream& os, const HitTestBehavior& value);
std::ostream& operator<<(std::ostream& os,
                         const HitTestBehavior::Visibility* value);
std::ostream& operator<<(std::ostream& os, const HitTestResult& value);
std::ostream& operator<<(std::ostream& os, const Hit& value);
std::ostream& operator<<(std::ostream& os, const SceneHit& value);
std::ostream& operator<<(std::ostream& os, const NodeHit& value);

}  // namespace mozart

#endif  // APPS_MOZART_SERVICES_COMPOSITION_CPP_FORMATTING_H_
