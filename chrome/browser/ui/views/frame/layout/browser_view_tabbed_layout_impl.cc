// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/layout/browser_view_tabbed_layout_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/immersive/immersive_mode_controller.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/animations/tab_strip_animations.h"
#include "chrome/browser/ui/views/frame/custom_corners.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/custom_floating_corner.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_delegate.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_impl.h"
#include "chrome/browser/ui/views/frame/layout/browser_view_layout_params.h"
#include "chrome/browser/ui/views/frame/multi_contents_view.h"
#include "chrome/browser/ui/views/frame/shadow_frame_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container_view.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_tray_view.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/fullscreen_util_mac.h"
#endif

namespace {

#if BUILDFLAG(IS_MAC)
constexpr bool kUseFlyoverAnimation = false;
#else
constexpr bool kUseFlyoverAnimation = true;
#endif

// The opacity of flyover panels (e.g. the organizer tray) when the glass frame
// is enabled and expand-on-hover is active.
constexpr double kGlassExpandOnHoverOpacity = 0.95;

// Increases the leading or trailing exclusion padding to `minimum`.
// Gets the sum of leading and trailing exclusions in `params` for minimum-size
// computation.
int GetExclusionWidth(const BrowserLayoutParams& params) {
  const float width = params.leading_exclusion.content.width() +
                      params.trailing_exclusion.content.width();
  const float padding = params.leading_exclusion.horizontal_padding +
                        params.trailing_exclusion.horizontal_padding;
  return base::ClampCeil(width + padding);
}

}  // namespace

// Determines how much space to give to the horizontal elements.
struct BrowserViewTabbedLayoutImpl::HorizontalLayout {
  int min_content_width = 0;

  // Whether the top container (containing the toolbar, etc.) is laid out at
  // the top of the screen, above the contents.
  bool force_top_container_to_top = false;

  // True if the split view is present.
  bool is_split_view = false;
};

// Describes how to render the organizer panel.
struct BrowserViewTabbedLayoutImpl::OrganizerPanelAnimation {
  // These are core animation parameters used by all organizer panel animations.
  OrganizerPanelLocation location = OrganizerPanelLocation::kNone;
  double reveal_amount = 0.0;
  double panel_opacity = 1.0;
  double shadow_opacity = 1.0;
};

// Stores information about visual separators throughout the browser.
struct BrowserViewTabbedLayoutImpl::SeparatorInfo {
  // True if the separator should be rendered in the top container.
  bool top_container_separator = false;
  // True if the separator should be rendered in the multi-contents view.
  bool multi_contents_separator = false;
};

// Data computed during layout which is discarded afterwards.
// Members are presented in the order they're computed.
struct BrowserViewTabbedLayoutImpl::TransientLayoutData {
  explicit TransientLayoutData(const BrowserLayoutParams& browser_params)
      : revised_params(browser_params) {}
  ~TransientLayoutData() = default;

  // These parameters should be used by the layout function and may include
  // additional padding, etc. They are generated
  BrowserLayoutParams revised_params;
  WindowState window_state;
  TabStripType tab_strip_type;
  HorizontalLayout horizontal_layout;
  OrganizerPanelAnimation organizer_panel_animation;
  SeparatorInfo separator_info;
};

BrowserViewTabbedLayoutImpl::BrowserViewTabbedLayoutImpl(
    std::unique_ptr<BrowserViewLayoutDelegate> delegate,
    BrowserViewLayoutViews views)
    : BrowserViewLayoutImpl(std::move(delegate), std::move(views)) {}

BrowserViewTabbedLayoutImpl::~BrowserViewTabbedLayoutImpl() = default;

