// -----------------------------------------------------------------------------
// ICQ plugin for Miranda NG
// -----------------------------------------------------------------------------
// Copyright � 2018-19 Miranda NG team
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
// -----------------------------------------------------------------------------

#include "stdafx.h"

void CIcqProto::LoadChatInfo(SESSION_INFO *si)
{
	int memberCount = getDword(si->hContact, "MemberCount");
	for (int i = 0; i < memberCount; i++) {
		char buf[100];
		mir_snprintf(buf, "m%d", i);
		ptrW szSetting(getWStringA(si->hContact, buf));
		JSONNode *node = json_parse(T2Utf(szSetting));
		if (node == nullptr)
			continue;

		CMStringW nick((*node)["nick"].as_mstring());
		CMStringW role((*node)["role"].as_mstring());
		CMStringW sn((*node)["sn"].as_mstring());

		GCEVENT gce = { m_szModuleName, si->ptszID, GC_EVENT_JOIN };
		gce.dwFlags = GCEF_SILENT;
		gce.ptszNick = nick;
		gce.ptszUID = sn;
		gce.time = ::time(0);
		gce.bIsMe = _wtoi(sn) == (int)m_dwUin;
		gce.ptszStatus = TranslateW(role);
		Chat_Event(&gce);

		json_delete(node);
	}
}

void CIcqProto::OnGetChatInfo(NETLIBHTTPREQUEST *pReply, AsyncHttpRequest *pReq)
{
	SESSION_INFO *si = (SESSION_INFO*)pReq->pUserInfo;

	RobustReply root(pReply);
	if (root.error() != 20000)
		return;

	int n = 0;
	char buf[100];
	const JSONNode &results = root.results();
	for (auto &it : results["members"]) {
		mir_snprintf(buf, "m%d", n++);

		CMStringW friendly = it["friendly"].as_mstring();
		CMStringW role = it["role"].as_mstring();
		CMStringW sn = it["sn"].as_mstring();

		JSONNode member;
		member << WCHAR_PARAM("nick", friendly) << WCHAR_PARAM("role", role) << WCHAR_PARAM("sn", sn);
		ptrW text(json_write(&member));
		setWString(si->hContact, buf, text);
	}

	setDword(si->hContact, "MemberCount", n);
	setId(si->hContact, "InfoVersion", _wtoi64(results["infoVersion"].as_mstring()));
	setId(si->hContact, "MembersVersion", _wtoi64(results["membersVersion"].as_mstring()));

	LoadChatInfo(si);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Invitation dialog

class CGroupchatInviteDlg : public CProtoDlgBase<CIcqProto>
{
	typedef CProtoDlgBase<CIcqProto> CSuper;

	CCtrlClc m_clc;
	SESSION_INFO *m_si;

	void FilterList(CCtrlClc*)
	{
		for (auto &hContact : Contacts()) {
			char *proto = GetContactProto(hContact);
			if (mir_strcmp(proto, m_proto->m_szModuleName) || m_proto->isChatRoom(hContact))
				if (HANDLE hItem = m_clc.FindContact(hContact))
					m_clc.DeleteItem(hItem);
		}
	}

	void ResetListOptions(CCtrlClc*)
	{
		m_clc.SetBkBitmap(0, nullptr);
		m_clc.SetBkColor(GetSysColor(COLOR_WINDOW));
		m_clc.SetGreyoutFlags(0);
		m_clc.SetLeftMargin(4);
		m_clc.SetIndent(10);
		m_clc.SetHideEmptyGroups(1);
		m_clc.SetHideOfflineRoot(1);
		for (int i = 0; i <= FONTID_MAX; i++)
			m_clc.SetTextColor(i, GetSysColor(COLOR_WINDOWTEXT));
	}

public:
	CGroupchatInviteDlg(CIcqProto *ppro, SESSION_INFO *si) :
		CSuper(ppro, IDD_GROUPCHAT_INVITE),
		m_si(si),
		m_clc(this, IDC_CLIST)
	{
		m_clc.OnNewContact =
			m_clc.OnListRebuilt = Callback(this, &CGroupchatInviteDlg::FilterList);
		m_clc.OnOptionsChanged = Callback(this, &CGroupchatInviteDlg::ResetListOptions);
	}

	bool OnInitDialog() override
	{
		SetWindowLongPtr(m_clc.GetHwnd(), GWL_STYLE,
			GetWindowLongPtr(m_clc.GetHwnd(), GWL_STYLE) | CLS_SHOWHIDDEN | CLS_HIDEOFFLINE | CLS_CHECKBOXES | CLS_HIDEEMPTYGROUPS | CLS_USEGROUPS | CLS_GREYALTERNATE | CLS_GROUPCHECKBOXES);
		m_clc.SendMsg(CLM_SETEXSTYLE, CLS_EX_DISABLEDRAGDROP | CLS_EX_TRACKSELECT, 0);

		ResetListOptions(&m_clc);
		FilterList(&m_clc);
		return true;
	}

	bool OnApply() override
	{
		CMStringA szMembers;
		for (auto &hContact : m_proto->AccContacts()) {
			if (m_proto->isChatRoom(hContact))
				continue;

			if (HANDLE hItem = m_clc.FindContact(hContact)) {
				if (m_clc.GetCheck(hItem)) {
					if (!szMembers.IsEmpty())
						szMembers.AppendChar(',');
					szMembers.Append(m_proto->GetUserId(hContact));
				}
			}
		}

		auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, ICQ_API_SERVER "/mchat/AddChat");
		pReq << CHAR_PARAM("f", "json") << WCHAR_PARAM("chat_id", m_si->ptszID) << CHAR_PARAM("aimsid", m_proto->m_aimsid) << CHAR_PARAM("r", pReq->m_reqId) << CHAR_PARAM("members", szMembers);
		m_proto->Push(pReq);
		return true;
	}
};

