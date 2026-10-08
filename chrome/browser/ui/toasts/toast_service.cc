// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toasts/toast_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/api/toast_registry.h"
#include "chrome/browser/ui/toasts/api/toast_specification.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/base_window.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/strings/grit/ui_strings.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/analysis/copy_warning_delegate_tracker.h"
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

DEFINE_USER_DATA(ToastService);

// static
ToastService* ToastService::From(BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

ToastService::ToastService(BrowserWindowInterface* browser_window_interface)
    : scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  toast_registry_ = std::make_unique<ToastRegistry>();
  toast_controller_ = std::make_unique<ToastController>(
      browser_window_interface, toast_registry_.get());
  toast_controller_->Init();
  RegisterToasts(browser_window_interface);
}

ToastService::~ToastService() = default;

void ToastService::RegisterToasts(
    BrowserWindowInterface* browser_window_interface) {
  CHECK(toast_registry_->IsEmpty());

  toast_registry_->RegisterToast(
      ToastId::kLinkCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kLinkIcon
                                      : kLinkChromeRefreshOldIcon,
                                  IDS_LINK_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kImageCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kContentCopyIcon
                                      : kCopyMenuOldIcon,
                                  IDS_IMAGE_COPIED_TOAST_BODY)
          .Build());
  toast_registry_->RegisterToast(
      ToastId::kVideoFrameCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kContentCopyIcon
                                      : kCopyMenuOldIcon,
                                  IDS_VIDEO_FRAME_COPIED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kLinkToHighlightCopied,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kLinkIcon
                                      : kLinkChromeRefreshOldIcon,
                                  IDS_LINK_COPIED_TO_HIGHLIGHT_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kClearBrowsingData,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kDeleteIcon
                                      : kTrashCanRefreshOldIcon,
                                  IDS_CLEAR_BROWSING_DATA_TOAST_BODY)
          .Build());

  // TODO(crbug.com/357930023): This registration only partially implements the
  // non-milestone update toast for testing purposes and will need to be
  // updated.
  toast_registry_->RegisterToast(
      ToastId::kNonMilestoneUpdate,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? kChromeProductIcon
                                      : kBrowserLogoOldIcon,
                                  IDS_NON_MILESTONE_UPDATE_TOAST_BODY)
          .AddGlobalScoped()
          .Build());

  // ESB as a synced setting.
  if (base::FeatureList::IsEnabled(safe_browsing::kEsbAsASyncedSetting)) {
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOn,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            features::IsRoundedIconsEnabled() ? kSecurityIcon
                                              : kSecurityOldIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SETTINGS,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      window->OpenGURL(
                          chrome::GetSettingsUrl(
                              chrome::kSafeBrowsingEnhancedProtectionSubPage),
                          WindowOpenDisposition::NEW_FOREGROUND_TAB);
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOnWithoutActionButton,
        ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            vector_icons::kGshieldIcon,
#else
            features::IsRoundedIconsEnabled() ? kSecurityIcon
                                              : kSecurityOldIcon,
#endif
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_ON_TOAST_MESSAGE)
            .Build());
    toast_registry_->RegisterToast(
        ToastId::kSyncEsbOff,
        ToastSpecification::Builder(
            features::IsRoundedIconsEnabled() ? kInfoIcon : kInfoOldIcon,
            IDS_SETTINGS_SAFEBROWSING_ENHANCED_OFF_TOAST_MESSAGE)
            .AddActionButton(
                IDS_SETTINGS_SAFEBROWSING_TURN_ON_ENHANCED_TOAST_BUTTON,
                base::BindRepeating(
                    [](BrowserWindowInterface* window) {
                      Profile* profile = window->GetProfile();
                      if (profile) {
                        profile->GetPrefs()->SetBoolean(
                            prefs::kSafeBrowsingEnhanced, true);
                      }
                    },
                    base::Unretained(browser_window_interface)))
            .AddCloseButton()
            .Build());
  }

  toast_registry_->RegisterToast(
      ToastId::kClosePinnedTab,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kKeepIcon : kKeepOldIcon,
          IDS_CLOSE_PINNED_TAB_TOAST_BODY)
          .SetToastAsActionable()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kDiceUserMigrated,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kCelebrationIcon
                                      : vector_icons::kCelebrationOldIcon,
                                  IDS_DICE_MIGRATION_CONFIRMATION_TOAST_MESSAGE)
          .AddCloseButton()
          .AddActionButton(IDS_DICE_MIGRATION_CONFIRMATION_TOAST_BUTTON,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 chrome::ShowSettingsSubPageForProfile(
                                     window->GetProfile(),
                                     chrome::kSyncSetupSubPage);
                               },
                               base::Unretained(browser_window_interface)))
          .AddGlobalScoped()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kEmailVerified,
      ToastSpecification::Builder(features::IsRoundedIconsEnabled()
                                      ? vector_icons::kMarkEmailReadIcon
                                      : vector_icons::kMarkEmailReadOldIcon,
                                  IDS_EMAIL_VERIFIED)
          .SetPersistOnNavigation()
          .AddMenu()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kCopiedToClipboard,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kInfoIcon : kInfoOldIcon,
          IDS_COPIED_TO_CLIPBOARD_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kEnhancedBundledSecuritySettings,
      ToastSpecification::Builder(
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          vector_icons::kGshieldIcon,
#else
          features::IsRoundedIconsEnabled() ? kSecurityIcon : kSecurityOldIcon,
#endif
          IDS_SETTINGS_SECURITY_BUNDLE_TOAST_FOR_USER_OPTED_INTO_ENHANCED_BUNDLE)
          .AddActionButton(
              IDS_SETTINGS_SETTINGS,
              base::BindRepeating(
                  [](BrowserWindowInterface* window) {
                    window->OpenGURL(
                        chrome::GetSettingsUrl(chrome::kSecuritySubPage),
                        WindowOpenDisposition::NEW_FOREGROUND_TAB);
                  },
                  base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .AddGlobalScoped()
          .Build());

  // Report a scam confirmation toast.
  toast_registry_->RegisterToast(
      ToastId::kReportUnsafeSiteConfirmation,
      ToastSpecification::Builder(
          features::IsRoundedIconsEnabled() ? kCheckSmallIcon : kCheckOldIcon,
          IDS_REPORT_UNSAFE_SITE_CONFIRMATION_TOAST)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kTabStripSwitchDelayedHorizontal,
      ToastSpecification::Builder(
          kFullscreenIcon,
          IDS_TAB_STRIP_SWITCH_DELAYED_TO_HORIZONTAL_TOAST_BODY)
          .AddActionButton(IDS_TAB_STRIP_SWITCH_DELAYED_EXIT_FULLSCREEN_ACTION,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 if (window->GetWindow()->IsFullscreen()) {
                                   chrome::ToggleFullscreenMode(
                                       window, /*user_initiated=*/true);
                                 }
                               },
                               base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .AddGlobalScoped()
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kTabStripSwitchDelayedVertical,
      ToastSpecification::Builder(
          kFullscreenIcon, IDS_TAB_STRIP_SWITCH_DELAYED_TO_VERTICAL_TOAST_BODY)
          .AddActionButton(IDS_TAB_STRIP_SWITCH_DELAYED_EXIT_FULLSCREEN_ACTION,
                           base::BindRepeating(
                               [](BrowserWindowInterface* window) {
                                 if (window->GetWindow()->IsFullscreen()) {
                                   chrome::ToggleFullscreenMode(
                                       window, /*user_initiated=*/true);
                                 }
                               },
                               base::Unretained(browser_window_interface)))
          .AddCloseButton()
          .AddGlobalScoped()
          .Build());


#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  toast_registry_->RegisterToast(
      ToastId::kEnterpriseCopyAudit,
      ToastSpecification::Builder(vector_icons::kDomainIcon,
                                  IDS_ENTERPRISE_COPY_MONITORED_TOAST_BODY)
          .Build());
  toast_registry_->RegisterToast(
      ToastId::kEnterpriseCopyKeptInManagedChrome,
      ToastSpecification::Builder(vector_icons::kDomainIcon,
                                  IDS_ENTERPRISE_COPY_MONITORED_TOAST_BODY)
          .Build());

  toast_registry_->RegisterToast(
      ToastId::kEnterpriseCopyWarning,
      ToastSpecification::Builder(vector_icons::kDomainIcon,
                                  IDS_ENTERPRISE_COPY_WARNING_TOAST_BODY)
          .AddCloseButton()
          .AddActionButton(
              IDS_ENTERPRISE_COPY_WARNING_TOAST_BUTTON,
              base::BindRepeating(
                  [](BrowserWindowInterface* window) {
                    if (auto* tab = window->GetActiveTabInterface()) {
                      if (auto* web_contents = tab->GetContents()) {
                        enterprise_connectors::CopyWarningDelegateTracker::
                            BypassAndClear(web_contents);
                      }
                    }
                  },
                  base::Unretained(browser_window_interface)))
          .Build());
  toast_registry_->RegisterToast(
      ToastId::kEnterpriseCopyBlocked,
      ToastSpecification::Builder(vector_icons::kDomainIcon,
                                  IDS_ENTERPRISE_COPY_BLOCKED_TOAST_BODY)
          .Build());
#endif
}  // RegisterToasts() end.
