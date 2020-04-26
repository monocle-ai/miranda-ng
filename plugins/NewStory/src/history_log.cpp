#include "stdafx.h"

class CNewStoryLogWindow : public CSrmmLogWindow
{
	HWND m_hwnd = nullptr;

public:
	CNewStoryLogWindow(CMsgDialog &pDlg) :
		CSrmmLogWindow(pDlg)
	{
	}

	void Attach() override
	{
		m_hwnd = ::CreateWindow(_T(NEWSTORYLIST_CLASS), L"NewStory", WS_VISIBLE | WS_CHILD | WS_TABSTOP, 0, 0, 300, 150, m_pDlg.GetHwnd(), 0, m_pDlg.GetInst(), 0);
	}

	void Detach() override
	{
		::DestroyWindow(m_hwnd);
	}

	//////////////////////////////////////////////////////////////////////////////////////

	bool AtBottom() override
	{
		return false;
	}

	void Clear() override
	{
	}

	HWND GetHwnd() override
	{
		return m_hwnd;
	}

	wchar_t* GetSelection() override
	{
		return nullptr;
	}

	int GetType() override
	{
		return 1;
	}

	void LogEvents(MEVENT hDbEventFirst, int count, bool bAppend) override
	{
		if (!bAppend)
			Clear();

		ADDEVENTS tmp = { m_pDlg.m_hContact, hDbEventFirst, count };
		SendMessage(m_hwnd, NSM_ADDEVENTS, (LPARAM)&tmp, 0);
	}

	void LogEvents(LOGINFO *pLog, bool) override
	{
		while (pLog) {
			SendMessage(m_hwnd, NSM_ADDCHATEVENT, (WPARAM)m_pDlg.getChat(), (LPARAM)pLog);
			pLog = pLog->prev;
		}
	}

	void Resize() override
	{
		RECT rc;
		GetWindowRect(GetDlgItem(m_pDlg.GetHwnd(), IDC_SRMM_LOG), &rc);

		POINT pt = { rc.left, rc.top };
		ScreenToClient(GetParent(m_hwnd), &pt);

		::SetWindowPos(m_hwnd, 0, pt.x, pt.y, rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_NOZORDER);
	}

	void ScrollToBottom() override
	{
		::SendMessage(m_hwnd, NSM_SEEKEND, 0, 0);
	}
};

CSrmmLogWindow* __cdecl NewStory_Stub(CMsgDialog &pDlg)
{
	return new CNewStoryLogWindow(pDlg);
}
