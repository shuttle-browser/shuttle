// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container_view.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/i18n/rtl.h"
#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"
#include "chrome/browser/enterprise/data_protection/data_protection_overlay_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_capture_border_view.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "chrome/browser/ui/views/frame/contents_separator.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "chrome/browser/ui/views/frame/tab_modal_dialog_host.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/border.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {
constexpr int kSplitViewContentPadding = 4;
}  // namespace

ContentsContainerView::ContentsContainerView(BrowserView* browser_view)
    : browser_view_(browser_view),
      web_contents_modal_dialog_host_(browser_view_, this) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  SetProperty(views::kElementIdentifierKey, kContentsContainerViewElementId);

  // The default z-order is the order in which children were added to the
  // parent view. So first added devtools and the devtools scrim view (as it
  // exists behind the content view), then the content view and new tab page
  // footer. This should be followed by scrims, borders and lastly mini-toolbar.

  auto devtools_web_view =
      std::make_unique<views::WebView>(browser_view->GetProfile());
  devtools_web_view->SetID(VIEW_ID_DEV_TOOLS_DOCKED);
  devtools_web_view->SetVisible(false);
  devtools_web_view_ = AddChildView(std::move(devtools_web_view));

  devtools_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  devtools_scrim_view_->layer()->SetName(
      "ContentsContainerView:DevtoolsScrimView");

  toast_anchor_view_ = AddChildView(std::make_unique<views::View>());

  contents_view_ = AddChildView(
      std::make_unique<ContentsWebView>(browser_view->GetProfile()));
  contents_view_->SetID(VIEW_ID_TAB_CONTAINER);
  contents_view_->set_use_default_deadline_when_animating_bounds(
      base::FeatureList::IsEnabled(
          features::kUseDefaultDeadlineWhenAnimatingBounds));

  data_protection_overlay_view_ =
      AddChildView(std::make_unique<
                   enterprise_data_protection::DataProtectionOverlayView>());

  contents_scrim_view_ = AddChildView(std::make_unique<ScrimView>());
  contents_scrim_view_->layer()->SetName(
      "ContentsContainerView:ContentsScrimView");

  mini_toolbar_ = AddChildView(std::make_unique<MultiContentsViewMiniToolbar>(
      browser_view, contents_view_));

  container_outline_ =
      AddChildView(std::make_unique<ContentsContainerOutline>(mini_toolbar_));

  capture_contents_border_view_ =
      AddChildView(std::make_unique<ContentsCaptureBorderView>(mini_toolbar_));

  view_bounds_observer_.Observe(contents_view_);
}

ContentsContainerView::~ContentsContainerView() = default;

std::vector<views::View*> ContentsContainerView::GetAccessiblePanes() {
  std::vector<views::View*> accessible_panes;
  if (contents_view_->GetVisible()) {
    accessible_panes.push_back(contents_view_);
  }
  if (devtools_web_view_->GetVisible()) {
    accessible_panes.push_back(devtools_web_view_);
  }
  if (devtools_scrim_view_->GetVisible()) {
    accessible_panes.push_back(devtools_scrim_view_);
  }
  return accessible_panes;
}

