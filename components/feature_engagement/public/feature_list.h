// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_

#include <vector>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/webui/flags/feature_entry.h"
#include "extensions/buildflags/buildflags.h"

namespace feature_engagement {
using FeatureVector = std::vector<const base::Feature*>;

// The param name for the FeatureVariation configuration, which is used by
// chrome://flags to set the variable name for the selected feature. The Tracker
// backend will then read this to figure out which feature (if any) was selected
// by the end user.
inline constexpr char kIPHDemoModeFeatureChoiceParam[] = "chosen_feature";

// Defines a const flags_ui::FeatureEntry::FeatureParam for the given
// base::Feature. The constant name will be on the form
// kFooFeature --> kFooFeatureVariation. The |feature_name| argument must
// match the base::Feature::name member of the |base_feature|.
// This is intended to be used with VARIATION_ENTRY below to be able to insert
// it into an array of flags_ui::FeatureEntry::FeatureVariation.
#define DEFINE_VARIATION_PARAM(base_feature, feature_name)     \
  using validate_##base_feature##_t = decltype(&base_feature); \
  inline constexpr flags_ui::FeatureEntry::FeatureParam        \
      base_feature##Variation[] = {                            \
          {kIPHDemoModeFeatureChoiceParam, feature_name}}

// Defines a single flags_ui::FeatureEntry::FeatureVariation entry, fully
// enclosed. This is intended to be used with the declaration of
// |kIPHDemoModeChoiceVariations| below.
#define VARIATION_ENTRY(base_feature) \
  {base_feature##Variation[0].param_value, base_feature##Variation, nullptr}

// Defines a flags_ui::FeatureEntry::FeatureParam for each feature.
DEFINE_VARIATION_PARAM(kIPHDummyFeature, "IPH_Dummy");
#if BUILDFLAG(IS_ANDROID)
// DEFINE_VARIATION_PARAM_ANDROID_START
DEFINE_VARIATION_PARAM(kIPHAndroidBottomBarNewTab,
                       "IPH_AndroidBottomBarNewTab");
DEFINE_VARIATION_PARAM(kIPHAndroidBottomBarPromoDialog,
                       "IPH_AndroidBottomBarPromoDialog");
DEFINE_VARIATION_PARAM(kIPHAndroidTabDeclutter, "IPH_AndroidTabDeclutter");
DEFINE_VARIATION_PARAM(kIPHAndroidVerticalTabsPromoFeature,
                       "IPH_AndroidVerticalTabsPromo");
DEFINE_VARIATION_PARAM(kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
                       "IPH_AdaptiveButtonInTopToolbarCustomization_NewTab");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationOpenInBrowserFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_OpenInBrowser");
DEFINE_VARIATION_PARAM(kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
                       "IPH_AdaptiveButtonInTopToolbarCustomization_Share");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_VoiceSearch");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_ReadAloud");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_PageSummary_Web");
DEFINE_VARIATION_PARAM(
    kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature,
    "IPH_AdaptiveButtonInTopToolbarCustomization_PageSummary_Pdf");
DEFINE_VARIATION_PARAM(kIPHPageSummaryWebMenuFeature, "IPH_PageSummaryWebMenu");
DEFINE_VARIATION_PARAM(kIPHPageSummaryPdfMenuFeature, "IPH_PageSummaryPdfMenu");
DEFINE_VARIATION_PARAM(kIPHAutoDarkOptOutFeature, "IPH_AutoDarkOptOut");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageFeature,
                       "IPH_AutoDarkUserEducationMessage");
DEFINE_VARIATION_PARAM(kIPHAutoDarkUserEducationMessageOptInFeature,
                       "IPH_AutoDarkUserEducationMessageOptIn");
DEFINE_VARIATION_PARAM(kIPHAppRatingPromptFeature, "IPH_AppRatingPrompt");
DEFINE_VARIATION_PARAM(kIPHAppSpecificHistory, "IPH_AppSpecificHistory");
DEFINE_VARIATION_PARAM(kIPHCCTHistory, "IPH_CCTHistory");
DEFINE_VARIATION_PARAM(kIPHCCTMinimized, "IPH_CCTMinimized");
DEFINE_VARIATION_PARAM(kIPHContextualPageActionsQuietVariantFeature,
                       "IPH_ContextualPageActions_QuietVariant");
DEFINE_VARIATION_PARAM(kIPHContextualPageActionsActionChipFeature,
                       "IPH_ContextualPageActions_ActionChip");
DEFINE_VARIATION_PARAM(kIPHDataSaverDetailFeature, "IPH_DataSaverDetail");
DEFINE_VARIATION_PARAM(kIPHDataSaverMilestonePromoFeature,
                       "IPH_DataSaverMilestonePromo");
DEFINE_VARIATION_PARAM(kIPHDataSaverPreviewFeature, "IPH_DataSaverPreview");
DEFINE_VARIATION_PARAM(kIPHDownloadHomeFeature, "IPH_DownloadHome");
DEFINE_VARIATION_PARAM(kIPHDownloadIndicatorFeature, "IPH_DownloadIndicator");
DEFINE_VARIATION_PARAM(kIPHDownloadPageFeature, "IPH_DownloadPage");
DEFINE_VARIATION_PARAM(kIPHDownloadPageScreenshotFeature,
                       "IPH_DownloadPageScreenshot");
