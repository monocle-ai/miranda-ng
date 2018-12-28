#include "stdafx.h"

#define MS_SETINVIS   "MenuEx/SetInvis"
#define MS_SETVIS     "MenuEx/SetVis"
#define MS_HIDE       "MenuEx/Hide"
#define MS_IGNORE     "MenuEx/Ignore"
#define MS_PROTO      "MenuEx/ChangeProto"
#define MS_AUTHREQ    "MenuEx/SendAuthReq"
#define MS_COPYID     "MenuEx/CopyID"
#define MS_RECVFILES  "MenuEx/RecvFiles"
#define MS_STATUSMSG  "MenuEx/CopyStatusMsg"
#define MS_COPYIP     "MenuEx/CopyIP"
#define MS_COPYMIRVER "MenuEx/CopyMirVer"
#define MS_OPENIGNORE "MenuEx/OpenIgnoreOptions"

const int vf_default = VF_VS | VF_HFL | VF_IGN | VF_CID | VF_SHOWID | VF_RECV | VF_STAT | VF_SMNAME | VF_CIDN | VF_CIP;

HGENMENU hmenuVis, hmenuOff, hmenuHide, hmenuIgnore, hmenuProto, hmenuAuthReq;
HGENMENU hmenuCopyID, hmenuRecvFiles, hmenuStatusMsg, hmenuCopyIP, hmenuCopyMirVer;
static HGENMENU hIgnoreItem[9], hProtoItem[MAX_PROTOS];
HICON hIcons[5];
BOOL bPopupService = FALSE;
PROTOACCOUNT **accs;
int protoCount;
CMPlugin g_plugin;

struct {
	char *module;
	char *name;
	wchar_t *fullName;
	char flag;
}
static const statusMsg[] = {
	{ "CList", "StatusMsg", LPGENW("Status message"), 1 },
	{ nullptr, "XStatusName", LPGENW("xStatus title"), 4 },
	{ nullptr, "XStatusMsg", LPGENW("xStatus message"), 2 },
	{ "AdvStatus", "tune/text", LPGENW("Listening to"), 8 },
	{ "AdvStatus", "activity/title", LPGENW("Activity title"), 8 },
	{ "AdvStatus", "activity/text", LPGENW("Activity text"), 8 }
};

static IconItem iconList[] = {
	{ LPGEN("Hide from list"), "miex_hidefl", IDI_ICON0 },
	{ LPGEN("Show in list"), "miex_showil", IDI_ICON8 },
	{ LPGEN("Always visible"), "miex_vis", IDI_ICON1 },
	{ LPGEN("Never visible"), "miex_invis", IDI_ICON2 },
	{ LPGEN("Copy to Account"), "miex_protocol", IDI_ICON6 },
	{ LPGEN("Ignore"), "miex_ignore", IDI_ICON7 },
	{ LPGEN("Browse Received Files"), "miex_recfiles", IDI_ICON12 },
	{ LPGEN("Copy MirVer"), "miex_copymver", IDI_ICON13 }
};

static IconItem overlayIconList[] = {
	{ LPGEN("Copy ID"), "miex_copyid", IDI_ICON3 },
	{ LPGEN("Copy Status Message"), "miex_copysm1", IDI_ICON9 },
	{ LPGEN("Copy xStatus Message"), "miex_copysm2", IDI_ICON10 },
	{ LPGEN("Copy IP"), "miex_copyip", IDI_ICON11 }
};

struct {
	wchar_t* name;
	int type;
	int icon;
}
static const ii[] = {
	{ LPGENW("All"), IGNOREEVENT_ALL, SKINICON_OTHER_FILLEDBLOB },
	{ LPGENW("Messages"), IGNOREEVENT_MESSAGE, SKINICON_EVENT_MESSAGE },
	{ LPGENW("URL"), IGNOREEVENT_URL, SKINICON_EVENT_URL },
	{ LPGENW("Files"), IGNOREEVENT_FILE, SKINICON_EVENT_FILE },
	{ LPGENW("User Online"), IGNOREEVENT_USERONLINE, SKINICON_OTHER_USERONLINE },
	{ LPGENW("Authorization"), IGNOREEVENT_AUTHORIZATION, SKINICON_AUTH_REQUEST },
	{ LPGENW("You Were Added"), IGNOREEVENT_YOUWEREADDED, SKINICON_AUTH_ADD },
	{ LPGENW("Typing Notify"), IGNOREEVENT_TYPINGNOTIFY, SKINICON_OTHER_TYPING }
};

/////////////////////////////////////////////////////////////////////////////////////////

PLUGININFOEX pluginInfoEx = {
	sizeof(PLUGININFOEX),
	__PLUGIN_NAME,
	PLUGIN_MAKE_VERSION(__MAJOR_VERSION, __MINOR_VERSION, __RELEASE_NUM, __BUILD_NUM),
	__DESCRIPTION,
	__AUTHOR,
	__COPYRIGHT,
	__AUTHORWEB,
	UNICODE_AWARE,
	// {B1902A52-9114-4D7E-AC2E-B3A52E01D574}
	{ 0xb1902a52, 0x9114, 0x4d7e, { 0xac, 0x2e, 0xb3, 0xa5, 0x2e, 0x01, 0xd5, 0x74 } }
};

CMPlugin::CMPlugin() :
	PLUGIN<CMPlugin>(MODULENAME, pluginInfoEx)
{}

/////////////////////////////////////////////////////////////////////////////////////////

