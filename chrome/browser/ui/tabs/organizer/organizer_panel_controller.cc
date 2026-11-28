// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"

#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/actions_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_host.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/site_engagement/content/site_engagement_helper.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

DEFINE_USER_DATA(OrganizerPanelController);

// Handles the work of placing the panel view in the correct host based on
// browser state. This is the organizer tray view.
class OrganizerPanelController::PanelViewManager {
 public:
  PanelViewManager(OrganizerPanelController& controller,
                   BrowserWindowInterface& browser)
      : controller_(controller), browser_(browser) {}

  std::unique_ptr<views::View> SetPanelView(
      std::unique_ptr<views::View> panel_view) {
    const auto old_location = current_location_;
    std::unique_ptr<views::View> old_panel = RemovePanelView();
    if (panel_view) {
      AddPanelView(std::move(panel_view));
    }
    if (current_location_ != old_location) {
      controller_->NotifyStateChanged();
    }
    return old_panel;
  }

  OrganizerPanelLocation current_location() const { return current_location_; }

 private:
  // Removes the panel view from its current host.
  std::unique_ptr<views::View> RemovePanelView() {
    auto* const panel_view = panel_view_.view();
    panel_view_.SetView(nullptr);
    current_location_ = OrganizerPanelLocation::kNone;
    if (!panel_view) {
      return nullptr;
    }
    auto* const old_host = OrganizerPanelHost::FromView(panel_view);
    CHECK(old_host) << "Panel was present but not in a host.";
    return old_host->TakeOrganizerPanelView();
  }

  // Adds the panel view to the correct host for the current browser state.
  void AddPanelView(std::unique_ptr<views::View> panel_view) {
    CHECK(panel_view);
    CHECK(!panel_view_);
    CHECK_EQ(OrganizerPanelLocation::kNone, current_location_);
    current_location_ = OrganizerPanelHost::GetPreferredLocation(*browser_);
    CHECK_NE(OrganizerPanelLocation::kNone, current_location_)
        << "Browser has no panel host";
    panel_view_.SetView(panel_view.get());
    auto* const host =
        OrganizerPanelHost::GetHostForLocation(*browser_, current_location_);
    CHECK(host) << "Panel host is missing.";
    host->SetOrganizerPanelView(std::move(panel_view));
  }

  const raw_ref<OrganizerPanelController> controller_;
  const raw_ref<BrowserWindowInterface> browser_;
  views::ViewTracker panel_view_;
  OrganizerPanelLocation current_location_ = OrganizerPanelLocation::kNone;
};

OrganizerPanelController::OrganizerPanelController(
    BrowserWindowInterface& browser_window,
    actions::ActionItem* root_action_item)
    : browser_window_(browser_window),
      root_action_item_(root_action_item),
      panel_view_manager_(
          std::make_unique<PanelViewManager>(*this, browser_window)),
      scoped_unowned_user_data_(browser_window.GetUnownedUserDataHost(),
                                *this) {
  UpdateOrganizerActionItem();
}

OrganizerPanelController::~OrganizerPanelController() = default;

// static
OrganizerPanelController* OrganizerPanelController::From(
    BrowserWindowInterface* browser_window) {
  return Get(browser_window->GetUnownedUserDataHost());
}

bool OrganizerPanelController::IsOrganizerPanelVisible() const {
  return is_visible_;
}

void OrganizerPanelController::SetOrganizerVisible(bool visible,
                                                   bool immediate) {
  if (is_visible_ == visible) {
    return;
  }

  is_visible_ = visible;
  base::RecordAction(base::UserMetricsAction(
      is_visible_ ? "OrganizerPanel.Open" : "OrganizerPanel.Close"));
  if (is_visible_) {
    UpdatePanelWebContents();
  }
  if (immediate) {
    BrowserAnimationController::From(&*browser_window_)
        ->Reset(OrganizerPanelAnimations::kOrganizerPanel, GetTargetMotion());
  } else {
    BrowserAnimationController::From(&*browser_window_)
        ->Start(OrganizerPanelAnimations::kOrganizerPanel, GetTargetMotion());
  }

  NotifyStateChanged();
}