DEFINE_VARIATION_PARAM(kIPHChromeHomeExpandFeature, "IPH_ChromeHomeExpand");
DEFINE_VARIATION_PARAM(kIPHChromeHomePullToRefreshFeature,
                       "IPH_ChromeHomePullToRefresh");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification1Feature,
                       "IPH_ChromeReengagementNotification1");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification2Feature,
                       "IPH_ChromeReengagementNotification2");
DEFINE_VARIATION_PARAM(kIPHChromeReengagementNotification3Feature,
                       "IPH_ChromeReengagementNotification3");
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoMagicStackFeature,
                       "IPH_DefaultBrowserPromoMagicStack");
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoMessagesFeature,
                       "IPH_DefaultBrowserPromoMessages");
DEFINE_VARIATION_PARAM(kIPHDefaultBrowserPromoSettingCardFeature,
                       "IPH_DefaultBrowserPromoSettingCard");
DEFINE_VARIATION_PARAM(kIPHDownloadSettingsFeature, "IPH_DownloadSettings");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadContinuingFeature,
                       "IPH_DownloadInfoBarDownloadContinuing");
DEFINE_VARIATION_PARAM(kIPHDownloadInfoBarDownloadsAreFasterFeature,
                       "IPH_DownloadInfoBarDownloadsAreFaster");
DEFINE_VARIATION_PARAM(kIPHEphemeralTabFeature, "IPH_EphemeralTab");
DEFINE_VARIATION_PARAM(kIPHExtensionsManageAppMenuFeature,
                       "IPH_ExtensionsManageAppMenu");
DEFINE_VARIATION_PARAM(kIPHExtensionsManageToolbarFeature,
                       "IPH_ExtensionsManageToolbar");
DEFINE_VARIATION_PARAM(kIPHFeedCardMenuFeature, "IPH_FeedCardMenu");
DEFINE_VARIATION_PARAM(kIPHFuseboxAttachmentFeature, "IPH_FuseboxAttachment");
DEFINE_VARIATION_PARAM(kIPHGenericAlwaysTriggerHelpUiFeature,
                       "IPH_GenericAlwaysTriggerHelpUiFeature");
DEFINE_VARIATION_PARAM(kIPHGestureUserEducation, "IPH_GestureUserEducation");
DEFINE_VARIATION_PARAM(kIPHIdentityDiscFeature, "IPH_IdentityDisc");
DEFINE_VARIATION_PARAM(kIPHIncognitoIndicatorCloseAllWindows,
                       "IPH_IncognitoIndicatorCloseAllWindows");
DEFINE_VARIATION_PARAM(kIPHInstanceSwitcherFeature, "IPH_InstanceSwitcher");
DEFINE_VARIATION_PARAM(kIPHRecentTabsFeature, "IPH_RecentTabs");
DEFINE_VARIATION_PARAM(kIPHKeyboardAccessoryBarSwipingFeature,
                       "IPH_KeyboardAccessoryBarSwiping");
DEFINE_VARIATION_PARAM(kIPHMicToolbarFeature, "IPH_MicToolbar");
DEFINE_VARIATION_PARAM(kIPHMenuAddToGroup, "IPH_MenuAddToGroup");
DEFINE_VARIATION_PARAM(kIPHMostVisitedTilesCustomizationPinFeature,
                       "IPH_MostVisitedTilesCustomizationPin");
DEFINE_VARIATION_PARAM(kIPHPageInfoFeature, "IPH_PageInfo");
DEFINE_VARIATION_PARAM(kIPHPageInfoStoreInfoFeature, "IPH_PageInfoStoreInfo");
DEFINE_VARIATION_PARAM(kIPHPageZoomFeature, "IPH_PageZoom");
DEFINE_VARIATION_PARAM(kIPHPdfPageDownloadFeature, "IPH_PdfPageDownload");
DEFINE_VARIATION_PARAM(kIPHPreviewsOmniboxUIFeature, "IPH_PreviewsOmniboxUI");
DEFINE_VARIATION_PARAM(kIPHReadAloudAppMenuFeature,
                       "IPH_ReadAloudAppMenuFeature");
DEFINE_VARIATION_PARAM(kIPHReadAloudExpandedPlayerFeature,
                       "IPH_ReadAloudExpandedPlayerFeature");
DEFINE_VARIATION_PARAM(kIPHReadAloudPlaybackModeFeature,
                       "IPH_ReadAloudPlaybackModeFeature");
DEFINE_VARIATION_PARAM(kIPHReaderModeDistillInAppFeature,
                       "IPH_ReaderModeDistillInApp");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteDefaultOnFeature,
                       "IPH_RequestDesktopSiteDefaultOn");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteExceptionsGenericFeature,
                       "IPH_RequestDesktopSiteExceptionsGeneric");
DEFINE_VARIATION_PARAM(kIPHRequestDesktopSiteWindowSettingFeature,
                       "IPH_RequestDesktopSiteWindowSetting");
DEFINE_VARIATION_PARAM(kIPHSiteControlsFeature, "IPH_SiteControls");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherAddToGroup, "IPH_TabSwitcherAddToGroup");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherButtonFeature, "IPH_TabSwitcherButton");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherButtonSwitchIncognitoFeature,
                       "IPH_TabSwitcherButtonSwitchIncognito");
