// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../settings_page/settings_section.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {MetricsBrowserProxy} from '../metrics_browser_proxy.js';
import {AiPageInteractions, MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import {getTemplate} from './ai_page.html.js';

const SettingsAiPageElementBase = SettingsViewMixin(PrefsMixin(PolymerElement));

export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showPasswordChangeControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showPasswordChangeControl'),
      },
    };
  }

  declare private showPasswordChangeControl_: boolean;

  private shouldRecordMetrics_: boolean = true;
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.maybeLogVisibilityMetrics_();
  }

  private maybeLogVisibilityMetrics_() {
    if (!this.shouldRecordMetrics_ ||
        Router.getInstance().getCurrentRoute() !== routes.AI) {
      return;
    }
    this.shouldRecordMetrics_ = false;

    this.metricsBrowserProxy_.recordBooleanHistogram(
        'Settings.AiPage.ElementVisibility.PasswordChange',
        this.showPasswordChangeControl_);
  }

  private onPasswordChangeRowClick_() {
    this.metricsBrowserProxy_.recordAiPageInteractions(
        AiPageInteractions.PASSWORD_CHANGE_CLICK);
    this.metricsBrowserProxy_.recordAction(
        'Settings.AiPage.PasswordChangeEntryPointClick');

    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('passwordChangeSettingsUrl'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ai-page': SettingsAiPageElement;
  }
}

customElements.define(SettingsAiPageElement.is, SettingsAiPageElement);
