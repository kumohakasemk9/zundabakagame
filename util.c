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

#include "main.h"

#include <wchar.h>
#include <string.h>
#include <locale.h>
#include <stdarg.h>

#include <cairo/cairo.h>
//#include <pango/pangocairo.h>

//extern PangoLayout *PangoL;
extern int32_t ProgramExiting;
extern cairo_t* G;
extern cairo_surface_t* Plimgs[IMAGE_COUNT];

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

uint32_t constrain_ui32(uint32_t v, uint32_t mi, uint32_t mx) {
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
	uint32_t subl;
	ed = constrain_i32(ed, 0, (int32_t)utf8_strlen(src) ); //Restrict ed not to be greater than src letter count
	if(st > ed || dstlen < 2) {
		die("utf8_substring() failed. Parameters error: st=%d, ed=%d, dstlen=%d\n", st, ed, dstlen);
		return;
	}
	s = utf8_strlen_to_pointer(src, st);
	e = utf8_strlen_to_pointer(src, ed);
	subl = (uint32_t)(e - s);
	if(subl + 1 > dstlen) {
		die("util.c: utf8_substring() failed. No enough buffer for substring.\n");
		return;
	}
	memcpy(dst, s, subl);
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
	vprintf(p, v);
	va_end(v);
	ProgramExiting = 1;
}

//Get how string ctx occupy width if drawn in current font
int32_t get_string_width(char* ctx) {
	int32_t w;
	//pango_layout_set_text(PangoL, ctx, -1);
	//pango_layout_get_pixel_size(PangoL, &w, NULL);
	cairo_text_extents_t t;
	cairo_text_extents(G, ctx, &t);
	w = (int32_t)t.x_advance;
	return w;
}

//get_string_width but substring version (from index st to ed)
int32_t get_substring_width(char* ctx, int32_t st, int32_t ed) {
	char b[BUFFER_SIZE];
	utf8_substring(ctx, st, ed, b, sizeof(b) );
	return get_string_width(b);
}

//Get maximum letter height of current selected font.
int32_t get_font_height() {
	cairo_font_extents_t t;
	int32_t h;
	//pango_layout_set_text(PangoL, "abcdefghijklmnopqrstuvwxyz", -1);
	//pango_layout_get_pixel_size(PangoL, NULL, &h);
	cairo_font_extents(G, &t);
	h = (int32_t)t.height;
	return h;
}

//shorten string to fit in width wid, returns shorten string length, stores actual width to widr if not NULL
int32_t shrink_string(char *ctx, int32_t wid, int32_t* widr) {
	int32_t l = utf8_strlen(ctx);
	return shrink_substring(ctx, wid, 0, l, widr);
}

//substring version of shrink_string()
int32_t shrink_substring(char *ctx, int32_t wid, int32_t st, int32_t ed, int32_t* widr) {
	int32_t l = constrain_i32(ed, 0, utf8_strlen(ctx) );
	int32_t w = wid + 10;
	//Delete one letter from ctx in each loops, continue until string width fits in wid
	while(l > 0) {
		w = get_substring_width(ctx, st, l);
		if(widr != NULL) {*widr = w;}
		if(w < wid || l <= 1) { return l; }
		l--;
		//g_print("%d %d\n", l, w);
	}
	return 1;
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

//get image size of imgid
void get_image_size(int32_t imgid, double *w, double *h) {
	if(!is_range(imgid, 0, IMAGE_COUNT - 1)) {
		die("get_image_size() failed: Bad imgid passed: %d\n", imgid);
		return;
	}
	*w = (double)cairo_image_surface_get_width(Plimgs[imgid]);
	*h = (double)cairo_image_surface_get_height(Plimgs[imgid]);
}

size_t utf8_get_letter_bytelen(char *l) {
	int32_t c = (int32_t)l[0];
	if(is_range(c, 0, 127) ) {
		return 1;
	} else if(is_range(c, 194, 223) ) {
		return 2;
	} else if(is_range(c, 224, 239) ) {
		return 3;
	} else if(is_range(c, 240, 247) ) {
		return 4;
	}
	printf("utf8_get_letter_bytelen(): Bad UTF8 sequence!\n");
	return 1;
}

//UTF-8 compatible strlen
int32_t utf8_strlen(char* ctx) {
	setlocale(LC_ALL, "en_US.utf8"); //Needed for mbrlen
	mbstate_t mb;
	int32_t lcnt = 0;
	ssize_t bl = 0; //byte offset
	memset(&mb, 0, sizeof(mbstate_t) );
	while(lcnt < BUFFER_SIZE) {
		ssize_t r = (ssize_t)mbrlen(&ctx[bl], 4, &mb);
		//if first byte is utf8 header, get next byte to examine entire letter byte size
		if(r == -2) {
			bl++;
			r = (ssize_t)mbrlen(&ctx[bl], 4, &mb);
			if(r == -1 || r == -2) {
				printf("utf8_strlen_test(): Bad UTF8, decorder terminated.\n");
				return 0;
			}
		} else if(r == 0) {
			return lcnt; //null terminator detected
		} else if(r == -1) {
			printf("utf8_strlen_test(): Bad UTF8, decorder terminated.\n");
		}
		bl += r; //advance byte count to point next letter
		lcnt++;
	}
	return lcnt;
}

//Get pointer that starts from letter index letterpos for utf8
char *utf8_strlen_to_pointer(char *ctx, int32_t letterpos) {
	setlocale(LC_ALL, "en_US.utf8"); //Needed for mbrlen
	mbstate_t mb;
	int32_t lcnt = 0;
	int32_t t = constrain_i32(letterpos, 0, BUFFER_SIZE);
	ssize_t bl = 0; //byte offset
	memset(&mb, 0, sizeof(mbstate_t) );
	while(lcnt < t) {
		ssize_t r = (ssize_t)mbrlen(&ctx[bl], 4, &mb);
		//if first byte is utf8 header, get next byte to examine entire letter byte size
		if(r == -2) {
			bl++;
			r = (ssize_t)mbrlen(&ctx[bl], 4, &mb);
			if(r == -1 || r == -2) {
				printf("utf8_strlen_test(): Bad UTF8, decorder terminated.\n");
				return NULL;
			}
		} else if(r == 0) {
			return NULL; //null terminator detected
		} else if(r == -1) {
			printf("utf8_strlen_test(): Bad UTF8, decorder terminated.\n");
		}
		bl += r; //advance byte count to point next letter
		lcnt++;
	}
	return &ctx[bl];
}