DEFINE_VARIATION_PARAM(kIPHThreeDotMenuBackButton,
                       "IPH_ThreeDotMenuBackButton");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPChromeIntroFeature,
                       "IPH_VideoTutorial_NTP_ChromeIntro");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPDownloadFeature,
                       "IPH_VideoTutorial_NTP_Download");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPSearchFeature,
                       "IPH_VideoTutorial_NTP_Search");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPVoiceSearchFeature,
                       "IPH_VideoTutorial_NTP_VoiceSearch");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialNTPSummaryFeature,
                       "IPH_VideoTutorial_NTP_Summary");
DEFINE_VARIATION_PARAM(kIPHVideoTutorialTryNowFeature,
                       "IPH_VideoTutorial_TryNow");
DEFINE_VARIATION_PARAM(kIPHExploreSitesTileFeature, "IPH_ExploreSitesTile");
DEFINE_VARIATION_PARAM(kIPHFeedHeaderMenuFeature, "IPH_FeedHeaderMenu");
DEFINE_VARIATION_PARAM(kIPHFeedSwipeRefresh, "IPH_FeedSwipeRefresh");
DEFINE_VARIATION_PARAM(kIPHShareScreenshotFeature, "IPH_ShareScreenshot");
DEFINE_VARIATION_PARAM(kIPHSharingHubLinkToggleFeature,
                       "IPH_SharingHubLinkToggle");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingBuilder,
                       "IPH_SharedHighlightingBuilder");
DEFINE_VARIATION_PARAM(kIPHSharedHighlightingReceiverFeature,
                       "IPH_SharedHighlightingReceiver");
DEFINE_VARIATION_PARAM(kIPHSharingHubWebnotesStylizeFeature,
                       "IPH_SharingHubWebnotesStylize");
DEFINE_VARIATION_PARAM(kIPHRestoreTabsOnFREFeature, "IPH_RestoreTabsOnFRE");
DEFINE_VARIATION_PARAM(kIPHTabSwitcherXR, "IPH_TabSwitcherXR");
DEFINE_VARIATION_PARAM(kIPHTabTearingXR, "IPH_TabTearingXR");
// DEFINE_VARIATION_PARAM_ANDROID_END
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHBottomToolbarTipFeature, "IPH_BottomToolbarTip");
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
DEFINE_VARIATION_PARAM(kIPHiOSActiveDaysTrackingFeature,
                       "IPH_iOSActiveDaysTrackingFeature");
DEFINE_VARIATION_PARAM(kIPHLongPressToolbarTipFeature,
                       "IPH_LongPressToolbarTip");
DEFINE_VARIATION_PARAM(kIPHBadgedReaderModeFeature, "IPH_BadgedReaderMode");
DEFINE_VARIATION_PARAM(kIPHiOSReaderModeLargeOmniboxEntrypointFeature,
                       "IPH_iOSReaderModeLargeOmniboxEntrypoint");
DEFINE_VARIATION_PARAM(kIPHBadgedReadingListFeature, "IPH_BadgedReadingList");
DEFINE_VARIATION_PARAM(kIPHReadingListMessagesFeature,
                       "IPH_ReadingListMessages");
DEFINE_VARIATION_PARAM(kIPHDiscoverFeedHeaderFeature,
                       "IPH_DiscoverFeedHeaderMenu");
DEFINE_VARIATION_PARAM(kIPHDefaultSiteViewFeature, "IPH_DefaultSiteView");
DEFINE_VARIATION_PARAM(kIPHFollowWhileBrowsingFeature,
                       "IPH_FollowWhileBrowsing");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
                       "IPH_iOSDefaultBrowserOverflowMenuBadge");
DEFINE_VARIATION_PARAM(kIPHiOSFeedSwipeAnimatedFeature,
                       "IPH_iOSFeedSwipeAnimatedFeature");
DEFINE_VARIATION_PARAM(kIPHiOSFeedSwipeStaticFeature,
                       "IPH_iOSFeedSwipeStaticFeature");
DEFINE_VARIATION_PARAM(kIPHiOSBackendPromoFeature,
                       "IPH_iOSBackendPromoFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPromoAppStoreFeature, "IPH_iOSPromoAppStore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoBackgroundCustomizationFeature,
                       "IPH_iOSPromoBackgroundCustomization");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreFeature,
                       "IPH_iOSPromoPostRestore");
DEFINE_VARIATION_PARAM(kIPHiOSPromoDefaultBrowserReminderFeature,
                       "IPH_iOSPromoDefaultBrowserReminder");
DEFINE_VARIATION_PARAM(kIPHiOSHistoryOnOverflowMenuFeature,
                       "IPH_iOSHistoryOnOverflowMenuFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPromoPostRestoreDefaultBrowserFeature,
                       "IPH_iOSPromoPostRestoreDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalUrlPasteDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalAppSwitcherDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoNonModalShareDefaultBrowserFeature,
                       "IPH_iOSPromoNonModalShareDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPullToRefreshFeature,
                       "IPH_iOSPullToRefreshFeature");
DEFINE_VARIATION_PARAM(kIPHiOSTabGridSwipeRightForIncognito,
                       "IPH_iOSTabGridSwipeRightForIncognito");
DEFINE_VARIATION_PARAM(kIPHiOSDockingPromoFeature, "IPH_iOSDockingPromo");
DEFINE_VARIATION_PARAM(kIPHiOSDockingPromoEligibilityFeature,
                       "IPH_iOSDockingPromoEligibility");
