// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "nearby_sharing_service_factory.h"

#include <memory>
#include <utility>

#include "internal/analytics/event_logger.h"
#include "nearby_connections_manager_factory.h"
#include "nearby_sharing_decoder_impl.h"
#include "nearby_sharing_service.h"
#include "nearby_sharing_service_impl.h"

namespace nearby {
namespace sharing {


NearbySharingServiceFactory* NearbySharingServiceFactory::GetInstance() {
  static NearbySharingServiceFactory* instance =
      new NearbySharingServiceFactory();
  return instance;
}

NearbySharingService* NearbySharingServiceFactory::CreateSharingService() {
  if (nearby_sharing_service_ != nullptr) {
    return nullptr;
  }

  decoder_ = std::make_unique<NearbySharingDecoderImpl>();
  nearby_connections_manager_ =
      NearbyConnectionsManagerFactory::CreateConnectionsManager();

  nearby_sharing_service_ = std::make_unique<NearbySharingServiceImpl>(decoder_.get(),
      std::move(nearby_connections_manager_));

  return nearby_sharing_service_.get();
}

}  // namespace sharing
}  // namespace nearby
