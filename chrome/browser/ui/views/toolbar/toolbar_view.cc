// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/page_action/page_action_properties_provider.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_container_views.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_coordinator.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_desktop.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/back_forward_button.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/overflow_menu.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/split_tabs_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_divider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "chrome/browser/ui/views/zoom/zoom_view_controller.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/cascading_property.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/mouse_watcher.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include <array>

#include "ui/aura/window_occlusion_tracker.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kActionItemUnderlineIndicatorKey, false)

namespace {

// Gets the display mode for a given browser.
ToolbarView::DisplayMode GetDisplayMode(BrowserWindowInterface* browser) {
  if (WindowFeatureController::From(browser)->SupportsWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTabStrip)) {
    return ToolbarView::DisplayMode::kNormal;
  }

  return ToolbarView::DisplayMode::kLocation;
}

auto& GetViewCommandMap() {
  static constexpr auto kViewCommandMap = base::MakeFixedFlatMap<int, int>(
      {{VIEW_ID_BACK_BUTTON, IDC_BACK},
       {VIEW_ID_FORWARD_BUTTON, IDC_FORWARD},
       {VIEW_ID_HOME_BUTTON, IDC_HOME},
       {VIEW_ID_RELOAD_BUTTON, IDC_RELOAD}});
  return kViewCommandMap;
}

constexpr int kBrowserAppMenuRefreshCollapsedMargin = 2;

// Returns whether `point` should be treated as part of the caption area in
// `view`. Recursively traverses into icon containers to correctly handle
// padding between buttons.
bool IsPositionInWindowCaptionForView(const views::View* view,
                                      const gfx::Point& point) {
  for (const views::View* child : view->children()) {
    if (!child->GetVisible() || !child->bounds().Contains(point)) {
      continue;
    }
    // Recurse into known icon container types to check their children.
    if (views::IsViewClass<ToolbarIconContainerView>(child) ||
        views::IsViewClass<page_actions::PageActionContainerView>(child)) {
      const gfx::Point point_in_child =
          views::View::ConvertPointToTarget(view, child, point);
      return IsPositionInWindowCaptionForView(child, point_in_child);
    }
    // Separators and dividers are non-interactive and should be treated
    // as caption area.
    if (views::IsViewClass<views::Separator>(child) ||
        views::IsViewClass<ToolbarDivider>(child)) {
      return true;
    }
    // The point hit an interactive control (button, location bar, etc.).
    return false;
  }
  // The point is not in any child's bounds — it's in empty space between
  // children, padding, or above/below a child. In VTS mode the toolbar is
  // at the very top of the window, so all non-interactive areas should be
  // draggable regardless of vertical position.
  return true;
}

void SetRefreshMargins(views::View* button, bool expanded) {
  button->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(
          0, expanded ? GetLayoutConstant(
                            LayoutConstant::kToolbarButtonRefreshExpandedMargin)
                      : kBrowserAppMenuRefreshCollapsedMargin));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ToolbarView, kToolbarElementId);

ToolbarView::ToolbarView(BrowserWindowInterface* browser,
                         BrowserView* browser_view)
    : AnimationDelegateViews(this),
      browser_(browser),
      browser_view_(browser_view),
      app_menu_icon_controller_(browser->GetProfile(), this),
      display_mode_(GetDisplayMode(browser)) {
  scoped_unowned_user_data_.emplace(browser_->GetUnownedUserDataHost(),
                                    *this);

  SetID(VIEW_ID_TOOLBAR);
  SetProperty(views::kElementIdentifierKey, kToolbarElementId);

  GetViewAccessibility().SetRole(ax::mojom::Role::kToolbar);

  if (display_mode_ == DisplayMode::kNormal) {
    for (const auto& view_and_command : GetViewCommandMap()) {
      chrome::AddCommandObserver(browser_, view_and_command.second, this);
    }
  }
  views::SetCascadingColorProviderColor(this, views::kCascadingBackgroundColor,
                                        kColorToolbar);
}

ToolbarView::~ToolbarView() {
  if (display_mode_ != DisplayMode::kNormal) {
    return;
  }

  if (overflow_button_) {
    overflow_button_->set_toolbar_controller(nullptr);
  }

  for (const auto& view_and_command : GetViewCommandMap()) {
    chrome::RemoveCommandObserver(browser_, view_and_command.second, this);
  }
}

