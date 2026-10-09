// Copyright 2026 Silkweave
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shuttle/avatar_cache.h"

#include <algorithm>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/shuttle/profile_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/data_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkSamplingOptions.h"
#include "url/gurl.h"

namespace shuttle {

namespace {

// Matches ProfileRegistry's avatar size limit.
constexpr size_t kMaxAvatarBytes = 256 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("shuttle_avatar", R"(
      semantics {
        sender: "Shuttle"
        description:
          "Fetches an agent's avatar image (an https: URL the user or an MCP "
          "client set) to show it on the agent's tabs."
        trigger: "An agent with an https: avatar has tabs in a window."
        data: "None; the request carries no cookies or credentials."
        destination: OTHER
      }
      policy {
        cookies_allowed: NO
        setting: "Remove the agent's avatar on chrome://agents."
        policy_exception_justification: "Not implemented."
      })");

// Center-crops `bitmap` to a square and draws it into a kSize circle.
gfx::ImageSkia MakeRound(const SkBitmap& bitmap) {
  constexpr int kSize = AvatarCache::kSize;
  SkBitmap round;
  round.allocN32Pixels(kSize, kSize);
  round.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas(round, SkSurfaceProps{});
  canvas.clipPath(SkPath::Circle(kSize / 2.f, kSize / 2.f, kSize / 2.f),
                  /*doAntiAlias=*/true);
  const float side = std::min(bitmap.width(), bitmap.height());
  canvas.drawImageRect(
      bitmap.asImage(),
      SkRect::MakeXYWH((bitmap.width() - side) / 2.f,
                       (bitmap.height() - side) / 2.f, side, side),
      SkRect::MakeWH(kSize, kSize),
      SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear),
      /*paint=*/nullptr, SkCanvas::kFast_SrcRectConstraint);
  round.setImmutable();
  return gfx::ImageSkia::CreateFrom1xBitmap(round);
}

}  // namespace

AvatarCache::Entry::Entry() = default;
AvatarCache::Entry::~Entry() = default;

AvatarCache::AvatarCache(
    Profile* profile,
    ProfileRegistry* registry,
    base::RepeatingCallback<void(const std::string& agent_id)> on_changed)
    : profile_(profile),
      registry_(registry),
      on_changed_(std::move(on_changed)) {}

AvatarCache::~AvatarCache() = default;

gfx::ImageSkia AvatarCache::Get(const std::string& agent_id) {
  std::optional<ProfileRegistry::Entry> agent = registry_->Get(agent_id);
  if (!agent || agent->avatar.empty()) {
    entries_.erase(agent_id);
    return gfx::ImageSkia();
  }
  auto it = entries_.find(agent_id);
  if (it == entries_.end() || it->second.source != agent->avatar) {
    Load(agent_id, agent->avatar);
    return gfx::ImageSkia();
  }
  return it->second.image;
}

void AvatarCache::Load(const std::string& agent_id,
                       const std::string& source) {
  Entry& entry = entries_[agent_id];
  entry.source = source;
  entry.image = gfx::ImageSkia();
  entry.loader.reset();

  GURL url(source);
  if (url.SchemeIs(url::kDataScheme)) {
    std::string mime_type, charset, data;
    if (net::DataURL::Parse(url, &mime_type, &charset, &data) &&
        base::StartsWith(mime_type, "image/")) {
      Decode(agent_id, source, data);
    }
    return;
  }
  if (!url.SchemeIs(url::kHttpsScheme)) {
    return;
  }
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  entry.loader = network::SimpleURLLoader::Create(std::move(request),
                                                  kTrafficAnnotation);
  entry.loader->DownloadToString(
      profile_->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&AvatarCache::OnFetched, weak_factory_.GetWeakPtr(),
                     agent_id, source),
      kMaxAvatarBytes);
}

void AvatarCache::OnFetched(const std::string& agent_id,
                            const std::string& source,
                            std::optional<std::string> body) {
  auto it = entries_.find(agent_id);
  if (it == entries_.end() || it->second.source != source) {
    return;
  }
  it->second.loader.reset();
  if (body) {
    Decode(agent_id, source, *body);
  }
}

void AvatarCache::Decode(const std::string& agent_id,
                         const std::string& source,
                         const std::string& bytes) {
  data_decoder::DecodeImageIsolated(
      base::as_byte_span(bytes), data_decoder::mojom::ImageCodec::kDefault,
      /*shrink_to_fit=*/true, data_decoder::kDefaultMaxSizeInBytes,
      gfx::Size(),
      base::BindOnce(&AvatarCache::OnDecoded, weak_factory_.GetWeakPtr(),
                     agent_id, source));
}

void AvatarCache::OnDecoded(const std::string& agent_id,
                            const std::string& source,
                            const SkBitmap& bitmap) {
  auto it = entries_.find(agent_id);
  if (it == entries_.end() || it->second.source != source ||
      bitmap.drawsNothing()) {
    return;
  }
  it->second.image = MakeRound(bitmap);
  on_changed_.Run(agent_id);
}

}  // namespace shuttle