struct ModSetLinkLinkItem { // code from dbe++ plugin by Bio
	char *name;
	BYTE *next; //struct ModSetLinkLinkItem
};

struct ModuleSettingLL {
	struct ModSetLinkLinkItem *first;
	struct ModSetLinkLinkItem *last;
};

static int GetSetting(MCONTACT hContact, const char *szModule, const char *szSetting, DBVARIANT *dbv)
{
	return db_get_s(hContact, szModule, szSetting, dbv, 0);
}

static int enumModulesSettingsProc(const char *szName, void *lParam)
{
	ModuleSettingLL *msll = (ModuleSettingLL *)lParam;
	if (!msll->first)
	{
		msll->first = (struct ModSetLinkLinkItem *)malloc(sizeof(struct ModSetLinkLinkItem));
		if (!msll->first) return 1;
		msll->first->name = _strdup(szName);
		msll->first->next = nullptr;
		msll->last = msll->first;
	}
	else
	{
		struct ModSetLinkLinkItem *item = (struct ModSetLinkLinkItem *)malloc(sizeof(struct ModSetLinkLinkItem));
		if (!item) return 1;
		msll->last->next = (BYTE*)item;
		msll->last = (struct ModSetLinkLinkItem *)item;
		item->name = _strdup(szName);
		item->next = nullptr;
	}
	return 0;
}

static void FreeModuleSettingLL(ModuleSettingLL* msll)
{
	if (msll)
	{
		struct ModSetLinkLinkItem *item = msll->first;
		struct ModSetLinkLinkItem *temp;

		while (item)
		{
			if (item->name)
			{
				free(item->name);
				item->name = nullptr;
			}
			temp = item;
			item = (struct ModSetLinkLinkItem *)item->next;
			if (temp)
			{
				free(temp);
				temp = nullptr;
			}
		}

		msll->first = nullptr;
		msll->last = nullptr;
	}
}

static void RenameDbProto(MCONTACT hContact, MCONTACT hContactNew, char* oldName, char* newName, int delOld)
{
	// enum all setting the contact has for the module
	ModuleSettingLL settinglist = { nullptr, nullptr };
	db_enum_settings(hContact, enumModulesSettingsProc, oldName, &settinglist);

	for (ModSetLinkLinkItem *setting = settinglist.first; setting; setting = (ModSetLinkLinkItem *)setting->next) {
		DBVARIANT dbv;
		if (!GetSetting(hContact, oldName, setting->name, &dbv)) {
			switch (dbv.type) {
			case DBVT_BYTE:
				db_set_b(hContactNew, newName, setting->name, dbv.bVal);
				break;
			case DBVT_WORD:
				db_set_w(hContactNew, newName, setting->name, dbv.wVal);
				break;
			case DBVT_DWORD:
				db_set_dw(hContactNew, newName, setting->name, dbv.dVal);
				break;
			case DBVT_ASCIIZ:
				db_set_s(hContactNew, newName, setting->name, dbv.pszVal);
				break;
			case DBVT_UTF8:
				db_set_utf(hContactNew, newName, setting->name, dbv.pszVal);
				break;
			case DBVT_BLOB:
				db_set_blob(hContactNew, newName, setting->name, dbv.pbVal, dbv.cpbVal);
				break;
			}
			if (delOld)
				db_unset(hContact, oldName, setting->name);
		}
		db_free(&dbv);
	}
	FreeModuleSettingLL(&settinglist);
} // end code from dbe++

static void ShowPopup(const wchar_t *pwszText, MCONTACT hContact)
{
	if (!pwszText) return;

	POPUPDATAT ppd = {};
	ppd.lchIcon = Skin_LoadIcon(SKINICON_OTHER_MIRANDA);
	ppd.lchContact = hContact;
	wcsncpy(ppd.lptzContactName, Clist_GetContactDisplayName(hContact), MAX_CONTACTNAME - 1);
	wcsncpy(ppd.lptzText, pwszText, MAX_SECONDLINE - 1);
	ppd.iSeconds = -1;
	PUAddPopupT(&ppd);
}

BOOL DirectoryExists(MCONTACT hContact)
{
	char path[MAX_PATH];
	CallService(MS_FILE_GETRECEIVEDFILESFOLDER, hContact, (LPARAM)&path);
	DWORD attr = GetFileAttributesA(path);
	return (attr != -1) && (attr&FILE_ATTRIBUTE_DIRECTORY);
}

void CopyToClipboard(const wchar_t *pwszMsg)
{
	if (pwszMsg == nullptr)
		return;

	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, (mir_wstrlen(pwszMsg) + 1) * sizeof(wchar_t));
	LPWSTR lptstrCopy = (LPWSTR)GlobalLock(hglbCopy);
	mir_wstrcpy(lptstrCopy, pwszMsg);
	GlobalUnlock(hglbCopy);

	if (OpenClipboard(nullptr) == NULL)
		return;

	EmptyClipboard();

	SetClipboardData(CF_UNICODETEXT, hglbCopy);

	CloseClipboard();
}

BOOL isMetaContact(MCONTACT hContact)
{
	char *proto = GetContactProto(hContact);
	if (mir_strcmp(proto, META_PROTO) == 0)
		return TRUE;

	return FALSE;
}

