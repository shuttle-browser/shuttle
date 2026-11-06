// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/url_identity.h"

#include <optional>
#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"


#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "extensions/browser/extension_registry.h"  // nogncheck
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

using Type = UrlIdentity::Type;
using DefaultFormatOptions = UrlIdentity::DefaultFormatOptions;
using FormatOptions = UrlIdentity::FormatOptions;

namespace {

UrlIdentity CreateDefaultUrlIdentityFromUrl(const GURL& url,
                                            const FormatOptions& options) {
  std::u16string name;

  if (options.default_options.Has(DefaultFormatOptions::kRawSpec)) {
    return UrlIdentity{
        .type = Type::kDefault,
        .name = base::CollapseWhitespace(base::UTF8ToUTF16(url.spec()), false)};
  }
  if (options.default_options.Has(
          DefaultFormatOptions::kOmitCryptographicScheme)) {
    name = url_formatter::FormatUrlForSecurityDisplay(
        url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  } else if (options.default_options.Has(DefaultFormatOptions::kHostname)) {
    name = url_formatter::IDNToUnicode(url.host());
  } else if (options.default_options.Has(
                 DefaultFormatOptions::kOmitSchemePathAndTrivialSubdomains)) {
    name = url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
        url);
  } else {
    name = url_formatter::FormatUrlForSecurityDisplay(url);
  }

  return UrlIdentity{.type = Type::kDefault, .name = name};
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
UrlIdentity CreateChromeExtensionIdentityFromUrl(Profile* profile,
                                                 const GURL& url,
                                                 const FormatOptions& options) {
  CHECK(url.SchemeIs(extensions::kExtensionScheme), base::NotFatalUntil::M162);

  DCHECK(profile) << "Profile cannot be null when type is Chrome Extensions.";

  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);
  CHECK(extension_registry, base::NotFatalUntil::M162);

  const extensions::Extension* extension = nullptr;
  extension = extension_registry->enabled_extensions().GetByID(url.GetHost());

  if (!extension) {  // fallback to default
    return CreateDefaultUrlIdentityFromUrl(url, options);
  }

  return UrlIdentity{.type = Type::kChromeExtension,
                     .name = base::CollapseWhitespace(
                         base::UTF8ToUTF16(extension->name()), false)};
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)


UrlIdentity CreateFileIdentityFromUrl(Profile* profile,
                                      const GURL& url,
                                      const FormatOptions& options) {
  CHECK(url.SchemeIsFile(), base::NotFatalUntil::M162);

  return UrlIdentity{
      .type = Type::kFile,
      .name = url_formatter::FormatUrlForSecurityDisplay(url),
  };
}
}  // namespace

UrlIdentity UrlIdentity::CreateFromUrl(Profile* profile,
                                       const GURL& url,
                                       const TypeSet& allowed_types,
                                       const FormatOptions& options) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (url.SchemeIs(extensions::kExtensionScheme)) {
    // TODO(crbug.com/577619404): CHECK-exclusion: Convert to a CHECK once we
    // are confident it won't be triggered.
    DCHECK(allowed_types.Has(Type::kChromeExtension));
    return CreateChromeExtensionIdentityFromUrl(profile, url, options);
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)


  if (url.SchemeIsFile()) {
    CHECK(allowed_types.Has(Type::kFile), base::NotFatalUntil::M162);
    return CreateFileIdentityFromUrl(profile, url, options);
  }

  CHECK(allowed_types.Has(Type::kDefault), base::NotFatalUntil::M162);
  return CreateDefaultUrlIdentityFromUrl(url, options);
}
