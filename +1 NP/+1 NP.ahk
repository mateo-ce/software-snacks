; MCE, 2020
#SingleInstance Force
#NoTrayIcon

>^>+Enter::OnKeyPress("NumpadEnter")
>^>+Backspace::OnKeyPress("NumpadDel")
>^>+=::OnKeyPress("NumpadAdd")
>^>+-::OnKeyPress("NumpadSub")
>^>+.::OnKeyPress("NumpadDot")

>^>+Up::OnKeyPress("NumpadUp")
>^>+Down::OnKeyPress("NumpadDown")
>^>+Left::OnKeyPress("NumpadLeft")
>^>+Right::OnKeyPress("NumpadRight")

>^>+0::OnNumPress("Numpad0")
>^>+1::OnNumPress("Numpad1")
>^>+2::OnNumPress("Numpad2")
>^>+3::OnNumPress("Numpad3")
>^>+4::OnNumPress("Numpad4")
>^>+5::OnNumPress("Numpad5")
>^>+6::OnNumPress("Numpad6")
>^>+7::OnNumPress("Numpad7")
>^>+8::OnNumPress("Numpad8")
>^>+9::OnNumPress("Numpad9")

#if (GetKeyState("CapsLock", "T"))
	Up::OnKeyPress("NumpadUp")
	Down::OnKeyPress("NumpadDown")
	Left::OnKeyPress("NumpadLeft")
	Right::OnKeyPress("NumpadRight")

	0::OnNumPress("Numpad0")
	1::OnNumPress("Numpad1")
	2::OnNumPress("Numpad2")
	3::OnNumPress("Numpad3")
	4::OnNumPress("Numpad4")
	5::OnNumPress("Numpad5")
	6::OnNumPress("Numpad6")
	7::OnNumPress("Numpad7")
	8::OnNumPress("Numpad8")
	9::OnNumPress("Numpad9")
return

OnKeyPress(nk) {
	SetNumLockState, off
	Send, {%nk%}
}

OnNumPress(nk) {
	SetNumLockState, on
	Send, {%nk%}
}