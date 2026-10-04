// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/preinstalled_extensions.h"

#include <stddef.h>

#include <array>
#include <memory>
#include <set>
#include <string>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/external_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

using extensions::mojom::ManifestLocation;

bool IsLocaleSupported() {
  // Don't bother installing pre-installed apps in locales where it is known
  // that they don't work.
  // TODO(rogerta): Do this check dynamically once the webstore can expose
  // an API. See http://crbug.com/40102876
  std::string locale =
      extensions::ExtensionsBrowserClient::Get()->GetApplicationLocale();
  static constexpr const char* unsupported_locales[] = {"CN", "TR", "IR"};
  for (const char* unsupported : unsupported_locales) {
    if (base::EndsWith(locale, unsupported,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return false;
    }
  }
  return true;
}

std::set<Profile*>& GetPerformNewInstallationProfiles() {
  static base::NoDestructor<std::set<Profile*>> perform_new_installation;
  return *perform_new_installation;
}

// A null loader that immediately calls LoadFinished().
class NullLoader : public extensions::ExternalLoader {
 public:
  NullLoader() = default;
  const NullLoader& operator=(const NullLoader&) = delete;
  NullLoader& operator=(NullLoader&&) = delete;

  // ExternalLoader:
  void StartLoading() override {
    // LoadFinished will call SetPrefs() and add the extensions.
    LoadFinished(base::DictValue());
  }

 private:
  // Private destructor because ExternalLoader is ref counted.
  ~NullLoader() override = default;
};

}  // namespace

namespace preinstalled_extensions {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kPreinstalledExtensionsInstallState,
                                kUnknown);
}

// static
bool Provider::DidPerformNewInstallationForProfile(Profile* profile) {
  return GetPerformNewInstallationProfiles().count(profile);
}

void Provider::InitProfileState() {
  // We decide to install or not install pre-installed extensions based on the
  // following criteria, from highest priority to lowest priority:
  //
  // - If the locale is not compatible with the pre-installed extensions, don't
  //   install them.
  // - The kPreinstalledExtensions preferences value in the profile.  This
  //   value is usually set in the master_preferences file.
  // - If they have already been installed, don't reinstall them.

  preinstalled_extensions_enabled_ =
      IsLocaleSupported() && profile_->GetPrefs()->GetString(
                                 prefs::kPreinstalledExtensions) == "install";
  DCHECK(!perform_new_installation_);

  InstallState state =
      static_cast<InstallState>(profile_->GetPrefs()->GetInteger(
          prefs::kPreinstalledExtensionsInstallState));

  std::optional<InstallState> new_install_state;

  switch (state) {
    case kUnknown: {
      // Pre-installed apps are only installed on profile creation or a new
      // chrome download.
      bool is_new_profile = profile_->WasCreatedByVersionOrLater(
          std::string(version_info::GetVersionNumber()));
      if (is_new_profile && preinstalled_extensions_enabled_) {
        new_install_state = kAlreadyInstalledPreinstalledExtensions;
        perform_new_installation_ = true;
      } else {
        new_install_state = kNeverInstallPreinstalledExtensions;
      }
      break;
    }

    // The old pre-installed apps were provided as external extensions and were
    // installed everytime Chrome was run. Thus, changing the list of default
    // apps affected all users. Migrate old pre-installed apps to new mechanism
    // where they are installed only once as INTERNAL.
    // TODO(grv) : remove after Q1-2013.
    case kProvideLegacyPreinstalledApps:
      is_migration_ = true;
      new_install_state = kAlreadyInstalledPreinstalledExtensions;
      break;

    case kAlreadyInstalledPreinstalledExtensions:
    case kNeverInstallPreinstalledExtensions:
      break;

    default:
      NOTREACHED();
  }

  if (new_install_state) {
    profile_->GetPrefs()->SetInteger(prefs::kPreinstalledExtensionsInstallState,
                                     *new_install_state);
  }
  if (perform_new_installation_) {
    GetPerformNewInstallationProfiles().insert(profile_);
  }
}

Provider::Provider(Profile* profile,
                   VisitorInterface* service,
                   extensions::mojom::ManifestLocation crx_location,
                   extensions::mojom::ManifestLocation download_location,
                   int creation_flags)
    : extensions::ExternalProviderImpl(service,
                                       base::MakeRefCounted<NullLoader>(),
                                       profile,
                                       crx_location,
                                       download_location,
                                       creation_flags),
      profile_(profile) {
  DCHECK(profile);
  // See SetPrefs() below.
  CHECK_EQ(download_location, ManifestLocation::kInternal);
  set_auto_acknowledge(true);

  InitProfileState();
}

void Provider::VisitRegisteredExtension() {
  if (!preinstalled_extensions_enabled_) {
    // If pre-installed apps aren't enabled for the profile, we short-circuit
    // the flow to add them and immediately set empty prefs.
    ExternalProviderImpl::SetPrefs(base::DictValue());
    return;
  }

  extensions::ExternalProviderImpl::VisitRegisteredExtension();
}

void Provider::SetPrefs(base::DictValue prefs) {
  DCHECK(preinstalled_extensions_enabled_);

  // Load a hard-coded list of external extensions. These are not component
  // extensions; they are installed from the webstore and don't get access to
  // component only APIs. Must be used with ManifestLocation::kInternal to allow
  // CWS to download certain extensions.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (perform_new_installation_) {
    AddExtension(extension_misc::kDocsOfflineExtensionId, prefs);
  }
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  // Without a new installation, the only apps installed here used to be those
  // re-added after a rolled-back web app migration. Web apps are removed, so
  // there is nothing to install.
  if (!perform_new_installation_) {
    prefs.clear();
  }

  ExternalProviderImpl::SetPrefs(std::move(prefs));
}

void Provider::AddExtension(const std::string& extension_id,
                            base::DictValue& prefs) {
  prefs.SetByDottedPath(extension_id + ".external_update_url",
                        extension_urls::GetWebstoreUpdateUrl().spec());
}

}  // namespace preinstalled_extensions
