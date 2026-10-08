// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/history_clusters/clusters.js';
import 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './history_list.js';
import './history_toolbar.js';
import './history_filter_chips.js';
import './query_manager.js';
import './router.js';
import './side_bar.js';
import '/strings.m.js';

import {ColorChangeUpdater, COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {HistoryResultType} from 'chrome://resources/cr_components/history/constants.js';
import type {PageCallbackRouter, QueryResult, QueryState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import type {CrPageSelectorElement} from 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import {FindShortcutMixinLit} from 'chrome://resources/cr_elements/find_shortcut_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import {HistoryPageViewHistogram} from './constants.js';
import type {HistoryListElement} from './history_list.js';
import type {HistoryToolbarElement} from './history_toolbar.js';
import {Page, TABBED_PAGES} from './router.js';
import type {HistoryRouterElement} from './router.js';
import type {FooterInfo, HistorySideBarElement} from './side_bar.js';

// Click/auxclick listeners to intercept any link clicks. If the link points
// to a chrome: or file: url, then calls into the browser to do the
// navigation.
function onDocumentClick(evt: Event) {
  const e = evt as MouseEvent;
  // Ignore buttons other than left and middle.
  if (e.button > 1 || e.defaultPrevented) {
    return;
  }

  const eventPath = e.composedPath() as HTMLElement[];
  let anchor: HTMLAnchorElement|null = null;
  if (eventPath) {
    for (const element of eventPath) {
      if (element.tagName === 'A' && (element as HTMLAnchorElement).href) {
        anchor = element as HTMLAnchorElement;
        break;
      }
    }
  }

  // Fallback if Event.path is not available.
  let el = e.target as HTMLElement;
  if (!anchor && el.nodeType === Node.ELEMENT_NODE &&
      el.webkitMatchesSelector('A, A *')) {
    while (el.tagName !== 'A') {
      el = el.parentElement as HTMLElement;
    }
    anchor = el as HTMLAnchorElement;
  }

  if (!anchor) {
    return;
  }

  if ((anchor.protocol === 'file:' || anchor.protocol === 'about:') &&
      (e.button === 0 || e.button === 1)) {
    BrowserProxyImpl.getInstance().navigateToUrl(anchor.href, anchor.target, e);
    e.preventDefault();
  }
}
export interface HistoryAppElement {
  $: {
    content: CrPageSelectorElement,
    contentSideBar: HistorySideBarElement,
    drawer: CrLazyRenderLitElement<CrDrawerElement>,
    history: HistoryListElement,
    tabsContainer: HTMLElement,
    tabsContent: CrPageSelectorElement,
    toolbar: HistoryToolbarElement,
    tabsScrollContainer: HTMLElement,
    router: HistoryRouterElement,
  };
}

const HistoryAppElementBase =
    FindShortcutMixinLit(WebUiListenerMixinLit(CrLitElement));

export class HistoryAppElement extends HistoryAppElementBase {
  static get is() {
    return 'history-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contentPage_: {type: String},
      tabsContentPage_: {type: String},
      // The id of the currently selected page.
      selectedPage_: {type: String},
      queryResult_: {type: Object},
      pendingDelete_: {type: Boolean},
      queryState_: {type: Object},
      // True if the window is narrow enough for the page to have a drawer.
      hasDrawer_: {type: Boolean},
      footerInfo: {type: Object},
      historyClustersEnabled_: {type: Boolean},
      historyClustersVisible_: {type: Boolean},
      lastSelectedTab_: {type: Number},
      showHistoryClusters_: {
        type: Boolean,
        reflect: true,
      },
      showTabs_: {type: Boolean},
      // The index of the currently selected tab.
      selectedTab_: {type: Number},
      tabsIcons_: {type: Array},
      tabsNames_: {type: Array},
      scrollTarget_: {type: Object},
      queryStateAfterDate_: {type: Object},
      tabContentScrollOffset_: {type: Number},
      numCharsTypedInSearch_: {type: Number},
      includeActorVisits_: {type: Boolean},
      includeUserVisits_: {type: Boolean},
      isBrowsingHistoryActorIntegrationM3Enabled_: {type: Boolean},
      isGlicWebActuationAvailable_: {type: Boolean},
    };
  }

  accessor footerInfo: FooterInfo = {
    managed: loadTimeData.getBoolean('isManaged'),
    otherFormsOfHistory: false,
    geminiAppsActivity: loadTimeData.getBoolean('isGlicEnabled'),
  };
  protected accessor hasDrawer_: boolean = false;
  protected accessor historyClustersEnabled_: boolean =
      loadTimeData.getBoolean('isHistoryClustersEnabled');
  protected accessor historyClustersVisible_: boolean =
      loadTimeData.getBoolean('isHistoryClustersVisible');
  protected accessor lastSelectedTab_: number =
      loadTimeData.getInteger('lastSelectedTab');
  protected accessor contentPage_: string = Page.HISTORY;
  protected accessor tabsContentPage_: string = Page.HISTORY;
  protected accessor pendingDelete_: boolean = false;
  protected accessor queryResult_: QueryResult = {
    info: null,
    value: [],
  };
  protected accessor queryState_: QueryState = {
    incremental: false,
    querying: false,
    searchTerm: '',
    after: null,
    includeUserVisits: true,
    includeActorVisits: true,
  };
  protected accessor selectedPage_: string = Page.HISTORY;
  protected accessor selectedTab_: number =
      loadTimeData.getInteger('lastSelectedTab') || 0;
  protected accessor showTabs_: boolean = false;
  protected accessor showHistoryClusters_: boolean = false;
  protected accessor tabsIcons_: string[] = [
    (document.documentElement.hasAttribute('webui-rounded-icons') ?
         'images/list.svg' :
         'images/list_old.svg'),
    (document.documentElement.hasAttribute('webui-rounded-icons') ?
         'chrome://resources/images/icon_journeys.svg' :
         'chrome://resources/images/icon_journeys_old.svg'),
  ];
  protected accessor tabsNames_: string[] = [
    loadTimeData.getString('historyListTabLabel'),
    loadTimeData.getString('historyClustersTabLabel'),
  ];
  protected accessor scrollTarget_: HTMLElement = document.body;
  protected accessor queryStateAfterDate_: Date|undefined;
  protected accessor tabContentScrollOffset_: number = 0;
  protected accessor numCharsTypedInSearch_: number = 0;
  protected accessor includeActorVisits_: boolean = true;
  protected accessor includeUserVisits_: boolean = true;
  protected accessor isBrowsingHistoryActorIntegrationM3Enabled_: boolean =
      loadTimeData.getBoolean('isBrowsingHistoryActorIntegrationM3Enabled');
  protected accessor isGlicWebActuationAvailable_: boolean =
      loadTimeData.getBoolean('isGlicWebActuationAvailable');

  private callbackRouter_: PageCallbackRouter;
  private dataFromNativeBeforeInput_: string|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private historyClustersViewStartTime_: Date|null = null;
  private lastRecordedSelectedPageHistogramValue_: HistoryPageViewHistogram =
      HistoryPageViewHistogram.END;
  private onHasOtherFormsChangedListenerId_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();

    const enableWebuiRefresh2026 =
        loadTimeData.getString('webuiRefresh2026') !== '';
    if (enableWebuiRefresh2026) {
      this.addThemedColors_();
      ColorChangeUpdater.forDocument().start();
    }

    this.eventTracker_.add(document, 'click', onDocumentClick);
    this.eventTracker_.add(document, 'auxclick', onDocumentClick);
    this.eventTracker_.add(
        document, 'keydown', (e: Event) => this.onKeyDown_(e as KeyboardEvent));
    this.eventTracker_.add(
        document, 'visibilitychange', this.onVisibilityChange_.bind(this));
    this.eventTracker_.add(
        document, 'record-history-link-click',
        this.onRecordHistoryLinkClick_.bind(this));
    this.shadowRoot.querySelector('history-query-manager')!.initialize();

    const mediaQuery = window.matchMedia('(max-width: 1023px)');
    this.hasDrawer_ = mediaQuery.matches;
    this.eventTracker_.add(
        mediaQuery, 'change',
        (e: MediaQueryListEvent) => this.hasDrawer_ = e.matches);

    this.onHasOtherFormsChangedListenerId_ =
        this.callbackRouter_.onHasOtherFormsChanged.addListener(
            (hasOtherForms: boolean) =>
                this.onHasOtherFormsChanged_(hasOtherForms));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();

    if (this.onHasOtherFormsChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.onHasOtherFormsChangedListenerId_);
      this.onHasOtherFormsChangedListenerId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('historyClustersEnabled_') ||
        changedPrivateProperties.has('historyClustersVisible_')) {
      this.showHistoryClusters_ =
          this.historyClustersEnabled_ && this.historyClustersVisible_;
    }

    if (changedPrivateProperties.has('showHistoryClusters_')) {
      this.showTabs_ = this.showHistoryClusters_;
    }

    if (changedPrivateProperties.has('selectedTab_')) {
      this.lastSelectedTab_ = this.selectedTab_;
      // Change in the currently selected tab requires change in the currently
      // selected page.
      if (!this.selectedPage_ || TABBED_PAGES.includes(this.selectedPage_)) {
        this.selectedPage_ = TABBED_PAGES[this.selectedTab_]!;
      }
    }

    if (changedPrivateProperties.has('queryState_')) {
      if (this.queryState_.after) {
        const afterDate = new Date(this.queryState_.after + 'T00:00:00');
        // This compute function listens for any subproperty changes on the
        // queryState_ so the `after` param may not have changed.
        if (this.queryStateAfterDate_?.getTime() !== afterDate.getTime()) {
          this.queryStateAfterDate_ = afterDate;
        }
      } else {
        this.queryStateAfterDate_ = undefined;
      }
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addEventListener('cr-toolbar-menu-click', this.onCrToolbarMenuClick_);
    this.addEventListener('delete-selected', this.deleteSelected);
    this.addEventListener('open-selected', this.openSelected);
    this.addEventListener('history-checkbox-select', this.checkboxSelected);
    this.addEventListener('history-close-drawer', this.closeDrawer_);
    this.addEventListener('history-view-changed', this.historyViewChanged_);
    this.addEventListener('unselect-all', this.unselectAll);
  }

  getScrollTargetForTesting(): HTMLElement {
    return this.scrollTarget_;
  }

  protected getShowResultsByGroup_(): boolean {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS;
  }

  protected getShowHistoryList_(): boolean {
    return this.selectedPage_ === Page.HISTORY;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedTab_')) {
      BrowserProxyImpl.getInstance().handler.setLastSelectedTab(
          this.selectedTab_);
    }

    if (changedPrivateProperties.has('selectedPage_')) {
      this.selectedPageChanged_(
          changedPrivateProperties.get('selectedPage_') as string);
    }

    if (changedPrivateProperties.has('hasDrawer_')) {
      this.hasDrawerChanged_();
    }
  }

  protected historyClustersSelected_(): boolean {
    return this.selectedPage_ === Page.HISTORY_CLUSTERS &&
        this.showHistoryClusters_;
  }

  private onFirstRender_() {
    // Focus the search field on load. Done here to ensure the history page
    // is rendered before we try to take focus.
    const searchField = this.$.toolbar.searchField;
    if (!searchField.narrow) {
      searchField.getSearchInput().focus();
    }

    requestIdleCallback(function() {
      document.fonts.load('bold 12px Roboto');
    });
  }

  private onCrToolbarMenuClick_() {
    this.$.drawer.get().toggle();
  }

  /**
   * Listens for history-item being selected or deselected (through checkbox)
   * and changes the view of the top toolbar.
   */
  checkboxSelected() {
    this.$.toolbar.count = this.$.history.getSelectedItemCount();
  }

  selectOrUnselectAll() {
    this.$.history.selectOrUnselectAll();
    this.$.toolbar.count = this.$.history.getSelectedItemCount();
  }

  /**
   * Listens for call to cancel selection and loops through all items to set
   * checkbox to be unselected.
   */
  private unselectAll() {
    this.$.history.unselectAllItems();
    this.$.toolbar.count = 0;
  }

  deleteSelected() {
    this.$.history.deleteSelectedWithPrompt();
  }

  openSelected() {
    this.$.history.openSelected();
  }

  protected onQueryFinished_(e: CustomEvent<{result: QueryResult}>) {
    this.queryResult_ = e.detail.result;
    this.$.history.historyResult(e.detail.result.info!, e.detail.result.value);
    if (document.body.classList.contains('loading')) {
      document.body.classList.remove('loading');
      this.onFirstRender_();
    }
  }

  private onKeyDown_(e: KeyboardEvent) {
    if ((e.key === 'Delete' || e.key === 'Backspace') && !hasKeyModifiers(e)) {
      this.onDeleteCommand_();
      return;
    }

    if (e.key === 'a' && !e.altKey && !e.shiftKey) {
      let hasTriggerModifier = e.ctrlKey && !e.metaKey;
      // <if expr="is_macosx">
      hasTriggerModifier = !e.ctrlKey && e.metaKey;
      // </if>
      if (hasTriggerModifier && this.onSelectAllCommand_()) {
        e.preventDefault();
      }
    }

    if (e.key === 'Escape') {
      this.unselectAll();
      getAnnouncerInstance().announce(
          loadTimeData.getString('itemsUnselected'));
      e.preventDefault();
    }
  }

  private onVisibilityChange_() {
    if (this.selectedPage_ !== Page.HISTORY_CLUSTERS) {
      return;
    }

    if (document.visibilityState === 'hidden') {
      this.recordHistoryClustersDuration_();
    } else if (
        document.visibilityState === 'visible' &&
        this.historyClustersViewStartTime_ === null) {
      // Restart the timer if the user switches back to the History tab.
      this.historyClustersViewStartTime_ = new Date();
    }
  }

  private onRecordHistoryLinkClick_(
      e: CustomEvent<{resultType: HistoryResultType, index: number}>) {
    // All of the above code only applies to History search results, not the
    // zero-query state. Check queryResult_ instead of queryState_ to key on
    // actually displayed results rather than the latest user input, which may
    // not have finished loading yet.
    if (!this.queryResult_.info || !this.queryResult_.info.term) {
      return;
    }

    const browserProxy = BrowserProxyImpl.getInstance();
    browserProxy.recordHistogram(
        'History.SearchResultClicked.Type', e.detail.resultType,
        HistoryResultType.END);

    // MetricsHandler uses a 100 bucket limit, so the max index is 99.
    const boundary = 100;
    const clampedIndex = Math.min(e.detail.index, 99);
    browserProxy.recordHistogram(
        'History.SearchResultClicked.Index', clampedIndex, boundary);

    switch (e.detail.resultType) {
      case HistoryResultType.TRADITIONAL:
        browserProxy.recordHistogram(
            'History.SearchResultClicked.Index.Traditional', clampedIndex,
            boundary);
        break;
      case HistoryResultType.GROUPED:
        browserProxy.recordHistogram(
            'History.SearchResultClicked.Index.Grouped', clampedIndex,
            boundary);
        break;
      case HistoryResultType.EMBEDDINGS:
        browserProxy.recordHistogram(
            'History.SearchResultClicked.Index.Embeddings', clampedIndex,
            boundary);
        break;
      case HistoryResultType.END:
        break;
      default:
        assertNotReachedCase(e.detail.resultType);
    }
  }

  private onDeleteCommand_() {
    if (this.$.toolbar.count === 0 || this.pendingDelete_) {
      return;
    }
    this.deleteSelected();
  }

  /**
   * @return Whether the command was actually triggered.
   */
  private onSelectAllCommand_(): boolean {
    if (this.$.toolbar.searchField.isSearchFocused() ||
        this.historyClustersSelected_()) {
      return false;
    }
    this.selectOrUnselectAll();
    return true;
  }

  private onHasOtherFormsChanged_(hasOtherForms: boolean) {
    this.footerInfo = Object.assign(
        {}, this.footerInfo, {otherFormsOfHistory: hasOtherForms});
  }

  /**
   * @return Whether a loading spinner should be shown (implies the
   *     backend is querying a new search term).
   */
  protected shouldShowSpinner_(): boolean {
    return this.queryState_.querying && !this.queryState_.incremental &&
        this.queryState_.searchTerm !== '';
  }

  private updateContentPage_() {
    this.contentPage_ = Page.HISTORY;
  }

  private updateTabsContentPage_() {
    this.tabsContentPage_ =
        (this.selectedPage_ === Page.HISTORY_CLUSTERS &&
         this.historyClustersEnabled_ && this.historyClustersVisible_) ?
        Page.HISTORY_CLUSTERS :
        Page.HISTORY;
  }

  private selectedPageChanged_(oldPage: string) {
    this.updateContentPage_();
    this.updateTabsContentPage_();
    this.unselectAll();
    this.historyViewChanged_();
    this.maybeUpdateSelectedHistoryTab_();

    if (oldPage === Page.HISTORY_CLUSTERS &&
        this.selectedPage_ !== Page.HISTORY_CLUSTERS) {
      this.recordHistoryClustersDuration_();
    }
    if (this.selectedPage_ === Page.HISTORY_CLUSTERS) {
      this.historyClustersViewStartTime_ = new Date();
    }
  }

  protected onContentIronSelect_() {
    this.updateScrollTarget_();
  }

  protected onTabsContentIronSelect_() {
    this.updateScrollTarget_();
  }

  private updateScrollTarget_() {
    const topLevelIronPages = this.$.content;
    const topLevelHistoryPage = this.$.tabsContainer;
    if (topLevelIronPages.selectedItem &&
        topLevelIronPages.selectedItem === topLevelHistoryPage) {
      this.scrollTarget_ = this.$.tabsScrollContainer;

      // Scroll target won't change as the scroll target for both Date and Group
      // view is this.$.tabsScrollContainer, which means history-list's
      // callbacks to fill the viewport do not get triggered automatically.
      this.$.history.fillCurrentViewport();
    } else {
      this.scrollTarget_ = topLevelIronPages.selectedItem as HTMLElement;
    }
  }

  private maybeUpdateSelectedHistoryTab_() {
    // Change in the currently selected page may require change in the currently
    // selected tab.
    if (TABBED_PAGES.includes(this.selectedPage_)) {
      this.selectedTab_ = TABBED_PAGES.indexOf(this.selectedPage_);
    }
  }

  private historyViewChanged_() {
    this.recordHistoryPageView_();
  }

  // Records the history clusters page duration.
  private recordHistoryClustersDuration_() {
    assert(this.historyClustersViewStartTime_ !== null);

    const duration =
        new Date().getTime() - this.historyClustersViewStartTime_.getTime();
    BrowserProxyImpl.getInstance().recordLongTime(
        'History.Clusters.WebUISessionDuration', duration);

    this.historyClustersViewStartTime_ = null;
  }

  private hasDrawerChanged_() {
    const drawer = this.$.drawer.getIfExists();
    if (!this.hasDrawer_ && drawer && drawer.open) {
      drawer.cancel();
    }
  }

  private closeDrawer_() {
    const drawer = this.$.drawer.get();
    if (drawer && drawer.open) {
      drawer.close();
    }
  }

  private recordHistoryPageView_() {
    let histogramValue = HistoryPageViewHistogram.END;
    switch (this.selectedPage_) {
      case Page.HISTORY_CLUSTERS:
        histogramValue = HistoryPageViewHistogram.JOURNEYS;
        break;
      default:
        histogramValue = HistoryPageViewHistogram.HISTORY;
        break;
    }

    // Avoid double-recording the same page consecutively.
    if (histogramValue === this.lastRecordedSelectedPageHistogramValue_) {
      return;
    }
    this.lastRecordedSelectedPageHistogramValue_ = histogramValue;

    BrowserProxyImpl.getInstance().recordHistogram(
        'History.HistoryPageView', histogramValue,
        HistoryPageViewHistogram.END);
  }

  // Override FindShortcutMixin methods.
  override handleFindShortcut(modalContextOpen: boolean): boolean {
    if (modalContextOpen) {
      return false;
    }
    this.$.toolbar.searchField.showAndFocus();
    return true;
  }

  // Override FindShortcutMixin methods.
  override searchInputHasFocus(): boolean {
    return this.$.toolbar.searchField.isSearchFocused();
  }

  setHasDrawerForTesting(enabled: boolean) {
    this.hasDrawer_ = enabled;
  }

  protected onQueryStateChanged_(e: CustomEvent<{value: QueryState}>) {
    this.queryState_ = e.detail.value;
  }

  protected onSelectedPageChanged_(e: CustomEvent<{value: string}>) {
    this.selectedPage_ = e.detail.value;
  }

  protected onToolbarSearchTermNativeBeforeInput_(
      e: CustomEvent<{e: InputEvent}>) {
    // TODO(crbug.com/40673976): This needs to be cached on the `beforeinput`
    //   event since there is a bug where this data is not available in the
    //   native `input` event below.
    this.dataFromNativeBeforeInput_ = e.detail.e.data;
  }

  protected onToolbarSearchTermNativeInput_(
      e: CustomEvent<{e: InputEvent, inputValue: string}>) {
    const insertedText = this.dataFromNativeBeforeInput_;
    this.dataFromNativeBeforeInput_ = null;
    if (e.detail.inputValue.length === 0) {
      // Input was entirely cleared (eg. backspace/delete was hit).
      this.numCharsTypedInSearch_ = 0;
    } else if (insertedText === e.detail.inputValue) {
      // If the inserted text matches exactly with the current value of the
      // input, that implies that the previous input value was cleared or
      // was empty to begin with. So, reset the num chars typed and count this
      // input event as 1 char typed.
      this.numCharsTypedInSearch_ = 1;
    } else {
      this.numCharsTypedInSearch_++;
    }
  }

  protected onToolbarSearchTermCleared_() {
    this.numCharsTypedInSearch_ = 0;
  }

  protected onListPendingDeleteChanged_(e: CustomEvent<{value: boolean}>) {
    this.pendingDelete_ = e.detail.value;
  }

  protected onTabsSelectedChanged_(e: CustomEvent<{value: number}>) {
    this.onSelectedTabChanged_(e);
  }

  protected onSelectedTabChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTab_ = e.detail.value;
  }

  protected onHistoryClustersVisibleChanged_(e: CustomEvent<{value: boolean}>) {
    this.historyClustersVisible_ = e.detail.value;
  }

  protected onFilterChanged_(
      e: CustomEvent<{userVisits: boolean, actorVisits: boolean}>) {
    this.includeUserVisits_ = e.detail.userVisits;
    this.includeActorVisits_ = e.detail.actorVisits;

    this.fire('change-query', {
      search: this.queryState_.searchTerm,
      includeActorVisits: this.includeActorVisits_,
      includeUserVisits: this.includeUserVisits_,
    });
  }

  protected showFilterChips_(): boolean {
    return this.isBrowsingHistoryActorIntegrationM3Enabled_ &&
        this.isGlicWebActuationAvailable_ && !this.getShowResultsByGroup_();
  }

  // TODO(crub.com/509908129): Add static stylesheet in history.html
  private addThemedColors_() {
    assert(document.body.querySelector(COLORS_CSS_SELECTOR) === null);
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = 'chrome://theme/colors.css?sets=ui,chrome';
    document.body.appendChild(link);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-app': HistoryAppElement;
  }
}

customElements.define(HistoryAppElement.is, HistoryAppElement);
