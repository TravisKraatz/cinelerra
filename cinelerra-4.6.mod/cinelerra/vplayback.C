
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

#include "audiodevice.h"
#include "edl.h"
#include "edlsession.h"
#include "playtransport.h"
#include "renderengine.h"
#include "transportque.h"
#include "vplayback.h"
#include "vtracking.h"
#include "vwindow.h"
#include "vwindowgui.h"

// Playback engine for viewer

VPlayback::VPlayback(MWindow *mwindow, VWindow *vwindow, Canvas *output)
 : PlaybackEngine(mwindow, output)
{
	this->vwindow = vwindow;
}

int VPlayback::create_render_engine()
{
	return PlaybackEngine::create_render_engine();
}

void VPlayback::init_cursor()
{
	vwindow->playback_cursor->start_playback(tracking_position);
}

void VPlayback::init_meters()
{
	AudioDevice *audio = render_engine->audio;
	int dmix = audio && (audio->get_idmix() || audio->get_odmix());
	vwindow->gui->meters->init_meters(dmix);
}

void VPlayback::stop_cursor()
{
	vwindow->playback_cursor->stop_playback();
}


