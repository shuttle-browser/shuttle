// Copyright 2026 The Agent Chrome Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AGENT_CHROME_AVATAR_CACHE_H_
#define CHROME_BROWSER_AGENT_CHROME_AVATAR_CACHE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

class Profile;
class SkBitmap;

namespace network {
class SimpleURLLoader;
}

namespace agent_chrome {

class ProfileRegistry;

// Agents' avatars as round images for the tab strip (the agent's tab group
// chip). Avatars are data:image/ or https: URLs; https ones are fetched
// without credentials, and every image is decoded in the sandboxed data
// decoder, never in the browser process.
class AvatarCache {
 public:
  // `on_changed` runs when an agent's image becomes available.
  AvatarCache(Profile* profile,
              ProfileRegistry* registry,
              base::RepeatingCallback<void(const std::string& agent_id)>
                  on_changed);
  AvatarCache(const AvatarCache&) = delete;
  AvatarCache& operator=(const AvatarCache&) = delete;
  ~AvatarCache();

  // The agent's avatar (kSize x kSize, round), or an empty image if it has
  // none or it is still loading (`on_changed` follows).
  gfx::ImageSkia Get(const std::string& agent_id);

  static constexpr int kSize = 48;

 private:
  struct Entry {
    Entry();
    ~Entry();
    std::string source;
    gfx::ImageSkia image;
    std::unique_ptr<network::SimpleURLLoader> loader;
  };

  void Load(const std::string& agent_id, const std::string& source);
  void OnFetched(const std::string& agent_id,
                 const std::string& source,
                 std::optional<std::string> body);
  void Decode(const std::string& agent_id,
              const std::string& source,
              const std::string& bytes);
  void OnDecoded(const std::string& agent_id,
                 const std::string& source,
                 const SkBitmap& bitmap);

  raw_ptr<Profile> profile_;
  raw_ptr<ProfileRegistry> registry_;
  base::RepeatingCallback<void(const std::string&)> on_changed_;
  std::map<std::string, Entry> entries_;
  base::WeakPtrFactory<AvatarCache> weak_factory_{this};
};

}  // namespace agent_chrome

#endif  // CHROME_BROWSER_AGENT_CHROME_AVATAR_CACHE_H_
