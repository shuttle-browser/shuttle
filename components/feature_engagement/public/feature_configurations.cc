// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/group_constants.h"
#include "components/segmentation_platform/public/features.h"

#if BUILDFLAG(IS_IOS)
#include "base/metrics/field_trial_params.h"
#include "components/feature_engagement/public/ios_promo_feature_configuration.h"
#endif  // BUILDFLAG(IS_IOS)

namespace {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
const int k10YearsInDays = 365 * 10;
#endif
}  // namespace

namespace feature_engagement {

FeatureConfig CreateAlwaysTriggerConfig(const base::Feature* feature) {
  // Trim "IPH_" prefix from the feature name to use for trigger and used
  // events.
  const char* prefix = "IPH_";
  std::string stripped_feature_name = feature->name;
  if (base::StartsWith(stripped_feature_name, prefix,
                       base::CompareCase::SENSITIVE)) {
    stripped_feature_name = stripped_feature_name.substr(strlen(prefix));
  }

  // A config that always meets condition to trigger IPH.
  FeatureConfig config;
  config.valid = true;
  config.availability = Comparator(ANY, 0);
  config.session_rate = Comparator(ANY, 0);
  config.trigger = EventConfig(stripped_feature_name + "_trigger",
                               Comparator(ANY, 0), 90, 90);
  config.used =
      EventConfig(stripped_feature_name + "_used", Comparator(ANY, 0), 90, 90);
  return config;
}

#if BUILDFLAG(IS_IOS)
std::optional<FeatureConfig> CreateNewUserGestureInProductHelpConfig(
    const base::Feature& feature,
    const char* action_event,
    const char* trigger_event,
    const char* used_event,
    const char* dismiss_button_tap_event,
    std::optional<uint32_t> first_run_recency_in_days = std::nullopt) {
  // Maximum storage days for iOS gesture IPHs in days. Note that they only
  // triggered for users who installed Chrome on iOS in the last specific number
  // of days, so this could be used as the maximum storage period of respective
  // events.
  const int kTotalMaxOccurrences = 2;
  const uint32_t kMaxStorageDays = 61;
  // The IPH only shows once a week, and honors `kTotalMaxOccurrences`.
  const int kDaysBetweenOccurrences = 7;

  FeatureConfig config;
  config.valid = true;
  config.availability = Comparator(ANY, 0);
  config.session_rate = Comparator(EQUAL, 0);
  // The user hasn't done the action suggested by the IPH.
  config.used = EventConfig(used_event, Comparator(EQUAL, 0), kMaxStorageDays,
                            kMaxStorageDays);
  // The IPH shows at most once per `kDaysBetweenOccurrences`.
  config.trigger =
      EventConfig(trigger_event, Comparator(EQUAL, 0), kDaysBetweenOccurrences,
                  kDaysBetweenOccurrences);
  config.event_configs.insert(
      EventConfig(trigger_event, Comparator(LESS_THAN, kTotalMaxOccurrences),
                  kMaxStorageDays, kMaxStorageDays));
  // The IPH only shows when user performs the action that should trigger the
  // IPH at least twice since the last time the IPH shows, or since installation
  // if it hasn't.
  config.event_configs.insert(
      EventConfig(action_event, Comparator(GREATER_THAN_OR_EQUAL, 2),
                  kDaysBetweenOccurrences, kDaysBetweenOccurrences));
  // The user hasn't explicitly dismissed the same IPH before.
  config.event_configs.insert(EventConfig(dismiss_button_tap_event,
                                          Comparator(EQUAL, 0), kMaxStorageDays,
                                          kMaxStorageDays));
  if (first_run_recency_in_days.has_value()) {
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kIOSFirstRunComplete,
                    Comparator(GREATER_THAN_OR_EQUAL, 1),
                    first_run_recency_in_days.value(), 360));
  }

  return config;
}
#endif

