// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/periodic_background_sync_permission_context.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/permission_prompt_decision.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#else
#endif

PeriodicBackgroundSyncPermissionContext::
    PeriodicBackgroundSyncPermissionContext(
        content::BrowserContext* browser_context)
    : ContentSettingPermissionContextBase(
          browser_context,
          ContentSettingsType::PERIODIC_BACKGROUND_SYNC,
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

PeriodicBackgroundSyncPermissionContext::
    ~PeriodicBackgroundSyncPermissionContext() = default;

bool PeriodicBackgroundSyncPermissionContext::IsPwaInstalled(
    const GURL& origin) const {
  // Because we're only passed the requesting origin from the permissions
  // infrastructure, we can't match the scope of installed PWAs to the exact URL
  // of the permission request. We instead look for any installed PWA for the
  // requesting origin. With this logic, if there's already a PWA installed for
  // google.com/travel, and a request to register Periodic Background Sync comes
  // in from google.com/maps, this method will return true and registration will
  // succeed, provided other required conditions are met.
  // Web apps have been removed, so no PWA is ever installed.
  return false;
}

#if BUILDFLAG(IS_ANDROID)
bool PeriodicBackgroundSyncPermissionContext::IsTwaInstalled(
    const GURL& origin) const {
  return ShortcutHelper::DoesOriginContainAnyInstalledTrustedWebActivity(
      origin);
}
#endif

ContentSetting
PeriodicBackgroundSyncPermissionContext::GetContentSettingStatusInternal(
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const GURL& embedding_origin) const {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);

// TODO(crbug.com/397357113): PermissionStatus `change` event not triggered
// when TWA or PWA is installed or uninstalled on Android.
#if BUILDFLAG(IS_ANDROID)
  if (IsTwaInstalled(requesting_origin))
    return CONTENT_SETTING_ALLOW;
#endif

  if (!IsPwaInstalled(requesting_origin)) {
    return CONTENT_SETTING_BLOCK;
  }

  // PWA installed. Check for one-shot Background Sync content setting.
  // Expected values are CONTENT_SETTING_BLOCK or CONTENT_SETTING_ALLOW.
  auto* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser_context());
  CHECK(host_content_settings_map, base::NotFatalUntil::M161);

  auto content_setting = host_content_settings_map->GetContentSetting(
      requesting_origin, embedding_origin,
      ContentSettingsType::BACKGROUND_SYNC);
  // TODO(crbug.com/584978616): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(content_setting == CONTENT_SETTING_BLOCK ||
         content_setting == CONTENT_SETTING_ALLOW);
  return content_setting;
}

void PeriodicBackgroundSyncPermissionContext::DecidePermission(
    std::unique_ptr<permissions::PermissionRequestData> request_data,
    permissions::BrowserPermissionCallback callback) {
  // The user should never be prompted to authorize Periodic Background Sync
  // from PeriodicBackgroundSyncPermissionContext.
  NOTREACHED();
}

void PeriodicBackgroundSyncPermissionContext::NotifyPermissionSet(
    const permissions::PermissionRequestData& request_data,
    permissions::BrowserPermissionCallback callback,
    bool persist,
    const content::PermissionResult* permission_result,
    const permissions::PermissionPromptDecision& decision) {
  CHECK(!persist, base::NotFatalUntil::M161);
  CHECK(decision.is_final, base::NotFatalUntil::M161);

  permissions::ContentSettingPermissionContextBase::NotifyPermissionSet(
      request_data, std::move(callback), persist, permission_result, decision);
}

void PeriodicBackgroundSyncPermissionContext::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (!content_type_set.Contains(ContentSettingsType::BACKGROUND_SYNC) &&
      !content_type_set.Contains(
          ContentSettingsType::PERIODIC_BACKGROUND_SYNC)) {
    return;
  }
  permissions::ContentSettingPermissionContextBase::OnContentSettingChanged(
      primary_pattern, secondary_pattern,
      ContentSettingsTypeSet(ContentSettingsType::PERIODIC_BACKGROUND_SYNC));
}
