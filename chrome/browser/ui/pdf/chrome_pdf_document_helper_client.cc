// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/chrome_pdf_document_helper_client.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/download/download_stats.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/content_restriction.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_frame_util.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/mime_handler/mime_handler_stream_manager.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace {

content::WebContents* GetWebContentsToUse(
    content::RenderFrameHost& render_frame_host) {
  // If we're viewing the PDF in a MimeHandlerViewGuest, use its embedder
  // WebContents.
  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(&render_frame_host);
  return guest_view
             ? guest_view->embedder_web_contents()
             : content::WebContents::FromRenderFrameHost(&render_frame_host);
}

}  // namespace

ChromePDFDocumentHelperClient::ChromePDFDocumentHelperClient() = default;

ChromePDFDocumentHelperClient::~ChromePDFDocumentHelperClient() = default;

void ChromePDFDocumentHelperClient::OnDocumentLoadComplete(
    content::RenderFrameHost& render_frame_host) {
}

void ChromePDFDocumentHelperClient::UpdateContentRestrictions(
    content::RenderFrameHost& render_frame_host,
    int content_restrictions) {
  // Speculative short-term-fix while we get at the root of
  // https://crbug.com/41337937 .
  content::WebContents* web_contents_to_use =
      GetWebContentsToUse(render_frame_host);
  if (!web_contents_to_use) {
    return;
  }

  CoreTabHelper* core_tab_helper =
      CoreTabHelper::FromWebContents(web_contents_to_use);
  // |core_tab_helper| is null for WebViewGuest.
  if (core_tab_helper) {
    core_tab_helper->UpdateContentRestrictions(content_restrictions);
  }
}

void ChromePDFDocumentHelperClient::OnSaveURL() {
  RecordDownloadSource(DOWNLOAD_INITIATED_BY_PDF_SAVE);
}

void ChromePDFDocumentHelperClient::SetPluginCanSave(
    content::RenderFrameHost& render_frame_host,
    bool can_save) {
  if (chrome_pdf::features::IsOopifPdfEnabled()) {
    auto* mime_handler_stream_manager =
        extensions::mime_handler::MimeHandlerStreamManager::FromWebContents(
            content::WebContents::FromRenderFrameHost(&render_frame_host));
    if (!mime_handler_stream_manager) {
      return;
    }

    content::RenderFrameHost* embedder_host =
        pdf_frame_util::GetEmbedderHost(&render_frame_host);
    CHECK(embedder_host);

    mime_handler_stream_manager->SetPluginCanSave(embedder_host, can_save);
    return;
  }

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromRenderFrameHost(&render_frame_host);
  if (guest_view) {
    guest_view->SetPluginCanSave(can_save);
  }
}