void ToolbarView::Init() {
#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/40171404#comment3
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif

  std::unique_ptr<LocationBarView> location_bar_view =
      std::make_unique<LocationBarView>(
          browser_, browser_->GetProfile(),
          chrome::BrowserCommandController::From(browser_), this,
          display_mode_ != DisplayMode::kNormal);

  // Make sure the toolbar shows by default.
  size_animation_.Reset(1);

  if (display_mode_ != DisplayMode::kNormal) {
    CHECK(location_bar_view)
        << "Alternate location bar impls need to handle this.";
    location_bar_view_ = AddChildView(std::move(location_bar_view));
    location_bar_ = location_bar_view_;
    location_bar_view_->Init();
  }

  if (display_mode_ == DisplayMode::kNormal) {
    SetBackground(std::make_unique<CustomCornersBackground>(
        *this, *browser_view_,
        /*primary_color=*/CustomCornersBackground::ToolbarTheme(),
        /*corner_color=*/CustomCornersBackground::FrameTheme()));
  } else if (display_mode_ == DisplayMode::kLocation) {
    // Add the pinned toolbar actions container so that downloads can be shown
    // in popups.
    pinned_toolbar_actions_container_ = AddChildView(
        std::make_unique<PinnedToolbarActionsContainer>(browser_view_, this));
    pinned_toolbar_actions_ = pinned_toolbar_actions_container_;
    SetBackground(views::CreateSolidBackground(kColorLocationBarBackground));
    SetLayoutManager(std::make_unique<views::FlexLayout>())
        ->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
        .SetDefault(views::kFlexBehaviorKey,
                    views::FlexSpecification(
                        views::LayoutOrientation::kHorizontal,
                        views::MinimumFlexSizeRule::kPreferredSnapToZero))
        .SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse);
    if (location_bar_view_) {
      location_bar_view_->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));
    }
    initialized_ = true;
    return;
  }

  const auto callback = [](BrowserWindowInterface* browser, int command,
                           const ui::Event& event) {
    chrome::ExecuteCommandWithDisposition(
        browser, command, ui::DispositionFromEventFlags(event.flags()));
  };

  PrefService* const prefs = browser_->GetProfile()->GetPrefs();

  std::unique_ptr<ExtensionsToolbarDesktop> extensions_container;
  std::unique_ptr<ToolbarDivider> toolbar_divider;

  extensions_container = std::make_unique<ExtensionsToolbarDesktop>(browser_);
  toolbar_divider = std::make_unique<ToolbarDivider>();

  // Always add children in order from left to right, for accessibility.
  back_ = AddChildView(std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kBack,
      base::BindRepeating(callback, browser_, IDC_BACK), browser_));
  forward_ = AddChildView(std::make_unique<BackForwardButton>(
      BackForwardButton::Direction::kForward,
      base::BindRepeating(callback, browser_, IDC_FORWARD), browser_));

  reload_ = AddChildView(std::make_unique<ReloadButton>(
      browser_->GetProfile(), chrome::BrowserCommandController::From(browser_)));

  home_ = AddChildView(std::make_unique<HomeButton>(
      browser_, base::BindRepeating(callback, browser_, IDC_HOME)));

  split_tabs_ = AddChildView(std::make_unique<SplitTabsToolbarButton>(browser_));

  location_bar_view_ = AddChildView(std::move(location_bar_view));
  location_bar_ = location_bar_view_;

  if (extensions_container) {
    extensions_container_ = AddChildView(std::move(extensions_container));
    extensions_toolbar_coordinator_ =
        std::make_unique<ExtensionsToolbarCoordinator>(extensions_container_);
  }

  if (toolbar_divider) {
    toolbar_divider_ = AddChildView(std::move(toolbar_divider));
  }

  pinned_toolbar_actions_container_ = AddChildView(
      std::make_unique<PinnedToolbarActionsContainer>(browser_view_, this));
  pinned_toolbar_actions_ = pinned_toolbar_actions_container_;

  if (IsChromeLabsEnabled()) {
    UpdateChromeLabsNewBadgePrefs(browser_->GetProfile());

    const bool should_show_chrome_labs_ui =
        ShouldShowChromeLabsUI(browser_->GetProfile());
    if (should_show_chrome_labs_ui) {
      show_chrome_labs_button_.Init(
          chrome_labs_prefs::kBrowserLabsEnabledEnterprisePolicy, prefs,
          base::BindRepeating(&ToolbarView::OnChromeLabsPrefChanged,
                              base::Unretained(this)));
      // Set the visibility for the button based on initial enterprise policy
      // value. Only call OnChromeLabsPrefChanged if there is a change from
      // the initial value.
      pinned_toolbar_actions_container_->GetActionItemFor(kActionShowChromeLabs)
          ->SetVisible(show_chrome_labs_button_.GetValue() &&
                       should_show_chrome_labs_ui);
    }
  }

  overflow_button_ = AddChildView(std::make_unique<OverflowButton>());
  overflow_button_->SetVisible(false);

  {
    auto app_menu_button = std::make_unique<BrowserAppMenuButton>(this);
    app_menu_button->SetFlipCanvasOnPaintForRTLUI(true);
    app_menu_button->GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
    app_menu_button->SetTooltipText(
        l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
    app_menu_button->SetID(VIEW_ID_APP_MENU);
    app_menu_button_ = AddChildView(std::move(app_menu_button));
  }

  LoadImages();

  // Set the button icon based on the system state. Do this after
  // |app_menu_button_| has been added as a bubble may be shown that needs
  // the widget (widget found by way of app_menu_button_->GetWidget()).
  app_menu_icon_controller_.UpdateDelegate();

  location_bar_view_->Init();

  show_forward_button_.Init(
      prefs::kShowForwardButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowForwardButtonChanged,
                          base::Unretained(this)));

  SetForwardButtonVisibility(show_forward_button_.GetValue());

  show_home_button_.Init(
      prefs::kShowHomeButton, prefs,
      base::BindRepeating(&ToolbarView::OnShowHomeButtonChanged,
                          base::Unretained(this)));

  if (home_) {
    home_->SetVisible(show_home_button_.GetValue());
  }

  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_view_->browser());
  if (vertical_tab_strip_state_controller) {
    vertical_tab_subscription_ =
        vertical_tab_strip_state_controller->RegisterOnModeChanged(
            base::BindRepeating(&ToolbarView::OnVerticalTabStripModeChanged,
                                base::Unretained(this)));
    should_display_vertical_tabs_ =
        vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs();
  }
  InitLayout();

  for (auto* button :
       std::array<views::Button*, 4>{back_, forward_, reload_, home_}) {
    if (button) {
      button->set_tag(GetViewCommandMap().at(button->GetID()));
    }
  }

  initialized_ = true;
}