BrowserViewTabbedLayoutImpl::SeparatorInfo
BrowserViewTabbedLayoutImpl::CalculateSeparatorInfo() const {
  SeparatorInfo info;

  const bool has_infobar = delegate().IsInfobarVisible();
  const bool is_split_view = layout_data_->horizontal_layout.is_split_view;

  const bool would_have_separator = has_infobar || !is_split_view;
  switch (layout_data_->window_state) {
    case WindowState::kNormal:
    case WindowState::kMaximized:
      info.multi_contents_separator = !has_infobar;
      info.top_container_separator = has_infobar;
      break;
    case WindowState::kFullscreen:
      // Mac and ChromeOS display separators on their slide-in top containers.
      // However, if top container elements are not visible (for example,
      // because they are offscreen), the separator is not shown.
      info.top_container_separator = delegate().IsToolbarVisible();
      break;
    case WindowState::kFullscreenWithToolbar:
      info.top_container_separator = would_have_separator;
      break;
  }

  return info;
}

// Inset the leading edge of the tabstrip by the size of the swoop of the
// first tab; this is especially important for Mac, where the negative
// space of the caption button margins and the edge of the tabstrip should
// overlap. This only applies if there are no other leading buttons; if
// there are, we want a consistent gap from the caption buttons. The
// trailing edge receives the usual treatment, as it is the new tab button
// and not a tab.
int BrowserViewTabbedLayoutImpl::GetHorizontalTabStripLeadingMargin(
    const BrowserLayoutParams& params) const {
  int leading_margin = TabStyle::Get()->GetBottomCornerRadius();
  if (const gfx::Insets* internal_padding =
          views().horizontal_tab_strip_region_view->GetProperty(
              views::kInternalPaddingKey)) {
    leading_margin =
        std::max(0.f, params.leading_exclusion.horizontal_padding -
                          static_cast<float>(internal_padding->left()));
  }
  return leading_margin;
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumTabStripSize(
    const BrowserLayoutParams& params) const {
  if (delegate().GetTabStripType() != TabStripType::kHorizontal) {
    return gfx::Size();
  }
  auto result = views().horizontal_tab_strip_region_view->GetMinimumSize();
  result.Enlarge(GetExclusionWidth(params), 0);
  return result;
}

BrowserViewTabbedLayoutImpl::HorizontalLayout
BrowserViewTabbedLayoutImpl::CalculateHorizontalLayout(
    BrowserLayoutParams& params) const {
  HorizontalLayout layout;
  // Start with some preliminary values.
  layout.force_top_container_to_top =
      layout_data_->tab_strip_type != TabStripType::kHorizontal;

  layout.is_split_view = delegate().IsActiveTabSplit();

  layout.min_content_width = kContentsContainerMinimumWidth;

  return layout;
}

BrowserViewTabbedLayoutImpl::OrganizerPanelAnimation
BrowserViewTabbedLayoutImpl::CalculateOrganizerPanelAnimation() const {
  OrganizerPanelAnimation anim;
  const auto* const animations = delegate().GetAnimationController();
  anim.reveal_amount =
      animations
          ->GetCurrentValue(OrganizerPanelAnimations::kOrganizerPanel,
                            OrganizerPanelAnimations::kVisibleWidth)
          .value_or(0.0);
  if (anim.reveal_amount > 0.0) {
    anim.location = delegate().GetOrganizerPanelLocation();
  }
  return anim;
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumMainAreaSize(
    const BrowserLayoutParams& params) const {
  const gfx::Size toolbar_size = views().toolbar->GetMinimumSize();
  const gfx::Size infobar_container_size =
      views().infobar_container->GetMinimumSize();
  const gfx::Size contents_size = views().multi_contents_view->GetMinimumSize();

  int width = std::max({toolbar_size.width(), infobar_container_size.width(),
                            kContentsContainerMinimumWidth});
  const int height = toolbar_size.height() + infobar_container_size.height() +
                     contents_size.height();

  return gfx::Size(width, height);
}

gfx::Size BrowserViewTabbedLayoutImpl::GetMinimumSize(
    const views::View* host) const {
  auto params = delegate().GetBrowserLayoutParams(/*use_browser_bounds=*/true);

  // This is a simplified version of the same method in
  // `BrowserViewLayoutImplOld` that assumes a standard browser.
  const gfx::Size tabstrip_size = GetMinimumTabStripSize(params);
  const gfx::Size main_area_size = GetMinimumMainAreaSize(params);

  const int min_height = tabstrip_size.height() + main_area_size.height();

  // This assumes a horizontal tabstrip. There is also a hard minimum on the
  // width of the browser defined by `kMainBrowserContentsMinimumWidth`.
  const int min_width =
      std::max({tabstrip_size.width(), main_area_size.width(),
                kMainBrowserContentsMinimumWidth});

  return gfx::Size(min_width, min_height);
}

BrowserViewTabbedLayoutImpl::ProposedLayout
BrowserViewTabbedLayoutImpl::CalculateProposedLayout(
    const BrowserLayoutParams& browser_params) const {
  // TODO(https://crbug.com/453717426): Consider caching layouts of the same
  // size if no `InvalidateLayout()` has happened.

  // Build the proposed layout here:
  TRACE_EVENT0("ui", "BrowserViewLayoutImpl::CalculateProposedLayout");
  ProposedLayout layout;

  // The window scrim covers the entire browser view.
  if (views().window_scrim) {
    layout.AddChild(views().window_scrim, browser_params.visual_client_area);
  }

  BrowserLayoutParams params = layout_data_->revised_params;
  bool needs_exclusion = true;
  const HorizontalLayout& horizontal_layout = layout_data_->horizontal_layout;

  const int target_organizer_width =
      layout_data_->tab_strip_type == TabStripType::kHorizontal
          ? organizer_panel::kOrganizerPanelDefaultWidth
          : organizer_panel::kOrganizerPanelMinWidth;
  if (IsParentedTo(views().organizer_tray, views().browser_view)) {
    const bool show_organizer_tray =
        layout_data_->organizer_panel_animation.location ==
        OrganizerPanelLocation::kOrganizerTray;
    gfx::Rect organizer_tray_bounds;
    if (show_organizer_tray) {
      views().organizer_tray->SetTargetWidth(target_organizer_width);
      views().organizer_tray->SetTopLeadingExclusion(
          gfx::ToCeiledSize(params.leading_exclusion.ContentWithPadding()));

      const double reveal_amount =
          layout_data_->organizer_panel_animation.reveal_amount;
      const int visible_width =
          base::ClampFloor(reveal_amount * target_organizer_width);

      organizer_tray_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          visible_width, params.visual_client_area.height());
    }
    layout.AddChild(views().organizer_tray, organizer_tray_bounds,
                    show_organizer_tray);
  }

  // Lay out horizontal tab strip region if present.
  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().browser_view)) {
    gfx::Rect tabstrip_bounds;
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal) {
      const int leading_margin = GetHorizontalTabStripLeadingMargin(params);
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view, leading_margin);
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    layout_data_->tab_strip_type == TabStripType::kHorizontal);
  }

  // When the tabstrip isn't at the top or in constrained widths, the top
  // container is laid out before all side panels.
  if (horizontal_layout.force_top_container_to_top &&
      IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());

    // Calculate the params for laying out the top container.
    auto top_container_params =
        params.InLocalCoordinates(params.visual_client_area);

    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout, top_container_params, needs_exclusion);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    params.SetTop(top_container_layout.bounds.bottom());
  }

  // Lay out the main area background. It is only shown while clipping the
  // contents during animations, which no longer happen.
  if (IsParentedTo(views().main_background_region, views().browser_view)) {
    layout.AddChild(views().main_background_region, params.visual_client_area,
                    /*visibility=*/false);
  }

  // Lay out top container. The top container is laid out here with a
  // horizontal tabstrip if the browser is wide enough.
  if (!horizontal_layout.force_top_container_to_top &&
      IsParentedTo(views().top_container, views().browser_view)) {
    auto& top_container_layout =
        layout.AddChild(views().top_container, gfx::Rect());
    const gfx::Rect top_container_local_bounds = CalculateTopContainerLayout(
        top_container_layout,
        params.InLocalCoordinates(params.visual_client_area), needs_exclusion);
    top_container_layout.bounds =
        GetTopContainerBoundsInParent(top_container_local_bounds, params);
    params.SetTop(top_container_layout.bounds.bottom());
  }

  // Lay out infobar container.
  const bool infobar_visible = delegate().IsInfobarVisible();
  if (IsParentedTo(views().infobar_container, views().browser_view)) {
    gfx::Rect infobar_bounds;
    if (infobar_visible) {
      // Infobars slide down with top container reveal.
      const int additional_offset = delegate().GetExtraInfobarOffset();
      const int infobar_height =
          views().infobar_container->GetPreferredSize().height();
      // The content starts below the infobar, but is not affected by the
      // extra offset from the toolbar reveal animation. The infobar slides
      // down to stay visible below the revealed toolbar, while the content
      // stays in place. See https://crbug.com/40278831.
      infobar_bounds =
          gfx::Rect(params.visual_client_area.x(),
                    params.visual_client_area.y() + additional_offset,
                    params.visual_client_area.width(), infobar_height);
      params.SetTop(params.visual_client_area.y() + infobar_height);
    }
    layout.AddChild(views().infobar_container, infobar_bounds, infobar_visible);
  }

  const auto& separator_info = layout_data_->separator_info;
  views().multi_contents_view->SetShouldShowTopSeparator(
      separator_info.multi_contents_separator);

  // Updating the top, left and right insets for contents container view when
  // in split view. This is dependent on a number of other browser states.
  if (horizontal_layout.is_split_view) {
    const bool include_top_inset =
        layout_data_->window_state == WindowState::kFullscreen ||
        infobar_visible;
    const int default_inset = MultiContentsView::kSplitViewContentInset;

    const gfx::Insets contents_insets = gfx::Insets::TLBR(
        include_top_inset ? default_inset : 0, default_inset, default_inset,
        default_inset);

    views().multi_contents_view->SetSplitViewInsets(contents_insets);
  }

  // Update the multi-contents view about if we will be animating content
  // bounds. Shuttle: no side panel and no vertical tab strip, so content
  // bounds never animate.
  views().multi_contents_view->SetIsAnimatingContent(
      /*is_animating=*/false, /*is_origin_changing=*/false);

  // Lay out contents container. The contents container contains the multi-
  // contents view when multi-contents are enabled. The checks here are to
  // force the logic to be updated when multi-contents is fully rolled-out.
  CHECK(IsParentedToAndVisible(views().multi_contents_view,
                               views().browser_view));

  // In a small browser, the minimum-sized contents pane may exceed the width of
  // the window. In this case, the contents pane is allowed to extend past the
  // visual client area.
  int content_left = params.visual_client_area.x();
  int content_right = params.visual_client_area.right();
  if (const int deficit = horizontal_layout.min_content_width -
                          params.visual_client_area.width();
      deficit > 0) {
    // However, do not let this go past the edge of the allowed area.
    content_left =
        std::max(content_left, browser_params.visual_client_area.x());
    content_right =
        std::min(content_right, browser_params.visual_client_area.right());
  }
  layout.AddChild(views().multi_contents_view,
                      gfx::Rect(content_left, params.visual_client_area.y(),
                                content_right - content_left,
                                params.visual_client_area.height()));

  // Clipping the contents during animations is not used on Mac.
  if (kUseFlyoverAnimation) {
    views().multi_contents_view->SetTargetContentBounds(std::nullopt);
  }

  return layout;
}

