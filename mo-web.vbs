' Bat server nen (neu chua chay) roi mo Chrome toi trang web.
' Khong can run.bat. Dong Chrome khong tat server.

Option Explicit
Dim shell, fso, root, url, chrome, edge

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)
url = "http://127.0.0.1:8080/"

shell.Run "wscript.exe """ & root & "\start-server.vbs""", 0, True

chrome = ""
If fso.FileExists("C:\Program Files\Google\Chrome\Application\chrome.exe") Then
  chrome = "C:\Program Files\Google\Chrome\Application\chrome.exe"
ElseIf fso.FileExists("C:\Program Files (x86)\Google\Chrome\Application\chrome.exe") Then
  chrome = "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"
End If

If chrome <> "" Then
  shell.Run """" & chrome & """ --new-window """ & url & """", 1, False
Else
  edge = "msedge"
  On Error Resume Next
  shell.Run edge & " """ & url & """", 1, False
  If Err.Number <> 0 Then shell.Run "explorer.exe """ & url & """", 1, False
  On Error Goto 0
End If
