Option explicit

Dim sh
Dim key
Dim path
Dim checkPath
Dim actiondata
Dim action
Dim snappath
Dim snappathlen
Dim idx
Dim args

' Parse the installer custom data, action - space - target directory

actiondata = Session.Property("CustomActionData")
snappath = ""
action = ""

if Len(actiondata) > 0 then 

    idx = instr(actiondata," ")
    if idx > 1 then action = LCase(Left(actiondata,idx-1))
    
    if idx > 0 then snappath = Mid(actiondata,idx+1) else snappath=""
    
    ' Installer TARGETDIR property includes a trailing backslash
    if Len(snappath) > 1 then snappath = Left(snappath,Len(snappath)-1)
end if

if snappath <> "" then

    Set Sh = CreateObject("WScript.Shell")

    ' Registry key for the PATH environment variable

    key = "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment\Path"

    path = Sh.RegRead(key)

    ' Get rid of snappath from the existing path ... assume that it won't
    ' be there more than once..

    snappathlen = Len(snappath)
    if LCase(path) = LCase(snappath) then path = ""
    if LCase(left(path,snappathlen+1)) = LCase(snappath)+";" then path = mid(path,snappathlen+2)
    if LCase(right(path,snappathlen+1)) = ";"+LCase(snappath) then path = left(path,Len(path)-snappathlen-1)

    idx = instr(1,path,";"+snappath+";",1)
    if idx > 0 then path = left(path,idx) + mid(path,idx+snappathlen+2)

    ' Add SNAP directory to the path if installing

    if action = "install" then path = snappath + ";" + path

    ' Write the key

    Sh.RegWrite key, path, "REG_SZ"
   
end if




