' Dam bao server phong thi luon chay.
' Neu bi tat (dong Cursor, tat process...) se tu bat lai.

Option Explicit
Dim shell, fso, root, exe, wmi, procs, p, already

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)
exe = root & "\build\online_exam.exe"

If Not fso.FileExists(exe) Then
  If fso.FileExists(root & "\build.bat") Then
    shell.Run """" & root & "\build.bat""", 0, True
  End If
End If
If Not fso.FileExists(exe) Then WScript.Quit 1

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
  WScript.Sleep 1500
End If
