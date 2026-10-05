// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles metrics for the settings pages. */

/**
 * Contains all possible recorded interactions across privacy settings pages.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the SettingsPrivacyElementInteractions enum in
 * histograms/metadata/settings/enums.xml
 */
// LINT.IfChange(PrivacyElementInteractions)
export enum PrivacyElementInteractions {
  // SYNC_AND_GOOGLE_SERVICES = 0,
  // CHROME_SIGN_IN = 1,
  DO_NOT_TRACK = 2,
  PAYMENT_METHOD = 3,
  // NETWORK_PREDICTION = 4,
  MANAGE_CERTIFICATES = 5,
  // SAFE_BROWSING = 6,
  // PASSWORD_CHECK = 7,
  IMPROVE_SECURITY = 8,
  // COOKIES_ALL = 9,
  // COOKIES_INCOGNITO = 10,
  // COOKIES_THIRD = 11,
  // COOKIES_BLOCK = 12,
  // COOKIES_SESSION = 13,
  // SITE_DATA_REMOVE_ALL = 14,
  // SITE_DATA_REMOVE_FILTERED = 15,
  // SITE_DATA_REMOVE_SITE = 16,
  // COOKIE_DETAILS_REMOVE_ALL = 17,
  // COOKIE_DETAILS_REMOVE_ITEM = 18,
  SITE_DETAILS_CLEAR_DATA = 19,
  THIRD_PARTY_COOKIES_ALLOW = 20,
  THIRD_PARTY_COOKIES_BLOCK_IN_INCOGNITO = 21,
  THIRD_PARTY_COOKIES_BLOCK = 22,
  BLOCK_ALL_THIRD_PARTY_COOKIES = 23,
  // IP_PROTECTION = 24,
  // FINGERPRINTING_PROTECTION = 25,
  // COUNT should be updated whenever new entries are added.
  COUNT = 26,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/settings/enums.xml:SettingsPrivacyElementInteractions)

/**
 * Contains all safe browsing interactions.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the UserAction in safe_browsing_settings_metrics.h.
 */
// LINT.IfChange(SafeBrowsingInteractions)
export enum SafeBrowsingInteractions {
  SAFE_BROWSING_SHOWED = 0,
  SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED = 1,
  SAFE_BROWSING_STANDARD_PROTECTION_CLICKED = 2,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED = 3,
  SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED = 4,
  SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED = 5,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED = 6,
  SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED = 7,
  // COUNT should be updated whenever new entries are added.
  COUNT = 8,
}
// LINT.ThenChange(//components/safe_browsing/core/common/safe_browsing_settings_metrics.h:UserAction)

/**
 * Contains the possible delete browsing data action types.
 * This should be kept in sync with the `DeleteBrowsingDataAction` enum in
 * components/browsing_data/core/browsing_data_utils.h
 */
// LINT.IfChange(DeleteBrowsingDataAction)
export enum DeleteBrowsingDataAction {
  CLEAR_BROWSING_DATA_DIALOG = 0,
  CLEAR_BROWSING_DATA_ON_EXIT = 1,
  INCOGNITO_CLOSE_TABS = 2,
  COOKIES_IN_USE_DIALOG = 3,
  SITES_SETTINGS_PAGE = 4,
  HISTORY_PAGE_ENTRIES = 5,
  QUICK_DELETE = 6,
  PAGE_INFO_RESET_PERMISSIONS = 7,
  RWS_DELETE_ALL_DATA = 8,
  COUNT = 9,
}
// LINT.ThenChange(
//   //components/browsing_data/core/browsing_data_utils.h:DeleteBrowsingDataAction,
//   //tools/metrics/histograms/metadata/privacy/enums.xml:DeleteBrowsingDataAction
// )

export interface MetricsBrowserProxy {
  /**
   * Helper function that calls recordAction with one action from
   * tools/metrics/actions/actions.xml.
   */
  recordAction(action: string): void;

  /**
   * Helper function that calls recordBooleanHistogram with the histogramName.
   */
  recordBooleanHistogram(histogramName: string, visible: boolean): void;

  /**
   * Helper function that calls recordHistogram for the
   * SettingsPage.PrivacyElementInteractions histogram
   */
  recordSettingsPageHistogram(interaction: PrivacyElementInteractions): void;

  /**
   * Helper function that calls recordHistogram for the
   * SafeBrowsing.Settings.UserAction histogram
   */
  recordSafeBrowsingInteractionHistogram(interaction: SafeBrowsingInteractions):
      void;

  /**
   * Helper function that delegates the metric recording to the
   * recordDeleteBrowsingDataAction backend function.
   */
  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction): void;

  // <if expr="_google_chrome and is_win">
  /**
   * Notifies Chrome that the `feature_notifications.enabled` Local State pref
   * was changed via the System settings page.
   *
   * @param enabled True if the toggle was changed to on (enabled), false if it
   * was changed to off (disabled).
   */
  recordFeatureNotificationsChange(enabled: boolean): void;
  // </if>
}

export class MetricsBrowserProxyImpl implements MetricsBrowserProxy {
  recordAction(action: string) {
    chrome.send('metricsHandler:recordAction', [action]);
  }

  recordBooleanHistogram(histogramName: string, visible: boolean): void {
    chrome.send('metricsHandler:recordBooleanHistogram', [
      histogramName,
      visible,
    ]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    chrome.send('metricsHandler:recordInHistogram', [
      'Settings.PrivacyElementInteractions',
      interaction,
      PrivacyElementInteractions.COUNT,
    ]);
  }

  recordSafeBrowsingInteractionHistogram(
      interaction: SafeBrowsingInteractions) {
    // TODO(crbug.com/40717279): Set the correct suffix for
    // SafeBrowsing.Settings.UserAction. Use the .Default suffix for now.
    chrome.send('metricsHandler:recordInHistogram', [
      'SafeBrowsing.Settings.UserAction.Default',
      interaction,
      SafeBrowsingInteractions.COUNT,
    ]);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction): void {
    chrome.send('metricsHandler:recordInHistogram', [
      'Privacy.DeleteBrowsingData.Action',
      action,
      DeleteBrowsingDataAction.COUNT,
    ]);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean): void {
    chrome.metricsPrivate.recordBoolean(
        'Windows.FeatureNotificationsSettingChange', enabled);
  }
  // </if>

  static getInstance(): MetricsBrowserProxy {
    return instance || (instance = new MetricsBrowserProxyImpl());
  }

  static setInstance(obj: MetricsBrowserProxy) {
    instance = obj;
  }
}

let instance: MetricsBrowserProxy|null = null;
