// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tab_strip_model_delegate.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_web_contents_delegate/browser_web_contents_delegate.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_menu_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/unload_controller.h"
#include "chrome/browser/ui/window_feature_controller/window_feature_controller.h"
#include "chrome/common/chrome_switches.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace {

}  // namespace

namespace chrome {

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, public:

BrowserTabStripModelDelegate::BrowserTabStripModelDelegate(
    BrowserWindowInterface* browser)
    : browser_(browser) {}

BrowserTabStripModelDelegate::~BrowserTabStripModelDelegate() = default;


////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, TabStripModelDelegate implementation:

void BrowserTabStripModelDelegate::AddTabAt(
    const GURL& url,
    int index,
    bool foreground,
    std::optional<tab_groups::TabGroupId> group,
    bool pinned) {
  chrome::AddTabAt(browser_, url, index, foreground, group, pinned);
}

BrowserWindowInterface* BrowserTabStripModelDelegate::CreateNewStripWithTabs(
    std::vector<NewStripContents> tabs,
    const gfx::Rect& window_bounds,
    bool maximize) {
  DCHECK(WindowFeatureController::From(browser_)->CanSupportWindowFeature(
      WindowFeatureController::WindowFeature::kFeatureTabStrip));

  // Create an empty new browser window the same size as the old one.
  BrowserWindowCreateParams params(browser_->GetProfile(), true);
  params.initial_bounds = window_bounds;
  params.initial_show_state = maximize ? ui::mojom::WindowShowState::kMaximized
                                       : ui::mojom::WindowShowState::kNormal;
  BrowserWindowInterface* browser = CreateBrowserWindow(std::move(params));
  TabStripModel* new_model = browser->GetTabStripModel();

  for (size_t i = 0; i < tabs.size(); ++i) {
    NewStripContents item = std::move(tabs[i]);

    // Enforce that there is an active tab in the strip at all times by forcing
    // the first web contents to be marked as active.
    if (i == 0) {
      item.add_types |= AddTabTypes::ADD_ACTIVE;
    }

    content::WebContents* const raw_web_contents =
        item.tab.get()->GetContents();
    new_model->InsertDetachedTabAt(static_cast<int>(i), std::move(item.tab),
                                   item.add_types);
    // Make sure the loading state is updated correctly, otherwise the throbber
    // won't start if the page is loading.
    // TODO(beng): find a better way of doing this.
    BrowserWebContentsDelegate::From(browser)->LoadingStateChanged(
        raw_web_contents, true);
  }

  return browser;
}

void BrowserTabStripModelDelegate::WillAddWebContents(
    content::WebContents* contents) {
  TabHelpers::AttachTabHelpers(contents);
}

int BrowserTabStripModelDelegate::GetDragActions() const {
  return TabStripModelDelegate::TAB_TEAROFF_ACTION |
         (browser_->GetTabStripModel()->count() > 1
              ? TabStripModelDelegate::TAB_MOVE_ACTION
              : 0);
}

bool BrowserTabStripModelDelegate::CanDuplicateContentsAt(int index) {
  return CanDuplicateTabAt(browser_, index);
}

bool BrowserTabStripModelDelegate::IsTabStripEditable() {
  return BrowserWindow::FromBrowser(browser_)->IsTabStripEditable();
}

content::WebContents* BrowserTabStripModelDelegate::DuplicateContentsAt(
    int index) {
  return DuplicateTabAt(browser_, index);
}

void BrowserTabStripModelDelegate::DuplicateSplit(
    split_tabs::SplitTabId split) {
  chrome::DuplicateSplit(browser_, split);
}

void BrowserTabStripModelDelegate::MoveToExistingWindow(
    const std::vector<int>& indices,
    int browser_index) {
  std::vector<BrowserWindowInterface*> existing_browsers =
      TabMenuModelDelegate::From(browser_)->GetOtherBrowserWindows(
          /*is_app=*/false);
  size_t existing_browser_count = existing_browsers.size();
  if (static_cast<size_t>(browser_index) < existing_browser_count &&
      existing_browsers[browser_index]) {
    chrome::MoveTabsToExistingWindow(browser_, existing_browsers[browser_index],
                                     indices);
  }
}

bool BrowserTabStripModelDelegate::CanMoveTabsToWindow(
    const std::vector<int>& indices) {
  return CanMoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveTabsToNewWindow(
    const std::vector<int>& indices) {
  // chrome:: to disambiguate the free function from this method.
  chrome::MoveTabsToNewWindow(browser_, indices);
}

void BrowserTabStripModelDelegate::MoveGroupToNewWindow(
    const tab_groups::TabGroupId& group) {
  TabGroupModel* group_model = browser_->GetTabStripModel()->group_model();
  if (!group_model || !group_model->ContainsTabGroup(group)) {
    return;
  }

  chrome::MoveGroupToNewWindow(browser_, group);
}

std::optional<SessionID> BrowserTabStripModelDelegate::CreateHistoricalTab(
    content::WebContents* contents) {
  if (!BrowserSupportsHistoricalEntries()) {
    return std::nullopt;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());

  // We only create historical tab entries for tabbed browser windows.
  if (service &&
      WindowFeatureController::From(browser_)->CanSupportWindowFeature(
          WindowFeatureController::WindowFeature::kFeatureTabStrip)) {
    return service->CreateHistoricalTab(
        sessions::ContentLiveTab::GetOrCreateForWebContents(contents),
        browser_->GetTabStripModel()->GetIndexOfWebContents(contents));
  }
  return std::nullopt;
}

void BrowserTabStripModelDelegate::CreateHistoricalGroup(
    const tab_groups::TabGroupId& group) {
  if (!BrowserSupportsHistoricalEntries()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->CreateHistoricalGroup(BrowserLiveTabContext::FindContextWithGroup(
                                       group, browser_->GetProfile()),
                                   group);
  }
}

void BrowserTabStripModelDelegate::CreateHistoricalSplit(
    const split_tabs::SplitTabId& split_id) {
  if (!BrowserSupportsHistoricalEntries()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->CreateHistoricalSplit(BrowserLiveTabContext::From(browser_),
                                   split_id);
  }
}

void BrowserTabStripModelDelegate::GroupAdded(
    const tab_groups::TabGroupId& group) {}

void BrowserTabStripModelDelegate::WillCloseGroup(
    const tab_groups::TabGroupId& group) {
  // Store updated information about the tab group in TabRestore.
  CreateHistoricalGroup(group);
}

void BrowserTabStripModelDelegate::WillCloseSplit(
    const split_tabs::SplitTabId& split_id) {
  CreateHistoricalSplit(split_id);
}

void BrowserTabStripModelDelegate::GroupCloseStopped(
    const tab_groups::TabGroupId& group) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->GroupCloseStopped(group);
  }
}

