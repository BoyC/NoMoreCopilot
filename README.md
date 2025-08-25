A couple months ago I spent weeks to find a cheap laptop that fulfills my needs.

I was very happy with my new purchase, right until I fired up Visual Studio and tried to navigate around the code and was greeted with the damn copilot popup.

Ok I thought, there must be a solution to this, but all I could find were powertoys hooks and other macro based solutions that didn't take into account a key issue: on my Asus TUF A14 (and presumably on other similar machines as well) the copilot button emits a WIN+SHIFT+F23 keypress, and anything that catches that in user mode swallows the shift signal with it. This means no more shift+ctrl+arrow key selection of code for me. If you're a programmer you'll know how frustrating it is to have random keybinds you're used to suddenly not work on a new system. There had to be a better way.

So I present a small keyboard driver that catches the lwin+lshift+f23 chord and replaces it with the right ctrl. Note that this was only tested on my laptop and since this is a driver and since I'm not a Microsoft certified anybody if you want to make use of this you'll need to build and sign it for yourself. During the development of this I had several instances where after a reboot the keyboard on my laptop wouldn't work at all (mostly having to do with me completely replacing the keyboard driver instead of chaining the new driver as a filter) so there's no promises that this will work on your system, but tbh ChatGPT is pretty helpful in working stuff like that out. The first draft in the first commit was written as a POC by an llm and after I knew this was possible I completely rewrote the logic so it'd actually be functional.

If you're experienced in driver development feel free to send a pull request my way that'd make this thing more universal and actually be helpful to more people.

Anyway. NO. MORE. COPILOT.