void ContentsContainerView::UpdateBorderAndOverlay(bool is_in_split,
                                                   bool is_active,
                                                   bool is_highlighted) {
  const bool split_changed = is_in_split != is_in_split_;
  is_in_split_ = is_in_split;

  if (!is_in_split) {
    if (split_changed) {
      SetBorder(nullptr);
      UpdateBorderRoundedCorners();

      mini_toolbar_->SetVisible(false);
      container_outline_->SetVisible(false);
      if (capture_contents_border_view_) {
        capture_contents_border_view_->SetIsInSplit(false);
      }
    }
  } else {
    if (split_changed) {
      SetBorder(views::CreateEmptyBorder(gfx::Insets(
          kSplitViewContentPadding + ContentsContainerOutline::kThickness)));
      UpdateBorderRoundedCorners();
    }

    container_outline_->UpdateState(is_active, is_highlighted);
    // Mini toolbar should only be visible for the inactive contents
    // container view or both depending on configuration.
    mini_toolbar_->UpdateState(is_active, is_highlighted);
    if (capture_contents_border_view_) {
      capture_contents_border_view_->SetIsInSplit(true);
    }
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (split_changed) {
    // Ensures correct window rounded corners after updating contents rounded
    // corners in UpdateBorderRoundedCorners().
    GetWidget()->non_client_view()->frame_view()->UpdateWindowRoundedCorners();
  }
#endif  //  BUILDFLAG(IS_CHROMEOS)
}

void ContentsContainerView::SetBorderRoundedCornersFrom(
    const gfx::RoundedCornersF& corner_radii) {
  // Update devtools rounded corners. Note, devtools exists behind the contents
  // view so all devtools corners are rounded.
  devtools_web_view_->holder()->SetNativeViewCornerRadii(corner_radii);
  devtools_scrim_view_->SetRoundedCorners(corner_radii);

  const bool devtools_in_upper_left =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft;
  const bool devtools_in_upper_right =
      devtools_web_view_->GetVisible() &&
      current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight;
  const bool devtools_in_lower_left =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kLeft);
  const bool devtools_in_lower_right =
      devtools_web_view_->GetVisible() &&
      (current_devtools_docked_placement_ == DevToolsDockedPlacement::kBottom ||
       current_devtools_docked_placement_ == DevToolsDockedPlacement::kRight);

  const gfx::RoundedCornersF content_rounded_corners = gfx::RoundedCornersF{
      devtools_in_upper_left ? 0 : corner_radii.upper_left(),
      devtools_in_upper_right ? 0 : corner_radii.upper_right(),
      devtools_in_lower_right ? 0 : corner_radii.lower_right(),
      devtools_in_lower_left ? 0 : corner_radii.lower_left()};

  auto radii = content_rounded_corners;

  contents_view_->SetBackgroundRadii(radii);
  contents_view_->holder()->SetNativeViewCornerRadii(radii);
  contents_scrim_view_->SetRoundedCorners(corner_radii);

}

void ContentsContainerView::UpdateBorderRoundedCorners() {
  SetBorderRoundedCornersFrom(rounded_corner_radii_);
}

void ContentsContainerView::ChildVisibilityChanged(View* child) {
  if (child == devtools_web_view_ &&
      !rounded_corner_radii_.IsEmpty()) {
    UpdateBorderRoundedCorners();
  }
}

void ContentsContainerView::Layout(PassKey pass_key) {
  LayoutSuperclass<views::View>(this);

  UpdateContentsClip();
}

views::View::Views ContentsContainerView::GetChildrenInZOrder() {
#if DCHECK_IS_ON()
  auto ordered_children = views::View::GetChildrenInZOrder();
  // |capture_contents_border_view_| should have the highest z-order.
  DCHECK(ordered_children.back() == capture_contents_border_view_);
  return ordered_children;
#else
  return views::View::GetChildrenInZOrder();
#endif
}

void ContentsContainerView::OnViewBoundsChanged(View* observed_view) {
  if (observed_view == contents_view_) {
    UpdateDevToolsDockedPlacement();
    if (!rounded_corner_radii_.IsEmpty()) {
      UpdateBorderRoundedCorners();
    }
  }
}

void ContentsContainerView::SetContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy) {
  if (strategy_.Equals(strategy)) {
    return;
  }

  strategy_.CopyFrom(strategy);
  InvalidateLayout();
}

void ContentsContainerView::ApplyWatermarkSettings(
    const std::string& watermark_text,
    SkColor fill_color,
    SkColor outline_color,
    int font_size) {
  data_protection_overlay_view_->SetWatermarkText(watermark_text, fill_color,
                                                  outline_color, font_size);
}

