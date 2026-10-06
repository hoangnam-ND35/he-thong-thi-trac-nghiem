' Chay server phong thi o che do nen (khong cua so CMD).
' Dong cua so nay khong tat server.

Option Explicit
Dim shell, fso, root, exe, wmi, procs, p, already

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)
exe = root & "\build\online_exam.exe"

If Not fso.FileExists(exe) Then
  shell.Run """" & root & "\build.bat""", 1, True
End If
If Not fso.FileExists(exe) Then
  MsgBox "Chua bien dich duoc server. Mo build.bat de xem loi.", 16, "Phong thi truc tuyen"
  WScript.Quit 1
End If

already = False
Set wmi = GetObject("winmgmts:\\.\root\cimv2")
Set procs = wmi.ExecQuery("Select * from Win32_Process Where Name='online_exam.exe'")
For Each p In procs
  already = True
  Exit For
Next

If Not already Then
  shell.CurrentDirectory = root
  shell.Run """" & exe & """", 0, False
  WScript.Sleep 1200
End If
