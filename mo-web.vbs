' Mo Chrome toi trang phong thi.
' Tu bat server neu chua chay.

Option Explicit
Dim shell, fso, root, url, chrome

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)
url = "http://127.0.0.1:8080/login.html"

shell.Run "wscript.exe """ & root & "\start-server.vbs""", 0, True
WScript.Sleep 800

chrome = ""
If fso.FileExists("C:\Program Files\Google\Chrome\Application\chrome.exe") Then
  chrome = "C:\Program Files\Google\Chrome\Application\chrome.exe"
ElseIf fso.FileExists("C:\Program Files (x86)\Google\Chrome\Application\chrome.exe") Then
  chrome = "C:\Program Files (x86)\Google\Chrome\Application\chrome.exe"
End If

If chrome <> "" Then
  shell.Run """" & chrome & """ """ & url & """", 1, False
Else
  shell.Run "explorer.exe """ & url & """", 1, False
End If
