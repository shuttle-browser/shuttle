// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const RESULTS_PER_PAGE = 150;

/**
 * Amount of time between pageviews that we consider a 'break' in browsing,
 * measured in milliseconds.
 */
export const BROWSING_GAP_TIME = 15 * 60 * 1000;


/**
 * Histogram buckets for UMA tracking of which view is being shown to the user.
 * Keep this in sync with the HistoryPageView enum in histograms.xml.
 * This enum is append-only.
 */
export enum HistoryPageViewHistogram {
  HISTORY = 0,
  DEPRECATED_GROUPED_WEEK = 1,
  DEPRECATED_GROUPED_MONTH = 2,
  SYNCED_TABS = 3,
  SIGNIN_PROMO = 4,
  JOURNEYS = 5,
  PRODUCT_SPECIFICATIONS_LISTS = 6,
  END = 7,  // Should always be last.
}

/**
 * Contains all context menu interactions for a visit in the history page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Must be kept in sync with the VisitContextMenuAction enum in
 * histograms/metadata/history/enums.xml
 */
// LINT.IfChange(VisitContextMenuAction)
export enum VisitContextMenuAction {
  MORE_FROM_THIS_SITE_CLICKED = 0,
  REMOVE_FROM_HISTORY_CLICKED = 1,
  REMOVE_BOOKMARK_CLICKED = 2,
  REVIEW_GEMINI_ACTIVITY_CLICKED = 3,
  MAX_VALUE = 4,
}
// LINT.ThenChange(/tools/metrics/histograms/metadata/history/enums.xml:VisitContextMenuAction)