void BrowserTabStripModelDelegate::SplitClosed(
    const split_tabs::SplitTabId& split_id) {
  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->SplitClosed(split_id);
  }
}

void BrowserTabStripModelDelegate::SplitCloseStopped(
    const split_tabs::SplitTabId& split_id) {
  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (service) {
    service->SplitCloseStopped(split_id);
  }
}

bool BrowserTabStripModelDelegate::RunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return UnloadController::From(browser_)->RunUnloadListenerBeforeClosing(
      contents);
}

bool BrowserTabStripModelDelegate::ShouldRunUnloadListenerBeforeClosing(
    content::WebContents* contents) {
  return UnloadController::From(browser_)->ShouldRunUnloadListenerBeforeClosing(
      contents);
}

bool BrowserTabStripModelDelegate::CanReload() const {
  return chrome::CanReload(browser_);
}

bool BrowserTabStripModelDelegate::IsForWebApp() {
  return false;
}

void BrowserTabStripModelDelegate::CopyURL(content::WebContents* web_contents) {
  chrome::CopyURL(browser_, web_contents);
}

void BrowserTabStripModelDelegate::GoBack(content::WebContents* web_contents) {
  chrome::GoBack(web_contents);
}

bool BrowserTabStripModelDelegate::CanGoBack(
    content::WebContents* web_contents) {
  return chrome::CanGoBack(web_contents);
}

bool BrowserTabStripModelDelegate::IsNormalWindow() {
  return browser_->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
}

BrowserWindowInterface*
BrowserTabStripModelDelegate::GetBrowserWindowInterface() {
  return browser_;
}

void BrowserTabStripModelDelegate::NewSplitTab(
    std::vector<int> indices,
    split_tabs::SplitTabLayout layout,
    split_tabs::SplitTabCreatedSource source) {
  if (indices.empty()) {
    chrome::NewSplitTab(browser_, layout, source);
  } else {
    browser_->GetTabStripModel()->AddToNewSplit(
        indices, split_tabs::SplitTabVisualData(layout), source);
  }
}

void BrowserTabStripModelDelegate::OnGroupsDestruction(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> close_callback,
    bool delete_groups) {
  // No saved tab groups: groups are always deleted immediately.
  std::move(close_callback).Run();
}