gfx::Rect BrowserViewTabbedLayoutImpl::CalculateTopContainerLayout(
    ProposedLayout& layout,
    BrowserLayoutParams params,
    bool needs_exclusion) const {
  const int original_top = params.visual_client_area.y();

  if (IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    gfx::Rect tabstrip_bounds;
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal) {
      const int leading_margin = GetHorizontalTabStripLeadingMargin(params);
      tabstrip_bounds = GetBoundsWithExclusion(
          params, views().horizontal_tab_strip_region_view, leading_margin);
      params.SetTop(tabstrip_bounds.bottom() -
                    GetLayoutConstant(LayoutConstant::kTabstripToolbarOverlap));
      needs_exclusion = false;
    }
    layout.AddChild(views().horizontal_tab_strip_region_view, tabstrip_bounds,
                    layout_data_->tab_strip_type == TabStripType::kHorizontal);
  }

  // Lay out toolbar. If tabstrip is completely absent (or vertical), this can
  // go in the top exclusion area.
  const bool toolbar_visible = delegate().IsToolbarVisible();
  if (IsParentedTo(views().toolbar, views().top_container)) {
    gfx::Rect toolbar_bounds;
    if (toolbar_visible) {
      toolbar_bounds =
          needs_exclusion
              ? GetBoundsWithExclusion(params, views().toolbar)
              : gfx::Rect(params.visual_client_area.x(),
                          params.visual_client_area.y(),
                          params.visual_client_area.width(),
                          views().toolbar->GetPreferredSize().height());
      params.SetTop(toolbar_bounds.bottom());
      needs_exclusion = false;
    }
    layout.AddChild(views().toolbar, toolbar_bounds, toolbar_visible);
  }

  // Maybe show the separator in the top container.
  if (IsParentedTo(views().top_container_separator, views().top_container)) {
    gfx::Rect separator_bounds;
    if (layout_data_->separator_info.top_container_separator) {
      separator_bounds = gfx::Rect(
          params.visual_client_area.x(), params.visual_client_area.y(),
          params.visual_client_area.width(),
          views().top_container_separator->GetPreferredSize().height());
      params.SetTop(separator_bounds.bottom());
    }
    layout.AddChild(views().top_container_separator, separator_bounds,
                    layout_data_->separator_info.top_container_separator);
  }

  return gfx::Rect(params.visual_client_area.x(), original_top,
                   params.visual_client_area.width(),
                   params.visual_client_area.y() - original_top);
}