void GetID(MCONTACT hContact, LPSTR szProto, LPWSTR szID, size_t dwIDSize)
{
	DBVARIANT dbv_uniqueid;
	LPCSTR uID = Proto_GetUniqueId(szProto);

	szID[0] = 0;
	if (uID && db_get(hContact, szProto, uID, &dbv_uniqueid) == 0) {
		switch (dbv_uniqueid.type) {
		case DBVT_DWORD:
			mir_snwprintf(szID, dwIDSize, L"%u", dbv_uniqueid.dVal);
			break;
		case DBVT_WORD:
			mir_snwprintf(szID, dwIDSize, L"%u", dbv_uniqueid.wVal);
			break;
		case DBVT_ASCIIZ:
			wcsncpy_s(szID, dwIDSize, _A2T(dbv_uniqueid.pszVal), _TRUNCATE);
			break;
		case DBVT_WCHAR:
			wcsncpy_s(szID, dwIDSize, dbv_uniqueid.pwszVal, _TRUNCATE);
			break;
		case DBVT_BLOB:
			CMStringW tmp(' ', dbv_uniqueid.cpbVal * 2 + 1);
			bin2hexW(dbv_uniqueid.pbVal, dbv_uniqueid.cpbVal, tmp.GetBuffer());
			wcsncpy_s(szID, dwIDSize, tmp, _TRUNCATE);
			break;
		}

		db_free(&dbv_uniqueid);
	}
}

int StatusMsgExists(MCONTACT hContact)
{
	char par[32];
	BOOL ret = 0;

	LPSTR module = GetContactProto(hContact);
	if (!module) return 0;

	for (auto &it : statusMsg) {
		if (it.flag & 8)
			mir_snprintf(par, "%s/%s", module, it.name);
		else
			strncpy(par, it.name, _countof(par) - 1);

		LPSTR msg = db_get_sa(hContact, (it.module) ? it.module : module, par);
		if (msg) {
			if (mir_strlen(msg))
				ret |= it.flag;
			mir_free(msg);
		}
	}
	return ret;
}

BOOL IPExists(MCONTACT hContact)
{
	LPSTR szProto = GetContactProto(hContact);
	if (!szProto) return 0;

	DWORD mIP = db_get_dw(hContact, szProto, "IP", 0);
	DWORD rIP = db_get_dw(hContact, szProto, "RealIP", 0);

	return (mIP != 0 || rIP != 0);
}

BOOL MirVerExists(MCONTACT hContact)
{
	LPSTR szProto = GetContactProto(hContact);
	if (!szProto)
		return 0;

	ptrW msg(db_get_wsa(hContact, szProto, "MirVer"));
	return mir_wstrlen(msg) != 0;
}

LPWSTR getMirVer(MCONTACT hContact)
{
	LPSTR szProto = GetContactProto(hContact);
	if (!szProto) return nullptr;

	LPWSTR msg = db_get_wsa(hContact, szProto, "MirVer");
	if (msg) {
		if (msg[0] != 0)
			return msg;
		mir_free(msg);
	}

	return nullptr;
}

static LRESULT CALLBACK AuthReqEditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_CHAR:
		if (wParam == '\n' && CTRL_IS_PRESSED) { // ctrl + ENTER
			PostMessage(GetParent(hwnd), WM_COMMAND, IDOK, 0);
			return 0;
		}
		if (wParam == 1 && CTRL_IS_PRESSED) { // ctrl + a
			SendMessage(hwnd, EM_SETSEL, 0, -1);
			return 0;
		}
		break;
	case WM_SETFOCUS:
		SendMessage(hwnd, EM_SETSEL, 0, -1);
		break;
	}
	return mir_callNextSubclass(hwnd, AuthReqEditSubclassProc, msg, wParam, lParam);
}

static INT_PTR CALLBACK AuthReqWndProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	static MCONTACT hcontact;

	switch (msg) {
	case WM_INITDIALOG:
		TranslateDialogDefault(hdlg);
		mir_subclassWindow(GetDlgItem(hdlg, IDC_REASON), AuthReqEditSubclassProc);
		SendDlgItemMessage(hdlg, IDC_REASON, EM_LIMITTEXT, (WPARAM)255, 0);
		SetDlgItemText(hdlg, IDC_REASON, TranslateT("Please authorize me to add you to my contact list."));
		hcontact = (MCONTACT)lparam;
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam)) {
		case IDOK:
			wchar_t tszReason[256];
			GetDlgItemText(hdlg, IDC_REASON, tszReason, _countof(tszReason));
			ProtoChainSend(hcontact, PSS_AUTHREQUEST, 0, (LPARAM)tszReason);
			__fallthrough;

		case IDCANCEL:
			DestroyWindow(hdlg);
			break;
		}
		break;
	}

	return 0;
}

static BOOL isProtoOnline(char *szProto)
{
	int protoStatus = Proto_GetStatus(szProto);
	return (protoStatus > ID_STATUS_OFFLINE && protoStatus < ID_STATUS_IDLE);
}

static INT_PTR onSendAuthRequest(WPARAM wparam, LPARAM)
{
	MCONTACT hContact = (MCONTACT)wparam;
	char *szProto = GetContactProto(hContact);

	DWORD flags = CallProtoService(szProto, PS_GETCAPS, PFLAGNUM_4, 0);
	if (flags&PF4_NOCUSTOMAUTH)
		ProtoChainSend(hContact, PSS_AUTHREQUEST, 0, (LPARAM)L"");
	else
		CreateDialogParam(g_plugin.getInst(), MAKEINTRESOURCE(IDD_AUTHREQ), g_clistApi.hwndContactList, AuthReqWndProc, (LPARAM)hContact);

	return 0;
}

