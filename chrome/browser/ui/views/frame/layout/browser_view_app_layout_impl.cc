// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_app_layout_impl.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/layout_types.h"

BrowserViewAppLayoutImpl::BrowserViewAppLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), std::move(views)) {}

BrowserViewAppLayoutImpl::~BrowserViewAppLayoutImpl() = default;

gfx::Size BrowserViewAppLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  const auto layout =
      delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/false);
  const int exclusion_width =
      base::ClampCeil(layout.leading_exclusion.ContentWithPadding().width() +
                      layout.trailing_exclusion.ContentWithPadding().width());

  const gfx::Size tabstrip_size =
      views().horizontal_tab_strip_region_view &&
              views().horizontal_tab_strip_region_view->GetVisible()
          ? views().horizontal_tab_strip_region_view->GetMinimumSize()
          : gfx::Size();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size =
      views().multi_contents_view->GetMinimumSize();

  const int width =
      std::max({exclusion_width + tabstrip_size.width(),
                infobar_container_size.width(), contents_size.width()});
  const int height = tabstrip_size.height() + infobar_container_size.height() +
                     contents_size.height();
  return gfx::Size(width, height);
}

BrowserViewAppLayoutImpl::ProposedLayout
BrowserViewAppLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = browser_params;

  // Lay out the top container if it's in the browser (even if it's empty).
  // This is required for certain things like hit-testing.
  if (IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout,
        params.InLocalCoordinates(params.visual_client_area),
        /*needs_exclusion=*/false);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    // In cases where the top container is nonzero size, need to move everything
    // else down.
    params.SetTop(std::max(params.visual_client_area.y(),
                           top_container_layout.bounds.bottom()));
  }

  // Lay out infobar container if present.
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    const bool infobar_visible = delegate().IsInfobarVisible();
    if (infobar_visible) {
      infobar_bounds = gfx::Rect(
          params.visual_client_area.x(),
          // Infobar needs to get down out of the way of immersive mode elements
          // in some cases.
          params.visual_client_area.y() + delegate().GetExtraInfobarOffset(),
          params.visual_client_area.width(),
          // This returns zero for empty infobar.
          views().infobar_container->GetPreferredSize().height());
      params.SetTop(infobar_bounds.bottom());
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  // Lay out contents container.
  CHECK(IsParentedToAndVisible(views().multi_contents_view,
                               views().browser_view));
  gfx::Rect contents_bounds = params.visual_client_area;
  contents_bounds.set_height(std::max(contents_bounds.height(), 1));
  layout.AddChild(views().multi_contents_view, contents_bounds);

  // If certain views were not laid out, make sure they're hidden to avoid
  // visual artifacts.
  if (delegate().ShouldLayoutTabStrip() &&
      IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().browser_view)) {
    layout.HideViewIfNotPresent(views().horizontal_tab_strip_region_view);
  }

  return layout;
}

gfx::Rect BrowserViewAppLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  // Note that on Mac, the titlebar area is native, so it is not counted in the
  // client area and no adjustment is necessary.

  // Lay out the standard toolbar if present. This is used in tab fullscreen
  // when custom tabs are present.
  if (IsParentedTo(views().toolbar, views().top_container)) {
    const bool show_toolbar = delegate().IsToolbarVisible();
    gfx::Rect toolbar_bounds(params.visual_client_area.origin(), gfx::Size());
    if (show_toolbar) {
      toolbar_bounds.set_width(params.visual_client_area.width());
      toolbar_bounds.set_height(views().toolbar->GetPreferredSize().height());
    }
    layout.AddChild(views().toolbar, toolbar_bounds, show_toolbar);
    params.SetTop(toolbar_bounds.bottom());
  }

  // Tabstrip must be hidden if it's not laid out.
  if (delegate().ShouldLayoutTabStrip() &&
      IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    layout.HideViewIfNotPresent(views().horizontal_tab_strip_region_view);
  }

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}
