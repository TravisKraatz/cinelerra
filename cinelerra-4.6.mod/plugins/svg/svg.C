
/*
 * CINELERRA
 * Copyright (C) 2008 Adam Williams <broadcast at earthling dot net>
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

#include "clip.h"
#include "filexml.h"
#include "picon_png.h"
#include "svg.h"
#include "svgwin.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>


#include <libintl.h>
#define _(String) gettext(String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)

#include "empty_svg.h"

REGISTER_PLUGIN(SvgMain)

SvgConfig::SvgConfig()
{
	in_x = 0;
	in_y = 0;
	in_w = 720;
	in_h = 480;
	out_x = 0;
	out_y = 0;
	out_w = 720;
	out_h = 480;
	strcpy(svg_file, "");
}

int SvgConfig::equivalent(SvgConfig &that)
{
	return EQUIV(in_x, that.in_x) && 
		EQUIV(in_y, that.in_y) && 
		EQUIV(in_w, that.in_w) && 
		EQUIV(in_h, that.in_h) &&
		EQUIV(out_x, that.out_x) && 
		EQUIV(out_y, that.out_y) && 
		EQUIV(out_w, that.out_w) &&
		EQUIV(out_h, that.out_h) &&
		!strcmp(svg_file, that.svg_file);
}

void SvgConfig::copy_from(SvgConfig &that)
{
	in_x = that.in_x;
	in_y = that.in_y;
	in_w = that.in_w;
	in_h = that.in_h;
	out_x = that.out_x;
	out_y = that.out_y;
	out_w = that.out_w;
	out_h = that.out_h;
	strcpy(svg_file, that.svg_file);
}

void SvgConfig::interpolate(SvgConfig &prev, 
	SvgConfig &next, 
	long prev_frame, 
	long next_frame, 
	long current_frame)
{
	double next_scale = (double)(current_frame - prev_frame) / (next_frame - prev_frame);
	double prev_scale = (double)(next_frame - current_frame) / (next_frame - prev_frame);

	this->in_x = prev.in_x * prev_scale + next.in_x * next_scale;
	this->in_y = prev.in_y * prev_scale + next.in_y * next_scale;
	this->in_w = prev.in_w * prev_scale + next.in_w * next_scale;
	this->in_h = prev.in_h * prev_scale + next.in_h * next_scale;
	this->out_x = prev.out_x * prev_scale + next.out_x * next_scale;
	this->out_y = prev.out_y * prev_scale + next.out_y * next_scale;
	this->out_w = prev.out_w * prev_scale + next.out_w * next_scale;
	this->out_h = prev.out_h * prev_scale + next.out_h * next_scale;
	strcpy(this->svg_file, prev.svg_file);
}








SvgMain::SvgMain(PluginServer *server)
 : PluginVClient(server)
{
	ofrm = 0;
	overlayer = 0;
	need_reconfigure = 1;
}

SvgMain::~SvgMain()
{
	delete ofrm;
	delete overlayer;
}

const char* SvgMain::plugin_title() { return N_("SVG via Inkscape"); }
int SvgMain::is_realtime() { return 1; }
int SvgMain::is_synthesis() { return 1; }

NEW_PICON_MACRO(SvgMain)

LOAD_CONFIGURATION_MACRO(SvgMain, SvgConfig)

void SvgMain::save_data(KeyFrame *keyframe)
{
	FileXML output;

// cause data to be stored directly in text
	output.set_shared_output(keyframe->get_data(), MESSAGESIZE);

// Store data
	output.tag.set_title("SVG");
	output.tag.set_property("IN_X", config.in_x);
	output.tag.set_property("IN_Y", config.in_y);
	output.tag.set_property("IN_W", config.in_w);
	output.tag.set_property("IN_H", config.in_h);
	output.tag.set_property("OUT_X", config.out_x);
	output.tag.set_property("OUT_Y", config.out_y);
	output.tag.set_property("OUT_W", config.out_w);
	output.tag.set_property("OUT_H", config.out_h);
	output.tag.set_property("SVG_FILE", config.svg_file);
	output.append_tag();
	output.tag.set_title("/SVG");
	output.append_tag();

	output.terminate_string();
// data is now in *text
}

void SvgMain::read_data(KeyFrame *keyframe)
{
	FileXML input;

	const char *data = keyframe->get_data();
	input.set_shared_input((char*)data, strlen(data));

	int result = 0;

	while(!result)
	{
		result = input.read_tag();

		if(!result)
		{
			if(input.tag.title_is("SVG"))
			{
 				config.in_x = input.tag.get_property("IN_X", config.in_x);
				config.in_y = input.tag.get_property("IN_Y", config.in_y);
				config.in_w = input.tag.get_property("IN_W", config.in_w);
				config.in_h = input.tag.get_property("IN_H", config.in_h);
				config.out_x =	input.tag.get_property("OUT_X", config.out_x);
				config.out_y =	input.tag.get_property("OUT_Y", config.out_y);
				config.out_w =	input.tag.get_property("OUT_W", config.out_w);
				config.out_h =	input.tag.get_property("OUT_H", config.out_h);
				input.tag.get_property("SVG_FILE", config.svg_file);
			}
		}
	}
}








int SvgMain::process_realtime(VFrame *input, VFrame *output)
{

	need_reconfigure |= load_configuration();
	output->copy_from(input);
	if( config.svg_file[0] == 0 ) return 0;

	if( need_reconfigure ) {
		need_reconfigure = 0;
		delete ofrm;  ofrm = 0;
		char filename_png[1024];
		strcpy(filename_png, config.svg_file);
		strncat(filename_png, ".png", sizeof(filename_png));
		int fd = open(filename_png, O_RDWR);
		if( fd < 0 ) { // file does not exist, export it
			char command[1024];
			sprintf(command,
				"inkscape --without-gui --export-background=0x000000 "
				"--export-background-opacity=0 %s --export-png=%s",
				config.svg_file, filename_png);
			printf(_("Running command %s\n"), command);
			system(command);
			// in order for lockf to work it has to be open for writing
			fd = open(filename_png, O_RDWR);
			if( fd < 0 )
				printf(_("Export of %s to %s failed\n"), config.svg_file, filename_png);
		}
		if( fd >= 0 ) {
			// file exists, ... lock it, mmap it and check time_of_creation
			// Blocking call - will wait for inkscape to finish!
			lockf(fd, F_LOCK, 0);
			struct stat st_png;
			fstat(fd, &st_png);
			unsigned char *png_buffer = (unsigned char *)
				mmap (NULL, st_png.st_size, PROT_READ, MAP_SHARED, fd, 0); 
			if( png_buffer != MAP_FAILED ) {
				if( png_buffer[0] == 0x89 && png_buffer[1] == 0x50 &&
				    png_buffer[2] == 0x4e && png_buffer[3] == 0x47 ) {
					ofrm = new VFrame(png_buffer, st_png.st_size);
					if( ofrm->get_color_model() != output->get_color_model() ) {
						VFrame *vfrm = new VFrame(ofrm->get_w(), ofrm->get_h(),
							output->get_color_model());
						BC_CModels::transfer(vfrm->get_rows(), ofrm->get_rows(),
							0, 0, 0, 0, 0, 0,
							0, 0, ofrm->get_w(), ofrm->get_h(),
							0, 0, vfrm->get_w(), vfrm->get_h(), 
							ofrm->get_color_model(), vfrm->get_color_model(),
							0, ofrm->get_bytes_per_line(),
							vfrm->get_bytes_per_line());
						delete ofrm;  ofrm = vfrm;
					}
				}
				else
					printf (_("The file %s that was generated from %s is not in PNG format."
						  " Try to delete all *.png files.\n"), filename_png, config.svg_file);	
				munmap(png_buffer, st_png.st_size);
			}
			else
				printf(_("Access mmap to %s as %s failed.\n"), config.svg_file, filename_png);
			lockf(fd, F_ULOCK, 0);
			close(fd);
		}
	}
	if( ofrm ) {
		if(!overlayer) overlayer = new OverlayFrame(smp + 1);
		overlayer->overlay(output, ofrm,
			 0, 0, ofrm->get_w(), ofrm->get_h(),
			config.out_x, config.out_y, 
			config.out_x + ofrm->get_w(),
			config.out_y + ofrm->get_h(),
			1, TRANSFER_NORMAL,
			get_interpolation_type());
	}
	return 0;
}


NEW_WINDOW_MACRO(SvgMain, SvgWin)

void SvgMain::update_gui()
{
	if(thread)
	{
		load_configuration();
		SvgWin *window = (SvgWin*)thread->window;
		window->lock_window();
//		window->in_x->update(config.in_x);
//		window->in_y->update(config.in_y);
//		window->in_w->update(config.in_w);
//		window->in_h->update(config.in_h);
		window->out_x->update(config.out_x);
		window->out_y->update(config.out_y);
//		window->out_w->update(config.out_w);
//		window->out_h->update(config.out_h);
		window->svg_file_title->update(config.svg_file);
		window->unlock_window();
	}
}
