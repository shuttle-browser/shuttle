// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_custom_dictionary.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_constants.h"
#include "components/spellcheck/browser/spellcheck_host_metrics.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/obsolete/md5.h"

using content::BrowserThread;

namespace spellcheck {
std::string Md5AsHexForDictionaryChecksum(std::string_view data) {
  return base::HexEncodeLower(crypto::obsolete::Md5::Hash(data));
}
}  // namespace spellcheck

namespace {

// Filename extension for backup dictionary file.
const base::FilePath::CharType BACKUP_EXTENSION[] = FILE_PATH_LITERAL("backup");

// Prefix for the checksum in the dictionary file.
const char CHECKSUM_PREFIX[] = "checksum_v1 = ";

// The status of the checksum in a custom spellcheck dictionary.
enum ChecksumStatus {
  VALID_CHECKSUM,
  INVALID_CHECKSUM,
};

// The result of a dictionary sanitation. Can be used as a bitmap.
enum ChangeSanitationResult {
  // The change is valid and can be applied as-is.
  VALID_CHANGE = 0,

  // The change contained words to be added that are not valid.
  DETECTED_INVALID_WORDS = 1,

  // The change contained words to be added that are already in the dictionary.
  DETECTED_DUPLICATE_WORDS = 2,

  // The change contained words to be removed that are not in the dictionary.
  DETECTED_MISSING_WORDS = 4,
};

// Loads the file at |file_path| into the |words| container. If the file has a
// valid checksum, then returns ChecksumStatus::VALID. If the file has an
// invalid checksum, then returns ChecksumStatus::INVALID and clears |words|.
ChecksumStatus LoadFile(const base::FilePath& file_path,
                        std::set<std::string>* words) {
  DCHECK(words);
  words->clear();
  std::string contents;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    base::ReadFileToString(file_path, &contents);
  }
  size_t pos = contents.rfind(CHECKSUM_PREFIX);
  if (pos != std::string::npos) {
    std::string checksum = contents.substr(pos + strlen(CHECKSUM_PREFIX));
    contents.erase(pos);
    if (checksum != spellcheck::Md5AsHexForDictionaryChecksum(contents)) {
      return INVALID_CHECKSUM;
    }
  }

  std::vector<std::string> word_list = base::SplitString(
      base::TrimWhitespaceASCII(contents, base::TRIM_ALL), "\n",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  words->insert(std::make_move_iterator(word_list.begin()),
                std::make_move_iterator(word_list.end()));
  return VALID_CHECKSUM;
}

// Returns true for valid custom dictionary words.
bool IsValidWord(const std::string& word) {
  std::string tmp;
  return !word.empty() &&
         word.size() <= spellcheck::kMaxCustomDictionaryWordBytes &&
         base::IsStringUTF8(word) &&
         base::TRIM_NONE ==
             base::TrimWhitespaceASCII(word, base::TRIM_ALL, &tmp);
}

// Removes duplicate and invalid words from |to_add| word list. Looks for
// duplicates in both |to_add| and |existing| word lists. Returns a bitmap of
// |ChangeSanitationResult| values.
int SanitizeWordsToAdd(const std::set<std::string>& existing,
                       std::set<std::string>* to_add) {
  DCHECK(to_add);
  // Do not add duplicate words.
  std::vector<std::string> new_words =
      base::STLSetDifference<std::vector<std::string>>(*to_add, existing);
  int result = VALID_CHANGE;
  if (to_add->size() != new_words.size())
    result |= DETECTED_DUPLICATE_WORDS;
  // Do not add invalid words.
  std::set<std::string> valid_new_words;
  for (const auto& word : new_words) {
    if (IsValidWord(word))
      valid_new_words.insert(valid_new_words.end(), word);
  }
  if (valid_new_words.size() != new_words.size())
    result |= DETECTED_INVALID_WORDS;
  // Save the sanitized words to be added.
  std::swap(*to_add, valid_new_words);
  return result;
}

// Loads and returns the custom spellcheck dictionary from |path|. Must be
// called on the file thread.
std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
LoadDictionaryFileReliably(const base::FilePath& path) {
  // Load the contents and verify the checksum.
  std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult> result(
      new SpellcheckCustomDictionary::LoadFileResult);
  if (LoadFile(path, &result->words) == VALID_CHECKSUM) {
    result->is_valid_file =
        VALID_CHANGE ==
        SanitizeWordsToAdd(std::set<std::string>(), &result->words);
    return result;
  }
  // Checksum is not valid. See if there's a backup.
  base::FilePath backup = path.AddExtension(BACKUP_EXTENSION);
  if (base::PathExists(backup))
    LoadFile(backup, &result->words);
  SanitizeWordsToAdd(std::set<std::string>(), &result->words);
  return result;
}

// Backs up the original dictionary, saves |custom_words| and its checksum into
// the custom spellcheck dictionary at |path|.
void SaveDictionaryFileReliably(const base::FilePath& path,
                                const std::set<std::string>& custom_words) {
  std::stringstream content;
  for (const std::string& word : custom_words)
    content << word << '\n';

  std::string checksum =
      spellcheck::Md5AsHexForDictionaryChecksum(content.str());
  content << CHECKSUM_PREFIX << checksum;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    base::FilePath backup_path = path.AddExtension(BACKUP_EXTENSION);
    if (!custom_words.empty()) {
      base::CopyFile(path, backup_path);
    } else {
      // The wordlist was just cleared, clean up the .backup file for privacy
      // reasons.
      base::DeleteFile(backup_path);
    }
    base::ImportantFileWriter::WriteFileAtomically(path, content.str());
  }
}

