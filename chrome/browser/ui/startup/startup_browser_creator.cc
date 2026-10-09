// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/profile_launch_observer.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#endif

#if BUILDFLAG(IS_MAC)
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/headless/headless_mode_util.h"
#include "components/headless/policy/headless_mode_policy.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/startup/focus/focus_handler.h"
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

// Dumps the current set of the browser process's histograms to |output_file|.
// The file is overwritten if it exists. This function should only be called in
// the blocking pool.
void DumpBrowserHistograms(const base::FilePath& output_file) {
  std::string output_string(
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::WriteFile(output_file, output_string);
}

// Returns whether |profile_info.profile| can be opened during Chrome startup
// without explicit user action.
bool CanOpenProfileOnStartup(StartupProfileInfo profile_info) {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, the user has already chosen and logged into the profile before
  // Chrome starts up.
  return true;
#else
  Profile* profile = profile_info.profile;

  // Profiles that require signin are not available.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return false;
  }

  return true;
#endif
}

bool IsSilentLaunchEnabled(const base::CommandLine& command_line,
                           const Profile* profile) {
  // This check should have been done in `ProcessCmdLineImpl()` before calling
  // this function. But `LaunchBrowser()` can be called directly, for example by
  // //chrome/browser/ash/login/session/user_session_manager.cc, so it has to be
  // checked again.
  // TODO(crbug.com/40819749): Investigate minimizing duplicate checks.

  if (command_line.HasSwitch(switches::kNoStartupWindow)) {
    return true;
  }

  if (command_line.HasSwitch(switches::kSilentLaunch)) {
    return true;
  }

  if (StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line)) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  return profile->GetPrefs()->GetBoolean(
      prefs::kStartupBrowserWindowLaunchSuppressed);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns the app id of the kiosk app associated with the current user session.
