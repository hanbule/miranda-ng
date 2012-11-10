/*

Miranda IM: the free IM client for Microsoft* Windows*

Copyright 2000-2003 Miranda ICQ/IM project, 
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
#include "commonheaders.h"

int DefaultImageListColorDepth = ILC_COLOR32;

HIMAGELIST himlCListClc;

static HANDLE hSettingChanged1;

int BgStatusBarChange(WPARAM wParam,LPARAM lParam);

int BgClcChange(WPARAM wParam,LPARAM lParam);
int OnFrameTitleBarBackgroundChange(WPARAM wParam,LPARAM lParam);

void InitDisplayNameCache(SortedList *list);
void FreeDisplayNameCache(SortedList *list);

void UnloadCLUIModule( void );

extern int sortByStatus;
struct ClcContact * hitcontact = NULL;

extern LRESULT ( CALLBACK *saveContactListControlWndProc )(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

static int stopStatusUpdater = 0;
void StatusUpdaterThread(void*)
{
	int i,curdelay,lastcheck = 0;
	HANDLE hContact = db_find_first();

	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_LOWEST);

	while (!stopStatusUpdater) {
		curdelay = db_get_b(hContact,"CList","StatusMsgAutoDelay",15000);
		if (curdelay<5000) curdelay = 5000;

		if ((int)(GetTickCount()-lastcheck)>curdelay)
		{
			lastcheck = GetTickCount();
			if ( db_get_b(hContact,"CList","StatusMsgAuto",0)) {
				for (i = 0; i<5; i++) {
					if (hContact != NULL) {
						pClcCacheEntry pdnce  = (pClcCacheEntry)pcli->pfnGetCacheEntry((HANDLE)hContact);
						if (pdnce && !pdnce->protoNotExists && pdnce->szProto)
							CallContactService(hContact, PSS_GETAWAYMSG, 0, 0);

						hContact = db_find_next(hContact);
					}
					if (hContact == NULL) {
						hContact = db_find_first();
						if (hContact == NULL)
							break;
					}
					Sleep(500);
				}
			}
		}
		//Sleep( db_get_b(hContact,"CList","StatusMsgAutoDelay",100));
		Sleep(200);
	}
}

HMENU BuildGroupPopupMenu( ClcGroup* group )
{
	return (HMENU)CallService(MS_CLIST_MENUBUILDSUBGROUP,(WPARAM)group,0);
}

void SortClcByTimer (HWND hwnd)
{
	KillTimer(hwnd,TIMERID_DELAYEDRESORTCLC);
	SetTimer(hwnd,TIMERID_DELAYEDRESORTCLC,db_get_b(NULL,"CLUI","DELAYEDTIMER",200),NULL);
}

static int ClcSettingChanged(WPARAM wParam, LPARAM lParam)
{
	DBCONTACTWRITESETTING *cws = (DBCONTACTWRITESETTING*)lParam;

	if ((HANDLE)wParam != NULL && !strcmp(cws->szModule,"MetaContacts") && !strcmp(cws->szSetting,"Handle"))
		pcli->pfnClcBroadcast( INTM_NAMEORDERCHANGED, 0, 0 );
	
	if ((HANDLE)wParam != NULL&&!strcmp(cws->szModule,"CList")) {
		if ( !strcmp( cws->szSetting, "noOffline" ))
			pcli->pfnClcBroadcast( INTM_NAMEORDERCHANGED, wParam, lParam );
		else if (!strcmp(cws->szSetting,"StatusMsg")) 
			pcli->pfnClcBroadcast( INTM_STATUSMSGCHANGED, wParam, lParam );
	}
	return 0;
}

static int ClcModulesLoaded(WPARAM wParam,LPARAM lParam)
{
	CallService(MS_BACKGROUNDCONFIG_REGISTER,(WPARAM)"StatusBar Background/StatusBar",0);
	CallService(MS_BACKGROUNDCONFIG_REGISTER,(WPARAM)"List Background/CLC",0);
	CallService(MS_BACKGROUNDCONFIG_REGISTER,(WPARAM)"Frames TitleBar BackGround/FrameTitleBar",0);
	HookEvent(ME_BACKGROUNDCONFIG_CHANGED,BgClcChange);
	HookEvent(ME_BACKGROUNDCONFIG_CHANGED,BgStatusBarChange);
	HookEvent(ME_BACKGROUNDCONFIG_CHANGED,OnFrameTitleBarBackgroundChange);
	return 0;
}

static int ClcShutdown(WPARAM wParam, LPARAM lParam)
{
	UnhookEvent(hSettingChanged1);
	UnloadCLUIModule();
	return 0;
}

LRESULT CALLBACK ContactListControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{     
	struct ClcData *dat = (struct ClcData*)GetWindowLongPtr(hwnd,0);
	if ( msg >= CLM_FIRST && msg < CLM_LAST )
		return pcli->pfnProcessExternalMessages(hwnd,dat,msg,wParam,lParam);

	switch (msg) {
	case WM_CREATE:
		dat = (struct ClcData*)mir_calloc( sizeof(struct ClcData));
		SetWindowLongPtr(hwnd,0,(LPARAM)dat);
		InitDisplayNameCache(&dat->lCLCContactsCache);
		break;

	case INTM_ICONCHANGED:
	{	
		int recalcScrollBar = 0,shouldShow;
		HANDLE hSelItem = NULL;
		struct ClcContact *selcontact = NULL;
		pClcCacheEntry cacheEntry = GetContactFullCacheEntry((HANDLE)wParam);

		WORD status;
		int needsResort = 0;

		char *szProto = cacheEntry->szProto;
		if (szProto == NULL)
			status = ID_STATUS_OFFLINE;
		else
			status = cacheEntry->status;
		
		shouldShow = (GetWindowLongPtr(hwnd,GWL_STYLE)&CLS_SHOWHIDDEN || !cacheEntry->bIsHidden) && (!pcli->pfnIsHiddenMode(dat,status)||cacheEntry->noHiddenOffline || CallService(MS_CLIST_GETCONTACTICON,wParam,0) != LOWORD(lParam));	//this means an offline msg is flashing, so the contact should be shown

		ClcContact *contact;
		ClcGroup *group;
		if (!FindItem(hwnd, dat, (HANDLE)wParam, &contact, &group, NULL)) {				
			if (shouldShow && CallService(MS_DB_CONTACT_IS, wParam, 0)) {
				if (dat->selection>=0 && GetRowByIndex(dat,dat->selection,&selcontact,NULL) != -1)
					hSelItem = pcli->pfnContactToHItem(selcontact);
				AddContactToTree(hwnd,dat,(HANDLE)wParam,0,0);
				needsResort = 1;
				recalcScrollBar = 1;					
				FindItem(hwnd,dat,(HANDLE)wParam,&contact,NULL,NULL);
				if (contact) {						
					contact->iImage = (WORD)lParam;
					pcli->pfnNotifyNewContact(hwnd,(HANDLE)wParam);
					dat->needsResort = 1;
				}
			}				
		}
		else {
			//item in list already
			DWORD style = GetWindowLongPtr(hwnd,GWL_STYLE);				
			if (contact->iImage == (WORD)lParam) break;				
			if (sortByStatus) dat->needsResort = 1;

			if (!shouldShow && !(style&CLS_NOHIDEOFFLINE) && (style&CLS_HIDEOFFLINE || group->hideOffline)) {
				if (dat->selection>=0 && GetRowByIndex(dat,dat->selection,&selcontact,NULL) != -1)
					hSelItem = pcli->pfnContactToHItem(selcontact);
				RemoveItemFromGroup(hwnd,group,contact,0);
				recalcScrollBar = 1;
				dat->needsResort = 1;
			}
			else {
				int oldflags;
				contact->iImage = (WORD)lParam;
				oldflags = contact->flags;
				if (!pcli->pfnIsHiddenMode(dat,status)||cacheEntry->noHiddenOffline) contact->flags |= CONTACTF_ONLINE;
				else contact->flags &= ~CONTACTF_ONLINE;
				if (oldflags != contact->flags)
					dat->needsResort = 1;
		}	}
		if (hSelItem) {
			ClcGroup *selgroup;
			if ( FindItem(hwnd,dat,hSelItem,&selcontact,&selgroup,NULL))
				dat->selection = GetRowsPriorTo(&dat->list,selgroup,List_IndexOf((SortedList*)&selgroup->cl, selcontact));
			else
				dat->selection = -1;
		}

		SortClcByTimer(hwnd);
		if (recalcScrollBar) RecalcScrollBar(hwnd,dat);			
		goto LBL_Exit;
	}
	case INTM_STATUSMSGCHANGED:
	{	
		DBVARIANT dbv;

		if (!(dat->style & CLS_SHOWSTATUSMESSAGES))
			break;

		ClcContact *contact;
		ClcGroup *group;
		if ( FindItem(hwnd,dat,(HANDLE)wParam,&contact,&group,NULL) && contact != NULL) {
			contact->flags  &=  ~CONTACTF_STATUSMSG;
			if (!DBGetContactSettingTString((HANDLE)wParam, "CList", "StatusMsg", &dbv)) {
				int j;
				if (dbv.ptszVal == NULL||_tcslen(dbv.ptszVal) == 0) break;
				lstrcpyn(contact->szStatusMsg, dbv.ptszVal, SIZEOF(contact->szStatusMsg));
				for (j = (int)_tcslen(contact->szStatusMsg)-1;j>=0;j--) {
					if (contact->szStatusMsg[j] == '\r' || contact->szStatusMsg[j] == '\n' || contact->szStatusMsg[j] == '\t') {
						contact->szStatusMsg[j] = ' ';
					}
				}
				DBFreeVariant(&dbv);
				if (_tcslen(contact->szStatusMsg)>0) {
					contact->flags |= CONTACTF_STATUSMSG;
					dat->needsResort = TRUE;
				}
			}
		}

		InvalidateRect(hwnd,NULL,TRUE);

		SortClcByTimer(hwnd);
		RecalcScrollBar(hwnd,dat);
		goto LBL_Exit;
	}

	case WM_TIMER:
		if (wParam == TIMERID_DELAYEDREPAINT) {
			KillTimer(hwnd,TIMERID_DELAYEDREPAINT);
			InvalidateRect(hwnd,NULL,FALSE);
			break;
		}

		if ( wParam == TIMERID_SUBEXPAND) {		
			KillTimer(hwnd,TIMERID_SUBEXPAND);
			if (hitcontact) {
				if (hitcontact->SubExpanded) hitcontact->SubExpanded = 0; else hitcontact->SubExpanded = 1;
				DBWriteContactSettingByte(hitcontact->hContact,"CList","Expanded",hitcontact->SubExpanded);
			}
			hitcontact = NULL;
			dat->needsResort = 1;
			SortCLC(hwnd,dat,1);		
			RecalcScrollBar(hwnd,dat);
			break;
		}			
		break;

	case WM_DESTROY:
		FreeDisplayNameCache(&dat->lCLCContactsCache);
		stopStatusUpdater = 1;
		break;
	}

	LRESULT res = saveContactListControlWndProc(hwnd, msg, wParam, lParam);
	switch (msg) {
	case WM_CREATE:
		mir_forkthread(StatusUpdaterThread,0);
		break;
	}
	return res;

LBL_Exit:
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

int LoadCLCModule(void)
{
	LoadCLUIFramesModule();

	himlCListClc = (HIMAGELIST)CallService(MS_CLIST_GETICONSIMAGELIST,0,0);
	
	HookEvent(ME_SYSTEM_MODULESLOADED,ClcModulesLoaded);
	hSettingChanged1 = HookEvent(ME_DB_CONTACT_SETTINGCHANGED,ClcSettingChanged);
	HookEvent(ME_OPT_INITIALISE,ClcOptInit);
	HookEvent(ME_SYSTEM_SHUTDOWN,ClcShutdown);
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////

static INT_PTR CLUIGetCapsService(WPARAM wParam,LPARAM lParam)
{
	switch (wParam) {
	case CLUICAPS_FLAGS1:
		return CLUIF_HIDEEMPTYGROUPS | CLUIF_DISABLEGROUPS | CLUIF_HASONTOPOPTION | CLUIF_HASAUTOHIDEOPTION;
	case CLUICAPS_FLAGS2:
		return MAKELONG(EXTRA_ICON_COUNT,1);
	}
	return 0;
}

int PreloadCLCModule()
{
	CreateServiceFunction(MS_CLUI_GETCAPS, CLUIGetCapsService);
	return 0;
}
