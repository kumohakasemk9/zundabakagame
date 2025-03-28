/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

main.h: integrated header file
*/

#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stddef.h>

#include "zunda-defs.h"
#include "network.h"
#include "zunda-structs.h"

#ifdef __WASM
	#define WASMIMPORT extern
#else
	#define WASMIMPORT
#endif

//main.c
void detect_syslang();
WASMIMPORT int32_t make_tcp_socket(char*, char*);
WASMIMPORT int32_t close_tcp_socket();
WASMIMPORT ssize_t send_tcp_socket(void*, size_t);
ssize_t recv_tcp_socket(void*, size_t);
int32_t isconnected_tcp_socket();
int16_t network2host_fconv_16(uint16_t);
int32_t network2host_fconv_32(uint32_t);
uint16_t host2network_fconv_16(int16_t);
uint32_t host2network_fconv_32(int32_t);
int32_t compute_passhash(char*, char*, uint8_t*, uint8_t*);
void warn(const char*, ...);
void info(const char*, ...);
void fail(const char*, ...);
void vfail(const char*, va_list);
void textinput_on_cb();
void textinput_off_cb();
void clipboard_readstart_cb();

//gamesys.c
void gametick();
int32_t gameinit(char*);
void do_finalize();
void keypress_handler(char, specialkey_t);
void mousemotion_handler(int, int);
void keyrelease_handler(char);
void mousepressed_handler(mousebutton_t);
void chatf(const char*, ...);
void chat(char*);
void set_cmdstatus(int32_t);
int32_t spawn_playable(int32_t);
void reset_game();
void use_skill(int32_t, int32_t, PlayableInfo_t);
void getlocalcoord(int32_t, double*, double*);
int32_t insert_cmdbuf(char*);
void map2local(double, double, double*, double*);
void spawn_playable_me();

//util.c
double scale_number(double, double, double);
int32_t is_range_number(double, double, double);
void utf8_substring(char*, int32_t, int32_t, char*, int32_t);
void die(const char*, ...);
int32_t utf8_strlen(char*);
int32_t is_range(int32_t, int32_t, int32_t);
int32_t randint(int32_t, int32_t);
double constrain_number(double, double, double);
char *utf8_strlen_to_pointer(char*, int32_t);
int32_t utf8_insertstring(char*, char*, int32_t, size_t);
int32_t constrain_i32(int32_t, int32_t, int32_t);
double get_current_time_ms();

//info.c
int32_t lookup(obj_type_t, LookupResult_t*);
int32_t lookup_playable(int32_t, PlayableInfo_t*);
int32_t is_playable_character(obj_type_t);
const char *get_localized_string(int32_t);
const char *get_localized_deathreason(int32_t);
const char *get_localized_menustring(int32_t);
const char *get_localized_titlestring(int32_t);
void check_data();
const char *get_localized_charactername(int32_t);
const char *get_localized_itemdesc(int32_t);
const char *get_localized_bool(int32_t);
int32_t get_skillcooldown(obj_type_t, int32_t);
int32_t get_skillinittimer(obj_type_t, int32_t);
int32_t get_skillrange(obj_type_t, int32_t);
int32_t get_localized_helpimgid(int32_t);

//network.c
void network_recv_task();
void process_smp();
void stack_packet(event_type_t, ...);
int32_t connect_server_cmd(char*);
void close_connection_cmd();
void close_connection(char*);
int32_t lookup_smp_player_from_cid(int32_t);
int32_t changetimeout_cmd(char*);
int32_t getcurrentsmp_cmd();
int32_t get_smp_cmd(char*);
int32_t add_smp_profile(char*, char);
int32_t add_smp_cmd(char*);
int32_t getclients_cmd();
int32_t listusermute_cmd();
int32_t togglechat_cmd();
int32_t addusermute_cmd(char*);
int32_t delusermute_cmd(char*);
int32_t addusermute(char*);
void respawn_smp_player(int32_t);

//aiproc.c
void procai();
int32_t add_character(obj_type_t, double, double, int32_t);
double get_distance_raw(double, double, double, double);

//ui.c
void game_paint();

//grapics.c
WASMIMPORT void clear_screen();
WASMIMPORT void chcolor(uint32_t, int32_t);
WASMIMPORT void restore_color();
WASMIMPORT void fillcircle(double, double, double);
WASMIMPORT void fillrect(double, double, double, double);
WASMIMPORT void hollowrect(double, double, double, double);
WASMIMPORT void draw_polygon(double, double, int32_t, double[]);
WASMIMPORT void drawline(double, double, double, double, double);
WASMIMPORT void drawimage_scale(double, double, double, double, int32_t);
WASMIMPORT void drawimage(double, double, int32_t);
WASMIMPORT void get_image_size(int32_t, double*, double*);
WASMIMPORT void drawstring(double, double, char*);
WASMIMPORT int32_t get_string_width(char*);
WASMIMPORT int32_t get_font_height();
WASMIMPORT void loadfont(char*);
WASMIMPORT void set_font_size(int32_t);

#ifndef __WASM
	int32_t init_graphics();
	void uninit_graphics();
#endif
