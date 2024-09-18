/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by Gtk4

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

gutil.c: drawing functions
*/

#define _USE_MATH_DEFINES //Windows MSVC workaround
#include "main.h"

double Fgcolor[4] = {1.0, 1.0, 1.0, 1.0}; //Selected FGcolor
extern cairo_t* G; //Gamescreen cairo context
extern cairo_surface_t* Plimgs[];
extern PangoLayout *Gpangolayout;

//Change drawing color to colorcode, backup if savecolor is TRUE
void chcolor(uint32_t colorcode, gboolean savecolor) {
	double a = scale_number((colorcode >> 24) & 0xff, 255.0, 1.0);
	double r = scale_number((colorcode >> 16) & 0xff, 255.0, 1.0);
	double g = scale_number((colorcode >> 8) & 0xff, 255.0, 1.0);
	double b = scale_number(colorcode & 0xff, 255.0, 1.0);
	if(savecolor) {
		Fgcolor[0] = a;
		Fgcolor[1] = r;
		Fgcolor[2] = g;
		Fgcolor[3] = b;
	}
	cairo_set_source_rgba(G, r, g, b, a);
}

//Restore saved color
void restore_color() {
	cairo_set_source_rgba(G, Fgcolor[1], Fgcolor[2], Fgcolor[3], Fgcolor[0]);
}

//Draw line (weight w) from x1, y1 to x2, y2
void drawline(double x1, double y1, double x2, double y2, double w) {
	cairo_move_to(G, x1, y1);
	cairo_line_to(G, x2, y2);
	cairo_set_line_width(G, w);
	cairo_stroke(G);
}

//draw circle that has center point on x, y with diameter diam
void fillcircle(double x, double y, double diam) {
	cairo_arc(G, x, y, diam / 2.0, 0, 2.0 * M_PI);
	cairo_fill(G);
}

//draw hollow circle that has center point on x, y with diameter diam
//void hollowcircle(double x, double y, double diam) {
//	cairo_arc(G, x, y, diam / 2.0, 0, 2.0 * M_PI);
//	cairo_stroke(G);
//}

//draw string ctx in pos x, y
void drawstring(double x, double y, char* ctx) {
	pango_layout_set_text(Gpangolayout, ctx, -1);
	cairo_move_to(G, x, y); // + get_font_height());
	//cairo_show_text(G, ctx);
	pango_cairo_show_layout(G, Gpangolayout);
}

//draw substring ctx (index sp to ed) in pos x, y
void drawsubstring(double x, double y, char* ctx, uint16_t st, uint16_t ed) {
	char b[BUFFER_SIZE];
	utf8_substring(ctx, st, ed, b, sizeof(b));
	drawstring(x, y, b);
}

void drawstringf(double x, double y, const char *p, ...) {
	char b[BUFFER_SIZE];
	va_list v;
	va_start(v, p);
	int32_t r = vsnprintf(b, sizeof(b), p, v);
	va_end(v);
	//Safety check
	if(r >= sizeof(b) || r == -1) {
		die("gutil.c: drawstringf() failed: formatted string not null terminated, input format string too long?\n");
		return;
	}
	//Print
	drawstring(x, y, b);
}

//draw string and let it fit in width wid using newline. if bg is TRUE, string will have rectangular background
double drawstring_inwidth(double x, double y, char* ctx, uint16_t wid, gboolean bg) {
	double ty = y;
	uint16_t cp = 0;
	uint16_t ch = get_font_height();
	uint16_t ml = (uint16_t)g_utf8_strlen(ctx, 65535);
	while(cp < ml) {
		//Write a adjusted string to fit in wid
		uint16_t a;
		uint16_t t = shrink_substring(ctx, wid, cp, ml, &a);
		if(bg) {
			chcolor(COLOR_TEXTBG, FALSE);
			fillrect(x, ty, a, ch + 5);
			restore_color();
		}
		drawsubstring(x, ty, ctx, cp, t);
	//	g_print("Substring: %d %d\n", cp, t);
		cp = t;
		ty += ch + 5;
	}
	return ty;
}

//draw image in pos x, y
void drawimage(double x, double y, uint8_t imgid) {
	if(!is_range(imgid, 0, IMAGE_COUNT - 1)) {
		die("gutil.c: drawimage() failed: Bad imgid passed: %d\n", imgid);
		return;
	}
	cairo_set_source_surface(G, Plimgs[imgid], x, y);
	cairo_paint(G);
}

//draw filled rectangle
void fillrect(double x, double y, double w, double h){
	cairo_rectangle(G, x, y, w, h);
	cairo_fill(G);
}

//draw hollow rectangle
void hollowrect(double x, double y, double w, double h){
	cairo_rectangle(G, x, y, w, h);
	cairo_set_line_width(G, 1);
	cairo_stroke(G);
}

uint16_t drawstring_title(double y, char* ctx, uint8_t s) {
	//cairo_set_font_size(G, s);
	set_font_size(s);
	uint16_t w = get_string_width(ctx);
	uint16_t h = get_font_height();
	double x = (WINDOW_WIDTH / 2) - (w / 2);
	chcolor(COLOR_TEXTBG, FALSE);
	fillrect(x, y, w, h);
	restore_color();
	drawstring(x, y, ctx);
	//cairo_set_font_size(G, FONT_DEFAULT_SIZE);
	set_font_size(FONT_DEFAULT_SIZE);
	return h;
}

//Set font size to s
void set_font_size(uint16_t s) {
	const PangoFontDescription *pfd = pango_layout_get_font_description(Gpangolayout);
	PangoFontDescription *pfdd = pango_font_description_copy(pfd);
	pango_font_description_set_size(pfdd, s * PANGO_SCALE);
	pango_layout_set_font_description(Gpangolayout, pfdd);
	pango_font_description_free(pfdd);
}

//load fonts from fontlist and use it for text drawing
void loadfont(const char* fontlist) {
	PangoFontDescription *pfd = pango_font_description_from_string(fontlist);
	pango_layout_set_font_description(Gpangolayout, pfd);
	pango_font_description_free(pfd);
}

//draw polygon that has start point x,y and vertexes represented by x: points[2n], y: points[2n+1]
//n: num_points, region specified by points don't have to be closed. all points are relative.
void draw_polygon(double x, double y, uint8_t num_points, double points[]) {
	cairo_set_line_width(G, 1);
	cairo_move_to(G, x, y);
	for(uint8_t i = 0; i < num_points; i++) {
		cairo_rel_line_to(G, points[i * 2], points[i * 2 + 1]);
	}
	cairo_move_to(G, x, y);
	cairo_fill(G);
}