void BrowserViewTabbedLayoutImpl::ConfigureTopContainerBackground(
    const BrowserLayoutParams& params,
    CustomCornersBackground* background) {
  CHECK(layout_data_);
  // The top container always draws an opaque background in tabbed browser mode
  // to avoid cracking between visual elements.
  background->SetVisible(true);

  // In immersive mode and ChromeOS touch UI mode, the top Chrome UI is
  // parented to the `top_container()` and the frame header is not visible.
  // In these cases, the top container's background color should match the
  // frame color to ensure visual consistency.
  if (layout_data_->tab_strip_type == TabStripType::kHorizontal &&
      IsParentedTo(views().horizontal_tab_strip_region_view,
                   views().top_container)) {
    background->SetPrimaryColor(ui::kColorFrameActive);
  } else {
    background->SetPrimaryColor(CustomCornersBackground::ToolbarTheme());
  }

  // By default, this is just a flat background.
  CustomCornersBackground::Corners corners;

  // Round top corners when `top_container` reaches the top edge of a
  // non-maximized, non-fullscreen window.
  if (layout_data_->window_state == WindowState::kNormal) {
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal &&
        IsParentedTo(views().horizontal_tab_strip_region_view,
                     views().top_container)) {
      // When the horizontal tab strip is in `top_container` (e.g. ChromeOS
      // touch UI in float mode), `top_container` covers the frame header.
      corners[CornerOrientation::kTopLeading] =
          background->GetWindowCorner(/*upper=*/true);
      corners[CornerOrientation::kTopTrailing] =
          background->GetWindowCorner(/*upper=*/true);
    }
  }

  background->SetCorners(corners);
}