std::optional<FeatureConfig> GetClientSideFeatureConfig(
    const base::Feature* feature) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (kIPHProfileSwitchFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the profile menu was not opened.
    config.trigger =
        EventConfig("profile_switch_trigger", Comparator(EQUAL, 0), 360, 360);
    config.used =
        EventConfig("profile_menu_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingListInSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the side panel was not opened.
    config.trigger =
        EventConfig("side_panel_trigger", Comparator(EQUAL, 0), 360, 360);
    config.used =
        EventConfig("side_panel_shown", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHReadingModeSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    // Show the promo up to 3 times a year.
    config.trigger = EventConfig("iph_reading_mode_side_panel_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("reading_mode_side_panel_shown",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSidePanelGenericPinnableFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show the promo once a year if the side panel was not opened.
    config.trigger = EventConfig("side_panel_pinnable_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig(feature_engagement::events::kSidePanelPinned,
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDesktopSharedHighlightingFeature.name == feature->name) {
    // A config that allows the shared highlighting desktop IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("iph_desktop_shared_highlighting_trigger",
                                 Comparator(LESS_THAN, 5), 360, 360);
    config.used = EventConfig("iph_desktop_shared_highlighting_used",
                              Comparator(LESS_THAN, 2), 360, 360);
    config.event_configs.insert(
        EventConfig("iph_desktop_shared_highlighting_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    return config;
  }

  if (kIPHBatterySaverModeFeature.name == feature->name) {
    // Show promo once a year when the battery saver toolbar icon is visible.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("battery_saver_info_triggered",
                                 Comparator(LESS_THAN, 1), 360, 360);
    config.used =
        EventConfig("battery_saver_info_shown", Comparator(EQUAL, 0), 7, 360);
    return config;
  }

  if (kIPHMemorySaverModeFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times, once per week.
    config.trigger = EventConfig("high_efficiency_prompt_in_trigger",
                                 Comparator(LESS_THAN, 1), 7, 360);
    // This event is never logged but is included for consistency.
    config.used = EventConfig("high_efficiency_prompt_in_used",
                              Comparator(EQUAL, 0), 360, 360);
    config.event_configs.insert(EventConfig("high_efficiency_prompt_in_trigger",
                                            Comparator(LESS_THAN, 1), 360,
                                            360));
    return config;
  }

  if (kIPHPerformanceInterventionDialogFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    // Show intervention dialog at most 3 times per day and no more than 21
    // times per week.
    config.trigger = EventConfig("performance_intervention_dialog_trigger",
                                 Comparator(LESS_THAN, 3), 1, 360);
    config.event_configs.insert(
        EventConfig("performance_intervention_dialog_trigger",
                    Comparator(LESS_THAN, 21), 7, 360));
    return config;
  }

  if (kIPHPowerBookmarksSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("iph_power_bookmarks_side_panel_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("power_bookmarks_side_panel_shown",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHPriceTrackingInSidePanelFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo once a year if the price tracking IPH was not triggered.
    config.trigger = EventConfig("iph_price_tracking_side_panel_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("price_tracking_side_panel_shown",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSmartTabSharingTryItFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times total (per year), once per week.
    config.trigger = EventConfig("smart_tab_sharing_try_it_trigger",
                                 Comparator(EQUAL, 0), 7, 7);
    config.event_configs.insert(EventConfig("smart_tab_sharing_try_it_trigger",
                                            Comparator(LESS_THAN, 3), 360,
                                            360));
    config.used = EventConfig("smart_tab_sharing_try_it_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHSmartTabSharingDefaultOnFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Show the promo max 3 times total (per year), once per week.
    // Repeated activation of smart tab sharing is also required.
    config.trigger = EventConfig("smart_tab_sharing_default_on_trigger",
                                 Comparator(EQUAL, 0), 7, 7);
    config.event_configs.insert(
        EventConfig("smart_tab_sharing_default_on_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    config.event_configs.insert(
        EventConfig("smart_tab_sharing_activated",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 360, 360));
    config.used = EventConfig("smart_tab_sharing_default_on_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (kIPHExtensionsMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the extensions menu is
    // opened while any extension has access to the current site.
    config.trigger = EventConfig("extensions_menu_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used =
        EventConfig("extensions_menu_opened_while_extension_has_access",
                    Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHExtensionsRequestAccessButtonFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);

    // Show promo up to three times a year or until the request access button
    // is clicked.
    config.trigger = EventConfig("extensions_request_access_button_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.used = EventConfig("extensions_request_access_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  if (kIPHDiscardRingFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("discard_ring_trigger", Comparator(EQUAL, 0), 360, 360);
    // This event is never logged but is included for consistency.
    config.used =
        EventConfig("discard_ring_used", Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHDownloadEsbPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    // Because this is a custom configuration being used in desktop user ed, use
    // a non-default availability so the configurator doesn't try to write its
    // own.
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    // Don't show if user has already seen an IPH this session.
    // Show the promo max once a year if the user hasn't interacted with
    // a dangerous download within the last 21 days.
    config.trigger = EventConfig("download_bubble_esb_iph_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("enable_enhanced_protection",
                              Comparator(EQUAL, 0), 21, 360);
    config.event_configs.insert(
        EventConfig("download_bubble_dangerous_download_detected",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 21, 360));
    return config;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (kEsbDownloadRowPromoFeature.name == feature->name) {
    // A config that allows a promotion row referring users to enable Enhanced
    // Safe Browsing (ESB), to be shown on the Downloads manager page. It
    // can be viewed at most 7 times without interaction across a 90 day period.
    // If the user clicks, then we aritificially increment the viewed event by 4
    // so that the row can be seen at most 2 more times.
    //
    // The trigger management can be found in
    // c/b/ui/webui/downloads/downloads_dom_handler.cc
    FeatureConfig config;
    config.valid = true;
    // Because this is a custom configuration being used in desktop user ed, use
    // a non-default availability so the configurator doesn't try to write its
    // own.
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);

    // This isn't an IPH so we don't suppress other engagement features.
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;

    // This isn't an IPH so we don't want to block or be blocked by any other
    // engagement features.
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    config.trigger = EventConfig("dangerous_download_esb_promo_row_trigger",
                                 Comparator(ANY, 0), 360, 360);
    config.used =
        EventConfig("enable_enhanced_protection", Comparator(EQUAL, 0), 21, 90);
    config.event_configs.insert(EventConfig("esb_download_promo_row_viewed",
                                            Comparator(LESS_THAN, 7), 90, 90));
    config.event_configs.insert(EventConfig("esb_download_promo_row_clicked",
                                            Comparator(LESS_THAN, 3), 90, 90));

    return config;
  }
#endif

  if (kIPHBackNavigationMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("back_navigation_menu_iph_is_triggered",
                                 Comparator(LESS_THAN_OR_EQUAL, 4), 360, 360);
    config.used = EventConfig("back_navigation_menu_is_opened",
                              Comparator(EQUAL, 0), 7, 360);
    config.snooze_params.snooze_interval = 7;
    config.snooze_params.max_limit = 4;
    return config;
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  if (kIPHiOSEnhancedBrowsingDesktopFeature.name == feature->name) {
    // Config for allowing other IPH's to explicitly block the iOS enhanced
    // browsing promo bubble on desktop if needed. Blocked and blocking by
    // default, so won't appear at the same time as other IPH, but without any
    // session rate impact.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.blocked_by.type = BlockedBy::Type::ALL;
    config.blocking.type = Blocking::Type::ALL;
    config.used = EventConfig(
        "ios_enhanced_browsing_promo_bubble_on_desktop_interacted_with",
        Comparator(ANY, 0), 0, 0);
    config.trigger =
        EventConfig("ios_enhanced_browsing_promo_bubble_on_desktop_shown",
                    Comparator(ANY, 0), 0, 0);
    return config;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  // CONFIGURATION_ANDROID_START
  if (kIPHAndroidBottomBarNewTab.name == feature->name) {
    FeatureConfig config;
    config.valid = true;

    // IPH is always available at start-up.
    config.availability = Comparator(ANY, 0);

    // IPH can show even if other IPHs have shown this session.
    config.session_rate = Comparator(ANY, 0);

    // IPH only shows once per 360 days.
    config.trigger = EventConfig("android_bottom_bar_new_tab_trigger",
                                 Comparator(EQUAL, 0), 360, 360);

    // IPH will not show if the user has interacted with the New Tab button.
    config.used = EventConfig("android_bottom_bar_new_tab_used",
                              Comparator(EQUAL, 0), 360, 360);

    return config;
  }

  if (kIPHAndroidBottomBarPromoDialog.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    // IPH only shows once per 360 days.
    config.trigger = EventConfig("android_bottom_bar_promo_dialog_trigger",
                                 Comparator(EQUAL, 0), 360, 360);

    // IPH will not show if the user has interacted with the Promo Dialog.
    config.used = EventConfig("android_bottom_bar_promo_dialog_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHFuseboxAttachmentFeature.name == feature->name) {
    // A config that allows measurement for user engagement on the fusebox
    // attachment button by checking:
    // * Interacted with attachment popup at least 1 time in 90 days.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("fusebox_attachment_popup_shown", Comparator(ANY, 0), 0, 0);
    config.used = EventConfig("fusebox_attachment_popup_interacted_with",
                              Comparator(ANY, 0), 0, 0);
    config.event_configs.insert(EventConfig(
        "fusebox_attachment_popup_used", Comparator(GREATER_THAN, 0), 28, 360));
    return config;
  }

  if (kIPHExtensionsManageToolbarFeature.name == feature->name) {
    // Allows an IPH to be shown after a user installs an extension to inform
    // them where it can be managed.
    // Constraints:
    // - Show at most once per year (360 days).
    // - Only show if the user hasn't already clicked the extensions menu
    //   button (puzzle piece) on their own.
    // - session_rate is set to EQUAL, 0 to ensure we don't show this if another
    //   IPH was already shown in the same session.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("manage_extensions_toolbar_iph_triggered",
                                  Comparator(LESS_THAN, 1), 360, 360);
    // Only show if the user hasn't already clicked the extension menu button.
    config->used = EventConfig("extensions_menu_button_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHExtensionsManageAppMenuFeature.name == feature->name) {
    // Allows an IPH to be shown after a user unpins the extensions menu to
    // inform them where it can be managed.
    // Constraints:
    // - Show at most once per year (360 days).
    // - Only show if the user hasn't already clicked open the Chrome app menu
    //   button on their own.
    // - session_rate is set to EQUAL, 0 to ensure we don't show this if another
    //   IPH was already shown in the same session.
    std::optional<FeatureConfig> config = FeatureConfig();
    config->valid = true;
    config->availability = Comparator(ANY, 0);
    config->session_rate = Comparator(EQUAL, 0);
    config->trigger = EventConfig("manage_extensions_app_menu_iph_triggered",
                                  Comparator(LESS_THAN, 1), 360, 360);
    // Only show if the user hasn't already clicked the "Extensions" section in
    // the Chrome app menu.
    config->used = EventConfig("extensions_row_in_app_menu_clicked",
                               Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHAndroidTabDeclutter.name == feature->name) {
    // Allows an IPH for tab declutter for the tab switcher button:
    // * Only once per week.
    // * Up to 3 times per year.
    // * And only as long as the user has never manually accessed their
    //   archived tabs.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("android_tab_declutter_iph_triggered",
                                 Comparator(EQUAL, 0), 7, 7);
    config.event_configs.insert(
        EventConfig("android_tab_declutter_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    config.used = EventConfig("android_tab_declutter_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHAndroidVerticalTabsPromoFeature.name == feature->name) {
    // A config that allows the Vertical Tabs promo IPH on Android tab strip:
    // * Only once per week.
    // * Up to 3 times per year.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("android_vertical_tabs_promo_iph_triggered",
                                 Comparator(EQUAL, 0), 7, 360);
    config.event_configs.insert(
        EventConfig("android_vertical_tabs_promo_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    config.used = EventConfig("android_vertical_tabs_promo_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHIncognitoIndicatorCloseAllWindows.name == feature->name) {
    // Allows an IPH to inform users they can close all Incognito windows:
    // - Only once per week.
    // - Up to 3 times per year.
    // - Only as long as the user has not manually clicked the Incognito
    // indicator in the last year.
    // - session_rate is set to EQUAL, 0 to ensure we don't show this if another
    //   IPH was already shown in the same session.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("incognito_indicator_close_all_windows_trigger",
                    Comparator(EQUAL, 0), 7, 360);
    config.event_configs.insert(
        EventConfig("incognito_indicator_close_all_windows_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    config.used = EventConfig("incognito_indicator_close_all_windows_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHAppRatingPromptFeature.name == feature->name) {
    // A config that allows the App Rating Prompt IPH to be shown only if
    // no other startup promos have been shown in the last 3 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    // Ensure no other startup promos were shown in the last 72 hours (3 days).
    config.trigger = EventConfig("android_startup_promo_shown",
                                 Comparator(EQUAL, 0), 3, 360);
    return config;
  }

  if (kIPHAppSpecificHistory.name == feature->name) {
    // A config that allows the AppSpecificHistory IPH to be shown once
    // a week, up to 3 times, unless the button is clicked at least once.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("app_specific_history_iph_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.event_configs.insert(EventConfig("app_specific_history_iph_trigger",
                                            Comparator(LESS_THAN, 1), 7, 360));
    config.used = EventConfig("history_toolbar_search_menu_item_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHSiteControlsFeature.name == feature->name) {
    // A config that allows the site controls IPH to be shown at most once.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("site_controls_iph_trigger",
                                 Comparator(LESS_THAN, 1), 360, 360);
    config.used = EventConfig("site_controls_menu_item_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHBottomToolbarTipFeature.name == feature->name) {
    // A config that allows the Bottom Toolbar IPH to be shown once
    // a week, up to 3 times, unless the user interacts with Bottom Toolbar
    // controls.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);

    config.trigger = EventConfig("bottom_toolbar_iph_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.event_configs.insert(EventConfig("bottom_toolbar_iph_trigger",
                                            Comparator(LESS_THAN, 1), 7, 360));
    config.used = EventConfig("bottom_toolbar_menu_triggered",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHCCTHistory.name == feature->name) {
    // A config that allows the CCTHistory IPH to be shown once
    // a week, up to 3 times, unless the button is clicked at least once.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("cct_history_iph_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.event_configs.insert(EventConfig("cct_history_iph_trigger",
                                            Comparator(LESS_THAN, 1), 7, 360));
    config.used = EventConfig("cct_history_menu_item_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHCCTMinimized.name == feature->name) {
    // A config that allows the Custom Tab minimize button IPH to be shown once
    // a day, up to 3 times, unless the button is clicked at least once.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("cct_minimized_iph_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.event_configs.insert(EventConfig("cct_minimized_iph_trigger",
                                            Comparator(LESS_THAN, 1), 1, 360));
    config.used = EventConfig("cct_minimize_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHDataSaverDetailFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("data_saver_detail_iph_trigger",
                                 Comparator(EQUAL, 0), 90, 360);
    config.used = EventConfig("data_saver_overview_opened",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("data_saved_page_load",
                    Comparator(GREATER_THAN_OR_EQUAL, 10), 90, 360));
    return config;
  }
  if (kIPHDataSaverPreviewFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("data_saver_preview_iph_trigger",
                                 Comparator(EQUAL, 0), 90, 360);
    config.used = EventConfig("data_saver_preview_opened",
                              Comparator(LESS_THAN_OR_EQUAL, 1), 90, 360);
    return config;
  }
  if (kIPHPreviewsOmniboxUIFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("previews_verbose_iph_triggered_2",
                                 Comparator(LESS_THAN, 2), 90, 360);
    config.used = EventConfig("previews_verbose_status_opened",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(EventConfig(
        "preview_page_load", Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 360));
    return config;
  }
  if (kIPHDownloadHomeFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("download_home_iph_trigger", Comparator(EQUAL, 0), 90, 360);
    config.used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(EventConfig(
        "download_completed", Comparator(GREATER_THAN_OR_EQUAL, 1), 90, 360));
    config.snooze_params.snooze_interval = 7;
    config.snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHDownloadIndicatorFeature.name == feature->name) {
    // A config that allows the DownloadIndicator IPH to be shown up to 2 times,
    // but only if download home hasn't been opened in the last 90 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("download_indicator_iph_trigger",
                                 Comparator(LESS_THAN, 2), 360, 360);
    config.used =
        EventConfig("download_home_opened", Comparator(EQUAL, 0), 90, 360);
    return config;
  }
  if (kIPHContextualPageActionsQuietVariantFeature.name == feature->name) {
    // A config that allows the contextual page action IPH to be shown:
    // * Once per day. 3 times max in 90 days
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("contextual_page_actions_quiet_variant_iph_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    config.used = EventConfig("contextual_page_actions_quiet_variant_used",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("contextual_page_actions_quiet_variant_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }
  if (kIPHContextualPageActionsActionChipFeature.name == feature->name) {
    // A config that allows the Contextual Page Action Chip to be shown:
    // * 3 times per session.
    // * 5 times per day.
    // * 10 times per week.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 3);
    config.trigger =
        EventConfig("contextual_page_actions_action_chip_iph_trigger",
                    Comparator(LESS_THAN, 5), 1, 360);
    config.event_configs.insert(
        EventConfig("contextual_page_actions_action_chip_iph_trigger",
                    Comparator(LESS_THAN, 10), 7, 360));
    return config;
  }
  if (kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature.name ==
      feature->name) {
    // A config that allows the web page summary toolbar button IPH to be shown:
    // * Once per day. 3 times max in 90 days
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("adaptive_toolbar_page_summary_web_iph_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    config.used = EventConfig("adaptive_toolbar_page_summary_web_used",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("adaptive_toolbar_page_summary_web_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }
  if (kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature.name ==
      feature->name) {
    // A config that allows the pdf page summary toolbar button IPH to be shown:
    // * Once per day. 3 times max in 90 days
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("adaptive_toolbar_page_summary_pdf_iph_trigger",
                    Comparator(LESS_THAN, 1), 1, 360);
    config.used = EventConfig("adaptive_toolbar_page_summary_pdf_used",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("adaptive_toolbar_page_summary_pdf_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }

  if (kIPHMenuAddToGroup.name == feature->name) {
    // Allows an IPH for the main app menu 'Add to Group' entry:
    // * Only once per year.
    // * If the user has used chrome for more than 14 days.
    // * And only if the menu option hasn't been opened in that time.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("menu_add_to_group_iph_triggered",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("menu_add_to_group_clicked", Comparator(EQUAL, 0),
                              360, 360);
    return config;
  }
  if (kIPHMostVisitedTilesCustomizationPinFeature.name == feature->name) {
    // Allows an IPH for the MVT customization "Pin this shortcut" feature.
    // * Only once in its lifetime.
    // * (Per trigger logic) Only if the user has no Custom Tiles.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("most_visited_tiles_customization_pin_triggered",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    config.used =
        EventConfig("most_visited_tiles_customization_pin_clicked",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHPageSummaryWebMenuFeature.name == feature->name) {
    // A config that allows the web page summary menu item IPH to be shown:
    // * Once per day. 3 times max in 90 days
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("menu_item_page_summary_web_iph_trigger",
                                 Comparator(LESS_THAN, 1), 1, 360);
    config.used = EventConfig("menu_item_page_summary_web_used",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("menu_item_page_summary_web_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }
  if (kIPHPageSummaryPdfMenuFeature.name == feature->name) {
    // A config that allows the pdf page summary menu item IPH to be shown:
    // * Once per day. 3 times max in 90 days
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("menu_item_page_summary_pdf_iph_trigger",
                                 Comparator(LESS_THAN, 1), 1, 360);
    config.used = EventConfig("menu_item_page_summary_pdf_used",
                              Comparator(EQUAL, 0), 90, 360);
    config.event_configs.insert(
        EventConfig("menu_item_page_summary_pdf_iph_trigger",
                    Comparator(LESS_THAN, 3), 90, 360));
    return config;
  }

  // A generic feature that always returns true.
  if (kIPHGenericAlwaysTriggerHelpUiFeature.name == feature->name) {
    return CreateAlwaysTriggerConfig(feature);
  }

  if (kIPHLowUserEngagementDetectorFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    config.trigger = EventConfig("low_user_engagement_detector_trigger",
                                 Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("low_user_engagement_detector_used",
                              Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(EventConfig("foreground_session_destroyed",
                                            Comparator(LESS_THAN_OR_EQUAL, 3),
                                            14, 14));
    return config;
  }

  if (kIPHFeedHeaderMenuFeature.name == feature->name) {
    // A config that allows the feed header menu IPH to be shown only once when
    // the user starts using a version of the feed that uploads click and view
    // actions.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);

    config.session_rate = Comparator(ANY, 0);
    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;

    // Keep the IPH trigger event for 10 years, which is a relatively long time
    // period that we could consider as being "forever".
    config.trigger =
        EventConfig("iph_feed_header_menu_triggered", Comparator(LESS_THAN, 1),
                    k10YearsInDays, k10YearsInDays);
    // Set a dummy config for the used event to be consistent with the other
    // IPH configurations. The used event is never recorded by the feature code
    // because the trigger event is already reported the first time the feed is
    // being used, which corresponds to a used event.
    config.used = EventConfig("iph_feed_header_menu_used", Comparator(EQUAL, 0),
                              k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHFeedSwipeRefresh.name == feature->name) {
    // A config that allows the feed swipe refresh message IPH to be shown:
    // * Once per 15 days
    // * Up to 2 times but only if unused in the last 15 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("feed_swipe_refresh_iph_trigger",
                                 Comparator(LESS_THAN, 2), 90, 90);
    config.used =
        EventConfig("feed_swipe_refresh_shown", Comparator(EQUAL, 0), 90, 90);
    config.event_configs.insert(EventConfig("feed_swipe_refresh_iph_trigger",
                                            Comparator(EQUAL, 0), 15, 90));
    return config;
  }
  if (kIPHTabSwitcherAddToGroup.name == feature->name) {
    // Allows an IPH for the 'Add to Group' 3-dot menu entry:
    // * Only once per year.
    // * If the user has used chrome for more than 14 days.
    // * And only if the menu option hasn't been opened in that time.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("tab_switcher_add_to_group_iph_triggered",
                                 Comparator(EQUAL, 0), 360, 360);
    config.used = EventConfig("tab_switcher_add_to_group_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
  if (kIPHTabSwitcherButtonFeature.name == feature->name) {
    // Adjusted to be less spammy for users that may know what the tab switcher
    // is. Show after 14 days of Chrome being installed, once every 90 days,
    // unless the user has used the tab switcher button in the last year.
    // Hopefully a year will be long enough that infrequent users of a given
    // Chrome channel should almost never see it.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger =
        EventConfig("tab_switcher_iph_triggered", Comparator(EQUAL, 0), 90, 90);
    config.used = EventConfig("tab_switcher_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    config.snooze_params.snooze_interval = 7;
    config.snooze_params.max_limit = 3;
    return config;
  }
  if (kIPHTabSwitcherButtonSwitchIncognitoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("tab_switcher_switch_incognito_iph_triggered",
                                 Comparator(EQUAL, 0), 90, 90);
    config.used = EventConfig("tab_switcher_button_long_clicked",
                              Comparator(EQUAL, 0), 14, 90);
    config.snooze_params.snooze_interval = 7;
    config.snooze_params.max_limit = 3;
    return config;
  }

  if (kIPHVideoTutorialNTPChromeIntroFeature.name == feature->name) {
    // A config that allows the chrome intro video tutorial card to show up
    // until explicitly interacted upon.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("tutorial_chrome_intro_iph_trigger",
                                 Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("chrome_intro", Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(
        EventConfig("video_tutorial_iph_clicked_chrome_intro",
                    Comparator(EQUAL, 0), 90, 90));
    config.event_configs.insert(
        EventConfig("video_tutorial_iph_dismissed_chrome_intro",
                    Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPDownloadFeature.name == feature->name) {
    // A config that allows the download video tutorial card to show up
    // until explicitly interacted upon.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("tutorial_download_iph_trigger",
                                 Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("download", Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(EventConfig(
        "video_tutorial_iph_clicked_download", Comparator(EQUAL, 0), 90, 90));
    config.event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_download", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPSearchFeature.name == feature->name) {
    // A config that allows the search video tutorial card to show up
    // until explicitly interacted upon.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("tutorial_search_iph_trigger", Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("search", Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(EventConfig("video_tutorial_iph_clicked_search",
                                            Comparator(EQUAL, 0), 90, 90));
    config.event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_search", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPVoiceSearchFeature.name == feature->name) {
    // A config that allows the voice search video tutorial card to show up
    // until explicitly interacted upon.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("tutorial_voice_search_iph_trigger",
                                 Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("voice_search", Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(
        EventConfig("video_tutorial_iph_clicked_voice_search",
                    Comparator(EQUAL, 0), 90, 90));
    config.event_configs.insert(
        EventConfig("video_tutorial_iph_dismissed_voice_search",
                    Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialNTPSummaryFeature.name == feature->name) {
    // A config that allows the summary video tutorial card to show up
    // until explicitly interacted upon.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("tutorial_summary_iph_trigger", Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("summary", Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(EventConfig(
        "video_tutorial_iph_dismissed_summary", Comparator(EQUAL, 0), 90, 90));

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    return config;
  }

  if (kIPHVideoTutorialTryNowFeature.name == feature->name) {
    // A config that allows the video tutorials Try Now button click to result
    // in an IPH bubble. This IPH is shown always regardless of session rate or
    // any other conditions.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("tutorial_try_now_iph_trigger", Comparator(ANY, 0), 90, 90);
    config.used = EventConfig("try_now", Comparator(ANY, 0), 90, 90);

    SessionRateImpact session_rate_impact;
    session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.session_rate_impact = session_rate_impact;

    return config;
  }

  if (kIPHSharedHighlightingReceiverFeature.name == feature->name) {
    // A config that allows the shared highlighting message IPH to be shown
    // when a user receives a highlight:
    // * Once per 7 days
    // * Up to 5 times but only if unused in the last 7 days.
    // * Used fewer than 2 times

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("iph_shared_highlighting_receiver_trigger",
                                 Comparator(LESS_THAN, 5), 360, 360);
    config.used = EventConfig("iph_shared_highlighting_used",
                              Comparator(LESS_THAN, 2), 360, 360);
    config.event_configs.insert(
        EventConfig("iph_shared_highlighting_receiver_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    return config;
  }

  if (kIPHSharingHubWebnotesStylizeFeature.name == feature->name) {
    // A config that allows the Webnotes Stylize IPH to be shown up to 6 times,
    // but only if the feature home hasn't been used in the last 360 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("sharing_hub_webnotes_stylize_iph_trigger",
                                 Comparator(LESS_THAN, 6), 360, 360);
    config.used = EventConfig("sharing_hub_webnotes_stylize_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHAutoDarkOptOutFeature.name == feature->name) {
    // A config that allows the auto dark dialog to be shown when a user
    // disables the feature for a site in the app menu when:
    // * They have not yet opened auto dark settings.
    // * The dialog has been shown 0 times before.
    // * They have done so at least 3 times.
    // TODO(crbug.com/40198496): Update this config from test values; Will
    // likely depend on giving feedback instead of opening settings, since the
    // primary purpose  of the dialog has changed.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("auto_dark_settings_opened", Comparator(EQUAL, 0), 90, 90);
    config.trigger = EventConfig("auto_dark_opt_out_iph_trigger",
                                 Comparator(EQUAL, 0), 90, 90);
    config.event_configs.insert(
        EventConfig("auto_dark_disabled_in_app_menu",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 90, 90));
    return config;
  }

  if (kIPHAutoDarkUserEducationMessageFeature.name == feature->name) {
    // A config that allows the auto dark message to be shown:
    // * Until the user opens auto dark settings
    // * 2 times per week
    // * Up to 6 times (3 weeks)
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("auto_dark_settings_opened", Comparator(EQUAL, 0),
                              360, 360);
    config.trigger = EventConfig("auto_dark_user_education_message_trigger",
                                 Comparator(LESS_THAN, 2), 7, 360);
    config.event_configs.insert(
        EventConfig("auto_dark_user_education_message_trigger",
                    Comparator(LESS_THAN, 6), 360, 360));
    return config;
  }

  if (kIPHAutoDarkUserEducationMessageOptInFeature.name == feature->name) {
    // A config that allows the auto dark message to be shown:
    // * Until the user opens auto dark settings
    // * 2 times per week
    // * Up to 6 times (3 weeks)
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used = EventConfig("auto_dark_settings_opened", Comparator(EQUAL, 0),
                              360, 360);
    config.trigger =
        EventConfig("auto_dark_user_education_message_opt_in_trigger",
                    Comparator(LESS_THAN, 2), 7, 360);
    config.event_configs.insert(
        EventConfig("auto_dark_user_education_message_opt_in_trigger",
                    Comparator(LESS_THAN, 6), 360, 360));
    return config;
  }

  if (kIPHInstanceSwitcherFeature.name == feature->name) {
    // A config that allows the 'Manage windows' text bubble IPH to be shown
    // only once when the user starts using the multi-instance feature by
    // opening more than one window.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("instance_switcher_iph_trigger", Comparator(LESS_THAN, 1),
                    k10YearsInDays, k10YearsInDays);
    config.used = EventConfig("instance_switcher_used", Comparator(EQUAL, 0),
                              k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHRecentTabsFeature.name == feature->name) {
    // A config that allows the 'Recent tabs' text bubble IPH to be shown
    // only once to inform users about the recently closed tabs/windows.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("recent_tabs_iph_trigger", Comparator(LESS_THAN, 1),
                    k10YearsInDays, k10YearsInDays);
    config.used =
        EventConfig("recent_tabs_used_with_closed_windows",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    return config;
  }

  if (kIPHRequestDesktopSiteDefaultOnFeature.name == feature->name) {
    // A config that allows the RDS default-on message to be shown:
    // * If the user has never accepted the message.
    // * The message can show twice, but only once in a week.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN, 1);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.used = EventConfig("desktop_site_settings_page_opened",
                              Comparator(ANY, 0), 360, 360);
    config.trigger = EventConfig("request_desktop_site_default_on_iph_trigger",
                                 Comparator(LESS_THAN_OR_EQUAL, 1), 360, 360);
    config.event_configs.insert(
        EventConfig("request_desktop_site_default_on_iph_trigger",
                    Comparator(EQUAL, 0), 7, 360));
    config.event_configs.insert(
        EventConfig("desktop_site_default_on_primary_action",
                    Comparator(EQUAL, 0), 360, 360));
    config.event_configs.insert(EventConfig("desktop_site_default_on_gesture",
                                            Comparator(ANY, 0), 360, 360));
    return config;
  }

  if (kIPHRequestDesktopSiteExceptionsGenericFeature.name == feature->name) {
    // A config that allows the RDS site-level setting IPH to be shown to
    // tablet users. This will be triggered a maximum of 2 times (once per
    // 2 weeks), and if the user has not used the app menu to create a desktop
    // site exception in a span of a year.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 2);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.used = EventConfig("app_menu_desktop_site_exception_added",
                              Comparator(EQUAL, 0), 360, 360);
    config.trigger =
        EventConfig("request_desktop_site_exceptions_generic_iph_trigger",
                    Comparator(LESS_THAN, 2), 720, 720);
    config.event_configs.insert(
        EventConfig("request_desktop_site_exceptions_generic_iph_trigger",
                    Comparator(EQUAL, 0), 14, 14));
    return config;
  }

  if (kIPHPageZoomFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("page_zoom_iph_trigger", Comparator(EQUAL, 0), 1440, 1440);
    config.used =
        EventConfig("page_zoom_opened", Comparator(EQUAL, 0), 1440, 1440);
    return config;
  }

  if (kIPHPdfPageDownloadFeature.name == feature->name) {
    // A config that allows the pdf page download IPH to be shown to users.
    // This will be triggered a maximum of 3 times (once per 2 weeks) with pdf
    // opened, and if the user has not used the app menu to download pdf in a
    // span of a year.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.used = EventConfig("app_menu_pdf_page_downloaded",
                              Comparator(EQUAL, 0), 360, 360);
    config.trigger = EventConfig("pdf_page_download_iph_trigger",
                                 Comparator(LESS_THAN, 3), 360, 360);
    config.event_configs.insert(EventConfig("pdf_page_download_iph_trigger",
                                            Comparator(EQUAL, 0), 14, 14));
    return config;
  }

  if (kIPHRestoreTabsOnFREFeature.name == feature->name) {
    // A config that allows the restore tabs on FRE promo to be shown:
    // * If the user has gone through the FRE workflow.
    // * If the promo has never been accepted.
    // * Once per week if continually dismissed for a max of 2 weeks.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(LESS_THAN_OR_EQUAL, 14);
    config.session_rate = Comparator(ANY, 0);
    config.trigger =
        EventConfig("restore_tabs_promo_trigger", Comparator(EQUAL, 0), 7, 14);
    config.used =
        EventConfig("restore_tabs_promo_used", Comparator(EQUAL, 0), 14, 14);
    config.event_configs.insert(
        EventConfig("restore_tabs_on_first_run_show_promo",
                    Comparator(GREATER_THAN_OR_EQUAL, 1), 14, 14));
    return config;
  }

  if (kIPHRequestDesktopSiteWindowSettingFeature.name == feature->name) {
    // A config that allows the RDS window setting IPH to be shown at most once
    // in 3 years per device.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig("request_desktop_site_window_setting_iph_shown",
                              Comparator(EQUAL, 0), 1080, 1080);
    config.trigger =
        EventConfig("request_desktop_site_window_setting_iph_trigger",
                    Comparator(EQUAL, 0), 1080, 1080);
    return config;
  }

  if (kIPHReadAloudExpandedPlayerFeature.name == feature->name) {
    // Show tooltip at most 3 times, once a day, but stop if user saw
    // expanded player.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig("read_aloud_expanded_player_shown",
                              Comparator(EQUAL, 0), 360, 360);
    config.trigger = EventConfig("read_aloud_expanded_player_shown_iph_trigger",
                                 Comparator(EQUAL, 0), 1, 1);
    config.event_configs.insert(
        EventConfig("read_aloud_expanded_player_shown_iph_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    return config;
  }

  if (kIPHReadAloudPlaybackModeFeature.name == feature->name) {
    // Show tooltip at most 3 times, once a day, but stop if user hit
    // playback mode.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig("read_aloud_playback_mode_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    config.trigger = EventConfig("read_aloud_playback_mode_iph_trigger",
                                 Comparator(EQUAL, 0), 1, 1);
    config.event_configs.insert(
        EventConfig("read_aloud_playback_mode_iph_trigger",
                    Comparator(LESS_THAN, 3), 360, 360));
    return config;
  }

  if (kIPHDefaultBrowserPromoMagicStackFeature.name == feature->name) {
    // A config that allows the default browser promo Magic Stack to be shown:
    // * Up to 3 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.trigger =
        EventConfig("default_browser_promo_magic_stack_trigger",
                    Comparator(LESS_THAN, 3), k10YearsInDays, k10YearsInDays);
    config.groups.push_back(kClankDefaultBrowserPromosGroup.name);

    return config;
  }

  if (kIPHDefaultBrowserPromoMessagesFeature.name == feature->name) {
    // A config that allows the default browser promo messages to be shown:
    // * Up to 2 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.trigger =
        EventConfig("default_browser_promo_messages_trigger",
                    Comparator(LESS_THAN, 2), k10YearsInDays, k10YearsInDays);
    config.used = EventConfig("default_browser_promo_messages_used",
                              Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(
        EventConfig("default_browser_promo_messages_dismissed",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays));
    config.groups.push_back(kClankDefaultBrowserPromosGroup.name);
    return config;
  }

  if (kIPHDefaultBrowserPromoSettingCardFeature.name == feature->name) {
    // A config that allows the default browser promo setting card to be shown:
    // * Up to 4 times
    // * Neither Magic Stack, Messages, nor the settings card has been triggered
    // in the last 7 days.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.trigger =
        EventConfig("default_browser_promo_setting_card_trigger",
                    Comparator(LESS_THAN, 4), k10YearsInDays, k10YearsInDays);
    config.used = EventConfig("default_browser_promo_setting_card_used",
                              Comparator(ANY, 0), 90, 90);
    config.event_configs.insert(
        EventConfig("default_browser_promo_setting_card_dismissed",
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays));
    config.groups.push_back(kClankDefaultBrowserPromosGroup.name);

    return config;
  }

  if (kIPHTabSwitcherXR.name == feature->name) {
    // A config that allows the XR tab switcher IPH to be shown at most 3 times.
    // IPH will not be shown is user has clicked on it, otherwise it will be
    // snoozed for one day.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.trigger =
        EventConfig("tab_switcher_xr_iph_trigger", Comparator(LESS_THAN, 3),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used = EventConfig("tab_switcher_xr_iph_used", Comparator(ANY, 0),
                              feature_engagement::kMaxStoragePeriod,
                              feature_engagement::kMaxStoragePeriod);
    config.snooze_params.snooze_interval = 1;
    config.snooze_params.max_limit = 3;

    return config;
  }
  if (kIPHTabTearingXR.name == feature->name) {
    // A config that allows XR tab tearing IPH to be shown
    // at most 3 times. IPH will not be shown is user has clicked on it,
    // otherwise it will be snoozed for one day.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.trigger =
        EventConfig("tab_tearing_xr_iph_trigger", Comparator(LESS_THAN, 3),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used = EventConfig("tab_tearing_xr_iph_used", Comparator(ANY, 0),
                              feature_engagement::kMaxStoragePeriod,
                              feature_engagement::kMaxStoragePeriod);
    config.snooze_params.snooze_interval = 1;
    config.snooze_params.max_limit = 3;

    return config;
  }
  if (kIPHReaderModeDistillInAppFeature.name == feature->name) {
    // A config which allows the reader mode IPH to be shown:
    // * Once per week.
    // * Three times per year.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.blocked_by.type = BlockedBy::Type::NONE;

    // Show max of one time per week.
    config.trigger = EventConfig("reader_mode_distill_in_app_iph_triggered",
                                 Comparator(EQUAL, 0), 7, 7);
    // Show max of three times per year.
    config.event_configs.insert(
        EventConfig("reader_mode_distill_in_app_iph_triggered",
                    Comparator(LESS_THAN, 3), 360, 360));
    return config;
  }

  if (kIPHThreeDotMenuBackButton.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability =
        Comparator(GREATER_THAN_OR_EQUAL,
                   4);  // Available after greater than three days
    config.session_rate = Comparator(
        EQUAL, 0);  // Only shows when no other IPH has been shown this session

    // Only show the IPH no more than 2 times per year
    config.trigger = EventConfig("three_dot_menu_back_button_trigger",
                                 Comparator(LESS_THAN, 2), 360, 360);

    // The IPH will only be shown if the back button has not been used in the
    // last 360 days.
    config.used = EventConfig("three_dot_menu_back_button_clicked",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }

  if (kIPHGestureUserEducation.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(
        GREATER_THAN_OR_EQUAL, 4);  // Available after greater than 3 days.
    config.session_rate = Comparator(
        EQUAL, 0);  // Only shows when no other IPH has been shown this session

    // Only show the IPH once per year
    config.trigger = EventConfig("gesture_user_education_trigger",
                                 Comparator(EQUAL, 0), 360, 360);
    // The IPH will only be shown if the back swipe has been used less than two
    // times in the last 360 days.
    config.used = EventConfig("swipe_on_left_edge_for_navigation_used",
                              Comparator(LESS_THAN, 2), 360, 360);
    return config;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // This is enabled on both Desktop and Desktop Android. However, this feature
  // is auto-configured dynamically by the Feature Engagement service on Desktop
  // (see
  // components/feature_engagement/browser/user_data/feature_engagement_tracker.h).
  // Manual configuration is explicitly bypassed on Desktop (and only done here
  // on Android) to prevent test failures in
  // BrowserUserEducationServiceTest.PreventNewHardCodedConfigurations. This
  // block is scoped strictly to Android where auto-configuration is not
  // supported by the Feature Engagement tracker.
  if (kIPHExtensionsPinnedByDefaultFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("extensions_pinned_by_default_trigger",
                                 Comparator(LESS_THAN, 1), 360, 360);
    config.used = EventConfig("extensions_pinned_by_default_used",
                              Comparator(EQUAL, 0), 360, 360);
    return config;
  }
#endif

// CONFIGURATION_ANDROID_END
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

  if (kIPHAutofillCreditCardBenefitFeature.name == feature->name) {
    // The credit card benefit IPH appears up to three times over 10 years and
    // only once per session. Dismissing it stops it from showing again.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("autofill_credit_card_benefit_iph_trigger",
                                 Comparator(LESS_THAN, 3), 90, 360);
    config.used =
        EventConfig("autofill_credit_card_benefit_iph_accepted",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
  if (kIPHiOSReaderModeOptionsFeature.name == feature->name) {
    // A config that shows the IPH on the Reading mode entrypoint in the
    // omnibox. Aligns with the contextual panel IPH entrypoint defaults, ie.
    // shows the IPH 3 times every 6 months (max 1 per day), for a maximum of 6
    // times lifetime. Stops showing the IPH if Reading Mode options
    // configuration is used.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.used = EventConfig(
        feature_engagement::events::kIOSIPHReaderModeOptionsUsed,
        Comparator(LESS_THAN, 1), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    config.trigger = EventConfig(
        feature_engagement::events::kIOSIPHReaderModeOptionsTriggered,
        Comparator(LESS_THAN, 3), 182, feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kIOSIPHReaderModeOptionsTriggered,
        Comparator(LESS_THAN, 1), 1, feature_engagement::kMaxStoragePeriod));
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kIOSIPHReaderModeOptionsTriggered,
        Comparator(LESS_THAN, 6), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHiOSContextualPanelSampleModelFeature.name == feature->name) {
    // The contextual panel's sample model entrypoint IPH config to control the
    // impressions of the IPH for this infoblock. Shows the IPH up to 3 times
    // per day if the user doesn't interact with the entrypoint or explicitly
    // dismiss it, and is blocking/blocked to/by all other IPHs.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.used = EventConfig(feature_engagement::events::
                                  kIOSContextualPanelSampleModelEntrypointUsed,
                              Comparator(LESS_THAN, 1), 1, 1);
    config.trigger =
        EventConfig("ios_contextual_panel_sample_model_entrypoint_iph_trigger",
                    Comparator(LESS_THAN, 3), 1, 1);
    config.event_configs.insert(EventConfig(
        "ios_contextual_panel_sample_model_entrypoint_explicitly_dismissed",
        Comparator(LESS_THAN, 1), 1, 1));
    return config;
  }

  if (kIPHDefaultSiteViewFeature.name == feature->name) {
    // A config that shows an IPH on the overflow menu button advertising the
    // Default Page Mode feature when the user has requested the Desktop version
    // of a website 3 times in 60 days. It will be shown every other year unless
    // the user interacted with the setting in the past 2 years.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.used =
        EventConfig("default_site_view_used", Comparator(EQUAL, 0), 720, 720);
    config.trigger =
        EventConfig("default_site_view_shown", Comparator(EQUAL, 0), 720, 720);
    config.event_configs.insert(
        EventConfig("desktop_version_requested",
                    Comparator(GREATER_THAN_OR_EQUAL, 3), 60, 60));
    return config;
  }

  if (kIPHiOSDefaultBrowserOverflowMenuBadgeFeature.name == feature->name) {
    // A config to allow a user to be shown the blue dot promo on the Chrome
    // Settings icon in overflow menu and Default Browser row in Chrome
    // Settings.
    // This FET feature is non-blocking because it is a passive promo
    // that appears alongside the rest of the UI, and does not interrupt the
    // user's flow.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.trigger = EventConfig("blue_dot_promo_overflow_menu_shown",
                                 Comparator(ANY, 0), 360, 360);
    // Stop showing blue dot promo if overflow menu opened while blue dot was
    // showing at least 3 times.
    config.used = EventConfig("blue_dot_promo_overflow_menu_opened",
                              Comparator(LESS_THAN, 3), 360, 360);
    // Stop showing blue dot promo if default browser settings was opened at
    // least once.
    config.event_configs.insert(EventConfig("blue_dot_promo_settings_dismissed",
                                            Comparator(EQUAL, 0), 360, 360));
    // Stop showing blue dot promo if overflow menu was customized while blue
    // dot was showing.
    config.event_configs.insert(EventConfig("blue_dot_overflow_menu_customized",
                                            Comparator(EQUAL, 0), 360, 360));

    // Cooldowns from other default browser promos.
    config.event_configs.insert(EventConfig("default_browser_promo_shown",
                                            Comparator(EQUAL, 0), 14, 360));
    config.event_configs.insert(EventConfig(events::kIOSDefaultBrowserFREShown,
                                            Comparator(EQUAL, 0), 21, 360));
    config.event_configs.insert(
        EventConfig(events::kDefaultBrowserPromosGroupTrigger,
                    Comparator(EQUAL, 0), 14, 360));
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kChromeOpened,
                    Comparator(GREATER_THAN_OR_EQUAL, 7), 360, 360));

    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSFeedSwipeAnimatedFeature.name == feature->name) {
    // The animated IPH to promote scrolling on the feed.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(GREATER_THAN_OR_EQUAL, 3);
    config.session_rate = Comparator(EQUAL, 0);
    // The IPH is shown at most once.
    config.trigger =
        EventConfig("iph_feed_swipe_animated_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The user hasn't scrolled on the NTP while the feed is visible.
    config.used =
        EventConfig(feature_engagement::events::kIOSScrolledOnFeed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The IPH only shows when user has engaged with the feed in any way.
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kIOSActionOnFeed,
        Comparator(GREATER_THAN, 0), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHiOSFeedSwipeStaticFeature.name == feature->name) {
    // The static IPH to promote scrolling on the feed.

    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    // The IPH is shown at most once.
    config.trigger =
        EventConfig("iph_feed_swipe_static_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The user hasn't scrolled on the NTP while the feed is visible.
    config.used =
        EventConfig(feature_engagement::events::kIOSScrolledOnFeed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSHistoryOnOverflowMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    // The user hasn't tapped the history on the overflow menu.
    config.used =
        EventConfig(feature_engagement::events::kHistoryOnOverflowMenuUsed,
                    Comparator(EQUAL, 0), k10YearsInDays, k10YearsInDays);
    // The IPH is shown at most 1 time a week.
    config.trigger = EventConfig("history_on_overflow_menu_trigger",
                                 Comparator(EQUAL, 0), 7, 7);
    // The IPH is shown at most 2 times a year.
    config.event_configs.insert(EventConfig("history_on_overflow_menu_trigger",
                                            Comparator(LESS_THAN, 2), 365,
                                            365));
    // The user has opened URL from omnibox > 2 times in the past.
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kOpenUrlFromOmnibox,
        Comparator(GREATER_THAN, 2), k10YearsInDays, k10YearsInDays));
    return config;
  }

  if (kIPHiOSHomepageCustomizationNewBadge.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);  // Available immediately
    config.session_rate = Comparator(LESS_THAN, 1);
    config.used =
        EventConfig(events::kHomeCustomizationMenuUsed, Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.trigger = EventConfig("ios_homepage_customization_badge_trigger",
                                 Comparator(LESS_THAN, 3),
                                 feature_engagement::kMaxStoragePeriod,
                                 feature_engagement::kMaxStoragePeriod);
    config.groups.push_back(kiOSHomepageNewBadgesGroup.name);
    return config;
  }

  if (kIPHiOSPullToRefreshFeature.name == feature->name) {
    // The IPH of the pull-to-refresh feature for the current tab.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSMultiGestureRefreshUsed,
        /*trigger_event=*/"iph_pull_to_refresh_trigger", /*used_event=*/
        feature_engagement::events::kIOSPullToRefreshUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::kIOSPullToRefreshIPHDismissButtonTapped);
  }

  if (kIPHiOSReminderNotificationsOverflowMenuBubbleFeature.name ==
      feature->name) {
    FeatureConfig config;
    config.valid = true;
    // No availability requirement for this feature.
    config.availability = Comparator(ANY, 0);
    // No session rate limit for this feature.
    config.session_rate = Comparator(ANY, 0);
    // Initially, show to users who haven't scheduled a tab reminder yet.
    config.used =
        EventConfig(feature_engagement::events::kIOSTabReminderScheduled,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The Overflow Menu Bubble IPH should not be triggered more than 3 times in
    // total.
    config.trigger = EventConfig(
        feature_engagement::events::
            kIOSReminderNotificationsOverflowMenuBubbleIPHTrigger,
        Comparator(LESS_THAN, 3), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    // Space out IPH triggers to once per day.
    config.event_configs.insert(EventConfig(
        feature_engagement::events::
            kIOSReminderNotificationsOverflowMenuBubbleIPHTrigger,
        Comparator(EQUAL, 0), 1, feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHBadgedReaderModeFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    // No availability requirement for this feature.
    config.availability = Comparator(ANY, 0);
    // No session rate limit for this feature.
    config.session_rate = Comparator(ANY, 0);
    // Initially, show to users who haven't interacted with Reading Mode.
    config.used =
        EventConfig(feature_engagement::events::kIOSReaderModeUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // The New Badge IPH should not be triggered more than 3 times
    // in total.
    config.trigger = EventConfig(
        feature_engagement::events::kIOSIPHBadgedReaderModeTriggered,
        Comparator(LESS_THAN, 3), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature.name ==
      feature->name) {
    FeatureConfig config;
    config.valid = true;
    // No availability requirement for this feature.
    config.availability = Comparator(ANY, 0);
    // No session rate limit for this feature.
    config.session_rate = Comparator(ANY, 0);
    // Initially, show to users who haven't tapped the "Set a Reminder" overflow
    // menu action yet.
    config.used = EventConfig(
        feature_engagement::events::kIOSOverflowMenuSetTabReminderTapped,
        Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    // The New Badge IPH should not be triggered more than 3 times
    // in total.
    config.trigger = EventConfig(
        feature_engagement::events::
            kIOSReminderNotificationsOverflowMenuNewBadgeIPHTrigger,
        Comparator(LESS_THAN, 3), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    // Don't show if the user has already scheduled a tab reminder.
    config.event_configs.insert(
        EventConfig(feature_engagement::events::kIOSTabReminderScheduled,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHiOSTabGridSwipeRightForIncognito.name == feature->name) {
    // The IPH of the tab grid swipe feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSIncognitoPageControlTapped,
        /*trigger_event=*/"swipe_left_for_incognito_trigger", /*used_event=*/
        feature_engagement::events::
            kIOSSwipeRightForIncognitoUsed, /*dismiss_button_tap_event=*/
        feature_engagement::events::
            kIOSSwipeRightForIncognitoIPHDismissButtonTapped);
  }

  if (kIPHiOSSwipeBackForwardFeature.name == feature->name) {
    // The IPH of the swipe back/forward feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSBackForwardButtonTapped,
        /*trigger_event=*/"swipe_back_forward_trigger", /*used_event=*/
        feature_engagement::events::kIOSSwipeBackForwardUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::kIOSSwipeBackForwardIPHDismissButtonTapped,
        /*first_run_recency_in_days=*/60);
  }

  if (kIPHiOSSwipeToolbarToChangeTabFeature.name == feature->name) {
    // The IPH of the swipe toolbar to go to adjacent tab feature.
    return CreateNewUserGestureInProductHelpConfig(
        *feature, /*action_event=*/
        feature_engagement::events::kIOSTabGridAdjacentTabTapped,
        /*trigger_event=*/"swipe_toolbar_to_change_tab_trigger", /*used_event=*/
        feature_engagement::events::kIOSSwipeToolbarToChangeTabUsed,
        /*dismiss_button_tap_event=*/
        feature_engagement::events::
            kIOSSwipeToolbarToChangeTabIPHDismissButtonTapped,
        /*first_run_recency_in_days=*/60);
  }

  if (kIPHiOSOverflowMenuCustomizationFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.used = EventConfig(
        feature_engagement::events::kIOSOverflowMenuCustomizationUsed,
        Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod);
    config.trigger =
        EventConfig("overflow_menu_customization_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed,
        Comparator(GREATER_THAN_OR_EQUAL, 2),
        feature_engagement::kMaxStoragePeriod,
        feature_engagement::kMaxStoragePeriod));
    return config;
  }

  if (kIPHiOSPageInfoRevampFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.trigger = EventConfig("IPH_iOSPageInfoRevamp_trigger",
                                 Comparator(LESS_THAN_OR_EQUAL, 3), 365, 365);
    config.used =
        EventConfig("IPH_iOSPageInfoRevamp_used", Comparator(ANY, 0), 365, 365);
    return config;
  }

  if (kIPHiOSInlineEnhancedSafeBrowsingPromoFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(LESS_THAN, 1);
    config.trigger = EventConfig("inline_enhanced_safe_browsing_promo_trigger",
                                 Comparator(LESS_THAN_OR_EQUAL, 10), 360, 360);
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kEnhancedSafeBrowsingPromoCriterionMet,
        Comparator(GREATER_THAN_OR_EQUAL, 1), 7, 360));
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kInlineEnhancedSafeBrowsingPromoClosed,
        Comparator(EQUAL, 0), 360, 360));
    config.used =
        EventConfig("inline_enhanced_safe_browsing_promo_used",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    return config;
  }

  // iOS Promo Configs are split out into a separate file, so check that too.
  if (std::optional<FeatureConfig> ios_promo_feature_config =
          GetClientSideiOSPromoFeatureConfig(feature)) {
    return ios_promo_feature_config;
  }

  if (kIPHDiscoverFeedHeaderFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("discover_feed_header_menu_iph_triggered",
                                 Comparator(EQUAL, 0), 365, 365);
    config.used = EventConfig("discover_feed_header_menu_clicked",
                              Comparator(EQUAL, 0), 365, 365);
    return config;
  }

  if (kIPHHomeCustomizationMenuFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("home_customization_menu_iph_triggered",
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used =
        EventConfig(feature_engagement::events::kHomeCustomizationMenuUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSDownloadAutoDeletionFeature.name == feature->name) {
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.trigger =
        EventConfig("download_auto_deletion_iph_trigger", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        feature_engagement::events::kIOSDownloadAutoDeletionIPHCriterionMet,
        Comparator(GREATER_THAN_OR_EQUAL, 1), 60, 360));
    config.used =
        EventConfig("download_auto_deletion_iph_used", Comparator(ANY, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;
    return config;
  }

  if (kIPHiOSSettingsInOverflowMenuBubbleFeature.name == feature->name) {
    // A config that allows the Settings-in-overflow-menu IPH to be shown to
    // users. This will be triggered a maximum of 2 times (once per day), and it
    // will stop triggering once the user opens Settings via the overflow menu.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);

    constexpr char kSettingsInOverflowTriggerEvent[] =
        "settings_in_overflow_trigger";

    // Show at most 2 times total.
    config.trigger =
        EventConfig(kSettingsInOverflowTriggerEvent, Comparator(LESS_THAN, 2),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    // Show at most once per day.
    config.event_configs.emplace(kSettingsInOverflowTriggerEvent,
                                 Comparator(EQUAL, 0), 1, 1);
    // Stop showing once the user opens settings via the overflow menu.
    config.used =
        EventConfig(events::kSettingsOnOverflowMenuUsed, Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

    return config;
  }

  if (kIPHiOSSwitchAccountsWithNTPAccountParticleDiscFeature.name ==
      feature->name) {
    // A config that allows the NTP-identity-disc IPH to be shown to users. This
    // will only triggered once.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);

    constexpr char kSwitchAccountsWithAccountParticleTrigger[] =
        "switch_accounts_with_account_particle_trigger";

    // Show only once.
    config.trigger =
        EventConfig(kSwitchAccountsWithAccountParticleTrigger,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);

    return config;
  }

  if (kIPHIOSPageActionMenu.name == feature->name) {
    // Show the promo only once when the conditions are met.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);

    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    config.trigger =
        EventConfig(feature_engagement::events::kIOSPageActionMenuIPHTrigger,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used =
        EventConfig(feature_engagement::events::kIOSPageActionMenuIPHUsed,
                    Comparator(EQUAL, 0), feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    return config;
  }

  if (kIPHiOSPinMostVisitedSiteFeature.name == feature->name) {
    // Show the in-product help if 1) it has never been triggered, and 2) user
    // has not pinned any site to the most visited tile.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger =
        EventConfig("ios_pin_mvt_site_triggered", Comparator(EQUAL, 0),
                    feature_engagement::kMaxStoragePeriod,
                    feature_engagement::kMaxStoragePeriod);
    config.used = EventConfig(events::kIOSPinMVTSiteUsed, Comparator(EQUAL, 0),
                              feature_engagement::kMaxStoragePeriod,
                              feature_engagement::kMaxStoragePeriod);
    return config;
  }

#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_CHROMEOS)
  if (kIPHLauncherSearchHelpUiFeature.name == feature->name) {
    // A config that allows the ChromeOS Ash Launcher search IPH to be shown.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(ANY, 0);
    config.session_rate = Comparator(ANY, 0);
    config.session_rate_impact.type = SessionRateImpact::Type::NONE;
    config.blocked_by.type = BlockedBy::Type::NONE;
    config.blocking.type = Blocking::Type::NONE;

    // Can be shown any time until the `assistant_click` event is recorded.
    config.trigger =
        EventConfig("IPH_LauncherSearchHelpUi_trigger", Comparator(ANY, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.used =
        EventConfig("IPH_LauncherSearchHelpUi_chip_click", Comparator(ANY, 0),
                    kMaxStoragePeriod, kMaxStoragePeriod);
    config.event_configs.insert(EventConfig(
        "IPH_LauncherSearchHelpUi_assistant_click", Comparator(EQUAL, 0),
        kMaxStoragePeriod, kMaxStoragePeriod));
    return config;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (kIPHDummyFeature.name == feature->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    FeatureConfig config;
    config.valid = true;
    config.availability = Comparator(LESS_THAN, 0);
    config.session_rate = Comparator(LESS_THAN, 0);
    config.trigger = EventConfig("dummy_feature_iph_trigger",
                                 Comparator(LESS_THAN, 0), 1, 1);
    config.used =
        EventConfig("dummy_feature_action", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return std::nullopt;
}

}  // namespace feature_engagement
