// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Bookmarks/UI/BookmarksMainMenu.h"
#include "Bookmarks/BookmarkHelper.h"
#include "Bookmarks/BookmarkTree.h"
#include "CoreInterface.h"
#include "MainResource.h"
#include "ResourceHelper.h"
#include "ShellBrowser/ShellBrowser.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "TabContainer.h"
#include "../Helper/DpiCompatibility.h"
#include "../Helper/MenuHelper.h"

BookmarksMainMenu::BookmarksMainMenu(CoreInterface *coreInterface, Navigator *navigator,
	IconFetcher *iconFetcher, BookmarkTree *bookmarkTree, const MenuIdRange &menuIdRange) :
	m_coreInterface(coreInterface),
	m_navigator(navigator),
	m_bookmarkTree(bookmarkTree),
	m_menuIdRange(menuIdRange),
	m_menuBuilder(coreInterface, iconFetcher, coreInterface->GetResourceInstance()),
	m_controller(bookmarkTree, coreInterface, navigator, coreInterface->GetMainWindow())
{
	m_connections.push_back(coreInterface->AddMainMenuPreShowObserver(
		std::bind_front(&BookmarksMainMenu::OnMainMenuPreShow, this)));
	m_connections.push_back(coreInterface->AddGetMenuItemHelperTextObserver(
		std::bind_front(&BookmarksMainMenu::MaybeGetMenuItemHelperText, this)));
	m_connections.push_back(coreInterface->AddMainMenuItemMiddleClickedObserver(
		std::bind_front(&BookmarksMainMenu::OnMenuItemMiddleClicked, this)));
	m_connections.push_back(coreInterface->AddMainMenuItemRightClickedObserver(
		std::bind_front(&BookmarksMainMenu::OnMenuItemRightClicked, this)));
}

BookmarksMainMenu::~BookmarksMainMenu()
{
	MENUITEMINFO mii;
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_SUBMENU;
	mii.hSubMenu = nullptr;
	SetMenuItemInfo(GetMenu(m_coreInterface->GetMainWindow()), IDM_BOOKMARKS, FALSE, &mii);
}

void BookmarksMainMenu::OnMainMenuPreShow(HMENU mainMenu)
{
	std::vector<wil::unique_hbitmap> menuImages;
	BookmarkMenuBuilder::MenuInfo menuInfo;
	auto bookmarksMenu = BuildMainBookmarksMenu(menuImages, menuInfo);

	MENUITEMINFO mii;
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_SUBMENU;
	mii.hSubMenu = bookmarksMenu.get();
	SetMenuItemInfo(mainMenu, IDM_BOOKMARKS, FALSE, &mii);

	m_bookmarksMenu = std::move(bookmarksMenu);
	m_menuImages = std::move(menuImages);
	m_menuInfo = menuInfo;
}

wil::unique_hmenu BookmarksMainMenu::BuildMainBookmarksMenu(
	std::vector<wil::unique_hbitmap> &menuImages, BookmarkMenuBuilder::MenuInfo &menuInfo)
{
	wil::unique_hmenu menu(CreatePopupMenu());

	UINT dpi = DpiCompatibility::GetInstance().GetDpiForWindow(m_coreInterface->GetMainWindow());

	std::wstring bookmarkThisTabText = ResourceHelper::LoadString(
		m_coreInterface->GetResourceInstance(), IDS_MENU_BOOKMARK_THIS_TAB);
	MenuHelper::AddStringItem(menu.get(), IDM_BOOKMARKS_BOOKMARKTHISTAB, bookmarkThisTabText, 0,
		TRUE);
	ResourceHelper::SetMenuItemImage(menu.get(), IDM_BOOKMARKS_BOOKMARKTHISTAB,
		m_coreInterface->GetIconResourceLoader(), Icon::AddBookmark, dpi, menuImages);

	std::wstring bookmarkAllTabsText = ResourceHelper::LoadString(
		m_coreInterface->GetResourceInstance(), IDS_MENU_BOOKMARK_ALL_TABS);
	MenuHelper::AddStringItem(menu.get(), IDM_BOOKMARKS_BOOKMARK_ALL_TABS, bookmarkAllTabsText, 1,
		TRUE);

	std::wstring manageBookmarksText = ResourceHelper::LoadString(
		m_coreInterface->GetResourceInstance(), IDS_MENU_MANAGE_BOOKMARKS);
	MenuHelper::AddStringItem(menu.get(), IDM_BOOKMARKS_MANAGEBOOKMARKS, manageBookmarksText, 2,
		TRUE);
	ResourceHelper::SetMenuItemImage(menu.get(), IDM_BOOKMARKS_MANAGEBOOKMARKS,
		m_coreInterface->GetIconResourceLoader(), Icon::Bookmarks, dpi, menuImages);

	AddBookmarkItemsToMenu(menu.get(), m_menuIdRange, GetMenuItemCount(menu.get()), menuImages,
		menuInfo);
	AddOtherBookmarksToMenu(menu.get(), { menuInfo.nextMenuId, m_menuIdRange.endId },
		GetMenuItemCount(menu.get()), menuImages, menuInfo);

	return menu;
}

