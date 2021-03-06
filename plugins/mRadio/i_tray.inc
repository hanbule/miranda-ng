{}
var
  trayradioparent:HGENMENU;
  trayprotoh:THANDLE;
  trayparent:THANDLE;
  traymute  :THANDLE;
  trayplay  :THANDLE;
  srvtrayplaypause:THANDLE;
  srvtraystop:THANDLE;
const
  trayStations:TSortedList = (items:nil; realCount:0; limit:0; increment:8; sortFunc: nil);
  
type
  pTrayRadioStation = ^tTrayRadioStation;
  tTrayRadioStation = record
    name    :pWideChar;
    hContact:THANDLE;
    service :THANDLE;
    menuitem:THANDLE;
    presents:int; // 0 - not used, 1 - ok, 2 - new
  end;

function MyStrSort(para1:pointer; para2:pointer):int; cdecl;
begin
  result:=lstrcmpiW{StrCmpW}(pTrayRadioStation(para1).name,pTrayRadioStation(para2).name);
end;

function ChooseStation(wParam:WPARAM;lParam,lParam1:LPARAM):int; cdecl;
begin
  result:=Service_RadioPlayStop(lParam1,0);
end;

procedure MakeStationsMenu;
var
  hContact:Cardinal;
  p:pWideChar;
  i,idx:integer;
  tmp:pTrayRadioStation;
  srch:tTrayRadioStation;
  pc:pAnsiChar;
  buf:array [0..63] of AnsiChar;
  mi:TMO_MenuItem;
begin
  trayStations.sortFunc:=@MyStrSort;

  // clear presents flag
  if trayStations.realCount>0 then
    for i:=0 to trayStations.realCount-1 do
      pTrayRadioStation(trayStations.Items[i]).presents:=0;

  // Fill list
  FillChar(srch,SizeOf(srch),0);
  hContact:=db_find_first(PluginName);
  while hContact<>0 do
  begin
     p:=DBReadUnicode(hContact,strCList,'MyHandle',nil);
     if p<>nil then
     begin
       srch.name:=p;
       // search in list
       if List_GetIndex(@trayStations,@srch,@idx)<>0 then
         // found - set mark
         pTrayRadioStation(trayStations.Items[idx]).presents:=1
       else // add if not found
       begin
         mGetMem(tmp,SizeOf(tTrayRadioStation));
         tmp.name    :=p;
         tmp.presents:=2;
         tmp.hContact:=hContact;
         List_InsertPtr(@trayStations,tmp);
       end;
     end;
    hContact:=db_find_next(hContact,PluginName);
  end;
  // delete obsolete elements
  for i:=trayStations.realCount-1 downto 0 do
  begin
    tmp:=pTrayRadioStation(trayStations.Items[i]);
    if tmp.presents=0 then
    begin
      mFreeMem(tmp.name);
      DestroyServiceFunction(tmp.service);
      Menu_RemoveItem(tmp.menuitem);
      mFreeMem(tmp);
      List_Remove(@trayStations,i);
    end;
  end;

  // build menu from sorted list
  FillChar(mi,SizeOf(mi),0);
  mi.Flags :=CMIF_KEEPUNTRANSLATED or CMIF_UNICODE;
  mi.root:=trayparent;
  pc:=StrCopyE(@buf,'mRadio/Choose');
  for i:=0 to trayStations.realCount-1 do
  begin
    tmp:=pTrayRadioStation(trayStations.Items[i]);
    if tmp.presents=2 then
    begin
      IntToStr(pc,tmp.hContact);
      tmp.service:=CreateServiceFunctionParam(@buf,@ChooseStation,tmp.hContact);
      mi.position  :=i;
      mi.pszService:=@buf;
      mi.szName.w  :=tmp.name;
      tmp.menuitem:=Menu_AddTrayMenuItem(@mi);
    end;
  end;
end;

function TrayPlayPause(wParam:WPARAM;lParam:LPARAM):int_ptr; cdecl;
begin
  result:=CallService(MS_RADIO_COMMAND,MRC_PAUSE,0);
end;

function TrayStop(wParam:WPARAM;lParam:LPARAM):int_ptr; cdecl;
begin
  result:=CallService(MS_RADIO_COMMAND,MRC_STOP,0);
end;

function TrayProto(wParam:WPARAM;lParam:LPARAM):int_ptr; cdecl;
begin
  if PluginStatus=ID_STATUS_ONLINE then
    wParam:=ID_STATUS_OFFLINE
  else
    wParam:=ID_STATUS_ONLINE;
  result:=Service_SetStatus(wParam,lParam);
end;

procedure CreateTrayMenu;
var
  mi:TMO_MenuItem;
  playstr:pWideChar;
  ico:pAnsiChar;
