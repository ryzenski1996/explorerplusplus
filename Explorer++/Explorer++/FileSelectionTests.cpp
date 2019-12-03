// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "../MyTreeView/MyTreeView.h"

BOOL Explorerplusplus::AnyItemsSelected()
{
	HWND hFocus;

	hFocus = GetFocus();

	if (hFocus == m_hActiveListView)
	{
		if (ListView_GetSelectedCount(m_hActiveListView) > 0)
			return TRUE;
	}
	else if (hFocus == m_hTreeView)
	{
		if (TreeView_GetSelection(m_hTreeView) != NULL)
			return TRUE;
	}

	return FALSE;
}

bool Explorerplusplus::CanCreate() const
{
	auto pidlDirectory = m_pActiveShellBrowser->GetDirectoryIdl();

	SFGAOF attributes = SFGAO_FILESYSTEM;
	HRESULT hr = GetItemAttributes(pidlDirectory.get(), &attributes);

	if (FAILED(hr))
	{
		return false;
	}

	if ((attributes & SFGAO_FILESYSTEM) == SFGAO_FILESYSTEM)
	{
		return true;
	}

	// Library folders aren't filesystem folders, but they act like them
	// (e.g. they allow items to be created, copied and moved) and
	// ultimately they're backed by filesystem folders. If this is a
	// library folder, file creation will be allowed.
	return IsChildOfLibrariesFolder(pidlDirectory.get());
}

BOOL Explorerplusplus::CanCut() const
{
	return TestItemAttributes(SFGAO_CANMOVE);
}

BOOL Explorerplusplus::CanCopy() const
{
	return TestItemAttributes(SFGAO_CANCOPY);
}

BOOL Explorerplusplus::CanRename() const
{
	return TestItemAttributes(SFGAO_CANRENAME);
}

BOOL Explorerplusplus::CanDelete() const
{
	return TestItemAttributes(SFGAO_CANDELETE);
}

BOOL Explorerplusplus::CanShowFileProperties() const
{
	return TestItemAttributes(SFGAO_HASPROPSHEET);
}

/* Returns TRUE if all the specified attributes are set on the selected items. */
BOOL Explorerplusplus::TestItemAttributes(SFGAOF attributes) const
{
	SFGAOF commonAttributes = attributes;
	HRESULT hr = GetSelectionAttributes(&commonAttributes);

	if (SUCCEEDED(hr))
	{
		return (commonAttributes & attributes) == attributes;
	}

	return FALSE;
}

HRESULT Explorerplusplus::GetSelectionAttributes(SFGAOF *pItemAttributes) const
{
	HWND	hFocus;
	HRESULT	hr = E_FAIL;

	hFocus = GetFocus();

	if (hFocus == m_hActiveListView)
		hr = GetListViewSelectionAttributes(pItemAttributes);
	else if (hFocus == m_hTreeView)
		hr = GetTreeViewSelectionAttributes(pItemAttributes);

	return hr;
}

HRESULT Explorerplusplus::TestListViewItemAttributes(int item, SFGAOF attributes) const
{
	SFGAOF commonAttributes = attributes;
	HRESULT hr = GetListViewItemAttributes(item, &commonAttributes);

	if (SUCCEEDED(hr))
	{
		return (commonAttributes & attributes) == attributes;
	}

	return FALSE;
}

HRESULT Explorerplusplus::GetListViewSelectionAttributes(SFGAOF *pItemAttributes) const
{
	HRESULT hr = E_FAIL;

	/* TODO: This should probably check all selected files. */
	int iSelected = ListView_GetNextItem(m_hActiveListView, -1, LVNI_SELECTED);

	if (iSelected != -1)
	{
		hr = GetListViewItemAttributes(iSelected, pItemAttributes);
	}

	return hr;
}

HRESULT Explorerplusplus::GetListViewItemAttributes(int item, SFGAOF *pItemAttributes) const
{
	auto pidlComplete = m_pActiveShellBrowser->GetItemCompleteIdl(item);

	if (!pidlComplete)
	{
		return E_FAIL;
	}

	HRESULT hr = GetItemAttributes(pidlComplete.get(), pItemAttributes);

	return hr;
}

HRESULT Explorerplusplus::GetTreeViewSelectionAttributes(SFGAOF *pItemAttributes) const
{
	HRESULT hr = E_FAIL;
	HTREEITEM hItem = TreeView_GetSelection(m_hTreeView);

	if (hItem != NULL)
	{
		auto pidl = m_pMyTreeView->GetItemPidl(hItem);
		hr = GetItemAttributes(pidl.get(), pItemAttributes);
	}

	return hr;
}

BOOL Explorerplusplus::CanPaste() const
{
	HWND hFocus = GetFocus();

	std::list<FORMATETC> ftcList;
	CDropHandler::GetDropFormats(ftcList);

	BOOL bDataAvailable = FALSE;

	/* Check whether the drop source has the type of data
	that is needed for this drag operation. */
	for (const auto &ftc : ftcList)
	{
		if (IsClipboardFormatAvailable(ftc.cfFormat))
		{
			bDataAvailable = TRUE;
			break;
		}
	}

	if (hFocus == m_hActiveListView)
	{
		return bDataAvailable && m_pActiveShellBrowser->CanCreate();
	}
	else if (hFocus == m_hTreeView)
	{
		HTREEITEM hItem = TreeView_GetSelection(m_hTreeView);

		if (hItem != NULL)
		{
			auto pidl = m_pMyTreeView->GetItemPidl(hItem);

			SFGAOF attributes = SFGAO_FILESYSTEM;
			HRESULT hr = GetItemAttributes(pidl.get(), &attributes);

			if (hr == S_OK)
			{
				return bDataAvailable;
			}
		}
	}

	return FALSE;
}