// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_WINDOW_H_
#define CHROME_BROWSER_UI_BROWSER_WINDOW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window_deleter.h"
#include "chrome/browser/ui/download_close_type.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/base/base_window.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/native_ui_types.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#error This file should only be included on desktop.
#endif

class BrowserView;
class BrowserWindowInterface;
class DownloadBubbleUIController;
class ExclusiveAccessContext;
class FindBar;
class GURL;
class LocationBar;
class StatusBubble;


namespace content {
class EyeDropper;
class EyeDropperListener;
class RenderFrameHost;
class WebContents;
struct DropData;
enum class KeyboardEventProcessingResult;
}  // namespace content

namespace input {
struct NativeWebKeyboardEvent;
}  // namespace input

namespace gfx {
class Size;
}

namespace ui {
class AcceleratorProvider;
class ColorProvider;
class NativeTheme;
class ThemeProvider;
}  // namespace ui

namespace web_modal {
class WebContentsModalDialogHost;
}


enum class BrowserThemeChangeType {
  // User changes the browser theme.
  kBrowserTheme,
  // User changes the OS native theme.
  kNativeTheme,
  // A web app sets a theme color at launch, or changes theme color.
  kWebAppTheme
};

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow interface
//  An interface implemented by the "view" of the Browser window.
//  This interface includes ui::BaseWindow methods as well as Browser window
//  specific methods.
//
// NOTE: All getters may return NULL.
//
class BrowserWindow : public ui::BaseWindow {
 public:
  virtual ~BrowserWindow() = default;

  //////////////////////////////////////////////////////////////////////////////
  // ui::BaseWindow interface notes:

  // Closes the window as soon as possible. If the window is not in a drag
  // session, it will close immediately; otherwise, it will move offscreen (so
  // events are still fired) until the drag ends, then close. This assumes
  // that the Browser is not immediately destroyed, but will be eventually
  // destroyed by other means (eg, the tab strip going to zero elements).
  // Bad things happen if the Browser dtor is called directly as a result of
  // invoking this method.
  // virtual void Close() = 0;

  // virtual void Show() = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Browser specific methods:

  // Returns the browser window currently hosting `web_contents`. If no browser
  // window exists this returns null.
  static BrowserWindow* FindBrowserWindowWithWebContents(
      content::WebContents* web_contents);

  // Returns the BrowserWindow associated with `browser`, or nullptr if one has
  // not been created yet (e.g. during Browser construction) or `browser` has
  // no associated NativeWindow.
  //
  // Prefer this over `Browser::window()` (which is being removed; see
  // https://crbug.com/496674143). When you need the concrete subclass, call
  // `BrowserView::GetBrowserViewForBrowser()` or
  // `WebUIBrowserWindow::FromBrowser()` directly instead.
  static BrowserWindow* FromBrowser(BrowserWindowInterface* browser);
  static const BrowserWindow* FromBrowser(
      const BrowserWindowInterface* browser);

  // Returns true if the browser window is on the current workspace (a.k.a.
  // virtual desktop) or if we can't tell. False otherwise.
  //
  // On Windows, it must not be called while application is dispatching an input
  // synchronous call like SendMessage, because IsWindowOnCurrentVirtualDesktop
  // will return an error.
  virtual bool IsOnCurrentWorkspace() const = 0;

  // Returns true if the browser window is visible on the screen.
  virtual bool IsVisibleOnScreen() const = 0;

  // Sets the shown |ratio| of the browser's top controls (a.k.a. top-chrome) as
  // a result of gesture scrolling in |web_contents|.
  virtual void SetTopControlsShownRatio(content::WebContents* web_contents,
                                        float ratio) = 0;

  // Whether or not the renderer's viewport size should be shrunk by the height
  // of the browser's top controls.
  // As top-chrome is slided up or down, we don't actually resize the web
  // contents (for perf reasons) but we have to do a bunch of adjustments on the
  // renderer side to make it appear to the user like we're resizing things
  // smoothly:
  //
  // 1) Expose content beyond the web contents rect by expanding the clip.
  // 2) Push bottom-fixed elements around until we get a resize. As top-chrome
  //    hides, we push the fixed elements down by an equivalent amount so that
  //    they appear to stay fixed to the viewport bottom.
  //
  // Only when the user releases their finger to finish the scroll do we
  // actually resize the web contents and clear these adjustments. So web
  // contents has two possible sizes, viewport filling and shrunk by the top
  // controls.
  //
  // The GetTopControlsHeight is a static number that never changes (as long as
  // the top-chrome slide with gesture scrolls feature is enabled). To get the
  // actual "showing" height as the user sees, you multiply this by the shown
  // ratio. However, it's not enough to know this value, the renderer also needs
  // to know which direction it should be doing the above-mentioned adjustments.
  // That's what the DoBrowserControlsShrinkRendererSize bit is for. It tells
  // the renderer whether it's currently in the "viewport filling" or the
  // "shrunk by top controls" state.
  // The returned value should never change while sliding top-chrome is in
  // progress (either due to an in-progress gesture scroll, or due to a
  // renderer-initiated animation of the top controls shown ratio).
  virtual bool DoBrowserControlsShrinkRendererSize(
      const content::WebContents* contents) const = 0;

