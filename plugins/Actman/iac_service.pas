unit iac_service;

interface

implementation

uses
  windows, messages, commctrl,
  global, iac_global,
  m_api,
  sedit,strans,mApiCardM,
  mirutils,mircontacts,dbsettings, editwrapper,
  awkservices,
  syswin,wrapper,common;

{$resource iac_service.res}

const
// V3
  ACF_SCRIPT_SERVICE = $00800000;
// V2
  ACF2_SRV_WPAR = $00000001;
  ACF2_SRV_LPAR = $00000002;
  ACF2_SRV_SRVC = $00000004;
  ACF2_FREEMEM  = $00000100;
  ACF_OLD_WPARNUM  = $00000001;
  ACF_OLD_LPARNUM  = $00000002;
  ACF_OLD_WUNICODE = $00000004;
  ACF_OLD_LUNICODE = $00000008;
  ACF_OLD_WCURRENT = $00000010;
  ACF_OLD_LCURRENT = $00000020;
  ACF_OLD_WPARHEX  = $00000040;
  ACF_OLD_LPARHEX  = $00000080;

  ACF_OLD_WRESULT  = $00010000;
  ACF_OLD_LRESULT  = $00020000;
  ACF_OLD_WPARAM   = $00040000;
  ACF_OLD_LPARAM   = $00080000;
  ACF_OLD_WSTRUCT  = $00100000;
  ACF_OLD_LSTRUCT  = $00200000;

  ACF_OLD_STRING   = $00000800;
  ACF_OLD_UNICODE  = $00001000;
  ACF_OLD_STRUCT   = $00008000;

const
  opt_service  = 'service';
  opt_flags2   = 'flags2';
  opt_wparam   = 'wparam';
  opt_lparam   = 'lparam';
const
  ioService   = 'service';
  ioType      = 'type';
  ioResult    = 'result';
  ioCurrent   = 'current';
  ioParam     = 'param';
  ioStruct    = 'struct';
  ioValue     = 'value';
  ioNumber    = 'number';
  ioUnicode   = 'unicode';
  ioVariables = 'variables';
  ioWParam    = 'WPARAM';
  ioLParam    = 'LPARAM';
  ioOutput    = 'OUTPUT';
  ioFree      = 'free';
  ioAnsi      = 'ansi';
  ioInt       = 'int';

type
  tServiceAction = class(tBaseAction)
  private
    service:tServiceValue;
  public
    constructor Create(uid:dword);
    destructor Destroy; override;

    function  DoAction(var WorkData:tWorkData):LRESULT; override;
    procedure Save(node:pointer;fmt:integer); override;
    procedure Load(node:pointer;fmt:integer); override;
  end;

//----- Support functions -----

//----- Object realization -----

constructor tServiceAction.Create(uid:dword);
begin
  inherited Create(uid);
end;

destructor tServiceAction.Destroy;
begin
  ClearServiceValue(service);

  inherited Destroy;
end;

function tServiceAction.DoAction(var WorkData:tWorkData):LRESULT;
var
  subst:tSubstData;
begin
  result:=0;

  subst.Parameter :=WorkData.Parameter;
  subst.LastResult:=WorkData.LastResult;
  case WorkData.ResultType of
    rtInt : subst.ResultType:=ACF_TYPE_NUMBER;
    rtWide: subst.ResultType:=ACF_TYPE_UNICODE;
{!!
    rtAnsi:
    rtUTF8:
}
  end;

  if ExecuteService(service,subst) then
  begin
    ClearResult(WorkData);

  // result type processing
    case subst.ResultType of
      ACF_TYPE_UNICODE: begin
        WorkData.ResultType:=rtWide;
        StrDupW(pWideChar(WorkData.LastResult),pWideChar(subst.LastResult));
      end;

      ACF_TYPE_NUMBER: begin
        WorkData.ResultType:=rtInt;
        WorkData.LastResult:=subst.LastResult;
      end;
    end;

    ClearSubstData(subst);
  end;
end;

procedure LoadParam(section:PAnsiChar;flags:dword; var param:pointer);
begin
  case flags and ACF_TYPE_MASK of
    ACF_TYPE_NUMBER,
    ACF_TYPE_STRING,
    ACF_TYPE_UNICODE: param:=DBReadUnicode(0,DBBranch,section,nil);
    ACF_TYPE_STRUCT : param:=DBReadUTF8   (0,DBBranch,section,nil);
  end;
end;

procedure tServiceAction.Load(node:pointer;fmt:integer);
var
  section:array [0..127] of AnsiChar;
  buf:array [0..31] of WideChar;
  pc:pAnsiChar;
  tmp:pWideChar;
  lflags,lflags2:dword;