void ToolbarView::OnVerticalTabStripModeChanged(
    tabs::VerticalTabStripStateController* controller) {
  should_display_vertical_tabs_ = controller->ShouldDisplayVerticalTabs();
  // Invalidate the layout cache so responsive buttons (forward, home, split,
  // etc) re-evaluate their visibility against the final toolbar width, clearing
  // out any zeroed out state from intermediate layout passes.
  InvalidateLayout();
}

void ToolbarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0) {
    SetToolbarVisibility(false);
  }
  BrowserWindow::FromBrowser(browser())->ToolbarSizeChanged(
      /*is_animating=*/false);
}

void ToolbarView::AnimationProgressed(const gfx::Animation* animation) {
  BrowserWindow::FromBrowser(browser())->ToolbarSizeChanged(
      /*is_animating=*/true);
}

void ToolbarView::Update(WebContents* tab) {
  if (location_bar_) {
    location_bar_->Update(tab);
  }

  if (custom_tab_bar_) {
    custom_tab_bar_->UpdateContents();
  }

  if (extensions_container_) {
    extensions_container_->UpdateAllIcons();
  }

  if (pinned_toolbar_actions_container_) {
    pinned_toolbar_actions_container_->UpdateAllIcons();
  }

  if (ReloadControl* reload_control = GetReloadButton(); reload_control) {
    reload_control->SetDevToolsStatus(
        chrome::IsDebuggerAttachedToCurrentTab(browser_));
  }
}

