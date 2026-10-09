// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/strings/grit/privacy_sandbox_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/template_expressions.h"
#include "ui/base/theme_provider.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#endif

using content::BrowserThread;

namespace {

std::string ReplaceTemplateExpressions(
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const ui::TemplateReplacements& replacements) {
  return ui::ReplaceTemplateExpressions(base::as_string_view(*bytes),
                                        replacements);
}

}  // namespace

SkColor GetThemeColor(const ui::NativeTheme* native_theme,
                      const ui::ColorProvider& cp,
                      int id) {
  SkColor color = cp.GetColor(id);
  // If web contents are being inverted because the system is in high-contrast
  // mode, any system theme colors we use must be inverted too to cancel out.
  // TODO(pkasting): I'm not sure this conditional is correct anymore.
  return (native_theme->forced_colors() !=
              ui::ColorProviderKey::ForcedColors::kNone &&
          native_theme->preferred_color_scheme() ==
              ui::NativeTheme::PreferredColorScheme::kDark)
             ? color_utils::InvertColor(color)
             : color;
}

// Get the CSS string for the background position on the new tab page.
std::string GetNewTabBackgroundPositionCSS(
    const ui::ThemeProvider& theme_provider) {
  // TODO(glen): This is a quick workaround to hide the notused.png image when
  // no image is provided - we don't have time right now to figure out why
  // this is painting as white.
  // http://crbug.com/40963242
  if (!theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    return "-64px";
  }

  return ThemeProperties::AlignmentToString(theme_provider.GetDisplayProperty(
      ThemeProperties::NTP_BACKGROUND_ALIGNMENT));
}

// How the background image on the new tab page should be tiled (see tiling
// masks in theme_service.h).
std::string GetNewTabBackgroundTilingCSS(
    const ui::ThemeProvider& theme_provider) {
  int repeat_mode =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING);
  return ThemeProperties::TilingToString(repeat_mode);
}

NTPResourceCache::NTPResourceCache(Profile* profile) : profile_(profile) {
  ThemeServiceFactory::GetForProfile(profile_)->AddObserver(this);

  // TODO(crbug.com/40677117): Remove the global accessor to NativeTheme.
  theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
}

NTPResourceCache::~NTPResourceCache() = default;

NTPResourceCache::WindowType NTPResourceCache::GetWindowType(Profile* profile) {
  return NORMAL;
}

base::RefCountedMemory* NTPResourceCache::GetNewTabHTML(
    WindowType win_type,
    const content::WebContents::Getter& wc_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  switch (win_type) {
    case NORMAL:
      if (!new_tab_normal_html_) {
        // Shuttle: regular profiles get a blank New Tab page.
        new_tab_normal_html_ = base::MakeRefCounted<base::RefCountedString>(
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>" +
            base::EscapeForHTML(l10n_util::GetStringUTF8(IDS_NEW_TAB_TITLE)) +
            "</title><style>:root{color-scheme:light dark}</style>"
            "</head><body></body></html>");
      }
      return new_tab_normal_html_.get();
  }
}

base::RefCountedMemory* NTPResourceCache::GetNewTabCSS(
    WindowType win_type,
    const content::WebContents::Getter& wc_getter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!new_tab_css_) {
    CreateNewTabCSS(wc_getter);
  }
  return new_tab_css_.get();
}

void NTPResourceCache::OnThemeChanged() {
  Invalidate();
}

void NTPResourceCache::Shutdown() {
  ThemeServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void NTPResourceCache::OnNativeThemeUpdated(ui::NativeTheme* updated_theme) {
  // TODO(crbug.com/40677117): Remove the global accessor to NativeTheme.
  DCHECK_EQ(updated_theme, ui::NativeTheme::GetInstanceForNativeUi());
  Invalidate();
}

void NTPResourceCache::Invalidate() {
  new_tab_css_ = nullptr;
}

void NTPResourceCache::CreateNewTabCSS(
    const content::WebContents::Getter& wc_getter) {
  auto* web_contents = wc_getter.Run();
  const ui::NativeTheme* native_theme =
      webui::GetNativeThemeDeprecated(web_contents);
  DCHECK(native_theme);

  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile_);
  const ui::ColorProvider& cp = web_contents->GetColorProvider();

  // Get our theme colors.
  SkColor color_background =
      GetThemeColor(native_theme, cp, kColorNewTabPageBackground);
  SkColor color_text = GetThemeColor(native_theme, cp, kColorNewTabPageText);
  SkColor color_text_light =
      GetThemeColor(native_theme, cp, kColorNewTabPageTextLight);

  SkColor color_section_border =
      GetThemeColor(native_theme, cp, kColorNewTabPageSectionBorder);

  // Generate the replacements.
  ui::TemplateReplacements substitutions;

  // Cache-buster for background.
  substitutions["themeId"] =
      profile_->GetPrefs()->GetString(prefs::kCurrentThemeID);

  // Colors.
  substitutions["colorBackground"] =
      color_utils::SkColorToRgbaString(color_background);
  substitutions["colorLink"] = color_utils::SkColorToRgbString(
      GetThemeColor(native_theme, cp, kColorNewTabPageLink));
  substitutions["backgroundPosition"] = GetNewTabBackgroundPositionCSS(tp);
  substitutions["backgroundTiling"] = GetNewTabBackgroundTilingCSS(tp);
  substitutions["colorTextRgba"] = color_utils::SkColorToRgbaString(color_text);
  substitutions["colorTextLight"] =
      color_utils::SkColorToRgbaString(color_text_light);
  substitutions["colorSectionBorder"] =
      color_utils::SkColorToRgbString(color_section_border);
  substitutions["colorText"] = color_utils::SkColorToRgbString(color_text);

  // For themes that right-align the background, we flip the attribution to the
  // left to avoid conflicts.
  int alignment =
      tp.GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
  if (alignment & ThemeProperties::ALIGN_RIGHT) {
    substitutions["leftAlignAttribution"] = "0";
    substitutions["rightAlignAttribution"] = "auto";
    substitutions["textAlignAttribution"] = "right";
  } else {
    substitutions["leftAlignAttribution"] = "auto";
    substitutions["rightAlignAttribution"] = "0";
    substitutions["textAlignAttribution"] = "left";
  }

  substitutions["displayAttribution"] =
      tp.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION) ? "inline" : "none";

  // Get our template.
  static const base::NoDestructor<scoped_refptr<base::RefCountedMemory>>
      new_tab_theme_css(
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              IDR_NEW_TAB_4_THEME_CSS));
  CHECK(*new_tab_theme_css);
  new_tab_css_ = base::MakeRefCounted<base::RefCountedString>(
      ReplaceTemplateExpressions(*new_tab_theme_css, substitutions));
}