begin
  inherited Load(node,fmt);

  case fmt of
    0: begin
      pc:=StrCopyE(section,pAnsiChar(node));

      StrCopy(pc,opt_flags2 ); service.l_flags:=DBReadDword(0,DBBranch,section,dword(-1));
      StrCopy(pc,opt_service);
      if service.l_flags=dword(-1) then
      begin
        LoadServiceValue(service,DBBranch,section);
      end
      else
      begin
        service.service:=DBReadString(0,DBBranch,section,nil);
        service.flags  :=ConvertResultFlags(flags);
        service.w_flags:=ConvertParamFlags (flags);
        service.l_flags:=ConvertParamFlags (service.l_flags);
        if (flags and ACF_SCRIPT_SERVICE)<>0 then
          service.flags:=service.flags or ACF_FLAG_SCRIPT;

        StrCopy(pc,opt_wparam); LoadParam(section,service.w_flags,pointer(service.wparam));
        StrCopy(pc,opt_lparam); LoadParam(section,service.l_flags,pointer(service.lparam));
      end;
    end;

    100: begin
      pc:=StrCopyE(section,pAnsiChar(node));

      StrCopy(pc,opt_service); service.service:=DBReadString(0,DBBranch,section,nil);
      StrCopy(pc,opt_flags2 ); service.l_flags:=DBReadDword (0,DBBranch,section);


      if (flags and (ACF_OLD_WCURRENT or ACF_OLD_WRESULT or ACF_OLD_WPARAM))=0 then
      begin
        StrCopy(pc,opt_wparam);

        if (flags and ACF_OLD_WSTRUCT)<>0 then
          service.wparam:=PWideChar(DBReadUTF8(0,DBBranch,section,nil))
        else if ((flags and ACF_OLD_WPARNUM)=0) or ((service.l_flags and ACF2_SRV_WPAR)<>0) then
          service.wparam:=DBReadUnicode(0,DBBranch,section,nil)
        else
          StrDupW(PWideChar(service.wparam),IntToStr(buf,DBReadDWord(0,DBBranch,section)));
      end;

      if (flags and (ACF_OLD_LCURRENT or ACF_OLD_LRESULT or ACF_OLD_LPARAM))=0 then
      begin
        StrCopy(pc,opt_lparam);

        if (flags and ACF_OLD_LSTRUCT)<>0 then
          service.lparam:=PWideChar(DBReadUTF8(0,DBBranch,section,nil))
        else if ((flags and ACF_OLD_LPARNUM)=0) or ((service.l_flags and ACF2_SRV_LPAR)<>0) then
          service.lparam:=DBReadUnicode(0,DBBranch,section,nil)
        else
          StrDupW(PWideChar(service.lparam),IntToStr(buf,DBReadDWord(0,DBBranch,section)));
      end;

      lflags :=flags;
      lflags2:=service.l_flags;
      flags :=flags and not ACF_MASK;

      service.flags  :=0;
      service.w_flags:=0;
      service.l_flags:=0;

      if      (lflags  and ACF_OLD_WPARNUM )<>0 then service.w_flags:=ACF_TYPE_NUMBER
      else if (lflags  and ACF_OLD_WUNICODE)<>0 then service.w_flags:=ACF_TYPE_UNICODE
      else if (lflags  and ACF_OLD_WCURRENT)<>0 then service.w_flags:=ACF_TYPE_CURRENT
      else if (lflags  and ACF_OLD_WRESULT )<>0 then service.w_flags:=ACF_TYPE_RESULT
      else if (lflags  and ACF_OLD_WPARAM  )<>0 then service.w_flags:=ACF_TYPE_PARAM
      else if (lflags  and ACF_OLD_WSTRUCT )<>0 then service.w_flags:=ACF_TYPE_STRUCT
      else                                           service.w_flags:=ACF_TYPE_STRING;
      if (lflags2 and ACF2_SRV_WPAR)<>0 then
        service.w_flags:=service.w_flags or ACF_FLAG_SCRIPT;

      if      (lflags  and ACF_OLD_LPARNUM )<>0 then service.l_flags:=ACF_TYPE_NUMBER
      else if (lflags  and ACF_OLD_LUNICODE)<>0 then service.l_flags:=ACF_TYPE_UNICODE
      else if (lflags  and ACF_OLD_LCURRENT)<>0 then service.l_flags:=ACF_TYPE_CURRENT
      else if (lflags  and ACF_OLD_LRESULT )<>0 then service.l_flags:=ACF_TYPE_RESULT
      else if (lflags  and ACF_OLD_LPARAM  )<>0 then service.l_flags:=ACF_TYPE_PARAM
      else if (lflags  and ACF_OLD_LSTRUCT )<>0 then service.l_flags:=ACF_TYPE_STRUCT
      else                                           service.l_flags:=ACF_TYPE_STRING;
      if (lflags2 and ACF2_SRV_LPAR)<>0 then
        service.l_flags:=service.l_flags or ACF_FLAG_SCRIPT;

      if      (lflags  and ACF_OLD_STRING )<>0 then service.flags:=ACF_TYPE_STRING
      else if (lflags  and ACF_OLD_UNICODE)<>0 then service.flags:=ACF_TYPE_UNICODE
      else if (lflags  and ACF_OLD_STRUCT )<>0 then service.flags:=ACF_TYPE_STRUCT
      else                                          service.flags:=ACF_TYPE_NUMBER;
      if (lflags2 and ACF2_FREEMEM )<>0 then service.flags:=service.flags or ACF_FLAG_FREEMEM;
      if (lflags2 and ACF2_SRV_SRVC)<>0 then service.flags:=service.flags or ACF_FLAG_SCRIPT;
    end;
  end;