void SavePassedWordsToDictionaryFileReliably(
    const base::FilePath& path,
    std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
        load_file_result) {
  DCHECK(load_file_result);
  SaveDictionaryFileReliably(path, load_file_result->words);
}

// Removes word from |to_remove| that are missing from |existing| word list and
// sorts |to_remove|. Returns a bitmap of |ChangeSanitationResult| values.
int SanitizeWordsToRemove(const std::set<std::string>& existing,
                          std::set<std::string>* to_remove) {
  DCHECK(to_remove);
  // Do not remove words that are missing from the dictionary.
  std::set<std::string> found_words =
      base::STLSetIntersection<std::set<std::string>>(existing, *to_remove);
  int result = VALID_CHANGE;
  if (to_remove->size() > found_words.size())
    result |= DETECTED_MISSING_WORDS;
  // Save the sanitized words to be removed.
  std::swap(*to_remove, found_words);
  return result;
}

}  // namespace

SpellcheckCustomDictionary::Change::Change() = default;

SpellcheckCustomDictionary::Change::~Change() = default;

void SpellcheckCustomDictionary::Change::AddWord(const std::string& word) {
  to_add_.insert(word);
}

void SpellcheckCustomDictionary::Change::AddWords(
    const std::set<std::string>& words) {
  to_add_.insert(words.begin(), words.end());
}

void SpellcheckCustomDictionary::Change::RemoveWord(const std::string& word) {
  to_remove_.insert(word);
}

void SpellcheckCustomDictionary::Change::Clear() {
  clear_ = true;
}

int SpellcheckCustomDictionary::Change::Sanitize(
    const std::set<std::string>& words) {
  int result = VALID_CHANGE;
  if (!to_add_.empty())
    result |= SanitizeWordsToAdd(words, &to_add_);
  if (!to_remove_.empty())
    result |= SanitizeWordsToRemove(words, &to_remove_);
  return result;
}

SpellcheckCustomDictionary::SpellcheckCustomDictionary(
    const base::FilePath& dictionary_directory_name)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      custom_dictionary_path_(
          dictionary_directory_name.Append(chrome::kCustomDictionaryFileName)),
      is_loaded_(false) {}

SpellcheckCustomDictionary::~SpellcheckCustomDictionary() = default;

std::set<std::string> SpellcheckCustomDictionary::GetWords() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return words_;
}

bool SpellcheckCustomDictionary::AddWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Change> dictionary_change(new Change);
  dictionary_change->AddWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Save(std::move(dictionary_change));
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::RemoveWord(const std::string& word) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::unique_ptr<Change> dictionary_change(new Change);
  dictionary_change->RemoveWord(word);
  int result = dictionary_change->Sanitize(GetWords());
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Save(std::move(dictionary_change));
  return result == VALID_CHANGE;
}