begin
  FillChar(mi, sizeof(mi), 0);
  SET_UID(@mi, 'CF40B5D0-B21E-4B10-B943-F35FDD343B97');
  mi.flags   :=CMIF_UNICODE;
  mi.szName.w:=cPluginName;
  mi.hIcon   :=IcoLib_GetIcon(IcoBtnSettings,0);
  trayradioparent:=Menu_AddTrayMenuItem(@mi);

  FillChar(mi, sizeof(mi), 0);
  mi.root:=trayradioparent;
  SET_UID(@mi, '7375ED25-F6C5-41F3-8EC2-55162E85E59D');

  if PluginStatus=ID_STATUS_ONLINE then
  begin
    mi.szName.w  :='mRadio off';
  end
  else
  begin
    mi.szName.w  :='mRadio on';
  end;
  mi.flags:=CMIF_UNICODE;
  mi.position  :=0;
  srvtraystop:=CreateServiceFunction('mRadio/TrayProto',@TrayProto);
  mi.pszService:='mRadio/TrayProto';
  trayprotoh:=Menu_AddTrayMenuItem(@mi);

  if gVolume<0 then
  begin
    mi.flags:=CMIF_UNICODE or CMIF_CHECKED;
    ico:=IcoBtnOff;
  end
  else
  begin
    mi.flags:=CMIF_UNICODE;
    ico:=IcoBtnOn;
  end;
  mi.hIcon     :=IcoLib_GetIcon(ico,0);
  mi.szName.w  :='Mute';
  mi.pszService:=MS_RADIO_MUTE;
  mi.position  :=1;
  traymute:=Menu_AddTrayMenuItem(@mi);

  SET_UID(@mi, '00041DD6-72DF-4F05-810F-C09CBA092C17');
  mi.hIcon:=0;
  mi.flags:=CMIF_UNICODE;
  if CallService(MS_RADIO_COMMAND,MRC_STATUS,RD_STATUS_GET)<>RD_STATUS_PLAYING then
  begin
    playstr:='Play'
  end
  else
  begin
    playstr:='Pause';
  end;
  mi.szName.w  :=playstr;
  mi.position  :=2;
  srvtrayplaypause:=CreateServiceFunction('mRadio/TrayPlayPause',@TrayPlayPause);
  mi.pszService:='mRadio/TrayPlayPause';
  trayplay:=Menu_AddTrayMenuItem(@mi);

  SET_UID(@mi, '9AD92E78-38FA-4855-9B50-F96C590D54DA');
  mi.szName.w  :='Stop';
  mi.position  :=3;
  srvtraystop:=CreateServiceFunction('mRadio/TrayStop',@TrayStop);
  mi.pszService:='mRadio/TrayStop';
  Menu_AddTrayMenuItem(@mi);

  SET_UID(@mi, 'AC6A4BFA-7A47-4B56-A914-9CBD137E64BC');
  mi.szName.w  :='Quick Open';
  mi.position  :=4;
  mi.pszService:=MS_RADIO_QUICKOPEN;
  mi.hIcon     :=IcoLib_GetIcon(IcoBtnOpen,0);
  Menu_AddTrayMenuItem(@mi);

  SET_UID(@mi, '4F9ED4D1-5264-4194-934A-21E2DC5116CA');
  mi.hIcon    :=0;
  mi.szName.w :='Play Station';
  mi.position :=1000;
  mi.pszService:=nil;
  trayparent:=Menu_AddTrayMenuItem(@mi);
end;

function TrayPrebuild(wParam:WPARAM;lParam:LPARAM):int; cdecl;
begin
  if PluginStatus=ID_STATUS_ONLINE then
    Menu_ModifyItem(trayprotoh, 'mRadio off')
  else
    Menu_ModifyItem(trayprotoh, 'mRadio on');

  if gVolume<0 then
    Menu_ModifyItem(traymute, nil, IcoLib_GetIconHandle(IcoBtnOff))
  else
    Menu_ModifyItem(traymute, nil, IcoLib_GetIconHandle(IcoBtnOn));

  if CallService(MS_RADIO_COMMAND,MRC_STATUS,RD_STATUS_GET)<>RD_STATUS_PLAYING then
    Menu_ModifyItem(trayplay, 'Play')
  else
    Menu_ModifyItem(trayplay, 'Pause');

  MakeStationsMenu();
  result:=0;
end;

procedure CreateMIMTrayMenu;
begin
  CreateTrayMenu();
  MakeStationsMenu();
  HookEvent(ME_CLIST_PREBUILDTRAYMENU,@TrayPrebuild)
end;

procedure RemoveTrayItems;
var
  i:integer;
  tmp:pTrayRadioStation;
begin
  // remove stations
  for i:=trayStations.realCount-1 downto 0 do
  begin
    tmp:=pTrayRadioStation(trayStations.Items[i]);
    mFreeMem(tmp.name);
    DestroyServiceFunction(tmp.service);
    mFreeMem(tmp);
  end;
  List_Destroy(@trayStations);
  DestroyServiceFunction(srvtrayplaypause);
  DestroyServiceFunction(srvtraystop);

  Menu_RemoveItem(trayradioparent);
end;
