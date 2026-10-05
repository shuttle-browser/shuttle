// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run.h"

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run_features.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/browser_features.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace content {
class BrowserContext;
}

using base::UserMetricsAction;

namespace {

// Indicates whether this is first run. Populated when IsChromeFirstRun
// is invoked, then used as a cache on subsequent calls.
first_run::internal::FirstRunState g_first_run =
    first_run::internal::FIRST_RUN_UNKNOWN;

// Cached first run sentinel creation time.
// Used to avoid excess file operations.
base::Time g_cached_sentinel_creation_time;

GURL UrlFromString(const std::string& in) {
  return GURL(in);
}

void ConvertStringVectorToGURLVector(
    const std::vector<std::string>& src,
    std::vector<GURL>* ret) {
  ret->resize(src.size());
  std::ranges::transform(src, ret->begin(), &UrlFromString);
}

base::FilePath& GetInitialPrefsPathForTesting() {
  static base::NoDestructor<base::FilePath> s;
  return *s;
}

// Makes chrome the user's default browser according to policy or
// |make_chrome_default_for_user| if no policy is set.
void ProcessDefaultBrowserPolicy(bool make_chrome_default_for_user) {
  // Only proceed if chrome can be made default unattended. In other cases, this
  // is handled by the first run default browser prompt (on Windows 8+).
  if (shell_integration::GetDefaultBrowserSetPermission() ==
      shell_integration::SET_DEFAULT_UNATTENDED) {
    // The policy has precedence over the user's choice.
    if (g_browser_process->local_state()->IsManagedPreference(
            prefs::kDefaultBrowserSettingEnabled)) {
      if (g_browser_process->local_state()->GetBoolean(
          prefs::kDefaultBrowserSettingEnabled)) {
        shell_integration::SetAsDefaultBrowser();
      }
    } else if (make_chrome_default_for_user) {
      shell_integration::SetAsDefaultBrowser();
    }
  }
}

// Get the file path of the first run sentinel; returns false on failure.
bool GetFirstRunSentinelFilePath(base::FilePath* path) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return false;
  *path = user_data_dir.Append(chrome::kFirstRunSentinel);
  return true;
}

// Create the first run sentinel file; returns the status of the operation.
startup_metric_utils::FirstRunSentinelCreationResult CreateSentinel() {
  base::FilePath first_run_sentinel;
  if (!GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFailedToGetPath;
  }

  if (base::PathExists(first_run_sentinel)) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFilePathExists;
  }

  if (!base::WriteFile(first_run_sentinel, "")) {
    return startup_metric_utils::FirstRunSentinelCreationResult::
        kFileSystemError;
  }

  return startup_metric_utils::FirstRunSentinelCreationResult::kSuccess;
}

// Reads the creation time of the first run sentinel file. If the first run
// sentinel file does not exist, it will return base::Time().
base::Time ReadFirstRunSentinelCreationTime() {
  base::Time first_run_sentinel_creation_time = base::Time();
  base::FilePath first_run_sentinel;
  if (GetFirstRunSentinelFilePath(&first_run_sentinel)) {
    base::File::Info info;
    if (base::GetFileInfo(first_run_sentinel, &info))
      first_run_sentinel_creation_time = info.creation_time;
  }
  return first_run_sentinel_creation_time;
}

// Returns true if the sentinel file exists (or the path cannot be obtained).
bool IsFirstRunSentinelPresent() {
  base::FilePath sentinel;
  return !GetFirstRunSentinelFilePath(&sentinel) || base::PathExists(sentinel);
}

}  // namespace

namespace first_run {
namespace internal {

void SetupInitialPrefsFromInstallPrefs(
    const installer::InitialPreferences& install_prefs,
    MasterPrefs* out_prefs) {
  ConvertStringVectorToGURLVector(
      install_prefs.GetFirstRunTabs(), &out_prefs->new_tabs);

  bool value = false;
  if (install_prefs.GetBool(
          installer::initial_preferences::kMakeChromeDefaultForUser, &value) &&
      value) {
    out_prefs->make_chrome_default_for_user = true;
  }

  install_prefs.GetString(
      installer::initial_preferences::kDistroImportBookmarksFromFilePref,
      &out_prefs->import_bookmarks_path);

  install_prefs.GetString(
      installer::initial_preferences::kDistroSuppressDefaultBrowserPromptPref,
      &out_prefs->suppress_default_browser_prompt_for_version);

  if (base::FeatureList::IsEnabled(features::kBookmarksImportOnFirstRun)) {
    const base::DictValue* bookmarks_dict = install_prefs.GetBookmarksBlock();
    if (bookmarks_dict) {
      out_prefs->import_bookmarks_dict = bookmarks_dict->Clone();
    }
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (base::FeatureList::IsEnabled(features::kInitialExternalExtensions)) {
    out_prefs->initial_extensions_provider_name =
        install_prefs.GetInitialExtensionsProviderName();

    if (const base::ListValue* initial_extensions =
            install_prefs.GetInitialExtensionsList()) {
      out_prefs->initial_extensions = initial_extensions->Clone();
    }
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_MAC)
  if (install_prefs.GetBool(prefs::kConfirmToQuitEnabled, &value) && value)
    out_prefs->confirm_to_quit = true;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
  install_prefs.GetBool(installer::initial_preferences::kRequireEula,
                        &out_prefs->eula_required);
#endif  // BUILDFLAG(IS_LINUX)
}

// -- Platform-specific functions --

#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_BSD)
bool IsOrganicFirstRun() {
  std::string brand;
  google_brand::GetBrand(&brand);
  return google_brand::IsOrganicFirstRun(brand);
}
#endif

FirstRunState DetermineFirstRunState(bool has_sentinel,
                                     bool force_first_run,
                                     bool no_first_run) {
  return (force_first_run || (!has_sentinel && !no_first_run))
             ? FIRST_RUN_TRUE
             : FIRST_RUN_FALSE;
}

}  // namespace internal

MasterPrefs::MasterPrefs() = default;

MasterPrefs::~MasterPrefs() = default;

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The data importer is removed; no first run import prefs are registered.
}

bool IsChromeFirstRun() {
  if (g_first_run == internal::FIRST_RUN_UNKNOWN) {
    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    g_first_run = internal::DetermineFirstRunState(
        IsFirstRunSentinelPresent(),
        command_line->HasSwitch(switches::kForceFirstRun),
        command_line->HasSwitch(switches::kNoFirstRun));
  }
  return g_first_run == internal::FIRST_RUN_TRUE;
}

#if BUILDFLAG(IS_MAC)
bool IsFirstRunSuppressed(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kNoFirstRun);
}
#endif