DEFINE_VARIATION_PARAM(kIPHiOSPromoAllTabsFeature, "IPH_iOSPromoAllTabs");
DEFINE_VARIATION_PARAM(kIPHiOSPromoMadeForIOSFeature, "IPH_iOSPromoMadeForIOS");
DEFINE_VARIATION_PARAM(kIPHiOSPromoStaySafeFeature, "IPH_iOSPromoStaySafe");
DEFINE_VARIATION_PARAM(kIPHiOSSwipeBackForwardFeature,
                       "IPH_iOSSwipeBackForward");
DEFINE_VARIATION_PARAM(kIPHiOSSwipeToolbarToChangeTabFeature,
                       "IPH_iOSSwipeToolbarToChangeTab");
DEFINE_VARIATION_PARAM(kIPHiOSPostDefaultAbandonmentPromoFeature,
                       "IPH_iOSPostDefaultAbandonmentPromo");
DEFINE_VARIATION_PARAM(kIPHiOSPromoGenericDefaultBrowserFeature,
                       "IPH_iOSPromoGenericDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoSettingsCardDefaultBrowserFeature,
                       "IPH_iOSPromoSettingsCardDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoSettingsCellDefaultBrowserFeature,
                       "IPH_iOSPromoSettingsCellDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoOverflowMenuDestinationDefaultBrowserFeature,
                       "IPH_iOSPromoOverflowMenuDestinationDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSPromoOverflowMenuShortcutsDefaultBrowserFeature,
                       "IPH_iOSPromoOverflowMenuShortcutsDefaultBrowser");
DEFINE_VARIATION_PARAM(kIPHiOSOverflowMenuCustomizationFeature,
                       "IPH_iOSOverflowMenuCustomization");
DEFINE_VARIATION_PARAM(kIPHiOSPageInfoRevampFeature, "IPH_iOSPageInfoRevamp");
DEFINE_VARIATION_PARAM(kIPHiOSInlineEnhancedSafeBrowsingPromoFeature,
                       "IPH_iOSInlineEnhancedSafeBrowsingPromo");
DEFINE_VARIATION_PARAM(kIPHiOSContextualPanelSampleModelFeature,
                       "IPH_iOSContextualPanelSampleModel");
DEFINE_VARIATION_PARAM(kIPHHomeCustomizationMenuFeature,
                       "IPH_HomeCustomizationMenu");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserBannerPromoFeature,
                       "IPH_iOSDefaultBrowserBannerPromoFeature");
DEFINE_VARIATION_PARAM(kIPHiOSDefaultBrowserOffCyclePromoFeature,
                       "IPH_iOSDefaultBrowserOffCyclePromo");
DEFINE_VARIATION_PARAM(kIPHiOSOneTimeDefaultBrowserNotificationFeature,
                       "IPH_iOSOneTimeDefaultBrowserNotification");
DEFINE_VARIATION_PARAM(kIPHiOSReminderNotificationsOverflowMenuBubbleFeature,
                       "IPH_iOSReminderNotificationsOverflowMenuBubbleFeature");
DEFINE_VARIATION_PARAM(
    kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature,
    "IPH_iOSReminderNotificationsOverflowMenuNewBadgeFeature");
DEFINE_VARIATION_PARAM(kIPHiOSDownloadAutoDeletionFeature,
                       "IPH_iOSDownloadAutoDeletion");
DEFINE_VARIATION_PARAM(kIPHiOSSettingsInOverflowMenuBubbleFeature,
                       "IPH_iOSSettingsInOverflowMenuBubbleFeature");
DEFINE_VARIATION_PARAM(
    kIPHiOSSwitchAccountsWithNTPAccountParticleDiscFeature,
    "IPH_iOSSwitchAccountsWithNTPAccountParticleDiscFeature");
DEFINE_VARIATION_PARAM(kIPHiOSWelcomeBackFeature, "IPH_iOSWelcomeBack");
DEFINE_VARIATION_PARAM(kIPHiOSSafariImportFeature,
                       "IPH_iOSSafariImportFeature");
DEFINE_VARIATION_PARAM(kIPHIOSPageActionMenu, "IPH_iOSPageActionMenu");
DEFINE_VARIATION_PARAM(kIPHiOSHomepageCustomizationNewBadge,
                       "IPH_iOSHomepageCustomizationNewBadge");
DEFINE_VARIATION_PARAM(kIPHiOSNewIAPromoFeature, "IPH_iOSNewIAPromoFeature");
DEFINE_VARIATION_PARAM(kIPHiOSPinMostVisitedSiteFeature,
                       "IPH_iOSPinMostVisitedSiteFeature");
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
DEFINE_VARIATION_PARAM(kEsbDownloadRowPromoFeature, "EsbDownloadRowPromo");
#endif
DEFINE_VARIATION_PARAM(kIPHBatterySaverModeFeature, "IPH_BatterySaverMode");
DEFINE_VARIATION_PARAM(kIPHCriticalActionAppMenuFeature,
                       "IPH_CriticalActionAppMenu");
DEFINE_VARIATION_PARAM(kIPHCriticalActionFilterChipFeature,
                       "IPH_CriticalActionFilterChip");
DEFINE_VARIATION_PARAM(kIPHDesktopRealboxContextualSearchFeature,
                       "IPH_DesktopRealboxContextualSearchFeature");
