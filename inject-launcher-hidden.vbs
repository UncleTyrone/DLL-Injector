' Double-click this to run inject-launcher.bat with NO visible console window.
' Use this one for silent background / startup use.
Dim shell, here
Set shell = CreateObject("WScript.Shell")
here = Left(WScript.ScriptFullName, InStrRev(WScript.ScriptFullName, "\"))
shell.Run """" & here & "inject-launcher.bat""", 0, False
