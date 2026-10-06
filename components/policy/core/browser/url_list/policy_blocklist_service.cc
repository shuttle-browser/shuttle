// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/url_list/policy_blocklist_service.h"

#include <utility>

#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kAllTrafficWildcard[] = "*";

// Returns a URL filter that covers all URL navigations.
base::ListValue GetAllTrafficFilter() {
  base::ListValue all_traffic;
  all_traffic.Append(kAllTrafficWildcard);
  // URLBlocklistManager excludes chrome:// and chrome-untrusted:// URLs by
  // default, see b/483966539. However in the Always-on VPN in strict mode with
  // lockdown enabled case, they should be blocked by default and only allowed
  // if explicitly allowlisted by the Administrator.
  all_traffic.Append("chrome://*");
  all_traffic.Append("chrome-untrusted://*");
  return all_traffic;
}

// BlocklistSource implementation that blocks all traffic with the
// exception of URLs specified by the admin via the policy
// AlwaysOnVpnPreConnectUrlAllowlist.
// Note that this implementation only supports one observer at a time. Adding a
// new observer will remove the previous one.
class AlwaysOnVpnPreConnectBlocklistSource : public policy::BlocklistSource {
 public:
  AlwaysOnVpnPreConnectBlocklistSource(PrefService* pref_service)
      : blocklist_(GetAllTrafficFilter()) {
    pref_change_registrar_.Init(pref_service);
  }
  AlwaysOnVpnPreConnectBlocklistSource(
      const AlwaysOnVpnPreConnectBlocklistSource&) = delete;
  AlwaysOnVpnPreConnectBlocklistSource& operator=(
      const AlwaysOnVpnPreConnectBlocklistSource&) = delete;
  ~AlwaysOnVpnPreConnectBlocklistSource() override = default;

  const base::ListValue* GetBlocklistSpec() const override {
    return &blocklist_;
  }

  const base::ListValue* GetAllowlistSpec() const override {
    return &pref_change_registrar_.prefs()->GetList(
        policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist);
  }

  // Adds an observer which will be notified when the blocklist is updated, i.e.
  // when the preference `
  // policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist` has a new value.
  // If an observer already exists, it will be removed.
  void SetBlocklistObserver(base::RepeatingClosure observer) override {
    pref_change_registrar_.RemoveAll();
    pref_change_registrar_.Add(
        policy::policy_prefs::kAlwaysOnVpnPreConnectUrlAllowlist, observer);
  }

  bool DowngradeAllowlistWildcardToNeutral() const override { return true; }

 private:
  const base::ListValue blocklist_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif

PolicyBlocklistService::PolicyBlocklistService(
    std::unique_ptr<policy::URLBlocklistManager> url_blocklist_manager,
    PrefService* user_prefs)
    : url_blocklist_manager_(std::move(url_blocklist_manager)),
      user_prefs_(user_prefs) {
  CHECK(user_prefs_);
}

PolicyBlocklistService::~PolicyBlocklistService() = default;

policy::URLBlocklist::URLBlocklistState
PolicyBlocklistService::GetURLBlocklistState(const GURL& url) const {
  return GetURLBlocklistStateWithPolicySource(url).url_blocklist_state;
}

PolicyBlocklistService::PolicyBlocklistState
PolicyBlocklistService::GetURLBlocklistStateWithPolicySource(
    const GURL& url) const {
  const auto general_state = url_blocklist_manager_->GetURLBlocklistState(url);
  return {.url_blocklist_state = general_state,
          .policy_source =
              PolicyBlocklistService::PolicyBlocklistState::URL_POLICY};
}

#if BUILDFLAG(IS_CHROMEOS)
void PolicyBlocklistService::SetAlwaysOnVpnPreConnectUrlAllowlistEnforced(
    bool enforced) {
  if (enforced) {
    url_blocklist_manager_->SetOverrideBlockListSource(
        std::make_unique<AlwaysOnVpnPreConnectBlocklistSource>(user_prefs_));
    return;
  }
  url_blocklist_manager_->SetOverrideBlockListSource(nullptr);
}

#endif