DEFINE_VARIATION_PARAM(kIPHDiscardRingFeature, "IPH_DiscardRing");
DEFINE_VARIATION_PARAM(kIPHDownloadEsbPromoFeature, "IPH_DownloadEsbPromo");
DEFINE_VARIATION_PARAM(kIPHExplicitBrowserSigninPreferenceRememberedFeature,
                       "IPH_ExplicitBrowserSigninPreferenceRemembered");
DEFINE_VARIATION_PARAM(kIPHHistorySearchFeature, "IPH_HistorySearch");
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
DEFINE_VARIATION_PARAM(kIPHExtensionsMenuFeature, "IPH_ExtensionsMenu");
DEFINE_VARIATION_PARAM(kIPHExtensionsRequestAccessButtonFeature,
                       "IPH_ExtensionsRequestAccessButton");
DEFINE_VARIATION_PARAM(kIPHExtensionsZeroStatePromoFeature,
                       "IPH_ExtensionsZeroStatePromo");
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// The feature is used in Finch experiments so it is unable to be renamed
// alongside the variable name.
DEFINE_VARIATION_PARAM(kIPHMemorySaverModeFeature, "IPH_HighEfficiencyMode");
DEFINE_VARIATION_PARAM(kIPHMultistepFilterPromoFeature,
                       "IPH_MultistepFilterPromo");
DEFINE_VARIATION_PARAM(kIPHPasswordsSavePrimingPromoFeature,
                       "IPH_PasswordsSavePrimingPromo");
DEFINE_VARIATION_PARAM(kIPHPdfInkSignaturesFeature, "IPH_PdfInkSignatures");
DEFINE_VARIATION_PARAM(kIPHPdfSearchifyFeature, "IPH_PdfSearchifyFeature");
DEFINE_VARIATION_PARAM(kIPHPdfTextAnnotationsFeature, "IPH_PdfTextAnnotations");
DEFINE_VARIATION_PARAM(kIPHPerformanceInterventionDialogFeature,
                       "IPH_PerformanceInterventionDialogFeature");
DEFINE_VARIATION_PARAM(kIPHPowerBookmarksSidePanelFeature,
                       "IPH_PowerBookmarksSidePanel");
DEFINE_VARIATION_PARAM(kIPHReadingListDiscoveryFeature,
                       "IPH_ReadingListDiscovery");
DEFINE_VARIATION_PARAM(kIPHReadingListEntryPointFeature,
                       "IPH_ReadingListEntryPoint");
DEFINE_VARIATION_PARAM(kIPHReadingListInSidePanelFeature,
                       "IPH_ReadingListInSidePanel");
DEFINE_VARIATION_PARAM(kIPHReadingModeKeyboardShortcutFeature,
                       "IPH_ReadingModeKeyboardShortcut");
DEFINE_VARIATION_PARAM(kIPHReadingModePageActionLabelFeature,
                       "IPH_ReadingModePageActionLabel");
DEFINE_VARIATION_PARAM(kIPHReadingModePresentationModeFeature,
                       "IPH_ReadingModePresentationMode");
DEFINE_VARIATION_PARAM(kIPHReadingModeSidePanelFeature,
                       "IPH_ReadingModeSidePanel");
DEFINE_VARIATION_PARAM(kIPHSideBySidePinnableFeature,
                       "IPH_SideBySidePinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSideBySideTabSwitchFeature,
                       "IPH_SideBySideTabSwitchFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelGenericPinnableFeature,
                       "IPH_SidePanelGenericPinnableFeature");
DEFINE_VARIATION_PARAM(kIPHSidePanelLensOverlayPinnableFeature,
                       "IPH_SidePanelLensOverlayPinnableFeature");

DEFINE_VARIATION_PARAM(kIPHSmartTabSharingDefaultOnFeature,
                       "IPH_SmartTabSharingDefaultOnFeature");
DEFINE_VARIATION_PARAM(kIPHSmartTabSharingFeature,
                       "IPH_SmartTabSharingFeature");
DEFINE_VARIATION_PARAM(kIPHSmartTabSharingTryItFeature,
                       "IPH_SmartTabSharingTryItFeature");

DEFINE_VARIATION_PARAM(kIPHSplitViewHorizontalIndirectAccessFeature,
                       "IPH_SplitViewHorizontalIndirectAccessFeature");
DEFINE_VARIATION_PARAM(kIPHVerticalTabsExpandOnHoverFeature,
                       "IPH_VerticalTabsExpandOnHoverFeature");
DEFINE_VARIATION_PARAM(kIPHVerticalTabstripTutorialFeature,
                       "IPH_VerticalTabstripTutorialFeature");

DEFINE_VARIATION_PARAM(kIPHPwaQuietNotificationFeature,
                       "IPH_PwaQuietNotification");
DEFINE_VARIATION_PARAM(kIPHTabAudioMutingFeature, "IPH_TabAudioMuting");
DEFINE_VARIATION_PARAM(kIPHTabSearchComboButtonFeature,
                       "IPH_TabSearchComboButton");
DEFINE_VARIATION_PARAM(kIPHTabScrollButtonFeature,
                       "IPH_TabScrollButtonFeature");
DEFINE_VARIATION_PARAM(kIPHDesktopPwaInstallFeature, "IPH_DesktopPwaInstall");
DEFINE_VARIATION_PARAM(kIPHProfileSwitchFeature, "IPH_ProfileSwitch");
DEFINE_VARIATION_PARAM(kIPHDesktopSharedHighlightingFeature,
                       "IPH_DesktopSharedHighlighting");
