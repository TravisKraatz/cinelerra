
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

#ifdef HAVE_GL
int BC_WindowBase::glx_fb_configs(int *attrs, GLXFBConfig *&fb_cfgs, int &ncfgs)
{
	ncfgs = 0;
	fb_cfgs = glXChooseFBConfig(get_display(), get_screen(), attrs, &ncfgs);
	if( !fb_cfgs ) ncfgs = 0;
	else if( !ncfgs && fb_cfgs ) { XFree(fb_cfgs);  fb_cfgs = 0; }
	if( !fb_cfgs )
		printf("BC_WindowBase::get_fb_config %d: failed\n", __LINE__);
	return ncfgs;
}

static int glx_window_fb_msgs = 0;

GLXFBConfig *BC_WindowBase::glx_window_fb_configs()
{
	if( glx_fbcfgs_window ) return glx_fbcfgs_window;

	int fb_attrs[] = {
		GLX_DRAWABLE_TYPE,	GLX_WINDOW_BIT | GLX_PBUFFER | GLX_PIXMAP_BIT,
		GLX_RENDER_TYPE,	GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER,	True,
		GLX_ACCUM_RED_SIZE,	1,
		GLX_ACCUM_GREEN_SIZE,	1,
		GLX_ACCUM_BLUE_SIZE,	1,
		GLX_ACCUM_ALPHA_SIZE,	1,
		GLX_RED_SIZE,		8,
		GLX_GREEN_SIZE,		8,
		GLX_BLUE_SIZE,		8,
		GLX_ALPHA_SIZE,		8,
		None
	};
	if( glx_fb_configs(fb_attrs, glx_fbcfgs_window, n_fbcfgs_window) )
		return glx_fbcfgs_window;

	if( glx_window_fb_msgs < 1 ) {
		++glx_window_fb_msgs;
		printf("BC_WindowBase::glx_window_fb_config %d: trying single buffering\n", __LINE__);
	}

	fb_attrs[5] = False;
	if( glx_fb_configs(fb_attrs, glx_fbcfgs_window, n_fbcfgs_window) )
		return glx_fbcfgs_window;

	if( glx_window_fb_msgs < 2 ) {
		++glx_window_fb_msgs;
		printf("BC_WindowBase::glx_window_fb_config %d: trying attributes None\n", __LINE__);
	}

	if( glx_fb_configs(None, glx_fbcfgs_window, n_fbcfgs_window) )
		return glx_fbcfgs_window;

	printf("BC_WindowBase::glx_window_fb_config %d: failed\n", __LINE__);
	return 0;
}

static int glx_pbuffer_fb_msgs = 0;

GLXFBConfig *BC_WindowBase::glx_pbuffer_fb_configs()
{
	if( glx_fbcfgs_pbuffer ) return glx_fbcfgs_pbuffer;

       static int fb_attrs[] = {
		GLX_DRAWABLE_TYPE,	GLX_PBUFFER_BIT | GLX_PIXMAP_BIT,
		GLX_RENDER_TYPE,	GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER,	True, //False,
		GLX_DEPTH_SIZE,		1,
		GLX_ACCUM_RED_SIZE,	1,
		GLX_ACCUM_GREEN_SIZE,	1,
		GLX_ACCUM_BLUE_SIZE,	1,
		GLX_ACCUM_ALPHA_SIZE,	1,
		GLX_RED_SIZE,		8,
		GLX_GREEN_SIZE,		8,
		GLX_BLUE_SIZE,		8,
		GLX_ALPHA_SIZE,		8,
		None
	};

	if( glx_fb_configs(fb_attrs, glx_fbcfgs_pbuffer, n_fbcfgs_pbuffer) )
		return glx_fbcfgs_pbuffer;

	if( glx_pbuffer_fb_msgs < 1 ) {
		++glx_pbuffer_fb_msgs;
		printf("BC_WindowBase::glx_pbuffer_fb_config %d: trying attributes None\n", __LINE__);
	}

	if( glx_fb_configs(None, glx_fbcfgs_pbuffer, n_fbcfgs_pbuffer) )
		return glx_fbcfgs_pbuffer;

	printf("BC_WindowBase::glx_pbuffer_fb_config %d: failed\n", __LINE__);
	return 0;
}

GLXFBConfig *BC_WindowBase::glx_pixmap_fb_configs()
{
	if( glx_fbcfgs_pixmap ) return glx_fbcfgs_pixmap;

       static int fb_attrs[] = {
		GLX_DRAWABLE_TYPE,	GLX_PIXMAP_BIT | GLX_PBUFFER,
		GLX_RENDER_TYPE,	GLX_RGBA_BIT,
		GLX_DOUBLEBUFFER,	True, //False,
		GLX_RED_SIZE,		8,
		GLX_GREEN_SIZE,		8,
		GLX_BLUE_SIZE,		8,
		GLX_ALPHA_SIZE,		8,
		None
	};

	if( glx_fb_configs(fb_attrs, glx_fbcfgs_pixmap, n_fbcfgs_pixmap) )
		return glx_fbcfgs_pixmap;

	printf("BC_WindowBase::glx_pixmap_fb_config %d: failed\n", __LINE__);
	return 0;
}

GLXContext BC_WindowBase::glx_get_context()
{
	if( !glx_win_context && top_level->glx_fb_config )
		glx_win_context = glXCreateNewContext(
			top_level->get_display(), top_level->glx_fb_config,
			GLX_RGBA_TYPE, 0, True);
	if( !glx_win_context )
		printf("BC_WindowBase::get_glx_context %d: failed\n", __LINE__);
	return glx_win_context;
}

bool BC_WindowBase::glx_make_current(GLXDrawable draw, GLXContext glx_ctxt)
{
	return glXMakeContextCurrent(get_display(), draw, draw, glx_ctxt);
}

bool BC_WindowBase::glx_make_current(GLXDrawable draw)
{
	return glXMakeContextCurrent(get_display(), draw, draw, glx_win_context);
}

#endif

void BC_WindowBase::enable_opengl()
{
#ifdef HAVE_GL
	GLXContext glx_context = glx_get_context();
	if( !glx_context ) {
		printf("BC_WindowBase::enable_opengl %d: no glx context\n", __LINE__);
		exit(1);
	}
	top_level->sync_display();
	get_synchronous()->is_pbuffer = 0;
	get_synchronous()->current_window = this;
	glx_make_current(glx_win, glx_context);
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
