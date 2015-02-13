
/*
 * CINELERRA
 * Copyright (C) 1997-2014 Adam Williams <broadcast at earthling dot net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// Originally developed by Heroine Virtual Ltd.
// Support for multiple encodings, outline (stroke) by
// Andraz Tori <Andraz.tori1@guest.arnes.si>
// Additional support for UTF-8 by
// Paolo Rampino aka Akirad <info at tuttoainternet.it>




#include "bcsignals.h"
#include "clip.h"
#include "colormodels.h"
#include "filexml.h"
#include "filesystem.h"
#include "transportque.inc"
#include "ft2build.h"
#include FT_GLYPH_H
#include FT_BBOX_H
#include FT_OUTLINE_H
#include FT_STROKER_H
#include "language.h"
#include "mwindow.inc"
#include "picon_png.h"
#include "cicolors.h"
#include "title.h"
#include "titlewindow.h"
#include "transportque.inc"


#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>
#include <byteswap.h>
#include <iconv.h>
#include <sys/stat.h>
#include <fontconfig/fontconfig.h>

#define ZERO (1.0 / 64.0)
#define FONT_SEARCHPATH "/fonts"

REGISTER_PLUGIN(TitleMain)

#ifdef X_HAVE_UTF8_STRING
#define DEFAULT_ENCODING "UTF-8"
#else
#define DEFAULT_ENCODING "ISO8859-1"
#endif
#define DEFAULT_TIMECODEFORMAT TIME_HMS

static YUV yuv;

TitleConfig::TitleConfig()
{
	style = 0;
	color = BLACK;
	alpha = 0xff;
	outline_alpha = 0xff;
	size = 24;
	motion_strategy = NO_MOTION;
	loop = 0;
	hjustification = JUSTIFY_CENTER;
	vjustification = JUSTIFY_MID;
	fade_in = 0.0;
	fade_out = 0.0;
	x = 0.0;
	y = 0.0;
	dropshadow = 10;
	sprintf(font, "fixed");
	sprintf(encoding, DEFAULT_ENCODING);
	timecode_format = DEFAULT_TIMECODEFORMAT;
	wtext[0] = 0;  wlen = 0;
	pixels_per_second = 1.0;
	timecode = 0;
	stroke_width = 1.0;
	color_stroke = 0xff0000;
	outline_color = WHITE;

	timecode_format = TIME_HMS;
	outline_size = 0;
	window_w = 640;
	window_h = 480;
	next_keyframe_position = 0;
	prev_keyframe_position = 0;
}

TitleConfig::~TitleConfig()
{
}

// Does not test equivalency but determines if redrawing text is necessary.
int TitleConfig::equivalent(TitleConfig &that)
{
	return dropshadow == that.dropshadow &&
		style == that.style &&
		size == that.size &&
		color == that.color &&
		color_stroke == that.color_stroke &&
		stroke_width == that.stroke_width &&
		outline_color == that.outline_color &&
		alpha == that.alpha &&
		outline_alpha == that.outline_alpha &&
		timecode == that.timecode &&
		timecode_format == that.timecode_format &&
		outline_size == that.outline_size &&
		hjustification == that.hjustification &&
		vjustification == that.vjustification &&
		EQUIV(pixels_per_second, that.pixels_per_second) &&
		!strcasecmp(font, that.font) &&
		!strcasecmp(encoding, that.encoding) &&
		wlen == that.wlen &&
		!memcmp(wtext, that.wtext, wlen * sizeof(wchar_t));
}

void TitleConfig::copy_from(TitleConfig &that)
{
	strcpy(font, that.font);
	style = that.style;
	size = that.size;
	color = that.color;
	color_stroke = that.color_stroke;
	stroke_width = that.stroke_width;
	outline_color = that.outline_color;
	alpha = that.alpha;
	outline_alpha = that.outline_alpha;
	pixels_per_second = that.pixels_per_second;
	motion_strategy = that.motion_strategy;
	loop = that.loop;
	hjustification = that.hjustification;
	vjustification = that.vjustification;
	fade_in = that.fade_in;
	fade_out = that.fade_out;
	x = that.x;
	y = that.y;
	dropshadow = that.dropshadow;
	timecode = that.timecode;
	timecode_format = that.timecode_format;
	outline_size = that.outline_size;
	strcpy(encoding, that.encoding);
	memcpy(wtext, that.wtext, that.wlen * sizeof(wchar_t));
	wlen = that.wlen;
	window_w = that.window_w;
	window_h = that.window_h;

	limits();
}

void TitleConfig::interpolate(TitleConfig &prev,
	TitleConfig &next,
	int64_t prev_frame,
	int64_t next_frame,
	int64_t current_frame)
{
	strcpy(font, prev.font);
	strcpy(encoding, prev.encoding);
	style = prev.style;
	size = prev.size;
	color = prev.color;
	color_stroke = prev.color_stroke;
	stroke_width = prev.stroke_width;
	outline_color = prev.outline_color;
	alpha = prev.alpha;
	outline_alpha = prev.outline_alpha;
	motion_strategy = prev.motion_strategy;
	loop = prev.loop;
	hjustification = prev.hjustification;
	vjustification = prev.vjustification;
	fade_in = prev.fade_in;
	fade_out = prev.fade_out;
	outline_size = prev.outline_size;
	pixels_per_second = prev.pixels_per_second;
	memcpy(wtext, prev.wtext, prev.wlen * sizeof(wchar_t));
	wlen = prev.wlen;

//	double next_scale = (double)(current_frame - prev_frame) / (next_frame - prev_frame);
//	double prev_scale = (double)(next_frame - current_frame) / (next_frame - prev_frame);
//	this->x = prev.x * prev_scale + next.x * next_scale;
//	this->y = prev.y * prev_scale + next.y * next_scale;
	this->x = prev.x;
	this->y = prev.y;
	timecode = prev.timecode;
	timecode_format = prev.timecode_format;
//	this->dropshadow = (int)(prev.dropshadow * prev_scale + next.dropshadow * next_scale);
	this->dropshadow = prev.dropshadow;
}

void TitleConfig::limits()
{
	if(window_w < 100) window_w = 100;
	if(window_h < 100) window_h = 100;
}




void TitleConfig::to_wtext(const char *from_enc, const char *text, int tlen)
{
	wlen = BC_Resources::encode(from_enc, BC_Resources::wide_encoding,
		(char*)text,tlen, (char *)wtext,sizeof(wtext)) / sizeof(wchar_t);
}




FontEntry::FontEntry()
{
	image = 0;
	path = 0;
	foundary = 0;
	family = 0;
	weight = 0;
	slant = 0;
	swidth = 0;
	adstyle = 0;
	spacing = 0;
	registry = 0;
	encoding = 0;
	fixed_title = 0;
	fixed_style = 0;
	pixelsize = 0;
	avg_width = 0;
	xres = 0;
	pointsize = 0;
	yres = 0;
}

FontEntry::~FontEntry()
{
	delete image;
	if(path) delete [] path;
	if(foundary) delete [] foundary;
	if(family) delete [] family;
	if(weight) delete [] weight;
	if(slant) delete [] slant;
	if(swidth) delete [] swidth;
	if(adstyle) delete [] adstyle;
	if(spacing) delete [] spacing;
	if(registry) delete [] registry;
	if(encoding) delete [] encoding;
	if(fixed_title) delete [] fixed_title;
}

void FontEntry::dump()
{
	printf("%s: %s %s %s %s %s %s %d %d %d %d %s %d %s %s\n",
		path, foundary, family, weight, slant, swidth, adstyle, pixelsize,
		pointsize, xres, yres, spacing, avg_width, registry, encoding);
}

TitleGlyph::TitleGlyph()
{
	char_code = 0;
	data = 0;
	data_stroke = 0;
	freetype_index = 0;
	height = 0;
	top = 0;
	width = 0;
	advance_w = 0;
	pitch = 0;
	left = 0;
}


TitleGlyph::~TitleGlyph()
{
//printf("TitleGlyph::~TitleGlyph 1\n");
	if(data) delete data;
	if(data_stroke) delete data_stroke;
}











GlyphPackage::GlyphPackage() : LoadPackage()
{
	glyph = 0;
}

GlyphUnit::GlyphUnit(TitleMain *plugin, GlyphEngine *server)
 : LoadClient(server)
{
	this->plugin = plugin;
	current_font = 0;
	freetype_library = 0;
	freetype_face = 0;
}

GlyphUnit::~GlyphUnit()
{
	if(freetype_library)
		FT_Done_FreeType(freetype_library);
}

void GlyphUnit::process_package(LoadPackage *package)
{
	GlyphPackage *pkg = (GlyphPackage*)package;
	TitleGlyph *glyph = pkg->glyph;
	int result = 0;
	char new_path[BCTEXTLEN];

	current_font = plugin->get_font();

	if(plugin->load_freetype_face(freetype_library,
			freetype_face,
			current_font->path)) {
		printf(_("GlyphUnit::process_package FT_New_Face failed.\n"));
		result = 1;
	}

	if(!result) {
		int gindex = FT_Get_Char_Index(freetype_face, glyph->char_code);

//printf("GlyphUnit::process_package 1 %c\n", glyph->char_code);
// Char not found
		if(gindex == 0) {
			BC_Resources *resources =  BC_WindowBase::get_resources();
			// Search replacement font
			if(resources->find_font_by_char(glyph->char_code, new_path, freetype_face))
			{
				plugin->load_freetype_face(freetype_library,
					freetype_face, new_path);
				gindex = FT_Get_Char_Index(freetype_face, glyph->char_code);
			}
		}
		FT_Set_Pixel_Sizes(freetype_face, plugin->config.size, 0);

		if (gindex == 0) {
// carrige return
			if (glyph->char_code != 10)  
				printf(_("GlyphUnit::process_package FT_Load_Char failed - char: %li.\n"),
					glyph->char_code);
// Prevent a crash here
			glyph->width = 8;  glyph->height = 8;  glyph->pitch = 8;
			glyph->advance_w = 8; glyph->left = 9; glyph->top = 9;
			glyph->freetype_index = 0;
			glyph->data = new VFrame(0, -1, 8, 8, BC_A8, 8);
			glyph->data->clear_frame();
			glyph->data_stroke = 0;

// create outline glyph
			if (plugin->config.stroke_width >= ZERO && 
				(plugin->config.style & FONT_OUTLINE)) {
				glyph->data_stroke = new VFrame(0, -1, 8, 8, BC_A8, 8);
				glyph->data_stroke->clear_frame();
			}



		}
// char found and no outline desired
		else if (plugin->config.stroke_width < ZERO ||
			!(plugin->config.style & FONT_OUTLINE)) {
			FT_Glyph glyph_image;
			FT_BBox bbox;
			FT_Bitmap bm;
			FT_Load_Glyph(freetype_face, gindex, FT_LOAD_DEFAULT);
		    	FT_Get_Glyph(freetype_face->glyph, &glyph_image);
			FT_Outline_Get_BBox(&((FT_OutlineGlyph) glyph_image)->outline, &bbox);
//			printf("Stroke: Xmin: %ld, Xmax: %ld, Ymin: %ld, yMax: %ld\n",
//					bbox.xMin,bbox.xMax, bbox.yMin, bbox.yMax);

			FT_Outline_Translate(&((FT_OutlineGlyph) glyph_image)->outline,
				- bbox.xMin,
				- bbox.yMin);
			glyph->width = bm.width = ((bbox.xMax - bbox.xMin + 63) >> 6);
			glyph->height = bm.rows = ((bbox.yMax - bbox.yMin + 63) >> 6);
			glyph->pitch = bm.pitch = bm.width;
			bm.pixel_mode = FT_PIXEL_MODE_GRAY;
			bm.num_grays = 256;
			glyph->left = (bbox.xMin + 31) >> 6;
			if (glyph->left < 0) glyph->left = 0;
			glyph->top = (bbox.yMax + 31) >> 6;
			glyph->freetype_index = gindex;
			glyph->advance_w = ((freetype_face->glyph->advance.x + 31) >> 6);
//printf("GlyphUnit::process_package 1 width=%d height=%d pitch=%d left=%d top=%d advance_w=%d freetype_index=%d\n", 
//glyph->width, glyph->height, glyph->pitch, glyph->left, glyph->top, glyph->advance_w, glyph->freetype_index);
	
			glyph->data = new VFrame(0,
				glyph->width,
				glyph->height,
				BC_A8,
				glyph->pitch);
			glyph->data->clear_frame();
			bm.buffer = glyph->data->get_data();
			FT_Outline_Get_Bitmap( freetype_library,
				&((FT_OutlineGlyph) glyph_image)->outline,
				&bm);
			FT_Done_Glyph(glyph_image);
		}
		else {
// Outline desired and glyph found
			FT_Glyph glyph_image;
			int no_outline = 0;
			FT_Stroker stroker;
			FT_Outline outline;
			FT_Bitmap bm;
			FT_BBox bbox;
			FT_UInt npoints, ncontours;	

			typedef struct  FT_LibraryRec_ 
			{    
				FT_Memory memory; 
			} FT_LibraryRec;

			FT_Load_Glyph(freetype_face, gindex, FT_LOAD_DEFAULT);
			FT_Get_Glyph(freetype_face->glyph, &glyph_image);

// check if the outline is ok (non-empty);
			FT_Outline_Get_BBox(&((FT_OutlineGlyph) glyph_image)->outline, &bbox);
			if (bbox.xMin == 0 && bbox.xMax == 0 && bbox.yMin ==0 && bbox.yMax == 0) {
				FT_Done_Glyph(glyph_image);
				glyph->data = new VFrame(0, -1, 0, BC_A8,0);
				glyph->data_stroke = new VFrame(0, -1, 0, BC_A8,0);
				glyph->width=0;   glyph->height=0;
				glyph->top=0;     glyph->left=0;
				glyph->advance_w =((int)(freetype_face->glyph->advance.x + 
					plugin->config.stroke_width * 64)) >> 6;
				return;
			}
#if FREETYPE_MAJOR > 2 || (FREETYPE_MAJOR == 2 && FREETYPE_MINOR >= 2)
			FT_Stroker_New(freetype_library, &stroker);
#else
			FT_Stroker_New(((FT_LibraryRec *)freetype_library)->memory, &stroker);
#endif
			FT_Stroker_Set(stroker, (int)(plugin->config.stroke_width * 64),
				FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
			FT_Stroker_ParseOutline(stroker, &((FT_OutlineGlyph) glyph_image)->outline,1);
			FT_Stroker_GetCounts(stroker,&npoints, &ncontours);
			if (npoints ==0 && ncontours == 0) 
			{
// this never happens, but FreeType has a bug regarding Linotype's Palatino font
				FT_Stroker_Done(stroker);
				FT_Done_Glyph(glyph_image);
				glyph->data =  new VFrame(0, -1, 0, BC_A8,0);
				glyph->data_stroke =  new VFrame(0, -1, 0, BC_A8,0);;
				glyph->width=0;
				glyph->height=0;
				glyph->top=0;
				glyph->left=0;
				glyph->advance_w =((int)(freetype_face->glyph->advance.x + 
					plugin->config.stroke_width * 64)) >> 6;
				return;
			};

			FT_Outline_New(freetype_library, npoints, ncontours, &outline);
			outline.n_points=0;
			outline.n_contours=0;
			FT_Stroker_Export (stroker, &outline);
			FT_Outline_Get_BBox(&outline, &bbox);
		
			FT_Outline_Translate(&outline,
					- bbox.xMin,
					- bbox.yMin);
		
			FT_Outline_Translate(&((FT_OutlineGlyph) glyph_image)->outline,
					- bbox.xMin,
					- bbox.yMin + (int)(plugin->config.stroke_width*32));
//			printf("Stroke: Xmin: %ld, Xmax: %ld, Ymin: %ld, yMax: %ld\n"
//					"Fill	Xmin: %ld, Xmax: %ld, Ymin: %ld, yMax: %ld\n",
//					bbox.xMin,bbox.xMax, bbox.yMin, bbox.yMax,
//					bbox_fill.xMin,bbox_fill.xMax, bbox_fill.yMin, bbox_fill.yMax);
	
			glyph->width = bm.width = ((bbox.xMax - bbox.xMin) >> 6)+1;
			glyph->height = bm.rows = ((bbox.yMax - bbox.yMin) >> 6) +1;
			glyph->pitch = bm.pitch = bm.width;
			bm.pixel_mode = FT_PIXEL_MODE_GRAY;
			bm.num_grays = 256;
			glyph->left = (bbox.xMin + 31) >> 6;
			if (glyph->left < 0) glyph->left = 0;
			glyph->top = (bbox.yMax + 31) >> 6;
			glyph->freetype_index = gindex;
			int real_advance = ((int)ceil((float)freetype_face->glyph->advance.x + 
				plugin->config.stroke_width * 64) >> 6);
			glyph->advance_w = glyph->width + glyph->left;
			if (real_advance > glyph->advance_w) 
				glyph->advance_w = real_advance;
//printf("GlyphUnit::process_package 1 width=%d height=%d "
// "pitch=%d left=%d top=%d advance_w=%d freetype_index=%d\n", 
// glyph->width, glyph->height, glyph->pitch, glyph->left,
// glyph->top, glyph->advance_w, glyph->freetype_index);


//printf("GlyphUnit::process_package 1\n");
			glyph->data = new VFrame(0, -1, glyph->width, glyph->height, BC_A8, glyph->pitch);
			glyph->data->clear_frame();
			glyph->data_stroke = new VFrame(0, -1, glyph->width, glyph->height, BC_A8, glyph->pitch);
			glyph->data_stroke->clear_frame();
// for debugging	memset(	glyph->data_stroke->get_data(), 60, glyph->pitch * glyph->height);
			bm.buffer=glyph->data->get_data();
			FT_Outline_Get_Bitmap( freetype_library,
				&((FT_OutlineGlyph) glyph_image)->outline,
				&bm);	
			bm.buffer=glyph->data_stroke->get_data();
			FT_Outline_Get_Bitmap( freetype_library,
           		&outline,
				&bm);
			FT_Outline_Done(freetype_library,&outline);
			FT_Stroker_Done(stroker);
			FT_Done_Glyph(glyph_image);

//printf("GlyphUnit::process_package 2\n");
		}
	}
}



GlyphEngine::GlyphEngine(TitleMain *plugin, int cpus)
 : LoadServer(cpus, cpus)
{
	this->plugin = plugin;
}

void GlyphEngine::init_packages()
{
	int current_package = 0;
	for(int i = 0; i < plugin->glyphs.total; i++) {
		if(!plugin->glyphs.values[i]->data) {
			GlyphPackage *pkg = (GlyphPackage*)get_package(current_package++);
			pkg->glyph = plugin->glyphs.values[i];
		}
	}
}

LoadClient* GlyphEngine::new_client()
{
	return new GlyphUnit(plugin, this);
}

LoadPackage* GlyphEngine::new_package()
{
	return new GlyphPackage;
}





// Copy a single character to the text mask
TitlePackage::TitlePackage()
 : LoadPackage()
{
	x = y = 0;
	char_code = 0;
}


TitleUnit::TitleUnit(TitleMain *plugin, TitleEngine *server)
 : LoadClient(server)
{
	this->plugin = plugin;
	this->engine = server;
}

void TitleUnit::draw_glyph(VFrame *output, TitleGlyph *glyph, int x, int y)
{
	int glyph_w = glyph->data->get_w();
	int glyph_h = glyph->data->get_h();
	int output_w = output->get_w();
	int output_h = output->get_h();
	unsigned char **in_rows = glyph->data->get_rows();
	unsigned char **out_rows = output->get_rows();
	int r, g, b, a;
	plugin->get_color_components(&r, &g, &b, &a, 0);
	int outline = plugin->config.outline_size;
	if(outline) a = 0xff;

//printf("TitleUnit::draw_glyph 1 %c %d %d\n", glyph->c, x, y);
	for(int in_y = 0; in_y < glyph_h; in_y++) {
		int y_out = y + plugin->get_char_height() + in_y - glyph->top;

		if(y_out >= 0 && y_out < output_h) {
			unsigned char *in_row = in_rows[in_y];
			int x1 = x + glyph->left;
			int y1 = y_out;

			if(engine->do_dropshadow) {
// Direct copy glyph value to black alpha
				x1 += plugin->config.dropshadow;
				y1 += plugin->config.dropshadow;

				if(y1 < output_h) {
					unsigned char *out_row = out_rows[y1];
					for(int in_x = 0; in_x < glyph_w; in_x++) {
						int x_out = x1 + in_x;
						if(x_out >= 0 && x_out < output_w) {
							if(in_row[in_x] > 0)
								out_row[x_out * 4 + 3] = in_row[in_x];
						}
					}
				}
			}
			else {
				unsigned char *out_row = out_rows[y1];
// Blend color value with shadow using glyph alpha.
				for(int in_x = 0; in_x < glyph_w; in_x++) {
					int x_out = x1 + in_x;
					if(x_out >= 0 && x_out < output_w) {
						int opacity = in_row[in_x] * a / 0xff;
						int transparency = out_row[x_out * 4 + 3] * (0xff - opacity) / 0xff;
//						int transparency = 0xff - opacity;
						if(in_row[in_x] > 0)
						{
							out_row[x_out * 4 + 0] = (r * opacity +
								out_row[x_out * 4 + 0] * transparency) / 0xff;
							out_row[x_out * 4 + 1] = (g * opacity +
								out_row[x_out * 4 + 1] * transparency) / 0xff;
							out_row[x_out * 4 + 2] = (b * opacity +
								out_row[x_out * 4 + 2] * transparency) / 0xff;
							out_row[x_out * 4 + 3] = MAX(opacity,
								out_row[x_out * 4 + 3]);
						}
					}
				}
			}
		}
	}
}


void TitleUnit::process_package(LoadPackage *package)
{
	TitlePackage *pkg = (TitlePackage*)package;

	if(pkg->char_code != '\n') {
		for(int i = 0; i < plugin->glyphs.total; i++) {
			TitleGlyph *glyph = plugin->glyphs.values[i];
			if(glyph->char_code == pkg->char_code) {
				draw_glyph(plugin->text_mask, glyph, pkg->x, pkg->y);
				if(plugin->config.stroke_width >= ZERO &&
					(plugin->config.style & FONT_OUTLINE)) {
					VFrame *tmp = glyph->data;
					glyph->data = glyph->data_stroke;
					draw_glyph(plugin->text_mask_stroke, glyph, pkg->x, pkg->y);
					glyph->data = tmp;
				}
				break;
			}
		}
	}
}

TitleEngine::TitleEngine(TitleMain *plugin, int cpus)
 : LoadServer(cpus, cpus)
{
	this->plugin = plugin;
}

void TitleEngine::init_packages()
{
	int visible_y1 = plugin->visible_row1 * plugin->get_char_height();
	int current_package = 0;
	for(int i = plugin->visible_char1; i < plugin->visible_char2; i++) {
		title_char_position_t *char_position = plugin->char_positions + i;
		TitlePackage *pkg = (TitlePackage*)get_package(current_package);
		pkg->x = char_position->x + plugin->config.outline_size;
		pkg->y = char_position->y - visible_y1 + plugin->config.outline_size;
		pkg->char_code = plugin->config.wtext[i];
		current_package++;
	}
}

LoadClient* TitleEngine::new_client()
{
	return new TitleUnit(plugin, this);
}

LoadPackage* TitleEngine::new_package()
{
	return new TitlePackage;
}




// Copy a single character to the text mask
TitleOutlinePackage::TitleOutlinePackage()
 : LoadPackage()
{
}


TitleOutlineUnit::TitleOutlineUnit(TitleMain *plugin, TitleOutlineEngine *server)
 : LoadClient(server)
{
	this->plugin = plugin;
	this->engine = server;
}

void TitleOutlineUnit::process_package(LoadPackage *package)
{
	TitleOutlinePackage *pkg = (TitleOutlinePackage*)package;
	int r, g, b, outline_a;
	int title_r, title_g, title_b, title_a;
	plugin->get_color_components(&r, &g, &b, &outline_a, 1);
	plugin->get_color_components(&title_r, &title_g, &title_b, &title_a, 0);

	if(engine->pass == 0) {
		for(int i = pkg->y1; i < pkg->y2; i++) {
			unsigned char *out_row = plugin->outline_mask->get_rows()[i];
			for(int j = 0; j < plugin->text_mask->get_w(); j++) {
				int x1 = j - plugin->config.outline_size;
				int x2 = j + plugin->config.outline_size;
				int y1 = i - plugin->config.outline_size;
				int y2 = i + plugin->config.outline_size;
				CLAMP(x1, 0, plugin->text_mask->get_w() - 1);
				CLAMP(x2, 0, plugin->text_mask->get_w() - 1);
				CLAMP(y1, 0, plugin->text_mask->get_h() - 1);
				CLAMP(y2, 0, plugin->text_mask->get_h() - 1);
				//int max_r = 0;
				//int max_g = 0;
				//int max_b = 0;
				int max_a = 0;

				for(int k = y1; k <= y2; k++) {
					unsigned char *text_row = plugin->text_mask->get_rows()[k];
					for(int l = x1; l <= x2; l++) {
						unsigned char *pixel = text_row + l * 4;
						if(pixel[3] > max_a) {
							//max_r = pixel[0];
							//max_g = pixel[1];
							//max_b = pixel[2];
							max_a = pixel[3];
						}
					}
				}

				unsigned char *out_pixel = out_row + j * 4;
				out_pixel[0] = r;
				out_pixel[1] = g;
				out_pixel[2] = b;
				out_pixel[3] = (max_a * outline_a) / 0xff;
			}
		}
	}
	else {
// Overlay text mask on top of outline mask
		for(int i = pkg->y1; i < pkg->y2; i++) {
			unsigned char *out_row = plugin->text_mask->get_rows()[i];
			unsigned char *in_row = plugin->outline_mask->get_rows()[i];
			for(int j = 0; j < plugin->text_mask->get_w(); j++)
			{
				unsigned char *out_pixel = out_row + j * 4;
				unsigned char *in_pixel = in_row + j * 4;
				int out_a = out_pixel[3];
				int in_a = in_pixel[3];
				int transparency = in_a * (0xff - out_a) / 0xff;
				out_pixel[0] = (out_pixel[0] * out_a + in_pixel[0] * transparency) / 0xff;
				out_pixel[1] = (out_pixel[1] * out_a + in_pixel[1] * transparency) / 0xff;
				out_pixel[2] = (out_pixel[2] * out_a + in_pixel[2] * transparency) / 0xff;
				int temp = in_a - out_a;
				if(temp < 0) temp = 0;
				out_pixel[3] = temp + out_a * title_a / 0xff;
			}
		}
	}
}

TitleOutlineEngine::TitleOutlineEngine(TitleMain *plugin, int cpus)
 : LoadServer(cpus, cpus)
{
	this->plugin = plugin;
}

void TitleOutlineEngine::init_packages()
{
	for(int i = 0; i < get_total_packages(); i++)
	{
		TitleOutlinePackage *pkg = (TitleOutlinePackage*)get_package(i);
		pkg->y1 = plugin->text_mask->get_h() * i / get_total_packages();
		pkg->y2 = plugin->text_mask->get_h() * (i + 1) / get_total_packages();
	}
}

void TitleOutlineEngine::do_outline()
{
	pass = 0;
	process_packages();
	pass = 1;
	process_packages();
}

LoadClient* TitleOutlineEngine::new_client()
{
	return new TitleOutlineUnit(plugin, this);
}

LoadPackage* TitleOutlineEngine::new_package()
{
	return new TitleOutlinePackage;
}




TitleTranslatePackage::TitleTranslatePackage()
 : LoadPackage()
{
	y2 = 0;
	y1 = 0;
}


TitleTranslateUnit::TitleTranslateUnit(TitleMain *plugin, TitleTranslate *server)
 : LoadClient(server)
{
	this->plugin = plugin;
}




#define TRANSLATE(type, max, components) \
{ \
	unsigned char **in_rows = plugin->text_mask->get_rows(); \
	type **out_rows = (type**)plugin->output->get_rows(); \
 \
	for(int i = pkg->y1; i < pkg->y2; i++) \
	{ \
		if(i + server->out_y1_int >= 0 && \
			i + server->out_y1_int < server->output_h) \
		{ \
			int in_y1, in_y2; \
			float y_fraction1, y_fraction2; \
			in_y1 = server->y_table[i].in_x1; \
			in_y2 = server->y_table[i].in_x2; \
			y_fraction1 = server->y_table[i].in_fraction1; \
			y_fraction2 = server->y_table[i].in_fraction2; \
			unsigned char *in_row1 = in_rows[in_y1]; \
			unsigned char *in_row2 = in_rows[in_y2]; \
			type *out_row = out_rows[i + server->out_y1_int]; \
 \
			for(int j = server->out_x1_int; j < server->out_x2_int; j++) \
			{ \
				if(j >= 0 && j < server->output_w) \
				{ \
					int in_x1; \
					int in_x2; \
					float x_fraction1; \
					float x_fraction2; \
					in_x1 =  \
						server->x_table[j - server->out_x1_int].in_x1; \
					in_x2 =  \
						server->x_table[j - server->out_x1_int].in_x2; \
					x_fraction1 =  \
						server->x_table[j - server->out_x1_int].in_fraction1; \
					x_fraction2 =  \
						server->x_table[j - server->out_x1_int].in_fraction2; \
 \
					float fraction1 = x_fraction1 * y_fraction1; \
					float fraction2 = x_fraction2 * y_fraction1; \
					float fraction3 = x_fraction1 * y_fraction2; \
					float fraction4 = x_fraction2 * y_fraction2; \
					type input_r = (type)(in_row1[in_x1 * 4 + 0] * fraction1 +  \
								in_row1[in_x2 * 4 + 0] * fraction2 +  \
								in_row2[in_x1 * 4 + 0] * fraction3 +  \
								in_row2[in_x2 * 4 + 0] * fraction4); \
					type input_g = (type)(in_row1[in_x1 * 4 + 1] * fraction1 +  \
								in_row1[in_x2 * 4 + 1] * fraction2 +  \
								in_row2[in_x1 * 4 + 1] * fraction3 +  \
								in_row2[in_x2 * 4 + 1] * fraction4); \
					type input_b = (type)(in_row1[in_x1 * 4 + 2] * fraction1 +  \
								in_row1[in_x2 * 4 + 2] * fraction2 +  \
								in_row2[in_x1 * 4 + 2] * fraction3 +  \
								in_row2[in_x2 * 4 + 2] * fraction4); \
					type input_a = (type)(in_row1[in_x1 * 4 + 3] * fraction1 +  \
								in_row1[in_x2 * 4 + 3] * fraction2 +  \
								in_row2[in_x1 * 4 + 3] * fraction3 +  \
								in_row2[in_x2 * 4 + 3] * fraction4); \
/* Plugin alpha is actually 0 - 0x100 */ \
					input_a = input_a * plugin->alpha / 0x100; \
					type transparency; \
 \
 \
					if(components == 4) \
					{ \
						transparency = out_row[j * components + 3] * (max - input_a) / max; \
						out_row[j * components + 0] =  \
							(input_r * input_a + out_row[j * components + 0] * transparency) / max; \
						out_row[j * components + 1] =  \
							(input_g * input_a + out_row[j * components + 1] * transparency) / max; \
						out_row[j * components + 2] =  \
							(input_b * input_a + out_row[j * components + 2] * transparency) / max; \
						out_row[j * components + 3] =  \
							MAX(input_a, out_row[j * components + 3]); \
					} \
					else \
					{ \
						transparency = max - input_a; \
						out_row[j * components + 0] =  \
							(input_r * input_a + out_row[j * components + 0] * transparency) / max; \
						out_row[j * components + 1] =  \
							(input_g * input_a + out_row[j * components + 1] * transparency) / max; \
						out_row[j * components + 2] =  \
							(input_b * input_a + out_row[j * components + 2] * transparency) / max; \
					} \
				} \
			} \
		} \
	} \
}

