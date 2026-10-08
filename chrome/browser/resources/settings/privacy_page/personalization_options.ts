// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '../controls/settings_toggle_button.js';
// <if expr="not is_chromeos">
import '../relaunch_confirmation_dialog.js';
// </if>

// <if expr="_google_chrome">
// <if expr="is_chromeos">
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
// </if>
// <if expr="not is_chromeos">
import '//resources/cr_elements/policy/cr_policy_indicator.js';
// </if>
// </if>

// <if expr="not is_chromeos">
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import type {CrToggleElement} from '//resources/cr_elements/cr_toggle/cr_toggle.js';
// </if>
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {MetricsReporting, PrivacyPageBrowserProxy} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
import {PrivacyPageBrowserProxyImpl} from '/shared/settings/privacy_page/privacy_page_browser_proxy.js';
// <if expr="is_chromeos">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

// </if>

import type {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {pageVisibility} from '../page_visibility.js';
import type {PrivacyPageVisibility} from '../page_visibility.js';
import {RelaunchMixinLit, RestartType} from '../relaunch_mixin_lit.js';

import {getCss} from './personalization_options.css.js';
import {getHtml} from './personalization_options.html.js';

export interface SettingsPersonalizationOptionsElement {
  $: {
    urlCollectionToggle: SettingsToggleButtonElement,
    // <if expr="not is_chromeos">
    toast: CrToastElement,
    // </if>
  };
}

const SettingsPersonalizationOptionsElementBase =
    RelaunchMixinLit(WebUiListenerMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement))));


export type PersonalizationOptionsElement =
    SettingsPersonalizationOptionsElement;

export class SettingsPersonalizationOptionsElement extends
    SettingsPersonalizationOptionsElementBase {
  static get is() {
    return 'settings-personalization-options';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      // <if expr="_google_chrome and not is_chromeos">
      metricsReporting_: {type: Object},

      showRestart_: {type: Boolean},
      // </if>

      showSearchAggregatorSuggest_: {type: Boolean},

      searchAggregatorSuggestFakePref_: {type: Object},

      shouldUseMetricsConsentRestructure_: {type: Boolean},

      spellCheckDictionariesPref_: {type: Object},
    };
  }

  // <if expr="_google_chrome and not is_chromeos">
  protected accessor metricsReporting_: MetricsReporting|undefined;
  protected accessor showRestart_: boolean = false;
  // </if>

  protected accessor showSearchAggregatorSuggest_: boolean =
      loadTimeData.getBoolean('showSearchAggregatorSuggest');
  protected accessor searchAggregatorSuggestFakePref_:
      chrome.settingsPrivate.PrefObject<boolean> = {
    key: 'enterprise_search_aggregator_settings.fake_pref',
    type: chrome.settingsPrivate.PrefType.BOOLEAN,
    value: true,
    enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
  };

  protected accessor shouldUseMetricsConsentRestructure_: boolean =
      loadTimeData.getBoolean('shouldUseMetricsConsentRestructure');

  protected accessor spellCheckDictionariesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('spellcheck.dictionaries', 'spellCheckDictionariesPref_');
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    // <if expr="_google_chrome and not is_chromeos">
    const setMetricsReporting = (metricsReporting: MetricsReporting) =>
        this.setMetricsReporting_(metricsReporting);
    this.addWebUiListener('metrics-reporting-change', setMetricsReporting);
    this.browserProxy_.getMetricsReporting().then(setMetricsReporting);
    // </if>
  }

  // <if expr="is_chromeos">
  /**
   * @return the autocomplete search suggestions CrToggleElement.
   */
  getSearchSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#searchSuggestToggle');
  }

  /**
   * @return the anonymized URL collection CrToggleElement.
   */
  getUrlCollectionToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot.querySelector<SettingsToggleButtonElement>(
        '#urlCollectionToggle');
  }
  // </if>

  // <if expr="_google_chrome and not is_chromeos">
  protected onMetricsReportingChange_(e: Event) {
    const target = e.target as CrToggleElement;
    this.browserProxy_.setMetricsReportingEnabled(target.checked);
  }

  private setMetricsReporting_(metricsReporting: MetricsReporting) {
    const hadPrevious = this.metricsReporting_ !== undefined;
    this.metricsReporting_ = metricsReporting;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPrevious) {
      this.showRestart_ = true;
    }
  }
  // </if>

  protected showSearchSuggestToggle_(): boolean {
    if (pageVisibility?.privacy === undefined) {
      // pageVisibility isn't defined in non-Guest profiles
      // (crbug.com/40211731).
      return true;
    }
    return (pageVisibility.privacy as PrivacyPageVisibility).searchPrediction;
  }

  // <if expr="is_chromeos">
  protected onMetricsReportingLinkClick_() {
    // TODO(wesokuhara) Deep link directly to metrics toggle via settingId.
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSettingsPrivacyHubSubpageUrl'));
  }
  // </if>

  // <if expr="_google_chrome">
  protected onUseSpellingServiceSettingsBooleanControlChange_(event: Event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if ((event.target as SettingsToggleButtonElement).checked) {
      PrefService.getInstance().setPrefValue(
          'browser.enable_spellchecking', true);
    }
  }

  // <if expr="not is_chromeos">
  protected showSpellCheckControlToggle_(): boolean {
    return !!this.spellCheckDictionariesPref_ &&
        this.spellCheckDictionariesPref_.value.length > 0;
  }
  // </if><!-- not chromeos -->

  // <if expr="is_chromeos">
  protected showSpellCheckControlLink_(): boolean {
    return !!this.spellCheckDictionariesPref_ &&
        this.spellCheckDictionariesPref_.value.length > 0;
  }

  protected onUseSpellingServiceLinkClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSyncSetupSettingsUrl'));
  }
  // </if><!-- chromeos -->
  // </if><!-- _google_chrome -->

  // <if expr="not is_chromeos">
  protected onRestartClick_(e: Event) {
    e.stopPropagation();
    this.performRestart(RestartType.RESTART);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-options': SettingsPersonalizationOptionsElement;
  }
}

customElements.define(
    SettingsPersonalizationOptionsElement.is,
    SettingsPersonalizationOptionsElement);
