// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This class is used to construct and hold tab-scoped state associated with a
// WebContents. When a WebContents is transformed into a tab, and instance of
// this class is created. When the tab is destroyed, this instance is destroyed.
//
// This class exists for 3 reasons:
//  (1) It provides explicit construction and destruction ordering.
//  (2) It allows for dependency-injection at construction time of tab features.
//  (3) It pairs with the UnownedUserData design pattern to ensure dependencies
//      are precisely specified by BUILD.gn files. This prevents circular
//      dependencies.
//
// If you want to make a new TabFeature, following these steps:
//  (1) Make a regular C++ class. It should NOT inherit from SupportsUserData.
//  (2) Forward declare the class, and add a std::unique_ptr member to this
//      header file.
//  (3) Construct the member in tab_features.cc.
//  (4) If tab-consumers need to access the feature, expose it via TabInterface
//      and UnownedUserData.
//
// For more details on UnownedUserData, see ui/base/unowned_user_data/README.md.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/buildflags.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

class AskBeforeHttpDialogController;
class CommitLimitOOMRecoveryTracker;
class ConnectionHelpTabHelper;
class CookieControlsPageActionController;
class FileSystemAccessPageActionController;
class FocusTabAfterNavigationHelper;
class FramebustBlockTabHelper;

class FormInteractionTabHelper;
class FromGWSNavigationAndKeepAliveRequestObserver;
class HttpAuthCacheStatus;
class JsOptimizationsPageActionController;
class Profile;
class QwacWebContentsObserver;
class ReadAnythingController;
class ReadAnythingSidePanelController;
class SadTabHelper;
class SearchPromotionNavigationObserver;
class SecurityStateEventObserver;
class SidePanelRegistry;
class TabUIHelper;
class ThumbnailTabHelper;
class ZeroSuggestPrefetchTabHelper;

namespace back_to_opener {
class BackToOpenerController;
}  // namespace back_to_opener


namespace actor {
class ActorTabData;
}  // namespace actor

namespace actor::ui {
class ActorUiTabControllerInterface;
}  // namespace actor::ui

namespace enterprise_data_protection {
class DataProtectionNavigationController;
}  // namespace enterprise_data_protection

namespace enterprise_reporting {
class SaasUsageNavigationObserver;
}  // namespace enterprise_reporting

namespace content {
class WebContents;
}  // namespace content

namespace extensions {
class ExtensionSidePanelManager;
}  // namespace extensions

namespace zoom {
class ZoomViewController;
}  // namespace zoom

namespace permissions {
class PermissionIndicatorsTabData;
}  // namespace permissions

namespace page_actions {
class PageActionController;
}  // namespace page_actions

namespace multistep_filter {
class ChromeFilterNavigationObserver;
class FilterUiController;
}  // namespace multistep_filter

namespace tabs {

class ContextHighlightTabFeature;
class InactiveWindowMouseEventController;
class PageContextEligibilityHelper;
class TabAlertController;
class TabAttachmentTracker;
class TabCreationMetricsController;
class TabDialogManager;
class TabInterface;

// This class owns the core controllers for features that are scoped to a given
// tab. It can be subclassed by tests to perform dependency injection.
//
// Do not add more public accessors. Instead use the UnownedUserData design
// pattern, see ui/base/unowned_user_data/README.md.
// TODO(crbug.com/481268779): Remove existing public accessors.
class TabFeatures {
 public:
  TabFeatures();
  ~TabFeatures();

  TabFeatures(const TabFeatures&) = delete;
  TabFeatures& operator=(const TabFeatures&) = delete;

  enterprise_data_protection::DataProtectionNavigationController*
  data_protection_controller() {
    return data_protection_tab_controller_.get();
  }

  permissions::PermissionIndicatorsTabData* permission_indicators_tab_data() {
    return permission_indicators_tab_data_.get();
  }

  extensions::ExtensionSidePanelManager* extension_side_panel_manager() {
    return extension_side_panel_manager_.get();
  }

  TabDialogManager* tab_dialog_manager() { return tab_dialog_manager_.get(); }

  page_actions::PageActionController* page_action_controller() {
    return page_action_controller_.get();
  }

  JsOptimizationsPageActionController*
  js_optimizations_page_action_controller() {
    return js_optimizations_page_action_controller_.get();
  }

  FileSystemAccessPageActionController*
  file_system_access_page_action_controller() {
    return file_system_access_page_action_controller_.get();
  }

  zoom::ZoomViewController* zoom_view_controller() {
    return zoom_view_controller_.get();
  }

  InactiveWindowMouseEventController* inactive_window_mouse_event_controller() {
    return inactive_window_mouse_event_controller_.get();
  }

  TabUIHelper* SetTabUIHelperForTesting(
      std::unique_ptr<TabUIHelper> tab_ui_helper);

  TabAlertController* SetTabAlertControllerForTesting(
      std::unique_ptr<TabAlertController> tab_alert_controller);

  TabCreationMetricsController* tab_creation_metrics_controller() {
    return tab_creation_metrics_controller_.get();
  }

  AskBeforeHttpDialogController* ask_before_http_dialog_controller() {
    return ask_before_http_dialog_controller_.get();
  }

  back_to_opener::BackToOpenerController* back_to_opener_controller() {
    return back_to_opener_controller_.get();
  }

  // Called exactly once to initialize features.
  void Init(TabInterface& tab, Profile* profile);

  static ui::UserDataFactoryWithOwner<TabInterface>&
  GetUserDataFactoryForTesting();

