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
- `q/w/e`   : skill (unimplemented)
   
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
- MSVC can not process code to establish variable length buffer on
  stack. and some code must be fixed.    
   
# Thanks
- You
- Gtk4 https://www.gtk.org/
- Zundamon (C) 2024 ＳＳＳ合同会社 https://zunko.jp/
- Zundamon (Image) (C) 2024 https://twitter.com/sakamoto_ahr 坂本アヒル
- Adwaita Icon Theme https://github.com/GNOME/adwaita-icon-theme
- gvsbuild https://github.com/wingtk/gvsbuild

only adwaita legacy and programs are licensed by CC,   
do not redistribute or use out of game without perm    

# にほんご

#ズンダモンゲーム
Copyright (C) 2024 Kumohakase https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0   
https://ko-fi.com/kumohakase (kofi) や     
https://www.patreon.com/kumohakasemk8 (pateron) を通じて私を   
サポートすることを検討してください   
kumohakaseは、このソフトウェアパッケージを使用した場合の   
データの損失/エラーについて責任を負いません。   

# オペレーション 
- `T`       : チャット 
- `/`       ：コマンド
- `A/S`      : アイテム切り替え
- `D`        : アイテム使用
- `スペース` : 移動on off
- `J/K/L/I`  : MOVE (未実装) 
- `Q/W/E`    : スキル(未実装)

(1)マウスホイールでもアイテムを循環させることができます。   
(2)アイテムは右クリックでもご利用いただけます。   
(3)左クリックでも動きをon offすることができます   

# コマンドヘルプ
- `/en`              : 英語に切り替える
- `/jp`              : 日本語に切り替えます
- `/chfont fontname` : fontnameで指定されたフォントリストの読み込み、「,」で区切られる。
- `/version`         : バージョン文字列を表示する
- `/credit`          : クレジット文字列を表示
- `/reset`           : ゲームリセット

# Windowsの準備
1. gvsbuild( https://github.com/wingtk/gvsbuild )をダウンロードします  
   リリースビルドをダウンロードしてください。   
2. `gvsbuild\bin`内のすべてのDLLファイルを、ゲームの実行可能ファイルが
    あるディレクトリにコピーします。    
    (DLL をコピーする代わりに `gvsbuild/bin` を PATH    
    に追加することもできます。しかしもしデバッグしたいならPATHに追加してください)   
3. Microsoft Visual C++ 再頒布可能パッケージのダウンロード   
   (https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)   
   インストールします。   
4. ゲームの実行可能ファイルをダブルクリックしてお楽しみください。

# linux の準備
1. gtk4 だけが必要です。たぶん、ほとんどのLinuxはすでにそれを持っています。
   
# Windowsでのビルド (Cygwin)
1. cygwinを準備します。 `make` `gcc-g++` `gdb` ( `gdb` はデバッグしたいなら)   
   をインストールしてください。cygwinの `bin` ディレクトリをPATHに追加すること   
   も忘れないでください。   
2. gvsbuildを `gvsbuild` という名前のディレクトリに展開し、makefileがあるディレクトリに移動します。
3. `make gvsbuild` を実行します。
4. cygwinディレクトリをPATHに追加しないと、実行した時にcygwin1.dll見つかりませんというエラー   
  が表示されるでしょう。   
   
gdbが入ってPATHが適切にセットされていれば、ゲームをビルドしたのちに、    
`gdb gvsbuild-zundagame.exe` と打てばコマンドラインからデバッグできます。    

# IDEによるデバッグ(ヒントのみ、私はIDEが嫌いです :/)
- 適切なインクルードディレクトリを追加します。必要なディレクトリは、次の方法で取得できます    
  `gvsbuild/bin/pkg-config gtk4 --cflags` と入力します。    
  今のところ、必要なディレクトリは次のとおりです。   
 
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
 
- IDE が makefile をサポートしていない場合は、適切な外部ライブラリを追加します。    
  必要な外部ライブラリを取得するには、`gvsbuild/bin/pkg-config --libs` と入力します。    
- IDE が makefile をサポートしていない場合、リンカは `gvsbuild/lib` をロードする必要   
  があります 外部ライブラリディレクトリとして。それを行うようにIDEを構成します。   
- VisualStudioでは、各ライブラリの最後に `.lib` を入れてください。   
- IDEがmakefileをサポートしている場合、makefileは自動的にライブラリのインポートを行います。   
- すべての警告とコードのセキュリティチェックをオフにします (Makefile がサポートされていない場合)。  
- MSVC は、可変長バッファーをスタックに積むコードを処理できません
  よって、一部のコードを修正する必要があります。    

# ありがとう 
- あなた
- Gtk4 https://www.gtk.org/
- ずんだもん (C) 2024 SSS合同会社 https://zunko.jp/ 
- ずんだもん (イメージ) (C) 2024 https://twitter.com/sakamoto_ahr 坂本アヒル
- Adwaitaアイコンテーマ https://github.com/GNOME/adwaita-icon-theme
- gvsbuild https://github.com/wingtk/gvsbuild
- duckduckgo翻訳システム https://duckduckgo.com/

プログラムとadwaitalegacy以外はCCライセンスの適用外です。     
許可なく再頒布やゲーム外使用をしないでください   