#define TRANSLATEA(type, max, components, r, g, b) \
{ \
	unsigned char **in_rows = plugin->text_mask->get_rows(); \
	type **out_rows = (type**)plugin->output->get_rows(); \
 \
	for(int i = pkg->y1; i < pkg->y2; i++) \
	{ \
		if(i + server->out_y1_int >= 0 && \
			i + server->out_y1_int < server->output_h) \
		{ \
			unsigned char *in_row = in_rows[i]; \
			type *out_row = out_rows[i + server->out_y1_int]; \
 \
			for(int j = server->out_x1; j < server->out_x2_int; j++) \
			{ \
				if(j  >= 0 && \
					j < server->output_w) \
				{ \
					int input = (int)(in_row[j - server->out_x1]);  \
 \
					input *= plugin->alpha; \
/* Alpha is 0 - 256 */ \
					input >>= 8; \
 \
					int anti_input = 0xff - input; \
					if(components == 4) \
					{ \
						out_row[j * components + 0] =  \
							(r * input + out_row[j * components + 0] * anti_input) / 0xff; \
						out_row[j * components + 1] =  \
							(g * input + out_row[j * components + 1] * anti_input) / 0xff; \
						out_row[j * components + 2] =  \
							(b * input + out_row[j * components + 2] * anti_input) / 0xff; \
						if(max == 0xffff) \
							out_row[j * components + 3] =  \
								MAX((input << 8) | input, out_row[j * components + 3]); \
						else \
							out_row[j * components + 3] =  \
								MAX(input, out_row[j * components + 3]); \
					} \
					else \
					{ \
						out_row[j * components + 0] =  \
							(r * input + out_row[j * components + 0] * anti_input) / 0xff; \
						out_row[j * components + 1] =  \
							(g * input + out_row[j * components + 1] * anti_input) / 0xff; \
						out_row[j * components + 2] =  \
							(b * input + out_row[j * components + 2] * anti_input) / 0xff; \
					} \
				} \
			} \
		} \
	} \
}