// Returns nullopt for non-kiosk user sessions and for ARCVM kiosk sessions,
// since crash recovery is not supported there.
std::optional<ash::KioskAppId> GetAppId(const base::CommandLine& command_line,
                                        Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);

  if (!user) {
    return std::nullopt;
  }

  switch (user->GetType()) {
    case user_manager::UserType::kKioskChromeApp:
      return ash::KioskAppId::ForChromeApp(
          command_line.GetSwitchValueASCII(::switches::kAppId),
          user->GetAccountId());
    case user_manager::UserType::kKioskWebApp:
      return ash::KioskAppId::ForWebApp(user->GetAccountId());
    case user_manager::UserType::kKioskIWA:
      return ash::KioskAppId::ForIsolatedWebApp(user->GetAccountId());
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskArcvmApp:
      return std::nullopt;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
// Attempts to handle the --focus command line switch to focus an existing
// browser window or tab. Returns true if the focus request was handled
// (successfully focused, parsing failed, or no fallback URL available),
// std::nullopt if the focus request was not applicable or not successful
// and processing should continue.
std::optional<bool> MaybeHandleFocusRequest(
    const base::CommandLine& command_line,
    chrome::startup::IsProcessStartup process_startup,
    const StartupProfileInfo& profile_info) {
  // Only handle focus requests when:
  // - Not during process startup (existing process)
  // - --focus switch is present
  // - Profile mode is BrowserWindow
  // - Profile is available
  if (process_startup != chrome::startup::IsProcessStartup::kNo ||
      !command_line.HasSwitch(switches::kFocus) ||
      profile_info.mode != StartupProfileMode::kBrowserWindow ||
      !profile_info.profile) {
    return std::nullopt;
  }

  focus::FocusResult focus_result = focus::ProcessFocusRequestWithResultFile(
      command_line, *profile_info.profile);

  // Early return for successful focus, parse errors, or when there's no
  // fallback URL. When focus succeeds or parsing fails, we're done. When no
  // match is found but there are no command line args to fall back to
  // (e.g., no URL to open), we also return to avoid opening an empty browser.
  if (focus_result.status == focus::FocusStatus::kFocused ||
      focus_result.status == focus::FocusStatus::kParseError ||
      (focus_result.status == focus::FocusStatus::kNoMatch &&
       command_line.GetArgs().empty())) {
    return true;
  }

  // Focus request didn't match anything, but there are command line args
  // to fall back to, so continue normal processing.
  return std::nullopt;
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Attempts to handle the --validate-crx command line switch. Returns false if
// the switch was present (regardless of whether Chrome is already running or
// the validation succeeded/failed), or std::nullopt if the switch wasn't
// present.
std::optional<bool> MaybeHandleValidateCrx(
    const base::CommandLine& command_line,
    chrome::startup::IsProcessStartup process_startup) {
  if (!command_line.HasSwitch(switches::kValidateCrx)) {
    return std::nullopt;
  }

  if (process_startup == chrome::startup::IsProcessStartup::kNo) {
    LOG(ERROR) << "chrome is already running; you must close all running "
               << "instances before running with the --"
               << switches::kValidateCrx << " flag";
    return false;
  }

  extensions::StartupHelper helper;
  std::string message;
  std::string error;
  if (helper.ValidateCrx(command_line, &error)) {
    message = std::string("ValidateCrx Success");
  } else {
    message = std::string("ValidateCrx Failure: ") + error;
  }
  printf("%s\n", message.c_str());
  return false;
}

}  // namespace

StartupBrowserCreator::StartupBrowserCreator() = default;

StartupBrowserCreator::~StartupBrowserCreator() {
  // When StartupBrowserCreator finishes doing its job, we should reset
  // was_restarted_read_ so that this browser no longer reads as restarted.
  // In the case where the browser is still running due to PWA or
  // background-extension subsequent startups should not execute restarted
  // behaviors.
  was_restarted_read_ = false;
  was_restore_last_session_read_ = true;
  restore_last_session_active_ = false;
}

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::was_restore_last_session_read_ = false;

// static
bool StartupBrowserCreator::restore_last_session_active_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTabs(const std::vector<GURL>& urls) {
  for (const auto& url : urls) {
    if (url.is_valid()) {
      first_run_tabs_.push_back(url);
    }
  }
}

bool StartupBrowserCreator::Start(const base::CommandLine& cmd_line,
                                  const base::FilePath& cur_dir,
                                  StartupProfileInfo profile_info,
                                  const Profiles& last_opened_profiles) {
  TRACE_EVENT0("startup", "StartupBrowserCreator::Start");
  return ProcessCmdLineImpl(cmd_line, cur_dir,
                            chrome::startup::IsProcessStartup::kYes,
                            profile_info, last_opened_profiles);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

void StartupBrowserCreator::LaunchBrowser(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool restore_tabbed_browser) {
  TRACE_EVENT0("ui", "StartupBrowserCreator::LaunchBrowser");
  SCOPED_UMA_HISTOGRAM_TIMER("Startup.StartupBrowserCreator.LaunchBrowser");

  DCHECK(profile);
#if BUILDFLAG(IS_WIN)
  DCHECK(!command_line.HasSwitch(credential_provider::kGcpwSigninSwitch));
  DCHECK(!command_line.HasSwitch(switches::kNotificationLaunchId));
#endif  // BUILDFLAG(IS_WIN)
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IsProcessStartup::kYes;

  if (!IsSilentLaunchEnabled(command_line, profile)) {
    StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
    lwp.Launch(profile,
               in_synchronous_profile_launch_
                   ? chrome::startup::IsProcessStartup::kYes
                   : chrome::startup::IsProcessStartup::kNo,
               restore_tabbed_browser);
  }
  in_synchronous_profile_launch_ = false;
  ProfileLaunchObserver::AddLaunched(profile);
}

void StartupBrowserCreator::LaunchBrowserForLastProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    StartupProfileInfo profile_info,
    const Profiles& last_opened_profiles,
    bool restore_tabbed_browser) {
  TRACE_EVENT0("ui", "StartupBrowserCreator::LaunchBrowserForLastProfiles");
  DCHECK_NE(profile_info.mode, StartupProfileMode::kError);

  Profile* profile = profile_info.profile;
  // On Windows, when chrome is launched by notification activation where the
  // kNotificationLaunchId switch is used, always use `profile` which contains
  // the profile id extracted from the notification launch id.
  bool was_windows_notification_launch = false;
#if BUILDFLAG(IS_WIN)
  was_windows_notification_launch =
      command_line.HasSwitch(switches::kNotificationLaunchId);
#endif  // BUILDFLAG(IS_WIN)

  // `last_opened_profiles` will be empty in the following circumstances:
  // - This is the first launch. `profile` is the initial profile.
  // - The user exited the browser by closing all windows for all profiles.
  //   `profile` is the profile which owned the last open window.
  // - Only incognito windows were open when the browser exited.
  //   `profile` is the last used incognito profile. Restoring it will create a
  //   browser window for the corresponding original profile.
  // - All of the last opened profiles fail to initialize.
  if (last_opened_profiles.empty() || was_windows_notification_launch) {
    if (CanOpenProfileOnStartup(profile_info)) {
      Profile* profile_to_open = profile;
#if BUILDFLAG(IS_CHROMEOS)
      if (process_startup == chrome::startup::IsProcessStartup::kYes) {
        if (ash::floating_workspace_util::IsFloatingWorkspaceV2Enabled() ||
            ash::floating_workspace_util::IsFloatingSsoEnabled(
                profile_to_open)) {
          // Calling `GetForProfile` here ensures that
          // `FloatingWorkspaceService` is created.
          // TODO(crbug.com/419801387): we can likely remove this call and
          // instead override `ServiceIsCreatedWithBrowserContext` in the
          // factory to conditionally construct the service after profile
          // creation.
          ash::FloatingWorkspaceServiceFactory::GetForProfile(profile_to_open);
        }
        // If floating workspace is responsible for restore, stop here before
        // entering the FullRestoreService code path.
        if (ash::floating_workspace_util::ShouldHandleRestartRestore()) {
          return;
        }
        // If FullRestoreService is available for the profile (i.e. the full
        // restore feature is enabled and the profile is a regular user
        // profile), defer the browser launching to FullRestoreService code.
        auto* full_restore_service =
            ash::full_restore::FullRestoreServiceFactory::GetForProfile(
                profile_to_open);
        if (full_restore_service) {
          full_restore_service->LaunchBrowserWhenReady();
          return;
        }
      }
#endif
      LaunchBrowser(command_line, profile_to_open, cur_dir, process_startup,
                    is_first_run, restore_tabbed_browser);
      return;
    }

    // Shuttle: no profile picker; fall back to opening the profile.
    LaunchBrowser(command_line, profile, cur_dir, process_startup,
                  is_first_run, restore_tabbed_browser);
    return;
  }
  ProcessLastOpenedProfiles(command_line, cur_dir, process_startup,
                            is_first_run, profile, last_opened_profiles);
}

// static
bool StartupBrowserCreator::WasRestarted() {
  // Stores the value of the preference kWasRestarted had when it was read.
  static bool was_restarted = false;

  if (!was_restarted_read_) {
    PrefService* pref_service = g_browser_process->local_state();
    was_restarted = pref_service->GetBoolean(prefs::kWasRestarted);
    pref_service->SetBoolean(prefs::kWasRestarted, false);
    was_restarted_read_ = true;
  }
  return was_restarted;
}

// static
bool StartupBrowserCreator::ShouldRestoreLastSession(
    const base::CommandLine& command_line) {
#if BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS, session restoration is handled by Ash FullRestore, which
  // dynamically sets switches::kRestoreLastSession when launching a user's
  // browser session post-login.
  return command_line.HasSwitch(switches::kRestoreLastSession);
#else
  if (!was_restore_last_session_read_) {
    restore_last_session_active_ =
        command_line.HasSwitch(switches::kRestoreLastSession);
    was_restore_last_session_read_ = true;
  }
  return restore_last_session_active_;
#endif
}

// static
SessionStartupPref StartupBrowserCreator::GetSessionStartupPref(
    const base::CommandLine& command_line,
    const Profile* profile) {
  DCHECK(profile);
  const PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);

  // IsChromeFirstRun() looks for a sentinel file to determine whether the user
  // is starting Chrome for the first time. On Chrome OS, the sentinel is stored
  // in a location shared by all users and the check is meaningless. Query the
  // UserManager instead to determine whether the user is new.
#if BUILDFLAG(IS_CHROMEOS)
  const bool is_first_run =
      user_manager::UserManager::Get()->IsCurrentUserNew();
  // On ChromeOS restarts force the user to login again. The expectation is that
  // after a login the user gets clean state. For this reason we ignore
  // StartupBrowserCreator::WasRestarted(). However
  // StartupBrowserCreator::WasRestarted has to be called in order to correctly
  // update pref values.
  const bool did_restart = false;
  StartupBrowserCreator::WasRestarted();
#else
  const bool is_first_run = first_run::IsChromeFirstRun();
  const bool did_restart = StartupBrowserCreator::WasRestarted();
#endif

  // The pref has an OS-dependent default value. For the first run only, this
  // default is overridden with SessionStartupPref::DEFAULT so that first run
  // behavior (sync promo, welcome page) is consistently invoked.
  // This applies only if the pref is still at its default and has not been
  // set by the user, managed prefs or policy.
  if (is_first_run && SessionStartupPref::TypeIsDefault(prefs)) {
    pref.type = SessionStartupPref::DEFAULT;
  }

  // The switches::kRestoreLastSession command line switch is used to restore
  // sessions after a browser self restart (e.g. after a Chrome upgrade).
  // However, new profiles can be created from a browser process that has this
  // switch so do not set the session pref to SessionStartupPref::LAST for
  // those as there is nothing to restore.
  bool restore_last_session = ShouldRestoreLastSession(command_line);
  if ((restore_last_session || did_restart) && !profile->IsNewProfile()) {
    pref.type = SessionStartupPref::LAST;
  }

  // A browser starting for a profile being unlocked should always restore.
  {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());

    if (entry && entry->IsSigninRequired()) {
      pref.type = SessionStartupPref::LAST;
    }
  }
  if ((pref.ShouldRestoreLastSession() && !pref.ShouldOpenUrls()) &&
      profile->IsOffTheRecord()) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }

  return pref;
}