DEFINE_VARIATION_PARAM(kIPHWebUiHelpBubbleTestFeature,
                       "IPH_WebUiHelpBubbleTest");
DEFINE_VARIATION_PARAM(kIPHPriceTrackingInSidePanelFeature,
                       "IPH_PriceTrackingInSidePanel");
DEFINE_VARIATION_PARAM(kIPHBackNavigationMenuFeature, "IPH_BackNavigationMenu");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
DEFINE_VARIATION_PARAM(kIPHAutofillCreditCardBenefitFeature,
                       "IPH_AutofillCreditCardBenefit");

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
DEFINE_VARIATION_PARAM(kIPHGrowthFramework, "IPH_GrowthFramework");
DEFINE_VARIATION_PARAM(kIPHGoogleOneOfferNotificationFeature,
                       "IPH_GoogleOneOfferNotification");
DEFINE_VARIATION_PARAM(kIPHLauncherSearchHelpUiFeature,
                       "IPH_LauncherSearchHelpUi");
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunch,
                       "IPH_DesktopPWAsLinkCapturingLaunch");
DEFINE_VARIATION_PARAM(kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
                       "IPH_DesktopPWAsLinkCapturingLaunchAppInTab");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
DEFINE_VARIATION_PARAM(kIPHSignInBenefitsFeature, "IPH_SignInBenefits");
DEFINE_VARIATION_PARAM(kIPHSupervisedUserProfileSigninFeature,
                       "IPH_SupervisedUserProfileSignin");
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if !BUILDFLAG(IS_ANDROID)
DEFINE_VARIATION_PARAM(kIPHiOSEnhancedBrowsingDesktopFeature,
                       "IPH_iOSEnhancedBrowsingDesktop");
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_IOS)
DEFINE_VARIATION_PARAM(kIPHResumptionRailFeature, "IPH_ResumptionRail");
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN)
DEFINE_VARIATION_PARAM(kIPHSearchPromotionFeature,
                       "IPH_SearchPromotionFeature");
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
DEFINE_VARIATION_PARAM(kIPHExtensionsPinnedByDefaultFeature,
                       "IPH_ExtensionsPinnedByDefault");
#endif

