// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../../controls/collapse_radio_button.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../controls/settings_toggle_button.js';
import '../../icons.html.js';
import '../../settings_page/settings_subpage.js';
import '../../simple_confirmation_dialog.js';
import './secure_dns.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {SettingsCollapseRadioButtonElement} from '../../controls/collapse_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions, SafeBrowsingInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {Router} from '../../router.js';
import {SettingsViewMixinLit} from '../../settings_page/settings_view_mixin_lit.js';
import {ContentSettingsTypes} from '../../site_settings/constants.js';
import type {SiteSettingsBrowserProxy} from '../../site_settings/site_settings_browser_proxy.js';
import {SiteSettingsBrowserProxyImpl} from '../../site_settings/site_settings_browser_proxy.js';
import {isSettingEnabled} from '../../site_settings/site_settings_util.js';

import {SafeBrowsingSetting} from './safe_browsing_types.js';
import {getCss} from './security_page.css.js';
import {getHtml} from './security_page.html.js';

/**
 * Enumeration of all HTTPS-First Mode setting states. Must be kept in sync with
 * the enum of the same name located in:
 * chrome/browser/ssl/https_first_mode_settings_tracker.h
 */
export enum HttpsFirstModeSetting {
  DISABLED = 0,
  // DEPRECATED: A separate Incognito setting never shipped.
  // ENABLED_INCOGNITO = 1,
  ENABLED_FULL = 2,
  ENABLED_BALANCED = 3,
}

export interface SettingsSecurityPageElement {
  $: {
    safeBrowsingDisabled: SettingsCollapseRadioButtonElement,
    safeBrowsingEnhanced: SettingsCollapseRadioButtonElement,
    safeBrowsingRadioGroup: SettingsRadioGroupElement,
    safeBrowsingStandard: SettingsCollapseRadioButtonElement,
  };
}

const SettingsSecurityPageElementBase =
    SettingsViewMixinLit(WebUiListenerMixinLit(
        I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement))));

export type SecurityPageElement = SettingsSecurityPageElement;

