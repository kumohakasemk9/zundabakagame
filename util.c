/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

util.c: utility functions
*/

#include "inc/zundagame.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h> //Do not include with windows.h, it will break windows compatibility

size_t utf8_get_letter_bytelen(char);

extern int32_t ProgramExiting;

//Scales number v in range of 0 - ma to 0 - mp
double scale_number(double v, double ma, double mp) {
	return (v / ma) * mp;
}

//Constrains number v between mi - mx
double constrain_number(double v, double mi, double mx) {
	if(v < mi) {
		return mi;
	} else if(v > mx) {
		return mx;
	}
	return v;
}

int32_t constrain_i32(int32_t v, int32_t mi, int32_t mx) {
	if(v < mi) {
		return mi;
	} else if(v > mx) {
		return mx;
	}
	return v;
} 

//Get utf8 substring of src from index st to ed, store into dst (length dstlen restricted)
void utf8_substring(char* src, int32_t st, int32_t ed, char *dst, int32_t dstlen) {
	char* s;
	char* e;
	int32_t subl;
	ed = constrain_i32(ed, 0, (int32_t)utf8_strlen(src) ); //Restrict ed not to be greater than src letter count
	if(st > ed || dstlen < 2) {
		die("utf8_substring() failed. Parameters error: st=%d, ed=%d, dstlen=%d\n", st, ed, dstlen);
		return;
	}
	s = utf8_strlen_to_pointer(src, st);
	e = utf8_strlen_to_pointer(src, ed);
	subl = (int32_t)(e - s);
	if(subl + 1 > dstlen) {
		die("util.c: utf8_substring() failed. No enough buffer for substring.\n");
		return;
	}
	memcpy(dst, s, (size_t)subl);
	dst[subl] = '\0';
}

//insert string src at index pos to dst, returns 1 if insufficient src space. (length dstlen restricted)
int32_t utf8_insertstring(char *dst, char *src, int32_t pos, size_t dstlen) {
	uint16_t l = (uint16_t)strlen(src);
	//Restrict pos 
	pos = constrain_i32(pos, 0, (int32_t)utf8_strlen(dst) );
	//Check if processed string is shorter than dstlen bytes
	if(strlen(dst) + l + 1 > dstlen) {
		return 1;
	}
	//dstlen = 0, this is nonsense
	if(dstlen == 0) {
		return 1;
	}
	//backup string
	char *b = malloc(dstlen);
	if(b == NULL) {
		die("utf8_insertstring(): String backup failed.\n");
	}
	char *s = utf8_strlen_to_pointer(dst, pos);
	strcpy(b, s); //b = string after pos
	//join string
	memcpy(s, src, l); //copy src content after pos to original buffer
	strcpy(&s[l], b); //copy b content after pos + l to original buffer
	free(b);
	return 0;
}

//Put error message and exit game
void die(const char *p, ...) {
	va_list v;
	va_start(v, p);
	vfail(p, v);
	va_end(v);
	ProgramExiting = 1;
}

//check if v is in range from ma to mx
int32_t is_range(int32_t v, int32_t ma, int32_t mx) {
	if(ma <= v && v <= mx) {
		return 1;
	}
	return 0;
}

int32_t is_range_number(double v, double ma, double mx) {
	if(ma <= v && v <= mx) {
		return 1;
	}
	return 0;
}

//get random value
int32_t randint(int32_t mi, int32_t ma) {
	return mi + (rand() % (ma + 1 - mi) );
}

size_t utf8_get_letter_bytelen(char c) {
	uint8_t _c = (uint8_t)c;
	if(is_range(_c, 0, 127) ) {
		return 1;
	} else if(is_range(_c, 194, 223) ) {
		return 2;
	} else if(is_range(_c, 224, 239) ) {
		return 3;
	} else if(is_range(_c, 240, 247) ) {
		return 4;
	}
	warn("utf8_get_letter_bytelen(): Bad UTF8 sequence!\n");
	return 1;
}

//UTF-8 compatible strlen
int32_t utf8_strlen(char* ctx) {
	int32_t lcnt = 0;
	size_t bl = 0; //byte offset
	while(ctx[bl] && lcnt < BUFFER_SIZE) {
		bl += utf8_get_letter_bytelen(ctx[bl]);
		lcnt++;
	}
	return lcnt;
}

//Get pointer that starts from letter index letterpos for utf8
char *utf8_strlen_to_pointer(char *ctx, int32_t letterpos) {
	int32_t lcnt = 0;
	int32_t t = constrain_i32(letterpos, 0, BUFFER_SIZE);
	size_t bl = 0; //byte offset
	while(lcnt < t) {
		bl += utf8_get_letter_bytelen(ctx[bl]);
		lcnt++;
	}
	return &ctx[bl];
}

//get current time (unix epoch) in mS
double get_current_time_ms() {
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return ( (double)t.tv_sec * 1000.0) + ( (double)t.tv_nsec / 1000000.0);
}
