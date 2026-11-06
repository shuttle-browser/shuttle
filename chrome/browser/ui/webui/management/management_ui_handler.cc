// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/enterprise/browser_management/management_identity.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#endif

#include "build/android_buildflags.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service.h"
#include "chrome/browser/media/webrtc/multi_capture/multi_capture_data_service_factory.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/managed_ui.h"  // nogncheck crbug.com/40147906
#endif


#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"


enum class ReportingType {
  kDevice,
  kExtensions,
  kSecurity,
  kUser,
  kUserActivity,
  kLegacyTech,
  kUrl,
};

namespace {

#if !BUILDFLAG(IS_CHROMEOS)

bool IsBrowserManaged() {
  return g_browser_process->browser_policy_connector()
      ->HasMachineLevelPolicies();
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
base::ListValue GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  base::ListValue permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location())) {
    return permission_messages;
  }

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()
          ->GetManagementUIPermissionIDs(
              extension->permissions_data()->active_permissions(),
              extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()->GetPermissionMessages(
          permissions);

  for (const auto& message : messages) {
    permission_messages.Append(message.message());
  }

  return permission_messages;
}

base::ListValue GetPowerfulExtensions(
    const extensions::ExtensionSet& extensions) {
  base::ListValue powerful_extensions;

  for (const auto& extension : extensions) {
    base::ListValue permission_messages = GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      base::DictValue extension_to_add =
          extensions::util::GetExtensionInfo(extension.get());
      extension_to_add.Set("permissions", std::move(permission_messages));
      powerful_extensions.Append(std::move(extension_to_add));
    }
  }

  return powerful_extensions;
}
#endif

const char* GetReportingTypeValue(ReportingType reportingType) {
  switch (reportingType) {
    case ReportingType::kDevice:
      return kReportingTypeDevice;
    case ReportingType::kExtensions:
      return kReportingTypeExtensions;
    case ReportingType::kSecurity:
      return kReportingTypeSecurity;
    case ReportingType::kUser:
      return kReportingTypeUser;
    case ReportingType::kUserActivity:
      return kReportingTypeUserActivity;
    case ReportingType::kLegacyTech:
      return kReportingTypeLegacyTech;
    case ReportingType::kUrl:
      return kReportingTypeUrl;
    default:
      return kReportingTypeSecurity;
  }
}

}  // namespace

ManagementUIHandler::ManagementUIHandler(Profile* profile) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId};
#endif
  UpdateAccountManagedState(profile);
#if !BUILDFLAG(IS_CHROMEOS)
  UpdateBrowserManagedState();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

ManagementUIHandler::~ManagementUIHandler() {
  DisallowJavascript();
}

#if !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ManagementUIHandler> ManagementUIHandler::Create(
    Profile* profile) {
  return std::make_unique<ManagementUIHandler>(profile);
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getContextualManagedData",
      base::BindRepeating(&ManagementUIHandler::HandleGetContextualManagedData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getManagedWebsites",
      base::BindRepeating(&ManagementUIHandler::HandleGetManagedWebsites,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getApplications",
      base::BindRepeating(&ManagementUIHandler::HandleGetApplications,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitBrowserReportingInfo,
                          base::Unretained(this)));
}

void ManagementUIHandler::OnJavascriptAllowed() {
  AddObservers();
}

void ManagementUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagementUIHandler::AddBrowserReportingInfo(
    base::ListValue* report_sources) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  const policy::PolicyService* policy_service = GetPolicyService();
  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  const policy::PolicyMap& on_prem_reporting_extension_stable_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_stable_policy_namespace);
  const policy::PolicyMap& on_prem_reporting_extension_beta_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_beta_policy_namespace);

  const policy::PolicyMap* policy_maps[] = {
      &on_prem_reporting_extension_stable_policy_map,
      &on_prem_reporting_extension_beta_policy_map};