void CIcqProto::InviteUserToChat(SESSION_INFO *si)
{
	CGroupchatInviteDlg dlg(this, si);
	if (si->pDlg)
		dlg.SetParent(((CDlgBase*)si->pDlg)->GetHwnd());
	dlg.DoModal();
}

void CIcqProto::LeaveDestroyChat(SESSION_INFO *si)
{
	auto *pReq = new AsyncHttpRequest(CONN_MAIN, REQUEST_GET, ICQ_API_SERVER "/buddylist/hideChat");
	pReq << CHAR_PARAM("f", "json") << CHAR_PARAM("aimsid", m_aimsid) << WCHAR_PARAM("buddy", si->ptszID)
		<< CHAR_PARAM("r", pReq->m_reqId) << INT64_PARAM("lastMsgId", getId(si->hContact, DB_KEY_LASTMSGID));
	Push(pReq);

	Chat_Terminate(si->pszModule, si->ptszID, true);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Group chats

static gc_item sttLogListItems[] =
{
	{ LPGENW("&Invite a user"), IDM_INVITE, MENU_ITEM },
	{ nullptr, 0, MENU_SEPARATOR },
	{ LPGENW("&Leave/destroy chat"), IDM_LEAVE, MENU_ITEM }
};

int CIcqProto::GroupchatMenuHook(WPARAM, LPARAM lParam)
{
	GCMENUITEMS *gcmi = (GCMENUITEMS*)lParam;
	if (gcmi == nullptr)
		return 0;

	if (mir_strcmpi(gcmi->pszModule, m_szModuleName))
		return 0;

	SESSION_INFO *si = g_chatApi.SM_FindSession(gcmi->pszID, gcmi->pszModule);
	if (si == nullptr)
		return 0;

	if (gcmi->Type == MENU_ON_LOG)
		Chat_AddMenuItems(gcmi->hMenu, _countof(sttLogListItems), sttLogListItems, &g_plugin);

	return 0;
}

int CIcqProto::GroupchatEventHook(WPARAM, LPARAM lParam)
{
	GCHOOK *gch = (GCHOOK*)lParam;
	if (gch == nullptr)
		return 0;

	if (mir_strcmpi(gch->pszModule, m_szModuleName))
		return 0;

	SESSION_INFO *si = g_chatApi.SM_FindSession(gch->ptszID, gch->pszModule);
	if (si == nullptr)
		return 0;

	switch (gch->iType) {
	case GC_USER_MESSAGE:
		rtrimw(gch->ptszText);
		if (!mir_wstrlen(gch->ptszText))
			break;

		if (m_bOnline) {
			wchar_t *wszText = NEWWSTR_ALLOCA(gch->ptszText);
			Chat_UnescapeTags(wszText);
			SendMsg(si->hContact, 0, T2Utf(wszText));
		}
		break;

	case GC_USER_PRIVMESS:
		Chat_SendPrivateMessage(gch);
		break;

	case GC_USER_LOGMENU:
		Chat_ProcessLogMenu(si, gch->dwData);
		break;
	}

	return 0;
}

void CIcqProto::Chat_ProcessLogMenu(SESSION_INFO *si, int iChoice)
{
	switch (iChoice) {
	case IDM_INVITE:
		InviteUserToChat(si);
		break;

	case IDM_LEAVE:
		LeaveDestroyChat(si);
		break;
	}
}

void CIcqProto::Chat_SendPrivateMessage(GCHOOK *gch)
{
	MCONTACT hContact;
	DWORD dwUin = _wtoi(gch->ptszUID);
	auto *pCache = FindContactByUIN(dwUin);
	if (pCache == nullptr) {
		hContact = CreateContact(dwUin, true);
		setWString(hContact, "Nick", gch->ptszNick);
		db_set_b(hContact, "CList", "Hidden", 1);
		db_set_dw(hContact, "Ignore", "Mask1", 0);
	}
	else hContact = pCache->m_hContact;

	CallService(MS_MSG_SENDMESSAGE, hContact, 0);
}

void CIcqProto::ProcessGroupChat(const JSONNode &ev)
{
	for (auto &it : ev["mchats"]) {
		CMStringW wszId(it["sender"].as_mstring());
		SESSION_INFO *si = g_chatApi.SM_FindSession(wszId, m_szModuleName);
		if (si == nullptr)
			continue;

		CMStringW method(it["method"].as_mstring());
		GCEVENT gce = { m_szModuleName, si->ptszID, (method == "add_members") ? GC_EVENT_JOIN : GC_EVENT_PART };

		int iStart = 0;
		CMStringW members(it["members"].as_mstring());
		while (true) {
			CMStringW member = members.Tokenize(L",", iStart);
			if (member.IsEmpty())
				break;

			auto *pCache = FindContactByUIN(_wtoi(member));
			if (pCache == nullptr)
				continue;

			gce.ptszNick = Clist_GetContactDisplayName(pCache->m_hContact);
			gce.ptszUID = member;
			gce.time = ::time(0);
			gce.bIsMe = _wtoi(member) == (int)m_dwUin;
			Chat_Event(&gce);
		}
	}
}
