
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

#ifndef TITLE_H
#define TITLE_H



// Theory:

// Stage 1:
// Only performed when text mask changes.
// Update glyph cache with every glyph used in the title.
// A parallel text renderer draws one character per CPU.
// The titler direct copies all the text currently visible onto the text mask.
// in integer coordinates.
// The text mask is in the same color space as the output but always has
// an alpha channel.

// Stage 2:
// Performed every frame.
// The text mask is overlayed with fractional translation and fading on the output.






class TitleMain;
class TitleEngine;
class TitleOutlineEngine;
class GlyphEngine;
class TitleTranslate;

#include "bchash.h"
#include "bcfontentry.h"
#include "loadbalance.h"
#include "mutex.h"
#include "overlayframe.h"
#include "pluginvclient.h"
#include "titlewindow.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <sys/types.h>

// Motion strategy
#define TOTAL_PATHS 5
#define NO_MOTION     0x0
#define BOTTOM_TO_TOP 0x1
#define TOP_TO_BOTTOM 0x2
#define RIGHT_TO_LEFT 0x3
#define LEFT_TO_RIGHT 0x4

// Horizontal justification
#define JUSTIFY_LEFT   0x0
#define JUSTIFY_CENTER 0x1
#define JUSTIFY_RIGHT  0x2

// Vertical justification
#define JUSTIFY_TOP     0x0
#define JUSTIFY_MID     0x1
#define JUSTIFY_BOTTOM  0x2

class TitleConfig
{
public:
	TitleConfig();
	~TitleConfig();

	void to_wtext(const char *from_enc, const char *text, int tlen);
// Only used to clear glyphs
	int equivalent(TitleConfig &that);
	void copy_from(TitleConfig &that);
	void interpolate(TitleConfig &prev,
		TitleConfig &next,
		int64_t prev_frame,
		int64_t next_frame,
		int64_t current_frame);
	void limits();

// Font information
	char font[BCTEXTLEN];
	int64_t style;
	int size;
	int color;
	int color_stroke;
	int outline_color;
	int alpha;
	int outline_alpha;
	int motion_strategy;     // Motion of title across frame
	int line_pitch;
	int loop;                // Loop motion path
	float pixels_per_second; // Speed of motion
	int hjustification;
	int vjustification;
// Number of seconds the fade in and fade out of the title take
	double fade_in, fade_out;
// Position in frame relative to top left
	float x, y;
// Pixels down and right of dropshadow
	int dropshadow;
	int outline_size;
// Calculated during every frame for motion strategy
	int64_t prev_keyframe_position;
	int64_t next_keyframe_position;
// Stamp timecode
	int timecode;

// Text to display
	wchar_t wtext[BCTEXTLEN];
	int wlen;
	void convert_text();

// Encoding to convert from
	char encoding[BCTEXTLEN];
// Time Code Format
	int timecode_format;
// Width of the stroke
	double stroke_width;
// Size of window
	int window_w, window_h;
};

class TitleGlyph
{
public:
	TitleGlyph();
	~TitleGlyph();

	FT_ULong char_code;
	int width, height, pitch;
	int advance_x;
	int left, top, right, bottom;
	int freetype_index;
	VFrame *data;
	VFrame *data_stroke;
};









// Draw a single character into the glyph cache
//
class GlyphPackage : public LoadPackage
{
public:
	GlyphPackage();
	TitleGlyph *glyph;
};


class GlyphUnit : public LoadClient
{
public:
	GlyphUnit(TitleMain *plugin, GlyphEngine *server);
	~GlyphUnit();
	void process_package(LoadPackage *package);

	TitleMain *plugin;
	BC_FontEntry *current_font;       // Current font configured by freetype
	FT_Library freetype_library;      	// Freetype library
	FT_Face freetype_face;
};

class GlyphEngine : public LoadServer
{
public:
	GlyphEngine(TitleMain *plugin, int cpus);
	void init_packages();
	LoadClient* new_client();
	LoadPackage* new_package();
	TitleMain *plugin;
};







// Copy a single character to the text mask
class TitlePackage : public LoadPackage
{
public:
	TitlePackage();
	int x, y;
	wchar_t char_code;
};


class TitleUnit : public LoadClient
{
public:
	TitleUnit(TitleMain *plugin, TitleEngine *server);
	void process_package(LoadPackage *package);
	void draw_glyph(VFrame *output, VFrame *data, TitleGlyph *glyph, int x, int y);
	TitleMain *plugin;
	TitleEngine *engine;
};

class TitleEngine : public LoadServer
{
public:
	TitleEngine(TitleMain *plugin, int cpus);
	void init_packages();
	LoadClient* new_client();
	LoadPackage* new_package();
	TitleMain *plugin;
	int do_dropshadow;
};





// Create outline
class TitleOutlinePackage : public LoadPackage
{
public:
	TitleOutlinePackage();
	int y1, y2;
};


