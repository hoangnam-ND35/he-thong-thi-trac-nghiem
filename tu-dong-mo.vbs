' Khi dang nhap Windows: bat server + mo Chrome + giu server luon chay.

Option Explicit
Dim shell, fso, root, wmi, procs, p, watching

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)

shell.Run "wscript.exe """ & root & "\start-server.vbs""", 0, True
WScript.Sleep 1200
shell.Run "wscript.exe """ & root & "\mo-web.vbs""", 0, False

watching = False
Set wmi = GetObject("winmgmts:\\.\root\cimv2")
Set procs = wmi.ExecQuery("Select * from Win32_Process Where CommandLine Like '%giu-server.vbs%'")
For Each p In procs
  watching = True
  Exit For
Next
If Not watching Then
  shell.Run "wscript.exe """ & root & "\giu-server.vbs""", 0, False
End If
