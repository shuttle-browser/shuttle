// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_

#include <memory>
#include <utility>

#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/flex_layout_types.h"

namespace views {
class View;
}

// Provides a specialized layout implementation for normal tabbed browsers.
// Should not be used for other types of browsers.
class BrowserViewTabbedLayoutImpl : public BrowserViewLayoutImpl {
 public:
  BrowserViewTabbedLayoutImpl(
      std::unique_ptr<BrowserViewLayoutDelegate> delegate,
      BrowserViewLayoutViews views);
  ~BrowserViewTabbedLayoutImpl() override;

 protected:
  // BrowserViewLayoutImplCommon:
  gfx::Size GetMinimumSize(const views::View* host) const override;
  ProposedLayout CalculateProposedLayout(
      const BrowserLayoutParams& params) const override;
  gfx::Rect CalculateTopContainerLayout(ProposedLayout& layout,
                                        BrowserLayoutParams params,
                                        bool needs_exclusion) const override;
  void OnLayoutParamsChanged(const BrowserLayoutParams& old_params,
                             const BrowserLayoutParams& new_params) override;
  void OnGlassModeChanged() override;
  void ConfigureTopContainerBackground(
      const BrowserLayoutParams& params,
      CustomCornersBackground* background) override;
  void DoPreLayoutComputations(const BrowserLayoutParams& params) override;
  void DoPostLayoutVisualAdjustments(
      const BrowserLayoutParams& params) override;
  void DoPostLayoutCleanup() override;

 private:
  struct HorizontalLayout;
  struct SeparatorInfo;
  struct TransientLayoutData;
  struct OrganizerPanelAnimation;

  // Returns the minimum size of all toolbar-height content except the side
  // panel.
  gfx::Size GetMinimumMainAreaSize(const BrowserLayoutParams& params) const;

  // Returns the minimum size of the horizontal tabstrip, if any.
  gfx::Size GetMinimumTabStripSize(const BrowserLayoutParams& params) const;

  // Returns the leading margin for the horizontal tab strip region.
  int GetHorizontalTabStripLeadingMargin(
      const BrowserLayoutParams& params) const;

  // These helper functions are called during `DoPreLayoutCalculations()` to set
  // up `layout_data_`.
  HorizontalLayout CalculateHorizontalLayout(BrowserLayoutParams& params) const;
  OrganizerPanelAnimation CalculateOrganizerPanelAnimation() const;
  SeparatorInfo CalculateSeparatorInfo() const;

  std::unique_ptr<TransientLayoutData> layout_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_LAYOUT_BROWSER_VIEW_TABBED_LAYOUT_IMPL_H_
