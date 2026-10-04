// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_

namespace contextual_search {

// LINT.IfChange(ContextUploadStatus)
// Upload status of a file.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.contextual_search
enum class ContextUploadStatus {
  // Not uploaded.
  kNotUploaded = 0,
  // File being processed.
  kProcessing = 1,
  // Failed validation - Terminal for this file attempt.
  kValidationFailed = 2,
  // Request sent to Lens server.
  kUploadStarted = 3,
  // Server confirmed successful receipt.
  kUploadSuccessful = 4,
  // Server or network error during upload - Terminal for this file attempt.
  kUploadFailed = 5,
  // File expired.
  kUploadExpired = 6,
  // File being processed, and suggest signals are ready.
  kProcessingSuggestSignalsReady = 7,
  // File is being replaced.
  kUploadReplaced = 8,

  // Add new enumerators above this line.
  // This must always be the last valid enum.
  kMaxValue = kUploadReplaced,
};
// LINT.ThenChange(
//   //components/omnibox/composebox/composebox_query.mojom:ContextUploadStatus,
//   //tools/metrics/histograms/metadata/omnibox/enums.xml:ContextUploadStatus
// )

// Returns true if the upload status is considered terminal.
bool IsTerminalContextStatus(ContextUploadStatus status);

// LINT.IfChange(ContextUploadErrorType)
// For upload error notifications and metrics.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.contextual_search
enum class ContextUploadErrorType {
  // Unknown.
  kUnknown = 0,
  // Browser error before/during request, not covered by validation.
  kBrowserProcessingError = 1,
  // Network-level issue (e.g., no connectivity, DNS failure).
  kNetworkError = 2,
  // Server returned an error (e.g., 5xx, specific API error).
  kServerError = 3,
  // Server rejected due to size after upload attempt - Considered terminal.
  kServerSizeLimitExceeded = 4,
  // Upload aborted by user deletion or session end.
  kAborted = 5,
  // Image processing error.
  kImageProcessingError = 6,
  // Browser processing error: File is too large.
  kBrowserProcessingFileTooLargeError = 7,
  // Browser processing error: File is empty.
  kBrowserProcessingFileEmptyError = 8,
  // Browser processing error: Maximum number of files exceeded.
  kBrowserProcessingMaxFilesExceededError = 9,
  // Browser processing error: Unsupported file type.
  kBrowserProcessingUnsupportedFileTypeError = 10,
  // Browser processing error: File upload not allowed.
  kBrowserProcessingFileUploadNotAllowedError = 11,
  // Browser processing error: Maximum number of images exceeded.
  kBrowserProcessingMaxImagesExceededError = 12,
  // Browser processing error: Maximum number of PDFs exceeded.
  kBrowserProcessingMaxPdfsExceededError = 13,

  // Add new enumerators above this line.
  // This must always be the last valid enum.
  kMaxValue = kBrowserProcessingMaxPdfsExceededError,
};
// LINT.ThenChange(
//   //components/omnibox/composebox/composebox_query.mojom:ContextUploadErrorType,
//   //tools/metrics/histograms/metadata/omnibox/enums.xml:ContextUploadErrorType
// )

// LINT.IfChange(ContextualSearchErrorPage)

// Reasons the contextual search error page appeared.
enum class ContextualSearchErrorPage {
  kUnknown = 0,
  kPageContextNotEligible = 1,
  kMaxValue = kPageContextNotEligible,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/contextual_search/enums.xml:ContextualSearchErrorPage)

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_TYPES_H_
