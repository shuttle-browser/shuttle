// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/feature_list.h"

#include <vector>

#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace {
// Whenever a feature is added to |kAllFeatures|, it should also be added as
// DEFINE_VARIATION_PARAM in the header, and also added to the
// |kIPHDemoModeChoiceVariations| array.
const base::Feature* const kAllFeatures[] = {
    &kIPHDummyFeature,  // Ensures non-empty array for all platforms.
#if BUILDFLAG(IS_ANDROID)
    // ALL_FEATURES_ANDROID_START
    // keep-sorted start case=no
    &kIPHAdaptiveButtonInTopToolbarCustomizationNewTabFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationOpenInBrowserFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryPdfFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationPageSummaryWebFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationReadAloudFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationShareFeature,
    &kIPHAdaptiveButtonInTopToolbarCustomizationVoiceSearchFeature,
    &kIPHAndroidBottomBarNewTab,
    &kIPHAndroidBottomBarPromoDialog,
    &kIPHAndroidTabDeclutter,
    &kIPHAndroidVerticalTabsPromoFeature,
    &kIPHAppRatingPromptFeature,
    &kIPHAppSpecificHistory,
    &kIPHAutoDarkOptOutFeature,
    &kIPHAutoDarkUserEducationMessageFeature,
    &kIPHAutoDarkUserEducationMessageOptInFeature,
    &kIPHCCTHistory,
    &kIPHCCTMinimized,
    &kIPHChromeHomeExpandFeature,
    &kIPHChromeHomePullToRefreshFeature,
    &kIPHChromeReengagementNotification1Feature,
    &kIPHChromeReengagementNotification2Feature,
    &kIPHChromeReengagementNotification3Feature,
    &kIPHContextualPageActionsActionChipFeature,
    &kIPHContextualPageActionsQuietVariantFeature,
    &kIPHDataSaverDetailFeature,
    &kIPHDataSaverMilestonePromoFeature,
    &kIPHDataSaverPreviewFeature,
    &kIPHDefaultBrowserPromoMagicStackFeature,
    &kIPHDefaultBrowserPromoMessagesFeature,
    &kIPHDefaultBrowserPromoSettingCardFeature,
    &kIPHDownloadHomeFeature,
    &kIPHDownloadIndicatorFeature,
    &kIPHDownloadInfoBarDownloadContinuingFeature,
    &kIPHDownloadInfoBarDownloadsAreFasterFeature,
    &kIPHDownloadPageFeature,
    &kIPHDownloadPageScreenshotFeature,
    &kIPHDownloadSettingsFeature,
    &kIPHEphemeralTabFeature,
    &kIPHExploreSitesTileFeature,
    &kIPHExtensionsManageAppMenuFeature,
    &kIPHExtensionsManageToolbarFeature,
    &kIPHFeedCardMenuFeature,
    &kIPHFeedHeaderMenuFeature,
    &kIPHFeedSwipeRefresh,
    &kIPHFuseboxAttachmentFeature,
    &kIPHGenericAlwaysTriggerHelpUiFeature,
    &kIPHGestureUserEducation,
    &kIPHIdentityDiscFeature,
    &kIPHIncognitoIndicatorCloseAllWindows,
    &kIPHInstanceSwitcherFeature,
    &kIPHKeyboardAccessoryBarSwipingFeature,
    &kIPHLowUserEngagementDetectorFeature,
    &kIPHMenuAddToGroup,
    &kIPHMicToolbarFeature,
    &kIPHMostVisitedTilesCustomizationPinFeature,
    &kIPHPageInfoFeature,
    &kIPHPageInfoStoreInfoFeature,
    &kIPHPageSummaryPdfMenuFeature,
    &kIPHPageSummaryWebMenuFeature,
    &kIPHPageZoomFeature,
    &kIPHPdfPageDownloadFeature,
    &kIPHPreviewsOmniboxUIFeature,
    &kIPHReadAloudAppMenuFeature,
    &kIPHReadAloudExpandedPlayerFeature,
    &kIPHReadAloudPlaybackModeFeature,
    &kIPHReaderModeDistillInAppFeature,
    &kIPHRecentTabsFeature,
    &kIPHRequestDesktopSiteDefaultOnFeature,
    &kIPHRequestDesktopSiteExceptionsGenericFeature,
    &kIPHRequestDesktopSiteWindowSettingFeature,
    &kIPHRestoreTabsOnFREFeature,
    &kIPHSharedHighlightingBuilder,
    &kIPHSharedHighlightingReceiverFeature,
    &kIPHShareScreenshotFeature,
    &kIPHSharingHubLinkToggleFeature,
    &kIPHSharingHubWebnotesStylizeFeature,
    &kIPHSiteControlsFeature,
    &kIPHTabSwitcherAddToGroup,
    &kIPHTabSwitcherButtonFeature,
    &kIPHTabSwitcherButtonSwitchIncognitoFeature,
    &kIPHTabSwitcherXR,
    &kIPHTabTearingXR,
    &kIPHThreeDotMenuBackButton,
    &kIPHVideoTutorialNTPChromeIntroFeature,
    &kIPHVideoTutorialNTPDownloadFeature,
    &kIPHVideoTutorialNTPSearchFeature,
    &kIPHVideoTutorialNTPSummaryFeature,
    &kIPHVideoTutorialNTPVoiceSearchFeature,
    &kIPHVideoTutorialTryNowFeature,
    // keep-sorted end
// ALL_FEATURES_ANDROID_END
#else
    // keep-sorted start case=no
    &kIPHiOSEnhancedBrowsingDesktopFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
    &kIPHBottomToolbarTipFeature,
#endif  // BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_IOS)
    // keep-sorted start case=no
    &kIPHBadgedReaderModeFeature,
    &kIPHBadgedReadingListFeature,
    &kIPHDefaultSiteViewFeature,
    &kIPHDiscoverFeedHeaderFeature,
    &kIPHFollowWhileBrowsingFeature,
    &kIPHHomeCustomizationMenuFeature,
    &kIPHiOSActiveDaysTrackingFeature,
    &kIPHiOSBackendPromoFeature,
    &kIPHiOSContextualPanelSampleModelFeature,
    &kIPHiOSDefaultBrowserBadgeEligibilityFeature,
    &kIPHiOSDefaultBrowserBannerPromoFeature,
    &kIPHiOSDefaultBrowserOffCyclePromoFeature,
    &kIPHiOSDefaultBrowserOverflowMenuBadgeFeature,
    &kIPHiOSDockingPromoEligibilityFeature,
    &kIPHiOSDockingPromoFeature,
    &kIPHiOSDownloadAutoDeletionFeature,
    &kIPHiOSFeedSwipeAnimatedFeature,
    &kIPHiOSFeedSwipeStaticFeature,
    &kIPHiOSHistoryOnOverflowMenuFeature,
    &kIPHiOSHomepageCustomizationNewBadge,
    &kIPHiOSInlineEnhancedSafeBrowsingPromoFeature,
    &kIPHiOSNewIAPromoFeature,
    &kIPHiOSOneTimeDefaultBrowserNotificationFeature,
    &kIPHiOSOverflowMenuCustomizationFeature,
    &kIPHIOSPageActionMenu,
    &kIPHiOSPageInfoRevampFeature,
    &kIPHiOSPinMostVisitedSiteFeature,
    &kIPHiOSPostDefaultAbandonmentPromoFeature,
    &kIPHiOSPromoAllTabsFeature,
    &kIPHiOSPromoAppStoreFeature,
    &kIPHiOSPromoBackgroundCustomizationFeature,
    &kIPHiOSPromoDefaultBrowserReminderFeature,
    &kIPHiOSPromoGenericDefaultBrowserFeature,
    &kIPHiOSPromoMadeForIOSFeature,
    &kIPHiOSPromoNonModalAppSwitcherDefaultBrowserFeature,
    &kIPHiOSPromoNonModalShareDefaultBrowserFeature,
    &kIPHiOSPromoNonModalUrlPasteDefaultBrowserFeature,
    &kIPHiOSPromoOverflowMenuDestinationDefaultBrowserFeature,
    &kIPHiOSPromoOverflowMenuShortcutsDefaultBrowserFeature,
    &kIPHiOSPromoPostRestoreDefaultBrowserFeature,
    &kIPHiOSPromoPostRestoreFeature,
    &kIPHiOSPromoSettingsCardDefaultBrowserFeature,
    &kIPHiOSPromoSettingsCellDefaultBrowserFeature,
    &kIPHiOSPromoStaySafeFeature,
    &kIPHiOSPullToRefreshFeature,
    &kIPHiOSReaderModeLargeOmniboxEntrypointFeature,
    &kIPHiOSReaderModeOptionsFeature,
    &kIPHiOSReminderNotificationsOverflowMenuBubbleFeature,
    &kIPHiOSReminderNotificationsOverflowMenuNewBadgeFeature,
    &kIPHiOSSafariImportFeature,
    &kIPHiOSSettingsInOverflowMenuBubbleFeature,
    &kIPHiOSSwipeBackForwardFeature,
    &kIPHiOSSwipeToolbarToChangeTabFeature,
    &kIPHiOSSwitchAccountsWithNTPAccountParticleDiscFeature,
    &kIPHiOSTabGridSwipeRightForIncognito,
    &kIPHiOSWelcomeBackFeature,
    &kIPHLongPressToolbarTipFeature,
    &kIPHReadingListMessagesFeature,
// keep-sorted end
#else
    &kIPHResumptionRailFeature,
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    &kEsbDownloadRowPromoFeature,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    &kIPHExtensionsMenuFeature,
    &kIPHExtensionsRequestAccessButtonFeature,
    &kIPHExtensionsZeroStatePromoFeature,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    // keep-sorted start case=no
    &kIPHBackNavigationMenuFeature,
    &kIPHBatterySaverModeFeature,
    &kIPHCriticalActionAppMenuFeature,
    &kIPHCriticalActionFilterChipFeature,
    &kIPHDesktopPwaInstallFeature,
    &kIPHDesktopRealboxContextualSearchFeature,
    &kIPHDesktopSharedHighlightingFeature,
    &kIPHDiscardRingFeature,
    &kIPHDownloadEsbPromoFeature,
    &kIPHExplicitBrowserSigninPreferenceRememberedFeature,
    &kIPHFocusHelpBubbleScreenReaderPromoFeature,
    &kIPHHistorySearchFeature,
    &kIPHMemorySaverModeFeature,
    &kIPHMultistepFilterPromoFeature,
    &kIPHPasswordsSavePrimingPromoFeature,
    &kIPHPdfInkSignaturesFeature,
    &kIPHPdfSearchifyFeature,
    &kIPHPdfTextAnnotationsFeature,
    &kIPHPerformanceInterventionDialogFeature,
    &kIPHPowerBookmarksSidePanelFeature,
    &kIPHPriceTrackingInSidePanelFeature,
    &kIPHProfileSwitchFeature,
    &kIPHPwaQuietNotificationFeature,
    &kIPHReadingListDiscoveryFeature,
    &kIPHReadingListEntryPointFeature,
    &kIPHReadingListInSidePanelFeature,
    &kIPHReadingModeKeyboardShortcutFeature,
    &kIPHReadingModePageActionLabelFeature,
    &kIPHReadingModePresentationModeFeature,
    &kIPHReadingModeSidePanelFeature,
    &kIPHSideBySidePinnableFeature,
    &kIPHSideBySideTabSwitchFeature,
    &kIPHSidePanelGenericPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFeature,
    &kIPHSidePanelLensOverlayPinnableFollowupFeature,
    &kIPHSmartTabSharingDefaultOnFeature,
    &kIPHSmartTabSharingFeature,
    &kIPHSmartTabSharingTryItFeature,
    &kIPHSplitViewHorizontalIndirectAccessFeature,
    &kIPHTabAudioMutingFeature,
    &kIPHTabScrollButtonFeature,
    &kIPHTabSearchComboButtonFeature,
    &kIPHVerticalTabsExpandOnHoverFeature,
    &kIPHVerticalTabstripTutorialFeature,
    &kIPHWebUiHelpBubbleTestFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
    // keep-sorted start case=no
    &kIPHAutofillCreditCardBenefitFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_CHROMEOS)
    // keep-sorted start case=no
    &kIPHGoogleOneOfferNotificationFeature,
    &kIPHGrowthFramework,
    &kIPHLauncherSearchHelpUiFeature,
// keep-sorted end

#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
    // keep-sorted start case=no
    &kIPHDesktopPWAsLinkCapturingLaunch,
    &kIPHDesktopPWAsLinkCapturingLaunchAppInTab,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
        // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
    &kIPHExtensionsPinnedByDefaultFeature,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // keep-sorted start case=no
    &kIPHSignInBenefitsFeature,
    &kIPHSignInBenefitsNewSigninFeature,
    &kIPHSupervisedUserProfileSigninFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
    // keep-sorted start case=no
    &kIPHSearchPromotionFeature,
// keep-sorted end
#endif  // BUILDFLAG(IS_WIN)

};
}  // namespace

std::vector<const base::Feature*> GetAllFeatures() {
  return std::vector<const base::Feature*>(std::begin(kAllFeatures),
                                           std::end(kAllFeatures));
}

}  // namespace feature_engagement