end;

procedure tServiceAction.Save(node:pointer;fmt:integer);
var
  section: array [0..127] of AnsiChar;
  pc:pAnsiChar;
begin
  inherited Save(node,fmt);

  case fmt of
    0: begin
      pc:=StrCopyE(section,pAnsiChar(node));

      StrCopy(pc,opt_service);
      SaveServiceValue(service,DBBranch,section);
    end;

    13: begin
    end;
  end;
end;

//----- Dialog realization -----
{
function EnableThemeDialogTexture(hwnd: HWND; dwFlags: DWORD): HRESULT; stdcall;
external 'uxtheme.dll' name 'EnableThemeDialogTexture';

function IsThemeDialogTextureEnabled(hwnd: HWND): BOOL; stdcall;
external 'uxtheme.dll' name 'IsThemeDialogTextureEnabled';
}
function DlgProc(Dialog:HWND;hMessage:uint;wParam:WPARAM;lParam:LPARAM):LRESULT; stdcall;
var
  ServiceBlock:HWND;
  rc:TRECT;
begin
  result:=0;

  case hMessage of
    WM_DESTROY: begin
    end;

    WM_INITDIALOG: begin
      GetClientRect(Dialog,rc);
      ServiceBlock:=CreateServiceBlock(Dialog,0,0,rc.right,rc.bottom,
          ACF_BLOCK_NOVISUAL or ACF_BLOCK_EXPAND);
      SetWindowLongPtrW(Dialog,GWLP_USERDATA,ServiceBlock);
      SetServiceListMode(ServiceBlock,DBReadByte(0,DBBranch,'SrvListMode'));
{
      b:=IsThemeDialogTextureEnabled(Dialog);
      if b then
      begin
        b:=IsThemeDialogTextureEnabled(ServiceBlock);
        if not b then
          EnableThemeDialogTexture(ServiceBlock,2);
      end;
}
      TranslateDialogDefault(Dialog);
    end;

    WM_ACT_SETVALUE: begin
      ServiceBlock:=GetWindowLongPtrW(Dialog,GWLP_USERDATA);
      SetSrvBlockValue(ServiceBlock,tServiceAction(lParam).service);
    end;

    WM_ACT_RESET: begin
      ClearServiceBlock(GetWindowLongPtrW(Dialog,GWLP_USERDATA));
    end;

    WM_ACT_SAVE: begin
      with tServiceAction(lParam) do
      begin
        ServiceBlock:=GetWindowLongPtrW(Dialog,GWLP_USERDATA);
        GetSrvBlockValue(ServiceBlock,tServiceAction(lParam).service);
      end;
    end;

    //??
    WM_SHOWWINDOW: begin
{
      // hide window by ShowWindow function
      if (lParam=0) and (wParam=0) then
      begin
        pc:=pAnsiChar(SetWindowLongPtrW(GetDlgItem(Dialog,IDC_WSTRUCT),GWLP_USERDATA,0));
        mFreeMem(pc);
        pc:=pAnsiChar(SetWindowLongPtrW(GetDlgItem(Dialog,IDC_LSTRUCT),GWLP_USERDATA,0));
        mFreeMem(pc);
      end;
}
    end;

    WM_COMMAND: begin
      case wParam shr 16 of
        CBN_EDITCHANGE,
        BN_CLICKED:
          SendMessage(GetParent(GetParent(Dialog)),PSM_CHANGED,0,0);
      end;
    end;

    WM_HELP: begin
      ServiceBlock:=GetWindowLongPtrW(Dialog,GWLP_USERDATA);
      SendMessage(ServiceBlock,WM_HELP,0,0);

      result:=1;
    end;

  end;
end;

//----- Export/interface functions -----

var
  vc:tActModule;

function CreateAction:tBaseAction;
begin
  result:=tServiceAction.Create(vc.Hash);
end;

function CreateDialog(parent:HWND):HWND;
begin
  result:=CreateDialogW(hInstance,'IDD_ACTSERVICE',parent,@DlgProc);
end;

procedure Init;
begin
  vc.Next    :=ModuleLink;

  vc.Name    :='Service';
  vc.Dialog  :=@CreateDialog;
  vc.Create  :=@CreateAction;
  vc.Icon    :='IDI_SERVICE';
  vc.Hash    :=0;

  ModuleLink :=@vc;
end;

begin
  Init;
end.