#endif

  const struct {
    const char* reporting_extension_policy_key;
    const char* message;
    const ReportingType reporting_type;
  } report_definitions[] = {
      {kPolicyKeyReportMachineIdData, kManagementExtensionReportMachineName,
       ReportingType::kDevice},
      {kPolicyKeyReportMachineIdData,
       kManagementExtensionReportMachineNameAddress, ReportingType::kDevice},
      {kPolicyKeyReportVersionData, kManagementExtensionReportVersion,
       ReportingType::kDevice},
      {kPolicyKeyReportSystemTelemetryData, kManagementExtensionReportPerfCrash,
       ReportingType::kDevice},
      {kPolicyKeyReportUserIdData, kManagementExtensionReportUsername,
       ReportingType::kUser},
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      {kPolicyKeyReportExtensionsData,
       kManagementExtensionReportExtensionsPlugin, ReportingType::kExtensions},
#endif
      {kPolicyKeyReportUserBrowsingData,
       kManagementExtensionReportUserBrowsingData,
       ReportingType::kUserActivity},
      {kPolicyKeyReportVisitedUrlData, kManagementExtensionReportVisitedUrl,
       ReportingType::kUrl}};

  std::unordered_set<const char*> enabled_messages;
  for (auto& report_definition : report_definitions) {
    // Shuttle: cloud reporting is removed; only the on-prem reporting
    // extension (configured through local policy) can report.
    if (report_definition.reporting_extension_policy_key) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
      for (const policy::PolicyMap* policy_map : policy_maps) {
        const base::Value* policy_value = policy_map->GetValue(
            report_definition.reporting_extension_policy_key,
            base::Value::Type::BOOLEAN);
        if (policy_value && policy_value->GetBool()) {
          enabled_messages.insert(report_definition.message);
          break;
        }
      }
#endif
    }
  }

  // The message with more data collected for kPolicyKeyReportMachineIdData
  // trumps the one with less data.
  if (enabled_messages.find(kManagementExtensionReportMachineNameAddress) !=
      enabled_messages.end()) {
    enabled_messages.erase(kManagementExtensionReportMachineName);
  }

  for (auto& report_definition : report_definitions) {
    if (enabled_messages.find(report_definition.message) ==
        enabled_messages.end()) {
      continue;
    }

    base::DictValue data;
    data.Set("messageId", report_definition.message);
    data.Set("reportingType",
             GetReportingTypeValue(report_definition.reporting_type));
    report_sources->Append(std::move(data));
  }
}