// set the invisible-flag in db
static INT_PTR onSetInvis(WPARAM wparam, LPARAM)
{
	MCONTACT hContact = (MCONTACT)wparam;
	ProtoChainSend(hContact, PSS_SETAPPARENTMODE, (db_get_w(hContact, GetContactProto(hContact), "ApparentMode", 0) == ID_STATUS_OFFLINE) ? 0 : ID_STATUS_OFFLINE, 0);
	return 0;
}

// set visible-flag in db
static INT_PTR onSetVis(WPARAM wparam, LPARAM)
{
	MCONTACT hContact = (MCONTACT)wparam;
	ProtoChainSend(hContact, PSS_SETAPPARENTMODE, (db_get_w(hContact, GetContactProto(hContact), "ApparentMode", 0) == ID_STATUS_ONLINE) ? 0 : ID_STATUS_ONLINE, 0);
	return 0;
}

static INT_PTR onHide(WPARAM wparam, LPARAM)
{
	MCONTACT hContact = (MCONTACT)wparam;
	db_set_b(hContact, "CList", "Hidden", (BYTE)!db_get_b(hContact, "CList", "Hidden", 0));
	return 0;
}

// following 4 functions should be self-explanatory
static void ModifyVisibleSet(int mode, BOOL alpha)
{
	Menu_ModifyItem(hmenuVis, nullptr, (mode) ? hIcons[1] : (alpha ? hIcons[3] : Skin_LoadIcon(SKINICON_OTHER_SMALLDOT)));
}

static void ModifyInvisSet(int mode, BOOL alpha)
{
	Menu_ModifyItem(hmenuOff, nullptr, (mode) ? hIcons[2] : (alpha ? hIcons[4] : Skin_LoadIcon(SKINICON_OTHER_SMALLDOT)));
}

