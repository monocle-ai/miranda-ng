/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (c) 2012-18 Miranda NG team (https://miranda-ng.org),
Copyright (c) 2000-03 Miranda ICQ/IM project,
all portions of this codebase are copyrighted to the people
listed in contributors.txt.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "stdafx.h"

#include "coolscroll.h"

#ifndef SPI_GETDESKWALLPAPER
#define SPI_GETDESKWALLPAPER 115
#endif
//loads of stuff that didn't really fit anywhere else

extern int /*g_isConnecting,*/ during_sizing;

extern void ( *saveRecalcScrollBar )(HWND hwnd, struct ClcData *dat);

static int MY_pathIsAbsolute(const wchar_t *path)
{
	if (!path || !(mir_wstrlen(path) > 2))
		return 0;

	if ((path[1] == ':' && path[2] == '\\') || (path[0] == '\\' && path[1] == '\\'))
		return 1;

	return 0;
}

size_t MY_pathToRelative(const wchar_t *pSrc, wchar_t *pOut)
{
	size_t dwSrcLen, dwProfilePathLen;

	if (!pSrc || !pOut)
		return 0;
	dwSrcLen = mir_wstrlen(pSrc);
	if (!dwSrcLen || dwSrcLen > (MAX_PATH - 1))
		return 0;
	if (!MY_pathIsAbsolute(pSrc))
		goto path_not_abs;

	wchar_t szTmp[MAX_PATH];
	memcpy(szTmp, pSrc, (dwSrcLen * sizeof(wchar_t)));
	szTmp[dwSrcLen] = 0;
	wcslwr(szTmp);
	if (wcsstr(szTmp, cfg::dat.tszProfilePath)) {
		dwProfilePathLen = mir_wstrlen(cfg::dat.tszProfilePath);
		memcpy(pOut, (pSrc + (dwProfilePathLen - 1)), ((dwSrcLen - (dwProfilePathLen - 1)) * sizeof(wchar_t)));
		pOut[0] = '.';
		pOut[dwSrcLen] = 0;
		return (dwSrcLen - (dwProfilePathLen - 1));
	}

path_not_abs:
	memcpy(pOut, pSrc, (dwSrcLen * sizeof(wchar_t)));
	pOut[dwSrcLen] = 0;
	return dwSrcLen;
}

size_t MY_pathToAbsolute(const wchar_t *pSrc, wchar_t *pOut)
{
	size_t dwSrcLen;

	if (!pSrc || !pOut)
		return 0;
	dwSrcLen = mir_wstrlen(pSrc);
	if (!dwSrcLen || dwSrcLen > (MAX_PATH - 1))
		return 0;

	if (MY_pathIsAbsolute(pSrc) && pSrc[0] != '.') {
		memcpy(pOut, pSrc, (dwSrcLen * sizeof(wchar_t)));
		pOut[dwSrcLen] = 0;
		return dwSrcLen;
	}
	if (pSrc[0] == '.')
		return (mir_snwprintf(pOut, MAX_PATH, L"%s\\%s", cfg::dat.tszProfilePath, pSrc));

	return 0;
}

/*
 * performs hit-testing for reversed (mirrored) contact rows when using RTL
 * shares all the init stuff with HitTest()
 */

int RTL_HitTest(HWND hwnd, struct ClcData *dat, int testx, ClcContact *hitcontact, DWORD *flags, int indent, int hit)
{
	RECT clRect;
	int right, checkboxWidth, width;
	DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
	SIZE textSize;
	HDC hdc;
	HFONT hFont;

	GetClientRect(hwnd, &clRect);
	right = clRect.right;

	// avatar check
	if (hitcontact->type == CLCIT_CONTACT && cfg::dat.dwFlags & CLUI_FRAME_AVATARS && hitcontact->ace != nullptr && hitcontact->avatarLeft != -1)
		if (testx < right - hitcontact->avatarLeft && testx > right - hitcontact->avatarLeft - cfg::dat.avatarSize)
			if (flags)
				*flags |= CLCHT_ONAVATAR;

	if (testx > right - (dat->leftMargin + indent * dat->groupIndent)) {
		if (flags)
			*flags |= CLCHT_ONITEMINDENT;
		return hit;
	}
	checkboxWidth = 0;
	if (style & CLS_CHECKBOXES && hitcontact->type == CLCIT_CONTACT)
		checkboxWidth = dat->checkboxSize + 2;
	if (style & CLS_GROUPCHECKBOXES && hitcontact->type == CLCIT_GROUP)
		checkboxWidth = dat->checkboxSize + 2;
	if (hitcontact->type == CLCIT_INFO && hitcontact->flags & CLCIIF_CHECKBOX)
		checkboxWidth = dat->checkboxSize + 2;
	if (testx > right - (dat->leftMargin + indent * dat->groupIndent + checkboxWidth)) {
		if (flags)
			*flags |= CLCHT_ONITEMCHECK;
		return hit;
	}

	if (testx > right - (dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace)) {
		if (flags)
			*flags |= CLCHT_ONITEMICON;
		return hit;
	}

	int rightOffset = hitcontact->extraIconRightBegin;
	if (rightOffset) {
		for (int i = dat->extraColumnsCount - 1; i >= 0; i--) {
			if (hitcontact->iExtraImage[i] == EMPTY_EXTRA_ICON)
				continue;

			rightOffset -= dat->extraColumnSpacing;
			if (testx > rightOffset && testx < rightOffset + dat->extraColumnSpacing) {
				if (flags)
					*flags |= (CLCHT_ONITEMEXTRA | (i << 24));
				return hit;
			}
		}
	}

	hdc = GetDC(hwnd);
	if (hitcontact->type == CLCIT_GROUP)
		hFont = reinterpret_cast<HFONT>(SelectObject(hdc, dat->fontInfo[FONTID_GROUPS].hFont));
	else
		hFont = reinterpret_cast<HFONT>(SelectObject(hdc, dat->fontInfo[FONTID_CONTACTS].hFont));
	GetTextExtentPoint32(hdc, hitcontact->szText, (int)mir_wstrlen(hitcontact->szText), &textSize);
	width = textSize.cx;
	if (hitcontact->type == CLCIT_GROUP) {
		wchar_t *szCounts;
		szCounts = Clist_GetGroupCountsText(dat, hitcontact);
		if (szCounts[0]) {
			GetTextExtentPoint32(hdc, L" ", 1, &textSize);
			width += textSize.cx;
			SelectObject(hdc, dat->fontInfo[FONTID_GROUPCOUNTS].hFont);
			GetTextExtentPoint32(hdc, szCounts, (int)mir_wstrlen(szCounts), &textSize);
			width += textSize.cx;
		}
	}
	SelectObject(hdc, hFont);
	ReleaseDC(hwnd, hdc);
	if (testx > right - (dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace + width + 4 + (cfg::dat.dwFlags & CLUI_FRAME_AVATARS ? cfg::dat.avatarSize : 0))) {
		if (flags)
			*flags |= CLCHT_ONITEMLABEL;
		return hit;
	}
	if (cfg::dat.dwFlags & CLUI_FULLROWSELECT && !(GetKeyState(VK_SHIFT) & 0x8000) && testx < right - (dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace + width + 4 + (cfg::dat.dwFlags & CLUI_FRAME_AVATARS ? cfg::dat.avatarSize : 0))) {
		if (flags)
			*flags |= CLCHT_ONITEMSPACE;
		return hit;
	}
	if (flags)
		*flags |= CLCHT_NOWHERE;
	return -1;
}

int HitTest(HWND hwnd, struct ClcData *dat, int testx, int testy, ClcContact **contact, ClcGroup **group, DWORD *flags)
{
	ClcContact *hitcontact = nullptr;
	ClcGroup *hitgroup = nullptr;
	int indent, width, i;
	int checkboxWidth;
	SIZE textSize;
	RECT clRect;
	HFONT hFont;
	DWORD style = GetWindowLongPtr(hwnd, GWL_STYLE);
	BYTE mirror_mode = cfg::dat.bUseDCMirroring;

	if (flags)
		*flags = 0;
	GetClientRect(hwnd, &clRect);
	if (testx < 0 || testy < 0 || testy >= clRect.bottom || testx >= clRect.right) {
		if (flags) {
			if (testx < 0)
				*flags |= CLCHT_TOLEFT;
			else if (testx >= clRect.right)
				*flags |= CLCHT_TORIGHT;
			if (testy < 0)
				*flags |= CLCHT_ABOVE;
			else if (testy >= clRect.bottom)
				*flags |= CLCHT_BELOW;
		}
		return -1;
	}
	if (testx < dat->leftMargin) {
		if (flags)
			*flags |= CLCHT_INLEFTMARGIN | CLCHT_NOWHERE;
		return -1;
	}
	int hit = RowHeight::hitTest(dat, dat->yScroll + testy);
	if (hit != -1)
		hit = g_CLI.pfnGetRowByIndex(dat, hit, &hitcontact, &hitgroup);

	if (hit == -1) {
		if (flags)
			*flags |= CLCHT_NOWHERE | CLCHT_BELOWITEMS;
		return -1;
	}
	if (contact)
		*contact = hitcontact;
	if (group)
		*group = hitgroup;

	for (indent = 0; hitgroup->parent; indent++, hitgroup = hitgroup->parent)
		;

	if (!dat->bisEmbedded) {
		if (hitcontact->type == CLCIT_CONTACT) {
			if (mirror_mode == 1 || (mirror_mode == 2 && hitcontact->pExtra->dwCFlags & ECF_RTLNICK))
				return RTL_HitTest(hwnd, dat, testx, hitcontact, flags, indent, hit);
		}
		else if (hitcontact->type == CLCIT_GROUP) {
			if (cfg::dat.bGroupAlign == CLC_GROUPALIGN_RIGHT || (hitcontact->isRtl && cfg::dat.bGroupAlign == CLC_GROUPALIGN_AUTO))
				return RTL_HitTest(hwnd, dat, testx, hitcontact, flags, indent, hit);
		}
	}

	// avatar check
	if (hitcontact->type == CLCIT_CONTACT && cfg::dat.dwFlags & CLUI_FRAME_AVATARS && hitcontact->ace != nullptr && hitcontact->avatarLeft != -1) {
		if (testx > hitcontact->avatarLeft && testx < hitcontact->avatarLeft + cfg::dat.avatarSize) {
			if (flags)
				*flags |= CLCHT_ONAVATAR;
		}
	}
	if (testx < dat->leftMargin + indent * dat->groupIndent) {
		if (flags)
			*flags |= CLCHT_ONITEMINDENT;
		return hit;
	}
	checkboxWidth = 0;
	if (style & CLS_CHECKBOXES && hitcontact->type == CLCIT_CONTACT)
		checkboxWidth = dat->checkboxSize + 2;
	if (style & CLS_GROUPCHECKBOXES && hitcontact->type == CLCIT_GROUP)
		checkboxWidth = dat->checkboxSize + 2;
	if (hitcontact->type == CLCIT_INFO && hitcontact->flags & CLCIIF_CHECKBOX)
		checkboxWidth = dat->checkboxSize + 2;
	if (testx < dat->leftMargin + indent * dat->groupIndent + checkboxWidth) {
		if (flags)
			*flags |= CLCHT_ONITEMCHECK;
		return hit;
	}
	if (testx < dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace) {
		if (flags)
			*flags |= CLCHT_ONITEMICON;
		return hit;
	}

	int rightOffset = hitcontact->extraIconRightBegin;
	if (rightOffset) {
		for (i = dat->extraColumnsCount - 1; i >= 0; i--) {
			if (hitcontact->iExtraImage[i] == EMPTY_EXTRA_ICON)
				continue;

			rightOffset -= dat->extraColumnSpacing;
			if (testx > rightOffset && testx < rightOffset + dat->extraColumnSpacing) {
				if (flags)
					*flags |= (CLCHT_ONITEMEXTRA | (i << 24));
				return hit;
			}
		}
	}

	HDC hdc = GetDC(hwnd);
	if (hitcontact->type == CLCIT_GROUP)
		hFont = reinterpret_cast<HFONT>(SelectObject(hdc, dat->fontInfo[FONTID_GROUPS].hFont));
	else
		hFont = reinterpret_cast<HFONT>(SelectObject(hdc, dat->fontInfo[FONTID_CONTACTS].hFont));
	GetTextExtentPoint32(hdc, hitcontact->szText, (int)mir_wstrlen(hitcontact->szText), &textSize);
	width = textSize.cx;
	if (hitcontact->type == CLCIT_GROUP) {
		wchar_t *szCounts;
		szCounts = Clist_GetGroupCountsText(dat, hitcontact);
		if (szCounts[0]) {
			GetTextExtentPoint32(hdc, L" ", 1, &textSize);
			width += textSize.cx;
			SelectObject(hdc, dat->fontInfo[FONTID_GROUPCOUNTS].hFont);
			GetTextExtentPoint32(hdc, szCounts, (int)mir_wstrlen(szCounts), &textSize);
			width += textSize.cx;
		}
	}
	SelectObject(hdc, hFont);
	ReleaseDC(hwnd, hdc);
	if (cfg::dat.dwFlags & CLUI_FULLROWSELECT && !(GetKeyState(VK_SHIFT) & 0x8000) && testx > dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace + width + 4 + (cfg::dat.dwFlags & CLUI_FRAME_AVATARS ? cfg::dat.avatarSize : 0)) {
		if (flags)
			*flags |= CLCHT_ONITEMSPACE;
		return hit;
	}
	if (testx < dat->leftMargin + indent * dat->groupIndent + checkboxWidth + dat->iconXSpace + width + 4 + (cfg::dat.dwFlags & CLUI_FRAME_AVATARS ? cfg::dat.avatarSize : 0)) {
		if (flags)
			*flags |= CLCHT_ONITEMLABEL;
		return hit;
	}
	if (flags)
		*flags |= CLCHT_NOWHERE;
	return -1;
}

void ScrollTo(HWND hwnd, struct ClcData *dat, int desty, int noSmooth)
{
	DWORD startTick, nowTick;
	int oldy = dat->yScroll;
	RECT clRect, rcInvalidate;

	if (dat->iHotTrack != -1 && dat->yScroll != desty) {
		Clist_InvalidateItem(hwnd, dat, dat->iHotTrack);
		dat->iHotTrack = -1;
		ReleaseCapture();
	}
	GetClientRect(hwnd, &clRect);
	rcInvalidate = clRect;

	int maxy = RowHeight::getTotalHeight(dat) - clRect.bottom;
	if (desty > maxy)
		desty = maxy;
	if (desty < 0)
		desty = 0;
	if (abs(desty - dat->yScroll) < 4)
		noSmooth = 1;
	if (!noSmooth && dat->exStyle & CLS_EX_NOSMOOTHSCROLLING)
		noSmooth = 1;
	int previousy = dat->yScroll;
	if (!noSmooth) {
		startTick = GetTickCount();
		for (;;) {
			nowTick = GetTickCount();
			if (nowTick >= startTick + dat->scrollTime)
				break;
			dat->yScroll = oldy + (desty - oldy) * (int)(nowTick - startTick) / dat->scrollTime;
			if (dat->backgroundBmpUse & CLBF_SCROLL || dat->hBmpBackground == nullptr)
				ScrollWindowEx(hwnd, 0, previousy - dat->yScroll, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE);
			else
				InvalidateRect(hwnd, nullptr, FALSE);
			previousy = dat->yScroll;
			if (cfg::dat.bSkinnedScrollbar && !dat->bisEmbedded)
				CoolSB_SetScrollPos(hwnd, SB_VERT, dat->yScroll, TRUE);
			else
				SetScrollPos(hwnd, SB_VERT, dat->yScroll, TRUE);
			UpdateWindow(hwnd);
		}
	}
	dat->yScroll = desty;
	if (dat->backgroundBmpUse & CLBF_SCROLL || dat->hBmpBackground == nullptr) {
		if (!noSmooth)
			ScrollWindowEx(hwnd, 0, previousy - dat->yScroll, nullptr, nullptr, nullptr, nullptr, SW_INVALIDATE);
		else
			InvalidateRect(hwnd, nullptr, FALSE);
	}
	else
		InvalidateRect(hwnd, nullptr, FALSE);

	if (cfg::dat.bSkinnedScrollbar && !dat->bisEmbedded)
		CoolSB_SetScrollPos(hwnd, SB_VERT, dat->yScroll, TRUE);
	else
		SetScrollPos(hwnd, SB_VERT, dat->yScroll, TRUE);
	dat->bForceScroll = false;
}

void RecalcScrollBar(HWND hwnd, struct ClcData *dat)
{
	if (dat->bLockScrollbar)
		return;

	RowHeight::calcRowHeights(dat, hwnd);

	RECT clRect;
	GetClientRect(hwnd, &clRect);

	SCROLLINFO si = { 0 };
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = g_CLI.pfnGetRowTotalHeight(dat) - 1;
	si.nPage = clRect.bottom;
	si.nPos = dat->yScroll;

	if (GetWindowLongPtr(hwnd, GWL_STYLE) & CLS_CONTACTLIST) {
		if (!dat->bNoVScrollbar) {
			if (cfg::dat.bSkinnedScrollbar && !dat->bisEmbedded)
				CoolSB_SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			else
				SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		}
	}
	else {
		if (cfg::dat.bSkinnedScrollbar && !dat->bisEmbedded)
			CoolSB_SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		else
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	}
	ScrollTo(hwnd, dat, dat->yScroll, 1);

	NMCLISTCONTROL nm;
	nm.hdr.code = CLN_LISTSIZECHANGE;
	nm.hdr.hwndFrom = hwnd;
	nm.hdr.idFrom = GetDlgCtrlID(hwnd);
	nm.pt.y = si.nMax;
	SendMessage(GetParent(hwnd), WM_NOTIFY, 0, (LPARAM)& nm);
}

void SetGroupExpand(HWND hwnd, struct ClcData *dat, ClcGroup *group, int newState)
{
	int contentCount;
	int groupy;
	int newy;
	int posy;
	RECT clRect;
	NMCLISTCONTROL nm;

	if (newState == -1)
		group->expanded ^= 1;
	else {
		if (group->expanded == (newState != 0))
			return;
		group->expanded = newState != 0;
	}
	InvalidateRect(hwnd, nullptr, FALSE);
	contentCount = g_CLI.pfnGetGroupContentsCount(group, 1);

	groupy = g_CLI.pfnGetRowsPriorTo(&dat->list, group, -1);
	if (dat->selection > groupy && dat->selection < groupy + contentCount) dat->selection = groupy;
	g_CLI.pfnRecalcScrollBar(hwnd, dat);

	GetClientRect(hwnd, &clRect);
	newy = dat->yScroll;
	posy = RowHeight::getItemBottomY(dat, groupy + contentCount);
	if (posy >= newy + clRect.bottom)
		newy = posy - clRect.bottom;
	posy = RowHeight::getItemTopY(dat, groupy);
	if (newy > posy) newy = posy;
	ScrollTo(hwnd, dat, newy, 0);

	nm.hdr.code = CLN_EXPANDED;
	nm.hdr.hwndFrom = hwnd;
	nm.hdr.idFrom = GetDlgCtrlID(hwnd);
	nm.hItem = (HANDLE)group->groupId;
	nm.action = (group->expanded);
	SendMessage(GetParent(hwnd), WM_NOTIFY, 0, (LPARAM)&nm);
}

static LRESULT CALLBACK RenameEditSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_RETURN:
			Clist_EndRename((ClcData*)GetWindowLongPtr(GetParent(hwnd), 0), 1);
			return 0;
		case VK_ESCAPE:
			Clist_EndRename((ClcData*)GetWindowLongPtr(GetParent(hwnd), 0), 0);
			return 0;
		}
		break;
	
	case WM_GETDLGCODE:
		if (lParam) {
			MSG *msg = (MSG *)lParam;
			if (msg->message == WM_KEYDOWN && msg->wParam == VK_TAB)
				return 0;
			if (msg->message == WM_CHAR && msg->wParam == '\t')
				return 0;
		}
		return DLGC_WANTMESSAGE;
	
	case WM_KILLFOCUS:
		Clist_EndRename((ClcData*)GetWindowLongPtr(GetParent(hwnd), 0), 1);
		return 0;
	}
	return mir_callNextSubclass(hwnd, RenameEditSubclassProc, uMsg, wParam, lParam);
}

