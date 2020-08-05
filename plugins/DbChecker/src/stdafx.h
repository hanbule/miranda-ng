/*
Miranda Database Tool
Copyright 2000-2011 Miranda ICQ/IM project, 
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

#pragma once

#include <windows.h>
#include <process.h>
#include <malloc.h>
#include <commctrl.h>
#include <time.h>

#include <newpluginapi.h>
#include <m_database.h>
#include <m_db_int.h>
#include <m_langpack.h>
#include <win2k.h>

#include "resource.h"
#include "version.h"

#define WZM_GOTOPAGE   (WM_USER+1)
#define WZN_PAGECHANGING  (WM_USER+1221)
#define WZN_CANCELCLICKED (WM_USER+1222)

struct DbToolOptions
{
	wchar_t filename[MAX_PATH];
	MDatabaseCommon *db;
	MIDatabaseChecker *dbChecker;
	DWORD error;
};

struct CMPlugin : public PLUGIN<CMPlugin>
{
	CMPlugin();

	int Load() override;
	int Unload() override;
};

extern DbToolOptions opts;
extern HANDLE hEventRun, hEventAbort;
extern int errorCount;
extern LRESULT wizardResult;
extern bool bServiceMode, bLaunchMiranda, bAutoExit;

int DoMyControlProcessing(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam, INT_PTR *bReturn);

INT_PTR CALLBACK WizardDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SelectDbDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ProgressDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK FinishedDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK OpenErrorDlgProc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam);

int OpenDatabase(HWND hdlg);

#define STATUS_CLASSMASK  0x0f
void AddToStatus(int flags, const wchar_t* fmt, ...);
void SetProgressBar(int perThou);

