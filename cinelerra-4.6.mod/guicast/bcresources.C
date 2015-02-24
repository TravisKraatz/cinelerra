
/*
 * CINELERRA
 * Copyright (C) 2009 Adam Williams <broadcast at earthling dot net>
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

#include "bcdisplayinfo.h"
#include "bcipc.h"
#include "bclistbox.inc"
#include "bcfontentry.h"
#include "bcresources.h"
#include "bcsignals.h"
#include "bcsynchronous.h"
#include "bcwindowbase.h"
#include "colors.h"
#include "bccmodels.h"
#include "cstrdup.h"
#include "fonts.h"
#include "language.h"
#include "vframe.h"

#include <string.h>
#include <iconv.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>
#include <unistd.h>





int BC_Resources::error = 0;

VFrame* BC_Resources::bg_image = 0;
VFrame* BC_Resources::menu_bg = 0;

int BC_Resources::locale_utf8 = 0;
int BC_Resources::little_endian = 0;
char BC_Resources::language[LEN_LANG] = {0};
char BC_Resources::region[LEN_LANG] = {0};
char BC_Resources::encoding[LEN_ENCOD] = {0};
const char *BC_Resources::wide_encoding = 0;
ArrayList<BC_FontEntry*> *BC_Resources::fontlist = 0;
const char *BC_Resources::fc_properties[] = { FC_SLANT, FC_WEIGHT, FC_WIDTH };
#define LEN_FCPROP (sizeof(BC_Resources::fc_properties) / sizeof(const char*))

static const char *def_small_font = "-*-helvetica-medium-r-normal-*-%d-*";  // 10
static const char *def_small_font2 = "-*-helvetica-medium-r-normal-*-%d-*"; // 11
static const char *def_medium_font = "-*-helvetica-bold-r-normal-*-%d-*";   // 14
static const char *def_medium_font2 = "-*-helvetica-bold-r-normal-*-%d-*";  // 14
static const char *def_large_font = "-*-helvetica-bold-r-normal-*-%d-*";    // 18
static const char *def_large_font2 = "-*-helvetica-bold-r-normal-*-%d-*";   // 20
static const char *def_big_font =
  "-*-bitstream charter-bold-r-normal-*-*-0-%d-%d-p-0-iso8859-1"; // 160
static const char *def_big_font2 =
  "-*-nimbus sans l-bold-r-normal-*-*-0-%d-%d-p-0-iso8859-1";     // 160
static const char *def_small_fontset =  "-*-helvetica-medium-r-normal-*-%d-*";// 10
static const char *def_medium_fontset = "-*-helvetica-bold-r-normal-*-%d-*";  // 14
static const char *def_large_fontset =  "-*-helvetica-bold-r-normal-*-%d-*";  // 18
static const char *def_big_fontset =    "-*-helvetica-bold-r-normal-*-%d-*";  // 24
static const char *def_small_font_xft = "Sans:pixelsize=%.4f";           // 10.6667
static const char *def_small_b_font_xft = "Sans:bold:pixelsize=%.4f";    // 10.6667
static const char *def_medium_font_xft = "Sans:pixelsize=%.4f";          // 13.3333
static const char *def_medium_b_font_xft = "Sans:bold:pixelsize=%.4f";   // 13.3333
static const char *def_large_font_xft = "Sans:pixelsize=%.4f";           // 21.3333
static const char *def_large_b_font_xft = "Sans:bold:pixelsize=%.4f";    // 21.3333
static const char *def_big_font_xft = "Sans:pixelsize=37.3333";          // 37.3333
static const char *def_big_b_font_xft = "Sans:bold:pixelsize=37.33333";  // 37.3333

#define default_font_xft2 "-microsoft-verdana-*-*-*-*-*-*-*-*-*-*-*-*"

const char* BC_Resources::small_font = 0;
const char* BC_Resources::small_font2 = 0;
const char* BC_Resources::medium_font = 0;
const char* BC_Resources::medium_font2 = 0;
const char* BC_Resources::large_font = 0;
const char* BC_Resources::large_font2 = 0;
const char* BC_Resources::big_font = 0;
const char* BC_Resources::big_font2 = 0;
const char* BC_Resources::small_fontset = 0;
const char* BC_Resources::medium_fontset = 0;
const char* BC_Resources::large_fontset = 0;
const char* BC_Resources::big_fontset = 0;
const char* BC_Resources::small_font_xft = 0;
const char* BC_Resources::small_font_xft2 = 0;
const char* BC_Resources::small_b_font_xft = 0;
const char* BC_Resources::medium_font_xft = 0;
const char* BC_Resources::medium_font_xft2 = 0;
const char* BC_Resources::medium_b_font_xft = 0;
const char* BC_Resources::large_font_xft = 0;
const char* BC_Resources::large_font_xft2 = 0;
const char* BC_Resources::large_b_font_xft = 0;
const char* BC_Resources::big_font_xft = 0;
const char* BC_Resources::big_font_xft2 = 0;
const char* BC_Resources::big_b_font_xft = 0;

#define def_font(v, s...) do { sprintf(string,def_##v,s); v = cstrdup(string); } while(0)
#define set_font(v, s) do { sprintf(string, "%s", s); v = cstrdup(string); } while(0)
#define iround(v) ((int)(v+0.5))
void BC_Resources::init_font_defs(double scale)
{
	char string[BCTEXTLEN];
	def_font(small_font,       iround(scale*11));
	def_font(small_font2,      iround(scale*11));
	def_font(medium_font,      iround(scale*14));
	def_font(medium_font2,     iround(scale*14));
	def_font(large_font,       iround(scale*18));
	def_font(large_font2,      iround(scale*20));
	def_font(big_font,         iround(scale*160), iround(scale*160));
	def_font(big_font2,        iround(scale*160), iround(scale*160));
	def_font(small_fontset,    iround(scale*10));
	def_font(medium_fontset,   iround(scale*14));
	def_font(large_fontset,    iround(scale*18));
	def_font(big_fontset,      iround(scale*24));
	def_font(small_font_xft,   (scale*10.6667));
	def_font(small_b_font_xft, (scale*10.6667));
	def_font(medium_font_xft,  (scale*13.3333));
	def_font(medium_b_font_xft,(scale*13.3333));
	def_font(large_font_xft,   (scale*21.3333));
	def_font(large_b_font_xft, (scale*21.3333));
	def_font(big_font_xft,     (scale*37.3333));
	def_font(big_b_font_xft,   (scale*37.3333));

	set_font(small_font_xft2,  default_font_xft2);
	set_font(medium_font_xft2, default_font_xft2);
	set_font(large_font_xft2,  default_font_xft2);
	set_font(big_font_xft2,    default_font_xft2);
}

suffix_to_type_t BC_Resources::suffix_to_type[] =
{
	{ "m2v", ICON_FILM },
	{ "mov", ICON_FILM },
	{ "mp2", ICON_SOUND },
	{ "mp3", ICON_SOUND },
	{ "ac3", ICON_SOUND },
	{ "mpg", ICON_FILM },
	{ "vob", ICON_FILM },
	{ "ifo", ICON_FILM },
	{ "ts",  ICON_FILM },
	{ "vts", ICON_FILM },
	{ "wav", ICON_SOUND }
};

BC_Signals* BC_Resources::signal_handler = 0;
Mutex BC_Resources::fontconfig_lock("BC_Resources::fonconfig_lock");

int BC_Resources::x_error_handler(Display *display, XErrorEvent *event)
{
#if defined(OUTPUT_X_ERROR)
	char string[1024];
	XGetErrorText(event->display, event->error_code, string, 1024);
	fprintf(stderr, "BC_Resources::x_error_handler: error_code=%d opcode=%d,%d %s\n",
		event->error_code,
		event->request_code,
		event->minor_code,
		string);
#endif

	BC_Resources::error = 1;
// This bug only happens in 32 bit mode.
	if(sizeof(long) == 4)
		BC_WindowBase::get_resources()->use_xft = 0;
	return 0;
}

int BC_Resources::machine_cpus = 1;

int BC_Resources::get_machine_cpus()
{
	int cpus = 1;
	FILE *proc = fopen("/proc/cpuinfo", "r");
	if( proc ) {
		char string[BCTEXTLEN], *cp;
		while(!feof(proc) && fgets(string, sizeof(string), proc) ) {
			if( !strncasecmp(string, "processor", 9) &&
			    (cp = strchr(string, ':')) != 0 ) {
				int n = atol(cp+1) + 1;
				if( n > cpus ) cpus = n;
			}
			else if( !strncasecmp(string, "cpus detected", 13) &&
			    (cp = strchr(string, ':')) != 0 )
				cpus = atol(cp+1);
		}
		fclose(proc);
	}
	return cpus;
}

BC_Resources::BC_Resources()
{
	synchronous = 0;
	vframe_shm = 0;
	display_info = new BC_DisplayInfo("", 0);
	double scale = 1;
	char *font_scale = getenv("BC_FONT_SCALE");
	if( !font_scale ) {
		int display_w = display_info->get_root_w();
		int display_h = display_info->get_root_h();
		int display_size = display_h < display_w ? display_h : display_w;
		scale = display_size / 1000.;
	}
	else {
		double env_scale = atof(font_scale);
		if( env_scale > 0 ) scale = env_scale;
	}
	init_font_defs(scale);
	id_lock = new Mutex("BC_Resources::id_lock");
	create_window_lock = new Mutex("BC_Resources::create_window_lock", 1);
	id = 0;
	machine_cpus = get_machine_cpus();

	for(int i = 0; i < FILEBOX_HISTORY_SIZE; i++)
		filebox_history[i].path[0] = 0;

#ifdef HAVE_XFT
	XftInitFtLibrary();
#endif

	little_endian = (*(const u_int32_t*)"\01\0\0\0") & 1;
	wide_encoding = little_endian ?  "UTF32LE" : "UTF32BE";
	use_xvideo = 1;

#include "images/file_film_png.h"
#include "images/file_folder_png.h"
#include "images/file_sound_png.h"
#include "images/file_unknown_png.h"
#include "images/file_column_png.h"
	static VFrame* default_type_to_icon[] =
	{
		new VFrame(file_folder_png),
		new VFrame(file_unknown_png),
		new VFrame(file_film_png),
		new VFrame(file_sound_png),
		new VFrame(file_column_png)
	};
	type_to_icon = default_type_to_icon;


#include "images/bar_png.h"
	static VFrame* default_bar = new VFrame(bar_png);
	bar_data = default_bar;


#include "images/cancel_up_png.h"
#include "images/cancel_hi_png.h"
#include "images/cancel_dn_png.h"
	static VFrame* default_cancel_images[] =
	{
		new VFrame(cancel_up_png),
		new VFrame(cancel_hi_png),
		new VFrame(cancel_dn_png)
	};

#include "images/ok_up_png.h"
#include "images/ok_hi_png.h"
#include "images/ok_dn_png.h"
	static VFrame* default_ok_images[] =
	{
		new VFrame(ok_up_png),
		new VFrame(ok_hi_png),
		new VFrame(ok_dn_png)
	};

#include "images/usethis_up_png.h"
#include "images/usethis_uphi_png.h"
#include "images/usethis_dn_png.h"
	static VFrame* default_usethis_images[] =
	{
		new VFrame(usethis_up_png),
		new VFrame(usethis_uphi_png),
		new VFrame(usethis_dn_png)
	};


#include "images/checkbox_checked_png.h"
#include "images/checkbox_dn_png.h"
#include "images/checkbox_checkedhi_png.h"
#include "images/checkbox_up_png.h"
#include "images/checkbox_hi_png.h"
	static VFrame* default_checkbox_images[] =
	{
		new VFrame(checkbox_up_png),
		new VFrame(checkbox_hi_png),
		new VFrame(checkbox_checked_png),
		new VFrame(checkbox_dn_png),
		new VFrame(checkbox_checkedhi_png)
	};

#include "images/radial_checked_png.h"
#include "images/radial_dn_png.h"
#include "images/radial_checkedhi_png.h"
#include "images/radial_up_png.h"
#include "images/radial_hi_png.h"
	static VFrame* default_radial_images[] =
	{
		new VFrame(radial_up_png),
		new VFrame(radial_hi_png),
		new VFrame(radial_checked_png),
		new VFrame(radial_dn_png),
		new VFrame(radial_checkedhi_png)
	};

	static VFrame* default_label_images[] =
	{
		new VFrame(radial_up_png),
		new VFrame(radial_hi_png),
		new VFrame(radial_checked_png),
		new VFrame(radial_dn_png),
		new VFrame(radial_checkedhi_png)
	};


#include "images/file_text_up_png.h"
#include "images/file_text_hi_png.h"
#include "images/file_text_dn_png.h"
#include "images/file_icons_up_png.h"
#include "images/file_icons_hi_png.h"
#include "images/file_icons_dn_png.h"
#include "images/file_newfolder_up_png.h"
#include "images/file_newfolder_hi_png.h"
#include "images/file_newfolder_dn_png.h"
#include "images/file_rename_up_png.h"
#include "images/file_rename_hi_png.h"
#include "images/file_rename_dn_png.h"
#include "images/file_updir_up_png.h"
#include "images/file_updir_hi_png.h"
#include "images/file_updir_dn_png.h"
#include "images/file_delete_up_png.h"
#include "images/file_delete_hi_png.h"
#include "images/file_delete_dn_png.h"
#include "images/file_reload_up_png.h"
#include "images/file_reload_hi_png.h"
#include "images/file_reload_dn_png.h"
	static VFrame* default_filebox_text_images[] =
	{
		new VFrame(file_text_up_png),
		new VFrame(file_text_hi_png),
		new VFrame(file_text_dn_png)
	};

	static VFrame* default_filebox_icons_images[] =
	{
		new VFrame(file_icons_up_png),
		new VFrame(file_icons_hi_png),
		new VFrame(file_icons_dn_png)
	};

	static VFrame* default_filebox_updir_images[] =
	{
		new VFrame(file_updir_up_png),
		new VFrame(file_updir_hi_png),
		new VFrame(file_updir_dn_png)
	};

	static VFrame* default_filebox_newfolder_images[] =
	{
		new VFrame(file_newfolder_up_png),
		new VFrame(file_newfolder_hi_png),
		new VFrame(file_newfolder_dn_png)
	};


	static VFrame* default_filebox_rename_images[] =
	{
		new VFrame(file_rename_up_png),
		new VFrame(file_rename_hi_png),
		new VFrame(file_rename_dn_png)
	};

	static VFrame* default_filebox_delete_images[] =
	{
		new VFrame(file_delete_up_png),
		new VFrame(file_delete_hi_png),
		new VFrame(file_delete_dn_png)
	};

	static VFrame* default_filebox_reload_images[] =
	{
		new VFrame(file_reload_up_png),
		new VFrame(file_reload_hi_png),
		new VFrame(file_reload_dn_png)
	};

#include "images/listbox_button_dn_png.h"
#include "images/listbox_button_hi_png.h"
#include "images/listbox_button_up_png.h"
#include "images/listbox_button_disabled_png.h"
	static VFrame* default_listbox_button[] =
	{
		new VFrame(listbox_button_up_png),
		new VFrame(listbox_button_hi_png),
		new VFrame(listbox_button_dn_png),
		new VFrame(listbox_button_disabled_png)
	};
	listbox_button = default_listbox_button;

#include "images/menu_popup_bg_png.h"
	static VFrame* default_listbox_bg = 0;
	listbox_bg = default_listbox_bg;

#include "images/listbox_expandchecked_png.h"
#include "images/listbox_expandcheckedhi_png.h"
#include "images/listbox_expanddn_png.h"
#include "images/listbox_expandup_png.h"
#include "images/listbox_expanduphi_png.h"
	static VFrame* default_listbox_expand[] =
	{
		new VFrame(listbox_expandup_png),
		new VFrame(listbox_expanduphi_png),
		new VFrame(listbox_expandchecked_png),
		new VFrame(listbox_expanddn_png),
		new VFrame(listbox_expandcheckedhi_png),
	};
	listbox_expand = default_listbox_expand;

#include "images/listbox_columnup_png.h"
#include "images/listbox_columnhi_png.h"
#include "images/listbox_columndn_png.h"
	static VFrame* default_listbox_column[] =
	{
		new VFrame(listbox_columnup_png),
		new VFrame(listbox_columnhi_png),
		new VFrame(listbox_columndn_png)
	};
	listbox_column = default_listbox_column;


#include "images/listbox_up_png.h"
#include "images/listbox_dn_png.h"
	listbox_up = new VFrame(listbox_up_png);
	listbox_dn = new VFrame(listbox_dn_png);
	listbox_title_overlap = 0;
	listbox_title_margin = 0;
	listbox_title_color = BLACK;
	listbox_title_hotspot = 5;

	listbox_border1 = DKGREY;
	listbox_border2_hi = RED;
	listbox_border2 = BLACK;
	listbox_border3_hi = RED;
	listbox_border3 = MEGREY;
	listbox_border4 = WHITE;
	listbox_selected = BLUE;
	listbox_highlighted = LTGREY;
	listbox_inactive = WHITE;
	listbox_text = BLACK;

#include "images/pot_hi_png.h"
#include "images/pot_up_png.h"
#include "images/pot_dn_png.h"
	static VFrame *default_pot_images[] =
	{
		new VFrame(pot_up_png),
		new VFrame(pot_hi_png),
		new VFrame(pot_dn_png)
	};

#include "images/progress_up_png.h"
#include "images/progress_hi_png.h"
	static VFrame* default_progress_images[] =
	{
		new VFrame(progress_up_png),
		new VFrame(progress_hi_png)
	};

	pan_data = 0;
	pan_text_color = YELLOW;

#include "images/7seg_small/0_png.h"
#include "images/7seg_small/1_png.h"
#include "images/7seg_small/2_png.h"
#include "images/7seg_small/3_png.h"
#include "images/7seg_small/4_png.h"
#include "images/7seg_small/5_png.h"
#include "images/7seg_small/6_png.h"
#include "images/7seg_small/7_png.h"
#include "images/7seg_small/8_png.h"
#include "images/7seg_small/9_png.h"
#include "images/7seg_small/colon_png.h"
#include "images/7seg_small/period_png.h"
#include "images/7seg_small/a_png.h"
#include "images/7seg_small/b_png.h"
#include "images/7seg_small/c_png.h"
#include "images/7seg_small/d_png.h"
#include "images/7seg_small/e_png.h"
#include "images/7seg_small/f_png.h"
#include "images/7seg_small/space_png.h"
#include "images/7seg_small/dash_png.h"
	static VFrame* default_medium_7segment[] =
	{
		new VFrame(_0_png),
		new VFrame(_1_png),
		new VFrame(_2_png),
		new VFrame(_3_png),
		new VFrame(_4_png),
		new VFrame(_5_png),
		new VFrame(_6_png),
		new VFrame(_7_png),
		new VFrame(_8_png),
		new VFrame(_9_png),
		new VFrame(colon_png),
		new VFrame(period_png),
		new VFrame(a_png),
		new VFrame(b_png),
		new VFrame(c_png),
		new VFrame(d_png),
		new VFrame(e_png),
		new VFrame(f_png),
		new VFrame(space_png),
		new VFrame(dash_png)
	};

	generic_button_margin = 15;
	draw_clock_background=1;

	use_shm = -1;
	shm_reply = 1;

// Initialize
	bg_color = ORANGE;
	bg_shadow1 = DKGREY;
	bg_shadow2 = BLACK;
	bg_light1 = WHITE;
	bg_light2 = bg_color;


	border_light1 = bg_color;
	border_light2 = MEGREY;
	border_shadow1 = BLACK;
	border_shadow2 = bg_color;

	default_text_color = BLACK;
	disabled_text_color = DMGREY;

	button_light = MEGREY;           // bright corner
//	button_highlighted = LTGREY;  // face when highlighted
	button_highlighted = 0xffe000;  // face when highlighted
	button_down = MDGREY;         // face when down
//	button_up = MEGREY;           // face when up
	button_up = 0xffc000;           // face when up
	button_shadow = BLACK;       // dark corner
	button_uphighlighted = RED;   // upper side when highlighted

	tumble_data = 0;
	tumble_duration = 150;

	ok_images = default_ok_images;
	cancel_images = default_cancel_images;
	usethis_button_images = default_usethis_images;
	filebox_descend_images = default_ok_images;

	menu_light = LTCYAN;
	menu_highlighted = LTBLUE;
	menu_down = MDCYAN;
	menu_up = MECYAN;
	menu_shadow = DKCYAN;


#include "images/menuitem_up_png.h"
#include "images/menuitem_hi_png.h"
#include "images/menuitem_dn_png.h"
#include "images/menubar_up_png.h"
#include "images/menubar_hi_png.h"
#include "images/menubar_dn_png.h"
#include "images/menubar_bg_png.h"

	static VFrame *default_menuitem_data[] =
	{
		new VFrame(menuitem_up_png),
		new VFrame(menuitem_hi_png),
		new VFrame(menuitem_dn_png),
	};
	menu_item_bg = default_menuitem_data;


	static VFrame *default_menubar_data[] =
	{
		new VFrame(menubar_up_png),
		new VFrame(menubar_hi_png),
		new VFrame(menubar_dn_png),
	};
	menu_title_bg = default_menubar_data;

	menu_popup_bg = new VFrame(menu_popup_bg_png);

	menu_bar_bg = new VFrame(menubar_bg_png);

	popupmenu_images = 0;


	popupmenu_margin = 10;
	popupmenu_triangle_margin = 10;

	min_menu_w = 0;
	menu_title_text = BLACK;
	popup_title_text = BLACK;
	menu_item_text = BLACK;
	menu_highlighted_fontcolor = BLACK;
	progress_text = BLACK;



	text_default = BLACK;
	highlight_inverse = WHITE ^ BLUE;
	text_background = WHITE;
	text_background_hi = LTYELLOW;
	text_background_noborder_hi = LTGREY;
	text_background_noborder = -1;
	text_border1 = DKGREY;
	text_border2 = BLACK;
	text_border2_hi = RED;
	text_border3 = MEGREY;
	text_border3_hi = LTPINK;
	text_border4 = WHITE;
	text_highlight = BLUE;
	text_inactive_highlight = MEGREY;

	toggle_highlight_bg = 0;
	toggle_text_margin = 0;

// Delays must all be different for repeaters
	double_click = 300;
	blink_rate = 250;
	scroll_repeat = 150;
	tooltip_delay = 1000;
	tooltip_bg_color = YELLOW;
	tooltips_enabled = 1;

	filebox_margin = 110;
	dirbox_margin = 90;
	filebox_mode = LISTBOX_TEXT;
	sprintf(filebox_filter, "*");
	filebox_w = 640;
	filebox_h = 480;
	filebox_columntype[0] = FILEBOX_NAME;
	filebox_columntype[1] = FILEBOX_SIZE;
	filebox_columntype[2] = FILEBOX_DATE;
	filebox_columntype[3] = FILEBOX_EXTENSION;
	filebox_columnwidth[0] = 200;
	filebox_columnwidth[1] = 100;
	filebox_columnwidth[2] = 100;
	filebox_columnwidth[3] = 100;
	dirbox_columntype[0] = FILEBOX_NAME;
	dirbox_columntype[1] = FILEBOX_DATE;
	dirbox_columnwidth[0] = 200;
	dirbox_columnwidth[1] = 100;

	filebox_text_images = default_filebox_text_images;
	filebox_icons_images = default_filebox_icons_images;
	filebox_updir_images = default_filebox_updir_images;
	filebox_newfolder_images = default_filebox_newfolder_images;
	filebox_rename_images = default_filebox_rename_images;
	filebox_delete_images = default_filebox_delete_images;
	filebox_reload_images = default_filebox_reload_images;
	directory_color = BLUE;
	file_color = BLACK;

	filebox_sortcolumn = 0;
	filebox_sortorder = BC_ListBox::SORT_ASCENDING;
	dirbox_sortcolumn = 0;
	dirbox_sortorder = BC_ListBox::SORT_ASCENDING;


	pot_images = default_pot_images;
	pot_offset = 2;
	pot_x1 = pot_images[0]->get_w() / 2 - pot_offset;
	pot_y1 = pot_images[0]->get_h() / 2 - pot_offset;
	pot_r = pot_x1;
	pot_needle_color = BLACK;

	progress_images = default_progress_images;

	xmeter_images = 0;
	ymeter_images = 0;
	meter_font = SMALLFONT_3D;
	meter_font_color = RED;
	meter_title_w = 20;
	meter_3d = 1;
	medium_7segment = default_medium_7segment;

	audiovideo_color = RED;

	use_fontset = 0;

// Xft has priority over font set
#ifdef HAVE_XFT
// But Xft dies in 32 bit mode after some amount of drawing.
	use_xft = 1;
#else
	use_xft = 0;
#endif


	drag_radius = 10;
	recursive_resizing = 1;


}

BC_Resources::~BC_Resources()
{
}

int BC_Resources::initialize_display(BC_WindowBase *window)
{
// Set up IPC cleanup handlers
//	bc_init_ipc();

// Test for shm.  Must come before yuv test
	init_shm(window);
	return 0;
}


void BC_Resources::init_shm(BC_WindowBase *window)
{
	use_shm = 0;
	XSetErrorHandler(BC_Resources::x_error_handler);

	if(XShmQueryExtension(window->display))
	{
		XShmSegmentInfo test_shm;
		memset(&test_shm,0,sizeof(test_shm));
		XImage *test_image = XShmCreateImage(window->display, window->vis,
			window->default_depth, ZPixmap, (char*)NULL, &test_shm, 5, 5);
		BC_Resources::error = 0;
		test_shm.shmid = shmget(IPC_PRIVATE, 5 * test_image->bytes_per_line, (IPC_CREAT | 0600));
		if(test_shm.shmid != -1) {
			char *data = (char *)shmat(test_shm.shmid, NULL, 0);
			if(data != (void *)-1) {
				shmctl(test_shm.shmid, IPC_RMID, 0);
				test_shm.shmaddr = data;
				test_shm.readOnly = 0;

				if(XShmAttach(window->display, &test_shm)) {
					XSync(window->display, False);
					use_shm = 1;
				}
				shmdt(data);
			}
		}

		XDestroyImage(test_image);
		if(BC_Resources::error) use_shm = 0;
	}
}




BC_Synchronous* BC_Resources::get_synchronous()
{
	return synchronous;
}

void BC_Resources::set_synchronous(BC_Synchronous *synchronous)
{
	this->synchronous = synchronous;
}







int BC_Resources::get_top_border()
{
	return display_info->get_top_border();
}

int BC_Resources::get_left_border()
{
	return display_info->get_left_border();
}

int BC_Resources::get_right_border()
{
	return display_info->get_right_border();
}

int BC_Resources::get_bottom_border()
{
	return display_info->get_bottom_border();
}


int BC_Resources::get_bg_color() { return bg_color; }

int BC_Resources::get_bg_shadow1() { return bg_shadow1; }

int BC_Resources::get_bg_shadow2() { return bg_shadow2; }

int BC_Resources::get_bg_light1() { return bg_light1; }

int BC_Resources::get_bg_light2() { return bg_light2; }


int BC_Resources::get_id()
{
	id_lock->lock("BC_Resources::get_id");
	int result = id++;
	id_lock->unlock();
	return result;
}

int BC_Resources::get_filebox_id()
{
	id_lock->lock("BC_Resources::get_filebox_id");
	int result = filebox_id++;
	id_lock->unlock();
	return result;
}


void BC_Resources::set_signals(BC_Signals *signal_handler)
{
	BC_Resources::signal_handler = signal_handler;
}

int BC_Resources::init_fontconfig(const char *search_path)
{
	if( fontlist ) return 0;
	fontlist = new ArrayList<BC_FontEntry*>;

#define get_str(str,sep,ptr,cond) do { char *out = str; \
  while( *ptr && !strchr(sep,*ptr) && (cond) ) *out++ = *ptr++; \
  *out = 0; \
} while(0)

#define skip_str(sep,ptr) do { \
  while( *ptr && strchr(sep,*ptr) ) *ptr++; \
} while(0)

	char find_command[BCTEXTLEN];
	sprintf(find_command,
		"find %s -name 'fonts.dir' -print -exec cat {} \\;",
		search_path);
	FILE *in = popen(find_command, "r");

	FT_Library freetype_library = 0;
//	FT_Face freetype_face = 0;
//	FT_Init_FreeType(&freetype_library);

	char line[BCTEXTLEN], current_dir[BCTEXTLEN];
	current_dir[0] = 0;

	while( !feof(in) && fgets(line, BCTEXTLEN, in) ) {
		if(!strlen(line)) break;

		char *in_ptr = line;
		char *out_ptr;

// Get current directory
		if(line[0] == '/') {
			get_str(current_dir, "\n", in_ptr,1);
			for( int i=strlen(current_dir); --i>=0 && current_dir[i]!='/'; )
				current_dir[i] = 0;
			continue;
		}

//printf("TitleMain::build_fonts %s\n", line);
		BC_FontEntry *entry = new BC_FontEntry;
		char string[BCTEXTLEN];
// Path
		get_str(string, "\n", in_ptr, in_ptr[0]!=' ' || in_ptr[1]!='-');
		entry->path = cstrcat(2, current_dir, string);
// Foundary
		skip_str(" -", in_ptr);
		get_str(string, " -\n", in_ptr, 1);
		if( !string[0] ) { delete entry;  continue; }
		entry->foundry = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// Family
		get_str(string, "-\n", in_ptr, 1);
		if( !string[0] ) { delete entry;  continue; }
		entry->family = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// Weight
		get_str(string, "-\n", in_ptr, 1);
		entry->weight = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// Slant
		get_str(string, "-\n", in_ptr, 1);
		entry->slant = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// SWidth
		get_str(string, "-\n", in_ptr, 1);
		entry->swidth = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// Adstyle
		get_str(string, "-\n", in_ptr, 1);
		entry->adstyle = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// pixelsize
		get_str(string, "-\n", in_ptr, 1);
		entry->pixelsize = atol(string);
		if(*in_ptr == '-') in_ptr++;
// pointsize
		get_str(string, "-\n", in_ptr, 1);
		entry->pointsize = atol(string);
		if(*in_ptr == '-') in_ptr++;
// xres
		get_str(string, "-\n", in_ptr, 1);
		entry->xres = atol(string);
		if(*in_ptr == '-') in_ptr++;
// yres
		get_str(string, "-\n", in_ptr, 1);
		entry->yres = atol(string);
		if(*in_ptr == '-') in_ptr++;
// spacing
		get_str(string, "-\n", in_ptr, 1);
		entry->spacing = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// avg_width
		get_str(string, "-\n", in_ptr, 1);
		entry->avg_width = atol(string);
		if(*in_ptr == '-') in_ptr++;
// registry
		get_str(string, "-\n", in_ptr, 1);
		entry->registry = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;
// encoding
		get_str(string, "-\n", in_ptr, 1);
		entry->encoding = cstrdup(string);
		if(*in_ptr == '-') in_ptr++;

// Add to list
//printf("TitleMain::build_fonts 1 %s\n", entry->path);
// This takes a real long time to do.  Instead just take all fonts
// 		if(!load_freetype_face(freetype_library,
// 			freetype_face, entry->path) )
// Fix parameters
		sprintf(line, "%s (%s)", entry->family, entry->foundry);
		entry->displayname = cstrdup(line);

		if(!strcasecmp(entry->weight, "demibold") ||
		    !strcasecmp(entry->weight, "bold"))
			entry->fixed_style |= BC_FONT_BOLD;
		if(!strcasecmp(entry->slant, "i") ||
		    !strcasecmp(entry->slant, "o"))
			entry->fixed_style |= BC_FONT_ITALIC;
		fontlist->append(entry);
//		printf("TitleMain::build_fonts %s: success\n",	entry->path);
//printf("TitleMain::build_fonts 2\n");
	}
	pclose(in);


// Load all the fonts from fontconfig
	FcPattern *pat;
	FcFontSet *fs;
	FcObjectSet *os;
	FcChar8 *family, *file, *foundry, *style, *format;
	int slant, spacing, width, weight;
	int force_style = 0;
	int limit_to_trutype = 0; // if you want limit search to TrueType put 1
	FcConfig *config;
	int i;
	char tmpstring[BCTEXTLEN];
	if(!FcInit())
		return 1;
	config = FcConfigGetCurrent();
	FcConfigSetRescanInterval(config, 0);

	pat = FcPatternCreate();
	os = FcObjectSetBuild ( FC_FAMILY, FC_FILE, FC_FOUNDRY, FC_WEIGHT,
		FC_WIDTH, FC_SLANT, FC_FONTFORMAT, FC_SPACING, FC_STYLE, (char *) 0);
	FcPatternAddBool(pat, FC_SCALABLE, true);

	if(language[0]) {
		char langstr[LEN_LANG * 3];
		strcpy(langstr, language);

		if(region[0]) {
			strcat(langstr, "-");
			strcat(langstr, region);
		}

		FcLangSet *ls =  FcLangSetCreate();
		if(FcLangSetAdd(ls, (const FcChar8*)langstr))
		if(FcPatternAddLangSet(pat, FC_LANG, ls))
		FcLangSetDestroy(ls);
	}

	fs = FcFontList(config, pat, os);
	FcPatternDestroy(pat);
	FcObjectSetDestroy(os);

	for (i = 0; fs && i < fs->nfont; i++) {
		FcPattern *font = fs->fonts[i];
		force_style = 0;
		FcPatternGetString(font, FC_FONTFORMAT, 0, &format);
		//on this point you can limit font search
		if(!limit_to_trutype || !strcmp((char *)format, "TrueType"))
			continue;

		sprintf(tmpstring, "%s", format);
		BC_FontEntry *entry = new BC_FontEntry;
		if(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch) {
			entry->path = cstrdup((char*)file);
		}

		if(FcPatternGetString(font, FC_FOUNDRY, 0, &foundry) == FcResultMatch) {
			entry->foundry = cstrdup((char*)foundry);
		}

		if(FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) == FcResultMatch) {
			switch(weight) {
			case FC_WEIGHT_THIN:
			case FC_WEIGHT_EXTRALIGHT:
			case FC_WEIGHT_LIGHT:
			case FC_WEIGHT_BOOK:
				force_style = 1;
				entry->weight = cstrdup("medium");
				break;

			case FC_WEIGHT_NORMAL:
			case FC_WEIGHT_MEDIUM:
			default:
				entry->weight = cstrdup("medium");
				break;

			case FC_WEIGHT_BLACK:
			case FC_WEIGHT_SEMIBOLD:
			case FC_WEIGHT_BOLD:
				entry->weight = cstrdup("bold");
				entry->fixed_style |= BC_FONT_BOLD;
				break;

			case FC_WEIGHT_EXTRABOLD:
			case FC_WEIGHT_EXTRABLACK:
				force_style = 1;
				entry->weight = cstrdup("bold");
				entry->fixed_style |= BC_FONT_BOLD;
				break;
			}
		}

		if(FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch)
			entry->family = cstrdup((char*)family);

		if(FcPatternGetInteger(font, FC_SLANT, 0, &slant) == FcResultMatch) {
			switch(slant) {
			case FC_SLANT_ROMAN:
			default:
				entry->slant = cstrdup("r");
				break;
			case FC_SLANT_ITALIC:
				entry->slant = cstrdup("i");
				entry->fixed_style |= BC_FONT_ITALIC;
				break;
			case FC_SLANT_OBLIQUE:
				entry->slant = cstrdup("o");
				entry->fixed_style |= BC_FONT_ITALIC;
				break;
			}
		}

		if(FcPatternGetInteger(font, FC_WIDTH, 0, &width) == FcResultMatch) {
			switch(width) {
			case FC_WIDTH_ULTRACONDENSED:
				entry->swidth = cstrdup("ultracondensed");
				break;

			case FC_WIDTH_EXTRACONDENSED:
				entry->swidth = cstrdup("extracondensed");
				break;

			case FC_WIDTH_CONDENSED:
				entry->swidth = cstrdup("condensed");
				break;
			case FC_WIDTH_SEMICONDENSED:
				entry->swidth = cstrdup("semicondensed");
				break;

			case FC_WIDTH_NORMAL:
			default:
				entry->swidth = cstrdup("normal");
				break;

			case FC_WIDTH_SEMIEXPANDED:
				entry->swidth = cstrdup("semiexpanded");
				break;

			case FC_WIDTH_EXPANDED:
				entry->swidth = cstrdup("expanded");
				break;

			case FC_WIDTH_EXTRAEXPANDED:
				entry->swidth = cstrdup("extraexpanded");
				break;

			case FC_WIDTH_ULTRAEXPANDED:
				entry->swidth = cstrdup("ultraexpanded");
				break;
			}
		}

		if(FcPatternGetInteger(font, FC_SPACING, 0, &spacing) == FcResultMatch) {
			switch(spacing) {
			case 0:
			default:
				entry->spacing = cstrdup("p");
				break;

			case 90:
				entry->spacing = cstrdup("d");
				break;

			case 100:
				entry->spacing = cstrdup("m");
				break;

			case 110:
				entry->spacing = cstrdup("c");
				break;
			}
		}

		// Add fake stuff for compatibility
		entry->adstyle = cstrdup(" ");
		entry->pixelsize = 0;
		entry->pointsize = 0;
		entry->xres = 0;
		entry->yres = 0;
		entry->avg_width = 0;
		entry->registry = cstrdup("utf");
		entry->encoding = cstrdup("8");

		if(!FcPatternGetString(font, FC_STYLE, 0, &style) == FcResultMatch)
			force_style = 0;

		// If font has a style unmanaged by titler plugin, force style to be displayed on name
		// in this way we can shown all available fonts styles.
		if(force_style) {
			sprintf(tmpstring, "%s (%s)", entry->family, style);
			entry->displayname = cstrdup(tmpstring);
		}
		else {
			if(strcmp(entry->foundry, "unknown")) {
				sprintf(tmpstring, "%s (%s)", entry->family, entry->foundry);
				entry->displayname = cstrdup(tmpstring);
			}
			else {
				sprintf(tmpstring, "%s", entry->family);
				entry->displayname = cstrdup(tmpstring);
			}

		}
		fontlist->append(entry);
	}

	FcFontSetDestroy(fs);
	if(freetype_library)
		FT_Done_FreeType(freetype_library);
// for(int i = 0; i < fonts->total; i++)
//	fonts->values[i]->dump();

	FcConfigAppFontAddDir(0, (const FcChar8*)search_path);
	FcConfigSetRescanInterval(0, 0);

	os = FcObjectSetBuild(FC_FAMILY, FC_FILE, FC_FOUNDRY, FC_WEIGHT,
		FC_WIDTH, FC_SLANT, FC_SPACING, FC_STYLE, (char *)0);
	pat = FcPatternCreate();
	FcPatternAddBool(pat, FC_SCALABLE, true);

	if(language[0])
	{
		char langstr[LEN_LANG * 3];
		strcpy(langstr, language);

		if(region[0])
		{
			strcat(langstr, "-");
			strcat(langstr, region);
		}

		FcLangSet *ls =  FcLangSetCreate();
		if(FcLangSetAdd(ls, (const FcChar8*)langstr))
			if(FcPatternAddLangSet(pat, FC_LANG, ls))
		FcLangSetDestroy(ls);
	}

	fs = FcFontList(0, pat, os);
	FcPatternDestroy(pat);
	FcObjectSetDestroy(os);

	for(int i = 0; i < fs->nfont; i++)
	{
		FcPattern *font = fs->fonts[i];
		BC_FontEntry *entry = new BC_FontEntry;

		FcChar8 *strvalue;
		if(FcPatternGetString(font, FC_FILE, 0, &strvalue) == FcResultMatch)
		{
			entry->path = new char[strlen((char*)strvalue) + 1];
			strcpy(entry->path, (char*)strvalue);
		}

		if(FcPatternGetString(font, FC_FOUNDRY, 0, &strvalue) == FcResultMatch)
		{
			entry->foundry = new char[strlen((char*)strvalue) + 1];
			strcpy(entry->foundry, (char *)strvalue);
		}

		if(FcPatternGetString(font, FC_FAMILY, 0, &strvalue) == FcResultMatch)
		{
			entry->family = new char[strlen((char*)strvalue) + 2];
			strcpy(entry->family, (char*)strvalue);
		}

		int intvalue;
		if(FcPatternGetInteger(font, FC_SLANT, 0, &intvalue) == FcResultMatch)
		{
			switch(intvalue)
			{
			case FC_SLANT_ROMAN:
			default:
				entry->style |= FL_SLANT_ROMAN;
				break;

			case FC_SLANT_ITALIC:
				entry->style |= FL_SLANT_ITALIC;
				break;

			case FC_SLANT_OBLIQUE:
				entry->style |= FL_SLANT_OBLIQUE;
				break;
			}
		}

		if(FcPatternGetInteger(font, FC_SLANT, 0, &intvalue) == FcResultMatch)
		{
			switch(intvalue)
			{
			case FC_SLANT_ROMAN:
			default:
				entry->style |= FL_SLANT_ROMAN;
				break;

			case FC_SLANT_ITALIC:
				entry->style |= FL_SLANT_ITALIC;
				break;

			case FC_SLANT_OBLIQUE:
				entry->style |= FL_SLANT_OBLIQUE;
				break;
			}
		}

		if(FcPatternGetInteger(font, FC_WEIGHT, 0, &intvalue) == FcResultMatch)
		{
			switch(intvalue)
			{
			case FC_WEIGHT_THIN:
				entry->style |= FL_WEIGHT_THIN;
				break;

			case FC_WEIGHT_EXTRALIGHT:
				entry->style |= FL_WEIGHT_EXTRALIGHT;
				break;

			case FC_WEIGHT_LIGHT:
				entry->style |= FL_WEIGHT_LIGHT;
				break;

			case FC_WEIGHT_BOOK:
				entry->style |= FL_WEIGHT_BOOK;
				break;

			case FC_WEIGHT_NORMAL:
			default:
				entry->style |= FL_WEIGHT_NORMAL;
				break;

			case FC_WEIGHT_MEDIUM:
				entry->style |= FL_WEIGHT_MEDIUM;
				break;

			case FC_WEIGHT_DEMIBOLD:
				entry->style |= FL_WEIGHT_DEMIBOLD;
				break;

			case FC_WEIGHT_BOLD:
				entry->style |= FL_WEIGHT_BOLD;
				break;

			case FC_WEIGHT_EXTRABOLD:
				entry->style |= FL_WEIGHT_EXTRABOLD;
				break;

			case FC_WEIGHT_BLACK:
				entry->style |= FL_WEIGHT_BLACK;
				break;

			case FC_WEIGHT_EXTRABLACK:
				entry->style |= FL_WEIGHT_EXTRABLACK;
				break;
			}
		}

		if(FcPatternGetInteger(font, FC_WIDTH, 0, &intvalue) == FcResultMatch)
		{
			switch(intvalue)
			{
			case FC_WIDTH_ULTRACONDENSED:
				entry->style |= FL_WIDTH_ULTRACONDENSED;
				break;

			case FC_WIDTH_EXTRACONDENSED:
				entry->style |= FL_WIDTH_EXTRACONDENSED;
				break;

			case FC_WIDTH_CONDENSED:
				entry->style |= FL_WIDTH_CONDENSED;
				break;

			case FC_WIDTH_SEMICONDENSED:
				entry->style = FL_WIDTH_SEMICONDENSED;
				break;

			case FC_WIDTH_NORMAL:
			default:
				entry->style |= FL_WIDTH_NORMAL;
				break;

			case FC_WIDTH_SEMIEXPANDED:
				entry->style |= FL_WIDTH_SEMIEXPANDED;
				break;

			case FC_WIDTH_EXPANDED:
				entry->style |= FL_WIDTH_EXPANDED;
				break;

			case FC_WIDTH_EXTRAEXPANDED:
				entry->style |= FL_WIDTH_EXTRAEXPANDED;
				break;

			case FC_WIDTH_ULTRAEXPANDED:
				entry->style |= FL_WIDTH_ULTRAEXPANDED;
				break;
			}
		}
		if(FcPatternGetInteger(font, FC_SPACING, 0, &intvalue) == FcResultMatch)
		{
			switch(intvalue)
			{
			case FC_PROPORTIONAL:
			default:
				entry->style |= FL_PROPORTIONAL;
				break;

			case FC_DUAL:
				entry->style |= FL_DUAL;
				break;

			case FC_MONO:
				entry->style |= FL_MONO;
				break;

			case FC_CHARCELL:
				entry->style |= FL_CHARCELL;
				break;
			}
		}
		if(entry->foundry && strcmp(entry->foundry, "unknown"))
		{
			char tempstr[BCTEXTLEN];
			sprintf(tempstr, "%s (%s)", entry->family, entry->foundry);
			entry->displayname = new char[strlen(tempstr) + 1];
			strcpy(entry->displayname, tempstr);
		}
		else
		{
			entry->displayname = new char[strlen(entry->family) + 1];
			strcpy(entry->displayname, entry->family);
		}
		fontlist->append(entry);
	}
	FcFontSetDestroy(fs);
	return 0;
}

BC_FontEntry *BC_Resources::find_fontentry(const char *displayname, int style, int mask)
{
	BC_FontEntry *entry, *style_match;

	if(!fontlist)
		return 0;

	if(displayname)
	{
		for(int i = 0; i < fontlist->total; i++)
		{
			entry = fontlist->values[i];

			if(strcmp(entry->displayname, displayname) == 0 &&
					(entry->style & mask) == style)
				return entry;
		}
	}
// No exact match - assume normal width font
	style |= FL_WIDTH_NORMAL;
	mask |= FL_WIDTH_MASK;
	style_match = 0;
	for(int i = 0; i < fontlist->total; i++)
	{
		entry = fontlist->values[i];

		if((entry->style & mask) == style)
		{
			if(!style_match)
				style_match = entry;

			if(!strncasecmp(displayname, entry->family,
					strlen(entry->family)))
			return entry;
		}
	}
	return style_match;
}

size_t BC_Resources::encode(const char *from_enc, const char *to_enc,
	char *input, int input_length, char *output, int output_length)
{
	size_t inbytes, outbytes = 0;
	iconv_t cd;
	char *outbase = output;

	if(!from_enc || *from_enc == 0)
		from_enc = "UTF-8";

	if(!to_enc || *to_enc == 0)
		to_enc = "UTF-8";

	if(input_length < 0)
		inbytes = strlen(input);
	else
		inbytes = input_length;

	if(strcmp(from_enc, to_enc) && inbytes)
	{
		if((cd = iconv_open(to_enc, from_enc)) == (iconv_t)-1)
		{
			printf(_("Conversion from %s to %s is not available"),
				from_enc, to_enc);
			return 0;
		}

		outbytes = output_length - 1;

		iconv(cd, &input, &inbytes, &output, &outbytes);

		iconv_close(cd);
		inbytes = output - outbase;
	}
	else if(inbytes)
	{
		memcpy(output,  input, inbytes);
		outbytes -= inbytes;
	}
	for(int i = 0; i < 4; i++)
	{
		output[i] = 0;
		if(outbytes-- == 0)
			break;
	}
	return inbytes;
}

int BC_Resources::find_font_by_char(FT_ULong char_code, char *path_new, const FT_Face oldface)
{
	FcPattern *font, *ofont;
	FcChar8 *file;
	int result = 0;

	*path_new = 0;

	// Do not search control codes
	if(char_code < ' ')
		return 0;

	if(ofont = FcFreeTypeQueryFace(oldface, (const FcChar8*)"", 4097, 0))
	{
		if(font = find_similar_font(char_code, ofont))
		{
			if(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
			{
				strcpy(path_new, (char*)file);
				result = 1;
			}
			FcPatternDestroy(font);
		}
		FcPatternDestroy(ofont);
	}
	return result;
}

FcPattern* BC_Resources::find_similar_font(FT_ULong char_code, FcPattern *oldfont)
{
	FcPattern *pat, *font;
	FcFontSet *fs;
	FcObjectSet *os;
	FcCharSet *fcs;
	FcChar8 *file;
	double dval;
	int ival;

	// Do not search control codes
	if(char_code < ' ')
		return 0;

	fontconfig_lock.lock("BC_Resources::find_similar_font");
	pat = FcPatternCreate();
	os = FcObjectSetBuild(FC_FILE, FC_CHARSET, FC_SCALABLE, FC_FAMILY,
		FC_SLANT, FC_WEIGHT, FC_WIDTH, (char *)0);

	FcPatternAddBool(pat, FC_SCALABLE, true);
	fcs = FcCharSetCreate();
	if(FcCharSetAddChar(fcs, char_code))
		FcPatternAddCharSet(pat, FC_CHARSET, fcs);
	FcCharSetDestroy(fcs);
	for(int i = 0; i < LEN_FCPROP; i++)
	{
		if(FcPatternGetInteger(oldfont, fc_properties[i], 0, &ival) == FcResultMatch)
			FcPatternAddInteger(pat, fc_properties[i], ival);
	}
	fs = FcFontList(0, pat, os);

	for(int i = LEN_FCPROP - 1; i >= 0 && fs->nfont == 0; i--)
	{
		FcFontSetDestroy(fs);
		FcPatternDel(pat, fc_properties[i]);
		fs = FcFontList(0, pat, os);
	}
	FcPatternDestroy(pat);
	FcObjectSetDestroy(os);

	pat = 0;

	for (int i = 0; i < fs->nfont; i++)
	{
		font = fs->fonts[i];
		if(FcPatternGetCharSet(font, FC_CHARSET, 0, &fcs) == FcResultMatch)
		{
			if(FcCharSetHasChar(fcs, char_code))
			{
				pat =  FcPatternDuplicate(font);
				break;
			}
		}
	}
	FcFontSetDestroy(fs);
	fontconfig_lock.unlock();

	return pat;
}

