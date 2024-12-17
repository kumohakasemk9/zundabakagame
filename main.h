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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "zunda-defs.h"
#include "zunda-server.h"
#include "zunda-structs.h"

//main.c
void detect_syslang();
int32_t make_tcp_socket(char*, char*);
int32_t close_tcp_socket();
int32_t send_tcp_socket(uint8_t*, size_t);
int32_t install_io_handler();
ssize_t recv_tcp_socket(uint8_t*, size_t);
void poll_tcp_socket();
int16_t network2host_fconv_16(uint16_t);
int32_t network2host_fconv_32(uint32_t);
uint16_t host2network_fconv_16(int16_t);
uint32_t host2network_fconv_32(int32_t);
char *compute_passhash(char*, char*, char*);

//ui.c
void game_paint();
void draw_game_main();
void draw_ui();
void draw_info();
void draw_mchotbar(double, double);
void draw_lolhotbar(double, double);
void draw_game_object(int32_t, LookupResult_t, double, double);
void draw_shapes(int32_t, LookupResult_t, double, double);

//graphics.c
void drawline(double, double, double, double, double);
void fillcircle(double, double, double);
void drawstring(double, double, char*);
void set_font_size(int32_t);
void loadfont(const char*);
void drawstringf(double, double, const char*, ...);
void drawsubstring(double, double, char*, int32_t, int32_t);
double drawstring_inwidth(double, double, char*, int32_t, int32_t);
void drawimage(double, double, int32_t);
void drawimage_scale(double, double, double, double, int32_t);
void fillrect(double, double, double, double);
void hollowrect(double, double, double, double);
void chcolor(uint32_t, int32_t);
void restore_color();
int32_t drawstring_title(double, char*, int32_t);
void draw_polygon(double, double, int32_t, double[]);
void draw_hpbar(double, double, double, double, double, double, uint32_t, uint32_t);

//util.c
double scale_number(double, double, double);
double constrain_number(double, double, double);
uint32_t constrain_ui32(uint32_t, uint32_t, uint32_t);
int32_t constrain_i32(int32_t, int32_t, int32_t);
void utf8_substring(char*, int32_t, int32_t, char*, int32_t);
int32_t utf8_insertstring(char*, char*, int32_t, size_t);
void die(const char*, ...);
int32_t get_font_height();
int32_t get_string_width(char*);
int32_t shrink_string(char*, int32_t, int32_t*);
int32_t get_substring_width(char*, int32_t, int32_t);
int32_t shrink_substring(char*, int32_t, int32_t, int32_t, int32_t*);
int32_t is_range(int32_t, int32_t, int32_t);
int32_t is_range_number(double, double, double);
int32_t randint(int32_t, int32_t);
void get_image_size(int32_t, double*, double*);
size_t utf8_get_letter_bytelen(char*);
int32_t utf8_strlen(char*);
char *utf8_strlen_to_pointer(char*, int32_t);

//gamesys.c
void read_creds();
int32_t gameinit();
void do_finalize();
void showstatus(const char*, ...);
void use_skill(int32_t, int32_t, PlayableInfo_t);
void local2map(double, double, double*, double*);
void getlocalcoord(int32_t, double*, double*);
void chat(char*);
void chatf(const char*, ...);
int32_t add_character(obj_type_t, double, double, int32_t);
double get_distance(int32_t, int32_t);
void set_speed_for_following(int32_t);
void set_speed_for_going_location(int32_t, double, double, double);
int32_t find_nearest_unit(int32_t, int32_t, facility_type_t);
int32_t find_random_unit(int32_t, int32_t, facility_type_t);
void gametick();
void start_command_mode(int32_t);
void switch_character_move();
void execcmd();
void use_item();
void proc_playable_op();
int32_t buy_facility(int32_t fid);
void debug_add_character();
void reset_game();
void aim_earth(int32_t);
double get_distance_raw(double, double, double, double);
void select_next_item();
void select_prev_item();
void spawn_playable_me();
int32_t spawn_playable(int32_t);
void chat_request(char*);
void chatf_request(const char*, ...);
void keypress_handler(char, specialkey_t);
void mousemotion_handler(int, int);
void keyrelease_handler(char);
void mousepressed_handler(mousebutton_t);

//info.c
void lookup(obj_type_t, LookupResult_t*);
void check_data();
const char *getlocalizedstring(int32_t);
const char *getlocalizeditemdesc(int32_t);
const char *getlocalizedcharactername(int32_t);
void lookup_playable(int32_t, PlayableInfo_t*);
int32_t is_playable_character();
int32_t find_playable_id_from_tid(int32_t);

//network.c
void process_smp();
void process_smp_events(uint8_t*, size_t, int32_t);
void stack_packet(event_type_t, ...);
int32_t lookup_smp_player_from_cid(int32_t);
void connect_server();
void close_connection(int32_t);
void net_recv_handler();
void net_server_send_cmd(server_command_t);
void close_connection_silent();

//aiproc.c
void procai();
int32_t procobjhit(int32_t, int32_t, LookupResult_t, LookupResult_t);
void damage_object(int32_t, int32_t);

