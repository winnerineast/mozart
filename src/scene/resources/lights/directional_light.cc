// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/mozart/src/scene/resources/lights/directional_light.h"

namespace mozart {
namespace scene {

const ResourceTypeInfo DirectionalLight::kTypeInfo = {
    ResourceType::kDirectionalLight, "DirectionalLight"};

DirectionalLight::DirectionalLight(Session* session,
                                   ResourceId id,
                                   const escher::vec3& direction,
                                   float intensity)
    : Resource(session, DirectionalLight::kTypeInfo),
      direction_(direction),
      intensity_(intensity) {}

}  // namespace scene
}  // namespace mozart
