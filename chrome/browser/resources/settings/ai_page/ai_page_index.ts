// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ai-page-index' is the settings page containing settings for
 * passwords, payment methods and addresses.
 */
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import './ai_info_card.js';
import './ai_page.js';

import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {getTemplate} from './ai_page_index.html.js';


export interface SettingsAiPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsAiPageIndexElementBase =
    SearchableViewContainerMixin(RouteObserverMixin(PolymerElement));

export class SettingsAiPageIndexElement extends SettingsAiPageIndexElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-ai-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      showAiPageAiFeatureSection_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showAiPageAiFeatureSection'),
      },
    };
  }

  declare prefs: Record<string, unknown>;
  declare private showAiPageAiFeatureSection_: boolean;

  private showDefaultViews_() {
    const defaultViews: string[] = ['aiInfoCard'];

    if (this.showAiPageAiFeatureSection_) {
      defaultViews.push('parent');
    }

    this.$.viewManager.switchViews(
        defaultViews, 'no-animation', 'no-animation');
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(() => {
      switch (newRoute) {
        case routes.AI:
        case routes.BASIC:
          // Switch back to the default view in case they are part of search
          // results.
          this.showDefaultViews_();
          break;
        default:
          // Nothing to do. Other parent elements are responsible for updating
          // the displayed contents.
          break;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page-index': SettingsAiPageIndexElement;
  }
}

customElements.define(
    SettingsAiPageIndexElement.is, SettingsAiPageIndexElement);
