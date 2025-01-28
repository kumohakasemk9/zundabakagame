/*
Zundamon bakage (C) 2024 Kumohakase
https://creativecommons.org/licenses/by-sa/4.0/ CC-BY-SA 4.0
Please consider supporting me through ko-fi or pateron
https://ko-fi.com/kumohakase
https://www.patreon.com/kumohakasemk8

Zundamon bakage powered by cairo, X11.

Zundamon is from https://zunko.jp/
(C) 2024 ＳＳＳ合同会社, (C) 2024 坂本アヒル https://twitter.com/sakamoto_ahr

graphics.c: drawing functions
*/

#include "inc/zundagame.h"

#include <stdarg.h>
#include <math.h>

#include <pango/pangocairo.h>
#include <cairo/cairo.h>

double Fgcolor[4] = {1.0, 1.0, 1.0, 1.0}; //Selected FGcolor
cairo_t* G = NULL; //Gamescreen cairo context
cairo_surface_t* Plimgs[IMAGE_COUNT];
PangoLayout *PangoL;
cairo_surface_t *Gsfc = NULL; //GameScreen surface
extern const char* IMGPATHES[]; //preload image pathes

//Clear screen with black
void clear_screen() {
	cairo_set_source_rgb(G, 0, 0, 0);
	cairo_paint(G);
}

//Change drawing color to colorcode, backup if savecolor is 1
void chcolor(uint32_t colorcode, int32_t savecolor) {
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

//draw string ctx in pos x, y
void drawstring(double x, double y, char* ctx) {
	pango_layout_set_text(PangoL, ctx, -1);
	cairo_move_to(G, x, y); // + get_font_height() - 2);
	//cairo_show_text(G, ctx);
	pango_cairo_show_layout(G, PangoL);
}

//draw image in pos x, y
void drawimage(double x, double y, int32_t imgid) {
	if(!is_range(imgid, 0, IMAGE_COUNT - 1)) {
		die("drawimage() failed: Bad imgid passed: %d\n", imgid);
		return;
	}
	cairo_set_source_surface(G, Plimgs[imgid], x, y);
	cairo_paint(G);
}

//draw image in pos x, y in specific size w, h (scaled)
void drawimage_scale(double x, double y, double w, double h, int32_t imgid) {
	//Test
	if(w < 0 || h < 0) {
		die("drawimage_scale() failed, bad scale passed.\n");
		return;
	}
	if(!is_range(imgid, 0, IMAGE_COUNT - 1) ) {
		die("drawimage_scale() failed, bad imgid passed.\n");
		return;
	}
	//get original image size
	double _w, _h;
	get_image_size(imgid, &_w, &_h);
	//if specified size is the same as original one, do not apply filter.
	if(_w == w && _h == h) {
		drawimage(x, y, imgid);
		return;
	}
	cairo_save(G); //save transform matrix
	cairo_translate(G, x, y); //move image to specified pos
	cairo_scale(G, w / _w, h / _h); //calc scale and apply
	cairo_set_source_surface(G, Plimgs[imgid], 0, 0);
	cairo_paint(G);
	cairo_restore(G); //restore transform matrix
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

//Set font size to s
void set_font_size(int32_t s) {
	const PangoFontDescription *pfd = pango_layout_get_font_description(PangoL);
	PangoFontDescription *pfdd = pango_font_description_copy(pfd);
	pango_font_description_set_size(pfdd, s * PANGO_SCALE);
	pango_layout_set_font_description(PangoL, pfdd);
	pango_font_description_free(pfdd);
	//cairo_set_font_size(G, (double)s);
}

//load fonts from fontlist and use it for text drawing
void loadfont(char* fontlist) {
	PangoFontDescription *desc;
	desc = pango_font_description_from_string (fontlist);
	pango_layout_set_font_description (PangoL, desc);
	pango_font_description_free (desc);
	cairo_select_font_face(G, "Ubuntu Mono", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
}

//draw polygon that has start point x,y and vertexes represented by x: points[2n], y: points[2n+1]
//n: num_points, region specified by points don't have to be closed. all points are relative.
void draw_polygon(double x, double y, int32_t num_points, double points[]) {
	cairo_set_line_width(G, 1);
	cairo_move_to(G, x, y);
	for(uint8_t i = 0; i < num_points; i++) {
		cairo_rel_line_to(G, points[i * 2], points[i * 2 + 1]);
	}
	cairo_move_to(G, x, y);
	cairo_fill(G);
}

//Init task related to cairo and pango.
int32_t init_graphics() {
	//Create game screen and its content
	Gsfc = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, WINDOW_WIDTH, WINDOW_HEIGHT);
	G = cairo_create(Gsfc);
	if(cairo_status(G) != CAIRO_STATUS_SUCCESS) {
		fail("init_graphics(): Making game screen buffer failed.\n");
		return -1;
	}
	
	info("Loading image assets.\n");
	for(int32_t i = 0; i < IMAGE_COUNT; i++) {
		Plimgs[i] = cairo_image_surface_create_from_png(IMGPATHES[i]);
		if(cairo_surface_status(Plimgs[i]) != CAIRO_STATUS_SUCCESS) {
			fail("init_graphics(): Error loading %s\n", IMGPATHES[i]);
			return -1;
		}
	}
	PangoL = pango_cairo_create_layout(G);
	
	//Setup font
	loadfont("Ubuntu Mono,monospace");
	set_font_size(FONT_DEFAULT_SIZE);
	
	return 0;
}

//Uninit task related to pango and cairo
void uninit_graphics() {
	//Unload images
	for(uint8_t i = 0; i < IMAGE_COUNT; i++) {
		if(Plimgs[i] != NULL) {
			cairo_surface_destroy(Plimgs[i]);
		}
	}
	
	//Unload other resources
	if(PangoL != NULL) {
		g_object_unref(PangoL);
	}
	if(G != NULL) {
		cairo_destroy(G);
	}
	if(Gsfc != NULL) {
		cairo_surface_destroy(Gsfc);
	}
}

//Get how string ctx occupy width if drawn in current font
int32_t get_string_width(char* ctx) {
	int32_t w;
	pango_layout_set_text(PangoL, ctx, -1);
	pango_layout_get_pixel_size(PangoL, &w, NULL);
	//cairo_text_extents_t t;
	//cairo_text_extents(G, ctx, &t);
	//w = (int32_t)t.x_advance;
	return w;
}

//Get maximum letter height of current selected font.
int32_t get_font_height() {
	//cairo_font_extents_t t;
	int32_t h;
	pango_layout_set_text(PangoL, "abcdefghijklmnopqrstuvwxyz", -1);
	pango_layout_get_pixel_size(PangoL, NULL, &h);
	//cairo_font_extents(G, &t);
	//h = (int32_t)t.height;
	return h;
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


