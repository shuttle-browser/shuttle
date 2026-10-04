// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/toolbar/reading_list_sub_menu_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/ui_base_features.h"

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kShowBookmarkBarMenuItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kShowBookmarkSidePanelItem);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(BookmarkSubMenuModel,
                                      kReadingListMenuItem);

// For views and cocoa, we have complex delegate systems to handle
// injecting the bookmarks to the bookmark submenu. This is done to support
// advanced interactions with the menu contents, like right click context menus.

BookmarkSubMenuModel::BookmarkSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate,
    BrowserWindowInterface* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

BookmarkSubMenuModel::~BookmarkSubMenuModel() = default;

void BookmarkSubMenuModel::Build(BrowserWindowInterface* browser) {
#if !BUILDFLAG(IS_CHROMEOS)
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
#endif

  AddSeparator(ui::NORMAL_SEPARATOR);

  reading_list_sub_menu_model_ =
      std::make_unique<ReadingListSubMenuModel>(delegate());
  AddSubMenuWithStringIdAndIcon(
      AppMenuModel::kReadingListMenuPlaceholder, IDS_READING_LIST_MENU,
      reading_list_sub_menu_model_.get(),
      ui::ImageModel::FromVectorIcon(features::IsRoundedIconsEnabled()
                                         ? kListAltIcon
                                         : kReadingListOldIcon));
  SetElementIdentifierAt(
      GetIndexOfCommandId(AppMenuModel::kReadingListMenuPlaceholder).value(),
      kReadingListMenuItem);

  auto set_icon = [this](int command_id, const gfx::VectorIcon& vector_icon) {
    auto index = GetIndexOfCommandId(command_id);
    if (index) {
      SetIcon(index.value(), ui::ImageModel::FromVectorIcon(
                                 vector_icon, ui::kColorMenuIcon, 16));
    }
  };

  set_icon(IDC_IMPORT_SETTINGS, features::IsRoundedIconsEnabled()
                                    ? kMenuBookIcon
                                    : kMenuBookChromeRefreshOldIcon);
}