base::DictValue ManagementUIHandler::GetContextualManagedData(
    Profile* profile) {
  base::DictValue response;
#if !BUILDFLAG(IS_CHROMEOS)
  int message_id = IDS_MANAGEMENT_NOT_MANAGED_NOTICE;
  if (browser_managed_) {
    message_id = IDS_MANAGEMENT_BROWSER_NOTICE;
  } else if (account_managed_) {
    message_id = IDS_MANAGEMENT_PROFILE_NOTICE;
  }

  response.Set("browserManagementNotice",
               l10n_util::GetStringFUTF16(
                   message_id, chrome::kManagedUiLearnMoreUrl,
                   base::EscapeForHTML(l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));

  response.Set("pageSubtitle", ::GetManagementPageSubtitle(profile));

  response.Set("extensionReportingSubtitle",
               l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  response.Set(
      "applicationReportingSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_APPLICATIONS_INSTALLED));
  response.Set(
      "managedWebsitesSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  response.Set("managed", managed());
#endif  // !BUILDFLAG(IS_CHROMEOS)

  return response;
}

base::ListValue ManagementUIHandler::GetManagedWebsitesInfo(
    Profile* profile) const {
  base::ListValue managed_websites;
#if !BUILDFLAG(IS_ANDROID)
  auto* managed_configuration =
      ManagedConfigurationAPIFactory::GetForProfile(profile);

  if (!managed_configuration) {
    return managed_websites;
  }

  for (const auto& entry : managed_configuration->GetManagedOrigins()) {
    managed_websites.Append(entry.Serialize());
  }
#endif
  return managed_websites;
}

base::ListValue ManagementUIHandler::GetApplicationsInfo(
    Profile* profile) const {
  return base::ListValue();
}

policy::PolicyService* ManagementUIHandler::GetPolicyService() {
  return Profile::FromWebUI(web_ui())
      ->GetProfilePolicyConnector()
      ->policy_service();
}

bool ManagementUIHandler::managed() const {
  return account_managed() || browser_managed_;
}

void ManagementUIHandler::RegisterPrefChange(
    PrefChangeRegistrar& pref_registrar) {
  // Shuttle: the supervised-user id pref is gone with supervised users; there
  // is no pref to observe on desktop.
}

void ManagementUIHandler::UpdateManagedState() {
#if !BUILDFLAG(IS_CHROMEOS)
  bool is_account_updated =
      UpdateAccountManagedState(Profile::FromWebUI(web_ui()));
  bool is_browser_updated = UpdateBrowserManagedState();
  if (is_account_updated || is_browser_updated) {
    FireWebUIListener("managed_data_changed");
  }
#endif
}

bool ManagementUIHandler::UpdateAccountManagedState(Profile* profile) {
  if (!profile) {
    CHECK_IS_TEST();
    return false;
  }
  bool new_managed = IsProfileManaged(profile);
  bool is_updated = (new_managed != account_managed_);
  account_managed_ = new_managed;
  return is_updated;
}

#if !BUILDFLAG(IS_CHROMEOS)
bool ManagementUIHandler::UpdateBrowserManagedState() {
  bool new_managed = IsBrowserManaged();
  bool is_updated = (new_managed != browser_managed_);
  browser_managed_ = new_managed;
  return is_updated;
}
#endif

std::string ManagementUIHandler::GetAccountManager(Profile* profile) const {
  std::optional<std::string> manager = GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = GetDeviceManagerIdentity();
  }

  return manager.value_or(std::string());
}

bool ManagementUIHandler::IsProfileManaged(Profile* profile) const {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

void ManagementUIHandler::HandleGetExtensions(const base::ListValue& args) {
  AllowJavascript();
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetPowerfulExtensions(extensions));
#else
  ResolveJavascriptCallback(args[0] /* callback_id */,
                            base::Value(base::Value::Type::LIST));
#endif
}

void ManagementUIHandler::HandleGetContextualManagedData(
    const base::ListValue& args) {
  AllowJavascript();
  auto result = GetContextualManagedData(Profile::FromWebUI(web_ui()));
  ResolveJavascriptCallback(args[0] /* callback_id */, result);
}

void ManagementUIHandler::HandleGetManagedWebsites(
    const base::ListValue& args) {
  AllowJavascript();

  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      GetManagedWebsitesInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleGetApplications(const base::ListValue& args) {
  AllowJavascript();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetApplicationsInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::ListValue& args) {
  base::ListValue report_sources;
  AllowJavascript();
  AddBrowserReportingInfo(&report_sources);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::ListValue report_sources;
  AddBrowserReportingInfo(&report_sources);
  FireWebUIListener("browser-reporting-info-updated", report_sources);
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
void ManagementUIHandler::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::OnExtensionUnloaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason /*reason*/) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}
#endif

void ManagementUIHandler::OnPolicyUpdated(
    const policy::PolicyNamespace& /*ns*/,
    const policy::PolicyMap& /*previous*/,
    const policy::PolicyMap& /*current*/) {
  UpdateManagedState();
  NotifyBrowserReportingInfoUpdated();
}

void ManagementUIHandler::AddObservers() {
  if (has_observers_) {
    return;
  }

  has_observers_ = true;

  auto* profile = Profile::FromWebUI(web_ui());

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionRegistry::Get(profile)->AddObserver(this);
  auto* policy_service = GetPolicyService();
  policy_service->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
#endif

  pref_registrar_.Init(profile->GetPrefs());

  RegisterPrefChange(pref_registrar_);
}

void ManagementUIHandler::RemoveObservers() {
  if (!has_observers_) {
    return;
  }

  has_observers_ = false;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);
  auto* policy_service = GetPolicyService();
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);
#endif

  pref_registrar_.RemoveAll();
}