static void ModifyCopyID(MCONTACT hContact, BOOL bShowID, BOOL bTrimID)
{
	if (isMetaContact(hContact)) {
		MCONTACT hC = db_mc_getMostOnline(hContact);
		if (!hContact) hC = db_mc_getDefault(hContact);
		hContact = hC;
	}

	LPSTR szProto = GetContactProto(hContact);
	if (!szProto) {
		Menu_ShowItem(hmenuCopyID, false);
		return;
	}

	HICON hIconCID = (HICON)CallProtoService(szProto, PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
	{
		HICON hIcon = BindOverlayIcon(hIconCID, "miex_copyid");
		DestroyIcon(hIconCID);
		hIconCID = hIcon;
	}

	wchar_t buffer[256], szID[256];
	GetID(hContact, szProto, szID, _countof(szID));
	if (szID[0]) {
		if (bShowID) {
			if (bTrimID && (mir_wstrlen(szID) > MAX_IDLEN)) {
				szID[MAX_IDLEN - 2] = szID[MAX_IDLEN - 1] = szID[MAX_IDLEN] = '.';
				szID[MAX_IDLEN + 1] = 0;
			}

			mir_snwprintf(buffer, L"%s [%s]", TranslateT("Copy ID"), szID);
			Menu_ModifyItem(hmenuCopyID, buffer, hIconCID);
		}
		else Menu_ModifyItem(hmenuCopyID, LPGENW("Copy ID"), hIconCID);
	}
	else Menu_ShowItem(hmenuCopyID, false);

	DestroyIcon(hIconCID);
}

static void ModifyStatusMsg(MCONTACT hContact)
{
	LPSTR szProto = GetContactProto(hContact);
	if (!szProto) {
		Menu_ShowItem(hmenuStatusMsg, false);
		return;
	}

	HICON hIconSMsg = (HICON)CallProtoService(szProto, PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
	{
		HICON hIcon = BindOverlayIcon(hIconSMsg, (StatusMsgExists(hContact) & 2) ? "miex_copysm2" : "miex_copysm1");
		DestroyIcon(hIconSMsg);
		hIconSMsg = hIcon;
	}

	Menu_ModifyItem(hmenuStatusMsg, nullptr, hIconSMsg);
	DestroyIcon(hIconSMsg);
}

static void ModifyCopyIP(MCONTACT hContact)
{
	LPSTR szProto = GetContactProto(hContact);
	if (!szProto) {
		Menu_ShowItem(hmenuCopyIP, false);
		return;
	}

	HICON hIconCIP = (HICON)CallProtoService(szProto, PS_LOADICON, PLI_PROTOCOL | PLIF_SMALL, 0);
	{
		HICON hIcon = BindOverlayIcon(hIconCIP, "miex_copyip");
		DestroyIcon(hIconCIP);
		hIconCIP = hIcon;
	}

	Menu_ModifyItem(hmenuCopyIP, nullptr, hIconCIP);
	DestroyIcon(hIconCIP);
}

static void ModifyCopyMirVer(MCONTACT hContact)
{
	HICON hMenuIcon = nullptr;
	if (ServiceExists(MS_FP_GETCLIENTICONT)) {
		LPWSTR msg = getMirVer(hContact);
		if (msg) {
			hMenuIcon = Finger_GetClientIcon(msg, 1);
			mir_free(msg);
		}
	}
	if (hMenuIcon == nullptr)
		hMenuIcon = hIcons[0];
	Menu_ModifyItem(hmenuCopyMirVer, nullptr, hMenuIcon);
}

static INT_PTR onCopyID(WPARAM hContact, LPARAM)
{
	if (isMetaContact(hContact)) {
		MCONTACT hC = db_mc_getMostOnline(hContact);
		if (!hContact)
			hC = db_mc_getDefault(hContact);
		hContact = hC;
	}

	LPSTR szProto = GetContactProto(hContact);
	if (szProto == nullptr)
		return 0;

	wchar_t szID[256], buffer[256];
	GetID(hContact, szProto, szID, _countof(szID));

	if (g_plugin.getDword("flags", vf_default) & VF_CIDN) {
		PROTOACCOUNT *pa = Proto_GetAccount(szProto);
		if (!pa->bOldProto)
			mir_snwprintf(buffer, L"%s: %s", pa->tszAccountName, szID);
		else
			mir_snwprintf(buffer, L"%S: %s", szProto, szID);
	}
	else wcsncpy_s(buffer, szID, _TRUNCATE);

	CopyToClipboard(buffer);
	if (CTRL_IS_PRESSED && bPopupService)
		ShowPopup(buffer, hContact);

	return 0;
}

static INT_PTR onCopyStatusMsg(WPARAM hContact, LPARAM)
{
	DWORD flags = g_plugin.getDword("flags", vf_default);

	LPSTR module = GetContactProto(hContact);
	if (!module)
		return 0;

	CMStringW wszBuffer;
	for (auto &it : statusMsg) {
		char par[32];
		if (it.flag & 8)
			mir_snprintf(par, "%s/%s", module, it.name);
		else
			strncpy(par, it.name, _countof(par) - 1);

		ptrW msg(db_get_wsa(hContact, (it.module) ? it.module : module, par));
		if (msg) {
			if (wcslen(msg)) {
				if (flags & VF_SMNAME) {
					wszBuffer.Append(TranslateW(it.fullName));
					wszBuffer.Append(L": ");
				}
				wszBuffer.Append(msg);
				wszBuffer.Append(L"\r\n");
			}
		}
	}

	CopyToClipboard(wszBuffer);
	if (CTRL_IS_PRESSED && bPopupService)
		ShowPopup(wszBuffer, hContact);

	return 0;
}

static INT_PTR onCopyIP(WPARAM hContact, LPARAM)
{
	char *szProto = GetContactProto(hContact);

	CMStringW wszBuffer;
	DWORD mIP = db_get_dw(hContact, szProto, "IP", 0);
	if (mIP)
		wszBuffer.AppendFormat(L"External IP: %d.%d.%d.%d\r\n", mIP >> 24, (mIP >> 16) & 0xFF, (mIP >> 8) & 0xFF, mIP & 0xFF);

	DWORD rIP = db_get_dw(hContact, szProto, "RealIP", 0);
	if (rIP)
		wszBuffer.AppendFormat(L"Internal IP: %d.%d.%d.%d\r\n", rIP >> 24, (rIP >> 16) & 0xFF, (rIP >> 8) & 0xFF, rIP & 0xFF);

	CopyToClipboard(wszBuffer);
	if (CTRL_IS_PRESSED && bPopupService)
		ShowPopup(wszBuffer, hContact);

	return 0;
}

static INT_PTR onCopyMirVer(WPARAM hContact, LPARAM)
{
	LPWSTR msg = getMirVer(hContact);
	if (msg) {
		CopyToClipboard(msg);
		if (CTRL_IS_PRESSED && bPopupService)
			ShowPopup(msg, hContact);

		mir_free(msg);
	}
	return 0;
}

static INT_PTR OpenIgnoreOptions(WPARAM, LPARAM)
{
	g_plugin.openOptions(L"Contacts", L"Ignore");
	return 0;
}

static INT_PTR onRecvFiles(WPARAM hContact, LPARAM)
{
	char path[MAX_PATH];
	CallService(MS_FILE_GETRECEIVEDFILESFOLDER, hContact, (LPARAM)&path);
	ShellExecuteA(nullptr, "open", path, nullptr, nullptr, SW_SHOW);
	return 0;
}

static INT_PTR onChangeProto(WPARAM hContact, LPARAM lparam)
{
	char *szOldProto = GetContactProto(hContact);
	char *szNewProto = (char *)lparam;
	if (!mir_strcmp(szOldProto, szNewProto))
		return 0;

	MCONTACT hContactNew;
	if (CTRL_IS_PRESSED) {
		hContactNew = hContact;
		RenameDbProto(hContact, hContactNew, szOldProto, szNewProto, 1);
		Proto_RemoveFromContact(hContact, szOldProto);
		Proto_AddToContact(hContactNew, szNewProto);
	}
	else {
		hContactNew = db_add_contact();
		if (hContactNew) {
			Proto_AddToContact(hContactNew, szNewProto);
			RenameDbProto(hContact, hContactNew, szOldProto, szNewProto, 0);
			RenameDbProto(hContact, hContactNew, "CList", "CList", 0);
		}
		else
			return 0;
	}
	if (MessageBox(nullptr, TranslateT("Do you want to send authorization request\nto new contact?"), L"Miranda NG", MB_OKCANCEL | MB_SETFOREGROUND | MB_TOPMOST) == IDOK)
		onSendAuthRequest((WPARAM)hContactNew, 0);

	return 0;
}

static int isIgnored(MCONTACT hContact, int type)
{
	if (type != IGNOREEVENT_ALL)
		return CallService(MS_IGNORE_ISIGNORED, hContact, (LPARAM)type);

	int i = 0, all = 0;
	for (i = 1; i < _countof(ii); i++)
		if (isIgnored(hContact, ii[i].type))
			all++;

	return (all == _countof(ii) - 1) ? 1 : 0; // ignoring all or not
}

static INT_PTR onIgnore(WPARAM wparam, LPARAM lparam)
{
	if (g_plugin.getByte("ignorehide", 0) && (lparam == IGNOREEVENT_ALL))
		db_set_b((MCONTACT)wparam, "CList", "Hidden", (isIgnored((MCONTACT)wparam, lparam) ? (byte)0 : (byte)1));

	CallService(isIgnored((MCONTACT)wparam, lparam) ? MS_IGNORE_UNIGNORE : MS_IGNORE_IGNORE, wparam, lparam);
	return 0;
}

static HGENMENU AddSubmenuItem(HGENMENU hRoot, wchar_t* name, HICON icon, DWORD flag, char* service, int pos, INT_PTR param)
{
	CMenuItem mi(&g_plugin);
	mi.root = hRoot;
	mi.position = pos;
	mi.name.w = name;
	mi.hIcolibItem = icon;
	mi.flags = CMIF_UNICODE | CMIF_UNMOVABLE | flag;
	mi.pszService = service;

	HGENMENU res = Menu_AddContactMenuItem(&mi);
	Menu_ConfigureItem(res, MCI_OPT_EXECPARAM, param);
	return res;
}

static void ModifySubmenuItem(HGENMENU hItem, int checked, int hidden)
{
	int flags = 0;
	if (checked)
		flags |= CMIF_CHECKED;
	if (hidden)
		flags |= CMIF_HIDDEN;
	Menu_ModifyItem(hItem, nullptr, INVALID_HANDLE_VALUE, flags);
}

// called when the contact-menu is built
static int BuildMenu(WPARAM wparam, LPARAM)
{
	DWORD flags = g_plugin.getDword("flags", vf_default);
	int j = 0, all = 0, hide = 0;
	BOOL bIsOnline = FALSE, bShowAll = CTRL_IS_PRESSED;
	MCONTACT hContact = (MCONTACT)wparam;
	char* pszProto = GetContactProto(hContact);
	PROTOACCOUNT *pa = Proto_GetAccount(pszProto);

	bIsOnline = isProtoOnline(pszProto);

	bool bEnabled = bShowAll || (flags & VF_VS);
	Menu_ShowItem(hmenuVis, bEnabled);
	Menu_ShowItem(hmenuOff, bEnabled);

	bEnabled = bShowAll || (flags & VF_HFL);
	Menu_ShowItem(hmenuHide, bEnabled);
	if (bEnabled) {
		BYTE bHidden = db_get_b(hContact, "CList", "Hidden", 0);
		if (bHidden)
			Menu_ModifyItem(hmenuHide, LPGENW("Show in list"), IcoLib_GetIcon("miex_showil"));
		else
			Menu_ModifyItem(hmenuHide, LPGENW("Hide from list"), IcoLib_GetIcon("miex_hidefl"));
	}

	bEnabled = bShowAll || (flags & VF_IGN);
	Menu_ShowItem(hmenuIgnore, bEnabled);
	if (bEnabled) {
		for (int i = 1; i < _countof(ii); i++) {
			int check = isIgnored(hContact, ii[i].type);
			if (check)
				all++;

			ModifySubmenuItem(hIgnoreItem[i], check, 0);

			if (all == _countof(ii) - 1) // ignor all
				check = 1;
			else
				check = 0;
			ModifySubmenuItem(hIgnoreItem[0], check, 0);
		}
	}

	if (pa && (bShowAll || (flags & VF_PROTO))) {
		for (int i = 0; i < protoCount; i++) {
			if ((!accs[i]->bIsEnabled) || (mir_strcmp(pa->szProtoName, accs[i]->szProtoName)))
				hide = 1;
			else {
				hide = 0;
				j++;
			}

			int check = Proto_IsProtoOnContact(wparam, accs[i]->szModuleName);
			ModifySubmenuItem(hProtoItem[i], check, hide);
		}
		Menu_ShowItem(hmenuProto, j > 1);
	}
	else Menu_ShowItem(hmenuProto, false);

	Menu_ShowItem(hmenuAuthReq, (bShowAll || (flags & VF_REQ)) && bIsOnline && pa->IsEnabled());

	bEnabled = bShowAll || (flags & VF_CID);
	Menu_ShowItem(hmenuCopyID, bEnabled);
	if (bEnabled)
		ModifyCopyID(hContact, flags & VF_SHOWID, flags & VF_TRIMID);

	Menu_ShowItem(hmenuRecvFiles, (bShowAll || (flags & VF_RECV)) && DirectoryExists(hContact));

	bEnabled = (bShowAll || (flags & VF_STAT)) && StatusMsgExists(hContact);
	Menu_ShowItem(hmenuStatusMsg, bEnabled);
	if (bEnabled)
		ModifyStatusMsg(hContact);

	bEnabled = (bShowAll || (flags & VF_CIP)) && IPExists(hContact);
	Menu_ShowItem(hmenuCopyIP, bEnabled);
	if (bEnabled)
		ModifyCopyIP(hContact);

	bEnabled = (bShowAll || (flags & VF_CMV)) && MirVerExists(hContact);
	Menu_ShowItem(hmenuCopyMirVer, bEnabled);
	if (bEnabled)
		ModifyCopyMirVer(hContact);

	if ((bShowAll || (flags & VF_VS)) && pszProto) {
		INT_PTR caps = CallProtoService(pszProto, PS_GETCAPS, PFLAGNUM_1, 0);
		int apparent = db_get_w(hContact, GetContactProto(hContact), "ApparentMode", 0);

		Menu_ShowItem(hmenuVis, (caps & PF1_VISLIST) != 0);
		if (caps & PF1_VISLIST)
			ModifyVisibleSet(apparent == ID_STATUS_ONLINE, flags & VF_SAI);

		Menu_ShowItem(hmenuOff, (caps & PF1_INVISLIST) != 0);
		if (caps & PF1_INVISLIST)
			ModifyInvisSet(apparent == ID_STATUS_OFFLINE, flags & VF_SAI);
	}
	return 0;
}

static int EnumProtoSubmenu(WPARAM, LPARAM)
{
	int pos = 1000;
	if (protoCount) // remove old items
	{
		for (int i = 0; i < protoCount; i++)
		{
			if (hProtoItem[i])
			{
				Menu_RemoveItem(hProtoItem[i]);
				hProtoItem[i] = nullptr;
			}
		}
	}
	Proto_EnumAccounts(&protoCount, &accs);
	if (protoCount > MAX_PROTOS)
		protoCount = MAX_PROTOS;
	for (int i = 0; i < protoCount; i++)
	{
		hProtoItem[i] = AddSubmenuItem(hmenuProto, accs[i]->tszAccountName,
			Skin_LoadProtoIcon(accs[i]->szModuleName, ID_STATUS_ONLINE), CMIF_SYSTEM | CMIF_KEEPUNTRANSLATED,
			MS_PROTO, pos++, (INT_PTR)accs[i]->szModuleName);
	}
	return 0;
}

// Tabsrmm toolbar support
static int TabsrmmButtonPressed(WPARAM wParam, LPARAM lParam)
{
	CustomButtonClickData *cbcd = (CustomButtonClickData *)lParam;

	if (!mir_strcmp(cbcd->pszModule, MODULENAME) && cbcd->dwButtonId == 0)
		onRecvFiles(wParam, 0);

	return 0;
}

static int TabsrmmButtonsInit(WPARAM, LPARAM)
{
	BBButton bbd = {};
	bbd.pszModuleName = MODULENAME;
	bbd.dwDefPos = 1000;
	bbd.pwszTooltip = LPGENW("Browse Received Files");
	bbd.bbbFlags = BBBF_CANBEHIDDEN;
	bbd.hIcon = IcoLib_GetIconHandle("miex_recfiles");
	Srmm_AddButton(&bbd, &g_plugin);
	return 0;
}

static void TabsrmmButtonsModify(MCONTACT hContact)
{
	if (!DirectoryExists(hContact))
	{
		BBButton bbd = {};
		bbd.pszModuleName = MODULENAME;
		bbd.bbbFlags = BBSF_DISABLED | BBSF_HIDDEN;
		Srmm_SetButtonState(hContact, &bbd);
	}
}

static int ContactWindowOpen(WPARAM, LPARAM lParam)
{
	MessageWindowEventData *MWeventdata = (MessageWindowEventData*)lParam;

	if (MWeventdata->uType == MSG_WINDOW_EVT_OPENING && MWeventdata->hContact)
		TabsrmmButtonsModify(MWeventdata->hContact);

	return 0;
}

static int ModuleLoad(WPARAM, LPARAM)
{
	bPopupService = ServiceExists(MS_POPUP_ADDPOPUPT);
	return 0;
}

// called when all modules are loaded
static int PluginInit(WPARAM, LPARAM)
{
	int pos = 1000;

	ModuleLoad(0, 0);

	CMenuItem mi(&g_plugin);
	mi.flags = CMIF_UNICODE;

	SET_UID(mi, 0x2616aa3f, 0x535a, 0x464c, 0xbd, 0x26, 0x1b, 0x15, 0xbe, 0xfa, 0x1f, 0xf);
	mi.position = 120000;
	mi.name.w = LPGENW("Always visible");
	mi.pszService = MS_SETVIS;
	hmenuVis = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x7d93de78, 0xb1c, 0x4c51, 0x8c, 0x88, 0x33, 0x72, 0x12, 0xb5, 0xb8, 0xe7);
	mi.position++;
	mi.name.w = LPGENW("Never visible");
	mi.pszService = MS_SETINVIS;
	hmenuOff = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x724f6ac0, 0x7f69, 0x407d, 0x85, 0x98, 0x9c, 0x80, 0x32, 0xdb, 0x66, 0x2d);
	mi.position++;
	mi.name.w = LPGENW("Hide from list");
	mi.pszService = MS_HIDE;
	hmenuHide = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0xe09c04d4, 0xc6b1, 0x4048, 0x98, 0xd6, 0xbe, 0x11, 0xf6, 0x91, 0x15, 0xba);
	mi.position++;
	mi.name.w = LPGENW("Ignore");
	mi.pszService = nullptr;
	mi.hIcolibItem = IcoLib_GetIcon("miex_ignore");
	hmenuIgnore = Menu_AddContactMenuItem(&mi);

	hIgnoreItem[0] = AddSubmenuItem(hmenuIgnore, ii[0].name, Skin_LoadIcon(ii[0].icon), 0, MS_IGNORE, pos, ii[0].type);
	pos += 100000; // insert separator
	for (int i = 1; i < _countof(ii); i++)
		hIgnoreItem[i] = AddSubmenuItem(hmenuIgnore, ii[i].name, Skin_LoadIcon(ii[i].icon), 0, MS_IGNORE, pos++, ii[i].type);

	AddSubmenuItem(hmenuIgnore, LPGENW("Open ignore settings"), IcoLib_GetIcon("miex_ignore"), 0, MS_OPENIGNORE, pos, 0);

	pos += 100000; // insert separator

	SET_UID(mi, 0x820f4637, 0xbcc4, 0x46b7, 0x9c, 0x67, 0xf9, 0x69, 0xed, 0xc2, 0x46, 0xa2);
	mi.position++;
	mi.name.w = LPGENW("Copy to Account");
	mi.pszService = MS_PROTO;
	mi.hIcolibItem = IcoLib_GetIcon("miex_protocol");
	hmenuProto = Menu_AddContactMenuItem(&mi);

	EnumProtoSubmenu(0, 0);

	mi.flags = CMIF_UNICODE;

	SET_UID(mi, 0x332c5564, 0x6283, 0x43ff, 0xa2, 0xfc, 0x58, 0x29, 0x27, 0x83, 0xea, 0x1a);
	mi.position++;
	mi.name.w = LPGENW("Request authorization");
	mi.pszService = MS_AUTHREQ;
	mi.hIcolibItem = Skin_LoadIcon(SKINICON_AUTH_REQUEST);
	hmenuAuthReq = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x92826bf6, 0xd44c, 0x4dc2, 0xb2, 0xdd, 0xfe, 0xaf, 0x9b, 0x86, 0xe1, 0x53);
	mi.position++;
	mi.name.w = LPGENW("Copy ID");
	mi.pszService = MS_COPYID;
	hmenuCopyID = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x54d45bf1, 0x1c6d, 0x49c9, 0xbd, 0xde, 0x63, 0xb2, 0x3d, 0xb0, 0x89, 0xbf);
	mi.position++;
	mi.name.w = LPGENW("Browse Received Files");
	mi.pszService = MS_RECVFILES;
	mi.hIcolibItem = IcoLib_GetIcon("miex_recfiles");
	hmenuRecvFiles = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0xf750f36b, 0x284f, 0x4841, 0x83, 0x18, 0xc7, 0x10, 0x4, 0x73, 0xea, 0x22);
	mi.position++;
	mi.name.w = LPGENW("Copy Status Message");
	mi.pszService = MS_STATUSMSG;
	mi.hIcolibItem = nullptr;
	hmenuStatusMsg = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x3847bfcd, 0xfcd5, 0x4435, 0xa6, 0x54, 0x2e, 0x9, 0xc5, 0xba, 0xcf, 0x71);
	mi.position++;
	mi.name.w = LPGENW("Copy IP");
	mi.pszService = MS_COPYIP;
	hmenuCopyIP = Menu_AddContactMenuItem(&mi);

	SET_UID(mi, 0x8c6ad48, 0x882d, 0x47ed, 0xa6, 0x6e, 0xba, 0x26, 0xa3, 0x50, 0x17, 0x82);
	mi.position++;
	mi.name.w = LPGENW("Copy MirVer");
	mi.pszService = MS_COPYMIRVER;
	hmenuCopyMirVer = Menu_AddContactMenuItem(&mi);

	hIcons[0] = IcoLib_GetIcon("miex_copymver");
	hIcons[1] = IcoLib_GetIcon("miex_vis");
	hIcons[2] = IcoLib_GetIcon("miex_invis");
	hIcons[3] = MakeHalfAlphaIcon(hIcons[1]);
	hIcons[4] = MakeHalfAlphaIcon(hIcons[2]);

	HookTemporaryEvent(ME_MSG_TOOLBARLOADED, TabsrmmButtonsInit);
	HookEvent(ME_MSG_BUTTONPRESSED, TabsrmmButtonPressed);
	HookEvent(ME_MSG_WINDOWEVENT, ContactWindowOpen);
	return 0;
}