void BeginRenameSelection(HWND hwnd, struct ClcData *dat)
{
	ClcContact *contact;
	ClcGroup *group;
	int indent, x, y, h;
	RECT clRect;

	KillTimer(hwnd, TIMERID_RENAME);
	ReleaseCapture();
	dat->iHotTrack = -1;
	dat->selection = g_CLI.pfnGetRowByIndex(dat, dat->selection, &contact, &group);
	if (dat->selection == -1)
		return;
	if (contact->type != CLCIT_CONTACT && contact->type != CLCIT_GROUP)
		return;
	for (indent = 0; group->parent; indent++, group = group->parent) {
		;
	}
	GetClientRect(hwnd, &clRect);
	x = indent * dat->groupIndent + dat->iconXSpace - 2;
	//y = dat->selection * dat->rowHeight - dat->yScroll;
	y = RowHeight::getItemTopY(dat, dat->selection) - dat->yScroll;

	h = dat->row_heights[dat->selection];
	{
		int i;
		for (i = 0; i <= FONTID_LAST; i++)
			if (h < dat->fontInfo[i].fontHeight + 2) h = dat->fontInfo[i].fontHeight + 2;
	}

	dat->hwndRenameEdit = CreateWindowEx(0, L"RICHEDIT50W", contact->szText, WS_CHILD | WS_BORDER | ES_MULTILINE | ES_AUTOHSCROLL, x, y, clRect.right - x, h, hwnd, nullptr, g_plugin.getInst(), nullptr);
	{
		if ((contact->type == CLCIT_CONTACT && contact->pExtra->dwCFlags & ECF_RTLNICK) || (contact->type == CLCIT_GROUP && contact->isRtl)) {
			PARAFORMAT2 pf2;
			memset(&pf2, 0, sizeof(pf2));
			pf2.cbSize = sizeof(pf2);
			pf2.dwMask = PFM_RTLPARA;
			pf2.wEffects = PFE_RTLPARA;
			SetWindowText(dat->hwndRenameEdit, L"");
			SendMessage(dat->hwndRenameEdit, EM_SETPARAFORMAT, 0, (LPARAM)&pf2);
			SetWindowText(dat->hwndRenameEdit, contact->szText);
		}
	}

	//dat->hwndRenameEdit = CreateWindow(L"EDIT", contact->szText, WS_CHILD | WS_BORDER | ES_AUTOHSCROLL, x, y, clRect.right - x, dat->rowHeight, hwnd, NULL, g_plugin.getInst(), NULL);
	mir_subclassWindow(dat->hwndRenameEdit, RenameEditSubclassProc);
	SendMessage(dat->hwndRenameEdit, WM_SETFONT, (WPARAM)(contact->type == CLCIT_GROUP ? dat->fontInfo[FONTID_GROUPS].hFont : dat->fontInfo[FONTID_CONTACTS].hFont), 0);
	SendMessage(dat->hwndRenameEdit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN | EC_USEFONTINFO, 0);
	SendMessage(dat->hwndRenameEdit, EM_SETSEL, 0, (LPARAM)(-1));
	ShowWindow(dat->hwndRenameEdit, SW_SHOW);
	SetFocus(dat->hwndRenameEdit);
}