// static
void StartupBrowserCreator::ClearLaunchedProfilesForTesting() {
  ProfileLaunchObserver::ClearForTesting();  // IN-TEST
  was_restarted_read_ = false;
  was_restore_last_session_read_ = false;
  restore_last_session_active_ = false;
}

// static
void StartupBrowserCreator::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPromotionsEnabled, true);
#if !BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kCommandLineFlagSecurityWarningsEnabled,
                                true);
#endif
  registry->RegisterBooleanPref(prefs::kSuppressUnsupportedOSWarning, false);
  registry->RegisterBooleanPref(prefs::kWasRestarted, false);

#if BUILDFLAG(IS_WIN)
  registry->RegisterStringPref(prefs::kShortcutMigrationVersion, std::string());
#endif
}

// static
void StartupBrowserCreator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This will be set for newly created profiles, and is used to indicate which
  // users went through onboarding with the current experiment group.
  registry->RegisterStringPref(prefs::kNaviOnboardGroup, "");
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
bool StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
    const base::CommandLine& command_line) {
  // Don't open any browser windows if the command line indicates "background
  // mode".
  if (command_line.HasSwitch(switches::kNoStartupWindow)) {
    return true;
  }

  return false;
}

bool StartupBrowserCreator::ProcessCmdLineImpl(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    StartupProfileInfo profile_info,
    const Profiles& last_opened_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(profile_info.mode, StartupProfileMode::kError);
  TRACE_EVENT0("startup", "StartupBrowserCreator::ProcessCmdLineImpl");
  ComputeAndRecordLaunchMode(command_line);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (headless::IsHeadlessMode() &&
      headless::HeadlessModePolicy::IsHeadlessModeDisabled(
          g_browser_process->local_state())) {
    LOG(ERROR) << "Headless mode is disallowed by the system admin.";
    return false;
  }
#endif

  if (process_startup == chrome::startup::IsProcessStartup::kYes &&
      command_line.HasSwitch(switches::kDisablePromptOnRepost)) {
    content::NavigationController::DisablePromptOnRepost();
  }

  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;

  bool silent_launch = false;
  Profile* privacy_safe_profile = profile_info.profile;

#if !BUILDFLAG(IS_ANDROID)
  // Try to focus an existing window/tab if --focus is present.
  if (std::optional<bool> focus_result = MaybeHandleFocusRequest(
          command_line, process_startup, profile_info)) {
    return *focus_result;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Try to validate a CRX if --validate-crx is present.
  if (std::optional<bool> crx_result =
          MaybeHandleValidateCrx(command_line, process_startup)) {
    return *crx_result;
  }

#if BUILDFLAG(IS_CHROMEOS)

  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(ash::switches::kLoginManager)) {
    silent_launch = true;
  }

  if (IsRunningInForcedAppMode()) {
    // If we are here, it means the Chrome browser crashed/restarted while in
    // Kiosk mode, since the 'force app mode' switch is only added to the
    // commandline while in a kiosk session.
    Profile* profile = profile_info.profile;

    // Skip browser launch since app mode launches its app window.
    silent_launch = true;

    if (auto app_id = GetAppId(command_line, profile); app_id.has_value()) {
      ash::KioskController::Get().StartSessionAfterCrash(app_id.value(),
                                                         profile);
    } else {
      // If we are here, the user is invalid.
      // We should terminate the session in such cases.
      chrome::AttemptUserExit();
      return false;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (process_startup == chrome::startup::IsProcessStartup::kNo &&
      command_line.HasSwitch(switches::kDumpBrowserHistograms)) {
    // Only handle --dump-browser-histograms from a rendezvous. In this case, do
    // not open a new browser window even if no output file was given.
    base::FilePath output_file(
        command_line.GetSwitchValuePath(switches::kDumpBrowserHistograms));
    if (!output_file.empty()) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&DumpBrowserHistograms, output_file));
    }
    silent_launch = true;
  }

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kOnConnectNative) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectHost) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectExtension)) {
    if (profile_info.mode != StartupProfileMode::kBrowserWindow) {
      // TODO(http://crbug.com/40819749): Refactor command line processing logic
      // to validate the flag sets and reliably determine the startup mode.
      NOTREACHED()
          << "Failed to launch a native message host: couldn't pick a profile";
    } else {
      extensions::LaunchNativeMessageHostFromNativeApp(
          command_line.GetSwitchValueASCII(
              switches::kNativeMessagingConnectExtension),
          command_line.GetSwitchValueASCII(
              switches::kNativeMessagingConnectHost),
          command_line.GetSwitchValueASCII(switches::kNativeMessagingConnectId),
          privacy_safe_profile);

      // Chrome's lifetime, if the specified extension and native messaging host
      // are both valid and a connection is established, is prolonged by
      // BackgroundModeManager. If `process_startup` is true,
      // --no-startup-window must be set or a browser window must be created for
      // BackgroundModeManager to start background mode. Without this, nothing
      // will take the first keep-alive and the browser process will not
      // terminate. To avoid this situation, don't set `silent_launch` in
      // response to the native messaging connect switches; require the client
      // to pass --no-startup-window if suppressing the creation of a window is
      // desired.
    }
  }

