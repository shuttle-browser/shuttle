// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Subpages
import './appearance_page/appearance_fonts_page.js';
import './clear_browsing_data_dialog/clear_browsing_data_dialog.js';
import './clear_browsing_data_dialog/clear_browsing_data_time_picker.js';
import './privacy_page/cookies_page.js';
import './privacy_page/security/security_keys_subpage.js';
import './privacy_page/security/security_page_v2.js';
import './privacy_page/security/security_page.js';
import './search_page/search_engines_page.js';
import './simple_confirmation_dialog.js';
import './site_settings/ads_page.js';
import './site_settings/all_sites.js';
import './site_settings/anti_abuse_page.js';
import './site_settings/ar_page.js';
import './site_settings/automatic_downloads_page.js';
import './site_settings/automatic_full_screen_page.js';
import './site_settings/auto_picture_in_picture_page.js';
import './site_settings/background_sync_page.js';
import './site_settings/bluetooth_devices_page.js';
import './site_settings/bluetooth_scanning_page.js';
import './site_settings/camera_page.js';
import './site_settings/captured_surface_control_page.js';
import './site_settings/category_setting_exceptions.js';
import './site_settings/chooser_exception_list.js';
import './site_settings/clipboard_page.js';
import './site_settings/federated_identity_api_page.js';
import './site_settings/filesystem_page.js';
import './site_settings/file_system_site_details.js';
import './site_settings/geolocation_page.js';
import './site_settings/hand_tracking_page.js';
import './site_settings/hid_devices_page.js';
import './site_settings/idle_detection_page.js';
import './site_settings/images_page.js';
import './site_settings/insecure_content_page.js';
import './site_settings/javascript_page.js';
import './site_settings/keyboard_lock_page.js';
import './site_settings/local_fonts_page.js';
import './site_settings/local_network_page.js';
import './site_settings/loopback_network_page.js';
import './site_settings/microphone_page.js';
import './site_settings/midi_devices_page.js';
import './site_settings/notifications_page.js';
import './site_settings/payment_handler_page.js';
import './site_settings/pdf_documents_page.js';
import './site_settings/popups_page.js';
import './site_settings/protected_content_page.js';
import './site_settings/protocol_handlers.js';
import './site_settings/sensors_page.js';
import './site_settings/serial_ports_page.js';
import './site_settings/settings_category_default_radio_group.js';
import './site_settings/site_data.js';
import './site_settings/site_details.js';
import './site_settings/site_details_permission_device_entry.js';
import './site_settings/site_settings_page.js';
// <if expr="is_chromeos">
import './site_settings/smart_card_readers_page.js';
import './site_settings/web_printing_page.js';
// </if>
import './site_settings/sound_page.js';
import './site_settings/storage_access_page.js';
import './site_settings/usb_devices_page.js';
import './site_settings/v8_page.js';
import './site_settings/vr_page.js';
import './site_settings/web_applications_page.js';
import './site_settings/window_management_page.js';
import './site_settings/zoom_levels.js';
// <if expr="not is_chromeos">
import './a11y_page/live_caption.js';
// </if>
// Sections
import './a11y_page/a11y_page_index.js';
import './downloads_page/downloads_page.js';
// <if expr="is_chromeos">
import './languages_page/languages_page_index_cros.js';
// </if>
// <if expr="not is_chromeos">
import './languages_page/languages_page_index.js';
// </if>
import './reset_page/reset_page.js';
// <if expr="not is_chromeos">
import './system_page/system_page.js';

// </if>

// <if expr="not is_chromeos">
export {AxAnnotationsBrowserProxyImpl, ScreenAiInstallStatus} from '/shared/settings/a11y_page/ax_annotations_browser_proxy.js';
export type {AxAnnotationsBrowserProxy} from '/shared/settings/a11y_page/ax_annotations_browser_proxy.js';
export {CaptionsBrowserProxyImpl} from '/shared/settings/a11y_page/captions_browser_proxy.js';
export type {CaptionsBrowserProxy, LiveCaptionLanguageList} from '/shared/settings/a11y_page/captions_browser_proxy.js';
// </if>