export class SettingsSecurityPageElement extends
    SettingsSecurityPageElementBase {
  static get is() {
    return 'settings-security-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether the secure DNS setting should be displayed.
       */
      showSecureDnsSetting_: {type: Boolean},

      // <if expr="is_chromeos">
      /**
       * Whether a link to secure DNS OS setting should be displayed.
       */
      showSecureDnsSettingLink_: {type: Boolean},
      // </if>

      /**
       * Setting for HTTPS-First Mode when the toggle is off.
       */
      httpsFirstModeUncheckedValues_: {type: Array},

      javascriptOptimizerSubLabel_: {type: String},
      enableHttpsFirstModeNewSettings_: {type: Boolean},
      enableSecurityKeysSubpage_: {type: Boolean},
      enableHashPrefixRealTimeLookups_: {type: Boolean},
      hideExtendedReportingRadioButton_: {type: Boolean},
      showDisableSafebrowsingDialog_: {type: Boolean},

      safeBrowsingPref_: {type: Object},
      httpsFirstModePref_: {type: Object},
    };
  }

  protected accessor showSecureDnsSetting_: boolean =
      loadTimeData.getBoolean('showSecureDnsSetting');

  // <if expr="is_chromeos">
  protected accessor showSecureDnsSettingLink_: boolean =
      loadTimeData.getBoolean('showSecureDnsSettingLink');
  // </if>

  protected accessor enableSecurityKeysSubpage_: boolean =
      loadTimeData.getBoolean('enableSecurityKeysSubpage');
  protected accessor showDisableSafebrowsingDialog_: boolean = false;
  protected accessor enableHashPrefixRealTimeLookups_: boolean =
      loadTimeData.getBoolean('enableHashPrefixRealTimeLookups');
  protected accessor httpsFirstModeUncheckedValues_: HttpsFirstModeSetting[] =
      [HttpsFirstModeSetting.DISABLED];
  protected accessor enableHttpsFirstModeNewSettings_: boolean =
      loadTimeData.getBoolean('enableHttpsFirstModeNewSettings');
  protected accessor javascriptOptimizerSubLabel_: string = '';
  protected accessor hideExtendedReportingRadioButton_: boolean =
      loadTimeData.getBoolean('extendedReportingRemovePrefDependency') &&
      loadTimeData.getBoolean('hashPrefixRealTimeLookupsSamplePing');
  protected accessor safeBrowsingPref_:
      chrome.settingsPrivate.PrefObject<SafeBrowsingSetting>|undefined;
  protected accessor httpsFirstModePref_:
      chrome.settingsPrivate.PrefObject<HttpsFirstModeSetting>|undefined;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private siteBrowserProxy_: SiteSettingsBrowserProxy =
      SiteSettingsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPrefs({
      'generated.safe_browsing': 'safeBrowsingPref_',
      'generated.https_first_mode_enabled': 'httpsFirstModePref_',
    });
  }

  override firstUpdated(changedProperties: PropertyValues) {
    super.firstUpdated(changedProperties);

    PrefService.getInstance().whenInitialized().then(() => {
      // Expand initial pref value manually because automatic
      // expanding is disabled.
      const prefValue =
          PrefService.getInstance()
              .getPref<SafeBrowsingSetting>('generated.safe_browsing')
              .value;
      if (prefValue === SafeBrowsingSetting.ENHANCED) {
        this.$.safeBrowsingEnhanced.expanded = true;
      } else if (prefValue === SafeBrowsingSetting.STANDARD) {
        this.$.safeBrowsingStandard.expanded = true;
      }

      // The HTTPS-First Mode generated pref should never be set to
      // ENABLED_BALANCED if the feature flag is not enabled.
      if (!loadTimeData.getBoolean('enableHttpsFirstModeNewSettings')) {
        assert(
            PrefService.getInstance()
                .getPref('generated.https_first_mode_enabled')
                .value !== HttpsFirstModeSetting.ENABLED_BALANCED);
      }
    });

    this.addWebUiListener(
        'contentSettingCategoryChanged', (category: ContentSettingsTypes) => {
          if (category === ContentSettingsTypes.JAVASCRIPT_OPTIMIZER) {
            this.updateJavascriptOptimizerEnabledByDefault_();
          }
        });
    this.updateJavascriptOptimizerEnabledByDefault_();
  }

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route, oldRoute?: Route) {
    super.currentRouteChanged(route, oldRoute);

    if (route !== routes.SECURITY) {
      return;
    }
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        SafeBrowsingInteractions.SAFE_BROWSING_SHOWED);
    const queryParams = Router.getInstance().getQueryParameters();
    const section = queryParams.get('q');
    if (section === 'enhanced') {
      this.$.safeBrowsingEnhanced.expanded = false;
      this.$.safeBrowsingStandard.expanded = false;
    }
  }

  /**
   * Updates the buttons' expanded status by propagating previous click
   * events
   */
  private updateCollapsedButtons_() {
    this.$.safeBrowsingEnhanced.updateCollapsed();
    this.$.safeBrowsingStandard.updateCollapsed();
  }

  private async updateJavascriptOptimizerEnabledByDefault_() {
    const defaultValue =
        await this.siteBrowserProxy_.getDefaultValueForContentType(
            ContentSettingsTypes.JAVASCRIPT_OPTIMIZER);
    this.javascriptOptimizerSubLabel_ = this.i18n(
        isSettingEnabled(defaultValue.setting) ?
            'securityJavascriptOptimizerLinkRowLabelEnabled' :
            'securityJavascriptOptimizerLinkRowLabelDisabled');
  }

  /**
   * Possibly displays the Safe Browsing disable dialog based on the users
   * selection.
   */
  protected onSafeBrowsingRadioChange_() {
    const selected =
        Number.parseInt(this.$.safeBrowsingRadioGroup.selected || '', 10);
    const prefValue =
        PrefService.getInstance().getPref('generated.safe_browsing').value;
    if (prefValue !== selected) {
      this.recordInteractionHistogramOnRadioChange_(selected);
      this.recordActionOnRadioChange_(selected);
    }
    if (selected === SafeBrowsingSetting.DISABLED) {
      this.showDisableSafebrowsingDialog_ = true;
    } else {
      this.updateCollapsedButtons_();
      this.$.safeBrowsingRadioGroup.sendPrefChange();
    }
  }

  protected getDisabledExtendedSafeBrowsing_(): boolean {
    return this.safeBrowsingPref_?.value !== SafeBrowsingSetting.STANDARD;
  }

  protected getSafeBrowsingStandardSubLabel_(): string {
    return this.i18n(
        this.enableHashPrefixRealTimeLookups_ ?
            'safeBrowsingStandardDescProxy' :
            'safeBrowsingStandardDesc');
  }

  // Conversion helper for binding Integer pref values as String values.
  // For ControlledRadioButton elements, the name attribute must be of String
  // type in order to correctly match for the PrefControlMixin.
  protected getName_(value: number): string {
    return value.toString();
  }

  protected getHttpsFirstModeSubLabel_(): string {
    // If the backing HTTPS-Only Mode preference is enabled, but the
    // generated preference has its user control disabled, then additional
    // text explaining that the feature is locked down for Advanced Protection
    // users is added.
    const generatedPref = this.httpsFirstModePref_;
    if (!generatedPref) {
      return '';
    }
    if (this.enableHttpsFirstModeNewSettings_) {
      return this.i18n(
          generatedPref.userControlDisabled ?
              'httpsFirstModeDescriptionAdvancedProtection' :
              'httpsFirstModeSectionDescription');
    } else {
      return this.i18n(
          generatedPref.userControlDisabled ?
              'httpsOnlyModeDescriptionAdvancedProtection' :
              'httpsOnlyModeDescription');
    }
  }

  protected isHttpsFirstModeExpanded_(): boolean {
    // If the pref is not user-modifiable, we should only show the main toggle.
    // (Note: this is not the case when the setting is policy-managed -- the
    // radio group should be expanded and labeled with the enterprise
    // indicator.)
    const generatedPref = this.httpsFirstModePref_;
    if (!generatedPref || generatedPref.userControlDisabled) {
      return false;
    }
    return generatedPref.value !== HttpsFirstModeSetting.DISABLED;
  }

  protected onManageCertificatesClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('certManagementV2URL'));
  }

  protected onAdvancedProtectionProgramLinkClick_() {
    window.open(loadTimeData.getString('advancedProtectionURL'));
  }

  protected onJavascriptOptimizerSettingsClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER);
  }

  protected onSecurityKeysClick_() {
    Router.getInstance().navigateTo(routes.SECURITY_KEYS);
  }

  protected onEnhancedProtectionLearnMoreClick_(e: Event) {
    if ((e.target as HTMLElement).id === 'enhancedProtectionLearnMoreLink') {
      OpenWindowProxyImpl.getInstance().openUrl(
          loadTimeData.getString('enhancedProtectionHelpCenterURL'));
      e.preventDefault();
    }
  }

  protected onSafeBrowsingExtendedReportingChange_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.IMPROVE_SECURITY);
  }

  /**
   * Handles the closure of the disable safebrowsing dialog, reselects the
   * appropriate radio button if the user cancels the dialog, and puts focus on
   * the disable safebrowsing button.
   */
  protected onDisableSafebrowsingDialogClose_() {
    const dialog =
        this.shadowRoot.querySelector('settings-simple-confirmation-dialog');
    assert(dialog);
    const confirmed = dialog.wasConfirmed();
    this.recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed);
    this.recordActionOnSafeBrowsingDialogClose_(confirmed);
    // Check if the dialog was confirmed before closing it.
    if (confirmed) {
      this.$.safeBrowsingRadioGroup.sendPrefChange();
      this.updateCollapsedButtons_();
    } else {
      this.$.safeBrowsingRadioGroup.resetToPrefValue();
    }

    this.showDisableSafebrowsingDialog_ = false;

    // Set focus back to the no protection button regardless of user interaction
    // with the dialog, as it was the entry point to the dialog.
    focusWithoutInk(this.$.safeBrowsingDisabled);
  }

  protected onEnhancedProtectionExpandClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.ENHANCED);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.ENHANCED);
  }

  protected onStandardProtectionExpandClicked_() {
    this.recordInteractionHistogramOnExpandButtonClicked_(
        SafeBrowsingSetting.STANDARD);
    this.recordActionOnExpandButtonClicked_(SafeBrowsingSetting.STANDARD);
  }

  // <if expr="is_chromeos">
  protected onOpenChromeOsSecureDnsSettingsClick_() {
    const path =
        loadTimeData.getString('chromeOSPrivacyAndSecuritySectionPath');
    OpenWindowProxyImpl.getInstance().openUrl(`chrome://os-settings/${path}`);
  }
  // </if>

  private recordInteractionHistogramOnRadioChange_(safeBrowsingSetting:
                                                       SafeBrowsingSetting) {
    let action;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_ENHANCED_PROTECTION_CLICKED;
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_STANDARD_PROTECTION_CLICKED;
    } else {
      action =
          SafeBrowsingInteractions.SAFE_BROWSING_DISABLE_SAFE_BROWSING_CLICKED;
    }
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(action);
  }

  private recordInteractionHistogramOnExpandButtonClicked_(
      safeBrowsingSetting: SafeBrowsingSetting) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            SafeBrowsingInteractions
                .SAFE_BROWSING_ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED :
            SafeBrowsingInteractions
                .SAFE_BROWSING_STANDARD_PROTECTION_EXPAND_ARROW_CLICKED);
  }

  private recordInteractionHistogramOnSafeBrowsingDialogClose_(confirmed:
                                                                   boolean) {
    this.metricsBrowserProxy_.recordSafeBrowsingInteractionHistogram(
        confirmed ? SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED :
                    SafeBrowsingInteractions
                        .SAFE_BROWSING_DISABLE_SAFE_BROWSING_DIALOG_DENIED);
  }

  private recordActionOnRadioChange_(safeBrowsingSetting: SafeBrowsingSetting) {
    let actionName;
    if (safeBrowsingSetting === SafeBrowsingSetting.ENHANCED) {
      actionName = 'SafeBrowsing.Settings.EnhancedProtectionClicked';
    } else if (safeBrowsingSetting === SafeBrowsingSetting.STANDARD) {
      actionName = 'SafeBrowsing.Settings.StandardProtectionClicked';
    } else {
      actionName = 'SafeBrowsing.Settings.DisableSafeBrowsingClicked';
    }
    this.metricsBrowserProxy_.recordAction(actionName);
  }

  private recordActionOnExpandButtonClicked_(safeBrowsingSetting:
                                                 SafeBrowsingSetting) {
    this.metricsBrowserProxy_.recordAction(
        safeBrowsingSetting === SafeBrowsingSetting.ENHANCED ?
            'SafeBrowsing.Settings.EnhancedProtectionExpandArrowClicked' :
            'SafeBrowsing.Settings.StandardProtectionExpandArrowClicked');
  }

  private recordActionOnSafeBrowsingDialogClose_(confirmed: boolean) {
    this.metricsBrowserProxy_.recordAction(
        confirmed ? 'SafeBrowsing.Settings.DisableSafeBrowsingDialogConfirmed' :
                    'SafeBrowsing.Settings.DisableSafeBrowsingDialogDenied');
  }

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map([
      [
        routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER.path,
        '#javascriptOptimizerSettingLink',
      ],
    ]);

    if (routes.SECURITY_KEYS) {
      map.set(routes.SECURITY_KEYS.path, '#securityKeysSubpageTrigger');
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page': SettingsSecurityPageElement;
  }
}

customElements.define(
    SettingsSecurityPageElement.is, SettingsSecurityPageElement);