#endif  //  !BUILDFLAG(IS_CHROMEOS)

  // If --no-startup-window is specified then do not open a new window.
  if (command_line.HasSwitch(switches::kNoStartupWindow)) {
    silent_launch = true;
  }

  // If we don't want to launch a new browser window or tab we are done here.
  if (silent_launch) {
    bool should_block_browser_startup_metrics =
        process_startup == chrome::startup::IsProcessStartup::kYes;
#if BUILDFLAG(IS_CHROMEOS)
    // Login screen is an expected common case. Startup metrics should still be
    // recorded after the user logs in even though this is a `silent_launch`.
    should_block_browser_startup_metrics &=
        !command_line.HasSwitch(ash::switches::kLoginManager);
#endif  // BUILDFLAG(IS_CHROMEOS)
    if (should_block_browser_startup_metrics) {
      startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();
    }
    return true;
  }

#if BUILDFLAG(IS_WIN)
  if (command_line.HasSwitch(switches::kWinJumplistAction)) {
    // `switches::kWinJumplistAction` is expected to be set together with a
    // URL to open and with a specific profile dir.
    if (profile_info.mode == StartupProfileMode::kBrowserWindow) {
      // Use a non-NULL pointer to indicate JumpList has been used. We re-use
      // chrome::kJumpListIconDirname as the key to the data.
      privacy_safe_profile->SetUserData(
          chrome::kJumpListIconDirname,
          base::WrapUnique(new base::SupportsUserData::Data()));
    } else {
      // TODO(http://crbug.com/40819749): Refactor command line processing logic
      // to validate the flag sets and reliably determine the startup mode.
      DUMP_WILL_BE_NOTREACHED()
          << "Failed start for jumplist action: couldn't pick a profile";
    }
  }

  // If the command line has the kNotificationLaunchId switch, then this
  // call is from notification_helper.exe to process toast activation.
  // Delegate to the notification system; do not open a browser window here.
  if (command_line.HasSwitch(switches::kNotificationLaunchId)) {
    if (NotificationPlatformBridgeWin::HandleActivation(command_line)) {
      return true;
    }
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)

  LaunchBrowserForLastProfiles(command_line, cur_dir, process_startup,
                               is_first_run, profile_info, last_opened_profiles,
                               /*restore_tabbed_browser=*/true);
  return true;
}