void LoadClcOptions(HWND hwnd, struct ClcData *dat, BOOL bFirst)
{
	HDC hdc = GetDC(hwnd);
	for (int i = 0; i <= FONTID_MAX; i++) {
		if (!dat->fontInfo[i].changed)
			DeleteObject(dat->fontInfo[i].hFont);

		LOGFONT lf;
		Clist_GetFontSetting(i, &lf, &dat->fontInfo[i].colour);
		lf.lfHeight = -MulDiv(lf.lfHeight, GetDeviceCaps(hdc, LOGPIXELSY), 72);

		dat->fontInfo[i].hFont = CreateFontIndirect(&lf);
		dat->fontInfo[i].changed = 0;

		HFONT holdfont = (HFONT)SelectObject(hdc, dat->fontInfo[i].hFont);
		SIZE fontSize;
		GetTextExtentPoint32(hdc, L"x", 1, &fontSize);
		SelectObject(hdc, holdfont);

		dat->fontInfo[i].fontHeight = fontSize.cy;
	}
	ReleaseDC(hwnd, hdc);

	dat->min_row_heigh = (int)db_get_b(NULL, "CLC", "RowHeight", CLCDEFAULT_ROWHEIGHT);
	dat->group_row_height = (int)db_get_b(NULL, "CLC", "GRowHeight", CLCDEFAULT_ROWHEIGHT);
	dat->row_border = 0;
	dat->rightMargin = db_get_b(NULL, "CLC", "RightMargin", CLCDEFAULT_LEFTMARGIN);
	dat->bkColour = db_get_b(NULL, "CLC", "UseWinColours", CLCDEFAULT_USEWINDOWSCOLOURS) ?
		GetSysColor(COLOR_3DFACE) : db_get_dw(NULL, "CLC", "BkColour", CLCDEFAULT_BKCOLOUR);

	coreCli.pfnLoadClcOptions(hwnd, dat, bFirst);

	if (!dat->bkChanged) {
		if (cfg::dat.hBrushCLCBk)
			DeleteObject(cfg::dat.hBrushCLCBk);
		cfg::dat.hBrushCLCBk = CreateSolidBrush(dat->bkColour);
		if (dat->hBmpBackground) {
			if (cfg::dat.hdcPic) {
				SelectObject(cfg::dat.hdcPic, cfg::dat.hbmPicOld);
				DeleteDC(cfg::dat.hdcPic);
				cfg::dat.hdcPic = nullptr;
				cfg::dat.hbmPicOld = nullptr;
			}
		}

		cfg::dat.bmpBackground = dat->hBmpBackground;
		if (cfg::dat.bmpBackground) {
			HDC hdcThis = GetDC(g_CLI.hwndContactList);
			GetObject(cfg::dat.bmpBackground, sizeof(cfg::dat.bminfoBg), &(cfg::dat.bminfoBg));
			cfg::dat.hdcPic = CreateCompatibleDC(hdcThis);
			cfg::dat.hbmPicOld = reinterpret_cast<HBITMAP>(SelectObject(cfg::dat.hdcPic, cfg::dat.bmpBackground));
			ReleaseDC(g_CLI.hwndContactList, hdcThis);
		}
	}

	if (db_get_b(NULL, "CLCExt", "EXBK_FillWallpaper", 0)) {
		char wpbuf[MAX_PATH];
		if (dat->hBmpBackground) {
			DeleteObject(dat->hBmpBackground);
			dat->hBmpBackground = nullptr;
		}

		SystemParametersInfoA(SPI_GETDESKWALLPAPER, MAX_PATH, wpbuf, 0);

		// we have a wallpaper string
		if (wpbuf[0] != 0)
			dat->hBmpBackground = reinterpret_cast<HBITMAP>(LoadImageA(nullptr, wpbuf, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));

		cfg::dat.bmpBackground = dat->hBmpBackground;
		if (cfg::dat.bmpBackground) {
			HDC hdcThis = GetDC(g_CLI.hwndContactList);
			GetObject(cfg::dat.bmpBackground, sizeof(cfg::dat.bminfoBg), &(cfg::dat.bminfoBg));
			cfg::dat.hdcPic = CreateCompatibleDC(hdcThis);
			cfg::dat.hbmPicOld = reinterpret_cast<HBITMAP>(SelectObject(cfg::dat.hdcPic, cfg::dat.bmpBackground));
			ReleaseDC(g_CLI.hwndContactList, hdcThis);
		}
	}
}