void ContentsContainerView::UpdateDevToolsDockedPlacement() {
  DevToolsDockedPlacement placement = DevToolsDockedPlacement::kUnknown;
  gfx::Rect contents_view_bounds = GetContentsViewBounds();
  const gfx::Rect& container_bounds = GetContentsBounds();

  // If contents_webview has the same bounds as webview_container, it either
  // means that devtools are not open or devtools are open in a separate
  // window (not docked).
  if (contents_view_bounds == container_bounds) {
    placement = DevToolsDockedPlacement::kNone;
  } else if (contents_view_bounds.x() > container_bounds.x() &&
             contents_view_bounds.y() == container_bounds.y() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kLeft;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.height() == container_bounds.height()) {
    placement = DevToolsDockedPlacement::kRight;
  } else if (contents_view_bounds.origin() == container_bounds.origin() &&
             contents_view_bounds.width() == container_bounds.width()) {
    placement = DevToolsDockedPlacement::kBottom;
  }

  // When browser window is resizing, the contents_container and web_contents
  // bounds can be out of sync, resulting in a state, where it is impossible to
  // infer docked placement based on contents webview bounds. In this case, use
  // the last known docked placement, since resizing a window does not change
  // the devtools dock placement.
  if (placement != DevToolsDockedPlacement::kUnknown) {
    current_devtools_docked_placement_ = placement;
  }
}

void ContentsContainerView::ShowCaptureContentsBorder() {
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetVisible(true);
  }
}

void ContentsContainerView::HideCaptureContentsBorder() {
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetVisible(false);
  }
}

void ContentsContainerView::SetCaptureContentsBorderLocation(
    std::optional<gfx::Rect> border_location) {
  if (capture_contents_border_view_) {
    capture_contents_border_view_->SetCaptureContentsBorderLocation(
        border_location);
  }
}

gfx::Rect ContentsContainerView::GetContentsViewBounds() const {
  gfx::Rect contents_view_bounds = contents_view_->bounds();
  return contents_view_bounds;
}

void ContentsContainerView::SetTargetContentBounds(
    std::optional<gfx::Outsets> target_content_bounds) {
  if (target_content_bounds_ == target_content_bounds) {
    return;
  }

  target_content_bounds_ = target_content_bounds;
  InvalidateLayout(/*avoid_propagate_during_layout=*/true);
}

void ContentsContainerView::SetRoundedCorners(
    const gfx::RoundedCornersF& corner_radii) {
  if (corner_radii == rounded_corner_radii_) {
    return;
  }

  rounded_corner_radii_ = corner_radii;
  UpdateBorderRoundedCorners();
}

void ContentsContainerView::UpdateContentsClip() {
  bool changed =
      contents_view_->holder()->SetNativeViewClipRect(contents_clip_rect_);
  if (auto* const layer = contents_view_->layer()) {
    if (layer->clip_rect() != contents_clip_rect_) {
      layer->SetClipRect(contents_clip_rect_);
      changed = true;
    }
  }
  if (changed) {
    contents_view_->SchedulePaint();
  }
}