bool ToolbarView::UpdateSecurityState() {
  if ((location_bar_ && location_bar_->HasSecurityStateChanged()) ||
      (custom_tab_bar_ && custom_tab_bar_->HasSecurityStateChanged())) {
    Update(nullptr);
    return true;
  }

  return false;
}

void ToolbarView::SetToolbarVisibility(bool visible) {
  SetVisible(visible);
  views::View* bar = location_bar_view_;
  CHECK(bar) << "Alternate location bar impls need to handle this.";
  bar->SetVisible(visible);
}

void ToolbarView::ResetTabState(WebContents* tab) {
  if (location_bar_) {
    location_bar_->ResetTabState(tab);
  }
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  AppMenuControl* app_menu_control = GetAppMenuControl();
  if (app_menu_control) {
    app_menu_control->Focus(GetAsAccessiblePaneView());
  }
}

bool ToolbarView::GetAppMenuFocused() const {
  const AppMenuControl* app_menu_control = GetAppMenuControl();
  return app_menu_control && app_menu_control->HasFocus();
}

bool ToolbarView::IsPositionInWindowCaption(
    const gfx::Point& test_point) const {
  return IsPositionInWindowCaptionForView(this, test_point);
}

void ToolbarView::RecordHitTestMetrics(bool is_caption_area) {
  const bool is_mouse_down = GetWidget() && GetWidget()->IsMouseButtonDown();

  if (!is_mouse_down) {
    was_mouse_down_ = false;
  } else if (!was_mouse_down_) {
    was_mouse_down_ = true;
    if (browser_view_->ShouldDrawVerticalTabStrip()) {
      if (is_caption_area) {
        base::RecordAction(
            base::UserMetricsAction("VerticalTabs.Toolbar.CaptionAreaPressed"));
      } else {
        base::RecordAction(base::UserMetricsAction(
            "VerticalTabs.Toolbar.InteractiveAreaPressed"));
      }
    }
  }
}

views::Button* ToolbarView::GetChromeLabsButton() const {
  ChromeLabsCoordinator* coordinator = ChromeLabsCoordinator::From(browser_);
  return coordinator ? coordinator->GetChromeLabsButton() : nullptr;
}

ExtensionsToolbarButton* ToolbarView::GetExtensionsButton() const {
  return extensions_container_->GetExtensionsButton();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() {
  return browser_->GetTabStripModel()->GetActiveWebContents();
}

LocationBarModel* ToolbarView::GetLocationBarModel() {
  return LocationBarModel::From(browser_.get());
}

const LocationBarModel* ToolbarView::GetLocationBarModel() const {
  return LocationBarModel::From(browser_.get());
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return BrowserContentSettingBubbleModelDelegate::From(browser_);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  DCHECK(display_mode_ == DisplayMode::kNormal);

  const std::array<views::Button*, 4> kButtons{back_, forward_, reload_,
                                               home_};
  auto it = std::ranges::find_if(
      kButtons, [id](views::Button* b) { return b && b->tag() == id; });
  if (it != kButtons.end()) {
    (*it)->SetEnabled(enabled);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, ui::AcceleratorProvider implementation:

bool ToolbarView::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

gfx::Size ToolbarView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size;
  switch (display_mode_) {
    case DisplayMode::kLocation:
      size = location_bar_->PreferredSize();
      break;
    case DisplayMode::kNormal:
      size = AccessiblePaneView::CalculatePreferredSize(available_size);
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/41471763),
      // we cap the height at the size of known child views (location bar and
      // back button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height = std::max(location_bar_->PreferredSize().height(),
                                        GetBackForwardButtonSize().height()) +
                               layout_manager_->interior_margin().height();
        size.SetToMin({size.width(), max_height});
      }
  }
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