int CMPlugin::Load()
{
	g_plugin.registerIcon(LPGEN("MenuItemEx"), iconList);
	g_plugin.registerIcon(LPGEN("MenuItemEx"), overlayIconList);

	CreateServiceFunction(MS_SETINVIS, onSetInvis);
	CreateServiceFunction(MS_SETVIS, onSetVis);
	CreateServiceFunction(MS_HIDE, onHide);
	CreateServiceFunction(MS_IGNORE, onIgnore);
	CreateServiceFunction(MS_PROTO, onChangeProto);
	CreateServiceFunction(MS_AUTHREQ, onSendAuthRequest);
	CreateServiceFunction(MS_COPYID, onCopyID);
	CreateServiceFunction(MS_RECVFILES, onRecvFiles);
	CreateServiceFunction(MS_STATUSMSG, onCopyStatusMsg);
	CreateServiceFunction(MS_COPYIP, onCopyIP);
	CreateServiceFunction(MS_COPYMIRVER, onCopyMirVer);
	CreateServiceFunction(MS_OPENIGNORE, OpenIgnoreOptions);

	HookEvent(ME_SYSTEM_MODULESLOADED, PluginInit);
	HookEvent(ME_SYSTEM_MODULELOAD, ModuleLoad);
	HookEvent(ME_SYSTEM_MODULEUNLOAD, ModuleLoad);
	HookEvent(ME_CLIST_PREBUILDCONTACTMENU, BuildMenu);
	HookEvent(ME_OPT_INITIALISE, OptionsInit);
	HookEvent(ME_PROTO_ACCLISTCHANGED, EnumProtoSubmenu);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CMPlugin::Unload()
{
	DestroyIcon(hIcons[3]);
	DestroyIcon(hIcons[4]);
	return 0;
}