// Defines the array of which features should be listed in the chrome://flags
// UI to be able to select them alone for demo-mode. The features listed here
// are possible to enable on their own in demo mode.
inline constexpr flags_ui::FeatureEntry::FeatureVariation
    kIPHDemoModeChoiceVariations[] = {
// clang-format off
#if BUILDFLAG(IS_ANDROID)
// VARIATION_ENTRY_ANDROID_START
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature),
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature),
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature),
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature),
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature),
        VARIATION_ENTRY(kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature),
        VARIATION_ENTRY(kIPHAndroidBottomBarNewTab),
        VARIATION_ENTRY(kIPHAndroidBottomBarPromoDialog),
        VARIATION_ENTRY(kIPHAndroidTabDeclutter),
        VARIATION_ENTRY(kIPHAndroidVerticalTabsPromoFeature),
        VARIATION_ENTRY(kIPHAppRatingPromptFeature),
        VARIATION_ENTRY(kIPHAppSpecificHistory),
        VARIATION_ENTRY(kIPHAutoDarkOptOutFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageFeature),
        VARIATION_ENTRY(kIPHAutoDarkUserEducationMessageOptInFeature),
        VARIATION_ENTRY(kIPHCCTHistory),
        VARIATION_ENTRY(kIPHCCTMinimized),
        VARIATION_ENTRY(kIPHChromeHomeExpandFeature),
        VARIATION_ENTRY(kIPHChromeHomePullToRefreshFeature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification1Feature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification2Feature),
        VARIATION_ENTRY(kIPHChromeReengagementNotification3Feature),
        VARIATION_ENTRY(kIPHContextualPageActionsActionChipFeature),
        VARIATION_ENTRY(kIPHContextualPageActionsQuietVariantFeature),
        VARIATION_ENTRY(kIPHDataSaverDetailFeature),
        VARIATION_ENTRY(kIPHDataSaverMilestonePromoFeature),
        VARIATION_ENTRY(kIPHDataSaverPreviewFeature),
        VARIATION_ENTRY(kIPHDefaultBrowserPromoMagicStackFeature),
        VARIATION_ENTRY(kIPHDefaultBrowserPromoMessagesFeature),
        VARIATION_ENTRY(kIPHDefaultBrowserPromoSettingCardFeature),
        VARIATION_ENTRY(kIPHDownloadHomeFeature),
        VARIATION_ENTRY(kIPHDownloadIndicatorFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadContinuingFeature),
        VARIATION_ENTRY(kIPHDownloadInfoBarDownloadsAreFasterFeature),
        VARIATION_ENTRY(kIPHDownloadPageFeature),
        VARIATION_ENTRY(kIPHDownloadPageScreenshotFeature),
        VARIATION_ENTRY(kIPHDownloadSettingsFeature),
        VARIATION_ENTRY(kIPHEphemeralTabFeature),
        VARIATION_ENTRY(kIPHExploreSitesTileFeature),
        VARIATION_ENTRY(kIPHExtensionsManageAppMenuFeature),
        VARIATION_ENTRY(kIPHExtensionsManageToolbarFeature),
        VARIATION_ENTRY(kIPHFeedCardMenuFeature),
        VARIATION_ENTRY(kIPHFeedHeaderMenuFeature),
        VARIATION_ENTRY(kIPHFeedSwipeRefresh),
        VARIATION_ENTRY(kIPHGestureUserEducation),
        VARIATION_ENTRY(kIPHIdentityDiscFeature),
        VARIATION_ENTRY(kIPHIncognitoIndicatorCloseAllWindows),
        VARIATION_ENTRY(kIPHInstanceSwitcherFeature),
        VARIATION_ENTRY(kIPHKeyboardAccessoryBarSwipingFeature),
        VARIATION_ENTRY(kIPHMenuAddToGroup),
        VARIATION_ENTRY(kIPHMicToolbarFeature),
        VARIATION_ENTRY(kIPHMostVisitedTilesCustomizationPinFeature),
        VARIATION_ENTRY(kIPHPageInfoFeature),
        VARIATION_ENTRY(kIPHPageInfoStoreInfoFeature),
        VARIATION_ENTRY(kIPHPageSummaryPdfMenuFeature),
        VARIATION_ENTRY(kIPHPageSummaryWebMenuFeature),
        VARIATION_ENTRY(kIPHPageZoomFeature),
        VARIATION_ENTRY(kIPHPdfPageDownloadFeature),
        VARIATION_ENTRY(kIPHPreviewsOmniboxUIFeature),
        VARIATION_ENTRY(kIPHReadAloudAppMenuFeature),
        VARIATION_ENTRY(kIPHReadAloudExpandedPlayerFeature),
        VARIATION_ENTRY(kIPHReaderModeDistillInAppFeature),
        VARIATION_ENTRY(kIPHRecentTabsFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteDefaultOnFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteExceptionsGenericFeature),
        VARIATION_ENTRY(kIPHRequestDesktopSiteWindowSettingFeature),
        VARIATION_ENTRY(kIPHRestoreTabsOnFREFeature),
        VARIATION_ENTRY(kIPHSharedHighlightingBuilder),
        VARIATION_ENTRY(kIPHSharedHighlightingReceiverFeature),
        VARIATION_ENTRY(kIPHShareScreenshotFeature),
        VARIATION_ENTRY(kIPHSharingHubLinkToggleFeature),
        VARIATION_ENTRY(kIPHSharingHubWebnotesStylizeFeature),
        VARIATION_ENTRY(kIPHSiteControlsFeature),
        VARIATION_ENTRY(kIPHTabSwitcherAddToGroup),
        VARIATION_ENTRY(kIPHTabSwitcherButtonFeature),
        VARIATION_ENTRY(kIPHTabSwitcherButtonSwitchIncognitoFeature),
        VARIATION_ENTRY(kIPHTabSwitcherXR),
        VARIATION_ENTRY(kIPHTabTearingXR),
        VARIATION_ENTRY(kIPHThreeDotMenuBackButton),
        VARIATION_ENTRY(kIPHVideoTutorialNTPChromeIntroFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPDownloadFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPSearchFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPSummaryFeature),
        VARIATION_ENTRY(kIPHVideoTutorialNTPVoiceSearchFeature),
        VARIATION_ENTRY(kIPHVideoTutorialTryNowFeature),
// keep-sorted end
// VARIATION_ENTRY_ANDROID_END
#else
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHiOSEnhancedBrowsingDesktopFeature),
// keep-sorted end
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
        VARIATION_ENTRY(kIPHBottomToolbarTipFeature),
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHBadgedReadingListFeature),
        VARIATION_ENTRY(kIPHDefaultSiteViewFeature),
        VARIATION_ENTRY(kIPHDiscoverFeedHeaderFeature),
        VARIATION_ENTRY(kIPHFollowWhileBrowsingFeature),
        VARIATION_ENTRY(kIPHHomeCustomizationMenuFeature),
        VARIATION_ENTRY(kIPHiOSBackendPromoFeature),
        VARIATION_ENTRY(kIPHiOSContextualPanelSampleModelFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserBannerPromoFeature),
        VARIATION_ENTRY(kIPHiOSDefaultBrowserOverflowMenuBadgeFeature),
        VARIATION_ENTRY(kIPHiOSDockingPromoFeature),
        VARIATION_ENTRY(kIPHiOSDownloadAutoDeletionFeature),
        VARIATION_ENTRY(kIPHiOSHistoryOnOverflowMenuFeature),
        VARIATION_ENTRY(kIPHiOSNewIAPromoFeature),
        VARIATION_ENTRY(kIPHiOSOverflowMenuCustomizationFeature),
        VARIATION_ENTRY(kIPHIOSPageActionMenu),
        VARIATION_ENTRY(kIPHiOSPinMostVisitedSiteFeature),
        VARIATION_ENTRY(kIPHiOSPostDefaultAbandonmentPromoFeature),
        VARIATION_ENTRY(kIPHiOSPromoAllTabsFeature),
        VARIATION_ENTRY(kIPHiOSPromoAppStoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoBackgroundCustomizationFeature),
        VARIATION_ENTRY(kIPHiOSPromoGenericDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoMadeForIOSFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalShareDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature),
        VARIATION_ENTRY(
            kIPHiOSPromoOverflowMenuDestinationDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoOverflowMenuShortcutsDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoPostRestoreFeature),
        VARIATION_ENTRY(kIPHiOSPromoSettingsCardDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoSettingsCellDefaultBrowserFeature),
        VARIATION_ENTRY(kIPHiOSPromoStaySafeFeature),
        VARIATION_ENTRY(kIPHiOSPullToRefreshFeature),
        VARIATION_ENTRY(kIPHiOSReminderNotificationsOverflowMenuBubbleFeature),
        VARIATION_ENTRY(kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature),
        VARIATION_ENTRY(kIPHiOSSafariImportFeature),
        VARIATION_ENTRY(kIPHiOSSwipeBackForwardFeature),
        VARIATION_ENTRY(kIPHiOSSwipeToolbarToChangeTabFeature),
        VARIATION_ENTRY(kIPHiOSTabGridSwipeRightForIncognito),
        VARIATION_ENTRY(kIPHiOSWelcomeBackFeature),
        VARIATION_ENTRY(kIPHLongPressToolbarTipFeature),
        VARIATION_ENTRY(kIPHReadingListMessagesFeature),