 private:
  bool initialized_ = false;

  // Returns the factory used to create owned components.
  static ui::UserDataFactoryWithOwner<TabInterface>& GetUserDataFactory();

  // TODO(https://crbug.com/347770670): Delete this code when tab-discarding no
  // longer swizzles WebContents.
  // Called when the tab's WebContents is discarded.
  void WillDiscardContents(tabs::TabInterface* tab,
                           content::WebContents* old_contents,
                           content::WebContents* new_contents);

  std::unique_ptr<permissions::PermissionIndicatorsTabData>
      permission_indicators_tab_data_;

  std::unique_ptr<SidePanelRegistry> side_panel_registry_;

  // Responsible for managing the read anything (Reading mode) feature.
  std::unique_ptr<ReadAnythingController> read_anything_controller_;

  // The tab-scoped extension side-panel manager. There is a separate
  // window-scoped extension side-panel manager.
  std::unique_ptr<extensions::ExtensionSidePanelManager>
      extension_side_panel_manager_;

  // Security-state-driven side effects (known-interception disclosure,
  // form-submission UKM).
  std::unique_ptr<SecurityStateEventObserver> security_state_event_observer_;

  // Manages various tab modal dialogs.
  std::unique_ptr<TabDialogManager> tab_dialog_manager_;

  std::unique_ptr<
      enterprise_data_protection::DataProtectionNavigationController>
      data_protection_tab_controller_;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Responsible for managing the "File System Access" page action.
  std::unique_ptr<FileSystemAccessPageActionController>
      file_system_access_page_action_controller_;

  // Responsible for managing all page actions of a tab. Other controllers
  // interact with this to have their feature's page action shown.
  std::unique_ptr<page_actions::PageActionController> page_action_controller_;

  // Responsible for managing the "Zoom" page action and bubble.
  std::unique_ptr<zoom::ZoomViewController> zoom_view_controller_;

  // Responsible for managing the "JS Optimizations" page action.
  std::unique_ptr<JsOptimizationsPageActionController>
      js_optimizations_page_action_controller_;

  // Manages the Cookie Controls page action.
  std::unique_ptr<CookieControlsPageActionController>
      cookie_controls_page_action_controller_;

  std::unique_ptr<InactiveWindowMouseEventController>
      inactive_window_mouse_event_controller_;

  // Focuses the tab contents after browser-initiated and NTP-leaving
  // navigations.
  std::unique_ptr<FocusTabAfterNavigationHelper>
      focus_tab_after_navigation_helper_;

  // Tracks blocked framebusts on the current page for the omnibox UI.
  std::unique_ptr<FramebustBlockTabHelper> framebust_block_tab_helper_;

  // Redirects cert-error loads of the help center to bundled help content.
  std::unique_ptr<ConnectionHelpTabHelper> connection_help_tab_helper_;

  // Indicates if the tab contains forms that have been interacted with.
  std::unique_ptr<FormInteractionTabHelper> form_interaction_tab_helper_;

  std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver>
      from_gws_navigation_and_keep_alive_request_observer_;

  // Records use counters for cross-partition subresource loads that used
  // server HTTP auth.
  std::unique_ptr<HttpAuthCacheStatus> http_auth_cache_status_;

  std::unique_ptr<TabAlertController> tab_alert_controller_;

  std::unique_ptr<ContextHighlightTabFeature> context_highlight_tab_feature_;

  std::unique_ptr<TabUIHelper> tab_ui_helper_;

  std::unique_ptr<QwacWebContentsObserver> qwac_web_contents_observer_;

  std::unique_ptr<actor::ui::ActorUiTabControllerInterface>
      actor_ui_tab_controller_;

  std::unique_ptr<TabCreationMetricsController>
      tab_creation_metrics_controller_;

  std::unique_ptr<AskBeforeHttpDialogController>
      ask_before_http_dialog_controller_;

  std::unique_ptr<actor::ActorTabData> actor_tab_data_;

  // Manages the sad tab view shown when the tab's main frame has crashed.
  // Null when the WebUI browser is enabled.
  std::unique_ptr<SadTabHelper> sad_tab_helper_;

  std::unique_ptr<back_to_opener::BackToOpenerController>
      back_to_opener_controller_;

  std::unique_ptr<tabs::PageContextEligibilityHelper>
      page_context_eligibility_helper_;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<enterprise_reporting::SaasUsageNavigationObserver>
      saas_usage_navigation_observer_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
  std::unique_ptr<SearchPromotionNavigationObserver>
      search_promotion_navigation_observer_;
  std::unique_ptr<CommitLimitOOMRecoveryTracker>
      commit_limit_oom_recovery_tracker_;
#endif

  std::unique_ptr<multistep_filter::FilterUiController> filter_ui_controller_;
  std::unique_ptr<multistep_filter::ChromeFilterNavigationObserver>
      filter_navigation_observer_;

  std::unique_ptr<TabAttachmentTracker> tab_attachment_tracker_;

  // Prefetches zero-prefix suggestions on opening or switching to an NTP.
  std::unique_ptr<ZeroSuggestPrefetchTabHelper>
      zero_suggest_prefetch_tab_helper_;

  // Maintains the thumbnail shown in e.g. tab hover cards. Null when no
  // feature that needs thumbnails is enabled.
  std::unique_ptr<ThumbnailTabHelper> thumbnail_tab_helper_;

  // Must be the last member.
  base::WeakPtrFactory<TabFeatures> weak_factory_{this};
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_FEATURES_H_