gfx::Size ToolbarView::GetMinimumSize() const {
  gfx::Size size;
  switch (display_mode_) {
    case DisplayMode::kLocation:
      size = location_bar_->MinimumSize();
      break;
    case DisplayMode::kNormal:
      size = AccessiblePaneView::GetMinimumSize();
      // Because there are odd cases where something causes one of the views in
      // the toolbar to report an unreasonable height (see crbug.com/41471763),
      // we cap the height at the size of known child views (location bar and
      // back button) plus margins.
      // TODO(crbug.com/40663413): Figure out why the height reports incorrectly
      // on some installations.
      if (layout_manager_ && location_bar_->IsVisible()) {
        const int max_height =
            std::max(location_bar_->MinimumSize().height(),
                     GetBackForwardButtonSize(/*minimum_size=*/true).height()) +
            layout_manager_->interior_margin().height();
        size.SetToMin({size.width(), max_height});
      }
      // Overflow button must be part of minimum size calculation.
      if (overflow_button_ &&
          browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL &&
          !overflow_button_->GetVisible()) {
        const int default_margin =
            GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
        size.Enlarge(
            default_margin + overflow_button_->GetMinimumSize().width(), 0);
      }
  }
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

void ToolbarView::Layout(PassKey) {
  // If we have not been initialized yet just do nothing.
  if (!initialized_) {
    return;
  }

  if (display_mode_ == DisplayMode::kNormal) {
    LayoutCommon();
  }

  if (toolbar_controller_) {
    // Need to determine whether the overflow button should be visible, and only
    // update it if the visibility changes.
    const bool was_overflow_button_visible =
        toolbar_controller_->overflow_button()->GetVisible();
    const bool show_overflow_button =
        toolbar_controller_->ShouldShowOverflowButton(size());
    if (was_overflow_button_visible != show_overflow_button) {
      views::ManualLayoutUtil(layout_manager_)
          .SetViewHidden(toolbar_controller_->overflow_button(),
                         !show_overflow_button);
      base::RecordAction(base::UserMetricsAction(
          show_overflow_button ? "ResponsiveToolbar.OverflowButtonShown"
                               : "ResponsiveToolbar.OverflowButtonHidden"));
    }
  }

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  LayoutSuperclass<AccessiblePaneView>(this);
}

void ToolbarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  if (!initialized_) {
    return;
  }

  if (display_mode_ == DisplayMode::kNormal) {
    LoadImages();
  }

  SchedulePaint();
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view && (focused_view->GetID() == VIEW_ID_OMNIBOX)) {
    return false;  // Let the omnibox handle all accelerator events.
  }
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

void ToolbarView::ChildPreferredSizeChanged(views::View* child) {
  InvalidateLayout();
  if (size() != GetPreferredSize()) {
    PreferredSizeChanged();
  }
}

