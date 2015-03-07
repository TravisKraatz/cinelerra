
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

#define GL_GLEXT_PROTOTYPES
#include "bcpixmap.h"
#include "bcresources.h"
#include "bcsignals.h"
#include "bcsynchronous.h"
#include "bcwindowbase.h"

// OpenGL functions in BC_WindowBase

Visual *BC_WindowBase::glx_visual()
{
	Visual *visual = DefaultVisual(display, screen);
#ifdef HAVE_GL
	int fb_attrs[] = {
		GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER, True, // False,
		//GLX_DEPTH_SIZE, 1,
		//GLX_ACCUM_RED_SIZE, 1,
		//GLX_ACCUM_GREEN_SIZE, 1,
		//GLX_ACCUM_BLUE_SIZE, 1,
		//GLX_ACCUM_ALPHA_SIZE, 1,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_ALPHA_SIZE, 1,
		None
	};

	Display *dpy = top_level->get_display();
	int scr = top_level->get_screen();
	int n = 0;

	GLXFBConfig *fb_configs = glXChooseFBConfig(dpy, scr, fb_attrs, &n);
	if( !n || !fb_configs ) { // fallback to single buffered
		if( fb_configs ) XFree(fb_configs);
		fb_attrs[5] = False;  // GLX_DOUBLEBUFFER, False
		fb_configs = glXChooseFBConfig(dpy, scr, fb_attrs, &n);
	}
	if( !n || !fb_configs ) { // college try
		if( fb_configs ) XFree(fb_configs);
		fb_configs = glXChooseFBConfig(dpy, scr, None, &n);
	}
	if( n && fb_configs ) {
		fb_config = fb_configs[0];
		XVisualInfo *vis_info = glXGetVisualFromFBConfig(dpy, fb_config);
		if( vis_info ) {
			visual = vis_info->visual;
			XFree(vis_info);
		}
	}
	else
		printf("get_glx_context %d: glXChooseFBConfig failed\n", __LINE__);
	if( fb_configs ) XFree(fb_configs);
#endif
	return visual;
}

GLXContext BC_WindowBase::glx_get_context()
{
#ifdef HAVE_GL
	if( !gl_win_context && top_level->fb_config )
		gl_win_context = glXCreateNewContext(
			top_level->get_display(), top_level->fb_config,
			GLX_RGBA_TYPE, 0, True);
	if( !gl_win_context )
		printf("get_glx_context %d: glXCreateNewContext failed\n", __LINE__);
	return gl_win_context;
#endif
}

void BC_WindowBase::enable_opengl()
{
#ifdef HAVE_GL
	GLXContext gl_context = glx_get_context();
	if( !gl_context ) {
		printf("BC_WindowBase::enable_opengl %d: no glx context\n", __LINE__);
		exit(1);
	}
	top_level->sync_display();
	get_synchronous()->is_pbuffer = 0;
	get_synchronous()->current_window = this;
	glXMakeCurrent(top_level->display, win, gl_context);
#endif
}

void BC_WindowBase::disable_opengl()
{
#ifdef HAVE_GL
// 	unsigned long valuemask = CWEventMask;
// 	XSetWindowAttributes attributes;
// 	attributes.event_mask = DEFAULT_EVENT_MASKS;
// 	XChangeWindowAttributes(top_level->display, win, valuemask, &attributes);
#endif
}

void BC_WindowBase::flip_opengl()
{
#ifdef HAVE_GL
	glXSwapBuffers(top_level->display, glx_win);
	glFlush();
#endif
}

unsigned int BC_WindowBase::get_shader(char *source, int *got_it)
{
	return get_resources()->get_synchronous()->get_shader(source, got_it);
}

void BC_WindowBase::put_shader(unsigned int handle, char *source)
{
	get_resources()->get_synchronous()->put_shader(handle, source);
}

int BC_WindowBase::get_opengl_server_version()
{
#ifdef HAVE_GL
	int maj, min;
	if(glXQueryVersion(get_display(), &maj, &min))
		return 100 * maj + min;
#endif
	return 0;
}