void StartupBrowserCreator::ProcessLastOpenedProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  base::CommandLine command_line_without_urls(command_line.GetProgram());
  for (auto& switch_pair : command_line.GetSwitches()) {
    command_line_without_urls.AppendSwitchNative(switch_pair.first,
                                                 switch_pair.second);
  }

  // Launch the profiles in the order they became active.
  for (Profile* profile : last_opened_profiles) {
#if !BUILDFLAG(IS_CHROMEOS)
    // Skip any locked profile.
    if (!CanOpenProfileOnStartup(
            {profile, StartupProfileMode::kBrowserWindow})) {
      continue;
    }
#endif

    // Don't launch additional profiles which would only open a new tab
    // page. When restarting after an update, all profiles will reopen last
    // open pages.
    SessionStartupPref startup_pref =
        GetSessionStartupPref(command_line, profile);
    if (profile != last_used_profile &&
        startup_pref.type == SessionStartupPref::DEFAULT &&
        !HasPendingUncleanExit(profile)) {
      continue;
    }
    // Only record a launch mode histogram for |last_used_profile|. Pass a
    // null launch_mode_recorder for other profiles.
    LaunchBrowser((profile == last_used_profile) ? command_line
                                                 : command_line_without_urls,
                  profile, cur_dir, process_startup, is_first_run,
                  /*restore_tabbed_browser=*/true);
    // We've launched at least one browser.
    process_startup = chrome::startup::IsProcessStartup::kNo;
  }