export {FontsBrowserProxyImpl} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
export type {FontsBrowserProxy, FontsData} from '/shared/settings/appearance_page/fonts_browser_proxy.js';
export {CrShortcutInputElement} from 'chrome://resources/cr_components/cr_shortcut_input/cr_shortcut_input.js';
export {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
export {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
export {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
export {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
export {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
export {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
export {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.js';
export {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
export {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
export {CrTextareaElement} from 'chrome://resources/cr_elements/cr_textarea/cr_textarea.js';
export {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
export {getToastManager} from 'chrome://resources/cr_elements/cr_toast/cr_toast_manager.js';
export {CrTooltipElement} from 'chrome://resources/cr_elements/cr_tooltip/cr_tooltip.js';
export type {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
export {AccessibilityBrowserProxyImpl} from './a11y_page/a11y_browser_proxy.js';
export type {AccessibilityBrowserProxy} from './a11y_page/a11y_browser_proxy.js';
export {SettingsA11yPageElement, ToastAlertLevel} from './a11y_page/a11y_page.js';
// <if expr="is_win or is_linux or is_macosx">
export {SettingsAxAnnotationsSectionElement} from './a11y_page/ax_annotations_section.js';
// </if>
// <if expr="not is_chromeos">
export {SettingsLiveCaptionElement} from './a11y_page/live_caption.js';
export {SettingsLiveTranslateElement} from './a11y_page/live_translate.js';
// </if>

export {SettingsAppearanceFontsPageElement} from './appearance_page/appearance_fonts_page.js';
export {BrowsingDataType, ClearBrowsingDataBrowserProxyImpl, TimePeriod} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
export type {ClearBrowsingDataBrowserProxy, ClearBrowsingDataResult, UpdateSyncStateEvent} from './clear_browsing_data_dialog/clear_browsing_data_browser_proxy.js';
export {getDataTypePrefName, SettingsClearBrowsingDataDialogElement} from './clear_browsing_data_dialog/clear_browsing_data_dialog.js';
export {getTimePeriodString, SettingsClearBrowsingDataTimePickerElement} from './clear_browsing_data_dialog/clear_browsing_data_time_picker.js';
export {SettingsHistoryDeletionDialogElement} from './clear_browsing_data_dialog/history_deletion_dialog.js';
export {SettingsCollapseRadioButtonElement} from './controls/collapse_radio_button.js';
export {ControlledButtonElement} from './controls/controlled_button.js';
export {SettingsCheckboxElement} from './controls/settings_checkbox.js';
export {SettingsRadioGroupElement} from './controls/settings_radio_group.js';
export {SettingsSliderElement} from './controls/settings_slider.js';
export {SettingsToggleButtonElement} from './controls/settings_toggle_button.js';
export {DownloadsBrowserProxyImpl} from './downloads_page/downloads_browser_proxy.js';
export type {DownloadsBrowserProxy} from './downloads_page/downloads_browser_proxy.js';
export {SettingsDownloadsPageElement} from './downloads_page/downloads_page.js';
// <if expr="not is_chromeos">
export {SettingsAddLanguagesDialogElement} from './languages_page/add_languages_dialog.js';
// <if expr="not is_macosx">
export {SettingsEditDictionaryPageElement} from './languages_page/edit_dictionary_page.js';
// </if>

export {getLanguageHelperInstance, LanguageHelperImpl} from './languages_page/languages.js';
export {LanguagesBrowserProxyImpl} from './languages_page/languages_browser_proxy.js';
export type {LanguagesBrowserProxy} from './languages_page/languages_browser_proxy.js';
export {kMenuCloseDelay, SettingsLanguagesPageElement} from './languages_page/languages_page.js';
export {LanguageSettingsActionType, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from './languages_page/languages_settings_metrics_proxy.js';
export type {LanguageSettingsMetricsProxy} from './languages_page/languages_settings_metrics_proxy.js';
export type {LanguageHelper, LanguagesModel} from './languages_page/languages_types.js';
export {SettingsSpellCheckPageElement} from './languages_page/spell_check_page.js';
// </if>

export {NetworkPredictionOptions} from './performance_page/constants.js';
export {SettingsCookiesPageElement} from './privacy_page/cookies_page.js';
export {SettingsDoNotTrackToggleElement} from './privacy_page/do_not_track_toggle.js';
export {SettingsPersonalizationOptionsElement} from './privacy_page/personalization_options.js';
export {FINGERPRINT_CHECK_DARK_URL, FINGERPRINT_CHECK_LIGHT_URL, FINGERPRINT_SCANNED_ICON_DARK, FINGERPRINT_SCANNED_ICON_LIGHT, FingerprintProgressArcElement, PROGRESS_CIRCLE_BACKGROUND_COLOR_DARK, PROGRESS_CIRCLE_BACKGROUND_COLOR_LIGHT, PROGRESS_CIRCLE_FILL_COLOR_DARK, PROGRESS_CIRCLE_FILL_COLOR_LIGHT} from './privacy_page/security/fingerprint_progress_arc.js';
export {SafeBrowsingSetting} from './privacy_page/security/safe_browsing_types.js';
export {SecureDnsResolverType, SettingsSecureDnsElement} from './privacy_page/security/secure_dns.js';
export {SecureDnsInputElement} from './privacy_page/security/secure_dns_input.js';
export {SecureDnsV2ResolverType, SettingsSecureDnsV2Element} from './privacy_page/security/secure_dns_v2.js';
export {BioEnrollDialogPage, SettingsSecurityKeysBioEnrollDialogElement} from './privacy_page/security/security_keys_bio_enroll_dialog.js';
export {Ctap2Status, SampleStatus, SecurityKeysBioEnrollProxyImpl, SecurityKeysCredentialBrowserProxyImpl, SecurityKeysPinBrowserProxyImpl, SecurityKeysResetBrowserProxyImpl} from './privacy_page/security/security_keys_browser_proxy.js';
export type {Credential, CredentialManagementResponse, Enrollment, EnrollmentResponse, SecurityKeysBioEnrollProxy, SecurityKeysCredentialBrowserProxy, SecurityKeysPinBrowserProxy, SecurityKeysResetBrowserProxy, SensorInfo, SetPinResponse, StartCredentialManagementResponse} from './privacy_page/security/security_keys_browser_proxy.js';
export {CredentialManagementDialogPage, SettingsSecurityKeysCredentialManagementDialogElement} from './privacy_page/security/security_keys_credential_management_dialog.js';
export {ResetDialogPage, SettingsSecurityKeysResetDialogElement} from './privacy_page/security/security_keys_reset_dialog.js';
export {SetPinDialogPage, SettingsSecurityKeysSetPinDialogElement} from './privacy_page/security/security_keys_set_pin_dialog.js';
export {SecurityKeysSubpageElement} from './privacy_page/security/security_keys_subpage.js';
export {HttpsFirstModeSetting, SettingsSecurityPageElement} from './privacy_page/security/security_page.js';
export {SecurityPageFeatureRowElement} from './privacy_page/security/security_page_feature_row.js';
export {SecuritySettingsBundleSetting, SettingsSecurityPageV2Element} from './privacy_page/security/security_page_v2.js';
export {SettingsResetPageElement} from './reset_page/reset_page.js';
export {SettingsResetProfileDialogElement} from './reset_page/reset_profile_dialog.js';
export {SettingsOmniboxExtensionEntryElement} from './search_page/omnibox_extension_entry.js';
export {SettingsSearchEngineEditDialogElement} from './search_page/search_engine_edit_dialog.js';
export {SettingsSearchEngineEntryElement} from './search_page/search_engine_entry.js';
export {SettingsSearchEngineIconElement} from './search_page/search_engine_icon.js';
export {SettingsSearchEnginesListElement} from './search_page/search_engines_list.js';
export {SettingsSearchEnginesPageElement} from './search_page/search_engines_page.js';
export {SettingsSimpleConfirmationDialogElement} from './simple_confirmation_dialog.js';
export {AddSiteDialogElement} from './site_settings/add_site_dialog.js';
export {AllSitesElement} from './site_settings/all_sites.js';
export {SettingsAntiAbusePageElement} from './site_settings/anti_abuse_page.js';
export {CategorySettingExceptionsElement} from './site_settings/category_setting_exceptions.js';
export {ChooserExceptionListElement} from './site_settings/chooser_exception_list.js';
export {ChooserExceptionListEntryElement} from './site_settings/chooser_exception_list_entry.js';
export {ChooserType, ContentSetting, ContentSettingsTypes, CookieControlsMode, CookiesExceptionType, JavascriptOptimizerSetting, SettingsState, SITE_EXCEPTION_WILDCARD, SiteSettingSource, SortMethod} from './site_settings/constants.js';
export {SettingsEditExceptionDialogElement} from './site_settings/edit_exception_dialog.js';
export {FileSystemSiteDetailsElement} from './site_settings/file_system_site_details.js';
export {FileSystemSiteEntryElement} from './site_settings/file_system_site_entry.js';
export {FileSystemSiteEntryItemElement} from './site_settings/file_system_site_entry_item.js';
export {FileSystemSiteListElement} from './site_settings/file_system_site_list.js';
export {GeolocationPageElement} from './site_settings/geolocation_page.js';
export {NotificationsPageElement} from './site_settings/notifications_page.js';
export {PdfDocumentsPageElement} from './site_settings/pdf_documents_page.js';
export {ProtectedContentPageElement} from './site_settings/protected_content_page.js';
export {ProtocolHandlersElement} from './site_settings/protocol_handlers.js';
export type {HandlerEntry, ProtocolEntry} from './site_settings/protocol_handlers.js';
export {SettingsRecentSitePermissionsElement} from './site_settings/recent_site_permissions.js';
export {SettingsCategoryDefaultRadioGroupElement} from './site_settings/settings_category_default_radio_group.js';
export {SettingsSiteDataElement} from './site_settings/site_data.js';
export {SiteDetailsElement} from './site_settings/site_details.js';
export {SiteDetailsPermissionElement} from './site_settings/site_details_permission.js';
export {SiteDetailsPermissionDeviceEntryElement} from './site_settings/site_details_permission_device_entry.js';
export {SiteEntryElement} from './site_settings/site_entry.js';
export {SiteListElement} from './site_settings/site_list.js';
export {SiteListEntryElement} from './site_settings/site_list_entry.js';
export {DefaultSettingSource, SiteSettingsBrowserProxyImpl, ThirdPartyCookieBlockingSetting} from './site_settings/site_settings_browser_proxy.js';
export type {ChooserException, DefaultContentSetting, FileSystemGrant, OriginFileSystemGrants, OriginInfo, RawChooserException, RawSiteException, RecentSitePermissions, SiteException, SiteGroup, SiteSettingsBrowserProxy, StorageAccessEmbeddingException, StorageAccessSiteException, SubAppsPermissionExplanationInfo, ZoomLevelEntry} from './site_settings/site_settings_browser_proxy.js';
export {defaultSettingLabel} from './site_settings/site_settings_list.js';
export {SettingsSiteSettingsPageElement} from './site_settings/site_settings_page.js';
// <if expr="is_chromeos">
export {SettingsSmartCardReadersPageElement} from './site_settings/smart_card_readers_page.js';
// </if>
export {SoundPageElement} from './site_settings/sound_page.js';
export {StorageAccessSiteListElement} from './site_settings/storage_access_site_list.js';
export {StorageAccessSiteListEntryElement} from './site_settings/storage_access_site_list_entry.js';
export {StorageAccessStaticSiteListEntryElement} from './site_settings/storage_access_static_site_list_entry.js';
export type {StorageAccessStaticSiteListEntry} from './site_settings/storage_access_static_site_list_entry.js';
export {V8PageElement} from './site_settings/v8_page.js';
export {WebPrintingPageElement} from './site_settings/web_printing_page.js';
export {WebsiteUsageBrowserProxyImpl} from './site_settings/website_usage_browser_proxy.js';
export type {WebsiteUsageBrowserProxy} from './site_settings/website_usage_browser_proxy.js';
export {ZoomLevelsElement} from './site_settings/zoom_levels.js';
// <if expr="not is_chromeos">
export {SettingsSystemPageElement} from './system_page/system_page.js';
export {SystemPageBrowserProxyImpl} from './system_page/system_page_browser_proxy.js';
export type {SystemPageBrowserProxy} from './system_page/system_page_browser_proxy.js';

// </if>
