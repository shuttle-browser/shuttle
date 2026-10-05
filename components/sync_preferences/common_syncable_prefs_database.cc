// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/common_syncable_prefs_database.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/variations/service/google_groups_manager_prefs.h"

namespace sync_preferences {

namespace {
// Not an enum class to ease cast to int.
namespace syncable_prefs_ids {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When adding a new entry, append the
// enumerator to the end and add it to the `SyncablePref` enum in
// tools/metrics/histograms/metadata/sync/enums.xml. When removing an unused
// enumerator, comment it out here, making it clear the value was previously
// used, and add "(obsolete)" to the corresponding entry in enums.xml.
// LINT.IfChange(CommonSyncablePref)
enum {
  kSyncablePrefForTesting = 0,  // For tests.
  // kAutofillCreditCardEnabled = 1, (removed in Agent Chrome)
  // kAutofillEnabledDeprecated = 2, (deprecated)
  // kAutofillHasSeenIban = 3, (removed in Agent Chrome)
  // kAutofillIbanEnabled = 4,  (obsolete).
  // kAutofillLastVersionDeduped = 5, (removed in Agent Chrome)
  // kAutofillLastVersionDisusedAddressesDeleted = 6, (deprecated)
  // kAutofillProfileEnabled = 7, (removed in Agent Chrome)
  kShowAppsShortcutInBookmarkBar = 8,
  kShowBookmarkBar = 9,
  kShowManagedBookmarksInBookmarkBar = 10,
  // kClearBrowsingDataHistoryNoticeShownTimes = 11, (was never actually synced)
  // kDeleteBrowsingHistory = 12, (no longer synced)
  // kDeleteBrowsingHistoryBasic = 13, (no longer synced)
  // kDeleteCache = 14, (no longer synced)
  // kDeleteCacheBasic = 15, (no longer synced)
  // kDeleteCookies = 16, (no longer synced)
  // kDeleteCookiesBasic = 17, (no longer synced)
  // kDeleteDownloadHistory = 18, (no longer synced)
  // kDeleteFormData = 19, (no longer synced)
  // kDeleteHostedAppsData = 20, (no longer synced)
  // kDeletePasswords = 21, (no longer synced)
  // kDeleteSiteSettings = 22, (no longer synced)
  // kDeleteTimePeriod = 23, (no longer synced)
  // kDeleteTimePeriodBasic = 24, (no longer synced)
  kLastClearBrowsingDataTime = 25,
  // kPreferencesMigratedToBasic = 26, (deprecated)
  kPriceEmailNotificationsEnabled = 27,
  // kFont = 28, (deprecated)
  // kOfferReaderMode = 29, (deprecated)
  // kReaderForAccessibility = 30, (deprecated)
  // kTheme = 31, (deprecated)
  kAcceptLanguages = 32,
  // kApplicationLocale = 33,  (moved to chrome_syncable_prefs_database.cc)
  kSelectedLanguages = 34,
  kSyncDemographicsPrefName = 35,
  kCustomLinksInitialized = 36,
  kCustomLinksList = 37,
  kKeywordSpaceTriggeringEnabled = 38,
  // kCredentialsEnableAutosignin = 39, (removed in Agent Chrome)
  // kCredentialsEnableService = 40, (removed in Agent Chrome)
  // kPasswordDismissCompromisedAlertEnabled = 41, (removed in Agent Chrome)
  // kPasswordLeakDetectionEnabled = 42, (removed in Agent Chrome)
  // kSyncedLastTimePasswordCheckCompleted = 43, (deprecated)
  // kWasAutoSignInFirstRunExperienceShown = 44, (removed in Agent Chrome)
  kCanMakePaymentEnabled = 45,
  kAccountTailoredSecurityUpdateTimestamp = 46,
  kCookieControlsMode = 47,
  kSafeBrowsingEnabled = 48,
  // kSyncedDefaultSearchProviderGUID = 49, (deprecated)
  kPrefForceTriggerTranslateCount = 50,
  // kPrefNeverPromptSitesDeprecated = 51, (deprecated)
  kPrefTranslateAcceptedCount = 52,
  kPrefTranslateAutoAlwaysCount = 53,
  kPrefTranslateAutoNeverCount = 54,
  kPrefTranslateDeniedCount = 55,
  // kPrefTranslateIgnoredCount = 56, (no longer synced)
  kBlockedLanguages = 57,
  kOfferTranslateEnabled = 58,
  kPrefAlwaysTranslateList = 59,
  kPrefNeverPromptSitesWithTime = 60,
  kPrefTranslateRecentTarget = 61,
  kDogfoodGroupsSyncPrefName = 62,
  kSyncableMergeableDictPrefForTesting = 63,  // For tests.
  // kAutofillPaymentCvcStorage = 64, (removed in Agent Chrome)
  // kDeleteTimePeriodV2 = 65, (no longer synced)
  // kDeleteTimePeriodV2Basic = 66, (no longer synced)
  kSyncableMergeableListPrefForTesting = 67,         // For tests.
  kSyncableHistorySensitiveListPrefForTesting = 68,  // For tests.
  // kAutofillPaymentCardBenefits = 69, (removed in Agent Chrome)
  // kCloseTabs = 70, (no longer synced)
  kShowTabGroupsInBookmarkBar = 71,
  // kFacilitatedPaymentsPix = 72, (removed in Agent Chrome)
  kSyncableTabGroups = 73,
  kAutoPinNewTabGroups = 74,
  kShowGoogleLensShortcut = 75,
  // kSharingVapidKey = 76, (deprecated)
  // kFirstPlusAddressCreationTime = 77, (deprecated)
  // kLastPlusAddressFillingTime = 78, (deprecated)
  kSafeBrowsingEnhanced = 79,
  // kFacilitatedPaymentsEwallet = 80, (removed in Agent Chrome)
  // kAutofillBnplEnabled = 81, (removed in Agent Chrome)
  // kAutofillHasSeenBnpl = 82, (removed in Agent Chrome)
  // kAutomaticPasskeyUpgrades = 83, (removed in Agent Chrome)
  kSyncablePriorityPrefForTesting = 84,               // For tests.
  kSyncableAlwaysSyncingPriorityPrefForTesting = 85,  // For tests.
  // kFacilitatedPaymentsPixAccountLinking = 86, (removed in Agent Chrome)
  kShowSearchTools = 87,
  // kAutofillHomeMetadata = 88, (removed in Agent Chrome)
  // kAutofillWorkMetadata = 89, (removed in Agent Chrome)
  // kFacilitatedPaymentsA2AEnabled = 90, (removed in Agent Chrome)
  // kFacilitatedPaymentsA2ATriggeredOnce = 91, (removed in Agent Chrome)
  // kFingerprintingProtectionEnabled = 92, (deprecated)
  // kAutofillNameAndEmailProfileSignature = 93, (removed in Agent Chrome)
  // kAutofillNameAndEmailProfileNotSelectedCounter = 94, (removed in Agent Chrome)
  // kAutofillAiLastVersionDeduped = 96, (removed in Agent Chrome)
  kCrossDeviceOmniboxIsInBottomPosition = 97,
  // kAutofillWasNameAndEmailProfileUsed = 98, (removed in Agent Chrome)
  kCrossDeviceCrossPlatformPromosIOS16thActiveDay = 99,
  kCrossDeviceSafetyCheckHomeModuleEnabled = 100,
  // kAutofillAiIdentityEntitiesEnabled = 101, (removed in Agent Chrome)
  // kAutofillAiTravelEntitiesEnabled = 102, (removed in Agent Chrome)
  kCrossDeviceTabResumptionHomeModuleEnabled = 103,
  // kAutofillAmountExtractionAiTermsSeen = 104, (removed in Agent Chrome)
  kCrossDevicePriceTrackingHomeModuleEnabled = 105,
  kCrossDeviceMostVisitedHomeModuleEnabled = 106,
  kCrossDeviceTipsHomeModuleEnabled = 107,
  kCrossDeviceMagicStackHomeModuleEnabled = 108,
  kShowAiModeOmniboxButton = 109,
  // kAutofillAiSyncedOptInStatus = 110, (deprecated)
  kIOSPromoReminder = 111,
  // kAutofillAiReauthBeforeViewingSensitiveData = 112, (removed in Agent Chrome)
  // kDriveDisclaimerAccepted = 113,  // (deprecated)
  // kAutofillAiShoppingEntitiesEnabled = 114, (removed in Agent Chrome)
  kPrefTranslateRecentTargets = 115,
  kBookmarkBarVisibilityState = 116,
  kSyncableAccountScopedPrefForTesting = 117,  // For tests.
  // kAutofillAtMemoryTriggerInfo = 118, (removed in Agent Chrome)
  // kAutofillGmailOtpFillingEnabled = 119, (removed in Agent Chrome)
  // kAutofillGmailOtpFillingActivationDismissalTimestamp = 120, (removed in Agent Chrome)
  kDriveConsentState = 121,
  // kAutofillAiPrivateInferenceOptInStatus = 122, (removed in Agent Chrome)
  // kAutofillWalletReminderNoticeShown = 123, (removed in Agent Chrome)
  // New entries should be added above this line.
  kSubscriptionEligibilityAiSubscriptionTier =
      100338,  // (different ID as it's a move from //chrome)
  // See components/sync_preferences/README.md about adding new entries here.
  // vvvvv IMPORTANT! vvvvv
  // Note to the reviewer: IT IS YOUR RESPONSIBILITY to ensure that new syncable
  // prefs follow privacy guidelines! See the readme file linked above for
  // guidance and escalation path in case anything is unclear.
  // ^^^^^ IMPORTANT! ^^^^^
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:CommonSyncablePref)
}  // namespace syncable_prefs_ids

// List of syncable preferences common across platforms.
constexpr auto kCommonSyncablePrefsAllowlist =
    base::MakeFixedFlatMap<std::string_view, SyncablePrefMetadata>({
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_IOS)
#endif
        {browsing_data::prefs::kLastClearBrowsingDataTime,
         {syncable_prefs_ids::kLastClearBrowsingDataTime, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {language::prefs::kAcceptLanguages,
         {syncable_prefs_ids::kAcceptLanguages, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {language::prefs::kSelectedLanguages,
         {syncable_prefs_ids::kSelectedLanguages, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {metrics::kSyncDemographicsPrefName,
         {syncable_prefs_ids::kSyncDemographicsPrefName,
          syncer::PRIORITY_PREFERENCES,
          PrefSensitivity::kExemptFromUserControlWhileSignedIn,
          MergeBehavior::kNone}},
        {ntp_tiles::prefs::kCustomLinksInitialized,
         {syncable_prefs_ids::kCustomLinksInitialized, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {ntp_tiles::prefs::kCustomLinksList,
         {syncable_prefs_ids::kCustomLinksList, syncer::PREFERENCES,
          PrefSensitivity::kSensitiveRequiresHistory, MergeBehavior::kNone}},
        {omnibox::kKeywordSpaceTriggeringEnabled,
         {syncable_prefs_ids::kKeywordSpaceTriggeringEnabled,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone}},
        {omnibox::kShowGoogleLensShortcut,
         {syncable_prefs_ids::kShowGoogleLensShortcut, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {prefs::kAccountTailoredSecurityUpdateTimestamp,
         {syncable_prefs_ids::kAccountTailoredSecurityUpdateTimestamp,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {prefs::kCookieControlsMode,
         {syncable_prefs_ids::kCookieControlsMode, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {prefs::kSafeBrowsingEnabled,
         {syncable_prefs_ids::kSafeBrowsingEnabled, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
// For Ash, the OS_PRIORITY_PREFERENCES equivalent is defined in
// chrome/browser/sync/prefs/chrome_syncable_prefs_database.cc instead.
#if !BUILDFLAG(IS_CHROMEOS)
        {variations::kDogfoodGroupsSyncPrefName,
         {syncable_prefs_ids::kDogfoodGroupsSyncPrefName,
          syncer::PRIORITY_PREFERENCES,
          PrefSensitivity::kExemptFromUserControlWhileSignedIn,
          MergeBehavior::kNone}},
#endif
        {kSyncablePrefForTesting,
         {syncable_prefs_ids::kSyncablePrefForTesting, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {kSyncableAccountScopedPrefForTesting,
         {syncable_prefs_ids::kSyncableAccountScopedPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kNone, MergeBehavior::kNone,
          WriteBehavior::kWriteToAccountOnly}},
        {kSyncableMergeableDictPrefForTesting,
         {syncable_prefs_ids::kSyncableMergeableDictPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kMergeableDict}},
        {kSyncableMergeableListPrefForTesting,
         {syncable_prefs_ids::kSyncableMergeableListPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kMergeableListWithRewriteOnUpdate}},
        {kSyncableHistorySensitiveListPrefForTesting,
         {syncable_prefs_ids::kSyncableHistorySensitiveListPrefForTesting,
          syncer::PREFERENCES, PrefSensitivity::kSensitiveRequiresHistory,
          MergeBehavior::kNone}},
        {prefs::kSafeBrowsingEnhanced,
         {syncable_prefs_ids::kSafeBrowsingEnhanced, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
#if BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_ANDROID)
        {kSyncablePriorityPrefForTesting,
         {syncable_prefs_ids::kSyncablePriorityPrefForTesting,
          syncer::PRIORITY_PREFERENCES, PrefSensitivity::kNone,
          MergeBehavior::kNone}},
        {kSyncableAlwaysSyncingPriorityPrefForTesting,
         {syncable_prefs_ids::kSyncableAlwaysSyncingPriorityPrefForTesting,
          syncer::PRIORITY_PREFERENCES,
          PrefSensitivity::kExemptFromUserControlWhileSignedIn,
          MergeBehavior::kNone}},
        {omnibox::kShowSearchTools,
         {syncable_prefs_ids::kShowSearchTools, syncer::PREFERENCES,
          PrefSensitivity::kNone, MergeBehavior::kNone}},
        {subscription_eligibility::prefs::kAiSubscriptionTier,
         {syncable_prefs_ids::kSubscriptionEligibilityAiSubscriptionTier,
          syncer::PRIORITY_PREFERENCES,
          sync_preferences::PrefSensitivity::
              kExemptFromUserControlWhileSignedIn,
          sync_preferences::MergeBehavior::kNone}},
    });

}  // namespace

const SyncablePrefMetadata*
CommonSyncablePrefsDatabase::GetSyncablePrefMetadata(
    std::string_view pref_name) const {
  const auto it = kCommonSyncablePrefsAllowlist.find(pref_name);
  if (it == kCommonSyncablePrefsAllowlist.end()) {
    return nullptr;
  }
  return &it->second;
}

std::map<std::string_view, SyncablePrefMetadata>
CommonSyncablePrefsDatabase::GetAllSyncablePrefsForTest() const {
  return {kCommonSyncablePrefsAllowlist.begin(),
          kCommonSyncablePrefsAllowlist.end()};
}

}  // namespace sync_preferences
