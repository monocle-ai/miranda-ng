/*

Miranda NG: the free IM client for Microsoft* Windows*

Copyright (C) 2012-20 Miranda NG team (https://miranda-ng.org)
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

/////////////////////////////////////////////////////////////////////////////////////////

//VERY VERY VERY BASIC ENCRYPTION FUNCTION

static void Encrypt(char *msg, BOOL up)
{
	int jump = (up) ? 5 : -5;
	for (int i = 0; msg[i]; i++)
		msg[i] = msg[i] + jump;
}

__forceinline void DecodeString(LPSTR buf)
{
	Encrypt(buf, FALSE);
}

struct VarDescr
{
	VarDescr(LPCSTR var, LPCSTR value) :
		szVar(mir_strdup(var)),
		szValue(mir_strdup(value)),
		iLen(0)
	{}

	VarDescr(LPCSTR var, LPSTR value) :
		szVar(mir_strdup(var)),
		szValue(value),
		iLen(0)
	{}

	VarDescr(LPCSTR var, PBYTE value, int len) :
		szVar(mir_strdup(var)),
		szValue((char*)memcpy(mir_alloc(len), value, len)),
		iLen(len)
	{}

	ptrA szVar, szValue;
	int iLen;
};

struct SettingUgraderParam
{
	CDb3Mmap *db;
	LPCSTR    szModule;
	MCONTACT  contactID;
	OBJLIST<VarDescr>* pList;
};

int sttSettingUgrader(const char *szSetting, void *lParam)
{
	SettingUgraderParam *param = (SettingUgraderParam*)lParam;
	if (param->db->IsSettingEncrypted(param->szModule, szSetting)) {
		DBVARIANT dbv = { DBVT_UTF8 };
		if (!param->db->GetContactSettingStr(param->contactID, param->szModule, szSetting, &dbv)) {
			if (dbv.type == DBVT_UTF8) {
				DecodeString(dbv.pszVal);
				param->pList->insert(new VarDescr(szSetting, (LPCSTR)dbv.pszVal));
			}
			param->db->FreeVariant(&dbv);
		}
	}
	return 0;
}

void sttContactEnum(MCONTACT contactID, const char *szModule, CDb3Mmap *db)
{
	OBJLIST<VarDescr> arSettings(1);
	SettingUgraderParam param = { db, szModule, contactID, &arSettings };
	db->EnumContactSettings(0, sttSettingUgrader, szModule, &param);

	for (auto &p : arSettings) {
		size_t len;
		BYTE *pResult = db->m_crypto->encodeString(p->szValue, &len);
		if (pResult != nullptr) {
			DBCONTACTWRITESETTING dbcws = { szModule, p->szVar };
			dbcws.value.type = DBVT_ENCRYPTED;
			dbcws.value.pbVal = pResult;
			dbcws.value.cpbVal = (WORD)len;
			db->WriteContactSetting(contactID, &dbcws);

			mir_free(pResult);
		}
	}
}

int sttModuleEnum(const char *szModule, void *lParam)
{
	CDb3Mmap *db = (CDb3Mmap*)lParam;
	sttContactEnum(0, szModule, db);

	for (MCONTACT contactID = db->FindFirstContact(); contactID; contactID = db->FindNextContact(contactID))
		sttContactEnum(contactID, szModule, db);

	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

int CDb3Mmap::InitCrypt()
{
	if (m_dbHeader.version == DB_OLD_VERSION)
		return 0;

	CRYPTO_PROVIDER *pProvider;
	bool bMissingKey = false;

	DBVARIANT dbv = { 0 };
	dbv.type = DBVT_BLOB;
	if (GetContactSetting(0, "CryptoEngine", "Provider", &dbv)) {
	LBL_CreateProvider:
		CRYPTO_PROVIDER **ppProvs;
		int iNumProvs;
		Crypto_EnumProviders(&iNumProvs, &ppProvs);
		if (iNumProvs == 0)
			return 1;

		if (iNumProvs > 1)
		{
			CSelectCryptoDialog dlg(ppProvs, iNumProvs);
			dlg.DoModal();
			pProvider = dlg.GetSelected();
		}
		else pProvider = ppProvs[0];

		DBCONTACTWRITESETTING dbcws = { "CryptoEngine", "Provider" };
		dbcws.value.type = DBVT_BLOB;
		dbcws.value.pbVal = (PBYTE)pProvider->pszName;
		dbcws.value.cpbVal = (WORD)mir_strlen(pProvider->pszName) + 1;
		WriteContactSetting(0, &dbcws);
	}
	else {
		if (dbv.type != DBVT_BLOB) { // old version, clean it up
			bMissingKey = true;
			goto LBL_CreateProvider;
		}

		pProvider = Crypto_GetProvider(LPCSTR(dbv.pbVal));
		FreeVariant(&dbv);
		if (pProvider == nullptr)
			goto LBL_CreateProvider;
	}

	if ((m_crypto = pProvider->pFactory()) == nullptr)
		return 3;

	dbv.type = DBVT_BLOB;
	if (GetContactSetting(0, "CryptoEngine", "StoredKey", &dbv)) {
		bMissingKey = true;

	LBL_SetNewKey:
		m_crypto->generateKey(); // unencrypted key
		StoreKey();
	}
	else {
		size_t iKeyLength = m_crypto->getKeyLength();
		if (dbv.cpbVal != (WORD)iKeyLength)
			goto LBL_SetNewKey;

		if (!m_crypto->setKey(dbv.pbVal, iKeyLength)) {
			if (memcmp(m_dbHeader.signature, &dbSignatureE, sizeof(m_dbHeader.signature)))
				goto LBL_SetNewKey;

			if (!EnterPassword(dbv.pbVal, iKeyLength)) { // password protected?
				if (m_dbHeader.version >= DB_094_VERSION)
					return 4;

				// one of the early used version of mmap was replaced then by mmap_sa
				// simply remove old badly generated key
				bMissingKey = true;
				goto LBL_SetNewKey;
			}
		}

		FreeVariant(&dbv);
	}

	if (bMissingKey)
		EnumModuleNames(sttModuleEnum, this);

	dbv.type = DBVT_BYTE;
	if (!GetContactSetting(0, "CryptoEngine", "DatabaseEncryption", &dbv))
		m_bEncrypted = dbv.bVal != 0;

	InitDialogs();
	return 0;
}

void CDb3Mmap::StoreKey()
{
	size_t iKeyLength = m_crypto->getKeyLength();
	BYTE *pKey = (BYTE*)_alloca(iKeyLength);
	m_crypto->getKey(pKey, iKeyLength);

	DBCONTACTWRITESETTING dbcws = { "CryptoEngine", "StoredKey" };
	dbcws.value.type = DBVT_BLOB;
	dbcws.value.cpbVal = (WORD)iKeyLength;
	dbcws.value.pbVal = pKey;
	WriteContactSetting(0, &dbcws);

	SecureZeroMemory(pKey, iKeyLength);
}

void CDb3Mmap::SetPassword(const wchar_t *ptszPassword)
{
	if (ptszPassword == nullptr || *ptszPassword == 0) {
		m_bUsesPassword = false;
		m_crypto->setPassword(nullptr);
	}
	else {
		m_bUsesPassword = true;
		m_crypto->setPassword(T2Utf(ptszPassword));
	}
	UpdateMenuItem();
}

/////////////////////////////////////////////////////////////////////////////////////////

void CDb3Mmap::ToggleEncryption()
{
	HANDLE hSave1 = g_hevSettingChanged;    g_hevSettingChanged = nullptr;
	HANDLE hSave2 = g_hevEventAdded;       g_hevEventAdded = nullptr;
	HANDLE hSave3 = g_hevEventDeleted;     g_hevEventDeleted = nullptr;
	HANDLE hSave4 = g_hevEventFiltered; g_hevEventFiltered = nullptr;

	mir_cslock lck(m_csDbAccess);
	ToggleSettingsEncryption(0);
	ToggleEventsEncryption(0);

	for (MCONTACT contactID = FindFirstContact(); contactID; contactID = FindNextContact(contactID)) {
		ToggleSettingsEncryption(contactID);
		ToggleEventsEncryption(contactID);
	}

	m_bEncrypted = !m_bEncrypted;

	DBCONTACTWRITESETTING dbcws = { "CryptoEngine", "DatabaseEncryption" };
	dbcws.value.type = DBVT_BYTE;
	dbcws.value.bVal = m_bEncrypted;
	WriteContactSetting(0, &dbcws);

	g_hevSettingChanged = hSave1;
	g_hevEventAdded = hSave2;
	g_hevEventDeleted = hSave3;
	g_hevEventFiltered = hSave4;
}

void CDb3Mmap::ToggleSettingsEncryption(MCONTACT contactID)
{
	DWORD ofsContact = GetContactOffset(contactID);
	if (ofsContact == 0)
		return;

	DBContact *contact = (DBContact*)DBRead(ofsContact, nullptr);
	if (contact->ofsFirstSettings == 0)
		return;

	// fast cycle through all settings
	DBContactSettings *setting = (DBContactSettings*)DBRead(contact->ofsFirstSettings, nullptr);
	DWORD offset = contact->ofsFirstSettings;
	char *szModule = GetModuleNameByOfs(setting->ofsModuleName);
	if (szModule == nullptr)
		return;

	while (true) {
		OBJLIST<VarDescr> arSettings(10);
		char szSetting[256];
		int bytesRemaining, len;
		DWORD ofsBlobPtr = offset + offsetof(DBContactSettings, blob), ofsNext = setting->ofsNext;
		PBYTE pBlob = (PBYTE)DBRead(ofsBlobPtr, &bytesRemaining);
		while (pBlob[0]) {
			NeedBytes(1);
			len = pBlob[0];
			memcpy(szSetting, pBlob + 1, len); szSetting[len] = 0;
			NeedBytes(1 + pBlob[0]);
			MoveAlong(1 + pBlob[0]);
			NeedBytes(5);

			switch (pBlob[0]) {
			case DBVT_ASCIIZ:
				len = *(PWORD)(pBlob + 1);
				// we need to convert a string into utf8 and encrypt it
				if (!m_bEncrypted) {
					BYTE bSave = pBlob[len + 3]; pBlob[len + 3] = 0;
					arSettings.insert(new VarDescr(szSetting, mir_utf8encode((LPCSTR)pBlob + 3)));
					pBlob[len + 3] = bSave;
				}
				NeedBytes(3 + len);
				break;

			case DBVT_UTF8:
				len = *(PWORD)(pBlob + 1);
				// we need to encrypt these strings
				if (!m_bEncrypted) {
					BYTE bSave = pBlob[len + 3]; pBlob[len + 3] = 0;
					arSettings.insert(new VarDescr(szSetting, (LPCSTR)pBlob + 3));
					pBlob[len + 3] = bSave;
				}
				NeedBytes(3 + len);
				break;

			case DBVT_ENCRYPTED:
				len = *(PWORD)(pBlob + 1);
				// we need to decrypt these strings
				if (m_bEncrypted && !IsSettingEncrypted(szModule, szSetting))
					arSettings.insert(new VarDescr(szSetting, pBlob + 3, len));
				NeedBytes(3 + len);
				break;

			case DBVT_BLOB:
				NeedBytes(3 + *(PWORD)(pBlob + 1));
				break;
			}
			NeedBytes(3);
			MoveAlong(1 + GetSettingValueLength(pBlob));
			NeedBytes(1);
		}

		for (auto &p : arSettings) {
			if (!m_bEncrypted) {
				size_t encodedLen;
				BYTE *pResult = m_crypto->encodeString(p->szValue, &encodedLen);
				if (pResult != nullptr) {
					DBCONTACTWRITESETTING dbcws = { szModule, p->szVar };
					dbcws.value.type = DBVT_ENCRYPTED;
					dbcws.value.pbVal = pResult;
					dbcws.value.cpbVal = (WORD)encodedLen;
					WriteContactSetting(contactID, &dbcws);

					mir_free(pResult);
				}
			}
			else {
				size_t realLen;
				ptrA decoded(m_crypto->decodeString((PBYTE)(char*)p->szValue, p->iLen, &realLen));
				if (decoded != nullptr) {
					DBCONTACTWRITESETTING dbcws = { szModule, p->szVar };
					dbcws.value.type = DBVT_UNENCRYPTED;
					dbcws.value.pszVal = decoded;
					dbcws.value.cchVal = (WORD)realLen;
					WriteContactSetting(contactID, &dbcws);
				}
			}
		}

		if (!ofsNext)
			break;

		setting = (DBContactSettings*)DBRead(offset = ofsNext, nullptr);
		if ((szModule = GetModuleNameByOfs(setting->ofsModuleName)) == nullptr)
			break;
	}
}

void CDb3Mmap::ToggleEventsEncryption(MCONTACT contactID)
{
	DWORD ofsContact = GetContactOffset(contactID);
	if (ofsContact == 0)
		return;

	DBContact contact = *(DBContact*)DBRead(ofsContact, nullptr);
	if (contact.ofsFirstEvent == 0 || contact.signature != DBCONTACT_SIGNATURE)
		return;

	// fast cycle through all events
	for (DWORD offset = contact.ofsFirstEvent; offset != 0;) {
		DBEvent evt = *(DBEvent*)DBRead(offset, nullptr);
		if (evt.signature != DBEVENT_SIGNATURE)
			return;

		size_t len;
		DWORD ofsDest;
		mir_ptr<BYTE> pBlob;
		BYTE *pSource = DBRead(offset + offsetof(DBEvent, blob), nullptr);
		if (!m_bEncrypted) { // we need more space
			if ((pBlob = m_crypto->encodeBuffer(pSource, evt.cbBlob, &len)) == nullptr)
				return;

			ofsDest = ReallocSpace(offset, offsetof(DBEvent, blob) + evt.cbBlob, offsetof(DBEvent, blob) + (DWORD)len);

			if (evt.ofsNext) {
				DBEvent *e = (DBEvent*)DBRead(evt.ofsNext, nullptr);
				e->ofsPrev = ofsDest;
				DBWrite(evt.ofsNext, e, sizeof(DBEvent));
			}
			if (evt.ofsPrev) {
				DBEvent *e = (DBEvent*)DBRead(evt.ofsPrev, nullptr);
				e->ofsNext = ofsDest;
				DBWrite(evt.ofsPrev, e, sizeof(DBEvent));
			}
			if (contact.ofsFirstEvent == offset)
				contact.ofsFirstEvent = ofsDest;
			if (contact.ofsLastEvent == offset)
				contact.ofsLastEvent = ofsDest;
			if (contact.ofsFirstUnread == offset)
				contact.ofsFirstUnread = ofsDest;

			evt.flags |= DBEF_ENCRYPTED;
		}
		else {
			if ((pBlob = (BYTE*)m_crypto->decodeBuffer(pSource, evt.cbBlob, &len)) == nullptr)
				return;

			ofsDest = offset; // reuse the old space
			evt.flags &= ~DBEF_ENCRYPTED;

			if (len < evt.cbBlob)
				DBFill(ofsDest + offsetof(DBEvent, blob) + (DWORD)len, evt.cbBlob - (DWORD)len);
		}
		evt.cbBlob = (DWORD)len;

		DBWrite(ofsDest, &evt, offsetof(DBEvent, blob));
		DBWrite(ofsDest + offsetof(DBEvent, blob), pBlob, (DWORD)len);

		offset = evt.ofsNext;
	}

	DBWrite(ofsContact, &contact, sizeof(DBContact));
}