class TitleOutlineUnit : public LoadClient
{
public:
	TitleOutlineUnit(TitleMain *plugin, TitleOutlineEngine *server);
	void process_package(LoadPackage *package);
	TitleMain *plugin;
	TitleOutlineEngine *engine;
};

class TitleOutlineEngine : public LoadServer
{
public:
	TitleOutlineEngine(TitleMain *plugin, int cpus);
	void init_packages();
	void do_outline();
	LoadClient* new_client();
	LoadPackage* new_package();
	TitleMain *plugin;
	int pass;
};









// Overlay text mask with fractional translation
// We don't use OverlayFrame to enable alpha blending on non alpha
// output.
class TitleTranslatePackage : public LoadPackage
{
public:
	TitleTranslatePackage();
	int y1, y2;
};


class TitleTranslateUnit : public LoadClient
{
public:
	TitleTranslateUnit(TitleMain *plugin, TitleTranslate *server);
	void process_package(LoadPackage *package);
	TitleMain *plugin;
};

class TitleTranslate : public LoadServer
{
public:
	TitleTranslate(TitleMain *plugin, int cpus);
	~TitleTranslate();
	void init_packages();
	void run_packages();
	LoadClient* new_client();
	LoadPackage* new_package();
	TitleMain *plugin;
	transfer_table_f *y_table;
	transfer_table_f *x_table;
	int output_w;
	int output_h;
// Result of translation_array_f
	int out_x1_int;
	int out_x2_int;
	int out_y1_int;
	int out_y2_int;
// Values to process
	int out_x1;
	int out_x2;
	int out_y1;
	int out_y2;
};














// Position of each character relative to total text extents
typedef struct
{
	int x, y, w, row;
} char_pos_t;



class TitleMain : public PluginVClient
{
public:
	TitleMain(PluginServer *server);
	~TitleMain();

// required for all realtime plugins
	PLUGIN_CLASS_MEMBERS(TitleConfig)
	int process_realtime(VFrame *input_ptr, VFrame *output_ptr);
	int is_realtime();
	int is_synthesis();
	void update_gui();
	void save_data(KeyFrame *keyframe);
	void read_data(KeyFrame *keyframe);

	void build_previews(TitleWindow *gui);
	void reset_render();
	int init_freetype();
	void load_glyphs();
	int draw_mask();
	void overlay_mask();
	TitleGlyph *get_glyph(FT_ULong char_code);
	BC_FontEntry* get_font();
	int get_char_advance(FT_ULong current, FT_ULong next);
	int get_char_height();
	int get_char_width(FT_ULong c);
	void get_total_extents();
	void clear_glyphs();
	int check_char_code_path(FT_Library &freetype_library,
		char *path_old,
		FT_ULong &char_code,
		char *path_new);
	int load_freetype_face(FT_Library &freetype_library,
		FT_Face &freetype_face,
		const char *path);
	void get_color_components(int *r, int *g, int *b, int *a, int is_outline);




// backward compatibility
	void convert_encoding();
	static const char* motion_to_text(int motion);
	static int text_to_motion(const char *text);

	ArrayList<TitleGlyph*> glyphs;
	Mutex glyph_lock;

// Stage 1 parameters must be compared to redraw the text mask
	VFrame *text_mask;
	VFrame *text_mask_stroke;
	VFrame *outline_mask;
	GlyphEngine *glyph_engine;
	TitleEngine *title_engine;
	TitleTranslate *translate;
	TitleOutlineEngine *outline_engine;

// Necessary to get character width
	FT_Library freetype_library;      	// Freetype library
	FT_Face freetype_face;

// Visible area of all text present in the mask.
// Horizontal characters aren't clipped because column positions are
// proportional.
	int visible_row1, visible_char1;
	int visible_row2, visible_char2;
// Positions of the top pixels of the rows
	class Geom {
	public: // x1,y1 x2,y2 are abs
		int x0, y0, x1, y1, x2, y2;
	} extent;
	class RowGeom : public Geom {
	public: // x1,x2 y1,y2 are rel x0,y0
		int left()   { return x0+x1; }
		int top()    { return y0+y1; }
		int right()  { return x0+x2; }
		int bottom() { return y0+y2; }
	} *row_geom;
	int row_geom_size;

	int text_rows;
// relative position of all text to output
	int text_w, text_h;
	float text_x1, text_y1, text_x2, text_y2;
// Position of each character relative to total text extents
	char_pos_t *char_pos;
// relative position of visible part of text to output
	int mask_w, mask_h;

// Fade value
	int alpha;

// Max dimensions for all characters.  Not equal to config.size
// Must be calculated from rendering characters
	int ascent;
	int height;
// Relative position of mask to output is text_x1, mask_y1
// We can either round it to nearest ints to speed up replication while the text
// itself is offset fractionally
// or replicate with fractional offsetting.  Since fraction offsetting usually
// happens during motion and motion would require floating point offsetting
// for every frame we replicate with fractional offsetting.



	VFrame *input, *output;

	int need_reconfigure;
	int cpus;
};


#endif