void TitleTranslateUnit::process_package(LoadPackage *package)
{
	TitleTranslatePackage *pkg = (TitleTranslatePackage*)package;
	TitleTranslate *server = (TitleTranslate*)this->server;

	switch(plugin->output->get_color_model()) {
	case BC_RGB888:     TRANSLATE(unsigned char, 0xff, 3); break;
	case BC_RGB_FLOAT:  TRANSLATE(float, 1.0, 3);          break;
	case BC_YUV888:     TRANSLATE(unsigned char, 0xff, 3); break;
	case BC_RGBA_FLOAT: TRANSLATE(float, 1.0, 4);          break;
	case BC_RGBA8888:   TRANSLATE(unsigned char, 0xff, 4); break;
	case BC_YUVA8888:   TRANSLATE(unsigned char, 0xff, 4); break;
	}
//printf("TitleTranslateUnit::process_package 5\n");
}




TitleTranslate::TitleTranslate(TitleMain *plugin, int cpus)
 : LoadServer(1, 1)
{
	this->plugin = plugin;
	x_table = 0;
	y_table = 0;
	out_x1 = out_x2 = 0;
	out_y1 = out_y2 = 0;
	out_x1_int = out_x2_int = 0;
	out_y1_int = out_y2_int = 0;
	output_w = output_h = 0;
}