void ToolbarView::ChildVisibilityChanged(views::View* child) {
  if (child == home_) {
    if (!home_->GetVisible() && show_home_button_.GetValue()) {
      base::UmaHistogramBoolean("Toolbar.Overflow.HomeButton", true);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar - and
// also so that it selects all content in the location bar.
views::View* ToolbarView::GetDefaultFocusableChild() {
  return location_bar_view_;
}

void ToolbarView::InitLayout() {
  const int default_margin =
      GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  const int location_bar_margin =
      GetLayoutConstant(LayoutConstant::kLocationBarMargin);

  // Shift previously flex-able elements' order by `kOrderOffset`.
  // This will cause them to be the first ones to drop out or shrink to minimum.
  // Order 1 - kOrderOffset will be assigned to new flex-able elements.
  constexpr int kOrderOffset = 1000;
  // If kOmniboxResizingPrioritization is enabled, give the location bar the
  // highest priority as it will first shrink down to its soft minimum but won't
  // hit its hard minimum until all other items have dropped out.
  const int location_bar_flex_order =
      base::FeatureList::IsEnabled(features::kOmniboxResizingPrioritization)
          ? 1
          : kOrderOffset + 1;
  constexpr int kToolbarActionsFlexOrder = kOrderOffset + 2;
  constexpr int kExtensionsFlexOrder = kOrderOffset + 3;

  const views::FlexSpecification location_bar_flex_rule =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(location_bar_flex_order);

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, default_margin));

  // If the Views location bar is in use, set its properties here.
  if (location_bar_view_) {
    location_bar_view_->SetProperty(views::kFlexBehaviorKey,
                                    location_bar_flex_rule);
    location_bar_view_->SetProperty(views::kMarginsKey,
                                    gfx::Insets::VH(0, location_bar_margin));
  }

  if (extensions_container_) {
    const views::FlexSpecification extensions_flex_rule =
        views::FlexSpecification(
            extensions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kExtensionsFlexOrder);

    extensions_container_->SetProperty(views::kFlexBehaviorKey,
                                       extensions_flex_rule);
  }

  if (pinned_toolbar_actions_container_) {
    pinned_toolbar_actions_container_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(
            pinned_toolbar_actions_container_->GetAnimatingLayoutManager()
                ->GetDefaultFlexRule())
            .WithOrder(kToolbarActionsFlexOrder));
  }

  if (toolbar_divider_) {
    toolbar_divider_->SetProperty(
        views::kMarginsKey,
        gfx::Insets::VH(
            0, GetLayoutConstant(LayoutConstant::kToolbarDividerSpacing)));
  }

  if (app_menu_button_ &&
      base::FeatureList::IsEnabled(features::kToolbarAppMenuLabelResizing)) {
    app_menu_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                 views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
  }

  {
    // Order 1 is reserved for the location bar if
    // kOmniboxResizingPrioritization is enabled.
    constexpr int kToolbarFlexOrderStart = 2;

    // TODO(crbug.com/40929989): Ignore containers till issue addressed.
    toolbar_controller_ = std::make_unique<ToolbarController>(
        OverflowMenu::GetDefaultResponsiveElements(browser_),
        ToolbarController::GetDefaultOverflowOrder(), kToolbarFlexOrderStart,
        this, /*webui_toolbar_controller_delegate=*/nullptr, overflow_button_,
        pinned_toolbar_actions_,
        PinnedToolbarActionsModel::Get(browser_view_->GetProfile()));
    overflow_button_->set_toolbar_controller(toolbar_controller_.get());
  }

  LayoutCommon();
}

void ToolbarView::LayoutCommon() {
  DCHECK(display_mode_ == DisplayMode::kNormal);

  gfx::Insets interior_margin =
      GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN);

  if (app_menu_button_) {
    const bool expanded = app_menu_button_->IsLabelPresentAndVisible();
    if (expanded) {
      // The interior margin in an expanded state should be more than in a
      // collapsed state.
      interior_margin.set_right(interior_margin.right() + 1);
    }
    SetRefreshMargins(app_menu_button_, expanded);
  }

  layout_manager_->SetInteriorMargin(interior_margin);

  // Extend buttons to the window edge if we're either in a maximized or
  // fullscreen window. This makes the buttons easier to hit, see Fitts' law.
  // Note on layout synchronization: In Chromium UI Views, window maximize,
  // restore, or fullscreen transitions automatically invalidate container
  // geometry and invoke `ToolbarView::Layout()`. This layout pass guarantees
  // `SetIsMaximizedOrFullscreen` remains reliably synchronized with window
  // bounds without requiring supplemental widget observation hooks.
  const bool is_maximized_or_fullscreen =
      browser_->GetWindow() && (browser_->GetWindow()->IsMaximized() ||
                                browser_->GetWindow()->IsFullscreen());

  const int margin = is_maximized_or_fullscreen ? interior_margin.left() : 0;
  back_->SetLeadingMargin(margin);

  GetAppMenuControl()->SetIsMaximizedOrFullscreen(is_maximized_or_fullscreen);

  if (toolbar_divider_ && extensions_container_) {
    views::ManualLayoutUtil(layout_manager_)
        .SetViewHidden(toolbar_divider_, !extensions_container_->GetVisible());
  }
  // Cast button visibility is controlled externally.
}