// Set the |last_used_profile| to activate if a browser is launched for at
// least one profile. Otherwise, show UserManager.
// Note that this must be done after all profiles have
// been launched so the observer knows about all profiles to wait before
// activation this one.
  ProfileLaunchObserver::set_profile_to_activate(last_used_profile);
}

// static
void StartupBrowserCreator::ProcessCommandLineWithProfile(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    StartupProfileMode mode,
    Profile* profile) {
  DCHECK_NE(mode, StartupProfileMode::kError);
  if (mode == StartupProfileMode::kBrowserWindow && !profile) {
    LOG(ERROR) << "Failed to load the profile.";
    return;
  }

  // Trigger immediate policy refresh when Chrome is already running.
  // Useful for testing or forcing policy updates without waiting for
  // the next scheduled refresh.
  if (command_line.HasSwitch(switches::kRefreshPlatformPolicy)) {
    g_browser_process->browser_policy_connector()->RefreshPlatformPolicies();
    // Return early to prevent opening a new browser window.
    return;
  }

  Profiles last_opened_profiles;
#if !BUILDFLAG(IS_CHROMEOS)
  // On ChromeOS multiple profiles doesn't apply.
  // If no browser windows are open, i.e. the browser is being kept alive in
  // background mode or for other processing, restore |last_opened_profiles|.
  if (GlobalBrowserCollection::GetInstance()->GetSize() == 0) {
    last_opened_profiles =
        g_browser_process->profile_manager()->GetLastOpenedProfiles();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(
      command_line, cur_dir, chrome::startup::IsProcessStartup::kNo,
      {profile, mode}, last_opened_profiles);
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const StartupProfilePathInfo& profile_path_info) {
  if (profile_path_info.mode == StartupProfileMode::kError) {
    return;
  }

  Profile* profile = nullptr;
  if (profile_path_info.mode == StartupProfileMode::kBrowserWindow) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    profile = profile_manager->GetProfileByPath(profile_path_info.path);
    // The profile isn't loaded yet and so needs to be loaded asynchronously.
    if (!profile) {
      profile_manager->CreateProfileAsync(
          profile_path_info.path,
          base::BindOnce(&ProcessCommandLineWithProfile, command_line, cur_dir,
                         profile_path_info.mode));
      return;
    }
  }

  ProcessCommandLineWithProfile(command_line, cur_dir, profile_path_info.mode,
                                profile);
}

