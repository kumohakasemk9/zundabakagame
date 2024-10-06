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
   
# Prepareation for Windows
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
1. Prepare cygwin, install `make` `gcc-g++` `gdb`. `gdb` is needed if you want to debug.
   Please add cygwin's `bin` directory to your path, too.   
2. Extract gvsbuild to directory named `gvsbuild`, move it to the directory having makefile.
3. Run `make gvsbuild`
4. You need to add cygwin directory to your PATH or you will get cygwin1.dll not found error when
   you tried to run executable. 
   
If you installed gdb and appropriate PATH is set, you can already debug the game     
by typing `gdb gvsbuild-zundamon.exe` from your cmd.exe after building game.    

# Debugging with IDE (hints only, I hate IDE :/ )
- Append appropriate include directories. You can get needed directories by
   typing `gvsbuild/bin/pkg-config gtk4 --cflags` .   
   For now, needed directories are:   
   
```
 "gvsbuild/include/gtk-4.0",
 "gvsbuild/include/pango-1.0",
 "gvsbuild/include/fribidi",
 "gvsbuild/include/harfbuzz",
 "gvsbuild/include/gdk-pixbuf-2.0",
 "gvsbuild/include/cairo",
 "gvsbuild/include/freetype2",
 "gvsbuild/include/libpng16",
 "gvsbuild/include/pixman-1",
 "gvsbuild/include/graphene-1.0",
 "gvsbuild/lib/graphene-1.0/include",
 "gvsbuild/include",
 "gvsbuild/include/glib-2.0",
 "gvsbuild/lib/glib-2.0/include"
```

- If your IDE does not support makefile, Append appropriate external libraries.
  You can get required external libraries by typing `gvsbuild/bin/pkg-config gtk4 --libs` .   
- If your IDE does not support makefile, Linker should load `gvsbuild/lib`
  as external library directory. Configre your IDE to do it.   
  In VisualStudio, please put `.lib` in each entries.   
- If your IDE supports makefile, makefile will do library importing automatically.   
- Turn off all warnings and code security checks, if there is no makefile support.    
   
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