// AppMenuIconController::Delegate:
void ToolbarView::UpdateTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  AppMenuControl* app_menu_control = GetAppMenuControl();
  if (app_menu_control) {
    app_menu_control->SetTypeAndSeverity(type_and_severity);
  }
  auto* action_item = actions::ActionManager::Get().FindAction(
      kActionUpgradeDialog, BrowserActions::From(browser_)->root_action_item());
  CHECK(action_item);
  action_item->SetVisible(
      type_and_severity.type ==
      AppMenuIconController::IconType::kUpgradeNotification);
  action_item->SetText(AppMenuModel::GetUpgradeDialogTitleText());

  auto* global_error_action_item = actions::ActionManager::Get().FindAction(
      kActionGlobalError, BrowserActions::From(browser_)->root_action_item());
  CHECK(global_error_action_item);
  if (GlobalError* error =
          GlobalErrorServiceFactory::GetForProfile(browser_->GetProfile())
              ->GetHighestSeverityGlobalErrorWithAppMenuItem()) {
    global_error_action_item->SetVisible(true);
    global_error_action_item->SetText(error->MenuItemLabel());
    global_error_action_item->SetImage(error->MenuItemIcon());
  } else {
    global_error_action_item->SetVisible(false);
  }
}

ExtensionsContainerViews* ToolbarView::GetExtensionsContainerViews() {
  return extensions_container_;
}

PinnedToolbarActions* ToolbarView::GetPinnedToolbarActions() {
  return pinned_toolbar_actions_;
}

gfx::Size ToolbarView::GetToolbarButtonSize() const {
  // Since DisplayMode::kLocation is for a slimline toolbar showing only compact
  // location bar used for popups, toolbar buttons (ie downloads) must be
  // smaller to accommodate the smaller size.
  const int size =
      display_mode_ == DisplayMode::kLocation
          ? location_bar_->PreferredSize().height()
          : GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  return gfx::Size(size, size);
}

views::BubbleAnchor ToolbarView::GetDefaultExtensionDialogAnchor() {
  ExtensionsContainerViews* extensions_container =
      GetExtensionsContainerViews();
  if (extensions_container && extensions_container->IsVisible()) {
    return extensions_container->GetExtensionsButtonAnchor();
  }
  auto* control = GetAppMenuControl();
  return control ? control->GetAnchor() : views::BubbleAnchor();
}

page_actions::PageActionViewInterface* ToolbarView::GetPageActionViewInterface(
    actions::ActionId action_id) {
  page_actions::PageActionPropertiesProvider provider;
  if (!provider.Contains(action_id)) {
    return nullptr;
  }
  return location_bar_view()->page_action_container()->GetPageActionView(
      action_id);
}

AppMenuControl* ToolbarView::GetAppMenuControl() {
  return app_menu_button_;
}

const AppMenuControl* ToolbarView::GetAppMenuControl() const {
  return app_menu_button_;
}

gfx::Rect ToolbarView::GetFindBarBoundingBox(int contents_bottom) {
  if (!WindowFeatureController::From(browser_)->SupportsWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureLocationBar)) {
    return gfx::Rect();
  }

  CHECK(location_bar_);

  if (!location_bar_->IsDrawn()) {
    return gfx::Rect();
  }

  gfx::Rect bounds = views::View::ConvertRectFromScreen(
      GetWidget()->GetRootView(), location_bar_->BoundsInScreen());
  return gfx::Rect(bounds.x(), bounds.bottom(), bounds.width(),
                   contents_bottom - bounds.bottom());
}

void ToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::BubbleAnchor ToolbarView::GetBubbleAnchor(
    std::optional<actions::ActionId> action_id) {
  // If a pinned toolbar actions button exists for the action_id, return that.
  if (pinned_toolbar_actions_ && action_id.has_value() &&
      pinned_toolbar_actions_->IsActionPinnedOrPoppedOut(action_id.value())) {
    return pinned_toolbar_actions_->GetBubbleAnchor(action_id.value());
  }

  // Otherwise attempt to use the location bar.
  auto anchor = views::BubbleAnchor(location_bar_view_);
  bool anchor_not_drawn = false;
  if (views::View* view = anchor.GetIfView()) {
    anchor_not_drawn = !view->IsDrawn();
  }
  // In app windows the location bar view may exist but not be drawn. Avoid
  // anchoring bubbles to a non-drawn view (e.g. on Ozone/Wayland) and always
  // return a valid view anchor by falling back to the contents view.
  if (anchor_not_drawn && browser_view_) {
    auto* top_container = browser_view_->top_container();
    CHECK(top_container);
    return views::BubbleAnchor(top_container);
  }
  return anchor;
}