void BrowserViewTabbedLayoutImpl::OnLayoutParamsChanged(
    const BrowserLayoutParams& old_params,
    const BrowserLayoutParams& new_params) {
  CHECK(layout_data_);
  // If the size of the visual client area changes, the size of our working
  // client area should change by the same amount.
  const gfx::Insets insets =
      new_params.visual_client_area.InsetsFrom(old_params.visual_client_area);
  layout_data_->revised_params.visual_client_area.Inset(insets);
}

void BrowserViewTabbedLayoutImpl::OnGlassModeChanged() {
  views().browser_view->InvalidateLayout();
}

void BrowserViewTabbedLayoutImpl::DoPreLayoutComputations(
    const BrowserLayoutParams& params) {
  layout_data_ = std::make_unique<TransientLayoutData>(params);
  layout_data_->window_state = delegate().GetBrowserWindowState();
  layout_data_->tab_strip_type = delegate().GetTabStripType();
  layout_data_->horizontal_layout =
      CalculateHorizontalLayout(layout_data_->revised_params);
  layout_data_->organizer_panel_animation = CalculateOrganizerPanelAnimation();
  layout_data_->separator_info = CalculateSeparatorInfo();
}

void BrowserViewTabbedLayoutImpl::DoPostLayoutVisualAdjustments(
    const BrowserLayoutParams& params) {
  // If the window goes out of glass mode, these changes won't hurt anything.
  if (in_glass_mode()) {
    views().multi_contents_view->SetBackgroundRadii(gfx::RoundedCornersF());
  }

  CustomCornersBackground::Cutouts top_container_cutout_views;
  CustomCornersBackground::Cutouts main_background_cutout_views;

  const double flyover_panel_opacity =
      features::kGlassExpandOnHoverEnabled.Get() ? kGlassExpandOnHoverOpacity
                                                 : 1.0;

  CustomCorners::ColorChoiceWithAlpha frame_color(CustomCorners::FrameTheme(),
                                                  1.0f);
  if (layout_data_->tab_strip_type == TabStripType::kHorizontal &&
      !is_fullscreen(layout_data_->window_state) && in_glass_mode()) {
    frame_color.opacity = 0.0f;
  }

  if (IsParentedToAndVisible(views().organizer_tray, views().browser_view)) {
    const auto& anim = layout_data_->organizer_panel_animation;
    if (CustomCornersBackground* const background =
            views()
                .organizer_tray->background()
                ->AsA<CustomCornersBackground>()) {
      const bool blur = features::IsGlassFrameEnabled();

      background->SetPrimaryColor(CustomCorners::ColorChoiceWithAlpha(
          organizer_panel::kOrganizerPanelBackgroundColor,
          anim.panel_opacity * (blur ? flyover_panel_opacity : 1.0)));

      CustomCornersBackground::Corners corners;
      corners[CornerOrientation::kTopLeading] =
          background->GetWindowCorner(true);
      corners[CornerOrientation::kBottomLeading] =
          background->GetWindowCorner(false);
      corners[CornerOrientation::kTopTrailing].type =
          CustomCornersBackground::CornerType::kRounded;
      corners[CornerOrientation::kBottomTrailing].type =
          CustomCornersBackground::CornerType::kRounded;
      background->SetCorners(corners);
      background->SetUseBackgroundBlur(blur);
    }

    views().organizer_tray->layer()->SetOpacity(anim.panel_opacity);
    views().organizer_tray->UpdatePanelClipAndShadow();
  }

  auto* const toolbar_background =
      views().toolbar->background()->AsA<CustomCornersBackground>();
  CHECK(toolbar_background)
      << "Expected toolbar to have a CustomCornersBackground.";

  // Set toolbar corners.
  CustomCornersBackground::Corners toolbar_corners;
  switch (layout_data_->tab_strip_type) {
    case TabStripType::kHorizontal: {
      const gfx::Rect toolbar_bounds = views().toolbar->GetBoundsInScreen();
      const gfx::Rect tabstrip_bounds =
          views().horizontal_tab_strip_region_view->GetBoundsInScreen();
      if (toolbar_bounds.y() <= tabstrip_bounds.bottom()) {
        // Trailing curve is always shown for normal horizontal tabstrip when
        // the two are vertically adjacent.
        toolbar_corners[CornerOrientation::kTopTrailing].type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;

        // If there is anything on the leading side or the first tab is not
        // selected, then the corner radius is shown, otherwise we hide the
        // corner radius. (Don't show if the left edges don't line up.)
        if (!delegate().IsActiveTabAtLeadingWindowEdge() &&
            toolbar_bounds.x() <= tabstrip_bounds.x()) {
          toolbar_corners[CornerOrientation::kTopLeading].type =
              CustomCornersBackground::CornerType::kRoundedWithBackground;
        }
      }
      break;
    }
    default:
      // This can happen in content fullscreen on Mac, but otherwise doesn't
      // happen.
      break;
  }
  toolbar_background->SetCorners(toolbar_corners);

  auto* const top_container_background =
      views().top_container->background()->AsA<CustomCornersBackground>();
  CHECK(top_container_background)
      << "Expected top container to have a CustomCornersBackground.";

  // Set up top container cutouts. These need to be updated whether or not glass
  // is active for the current window.
  if (features::IsGlassFrameEnabled()) {
    if (!is_fullscreen(layout_data_->window_state)) {
      toolbar_background->SetCutoutFrom(top_container_cutout_views);
      // Cut the toolbar corners out of the top container itself.
      if (in_glass_mode()) {
        top_container_cutout_views.push_back(
            CustomCornersBackground::InverseOf(*toolbar_background));
      }
      top_container_background->SetCutoutFrom(top_container_cutout_views);
    } else {
      toolbar_background->SetCutoutFrom({});
      top_container_background->SetCutoutFrom({});
    }
    toolbar_background->SetCornerColor(frame_color);
    top_container_background->SetCornerColor(frame_color);
  }

  if (views().main_background_region &&
      views().main_background_region->GetVisible()) {
    auto* const main_background = views()
                                      .main_background_region->background()
                                      ->AsA<CustomCornersBackground>();
    CHECK(main_background)
        << "Expected main background region to have a CustomCornersBackground.";

    // Do the main area cutouts. These adjustments need to be made regardless of
    // whether the current window is in glass mode.
    if (features::IsGlassFrameEnabled()) {
      main_background->SetCutoutFrom(main_background_cutout_views);
      main_background->SetCornerColor(frame_color);
    }

    CustomCornersBackground::Corners main_background_corners;

    // Frame-colored corners are shown at the top in horizontal tabstrip mode.
    // This doesn't apply in fullscreen, as the tabstrip is not in the window.
    if (layout_data_->tab_strip_type == TabStripType::kHorizontal &&
        IsParentedTo(views().horizontal_tab_strip_region_view,
                     views().browser_view)) {
      // If (due to narrow width) the top container is not laid out in the main
      // area, it also doesn't get rounded corners.
      if (views().main_background_region->y() <= views().top_container->y()) {
        if (!delegate().IsActiveTabAtLeadingWindowEdge()) {
          main_background_corners[CornerOrientation::kTopLeading].type =
              CustomCornersBackground::CornerType::kRoundedWithBackground;
        }
        main_background_corners[CornerOrientation::kTopTrailing].type =
            CustomCornersBackground::CornerType::kRoundedWithBackground;
      }
    }

    // Need to ensure the bottom of the window is properly rounded for normal
    // windows.
    if (layout_data_->window_state == WindowState::kNormal) {
      main_background_corners[CornerOrientation::kBottomLeading] =
          main_background->GetWindowCorner(/*upper=*/false);
      main_background_corners[CornerOrientation::kBottomTrailing] =
          main_background->GetWindowCorner(/*upper=*/false);
    }

    main_background->SetCorners(main_background_corners);
  }
}

void BrowserViewTabbedLayoutImpl::DoPostLayoutCleanup() {
  layout_data_.reset();
}
