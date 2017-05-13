// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <memory>

// TODO: why do we need to include this first?
// It is included by "escher/geometry/types.h" ?!?
#include <glm/glm.hpp>

#include "application/lib/app/application_context.h"
#include "apps/mozart/services/composer/composer.fidl.h"
#include "apps/mozart/src/composer/composer_impl.h"
#include "escher/escher.h"
#include "escher/escher_process_init.h"
#include "escher/geometry/types.h"
#include "escher/material/material.h"
#include "escher/renderer/paper_renderer.h"
#include "escher/scene/model.h"
#include "escher/scene/stage.h"
#include "escher/vk/vulkan_swapchain_helper.h"
#include "lib/escher/examples/common/demo.h"
#include "lib/escher/examples/common/demo_harness_fuchsia.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/command_line.h"
#include "lib/ftl/log_settings.h"
#include "lib/ftl/logging.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"

static constexpr uint32_t kScreenWidth = 2160;
static constexpr uint32_t kScreenHeight = 1440;

// Material design places objects from 0.0f to 24.0f.
static constexpr float kNear = 24.f;
static constexpr float kFar = 0.f;

namespace mozart {
namespace composer {

class ComposerImpl;

class HelloComposerService : public Demo {
 public:
  explicit HelloComposerService(DemoHarnessFuchsia* harness)
      : Demo(harness),
        application_context_(harness->application_context()),
        renderer_(escher()->NewPaperRenderer()),
        swapchain_helper_(harness->GetVulkanSwapchain(), renderer_),
        composer_(std::make_unique<ComposerImpl>()),
        binding_(composer_.get()) {
    FTL_DCHECK(application_context_);

    AddOutgoingServices();
    InitializeEscherStage();
  }

  ~HelloComposerService() {
    FTL_LOG(INFO) << "HelloComposerService: shutting down";
  }

  void DrawFrame() override {
    size_t session_count = composer_->GetSessionCount();
    if (session_count == last_session_count_) {
      return;
    }
    last_session_count_ = session_count;

    // Create a grid of circles, where each circle represents a Session in
    // the Composer.
    std::vector<escher::Object> objects;
    size_t grid_size =
        static_cast<size_t>(ceil(sqrt(static_cast<double>(session_count))));
    float grid_cell_width = static_cast<float>(kScreenWidth) / grid_size;
    float grid_cell_height = static_cast<float>(kScreenHeight) / grid_size;

    auto background_material = ftl::MakeRefCounted<escher::Material>();
    background_material->set_color(escher::vec3(0.8f, 0.8f, 0.8f));
    objects.push_back(escher::Object::NewRect(
        escher::vec2(0.f, 0.f), escher::vec2(kScreenWidth, kScreenHeight), 0.f,
        background_material));

    auto circle_material = ftl::MakeRefCounted<escher::Material>();
    circle_material->set_color(
        escher::vec3(63.f / 255.f, 138.f / 255.f, 153.f / 255.f));

    for (size_t y = 0; y < grid_size; ++y) {
      for (size_t x = 0; x < grid_size; ++x) {
        if (session_count > y * grid_size + x) {
          escher::vec2 center(
              (0.5f + static_cast<float>(x)) * grid_cell_width,
              (0.5f + static_cast<float>(y)) * grid_cell_height);
          objects.push_back(escher::Object::NewCircle(
              center, grid_cell_height * 0.4f, 10.f, circle_material));
        }
      }
    }

    escher::Model model(std::move(objects));
    swapchain_helper_.DrawFrame(stage_, model);
  }

 private:
  void AddOutgoingServices() {
    application_context_->outgoing_services()->AddService<mozart2::Composer>(
        [this](fidl::InterfaceRequest<mozart2::Composer> request) {
          if (binding_.is_bound()) {
            FTL_LOG(WARNING)
                << "HelloComposerService: composer already bound to client";
          } else {
            FTL_LOG(INFO) << "HelloComposerService: binding client to composer";
            binding_.Bind(std::move(request));
            binding_.set_connection_error_handler([this] {
              FTL_LOG(INFO)
                  << "HelloComposerService: connection to client was closed";
              composer_.reset();
              mtl::MessageLoop::GetCurrent()->QuitNow();
            });
          }
        });
  }

  void InitializeEscherStage() {
    stage_.Resize(escher::SizeI(kScreenWidth, kScreenHeight), 1.0,
                  escher::SizeI(0, 0));
    stage_.set_viewing_volume(
        escher::ViewingVolume(kScreenWidth, kScreenHeight, kNear, kFar));
    stage_.set_key_light(escher::DirectionalLight(
        escher::vec2(1.5f * M_PI, 1.5f * M_PI), 0.15f * M_PI, 0.7f));
    stage_.set_fill_light(escher::AmbientLight(0.3f));
  }

  app::ApplicationContext* const application_context_;
  escher::PaperRendererPtr renderer_;
  escher::VulkanSwapchainHelper swapchain_helper_;
  escher::Stage stage_;
  std::unique_ptr<ComposerImpl> composer_;
  fidl::Binding<mozart2::Composer> binding_;
  size_t last_session_count_ = 987654321;

  FTL_DISALLOW_COPY_AND_ASSIGN(HelloComposerService);
};

}  // namespace composer
}  // namespace mozart

int main(int argc, const char** argv) {
  FTL_LOG(INFO) << "HelloComposerService: entering main()";

  auto command_line = ftl::CommandLineFromArgcArgv(argc, argv);
  if (!ftl::SetLogSettingsFromCommandLine(command_line))
    return 1;

  auto harness = DemoHarness::New(
      DemoHarness::WindowParams{"Mozart Composer Example", kScreenWidth,
                                kScreenHeight, 2, false},
      DemoHarness::InstanceParams());

  {
    mozart::composer::HelloComposerService composer_app(
        static_cast<DemoHarnessFuchsia*>(harness.get()));
    harness->Run(&composer_app);
  }
  harness->Shutdown();
  return 0;
}