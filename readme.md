# Zundamon game
Copyright (C) 2024 Kumohakase    
Program lisenced by https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0      
Please consider supporting me through 
kofi: https://ko-fi.com/kumohakase or    
patreon: https://www.patreon.com/kumohakasemk8     
Kumohakase won't be responsible for any data loss/error by using this software package.      
   
# Operation
- `t`       : chat
- `/`       : command
- `a/s`     : cycle items
- `d`       : use item
- `space`   : switch movement
- `j/k/l/i` : move (unimplemented)
- `q/w/e`   : skill
   
(1) You can cycle items also by mouse wheel.   
(2) You can use item also by right click.   
(3) You can switch movement also by left click.   
   
# Command Help
- `/en`              : Switch to english
- `/jp`              : Switch to japanese
- `/chfont fontname` : load fontlist separated by ","
- `/version`         : Show version string
- `/credit`          : Show credit string
- `/reset`           : Round reset
- `/smp id`          : Set your SMP connection profile id. Used when connect. id will start from 1       
- `/getsmps`         : Get total loaded SMP connection profile count.   
- `/getsmp`          : Show current selected SMP connection profile.  
- `/connect`         : Connect to SMP server using selected profile.   
- `/disconnect`      : Disconnect from SMP server.  

# About SMP   
SMP support is still in development      
You may experience very insane de-synced game like object   
pops out of nowhere, etc ....   Before doing SMP, you need to   
modify `credentials.txt` in the game directory. Please write one profile   
in one line, this can hold multiple profile (the file can be multi-lined),   
and you can change profile by `/smp` command.   
Profile id is associated in reading order. It means the profile written in    
the first line gets id 1. The format is:   
```
hostname<tab>portname<tab>username<tab>password<lf>

Example:
127.0.0.1<tab>25566<tab>kumohakase403<tab>kumokumo<lf>
```
Each information is splitted by tab letters, ensure new line    
comes at last. (Do not eorry if you are in windows, in windows code, cr
is recognized as newline instead.)   
   
# Prepareation for Windows
0. Get releases for Windows. I would pack all of required dlls inside releases.
1. Download Microsoft Visual C++ Redistributable   
   ( https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170 )   
   and install.   
2. Double click game executable and enjoy.
    
If dlls are missing, please download dlls from gvsbuild and cygwin   
then copy all the dlls to the game directory (or specify dll path into PATH)   
gvsbuild: https://github.com/wingtk/gvsbuild/releases (You need gtk4)
cygwin: https://cygwin.com/   
(install without no additional software, you can obtain `cygwin1.dll` from `C:\cygwin64\bin`)
   
# Prepareation for linux
1. You only need gtk4. Maybe most linux already have it.
   
# Building in Windows (Cygwin + gcc)
If you want to build in cygwin, install `gcc` and `make`    
Then copy and edit `makefile_windows` for it (like include path and compiler name)     

# Debugging in Windows (hints only)
- This project is made in linux, and debugging in linux is recommended. You can use gdb.   
- If you want to debug it in Windows, Microsoft Visual Studio + MSVC may be your choice.   
- Append appropriate include directories. You can get needed directories by
  typing `gvsbuild/bin/pkg-config gtk4 --cflags`. `makefile_windows` has big hint to pathname
  of those additional files.   
- Append appropriate external libraries too.   
  You can get required external libraries by typing `gvsbuild/bin/pkg-config gtk4 --libs` . 
- You will probably get some compile error due to MSVC restrictions or function   
  definition difference between linux and win, please fix it yourself.  
   
# Thanks
- You
- Gtk4 https://www.gtk.org/
- Zundamon (C) 2024 ＳＳＳ合同会社 https://zunko.jp/
- Zundamon (Image) (C) 2024 https://twitter.com/sakamoto_ahr 坂本アヒル
- Adwaita Icon Theme https://github.com/GNOME/adwaita-icon-theme
- gvsbuild https://github.com/wingtk/gvsbuild

only adwaita legacy and programs are licensed by CC,   
for other resources, do not redistribute or use out of game without perm    

#日本語
https://github-com.translate.goog/kumohakasemk9/zundabakagame?_x_tr_sl=auto&_x_tr_tl=ja&_x_tr_hl=ja&_x_tr_pto=wapp