// static
void StartupBrowserCreator::OpenStartupPages(
    BrowserWindowInterface* browser,
    chrome::startup::IsProcessStartup process_startup) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;
  StartupBrowserCreatorImpl startup_browser_creator_impl(
      base::FilePath(), command_line, is_first_run);
  SessionStartupPref session_startup_pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line,
                                                   browser->GetProfile());
  startup_browser_creator_impl.OpenURLsInBrowser(browser, process_startup,
                                                 session_startup_pref.urls);
}

// static
bool StartupBrowserCreator::ActivatedProfile() {
  return ProfileLaunchObserver::activated_profile();
}

bool HasPendingUncleanExit(Profile* profile) {
  return ExitTypeService::GetLastSessionExitType(profile) ==
             ExitType::kCrashed &&
         !ProfileLaunchObserver::HasBeenLaunchedAndBrowserOpen(profile) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kHideCrashRestoreBubble);
}

// Shuttle: the profile picker is gone. The three-argument form stays only
// because base::ScopedAllowBlocking befriends that exact signature (changing
// base/threading/thread_restrictions.h would rebuild the whole tree); the flag
// is ignored.
StartupProfilePathInfo GetStartupProfilePath(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line) {
  return GetStartupProfilePath(cur_dir, command_line,
                               /*ignore_profile_picker=*/true);
}

StartupProfilePathInfo GetStartupProfilePath(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    bool /*ignore_profile_picker*/) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath& user_data_dir = profile_manager->user_data_dir();
// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// all others.
#if BUILDFLAG(IS_WIN)
  base::FilePath profile_basename =
      NotificationLaunchId::GetNotificationLaunchProfileBaseName(command_line);
  if (!profile_basename.empty()) {
    return {.path = user_data_dir.Append(profile_basename),
            .mode = StartupProfileMode::kBrowserWindow};
  }