  // Returns the native theme associated with the frame.
  virtual ui::NativeTheme* GetNativeTheme() = 0;

  // Returns the ThemeProvider associated with the frame.
  virtual const ui::ThemeProvider* GetThemeProvider() const = 0;

  // Returns the ColorProvider associated with the frame.
  virtual const ui::ColorProvider* GetColorProvider() const = 0;

  // Returns the height of the browser's top controls. This height doesn't
  // change with the current shown ratio above. Renderers will call this to
  // calculate the top-chrome shown ratio from the gesture scroll offset.
  //
  // Note: This should always return 0 if hiding top-chrome with page gesture
  // scrolls is disabled. This is needed so the renderer scrolls the page
  // immediately rather than changing the shown ratio, thinking that top-chrome
  // and the page's top edge are moving.
  virtual int GetTopControlsHeight() const = 0;

  // Propagates to the browser that gesture scrolling has changed state.
  virtual void SetTopControlsGestureScrollInProgress(bool in_progress) = 0;

  // Return the status bubble(s) associated with the frame. In a split view,
  // there will be multiple status bubbles with the active one listed first.
  // It is possible for this to be empty.
  virtual std::vector<StatusBubble*> GetStatusBubbles() = 0;

  // Inform the frame that the selected tab favicon or title has changed. Some
  // frames may need to refresh their title bar.
  virtual void UpdateTitleBar() = 0;

  // Update any loading animations running in the window. |is_visible| is true
  // if the window is visible.
  virtual void UpdateLoadingAnimations(bool is_visible) = 0;

  // Called when the active tab changes.  Subclasses which implement
  // TabStripModelObserver should implement this instead of ActiveTabChanged();
  // the Browser will call this method while processing that one.
  virtual void OnActiveTabChanged(content::WebContents* old_contents,
                                  content::WebContents* new_contents,
                                  int index,
                                  int reason) = 0;

  // Called when a tab is detached. Subclasses which implement
  // TabStripModelObserver should implement this instead of processing this
  // in OnTabStripModelChanged(); the Browser will call this method.
  virtual void OnTabDetached(content::WebContents* contents,
                             bool was_active) = 0;

  // Returns the size of `WebContents` in the browser. This may be called before
  // the `TabStripModel` has an active tab.
  // Returns the size of the active `WebContents` if in a split view.
  virtual gfx::Size GetContentsSize() const = 0;

  // Resizes the window to fit `WebContents` of a certain size. This should only
  // be called after the `TabStripModel` has an active tab.
  // If in a split view, this will resize the active `WebContents` to the
  // specified size, while preserving the size ratio of the other
  // `WebContents`.
  virtual void SetContentsSize(const gfx::Size& size) = 0;


  // Returns the location bar.
  virtual LocationBar* GetLocationBar() const = 0;

  // Returns the AcceleratorProvider for this window, or null if the window
  // does not provide accelerators (notably TestBrowserWindow).
  virtual ui::AcceleratorProvider* GetAcceleratorProvider() = 0;

  // Tries to focus the location bar.  Clears the window focus (to avoid
  // inconsistent state) if this fails.
  virtual void SetFocusToLocationBar(bool is_user_initiated) = 0;

  // Informs the view whether or not a load is in progress for the current tab.
  // The view can use this notification to update the reload/stop button.
  virtual void UpdateReloadStopState(bool is_loading, bool force) = 0;

  // Updates the toolbar with the state for the specified |contents|.
  virtual void UpdateToolbar(content::WebContents* contents) = 0;

  // Updates the toolbar's visible security state. Returns true if the toolbar
  // was redrawn.
  virtual bool UpdateToolbarSecurityState() = 0;

  // Resets the toolbar's tab state for |contents|.
  virtual void ResetToolbarTabState(content::WebContents* contents) = 0;

  // Focuses the toolbar (for accessibility).
  virtual void FocusToolbar() = 0;

  // Called from toolbar subviews during their show/hide animations.
  virtual void ToolbarSizeChanged(bool is_animating) = 0;

  // Called when the associated window's tab dragging status changed.
  virtual void TabDraggingStatusChanged(bool is_dragging) = 0;

  // Called when a link is opened in the window from a user gesture.
  // Link will be opened with |disposition|.
  // TODO(crbug.com/40719979): see if this can't be piped through TabStripModel
  // events instead.
  virtual void LinkOpeningFromGesture(WindowOpenDisposition disposition) = 0;

  // Focuses the app menu like it was a menu bar.
  //
  // Not used on the Mac, which has a "normal" menu bar.
  virtual void FocusAppMenu() = 0;

  // Returns whether the tab strip is editable (for extensions).
  virtual bool IsTabStripEditable() const = 0;

