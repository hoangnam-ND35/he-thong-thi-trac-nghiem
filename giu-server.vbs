' Giu server luon song: kiem tra moi 20 giay, tat thi bat lai.
' Chay an (khong cua so). Dong Chrome khong anh huong.

Option Explicit
Dim shell, fso, root

Set shell = CreateObject("WScript.Shell")
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(WScript.ScriptFullName)

Do
  shell.Run "wscript.exe """ & root & "\start-server.vbs""", 0, True
  WScript.Sleep 20000
Loop