// keep-sorted end
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
        VARIATION_ENTRY(kIPHExtensionsMenuFeature),
        VARIATION_ENTRY(kIPHExtensionsRequestAccessButtonFeature),
#endif
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHBackNavigationMenuFeature),
        VARIATION_ENTRY(kIPHBatterySaverModeFeature),
        VARIATION_ENTRY(kIPHCriticalActionAppMenuFeature),
        VARIATION_ENTRY(kIPHCriticalActionFilterChipFeature),
        VARIATION_ENTRY(kIPHDesktopPwaInstallFeature),
        VARIATION_ENTRY(kIPHDesktopRealboxContextualSearchFeature),
        VARIATION_ENTRY(kIPHDesktopSharedHighlightingFeature),
        VARIATION_ENTRY(kIPHDiscardRingFeature),
        VARIATION_ENTRY(kIPHExplicitBrowserSigninPreferenceRememberedFeature),
        VARIATION_ENTRY(kIPHHistorySearchFeature),
        VARIATION_ENTRY(kIPHMemorySaverModeFeature),
        VARIATION_ENTRY(kIPHMultistepFilterPromoFeature),
        VARIATION_ENTRY(kIPHPdfInkSignaturesFeature),
        VARIATION_ENTRY(kIPHPdfSearchifyFeature),
        VARIATION_ENTRY(kIPHPdfTextAnnotationsFeature),
        VARIATION_ENTRY(kIPHPerformanceInterventionDialogFeature),
        VARIATION_ENTRY(kIPHPowerBookmarksSidePanelFeature),
        VARIATION_ENTRY(kIPHPriceTrackingInSidePanelFeature),
        VARIATION_ENTRY(kIPHProfileSwitchFeature),
        VARIATION_ENTRY(kIPHPwaQuietNotificationFeature),
        VARIATION_ENTRY(kIPHReadingListDiscoveryFeature),
        VARIATION_ENTRY(kIPHReadingListEntryPointFeature),
        VARIATION_ENTRY(kIPHReadingListInSidePanelFeature),
        VARIATION_ENTRY(kIPHReadingModeKeyboardShortcutFeature),
        VARIATION_ENTRY(kIPHReadingModePageActionLabelFeature),
        VARIATION_ENTRY(kIPHReadingModePresentationModeFeature),
        VARIATION_ENTRY(kIPHReadingModeSidePanelFeature),
        VARIATION_ENTRY(kIPHResumptionRailFeature),
        VARIATION_ENTRY(kIPHSideBySidePinnableFeature),
        VARIATION_ENTRY(kIPHSideBySideTabSwitchFeature),
        VARIATION_ENTRY(kIPHSidePanelGenericPinnableFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingDefaultOnFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingFeature),
        VARIATION_ENTRY(kIPHSmartTabSharingTryItFeature),
        VARIATION_ENTRY(kIPHSplitViewHorizontalIndirectAccessFeature),
        VARIATION_ENTRY(kIPHTabAudioMutingFeature),
        VARIATION_ENTRY(kIPHTabScrollButtonFeature),
        VARIATION_ENTRY(kIPHTabSearchComboButtonFeature),
        VARIATION_ENTRY(kIPHWebUiHelpBubbleTestFeature),
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
// keep-sorted start case=no
        VARIATION_ENTRY(kIPHAutofillCreditCardBenefitFeature),
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
        VARIATION_ENTRY(kIPHGoogleOneOfferNotificationFeature),
        VARIATION_ENTRY(kIPHGrowthFramework),
        VARIATION_ENTRY(kIPHLauncherSearchHelpUiFeature),
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunch),
        VARIATION_ENTRY(kIPHDesktopPWAsLinkCapturingLaunchAppInTab),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
        VARIATION_ENTRY(kIPHSignInBenefitsFeature),
        VARIATION_ENTRY(kIPHSupervisedUserProfileSigninFeature),
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
        VARIATION_ENTRY(kIPHSearchPromotionFeature),
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
        VARIATION_ENTRY(kIPHExtensionsPinnedByDefaultFeature),
#endif

        // clang-format on
};

#undef DEFINE_VARIATION_PARAM
#undef VARIATION_ENTRY

// Returns all the features that are in use for engagement tracking.
COMPONENT_EXPORT(FEATURE_ENGAGEMENT_FEATURE_CONSTANTS)
FeatureVector GetAllFeatures();

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_FEATURE_LIST_H_
