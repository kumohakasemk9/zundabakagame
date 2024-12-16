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
   
# Prepareation
1. Library needed: `cairo` `x11` `pangocairo`
2. `git clone https://github.com/kumohakasemk9/zundabakagame.git`
3. `cd zundabakagame-master`
4.	`make`
5. `./zundagame`
   
# Thanks
- You
- cairo https://www.cairographics.org/
- Xlib https://x.org/releases/current/doc/libX11/libX11/libX11.html
- pangocairo https://docs.gtk.org/PangoCairo/
- Zundamon (C) 2024 ＳＳＳ合同会社 https://zunko.jp/
- Zundamon (Image) (C) 2024 https://twitter.com/sakamoto_ahr 坂本アヒル
- Adwaita Icon Theme https://github.com/GNOME/adwaita-icon-theme

only adwaita legacy and programs are licensed by CC,   
for other resources, do not redistribute or use out of game without perm    

#日本語
https://github-com.translate.goog/kumohakasemk9/zundabakagame?_x_tr_sl=auto&_x_tr_tl=ja&_x_tr_hl=ja&_x_tr_pto=wapp
