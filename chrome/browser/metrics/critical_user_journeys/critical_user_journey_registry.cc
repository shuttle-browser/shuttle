// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/webui/settings/settings_element_ids.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

CriticalUserJourneyRegistry::CriticalUserJourneyRegistry() = default;
CriticalUserJourneyRegistry::~CriticalUserJourneyRegistry() = default;

void CriticalUserJourneyRegistry::AddJourneys() {
  // ---------------------------------------------------------------------------
  // Existing Browser Journeys
  // ---------------------------------------------------------------------------

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileJourneySteps::kFinishDownload)
          .AddAnyOf(
              {Branch(kToolbarDownloadBubbleElementId,
                      ui::InteractionSequence::StepType::kShown,
                      ViewDownloadedFileJourneySteps::kDownloadBubbleAppears),
               Branch(
                   kToolbarDownloadButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kUserClickedDownloadBubble)})
          .AddStep(kDownloadBubbleOpenButtonId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kClickDownloadedFile)
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileFromAppMenuJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileFromAppMenuJourneySteps::kFinishDownload)
          .AddStep(
              kToolbarAppMenuButtonElementId,
              ui::InteractionSequence::StepType::kActivated,
              ViewDownloadedFileFromAppMenuJourneySteps::kAppMenuButtonClicked)
          .AddStep(AppMenuModel::kDownloadsMenuItem,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileFromAppMenuJourneySteps::
                       kDownloadsMenuItemClicked)
          .AddStep(
              kDownloadedFileOpenedCustomEventId,
              ui::InteractionSequence::StepType::kCustomEvent,
              ViewDownloadedFileFromAppMenuJourneySteps::kDownloadedFileClicked)
          .Build());

  AddJourney(
      metrics::CriticalUserJourney::Builder(&kPinExtensionJourney)
          .AddStep(kExtensionsMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   PinExtensionJourneySteps::kExtensionsMenuButtonClicked)
          .AddAnyOf(
              {Branch(kExtensionsMenuPinExtensionsEventId,
                      PinExtensionJourneySteps::kPinExtensionsViaMenuItem)})
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kClearBrowsingHistoryJourney)
          .AddAnyOf(
              {Branch(browsing_data_important_sites_util::
                          kOpenClearBrowsingDataDialogViaAcceleratorEventId,
                      ClearBrowsingHistoryJourneySteps::
                          kOpenClearBrowsingDataDialogViaAccelerator),
               Branch(
                   kToolbarAppMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ClearBrowsingHistoryJourneySteps::kActivateAppMenuButton)})
          .AddStep(browsing_data_important_sites_util::
                       kShowClearBrowsingDataDialogEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ClearBrowsingHistoryJourneySteps::
                       kShowClearBrowsingDataSettingsDialog)
          .AddAnyOf({Branch(browsing_data_important_sites_util::
                                kClearBrowsingDataHistoryEventId,
                            ClearBrowsingHistoryJourneySteps::
                                kClearBrowsingDataHistoryEvent),
                     Branch(browsing_data_important_sites_util::
                                kDismissClearBrowsingDataDialogEventId,
                            ClearBrowsingHistoryJourneySteps::
                                kDismissClearBrowsingDataDialog)
                         .SetExitBranch(base::Milliseconds(2000))})
          .Build());

  // ---------------------------------------------------------------------------
  // Settings Glow Up Journeys
  // ---------------------------------------------------------------------------
  if (base::FeatureList::IsEnabled(kSettingsGlowupJourneys)) {
    // Navigational IA Findability
    AddJourney(CriticalUserJourney::Builder(&kSettingsNavIAJourney)
                   .AddStep(settings::kSettingsPageLoadedEventId,
                            ui::InteractionSequence::StepType::kCustomEvent,
                            SettingsNavIAJourneySteps::kSettingsPageLoaded)
                   .AddStep(settings::kSettingsNavCategoryClickedEventId,
                            ui::InteractionSequence::StepType::kCustomEvent,
                            SettingsNavIAJourneySteps::kNavCategoryClicked)
                   .AddStep(settings::kSettingStateChangedEventId,
                            ui::InteractionSequence::StepType::kCustomEvent,
                            SettingsNavIAJourneySteps::kSettingStateChanged)
                   .Build());

    // Supercharged Search & Inline Controls
    AddJourney(
        CriticalUserJourney::Builder(&kSettingsSuperchargedSearchJourney)
            .AddStep(settings::kSettingsSearchBoxShownEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsSuperchargedSearchJourneySteps::kSearchBoxShown)
            .AddStep(
                settings::kSettingsSearchQueryEnteredEventId,
                ui::InteractionSequence::StepType::kCustomEvent,
                SettingsSuperchargedSearchJourneySteps::kSearchQueryEntered)
            .AddStep(
                settings::kSettingsSearchResultClickedEventId,
                ui::InteractionSequence::StepType::kCustomEvent,
                SettingsSuperchargedSearchJourneySteps::kSearchResultClicked)
            .Build());

    // Dedicated Search Engine & Shortcuts
    AddJourney(
        CriticalUserJourney::Builder(&kSettingsSearchEngineJourney)
            .AddStep(settings::kSearchEngineNavClickedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsSearchEngineJourneySteps::kSearchEngineNavClicked)
            .AddAnyOf({Branch(settings::kDefaultSearchEngineChangedEventId,
                              SettingsSearchEngineJourneySteps::
                                  kDefaultSearchEngineChanged),
                       Branch(settings::kSearchShortcutsToggledEventId,
                              SettingsSearchEngineJourneySteps::
                                  kSearchShortcutsToggled)})
            .Build());

    // Sites Dashboard & Site Permissions
    AddJourney(
        CriticalUserJourney::Builder(&kSettingsSitesDashboardJourney)
            .AddStep(settings::kSitesNavClickedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsSitesDashboardJourneySteps::kSitesNavClicked)
            .AddStep(
                settings::kSitePermissionCategoryClickedEventId,
                ui::InteractionSequence::StepType::kCustomEvent,
                SettingsSitesDashboardJourneySteps::kPermissionCategoryClicked)
            .AddStep(settings::kSitePermissionChangedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsSitesDashboardJourneySteps::kSitePermissionChanged)
            .Build());

    // Clear Browsing Data Baseline
    AddJourney(
        CriticalUserJourney::Builder(&kSettingsClearBrowsingDataJourney)
            .AddStep(settings::kClearBrowsingDataClickedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsClearBrowsingDataJourneySteps::
                         kClearBrowsingDataClicked)
            .AddStep(
                settings::kClearBrowsingDataDialogOkClickedEventId,
                ui::InteractionSequence::StepType::kCustomEvent,
                SettingsClearBrowsingDataJourneySteps::kConfirmButtonClicked)
            .Build());

    // Appearance & GM3 Themes
    AddJourney(
        CriticalUserJourney::Builder(&kSettingsAppearanceThemesJourney)
            .AddStep(
                settings::kAppearanceNavClickedEventId,
                ui::InteractionSequence::StepType::kCustomEvent,
                SettingsAppearanceThemesJourneySteps::kAppearanceNavClicked)
            .AddStep(settings::kAppearanceColorTileSelectedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsAppearanceThemesJourneySteps::kColorTileSelected)
            .AddStep(settings::kAppearanceThemeChangedEventId,
                     ui::InteractionSequence::StepType::kCustomEvent,
                     SettingsAppearanceThemesJourneySteps::kThemeChanged)
            .Build());
  }
}

void CriticalUserJourneyRegistry::AddJourney(
    std::unique_ptr<CriticalUserJourney> journey) {
  journeys_.push_back(std::move(journey));
}

}  // namespace metrics