  // Forces the tab strip into a not editable state for testing.
  virtual void DisableTabStripEditingForTesting() = 0;

  // Returns whether the toolbar is available or not. It's called "Visible()"
  // to follow the name convention. But it does not indicate the visibility of
  // the toolbar, i.e. toolbar may be hidden, and only visible when the mouse
  // cursor is at a certain place.
  // TODO(zijiehe): Rename Visible() functions into Available() to match their
  // original meaning.
  virtual bool IsToolbarVisible() const = 0;

  // Returns whether the toolbar is showing up on the screen.
  // TODO(zijiehe): Rename this function into IsToolbarVisible() once other
  // Visible() functions are renamed to Available().
  virtual bool IsToolbarShowing() const = 0;

  // Returns whether the location bar is visible.
  virtual bool IsLocationBarVisible() const = 0;

  // Shows the Update Recommended dialog box.
  virtual void ShowUpdateChromeDialog() = 0;



#if BUILDFLAG(IS_CHROMEOS)
  // Toggles the multitask menu on the browser frame size button.
  virtual void ToggleMultitaskMenu() = 0;
#endif  // BUILDFLAG(IS_CHROMEOS)


  // Returns the DownloadBubbleUIController. Returns null if Download Bubble
  // UI is not enabled, or if the download toolbar button does not exist.
  virtual DownloadBubbleUIController* GetDownloadBubbleUIController() = 0;

  // Shows the confirmation dialog box warning that the browser is closing with
  // in-progress downloads.
  // This method should call |callback| with the user's response.
  virtual void ConfirmBrowserCloseWithPendingDownloads(
      int download_count,
      DownloadCloseType dialog_type,
      base::OnceCallback<void(bool)> callback) = 0;

  // Shows the app menu (for accessibility).
  virtual void ShowAppMenu() = 0;

  // Allows the BrowserWindow object to handle a mouse drag update
  // before sending it to the renderer.
  // `point` is relative to the content view.
  virtual void PreHandleDragUpdate(const content::DropData& drop_data,
                                   const gfx::PointF& point) = 0;
  virtual void PreHandleDragExit() = 0;
  virtual void HandleDragEnded() = 0;
  // Allows the BrowserWindow object to handle the specified keyboard event
  // before sending it to the renderer.
  virtual content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Allows the BrowserWindow object to handle the specified keyboard event,
  // if the renderer did not process it.
  virtual bool HandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event) = 0;

  // Construct a FindBar implementation for the |browser|.
  virtual std::unique_ptr<FindBar> CreateFindBar() = 0;

  // Return the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs within the browser window. This can sometimes be NULL (for
  // instance during tab drag on Views/Win32).
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHost() = 0;

  // Return the WebContentsModalDialogHost for use in positioning web contents
  // modal dialogs relative to its corresponding container view if possible,
  // otherwise falls back to returning the WebContentsModalDialogHost that is
  // responsible for modal positioning relative to the browser window.
  virtual web_modal::WebContentsModalDialogHost*
  GetWebContentsModalDialogHostFor(content::WebContents* web_contents) = 0;

  // Construct a BrowserWindow implementation for the specified |browser|.
  static std::unique_ptr<BrowserWindow, BrowserWindowDeleter>
  CreateBrowserWindow(BrowserWindowInterface* browser,
                      bool user_gesture,
                      bool in_tab_dragging);


  // Returns object implementing ExclusiveAccessContext interface.
  virtual ExclusiveAccessContext* GetExclusiveAccessContext() = 0;

  // Returns the platform-specific ID of the workspace the browser window
  // currently resides in.
  virtual std::string GetWorkspace() const = 0;
  virtual bool IsVisibleOnAllWorkspaces() const = 0;

  // Shows the platform specific emoji picker.
  virtual void ShowEmojiPanel() = 0;

  // Opens the eye dropper.
  virtual std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) = 0;

  // Shows a confirmation dialog about enabling caret browsing.
  virtual void ShowCaretBrowsingDialog() = 0;


  // Returns true when the window should be in unframed display mode. Only
  // supported in IWAs.
  virtual bool IsUnframedModeEnabled() const = 0;

  // Returns the overall resizability of the `BrowserView` when considering
  // both the value set by the `window.setResizable(bool)` API and browser's
  // "native" resizability.
  virtual bool GetCanResize() = 0;

  virtual ui::mojom::WindowShowState GetWindowShowState() const = 0;

  // Shows the Chrome Labs bubble if enabled.
  virtual void ShowChromeLabs() = 0;

  // In production code BrowserView is the only subclass for BrowserWindow. The
  // fact that this is not true in some tests is a problem with the tests. See
  // https://crbug.com/360163254.
  virtual BrowserView* AsBrowserView() = 0;

 protected:
  friend struct BrowserWindowDeleter;
  // Deletes `this`. Note BrowserWindow will no longer be valid after this
  // returns.
  virtual void DeleteBrowserWindow() = 0;
};

#endif  // CHROME_BROWSER_UI_BROWSER_WINDOW_H_
