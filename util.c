/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

util.c: utility functions
*/

#include "main.h"

extern GtkApplication *Application;
extern gboolean ProgramExiting;
//extern cairo_t* G;
extern cairo_surface_t* Plimgs[IMAGE_COUNT];
extern PangoLayout* Gpangolayout;

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
	ed = constrain_i32(ed, 0, g_utf8_strlen(src, 65535) ); //Restrict ed not to be greater than src letter count
	if(st > ed || dstlen < 2) {
		die("utf8_substring() failed. Parameters error: st=%d, ed=%d, dstlen=%d\n", st, ed, dstlen);
		return;
	}
	s = g_utf8_offset_to_pointer(src, st);
	e = g_utf8_offset_to_pointer(src, ed);
	subl = (uint32_t)(e - s);
	if(subl + 1 > dstlen) {
		die("util.c: utf8_substring() failed. No enough buffer for substring.\n");
		return;
	}
	memcpy(dst, s, subl);
	dst[subl] = '\0';
}

//convert double to uint8_t[8] and store into buf (offset offs)
void double2bytes(double ctx, uint8_t *buf, int32_t offs) {
	memcpy(&buf[offs], &ctx, 8);
}

//convert int32_t to uint8_t[4] and store into buf (offset offs)
void int322bytes(int32_t ctx, uint8_t *buf, int32_t offs) {
	memcpy(&buf[offs], &ctx, 4);
}

//insert string src at index pos to dst, returns FALSE if insufficient src space. (length dstlen restricted)
gboolean utf8_insertstring(char *dst, char *src, int32_t pos, int32_t dstlen) {
	uint16_t l = (uint16_t)strlen(src);
	//Restrict pos 
	pos = constrain_i32(pos, 0, g_utf8_strlen(dst, 65535) );
	//Check if processed string is shorter than dstlen bytes
	if(strlen(dst) + l + 1 > dstlen) {
		return FALSE;
	}
	//dstlen = 0, this is nonsense
	if(dstlen == 0) {
		return FALSE;
	}
	//backup string
	char *b = g_malloc(dstlen);
	char *s = g_utf8_offset_to_pointer(dst, pos);
	strcpy(b, s); //b = string after pos
	//join string
	memcpy(s, src, l); //copy src content after pos to original buffer
	strcpy(&s[l], b); //copy b content after pos + l to original buffer
	g_free(b);
	return TRUE;
}

//Put error message and exit game
void die(const char *p, ...) {
	va_list v;
	va_start(v, p);
	vprintf(p, v);
	va_end(v);
	/*if(Timer1_src != 0) {
		g_source_remove(Timer1_src);
		Timer1_src = 0;
	}
	if(Timer2_src != 0) {
		g_source_remove(Timer2_src);
		Timer2_src = 0;
	}*/
	ProgramExiting = TRUE;
	g_application_quit(G_APPLICATION(Application) );
}

//Get how string ctx occupy width if drawn in current font
int32_t get_string_width(char* ctx) {
	int w;
	pango_layout_set_text(Gpangolayout, ctx, -1);
	pango_layout_get_pixel_size(Gpangolayout, &w, NULL);
	return (uint16_t)w;
}

//get_string_width but substring version (from index st to ed)
int32_t get_substring_width(char* ctx, int32_t st, int32_t ed) {
	char b[BUFFER_SIZE];
	utf8_substring(ctx, st, ed, b, sizeof(b) );
	return get_string_width(b);
}

//Get maximum letter height of current selected font.
int32_t get_font_height() {
	int h;
	pango_layout_set_text(Gpangolayout, "abcdefghijklmnopqrstuvwxyz", -1);
	pango_layout_get_pixel_size(Gpangolayout, NULL, &h);
	return (uint16_t)h;
}

//shorten string to fit in width wid, returns shorten string length, stores actual width to widr if not NULL
int32_t shrink_string(char *ctx, int32_t wid, int32_t* widr) {
	int32_t l = g_utf8_strlen(ctx, 65535);
	return shrink_substring(ctx, wid, 0, l, widr);
}

//substring version of shrink_string()
int32_t shrink_substring(char *ctx, int32_t wid, int32_t st, int32_t ed, int32_t* widr) {
	int32_t l = constrain_i32(ed, 0, g_utf8_strlen(ctx, 65535) );
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
gboolean is_range(int32_t v, int32_t ma, int32_t mx) {
	if(ma <= v && v <= mx) {
		return TRUE;
	}
	return FALSE;
}

gboolean is_range_number(double v, double ma, double mx) {
	if(ma <= v && v <= mx) {
		return TRUE;
	}
	return FALSE;
}

//get random value
int32_t randint(int32_t mi, int32_t ma) {
	return mi + (rand() % (ma + 1 - mi) );
}

//get image size of imgid
void get_image_size(int32_t imgid, double *w, double *h) {
	if(!is_range(imgid, 0, IMAGE_COUNT - 1)) {
		die("gutil.c: get_image_size() failed: Bad imgid passed: %d\n", imgid);
		return;
	}
	*w = (double)cairo_image_surface_get_width(Plimgs[imgid]);
	*h = (double)cairo_image_surface_get_height(Plimgs[imgid]);
}
