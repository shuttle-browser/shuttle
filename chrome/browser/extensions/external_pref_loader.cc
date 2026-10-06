// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_pref_loader.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_iterators.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace {

constexpr base::FilePath::CharType kExternalExtensionJson[] =
    FILE_PATH_LITERAL("external_extensions.json");

// Extension installations are skipped here as excluding these in the overlay
// is a bit complicated.
// TODO(crbug.com/40658053) This is a temporary measure and should be replaced.
bool SkipInstallForChromeOSTablet(const base::FilePath& file_path) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!ash::switches::IsTabletFormFactor())
    return false;

  constexpr char const* kIdsNotToBeInstalledOnTabletFormFactor[] = {
      "blpcfgokakmgnkcojhhkbfbldkacnbeo.json",  // Youtube file name.
      "ejjicmeblgpmajnghnpcppodonldlgfn.json",  // Calendar file name.
      "hcglmfcclpfgljeaiahehebeoaiicbko.json",  // Google Photos file name.
      "lneaknkopdijkpnocmklfnjbeapigfbh.json",  // Google Maps file name.
      "pjkljhegncpnkpknbcohdijeoejaedia.json",  // Gmail file name.
  };

  return std::ranges::contains(kIdsNotToBeInstalledOnTabletFormFactor,
                               file_path.BaseName().value());
#else
  return false;
#endif
}

std::set<base::FilePath> GetPrefsCandidateFilesFromFolder(
    const base::FilePath& external_extension_search_path) {
  std::set<base::FilePath> external_extension_paths;

  if (!base::PathExists(external_extension_search_path)) {
    // Does not have to exist.
    return external_extension_paths;
  }

  base::FileEnumerator json_files(
      external_extension_search_path,
      false,  // Recursive.
      base::FileEnumerator::FILES);
#if BUILDFLAG(IS_WIN)
  base::FilePath::StringType extension = base::UTF8ToWide(".json");
#elif BUILDFLAG(IS_POSIX)
  base::FilePath::StringType extension(".json");
#endif
  do {
    base::FilePath file = json_files.Next();
    if (file.BaseName().value() == kExternalExtensionJson)
      continue;  // Already taken care of elsewhere.
    if (file.empty())
      break;
    if (file.MatchesExtension(extension)) {
      if (SkipInstallForChromeOSTablet(file))
        continue;
      external_extension_paths.insert(file.BaseName());
    } else {
      DVLOG(1) << "Not considering: " << file.LossyDisplayName()
               << " (does not have a .json extension)";
    }
  } while (true);

  return external_extension_paths;
}

}  // namespace

namespace extensions {

ExternalPrefLoader::ExternalPrefLoader(int base_path_id,
                                       int options,
                                       Profile* profile)
    : base_path_id_(base_path_id), options_(options), profile_(profile) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ExternalPrefLoader::~ExternalPrefLoader() = default;

const base::FilePath ExternalPrefLoader::GetBaseCrxFilePath() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |base_path_| was set in LoadOnFileThread().
  return base_path_;
}

void ExternalPrefLoader::StartLoading() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetExtensionFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadOnFileThread, this));
}

// static.
base::DictValue ExternalPrefLoader::ExtractExtensionPrefs(
    base::ValueDeserializer* deserializer,
    const base::FilePath& path) {
  std::string error_msg;
  std::unique_ptr<base::Value> extensions =
      deserializer->Deserialize(nullptr, &error_msg);
  if (!extensions) {
    LOG(WARNING) << "Unable to deserialize json data: " << error_msg
                 << " in file " << path.value() << ".";
    return base::DictValue();
  }

  if (extensions->is_dict())
    return std::move(*extensions).TakeDict();

  LOG(WARNING) << "Expected a JSON dictionary in file " << path.value() << ".";
  return base::DictValue();
}

void ExternalPrefLoader::LoadOnFileThread() {
  base::DictValue prefs;

  // TODO(skerner): Some values of base_path_id_ will cause
  // base::PathService::Get() to return false, because the path does
  // not exist.  Find and fix the build/install scripts so that
  // this can become a CHECK().  Known examples include chrome
  // OS developer builds and linux install packages.
  // Tracked as crbug.com/41308810 .
  if (base::PathService::Get(base_path_id_, &base_path_)) {
    ReadExternalExtensionPrefFile(prefs);

    if (!prefs.empty())
      LOG(WARNING) << "You are using an old-style extension deployment method "
                      "(external_extensions.json), which will soon be "
                      "deprecated. (see http://developer.chrome.com/"
                      "docs/extensions/how-to/distribute/install-extensions)";

    ReadStandaloneExtensionPrefFiles(prefs);
  }

  // If we have any records to process, then we must have
  // read at least one .json file.  If so, then we should have
  // set |base_path_|.
  if (!prefs.empty())
    CHECK(!base_path_.empty());

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ExternalPrefLoader::LoadFinished, this,
                                std::move(prefs)));
}

void ExternalPrefLoader::ReadExternalExtensionPrefFile(base::DictValue& prefs) {
  base::FilePath json_file = base_path_.Append(kExternalExtensionJson);

  if (!base::PathExists(json_file)) {
    // This is not an error.  The file does not exist by default.
    return;
  }

  if (IsOptionSet(ENSURE_PATH_CONTROLLED_BY_ADMIN)) {
#if BUILDFLAG(IS_MAC)
    if (!base::VerifyPathControlledByAdmin(json_file)) {
      LOG(ERROR) << "Can not read external extensions source.  The file "
                 << json_file.value() << " and every directory in its path, "
                 << "must be owned by root, have group \"admin\", and not be "
                 << "writable by all users. These restrictions prevent "
                 << "unprivleged users from making chrome install extensions "
                 << "on other users' accounts.";
      return;
    }
#else
    // The only platform that uses this check is Mac OS.  If you add one,
    // you need to implement base::VerifyPathControlledByAdmin() for
    // that platform.
    NOTREACHED();
#endif  // BUILDFLAG(IS_MAC)
  }

  JSONFileValueDeserializer deserializer(json_file);
  auto ext_prefs = ExtractExtensionPrefs(&deserializer, json_file);
  prefs.Merge(std::move(ext_prefs));
}

void ExternalPrefLoader::ReadStandaloneExtensionPrefFiles(
    base::DictValue& prefs) {
  // First list the potential .json candidates.
  std::set<base::FilePath> candidates =
      GetPrefsCandidateFilesFromFolder(base_path_);
  if (candidates.empty()) {
    DVLOG(1) << "Extension candidates list empty";
    return;
  }

  // For each file read the json description & build the proper
  // associated prefs.
  for (auto it = candidates.begin(); it != candidates.end(); ++it) {
    base::FilePath extension_candidate_path = base_path_.Append(*it);

    const std::string id =
#if BUILDFLAG(IS_WIN)
        base::WideToASCII(
            extension_candidate_path.RemoveExtension().BaseName().value());
#elif BUILDFLAG(IS_POSIX)
        extension_candidate_path.RemoveExtension().BaseName().value();
#endif

    DVLOG(1) << "Reading json file: "
             << extension_candidate_path.LossyDisplayName();

    JSONFileValueDeserializer deserializer(extension_candidate_path);
    auto ext_prefs =
        ExtractExtensionPrefs(&deserializer, extension_candidate_path);

    DVLOG(1) << "Adding extension with id: " << id;
    prefs.Set(id, std::move(ext_prefs));
  }
}

}  // namespace extensions