TitleTranslate::~TitleTranslate()
{
	delete [] x_table;
	delete [] y_table;
}

void TitleTranslate::init_packages()
{
//printf("TitleTranslate::init_packages 1\n");
// Generate scaling tables
	delete [] x_table;
	delete [] y_table;
//printf("TitleTranslate::init_packages 1\n");

	output_w = plugin->output->get_w();
	output_h = plugin->output->get_h();
//printf("TitleTranslate::init_packages 1 %f %d\n", plugin->text_x1, plugin->text_w);


	TranslateUnit::translation_array_f(x_table,
		plugin->text_x1 - plugin->config.outline_size,
		plugin->text_x1 + plugin->text_w - plugin->config.outline_size,
		0,
		plugin->text_w,
		plugin->text_w,
		output_w,
		out_x1_int,
		out_x2_int);
//printf("TitleTranslate::init_packages 1 %f %f\n", plugin->mask_y1, plugin->mask_y2);

	TranslateUnit::translation_array_f(y_table,
		plugin->mask_y1 + plugin->config.outline_size,
		plugin->mask_y1 + plugin->text_mask->get_h() + plugin->config.outline_size,
		0,
		plugin->text_mask->get_h(),
		plugin->text_mask->get_h(),
		output_h,
		out_y1_int,
		out_y2_int);

//printf("TitleTranslate::init_packages 1\n");


	out_y1 = out_y1_int;
	out_y2 = out_y2_int;
	out_x1 = out_x1_int;
	out_x2 = out_x2_int;
	int increment = (out_y2 - out_y1) / get_total_packages() + 1;

//printf("TitleTranslate::init_packages 1 %d %d %d %d\n",
//	out_y1, out_y2, out_y1_int, out_y2_int);
	for(int i = 0; i < get_total_packages(); i++)
	{
		TitleTranslatePackage *pkg = (TitleTranslatePackage*)get_package(i);
		pkg->y1 = i * increment;
		pkg->y2 = i * increment + increment;
		if(pkg->y1 > out_y2 - out_y1)
			pkg->y1 = out_y2 - out_y1;
		if(pkg->y2 > out_y2 - out_y1)
			pkg->y2 = out_y2 - out_y1;
	}
//printf("TitleTranslate::init_packages 2\n");
}

LoadClient* TitleTranslate::new_client()
{
	return new TitleTranslateUnit(plugin, this);
}

LoadPackage* TitleTranslate::new_package()
{
	return new TitleTranslatePackage;
}



ArrayList<FontEntry*>* TitleMain::fonts = 0;


TitleMain::TitleMain(PluginServer *server)
 : PluginVClient(server)
{
	text_mask = 0;
	outline_mask = 0;
	text_mask = 0;
	text_mask_stroke = 0;
	glyph_engine = 0;
	title_engine = 0;
	freetype_library = 0;
	freetype_face = 0;
	char_positions = 0;
	rows_bottom = 0;
	rows_size = 0;
	translate = 0;
	outline_engine = 0;
	need_reconfigure = 1;
}

TitleMain::~TitleMain()
{
	delete text_mask;
	delete outline_mask;
	delete text_mask_stroke;
	delete [] char_positions;
	delete [] rows_bottom;
	delete [] char_positions;
	clear_glyphs();
	delete glyph_engine;
	delete title_engine;
	if( freetype_face ) FT_Done_Face(freetype_face);
	if( freetype_library ) FT_Done_FreeType(freetype_library);
	delete translate;
	delete outline_engine;
}

const char* TitleMain::plugin_title() { return N_("Title"); }
int TitleMain::is_realtime() { return 1; }
int TitleMain::is_synthesis() { return 1; }

VFrame* TitleMain::new_picon()
{
	return new VFrame(picon_png);
}

NEW_WINDOW_MACRO(TitleMain, TitleWindow);