views::BubbleAnchor ToolbarView::GetPageActionBubbleAnchor(
    actions::ActionId action_id) {
  page_actions::PageActionViewInterface* view =
      GetPageActionViewInterface(action_id);
  if (view) {
    return view->GetBubbleAnchor();
  }
  return views::BubbleAnchor();
}

void ToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  auto* zoom_view_controller = browser_->GetActiveTabInterface()
                                   ->GetTabFeatures()
                                   ->zoom_view_controller();
  CHECK(zoom_view_controller);
  zoom_view_controller->UpdatePageActionIconAndBubbleVisibility(
      /*prefer_to_show_bubble=*/can_show_bubble, /*from_user_gesture=*/false);
}

ToolbarButton* ToolbarView::GetBackButton() {
  return back_;
}

ReloadControl* ToolbarView::GetReloadButton() {
  return reload_;
}

ToolbarButton* ToolbarView::GetDownloadButton() {
  return pinned_toolbar_actions_container_
             ? pinned_toolbar_actions_container_->GetButtonFor(
                   kActionShowDownloads)
             : nullptr;
}

std::optional<BrowserRootView::DropIndex> ToolbarView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return BrowserRootView::DropIndex{
      .index = browser_->GetTabStripModel()->active_index(),
      .relative_to_index =
          BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex};
}

BrowserRootView::DropTarget* ToolbarView::GetDropTarget(
    gfx::Point loc_in_local_coords) {
  return HitTestPoint(loc_in_local_coords) ? this : nullptr;
}

views::View* ToolbarView::GetViewForDrop() {
  return this;
}

void ToolbarView::OnChromeLabsPrefChanged() {
  actions::ActionItem* chrome_labs_action =
      pinned_toolbar_actions_container_->GetActionItemFor(
          kActionShowChromeLabs);
  chrome_labs_action->SetVisible(
      show_chrome_labs_button_.GetValue() &&
      ShouldShowChromeLabsUI(browser_->GetProfile()));
  GetViewAccessibility().AnnounceText(l10n_util::GetStringUTF16(
      chrome_labs_action->GetVisible()
          ? IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_ADDED_BY_ENTERPRISE_POLICY
          : IDS_ACCESSIBLE_TEXT_CHROMELABS_BUTTON_REMOVED_BY_ENTERPRISE_POLICY));
}

void ToolbarView::LoadImages() {
  DCHECK_EQ(display_mode_, DisplayMode::kNormal);

  if (extensions_container_) {
    extensions_container_->UpdateAllIcons();
  }
}

void ToolbarView::OnShowForwardButtonChanged() {
  SetForwardButtonVisibility(show_forward_button_.GetValue());
  InvalidateLayout();
}

void ToolbarView::OnShowHomeButtonChanged() {
  if (home_) {
    home_->SetVisible(show_home_button_.GetValue());
  }
}

void ToolbarView::OnTouchUiChanged() {
  if (display_mode_ == DisplayMode::kNormal) {
    // Update the internal margins for touch layout.
    // TODO(dfried): I think we can do better than this by making the touch UI
    // code cleaner.
    const int default_margin =
        GetLayoutConstant(LayoutConstant::kToolbarElementPadding);
    const int location_bar_margin =
        GetLayoutConstant(LayoutConstant::kLocationBarMargin);
    layout_manager_->SetDefault(views::kMarginsKey,
                                gfx::Insets::VH(0, default_margin));
    if (location_bar_view_) {
      location_bar_view_->SetProperty(views::kMarginsKey,
                                      gfx::Insets::VH(0, location_bar_margin));
    }

    LoadImages();
    PreferredSizeChanged();
  }
}

void ToolbarView::SetForwardButtonVisibility(bool visible) {
  forward_->SetVisible(visible);
}

gfx::Size ToolbarView::GetBackForwardButtonSize(bool minimum_size) const {
  if (back_) {
    return minimum_size ? back_->GetMinimumSize() : back_->GetPreferredSize();
  }
  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  return gfx::Size(size, size);
}

BEGIN_METADATA(ToolbarView)
ADD_READONLY_PROPERTY_METADATA(bool, AppMenuFocused)
END_METADATA