void BookmarksMainMenu::AddBookmarkItemsToMenu(HMENU menu, const MenuIdRange &menuIdRange,
	int position, std::vector<wil::unique_hbitmap> &menuImages,
	BookmarkMenuBuilder::MenuInfo &menuInfo)
{
	BookmarkItem *bookmarksMenuFolder = m_bookmarkTree->GetBookmarksMenuFolder();

	if (bookmarksMenuFolder->GetChildren().empty())
	{
		return;
	}

	MenuHelper::AddSeparator(menu, position++, TRUE);

	m_menuBuilder.BuildMenu(m_coreInterface->GetMainWindow(), menu, bookmarksMenuFolder,
		menuIdRange, position, menuImages, menuInfo);
}

void BookmarksMainMenu::AddOtherBookmarksToMenu(HMENU menu, const MenuIdRange &menuIdRange,
	int position, std::vector<wil::unique_hbitmap> &menuImages,
	BookmarkMenuBuilder::MenuInfo &menuInfo)
{
	BookmarkItem *otherBookmarksFolder = m_bookmarkTree->GetOtherBookmarksFolder();

	if (otherBookmarksFolder->GetChildren().empty())
	{
		return;
	}

	MenuHelper::AddSeparator(menu, position++, TRUE);

	// Note that as DestroyMenu is recursive, this menu will be destroyed when its parent menu is.
	wil::unique_hmenu subMenu(CreatePopupMenu());
	m_menuBuilder.BuildMenu(m_coreInterface->GetMainWindow(), subMenu.get(), otherBookmarksFolder,
		menuIdRange, 0, menuImages, menuInfo);

	std::wstring otherBookmarksName = otherBookmarksFolder->GetName();
	MenuHelper::AddSubMenuItem(menu, otherBookmarksName, std::move(subMenu), position++, TRUE);
}

std::optional<std::wstring> BookmarksMainMenu::MaybeGetMenuItemHelperText(HMENU menu, int id)
{
	if (!m_menuInfo.menus.contains(menu))
	{
		return std::nullopt;
	}

	auto itr = m_menuInfo.itemIdMap.find(id);

	if (itr == m_menuInfo.itemIdMap.end())
	{
		return std::nullopt;
	}

	const BookmarkItem *bookmark = itr->second;
	return bookmark->GetLocation();
}

void BookmarksMainMenu::OnMenuItemClicked(int menuItemId)
{
	auto itr = m_menuInfo.itemIdMap.find(menuItemId);

	if (itr == m_menuInfo.itemIdMap.end())
	{
		return;
	}

	m_controller.OnMenuItemSelected(itr->second, IsKeyDown(VK_CONTROL), IsKeyDown(VK_SHIFT));
}

bool BookmarksMainMenu::OnMenuItemMiddleClicked(const POINT &pt, bool isCtrlKeyDown,
	bool isShiftKeyDown)
{
	HMENU targetMenu = nullptr;
	int targetItem = -1;
	bool targetFound = false;

	for (auto menu : m_menuInfo.menus)
	{
		int item = MenuItemFromPoint(m_coreInterface->GetMainWindow(), menu, pt);

		// Although the documentation for MenuItemFromPoint() states that it returns -1 if there's
		// no menu item at the specified position, it appears the method will also return other
		// negative values on failure. So, it's better to check whether the return value is
		// positive, rather than checking whether it's equal to -1.
		if (item >= 0)
		{
			targetMenu = menu;
			targetItem = item;
			targetFound = true;
			break;
		}
	}

	if (!targetFound)
	{
		return false;
	}

	auto itr = m_menuInfo.itemPositionMap.find({ targetMenu, targetItem });

	if (itr == m_menuInfo.itemPositionMap.end())
	{
		// This branch will be taken if one of the other, non-bookmark, items on this menu is
		// clicked. In that case, there's nothing that needs to happen and there's no need for other
		// handlers to try and process this event.
		return true;
	}

	if (itr->second.menuItemType == BookmarkMenuBuilder::MenuItemType::EmptyItem)
	{
		return true;
	}

	m_controller.OnMenuItemMiddleClicked(itr->second.bookmarkItem, isCtrlKeyDown, isShiftKeyDown);

	return true;
}

bool BookmarksMainMenu::OnMenuItemRightClicked(HMENU menu, int index, const POINT &pt)
{
	if (!m_menuInfo.menus.contains(menu))
	{
		return false;
	}

	auto itr = m_menuInfo.itemPositionMap.find({ menu, index });

	if (itr == m_menuInfo.itemPositionMap.end())
	{
		// It's valid for the item not to be found, as the bookmarks menu contains several existing
		// menu items and this class only manages the actual bookmark items on the menu.
		return false;
	}

	m_controller.OnMenuItemRightClicked(itr->second.bookmarkItem, pt);

	return true;
}