void TitleMain::build_fonts()
{
	if(!fonts)
	{
		fonts = new ArrayList<FontEntry*>;
		char find_command[BCTEXTLEN];
		sprintf(find_command,
			"find %s%s -name 'fonts.dir' -print -exec cat {} \\;",
			PluginClient::get_plugin_dir(),
			FONT_SEARCHPATH);
		FILE *in = popen(find_command, "r");
		char current_dir[BCTEXTLEN];
		FT_Library freetype_library = 0;      	// Freetype library
//		FT_Face freetype_face = 0;

//		FT_Init_FreeType(&freetype_library);
		current_dir[0] = 0;

		while(!feof(in))
		{
			char string[BCTEXTLEN], string2[BCTEXTLEN];
			(void)fgets(string, BCTEXTLEN, in);
			if(!strlen(string)) break;

			char *in_ptr = string;
			char *out_ptr;

// Get current directory

			if(string[0] == '/')
			{
				out_ptr = current_dir;
				while(*in_ptr != 0 && *in_ptr != '\n')
					*out_ptr++ = *in_ptr++;
				out_ptr--;
				while(*out_ptr != '/')
					*out_ptr-- = 0;
			}
			else
			{


//printf("TitleMain::build_fonts %s\n", string);
				FontEntry *entry = new FontEntry;

// Path
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n')
				{
					if(*in_ptr == ' ' && *(in_ptr + 1) == '-') break;
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				if(string2[0] == '/')
				{
					entry->path = new char[strlen(string2) + 1];
					sprintf(entry->path, "%s", string2);
				}
				else
				{
					entry->path = new char[strlen(current_dir) + strlen(string2) + 1];
					sprintf(entry->path, "%s%s", current_dir, string2);
				}

// Foundary
				while(*in_ptr != 0 && *in_ptr != '\n' && (*in_ptr == ' ' || *in_ptr == '-'))
					in_ptr++;

				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != ' ' && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->foundary = new char[strlen(string2) + 1];
				strcpy(entry->foundary, string2);
				if(*in_ptr == '-') in_ptr++;


// Family
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->family = new char[strlen(string2) + 1];
				strcpy(entry->family, string2);
				if(*in_ptr == '-') in_ptr++;

// Weight
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->weight = new char[strlen(string2) + 1];
				strcpy(entry->weight, string2);
				if(*in_ptr == '-') in_ptr++;

// Slant
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->slant = new char[strlen(string2) + 1];
				strcpy(entry->slant, string2);
				if(*in_ptr == '-') in_ptr++;

// SWidth
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->swidth = new char[strlen(string2) + 1];
				strcpy(entry->swidth, string2);
				if(*in_ptr == '-') in_ptr++;

// Adstyle
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->adstyle = new char[strlen(string2) + 1];
				strcpy(entry->adstyle, string2);
				if(*in_ptr == '-') in_ptr++;

// pixelsize
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->pixelsize = atol(string2);
				if(*in_ptr == '-') in_ptr++;

// pointsize
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->pointsize = atol(string2);
				if(*in_ptr == '-') in_ptr++;

// xres
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->xres = atol(string2);
				if(*in_ptr == '-') in_ptr++;

// yres
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->yres = atol(string2);
				if(*in_ptr == '-') in_ptr++;

// spacing
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->spacing = new char[strlen(string2) + 1];
				strcpy(entry->spacing, string2);
				if(*in_ptr == '-') in_ptr++;

// avg_width
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->avg_width = atol(string2);
				if(*in_ptr == '-') in_ptr++;

// registry
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n' && *in_ptr != '-')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->registry = new char[strlen(string2) + 1];
				strcpy(entry->registry, string2);
				if(*in_ptr == '-') in_ptr++;

// encoding
				out_ptr = string2;
				while(*in_ptr != 0 && *in_ptr != '\n')
				{
					*out_ptr++ = *in_ptr++;
				}
				*out_ptr = 0;
				entry->encoding = new char[strlen(string2) + 1];
				strcpy(entry->encoding, string2);



// Add to list
				if(strlen(entry->foundary))
				{
//printf("TitleMain::build_fonts 1 %s\n", entry->path);
// This takes a real long time to do.  Instead just take all fonts
// 					if(!load_freetype_face(freetype_library,
// 						freetype_face,
// 						entry->path))
//					if(1)
					if(entry->family[0])
					{
// Fix parameters
						sprintf(string, "%s (%s)", entry->family, entry->foundary);
						entry->fixed_title = new char[strlen(string) + 1];
						strcpy(entry->fixed_title, string);

						if(!strcasecmp(entry->weight, "demibold") ||
							!strcasecmp(entry->weight, "bold"))
							entry->fixed_style |= FONT_BOLD;
						if(!strcasecmp(entry->slant, "i") ||
							!strcasecmp(entry->slant, "o"))
							entry->fixed_style |= FONT_ITALIC;
						fonts->append(entry);
//						printf("TitleMain::build_fonts %s: success\n",
//							entry->path);
//printf("TitleMain::build_fonts 2\n");
					}
					else
					{
//						printf("TitleMain::build_fonts %s: FT_New_Face failed\n",
//							entry->path);
//printf("TitleMain::build_fonts 3\n");
						delete entry;
					}
				}
				else
				{
					delete entry;
				}
			}
		}
		pclose(in);


// Load all the fonts from fontconfig
		FcPattern *pat;
		FcFontSet *fs;
		FcObjectSet *os;
		FcChar8 *family,
			*file,
			*foundry,
			*style,
			*format;
		int slant,
			spacing,
			width,
			weight;
		int force_style = 0;
		int limit_to_trutype = 0; // if you want limit search to TrueType put 1
		FcConfig *config;
		FcBool resultfc;
		int i;
		char tmpstring[BCTEXTLEN];
		resultfc = FcInit();
		config = FcConfigGetCurrent();
		FcConfigSetRescanInterval(config, 0);

		pat = FcPatternCreate();
		os = FcObjectSetBuild ( FC_FAMILY,
					FC_FILE,
					FC_FOUNDRY,
					FC_WEIGHT,
					FC_WIDTH,
					FC_SLANT,
					FC_FONTFORMAT,
					FC_SPACING,
					FC_STYLE,
					(char *) 0);
		fs = FcFontList(config, pat, os);
		FcPattern *font;

		for (i = 0; fs && i < fs->nfont; i++)
		{
			font = fs->fonts[i];
			force_style = 0;
			FcPatternGetString(font, FC_FONTFORMAT, 0, &format);
			if((!strcmp((char *)format, "TrueType")) || limit_to_trutype) //on this point you can limit font search
			{
				sprintf(tmpstring, "%s", format);

				FontEntry *entry = new FontEntry;

				if(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
				{
					sprintf(tmpstring, "%s", file);
					entry->path = new char[strlen(tmpstring) + 1];
					sprintf(entry->path, "%s", tmpstring);
				}

				if(FcPatternGetString(font, FC_FOUNDRY, 0, &foundry) == FcResultMatch)
				{
					sprintf(tmpstring, "%s", foundry);
					entry->foundary = new char[strlen(tmpstring) + 2];
					strcpy(entry->foundary, tmpstring);
				}

				if(FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) == FcResultMatch)
				{
					switch(weight)
					{
						case FC_WEIGHT_THIN:
						case FC_WEIGHT_EXTRALIGHT:
						case FC_WEIGHT_LIGHT:
						case FC_WEIGHT_BOOK:
							force_style = 1;
							entry->weight = new char[strlen("medium") + 1];
							strcpy(entry->weight, "medium");
							break;

						case FC_WEIGHT_NORMAL:
						case FC_WEIGHT_MEDIUM:
						default:
							entry->weight = new char[strlen("medium") + 1];
							strcpy(entry->weight, "medium");
							break;

						case FC_WEIGHT_BLACK:
						case FC_WEIGHT_SEMIBOLD:
						case FC_WEIGHT_BOLD:
							entry->weight = new char[strlen("bold") + 1];
							strcpy(entry->weight, "bold");
							entry->fixed_style |= FONT_BOLD;
							break;

						case FC_WEIGHT_EXTRABOLD:
						case FC_WEIGHT_EXTRABLACK:
							force_style = 1;
							entry->weight = new char[strlen("bold") + 1];
							strcpy(entry->weight, "bold");
							entry->fixed_style |= FONT_BOLD;
							break;
						break;
					}
				}

				if(FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch)
				{
					sprintf(tmpstring, "%s", family);
					entry->family = new char[strlen(tmpstring) + 2];
					strcpy(entry->family, tmpstring);
				}

				if(FcPatternGetInteger(font, FC_SLANT, 0, &slant) == FcResultMatch)
				{
					switch(slant)
					{
						case FC_SLANT_ROMAN:
						default:
							entry->slant = new char[strlen("r") + 1];
							strcpy(entry->slant, "r");
							break;
						case FC_SLANT_ITALIC:
							entry->slant = new char[strlen("i") + 1];
							strcpy(entry->slant, "i");
							entry->fixed_style |= FONT_ITALIC;
							break;
						case FC_SLANT_OBLIQUE:
							entry->slant = new char[strlen("o") + 1];
							strcpy(entry->slant, "o");
							entry->fixed_style |= FONT_ITALIC;
							break;
					}
				}

				if(FcPatternGetInteger(font, FC_WIDTH, 0, &width) == FcResultMatch)
				{
					switch(width)
					{
						case FC_WIDTH_ULTRACONDENSED:
							entry->swidth = new char[strlen("ultracondensed") + 1];
							strcpy(entry->swidth, "ultracondensed");
							break;

						case FC_WIDTH_EXTRACONDENSED:
							entry->swidth = new char[strlen("extracondensed") + 1];
							strcpy(entry->swidth, "extracondensed");
							break;

						case FC_WIDTH_CONDENSED:
							entry->swidth = new char[strlen("condensed") + 1];
							strcpy(entry->swidth, "condensed");
							break;
						case FC_WIDTH_SEMICONDENSED:
							entry->swidth = new char[strlen("semicondensed") + 1];
							strcpy(entry->swidth, "semicondensed");
							break;

						case FC_WIDTH_NORMAL:
						default:
							entry->swidth = new char[strlen("normal") + 1];
							strcpy(entry->swidth, "normal");
							break;

						case FC_WIDTH_SEMIEXPANDED:
							entry->swidth = new char[strlen("semiexpanded") + 1];
							strcpy(entry->swidth, "semiexpanded");
							break;

						case FC_WIDTH_EXPANDED:
							entry->swidth = new char[strlen("expanded") + 1];
							strcpy(entry->swidth, "expanded");
							break;

						case FC_WIDTH_EXTRAEXPANDED:
							entry->swidth = new char[strlen("extraexpanded") + 1];
							strcpy(entry->swidth, "extraexpanded");
							break;

						case FC_WIDTH_ULTRAEXPANDED:
							entry->swidth = new char[strlen("ultraexpanded") + 1];
							strcpy(entry->swidth, "ultraexpanded");
							break;
					}
				}

				if(FcPatternGetInteger(font, FC_SPACING, 0, &spacing) == FcResultMatch)
				{
					switch(spacing)
					{
						case 0:
						default:
							entry->spacing = new char[strlen("p") + 1];
							strcpy(entry->spacing, "p");
							break;

						case 90:
							entry->spacing = new char[strlen("d") + 1];
							strcpy(entry->spacing, "d");
							break;

						case 100:
							entry->spacing = new char[strlen("m") + 1];
							strcpy(entry->spacing, "m");
							break;

						case 110:
							entry->spacing = new char[strlen("c") + 1];
							strcpy(entry->spacing, "c");
							break;
					}

				}

				// Add fake stuff for compatibility
				entry->adstyle = new char[strlen(" ") + 1];
				strcpy(entry->adstyle, " ");
				entry->pixelsize = 0;
				entry->pointsize = 0;
				entry->xres = 0;
				entry->yres = 0;
				entry->avg_width = 0;
				entry->registry = new char[strlen("utf") + 1];
				strcpy(entry->registry, "utf");
				entry->encoding = new char[strlen("8") + 1];
				strcpy(entry->encoding, "8");

				if(!FcPatternGetString(font, FC_STYLE, 0, &style) == FcResultMatch) force_style = 0;

				// If font has a style unmanaged by titler plugin, force style to be displayed on name
				// in this way we can shown all available fonts styles.
				if(force_style)
				{
					sprintf(tmpstring, "%s (%s)", entry->family, style);
					entry->fixed_title = new char[strlen(tmpstring) + 1];
					strcpy(entry->fixed_title, tmpstring);
				}
				else
				{
					if(strcmp(entry->foundary, "unknown"))
					{
						sprintf(tmpstring, "%s (%s)", entry->family, entry->foundary);
						entry->fixed_title = new char[strlen(tmpstring) + 1];
						strcpy(entry->fixed_title, tmpstring);
					}
					else
					{
						sprintf(tmpstring, "%s", entry->family);
						entry->fixed_title = new char[strlen(tmpstring) + 1];
						strcpy(entry->fixed_title, tmpstring);
					}

				}
				fonts->append(entry);
			}
		}
		if(fs) FcFontSetDestroy(fs);






		if(freetype_library) FT_Done_FreeType(freetype_library);
	}


// for(int i = 0; i < fonts->total; i++)
//	fonts->values[i]->dump();


}


void TitleMain::build_previews(TitleWindow *gui)
{
	for(int font_number = 0; font_number < fonts->size(); font_number++)
	{
		FontEntry *font_entry = fonts->get(font_number);
// already have examples
		if(font_entry->image) return;
	}

// create example bitmaps
	FT_Library freetype_library = 0;      	// Freetype library
	FT_Face freetype_face = 0;
	const char *test_string = "Aa";
	char new_path[BCTEXTLEN];
	int text_height = gui->get_text_height(LARGEFONT);
	int text_color = BC_WindowBase::get_resources()->default_text_color;
	int r = (text_color >> 16) & 0xff;
	int g = (text_color >> 8) & 0xff;
	int b = text_color & 0xff;
// dimensions for each line
	int height[fonts->size()];
	int ascent[fonts->size()];

// pass 1 gets the extents for all the fonts
// pass 2 draws the image
	int total_w = 0;
	int total_h = 0;
	int total_ascent = 0;
	for(int pass = 0; pass < 2; pass++)
	{
//printf("TitleMain::build_previews %d %d %d\n",
//__LINE__,
//text_height,
//total_h);
		for(int font_number = 0; font_number < fonts->size(); font_number++)
		{
			FontEntry *font_entry = fonts->get(font_number);

// test if font of same name has been processed
			int skip = 0;
			for(int i = 0; i < font_number; i++)
			{
				if(!strcasecmp(fonts->get(i)->fixed_title,
					font_entry->fixed_title))
				{
					if(pass == 1)
					{
						font_entry->image = fonts->get(i)->image;
					}

					skip = 1;
					break;
				}
			}


			if(skip) continue;

			int current_x = 0;
			int current_w = 0;
			int current_ascent = 0;
			int current_h = 0;
			if(pass == 1)
			{
				font_entry->image = new VFrame;
				font_entry->image->set_use_shm(0);
				font_entry->image->reallocate(0,
					-1,
					0,
					0,
					0,
					total_w,
					total_h,
					BC_RGBA8888,
					-1);
				font_entry->image->clear_frame();
			}

			current_x = 0;
			current_w = 0;
			for(int j = 0; j < strlen(test_string); j++)
			{
				FT_ULong c = test_string[j];
				check_char_code_path(freetype_library,
					font_entry->path,
					c,
					(char *)new_path);
				if(!load_freetype_face(freetype_library,
					freetype_face,
					new_path))
				{
					FT_Set_Pixel_Sizes(freetype_face, text_height, 0);

					if(!FT_Load_Char(freetype_face, c, FT_LOAD_RENDER))
					{
						if(pass == 0)
						{
							current_w = current_x + freetype_face->glyph->bitmap.width;
							if(freetype_face->glyph->bitmap_top > current_ascent)
								current_ascent = freetype_face->glyph->bitmap_top;
							if(freetype_face->glyph->bitmap.rows > total_h)
								total_h = freetype_face->glyph->bitmap.rows;
							if(freetype_face->glyph->bitmap.rows > current_h)
								current_h = freetype_face->glyph->bitmap.rows;
						}
						else
						{
// copy 1 row at a time
// center vertically
							int out_y = (total_h - height[font_number]) / 2 +
								ascent[font_number] - freetype_face->glyph->bitmap_top;
							for(int in_y = 0;
								in_y < freetype_face->glyph->bitmap.rows &&
									out_y < total_h;
								in_y++, out_y++)
							{
								unsigned char *out_row = font_entry->image->get_rows()[out_y] +
									current_x * 4;
								unsigned char *in_row = freetype_face->glyph->bitmap.buffer +
									freetype_face->glyph->bitmap.pitch * in_y;

								for(int out_x = 0; out_x < freetype_face->glyph->bitmap.width &&
									out_x < total_w;
									out_x++)
								{
									*out_row++ = (*in_row * r +
										(0xff - *in_row) * *out_row) / 0xff;
									*out_row++ = (*in_row * g +
										(0xff - *in_row) * *out_row) / 0xff;
									*out_row++ = (*in_row * b +
										(0xff - *in_row) * *out_row) / 0xff;
									*out_row++ = MAX(*in_row, *out_row);
									in_row++;
								}
							}
						}


						current_x += freetype_face->glyph->advance.x >> 6;


// if(pass == 0)
// {
// printf("TitleMain::build_fonts %d %c %d %d\n",
// __LINE__,
// c,
// freetype_face->glyph->advance.x >> 6,
// freetype_face->glyph->bitmap.width);
// }
					}
				}
			}

			height[font_number] = current_h;
			ascent[font_number] = current_ascent;
			if(pass == 0 && current_w > total_w) total_w = current_w;

		}
	}

	if(freetype_library) FT_Done_FreeType(freetype_library);
}



//This checks if char_code is on the selected font, else it changes font to the first compatible //Akirad
int TitleMain::check_char_code_path(FT_Library &freetype_library,
	char *path_old,
	FT_ULong &char_code,
	char *path_new)
{
	FT_Face temp_freetype_face;
	FcPattern *pat;
	FcFontSet *fs;
	FcObjectSet *os;
	FcChar8 *file, *format;
	FcConfig *config;
	FcBool resultfc;
	int i;

	resultfc = FcInit();
	config = FcConfigGetCurrent();
	FcConfigSetRescanInterval(config, 0);

	pat = FcPatternCreate();
	os = FcObjectSetBuild ( FC_FILE, FC_FONTFORMAT, (char *) 0);
	fs = FcFontList(config, pat, os);
	FcPattern *font;
	int notfindit = 1;
	char tmpstring[BCTEXTLEN];
	int limit_to_truetype = 0; //if you want to limit search to truetype put 1
	if(!freetype_library) FT_Init_FreeType(&freetype_library);
	if(!FT_New_Face(freetype_library,
					path_old,
					0,
					&temp_freetype_face))
	{
		FT_Set_Pixel_Sizes(temp_freetype_face, 128, 0);
		int gindex = FT_Get_Char_Index(temp_freetype_face, char_code);
		if((!gindex == 0) && (!char_code != 10))
		{
			strcpy(path_new, path_old);
			notfindit = 0;
		}
	}

	if(notfindit)
	{
		for (i=0; fs && i < fs->nfont; i++)
		{
			font = fs->fonts[i];
			FcPatternGetString(font, FC_FONTFORMAT, 0, &format);
			if((!strcmp((char *)format, "TrueType")) || limit_to_truetype)
			{
				if(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
				{

					sprintf(tmpstring, "%s", file);
					if(!FT_New_Face(freetype_library,
								tmpstring,
								0,
								&temp_freetype_face))
					{
						FT_Set_Pixel_Sizes(temp_freetype_face, 128, 0);
						int gindex = FT_Get_Char_Index(temp_freetype_face, char_code);
						if((!gindex == 0) && (!char_code != 10))
						{
							sprintf(path_new, "%s", tmpstring);
							notfindit = 0;
							goto done;

						}
					}
				}
			}
		}
	}

done:
	if(fs) FcFontSetDestroy(fs);
	if(temp_freetype_face) FT_Done_Face(temp_freetype_face);
	temp_freetype_face = 0;

	if(notfindit)
	{
		strcpy(path_new, path_old);
		return 1;
	}

	return 0;
}




int TitleMain::load_freetype_face(FT_Library &freetype_library,
	FT_Face &freetype_face,
	const char *path)
{
//printf("TitleMain::load_freetype_face 1\n");
	if(!freetype_library) FT_Init_FreeType(&freetype_library);
	if(freetype_face) FT_Done_Face(freetype_face);
	freetype_face = 0;
//printf("TitleMain::load_freetype_face 2\n");

// Use freetype's internal function for loading font
	if(FT_New_Face(freetype_library, path, 0, &freetype_face))
	{
		fprintf(stderr, _("TitleMain::load_freetype_face %s failed.\n"), path);
		freetype_face = 0;
		freetype_library = 0;
		return 1;
	}
	return 0;
}

BC_FontEntry* TitleMain::get_font()
{
	int style = 0;
	int mask;

	style |= (config.style & FONT_ITALIC) ? FL_SLANT_ITALIC : FL_SLANT_ROMAN;
	style |= (config.style & FONT_BOLD) ? FL_WEIGHT_BOLD : FL_WEIGHT_NORMAL;

	mask = FL_WEIGHT_MASK | FL_SLANT_MASK;

	BC_Resources *resources =  BC_WindowBase::get_resources();
	return resources->find_fontentry(config.font, style, mask);
}

int TitleMain::get_char_height()
{
// this is height above the zero line, but does not include characters that go below
	int result = config.size;
	if((config.style & FONT_OUTLINE)) result += (int)ceil(config.stroke_width * 2);
	return result;
}

TitleGlyph *TitleMain::get_glyph(FT_ULong char_code)
{
	for(int i = 0; i < glyphs.size(); i++) {
		if(glyphs.get(i)->char_code == char_code)
			return glyphs.get(i);
	}
	return 0;
}

int TitleMain::get_char_width(FT_ULong char_code)
{
	if(char_code == '\n') return 0;
	TitleGlyph *glyph = get_glyph(char_code);
	return !glyph ? 0 : glyph->width;
}

int TitleMain::get_char_advance(FT_ULong current, FT_ULong next)
{
	FT_Vector kerning;

	if(current == '\n') return 0;
	TitleGlyph *current_glyph = get_glyph(current);
	int result = !current_glyph ? 0 : current_glyph->advance_w;
	TitleGlyph *next_glyph = !next ? 0 : get_glyph(next);
	if(next_glyph)
		FT_Get_Kerning(freetype_face,
				current_glyph->freetype_index,
				next_glyph->freetype_index,
				ft_kerning_default,
				&kerning);
	else
		kerning.x = 0;
	return result + (kerning.x >> 6);
}

void TitleMain::draw_glyphs()
{
// Build table of all glyphs needed
	int total_packages = 0;

	for(int i = 0; i < config.wlen; i++)
	{
		int exists = 0;
		FT_ULong char_code = config.wtext[i];

		for(int j = 0; j < glyphs.total; j++) {
			if(glyphs.values[j]->char_code == char_code) {
				exists = 1;
				break;
			}
		}

		if(!exists) {
			total_packages++;
//printf("TitleMain::draw_glyphs 1\n");
			TitleGlyph *glyph = new TitleGlyph;
//printf("TitleMain::draw_glyphs 2\n");
			glyphs.append(glyph);
			glyph->char_code = char_code;
		}
	}

	if(!glyph_engine)
		glyph_engine = new GlyphEngine(this, PluginClient::smp + 1);

	glyph_engine->set_package_count(total_packages);
//printf("TitleMain::draw_glyphs 3 %d\n", glyphs.total);
	glyph_engine->process_packages();
//printf("TitleMain::draw_glyphs 4\n");
}


void TitleMain::get_total_extents()
{
// Determine extents of total text
	int wlen = config.wlen;
	if(!char_positions)
		char_positions = new title_char_position_t[wlen];

	// get the number of rows first
	text_rows = 0;
	for(int i = 0; i < wlen; i++) {
		if(config.wtext[i] == '\n' ) text_rows++;
	}
	if( wlen > 0 && config.wtext[wlen-1] != '\n' ) text_rows++;

	if( rows_size < text_rows+1 ) {
		delete rows_bottom;
		rows_bottom = 0;
	}
	if (!rows_bottom) {
		rows_size = text_rows+1;
		rows_bottom = new int[rows_size];
	}

	int row_descent = 0;
	int row_w = 0, row_h = 0;
	int max_char_w = 0, max_char_h = 0;
	int row = 0;

	text_w = 0;  text_h = 0;
	for(int i = 0; i < wlen; i++) {
		char_positions[i].x = row_w;
		char_positions[i].y = text_h;
		wchar_t wchar = config.wtext[i];
		if( wchar == '\n' ) {
			rows_bottom[row++] = row_descent;
			if(row_w > text_w) text_w = row_w;
			text_h += row_h;
		}
		else {
			TitleGlyph *glyph = get_glyph(wchar);
			if( !glyph ) continue;
			int char_w = i+1>=wlen ? glyph->width :
				get_char_advance(wchar, config.wtext[i+1]);
			if( char_w > max_char_w ) max_char_w = char_w;
			char_positions[i].w = char_w;
			int char_h = glyph->height;
			if( char_h > max_char_h ) max_char_h = char_h;
			int descent = glyph->top - char_h;
			if(descent < row_descent) row_descent = descent;
			row_w += char_w;
			if(glyph->height > row_h ) row_h = glyph->height;
			row_w = row_h = 0;
			row_descent = 0;
		}			
	}
	if( wlen > 0 && config.wtext[wlen-1] != '\n' ) {
		rows_bottom[row++] = row_descent;
		if(row_w > text_w) text_w = row_w;
		text_h += row_h;
	}

	text_w += config.dropshadow + config.outline_size * 4;
	if(config.hjustification == JUSTIFY_MID) text_w += max_char_w;
	text_h += config.dropshadow + config.outline_size * 4;

	int row_start = 0;
	for(int i = 0; i < wlen; i++) {
		wchar_t wchar = config.wtext[i];
		if(wchar != '\n' && i != wlen-1) continue;
		for(int j = row_start; j <= i; j++) {
			switch(config.hjustification) {
			case JUSTIFY_LEFT:
				break;
			case JUSTIFY_MID:
				char_positions[j].x += (text_w - char_positions[i].x - char_positions[i].w) / 2;
				break;
			case JUSTIFY_RIGHT:
				char_positions[j].x += (text_w - char_positions[i].x - char_positions[i].w);
				break;
			}
		}
		row_start = i + 1;
	}
}

int TitleMain::draw_mask()
{
	int old_visible_row1 = visible_row1;
	int old_visible_row2 = visible_row2;


// Determine y of visible text
	if(config.motion_strategy == BOTTOM_TO_TOP)
	{
// printf("TitleMain::draw_mask 1 %d %lld %lld %lld\n",
// 	config.motion_strategy,
// 	get_source_position(),
// 	get_source_start(),
// 	config.prev_keyframe_position);
		float magnitude = config.pixels_per_second *
			(get_source_position() - config.prev_keyframe_position) /
			PluginVClient::project_frame_rate;
		if(config.loop)
		{
			int loop_size = text_h + input->get_h();
			magnitude -= (int)(magnitude / loop_size) * loop_size;
		}
		text_y1 = config.y + input->get_h() - magnitude;
	}
	else
	if(config.motion_strategy == TOP_TO_BOTTOM)
	{
		float magnitude = config.pixels_per_second *
			(get_source_position() - config.prev_keyframe_position) /
			PluginVClient::project_frame_rate;
		if(config.loop)
		{
			int loop_size = text_h + input->get_h();
			magnitude -= (int)(magnitude / loop_size) * loop_size;
		}
		text_y1 = config.y + magnitude;
		text_y1 -= text_h;
	}
	else
	if(config.vjustification == JUSTIFY_TOP)
	{
		text_y1 = config.y;
	}
	else
	if(config.vjustification == JUSTIFY_MID)
	{
		text_y1 = config.y + input->get_h() / 2 - text_h / 2;
	}
	else
	if(config.vjustification == JUSTIFY_BOTTOM)
	{
		text_y1 = config.y + input->get_h() - text_h;
	}

	text_y2 = text_y1 + text_h + 0.5;

// Determine x of visible text
	if(config.motion_strategy == RIGHT_TO_LEFT)
	{
		float magnitude = config.pixels_per_second *
			(get_source_position() - config.prev_keyframe_position) /
			PluginVClient::project_frame_rate;
		if(config.loop)
		{
			int loop_size = text_w + input->get_w();
			magnitude -= (int)(magnitude / loop_size) * loop_size;
		}
		text_x1 = config.x + (float)input->get_w() - magnitude;
	}
	else
	if(config.motion_strategy == LEFT_TO_RIGHT)
	{
		float magnitude = config.pixels_per_second *
			(get_source_position() - config.prev_keyframe_position) /
			PluginVClient::project_frame_rate;
		if(config.loop)
		{
			int loop_size = text_w + input->get_w();
			magnitude -= (int)(magnitude / loop_size) * loop_size;
		}
		text_x1 = config.x + -(float)text_w + magnitude;
	}
	else
	if(config.hjustification == JUSTIFY_LEFT)
	{
		text_x1 = config.x;
	}
	else
	if(config.hjustification == JUSTIFY_MID)
	{
		text_x1 = config.x + input->get_w() / 2 - text_w / 2;
	}
	else
	if(config.hjustification == JUSTIFY_RIGHT)
	{
		text_x1 = config.x + input->get_w() - text_w;
	}

	visible_row1 = 0;
	while( visible_row1 < text_rows ) {
		if( rows_bottom[visible_row1]+text_y1 > 0 ) break;
		++visible_row1;
	}

	visible_row2 = visible_row1+1;
	while( visible_row2 < text_rows ) {
		if( rows_bottom[visible_row2-1]+text_y1 >= input->get_h() ) break;
		++visible_row1;
	}

// Determine y extents just of visible text
	mask_y1 = text_y1 + rows_bottom[visible_row1 <= 0 ? 0 : visible_row1-1];
	mask_y2 = text_y1 + visible_row2 >= text_rows ? text_h : rows_bottom[visible_row2];
	if( mask_y1 < 0 ) mask_y1 = 0;
	if( mask_y2 < 0 ) mask_y2 = 0;
	if( mask_y1 > input->get_h() ) mask_y1 = input->get_h();
	if( mask_y2 > input->get_h() ) mask_y2 = input->get_h();
	int mask_h = mask_y2 - mask_y1;

	int mask_x1 = text_x1;
	int mask_x2 = text_x1 + text_w;
	if( mask_x1 < 0 ) mask_x1 = 0;
	if( mask_x2 < 0 ) mask_x2 = 0;
	if( mask_x1 > input->get_w() ) mask_x1 = input->get_w();
	if( mask_x2 > input->get_w() ) mask_x2 = input->get_w();
	int mask_w = mask_x2 - mask_x1;

//printf("TitleMain::draw_mask %d %d\n", visible_row1, visible_row2);
	visible_char1 = visible_char2 = -1;
	int wlen = config.wlen;
	for(int i = 0; i < wlen; i++) {
		title_char_position_t *char_position = char_positions + i;
		if( char_position->y+text_y1 < mask_y1 ) continue;
		if( char_position->y+text_y1 >= mask_y2 ) break;
		if(visible_char1 < 0) visible_char1 = i;
		visible_char2 = i;
	}
	visible_char2++;
	
	int need_redraw = 0;
	if(text_mask && (text_mask->get_w() != mask_w || text_mask->get_h() != mask_h)) {
		delete text_mask;         text_mask = 0;
		delete text_mask_stroke;  text_mask_stroke = 0;
	}

	if(!text_mask) {
// Always use 8 bit because the glyphs are 8 bit
// Need to set YUV to get clear_frame to set the right chroma.
		int output_model = get_output()->get_color_model();
		int color_model = BC_CModels::is_yuv(output_model) ? BC_YUVA8888 : BC_RGBA8888;
		text_mask = new VFrame;
		text_mask->set_use_shm(0);
		text_mask->reallocate(0, -1, 0, 0, 0, mask_w, mask_h, color_model, -1);
		need_redraw = 1;
	}

// Draw on text mask if it has changed
	if(old_visible_row1 != visible_row1 ||
		old_visible_row2 != visible_row2 ||
		need_redraw)
	{
		text_mask->clear_frame();
		text_mask_stroke->clear_frame();

		if(!title_engine)
			title_engine = new TitleEngine(this, PluginClient::smp + 1);

// Draw dropshadow first
		if(config.dropshadow) {
			title_engine->do_dropshadow = 1;
			title_engine->set_package_count(visible_char2 - visible_char1);
			title_engine->process_packages();
		}

// Then draw foreground
		title_engine->do_dropshadow = 0;
		title_engine->set_package_count(visible_char2 - visible_char1);
		title_engine->process_packages();

// Convert to text outlines
		if(config.outline_size > 0) {
			if(outline_mask &&
			    (text_mask->get_w() != outline_mask->get_w() ||
			     text_mask->get_h() != outline_mask->get_h())) {
				delete outline_mask;
				outline_mask = 0;
			}

			if(!outline_mask) {
				outline_mask = new VFrame;
				outline_mask->set_use_shm(0);
				outline_mask->reallocate(0, -1, 0, 0, 0,
					text_mask->get_w(), text_mask->get_h(),
					text_mask->get_color_model(), -1);
			}

			if(!outline_engine) outline_engine =
				new TitleOutlineEngine(this, PluginClient::smp + 1);
			outline_engine->do_outline();
		}
	}

	return 0;
}

void TitleMain::overlay_mask()
{

//printf("TitleMain::overlay_mask 1\n");
        alpha = 0x100;
        if(!EQUIV(config.fade_in, 0))
        {
		int fade_len = lroundf(config.fade_in * PluginVClient::project_frame_rate);
		int fade_position = get_source_position() - config.prev_keyframe_position;

		if(fade_position >= 0 && fade_position < fade_len)
		{
			alpha = lroundf(256.0f * fade_position / fade_len);
		}
	}
        if(!EQUIV(config.fade_out, 0))
        {
		int fade_len = lroundf(config.fade_out * PluginVClient::project_frame_rate);
		int fade_position = config.next_keyframe_position - get_source_position();


		if(fade_position >= 0 && fade_position < fade_len)
		{
			alpha = lroundf(256.0f * fade_position / fade_len);
		}
	}

	if(config.dropshadow)
	{
		text_x1 += config.dropshadow;
		mask_y1 += config.dropshadow;
		mask_y2 += config.dropshadow;
		if(text_x1 < input->get_w() && text_x1 + text_w > 0 &&
			mask_y1 < input->get_h() && mask_y2 > 0)
		{
			if(!translate)
				translate = new TitleTranslate(this, PluginClient::smp + 1);
// Do 2 passes if dropshadow.
			int temp_color = config.color;
			config.color = 0x0;
			translate->process_packages();
			config.color = temp_color;
		}
		text_x1 -= config.dropshadow;
		mask_y1 -= config.dropshadow;
		mask_y2 -= config.dropshadow;
	}
//printf("TitleMain::overlay_mask 1\n");

	if(text_x1 < input->get_w() && text_x1 + text_w > 0 &&
	    mask_y1 < input->get_h() && mask_y2 > 0) {
		if(!translate)
			translate = new TitleTranslate(this, PluginClient::smp + 1);
		translate->process_packages();
		if (config.stroke_width >= ZERO && (config.style & FONT_OUTLINE)) {
			int temp_color = config.color;
			VFrame *tmp_text_mask = this->text_mask;
			config.color = config.color_stroke;
			this->text_mask = this->text_mask_stroke;

			translate->process_packages();
			config.color = temp_color;
			this->text_mask = tmp_text_mask;
		}
	}
//printf("TitleMain::overlay_mask 200\n");
}

void TitleMain::get_color_components(int *r, int *g, int *b, int *a, int is_outline)
{
	int r_in, g_in, b_in, a_in;

	if(is_outline)
	{
		r_in = (config.outline_color & 0xff0000) >> 16;
		g_in = (config.outline_color & 0xff00) >> 8;
		b_in = config.outline_color & 0xff;
		a_in = config.outline_alpha;
	}
	else
	{
		r_in = (config.color & 0xff0000) >> 16;
		g_in = (config.color & 0xff00) >> 8;
		b_in = config.color & 0xff;
		a_in = config.alpha;
	}
	*r = r_in;
	*g = g_in;
	*b = b_in;
	*a = a_in;

	switch(output->get_color_model())
	{
		case BC_YUV888:
			yuv.rgb_to_yuv_8(r_in, g_in, b_in, *r, *g, *b);
			break;
		case BC_YUVA8888:
			yuv.rgb_to_yuv_8(r_in, g_in, b_in, *r, *g, *b);
			break;
	}
}

void TitleMain::clear_glyphs()
{
//printf("TitleMain::clear_glyphs 1\n");
	glyphs.remove_all_objects();
}

const char* TitleMain::motion_to_text(int motion)
{
	switch(motion)
	{
		case NO_MOTION: return _("No motion"); break;
		case BOTTOM_TO_TOP: return _("Bottom to top"); break;
		case TOP_TO_BOTTOM: return _("Top to bottom"); break;
		case RIGHT_TO_LEFT: return _("Right to left"); break;
		case LEFT_TO_RIGHT: return _("Left to right"); break;
	}
	return "";
}

int TitleMain::text_to_motion(const char *text)
{
	for(int i = 0; i < TOTAL_PATHS; i++)
	{
		if(!strcasecmp(motion_to_text(i), text)) return i;
	}
	return 0;
}


int TitleMain::process_realtime(VFrame *input_ptr, VFrame *output_ptr)
{
	int result = 0;
	input = input_ptr;
	output = output_ptr;
	build_fonts();

	need_reconfigure |= load_configuration();

// Check boundaries
	if(config.size <= 0 || config.size >= 2048)
		config.size = 72;
	if(config.stroke_width < 0 || config.stroke_width >= 512)
		config.stroke_width = 0.0;
	if(!config.wlen)
		return 0;
	if(!strlen(config.encoding))
		strcpy(config.encoding, DEFAULT_ENCODING);

// Always synthesize text and redraw it for timecode
	if(config.timecode)
	{
		int64_t rendered_frame = get_source_position();
		if (get_direction() == PLAY_REVERSE)
			rendered_frame -= 1;

		char text[BCTEXTLEN];
		Units::totext(text,
				(double)rendered_frame / PluginVClient::project_frame_rate,
				config.timecode_format,
				PluginVClient::get_project_samplerate(),
				PluginVClient::get_project_framerate(),
				16);
		config.to_wtext(config.encoding, text, strlen(text)+1);
		need_reconfigure = 1;
	}

// printf("TitleMain::process_realtime %d need_reconfigure=%d\n",
// __LINE__,
// need_reconfigure);

// Handle reconfiguration
	if(need_reconfigure) {
		if(text_mask) { delete text_mask;  text_mask = 0; }
		if(freetype_face) { FT_Done_Face(freetype_face); freetype_face = 0; }
		if(glyph_engine) { delete glyph_engine; glyph_engine = 0; }
		if(char_positions) { delete [] char_positions; char_positions = 0; }
		clear_glyphs();
		if(text_mask_stroke) { delete text_mask_stroke; text_mask_stroke = 0; }
		text_mask = 0;
		if( rows_bottom ) { delete [] rows_bottom; rows_bottom = 0; }
		rows_size = 0;
		visible_row1 = 0;
		visible_row2 = 0;

		if(!freetype_library)
			FT_Init_FreeType(&freetype_library);

		if(!freetype_face) {
			BC_FontEntry *font = get_font();
			if(load_freetype_face(freetype_library,
				freetype_face,
				font->path))
			{
				printf("TitleMain::process_realtime %s: FT_New_Face failed.\n",
					font->displayname);
				result = 1;
			}

			if(!result) FT_Set_Pixel_Sizes(freetype_face, config.size, 0);
		}


		if(!result)
		{
//PRINT_TRACE
			draw_glyphs();
			get_total_extents();
			need_reconfigure = 0;
		}
	}

	if(!result)
	{
//PRINT_TRACE
// Determine region of text visible on the output and draw mask
		result = draw_mask();
	}


// Overlay mask on output
	if(!result)
	{
//PRINT_TRACE
		overlay_mask();
	}

	return 0;
}

void TitleMain::update_gui()
{
	if(thread)
	{
		int reconfigure = load_configuration();
		if(reconfigure)
		{
			thread->window->lock_window("TitleMain::update_gui");
			((TitleWindow*)thread->window)->update();
			((TitleWindow*)thread->window)->unlock_window();
			((TitleWindow*)thread->window)->color_thread->update_gui(config.color, 0);
			thread->window->unlock_window();
		}
	}
}

int TitleMain::load_configuration()
{
	KeyFrame *prev_keyframe, *next_keyframe;
	prev_keyframe = get_prev_keyframe(get_source_position());
	next_keyframe = get_next_keyframe(get_source_position());
	int64_t prev_position = edl_to_local(prev_keyframe->position);
	int64_t next_position = edl_to_local(next_keyframe->position);

// printf("TitleMain::load_configuration 1 %d %d\n", 
// prev_keyframe->position,
// next_keyframe->position);

	TitleConfig old_config, prev_config, next_config;
	old_config.copy_from(config);
	read_data(prev_keyframe);
	prev_config.copy_from(config);
	read_data(next_keyframe);
	next_config.copy_from(config);

	config.prev_keyframe_position = prev_keyframe->position;
	config.next_keyframe_position = next_keyframe->position;

	// if no previous keyframe exists, it should be start of the plugin, not start of the track
	if(config.next_keyframe_position == config.prev_keyframe_position)
		config.next_keyframe_position = get_source_start() + get_total_len();
	if (config.prev_keyframe_position == 0) 
		config.prev_keyframe_position = get_source_start();


// printf("TitleMain::load_configuration 10 %d %d\n", 
// config.prev_keyframe_position,
// config.next_keyframe_position);

	config.interpolate(prev_config, 
		next_config, 
		(next_keyframe->position == prev_keyframe->position) ?
			get_source_position() :
			prev_keyframe->position,
		(next_keyframe->position == prev_keyframe->position) ?
			get_source_position() + 1 :
			next_keyframe->position,
		get_source_position());

	if(!config.equivalent(old_config))
		return 1;
	return 0;
}


void TitleMain::save_data(KeyFrame *keyframe)
{
	FileXML output;

	output.set_shared_string(keyframe->get_data(), MESSAGESIZE);
	output.tag.set_title("TITLE");
	output.tag.set_property("FONT", config.font);
	output.tag.set_property("ENCODING", config.encoding);
	output.tag.set_property("STYLE", (int64_t)config.style);
	output.tag.set_property("SIZE", config.size);
	output.tag.set_property("COLOR", config.color);
	output.tag.set_property("COLOR_STROKE", config.color_stroke);
	output.tag.set_property("STROKE_WIDTH", config.stroke_width);
        output.tag.set_property("OUTLINE_COLOR", config.outline_color);
        output.tag.set_property("ALPHA", config.alpha);
        output.tag.set_property("OUTLINE_ALPHA", config.outline_alpha);
	output.tag.set_property("MOTION_STRATEGY", config.motion_strategy);
	output.tag.set_property("LOOP", config.loop);
	output.tag.set_property("PIXELS_PER_SECOND", config.pixels_per_second);
	output.tag.set_property("HJUSTIFICATION", config.hjustification);
	output.tag.set_property("VJUSTIFICATION", config.vjustification);
	output.tag.set_property("FADE_IN", config.fade_in);
	output.tag.set_property("FADE_OUT", config.fade_out);
	output.tag.set_property("TITLE_X", config.x);
	output.tag.set_property("TITLE_Y", config.y);
	output.tag.set_property("DROPSHADOW", config.dropshadow);
	output.tag.set_property("OUTLINE_SIZE", config.outline_size);
	output.tag.set_property("TIMECODE", config.timecode);
	output.tag.set_property("TIMECODEFORMAT", config.timecode_format);
	output.tag.set_property("WINDOW_W", config.window_w);
	output.tag.set_property("WINDOW_H", config.window_h);
	output.append_tag();
	output.append_newline();
	char text[BCTEXTLEN];
	BC_Resources::encode(BC_Resources::wide_encoding, DEFAULT_ENCODING,
		(char*)config.wtext, config.wlen*sizeof(wchar_t),
		text, sizeof(text));
	output.encode_text(text);

	output.tag.set_title("/TITLE");
	output.append_tag();
	output.append_newline();
	output.terminate_string();
//printf("TitleMain::save_data 1\n%s\n", output.string);
//printf("TitleMain::save_data 2\n%s\n", config.text);
}

void TitleMain::read_data(KeyFrame *keyframe)
{
	FileXML input;

	input.set_shared_string(keyframe->get_data(), strlen(keyframe->get_data()));

	int result = 0;
	int new_interlace = 0;
	int new_horizontal = 0;
	int new_luminance = 0;

	config.prev_keyframe_position = keyframe->position;
	while(!result)
	{
		result = input.read_tag();

		if(!result)
		{
			if(input.tag.title_is("TITLE"))
			{
				input.tag.get_property("FONT", config.font);
				input.tag.get_property("ENCODING", config.encoding);
				config.style = input.tag.get_property("STYLE", (int64_t)config.style);
				config.size = input.tag.get_property("SIZE", config.size);
				config.color = input.tag.get_property("COLOR", config.color);
				config.color_stroke = input.tag.get_property("COLOR_STROKE", config.color_stroke);
				config.stroke_width = input.tag.get_property("STROKE_WIDTH", config.stroke_width);
				config.outline_color = input.tag.get_property("OUTLINE_COLOR", config.outline_color);
				config.alpha = input.tag.get_property("ALPHA", config.alpha);
				config.outline_alpha = input.tag.get_property("OUTLINE_ALPHA", config.outline_alpha);
				config.motion_strategy = input.tag.get_property("MOTION_STRATEGY", config.motion_strategy);
				config.loop = input.tag.get_property("LOOP", config.loop);
				config.pixels_per_second = input.tag.get_property("PIXELS_PER_SECOND", config.pixels_per_second);
				config.hjustification = input.tag.get_property("HJUSTIFICATION", config.hjustification);
				config.vjustification = input.tag.get_property("VJUSTIFICATION", config.vjustification);
				config.fade_in = input.tag.get_property("FADE_IN", config.fade_in);
				config.fade_out = input.tag.get_property("FADE_OUT", config.fade_out);
				config.x = input.tag.get_property("TITLE_X", config.x);
				config.y = input.tag.get_property("TITLE_Y", config.y);
				config.dropshadow = input.tag.get_property("DROPSHADOW", config.dropshadow);
				config.outline_size = input.tag.get_property("OUTLINE_SIZE", config.outline_size);
				config.timecode = input.tag.get_property("TIMECODE", config.timecode);
				input.tag.get_property("TIMECODEFORMAT", config.timecode_format);
				config.window_w = input.tag.get_property("WINDOW_W", config.window_w);
				config.window_h = input.tag.get_property("WINDOW_H", config.window_h);
				const char *text = input.read_text();
				config.to_wtext(config.encoding, text, strlen(text)+1);
			}
			else
			if(input.tag.title_is("/TITLE"))
			{
				result = 1;
			}
		}
	}
}