bool SpellcheckCustomDictionary::HasWord(const std::string& word) const {
  return GetWords().contains(word);
}

void SpellcheckCustomDictionary::Clear() {
  std::unique_ptr<Change> dictionary_change(new Change);
  dictionary_change->Clear();
  Apply(*dictionary_change);
  Notify(*dictionary_change);
  Save(std::move(dictionary_change));
}

void SpellcheckCustomDictionary::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void SpellcheckCustomDictionary::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

bool SpellcheckCustomDictionary::IsLoaded() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return is_loaded_;
}

void SpellcheckCustomDictionary::Load() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SpellcheckCustomDictionary::LoadDictionaryFile,
                     custom_dictionary_path_),
      base::BindOnce(&SpellcheckCustomDictionary::OnLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

SpellcheckCustomDictionary::LoadFileResult::LoadFileResult()
    : is_valid_file(false) {}

SpellcheckCustomDictionary::LoadFileResult::~LoadFileResult() = default;

// static
std::unique_ptr<SpellcheckCustomDictionary::LoadFileResult>
SpellcheckCustomDictionary::LoadDictionaryFile(const base::FilePath& path) {
  std::unique_ptr<LoadFileResult> result = LoadDictionaryFileReliably(path);
  return result;
}

// static
void SpellcheckCustomDictionary::UpdateDictionaryFile(
    std::unique_ptr<Change> dictionary_change,
    const base::FilePath& path) {
  DCHECK(dictionary_change);

  if (dictionary_change->empty())
    return;

  std::unique_ptr<LoadFileResult> result = LoadDictionaryFileReliably(path);

  // Clear.
  if (dictionary_change->clear())
    result->words.clear();

  // Add words.
  result->words.insert(dictionary_change->to_add().begin(),
                       dictionary_change->to_add().end());

  // Remove words and save the remainder.
  SaveDictionaryFileReliably(
      path, base::STLSetDifference<std::set<std::string>>(
                result->words, dictionary_change->to_remove()));
}

void SpellcheckCustomDictionary::OnLoaded(
    std::unique_ptr<LoadFileResult> result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(result);
  Change dictionary_change;
  dictionary_change.AddWords(result->words);
  dictionary_change.Sanitize(words_);
  ApplyToSet(dictionary_change, &words_);
  is_loaded_ = true;
  for (Observer& observer : observers_)
    observer.OnCustomDictionaryLoaded();
  if (!result->is_valid_file) {
    // Save cleaned up data only after startup.
    fix_invalid_file_.Reset(
        base::BindOnce(&SpellcheckCustomDictionary::FixInvalidFile,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
    content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
        ->PostTask(FROM_HERE, fix_invalid_file_.callback());
  }
}

void SpellcheckCustomDictionary::ApplyToSet(const Change& dictionary_change,
                                            std::set<std::string>* words) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (dictionary_change.clear()) {
    words->clear();
  }
  const std::set<std::string>& to_add = dictionary_change.to_add();
  if (!to_add.empty()) {
    words->insert(to_add.begin(), to_add.end());
  }
  for (const auto& word : dictionary_change.to_remove()) {
    words->erase(word);
  }
}

void SpellcheckCustomDictionary::Apply(const Change& dictionary_change) {
  ApplyToSet(dictionary_change, &words_);
}

void SpellcheckCustomDictionary::FixInvalidFile(
    std::unique_ptr<LoadFileResult> load_file_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SavePassedWordsToDictionaryFileReliably,
                     custom_dictionary_path_, std::move(load_file_result)));
}

void SpellcheckCustomDictionary::Save(
    std::unique_ptr<Change> dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  fix_invalid_file_.Cancel();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SpellcheckCustomDictionary::UpdateDictionaryFile,
                     std::move(dictionary_change), custom_dictionary_path_));
}

void SpellcheckCustomDictionary::Notify(const Change& dictionary_change) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!IsLoaded() || dictionary_change.empty())
    return;
  for (Observer& observer : observers_)
    observer.OnCustomDictionaryChanged(dictionary_change);
}