views::ProposedLayout ContentsContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }

  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  if (width == 0 || height == 0) {
    // On Wayland we receive a resize to 0 width first before the actual
    // size bounds. Ignore such requests.
    return layouts;
  }

  gfx::Rect full_contents_bounds = GetContentsBounds();
  gfx::Rect devtools_bounds;
  // The area contents excluding devtools is drawn (ie |contents_view_|,
  // etc).
  gfx::Rect non_devtools_contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy_, full_contents_bounds,
                                        &devtools_bounds,
                                        &non_devtools_contents_bounds);
  gfx::Rect contents_view_bounds = non_devtools_contents_bounds;

  // DevTools cares about the specific position, so we have to compensate RTL
  // layout here.
  layouts.child_layouts.emplace_back(
      devtools_web_view_.get(), devtools_web_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));
  layouts.child_layouts.emplace_back(
      devtools_scrim_view_.get(), devtools_scrim_view_->GetVisible(),
      GetMirroredRect(devtools_bounds),
      views::SizeBounds(full_contents_bounds.size()));

  const auto& contents_rect = GetMirroredRect(contents_view_bounds);
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(), contents_rect);

  layouts.child_layouts.emplace_back(
      toast_anchor_view_.get(), toast_anchor_view_->GetVisible(),
      gfx::BoundingRect(contents_rect.origin(), contents_rect.top_right()));

  // The content scrim view should cover the entire contents bounds.
  CHECK(contents_scrim_view_);
  layouts.child_layouts.emplace_back(contents_scrim_view_.get(),
                                     contents_scrim_view_->GetVisible(),
                                     full_contents_bounds);

  CHECK(data_protection_overlay_view_);
  layouts.child_layouts.emplace_back(
      data_protection_overlay_view_.get(),
      data_protection_overlay_view_->GetVisible(), full_contents_bounds);

  if (mini_toolbar_) {
    // |mini_toolbar_| should be offset in the bottom right corner, overlapping
    // the outline. Shrink the available space by corner radius to ensure we
    // have space to draw it at the corners.
    views::SizeBounds available_space(width, height);
    available_space.Enlarge(-ContentsContainerOutline::kCornerRadius,
                            -ContentsContainerOutline::kCornerRadius);
    gfx::Size mini_toolbar_size =
        mini_toolbar_->GetPreferredSize(available_space);
    const int offset_x = width - mini_toolbar_size.width();
    const int offset_y = height - mini_toolbar_size.height();
    const gfx::Rect mini_toolbar_rect =
        gfx::Rect(offset_x, offset_y, mini_toolbar_size.width(),
                  mini_toolbar_size.height());
    layouts.child_layouts.emplace_back(
        mini_toolbar_.get(), mini_toolbar_->GetVisible(), mini_toolbar_rect);
  }

  if (container_outline_) {
    layouts.child_layouts.emplace_back(container_outline_.get(),
                                       container_outline_->GetVisible(),
                                       gfx::Rect(0, 0, width, height));
  }

  if (capture_contents_border_view_) {
    gfx::Rect rect;
    if (auto capture_location =
            capture_contents_border_view_->capture_location();
        capture_location) {
      rect = *capture_location;
      rect.Offset(contents_view_bounds.OffsetFromOrigin());
    } else {
      rect = contents_view_bounds;
    }

#if BUILDFLAG(IS_CHROMEOS)
    // Immersive top container might overlap with the blue border in fullscreen
    // mode - see crbug.com/40880524. By insetting the bounds rectangle we
    // ensure that the blue border is always placed below the top container.
    if (ImmersiveModeController::From(browser_view_->browser())->IsRevealed()) {
      const int delta =
          browser_view_->top_container()->bounds().bottom() - rect.y();
      if (delta > 0) {
        rect.Inset(gfx::Insets().set_top(delta));
      }
    }
#endif

    bool visible = capture_contents_border_view_->GetVisible();
#if BUILDFLAG(IS_MAC)
    // Zero sized view should not be shown.
    if (rect.IsEmpty()) {
      visible = false;
    }
#endif  // BUILDFLAG(IS_MAC)

    layouts.child_layouts.emplace_back(capture_contents_border_view_.get(),
                                       visible, rect,
                                       views::SizeBounds(rect.size()));
  }

  auto* const content_layout = layouts.GetLayoutFor(contents_view_);
  if (target_content_bounds_) {
    content_layout->bounds.Outset(*target_content_bounds_);
    contents_clip_rect_ =
        gfx::Rect(gfx::Point(), content_layout->bounds.size());
    gfx::Insets insets = -target_content_bounds_->ToInsets();
    // Rendering layer isn't mirrored, so need to manually mirror insets.
    if (base::i18n::IsRTL()) {
      insets.set_left_right(insets.right(), insets.left());
    }
    contents_clip_rect_.Inset(insets);
  } else {
    contents_clip_rect_ = gfx::Rect();
  }

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}

BEGIN_METADATA(ContentsContainerView)
END_METADATA
