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
- `/user username`   : Set your username for SMP
- `/pwd password`    : Set your password for SMP
   
# Prepareation for Windows
0. Get releases for Windows.
1. Download gvsbuild ( https://github.com/wingtk/gvsbuild ), please choose releases,
   not cloning repo.   
2. Copy all DLL files in `gvsbuild\bin` to the directory having game executable.
   (You also can add `gvsbuild/bin` to PATH instead of copying DLLs. If you want to debug
   , you have to.)   
3. Download Microsoft Visual C++ Redistributable   
   ( https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170 )   
   and install.   
4. Double click game executable and enjoy.
   
# Prepareation for linux
1. You only need gtk4. Maybe most linux already have it.
   
# Building in Windows (Cygwin + gcc)
Cygwin build option is removed.   
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