void CreateSentinelIfNeeded() {
  if (IsChromeFirstRun()) {
    auto sentinel_creation_result = CreateSentinel();
    startup_metric_utils::GetBrowser().RecordFirstRunSentinelCreation(
        sentinel_creation_result);
  }

  // Causes the first run sentinel creation time to be read and cached, while
  // I/O is still allowed.
  std::ignore = GetFirstRunSentinelCreationTime();
}

base::Time GetFirstRunSentinelCreationTime() {
  if (g_cached_sentinel_creation_time.is_null())
    g_cached_sentinel_creation_time = ReadFirstRunSentinelCreationTime();
  return g_cached_sentinel_creation_time;
}

void ResetCachedSentinelDataForTesting() {
  g_cached_sentinel_creation_time = base::Time();
  g_first_run = first_run::internal::FIRST_RUN_UNKNOWN;
}

void SetInitialPrefsPathForTesting(const base::FilePath& initial_prefs) {
  GetInitialPrefsPathForTesting() = initial_prefs;
}

std::unique_ptr<installer::InitialPreferences> LoadInitialPrefs() {
  base::FilePath initial_prefs_path;
  if (!GetInitialPrefsPathForTesting().empty()) {
    initial_prefs_path = GetInitialPrefsPathForTesting();
  } else {
    initial_prefs_path =
        base::FilePath(first_run::internal::InitialPrefsPath());
  }

  if (initial_prefs_path.empty())
    return nullptr;
  auto initial_prefs =
      std::make_unique<installer::InitialPreferences>(initial_prefs_path);
  if (!initial_prefs->read_from_file())
    return nullptr;
  return initial_prefs;
}

ProcessInitialPreferencesResult ProcessInitialPreferences(
    const base::FilePath& user_data_dir,
    std::unique_ptr<installer::InitialPreferences> initial_prefs,
    MasterPrefs* out_prefs) {
  DCHECK(!user_data_dir.empty());

  if (initial_prefs.get()) {
    // Don't show EULA when running in headless mode since this would
    // effectively block the UI because there is no one to accept it.
    // On Linux, the EULA dialog is shown in ShowEulaDialog after
    // UI is initialized.
    if (!headless::IsHeadlessMode() &&
        !internal::ShowPostInstallEULAIfNeeded(initial_prefs.get())) {
      return EULA_EXIT_NOW;
    }

    base::DictValue initial_dictionary =
        initial_prefs->initial_dictionary().Clone();
    // The distribution dictionary (and any prefs below it) are never registered
    // for use in Chrome's PrefService. Strip them from the initial dictionary
    // before mapping it to prefs.
    initial_dictionary.Remove(installer::initial_preferences::kDistroDict);

    initial_dictionary.Remove(installer::initial_preferences::kBookmarksBlock);

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Extensions are not copied verbatim into prefs. Their installation
    // is managed by the `InitialExternalExtensionsLoader` which will load
    // extension ids from the local prefs.
    initial_dictionary.RemoveByDottedPath(
        installer::initial_preferences::kExtensionsBlock);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

    if (!chrome_prefs::InitializePrefsFromMasterPrefs(
            profiles::GetDefaultProfileDir(user_data_dir),
            std::move(initial_dictionary),
            g_browser_process->os_crypt_async())) {
      DLOG(ERROR) << "Failed to initialize from initial preferences.";
    }

    internal::SetupInitialPrefsFromInstallPrefs(*initial_prefs, out_prefs);
  }

  return FIRST_RUN_PROCEED;
}

#if BUILDFLAG(IS_LINUX)
bool ShowEulaDialog() {
  return internal::ShowEulaDialog();
}
#endif

void DoPostImportTasks(bool make_chrome_default_for_user) {
  // Only set default browser after import as auto import relies on the current
  // default browser to know what to import from.
  ProcessDefaultBrowserPolicy(make_chrome_default_for_user);

  internal::DoPostImportPlatformSpecificTasks();
}

}  // namespace first_run