void BrowserTabStripModelDelegate::OnRemovingAllTabsFromGroups(
    const std::vector<tab_groups::TabGroupId>& group_ids,
    base::OnceCallback<void()> callback) {
  std::move(callback).Run();
}

void BrowserTabStripModelDelegate::CloseTab(
    const tabs::TabInterface* tab_interface,
    CloseTabSource source,
    base::OnceCallback<void(CloseTabSource)> on_approved) {
  TabStripModel* model = browser_->GetTabStripModel();
  std::optional<int> maybe_tab_index = model->GetIndexOfTab(tab_interface);
  if (!maybe_tab_index.has_value()) {
    return;
  }
  int tab_index = maybe_tab_index.value();


#if BUILDFLAG(IS_CHROMEOS)
  // Tabs cannot be closed when the app is in locked fullscreen, which is
  // available only on ChromeOS.
  if (platform_util::IsBrowserLockedFullscreen(browser_)) {
    return;
  }
#endif


  auto [cb1, cb2] = base::SplitOnceCallback(std::move(on_approved));

  // Only consider pausing the close operation if this is the last remaining
  // tab (since otherwise closing it won't close the browser window).
  if (model->count() <= 1) {
    // Closing this tab will close the current window. See if the browser wants
    // to prompt the user before the browser is allowed to close.
    const UnloadController::WarnBeforeClosingResult result =
        UnloadController::From(browser_)->MaybeWarnBeforeClosing(base::BindOnce(
            [](base::WeakPtr<BrowserTabStripModelDelegate> delegate,
               const tabs::TabInterface* tab,
               base::OnceCallback<void(CloseTabSource)> cb,
               UnloadController::WarnBeforeClosingResult result) {
              if (delegate &&
                  result ==
                      UnloadController::WarnBeforeClosingResult::kOkToClose) {
                delegate->CloseTab(tab, CloseTabSource::kFromNonUIEvent,
                                   std::move(cb));
              }
            },
            weak_factory_.GetWeakPtr(), tab_interface, std::move(cb1)));

    if (result != UnloadController::WarnBeforeClosingResult::kOkToClose) {
      return;
    }
  }

  // Check to make sure the tab is not the last in its group.
  std::vector<tab_groups::TabGroupId> groups_to_delete =
      model->GetGroupsDestroyedFromRemovingIndices({tab_index});

  auto do_close = base::BindOnce(
      [](base::WeakPtr<BrowserTabStripModelDelegate> delegate,
         base::WeakPtr<content::WebContents> web_contents,
         base::OnceCallback<void(CloseTabSource)> on_approved,
         CloseTabSource source) {
        if (!delegate) {
          return;
        }
        BrowserWindowInterface* browser = delegate->browser_;
        TabStripModel* model = browser->GetTabStripModel();

        if (on_approved) {
          std::move(on_approved).Run(source);
        }

        if (!web_contents) {
          return;
        }
        int tab_index = model->GetIndexOfWebContents(web_contents.get());
        if (tab_index == TabStripModel::kNoTab) {
          return;
        }

        const tabs::TabInterface* tab = model->GetTabAtIndex(tab_index);

        if (tab->GetGroup().has_value()) {
          base::RecordAction(base::UserMetricsAction("CloseGroupedTab"));

          if (model->count() == 1) {
            // Prevent the browser from closing when the last grouped tab is
            // closed from the browser by adding a new tab.
            chrome::NewTab(browser, NewTabTypes::kNoUserAction);
            // In some situations the new tab is assigned a group. So if it is
            // in a group, we remove it from the group so that after closing the
            // tab at `tab_index`, the browser shows a tab without a group.
            model->RemoveFromGroup({1});
          }
        }

        model->CloseWebContents(web_contents.get(),
                                TabCloseTypes::CLOSE_USER_GESTURE |
                                    TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
      },
      weak_factory_.GetWeakPtr(), tab_interface->GetContents()->GetWeakPtr(),
      std::move(cb2));

  if (groups_to_delete.empty()) {
    std::move(do_close).Run(source);
    return;
  }

  std::move(do_close).Run(source);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserTabStripModelDelegate, private:

void BrowserTabStripModelDelegate::CloseFrame() {
  browser_->GetWindow()->Close();
}

bool BrowserTabStripModelDelegate::BrowserSupportsHistoricalEntries() {
  // We don't create historical tabs for incognito windows or windows without
  // profiles.
  return browser_->GetProfile() && !browser_->GetProfile()->IsOffTheRecord();
}

}  // namespace chrome