#endif  // BUILDFLAG(IS_WIN)

  base::FilePath command_line_profile_directory =
      command_line.GetSwitchValuePath(switches::kProfileDirectory);

  if (!command_line_profile_directory.empty() &&
      command_line.HasSwitch(switches::kIgnoreProfileDirectoryIfNotExists)) {
    base::FilePath profile_dir_path =
        user_data_dir.Append(command_line_profile_directory);
    // This is a blocking call to the filesystem, but unfortunately it is
    // required for startup to continue, as the
    // `kIgnoreProfileDirectoryIfNotExists` switch needs to check the file
    // system state to know if the profile directory exists.
    base::ScopedAllowBlocking allow_blocking;
    if (IsProfileDirectoryMarkedForDeletion(profile_dir_path) ||
        !base::DirectoryExists(profile_dir_path)) {
      command_line_profile_directory = base::FilePath();
    }
  }

  if (!command_line_profile_directory.empty()) {
    return {.path = user_data_dir.Append(command_line_profile_directory),
            .mode = StartupProfileMode::kBrowserWindow};
  }

  if (command_line.HasSwitch(switches::kProfileEmail)) {
    // Use GetSwitchValueNative() rather than GetSwitchValueASCII() to support
    // non-ASCII email addresses.
    base::CommandLine::StringType email_native =
        command_line.GetSwitchValueNative(switches::kProfileEmail);
    if (!email_native.empty()) {
      std::string email;
#if BUILDFLAG(IS_WIN)
      email = base::WideToUTF8(email_native);
#else
      email = std::move(email_native);
#endif
      base::FilePath profile_dir =
          g_browser_process->profile_manager()->GetProfileDirForEmail(email);
      if (!profile_dir.empty()) {
        return {.path = profile_dir,
                .mode = StartupProfileMode::kBrowserWindow};
      }
    }
  }

  return {.path = profile_manager->GetLastUsedProfileDir(),
          .mode = StartupProfileMode::kBrowserWindow};
}

#if !BUILDFLAG(IS_CHROMEOS)
StartupProfileInfo GetStartupProfile(const base::FilePath& cur_dir,
                                     const base::CommandLine& command_line) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  StartupProfilePathInfo path_info =
      GetStartupProfilePath(cur_dir, command_line);
  DCHECK_NE(path_info.mode, StartupProfileMode::kError);
  StartupProfileMode mode = path_info.mode;

  switch (mode) {
    case StartupProfileMode::kError:
      // No more info to add.
      return {nullptr, StartupProfileMode::kError};
    case StartupProfileMode::kBrowserWindow:
      // Try to acquire a profile below.
      break;
  }

  // NOTE: GetProfile() does synchronous file I/O on the main thread.
  Profile* profile = profile_manager->GetProfile(path_info.path);

  // There are several cases where we should show the profile picker:
  // - if there is no entry in profile attributes storage, which means that the
  //   profile is deleted,
  // - if the profile is locked,
  // - if the profile has failed to load
  // When neither of these is true, we can safely start up with `profile`.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(path_info.path);
  if (entry && !entry->IsSigninRequired() && profile) {
    return {profile, StartupProfileMode::kBrowserWindow};
  }

  return {nullptr, StartupProfileMode::kError};
}

StartupProfileInfo GetFallbackStartupProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The only known reason for profiles to fail initialization is being unable
  // to create the profile directory, and this has already happened in
  // GetStartupProfilePath() before calling this function. In this case,
  // creation of new profiles is expected to fail. So only existing profiles are
  // attempted for fallback.

  // If the last used profile could not be initialized, see if any of other last
  // opened profiles can be initialized successfully.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  for (Profile* profile : ProfileManager::GetLastOpenedProfiles()) {
    // Return any profile that is not locked.
    ProfileAttributesEntry* entry =
        storage->GetProfileAttributesWithPath(profile->GetPath());
    if (!entry || !entry->IsSigninRequired()) {
      return {profile, StartupProfileMode::kBrowserWindow};
    }
  }

  // Couldn't initialize any last opened profiles.
  // Couldn't show the profile picker either. Try to open any profile that is
  // not locked.
  for (ProfileAttributesEntry* entry : storage->GetAllProfilesAttributes()) {
    if (!entry->IsSigninRequired()) {
      Profile* profile = profile_manager->GetProfile(entry->GetPath());
      if (profile) {
        return {profile, StartupProfileMode::kBrowserWindow};
      }
    }
  }

  return {nullptr, StartupProfileMode::kError};
}
#endif  // !BUILDFLAG(IS_CHROMEOS)