OrganizerPanelLocation
OrganizerPanelController::GetCurrentOrganizerPanelLocation() const {
  return panel_view_manager_->current_location();
}

BrowserAnimationMotion OrganizerPanelController::GetTargetMotion() const {
  // Shuttle: no vertical tab strip, so the panel only lives in the tray.
  return is_visible_ ? OrganizerPanelAnimations::kShowInTray
                     : OrganizerPanelAnimations::kHideInTray;
}

base::CallbackListSubscription OrganizerPanelController::RegisterOnStateChanged(
    StateChangedCallback callback) {
  return on_state_changed_callback_list_.Add(std::move(callback));
}

content::WebContents* OrganizerPanelController::EnsureWebContentsCreated() {
  if (web_contents_) {
    return web_contents_.get();
  }

  auto* const preload_manager = WebUIContentsPreloadManager::GetInstance();
  CHECK(preload_manager);
  WebUIContentsPreloadManager::RequestResult result = preload_manager->Request(
      GURL(chrome::kChromeUIOrganizerPanelURL), browser_window_->GetProfile());
  web_contents_ = std::move(result.web_contents);
  webui::SetBrowserWindowInterface(web_contents_.get(), &*browser_window_);
  web_contents_->SetSupportsDraggableRegions(true);
  if (site_engagement::SiteEngagementService::IsEnabled()) {
    site_engagement_helper_ =
        std::make_unique<site_engagement::SiteEngagementService::Helper>(
            web_contents_.get());
  }
  return web_contents_.get();
}

void OrganizerPanelController::RegisterPanelView(
    OrganizerPanelView* panel_view) {
  CHECK(!panel_view_);
  CHECK(!is_visible_);
  panel_view_ = panel_view;
}

void OrganizerPanelController::UnregisterPanelView(
    OrganizerPanelView* panel_view) {
  CHECK_EQ(panel_view_, panel_view);
  panel_view_ = nullptr;
}

void OrganizerPanelController::UpdatePanelWebContents() {
  if (!is_visible_ || !panel_view_) {
    return;
  }
  if (content::WebContents* const contents = EnsureWebContentsCreated()) {
    panel_view_->AttachWebContents(contents);
  }
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelView(
    base::PassKey<BrowserView>,
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

std::unique_ptr<views::View> OrganizerPanelController::SetPanelViewForTesting(
    std::unique_ptr<views::View> panel_view) {
  return panel_view_manager_->SetPanelView(std::move(panel_view));
}

void OrganizerPanelController::NotifyStateChanged() {
  UpdateOrganizerActionItem();
  on_state_changed_callback_list_.Notify(this);
}

void OrganizerPanelController::UpdateOrganizerActionItem() {
  actions::ActionItem* organizer_action =
      actions::ActionManager::Get().FindAction(kActionToggleOrganizerPanel,
                                               root_action_item_);
  if (!organizer_action) {
    return;
  }

  // Shuttle: the fork keeps its View/Hide Organizer label and grid icon
  // (no vertical tab strip, so upstream's tab-search / close-tab-search
  // variants are cut).
  const auto& text = IsOrganizerPanelVisible() ? IDS_HIDE_ORGANIZER_PANEL
                                               : IDS_VIEW_ORGANIZER_PANEL;
  const std::u16string title_and_tooltip =
      chrome::GetCleanText(l10n_util::GetStringUTF16(text));
  organizer_action->SetText(title_and_tooltip);
  organizer_action->SetTooltipText(title_and_tooltip);

  const gfx::VectorIcon& icon = features::IsRoundedIconsEnabled()
                                    ? kGridViewIcon
                                    : kSavedTabGroupBarEverythingOldIcon;
  organizer_action->SetImage(
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon));
}
