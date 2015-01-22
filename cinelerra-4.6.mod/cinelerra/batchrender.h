
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

#ifndef BATCHRENDER_H
#define BATCHRENDER_H

#include "arraylist.h"
#include "asset.inc"
#include "batchrender.inc"
#include "bcbutton.h"
#include "bcdialog.h"
#include "browsebutton.inc"
#include "filexml.inc"
#include "formattools.h"
#include "keyframe.inc"
#include "mwindow.inc"
#include "preferences.inc"
#include "timeentry.h"

#define BATCHRENDER_COLUMNS 4




class BatchRenderMenuItem : public BC_MenuItem
{
public:
	BatchRenderMenuItem(MWindow *mwindow);
	int handle_event();
	MWindow *mwindow;
};



class BatchRenderJob
{
public:
	BatchRenderJob(Preferences *preferences);
	~BatchRenderJob();

	void copy_from(BatchRenderJob *src);
	void load(FileXML *file);
	void save(FileXML *file);
	void fix_strategy();

// Source EDL to render
	char edl_path[BCTEXTLEN];
// Destination file for output
	Asset *asset;
	int strategy;
	int enabled;
// Amount of time elapsed in last render operation
	double elapsed;
	Preferences *preferences;
};








class BatchRenderThread : public BC_DialogThread
{
public:
	BatchRenderThread(MWindow *mwindow);
	BatchRenderThread();

	void handle_close_event(int result);
	BC_Window* new_gui();

	int test_edl_files();
	void calculate_dest_paths(ArrayList<char*> *paths,
		Preferences *preferences);

// Load batch rendering jobs
	void load_jobs(char *path, Preferences *preferences);
// Not applicable to western civilizations
	void save_jobs(char *path);
	void load_defaults(BC_Hash *defaults);
	void save_defaults(BC_Hash *defaults);
// Create path for persistent storage functions
	char* create_path(char *string);
	void new_job();
	void delete_job();
	void update_selected_edl();
	void use_current_edl();
// Conditionally returns the job or the default job based on current_job
	BatchRenderJob* get_current_job();
	Asset* get_current_asset();
	char* get_current_edl();
// For command line usage
	void start_rendering(char *config_path, char *batch_path);
// For GUI usage
	void start_rendering();
	void stop_rendering();
// Highlight the currently rendering job.
	void update_active(int number);
	void update_done(int number, int create_list, double elapsed_time);
	void move_batch(int src, int dst);

	MWindow *mwindow;
	double current_start;
	double current_end;
	BatchRenderJob *default_job;
	ArrayList<BatchRenderJob*> jobs;
	BatchRenderGUI *gui;
	int column_width[BATCHRENDER_COLUMNS];
// job being edited
	int current_job;
// job being rendered
	int rendering_job;
	int is_rendering;
	ArrayList<BC_ListBoxItem*> *file_entries;
};










class BatchRenderEDLPath : public BC_TextBox
{
public:
	BatchRenderEDLPath(BatchRenderThread *thread, 
		int x, 
		int y, 
		int w, 
		char *text);
	int handle_event();
	BatchRenderThread *thread;
};


class BatchRenderCurrentEDL : public BC_GenericButton
{
public:
	BatchRenderCurrentEDL(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};


class BatchRenderUpdateEDL : public BC_GenericButton
{
public:
	BatchRenderUpdateEDL(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};


class BatchRenderNew : public BC_GenericButton
{
public:
	BatchRenderNew(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};

class BatchRenderDelete : public BC_GenericButton
{
public:
	BatchRenderDelete(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};

class BatchRenderList : public BC_ListBox
{
public:
	BatchRenderList(BatchRenderThread *thread, 
		int x, 
		int y,
		int w,
		int h);
	int handle_event();
	int selection_changed();
	int column_resize_event();
	int drag_start_event();
	int drag_motion_event();
	int drag_stop_event();
	int dragging_item;
	BatchRenderThread *thread;
};
class BatchRenderStart : public BC_GenericButton
{
public:
	BatchRenderStart(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};

class BatchRenderStop : public BC_GenericButton
{
public:
	BatchRenderStop(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	BatchRenderThread *thread;
};

class BatchRenderCancel : public BC_GenericButton
{
public:
	BatchRenderCancel(BatchRenderThread *thread, 
		int x, 
		int y);
	int handle_event();
	int keypress_event();
	BatchRenderThread *thread;
};


class BatchFormat : public FormatTools
{
public:
	BatchFormat(MWindow *mwindow,
				BatchRenderGUI *gui,
				Asset *asset);
	~BatchFormat();

	int handle_event();

	BatchRenderGUI *gui;
	MWindow *mwindow;
};


class BatchRenderGUI : public BC_Window
{
public:
	BatchRenderGUI(MWindow *mwindow, 
		BatchRenderThread *thread,
		int x,
		int y,
		int w,
		int h);
	~BatchRenderGUI();

	void create_objects();
	int resize_event(int w, int h);
	int translation_event();
	int close_event();
	void create_list(int update_widget);
	void change_job();
	void button_enable();
	void button_disable();

	ArrayList<BC_ListBoxItem*> list_columns[BATCHRENDER_COLUMNS];

	MWindow *mwindow;
	BatchRenderThread *thread;
	BC_Title *output_path_title;
	BatchFormat *format_tools;
	BrowseButton *edl_path_browse;
	BatchRenderEDLPath *edl_path_text;
	BC_Title *edl_path_title;
//	BC_Title *status_title;
//	BC_Title *status_text;
//	BC_ProgressBar *progress_bar;
	BC_Title *list_title;
	BatchRenderNew *new_batch;
	BatchRenderDelete *delete_batch;
	BatchRenderList *batch_list;
	BatchRenderStart *start_button;
	BatchRenderStop *stop_button;
	BatchRenderCancel *cancel_button;
	BatchRenderCurrentEDL *use_current_edl;
	BatchRenderUpdateEDL *update_selected_edl;
};




class CreateDVD_MenuItem : public BC_MenuItem
{
public:
	CreateDVD_MenuItem(MWindow *mwindow);
	int handle_event();
	MWindow *mwindow;
};


class CreateDVD_Thread : public BC_DialogThread
{
	static const int64_t DVD_SIZE;
	static const int DVD_STREAMS, DVD_WIDTH, DVD_HEIGHT;
	static const double DVD_ASPECT_WIDTH, DVD_ASPECT_HEIGHT;
	static const double DVD_WIDE_ASPECT_WIDTH, DVD_WIDE_ASPECT_HEIGHT;
	static const int DVD_MAX_BITRATE, DVD_CHANNELS, DVD_WIDE_CHANNELS;
	static const double DVD_FRAMERATE, DVD_SAMPLERATE, DVD_KAUDIO_RATE;
public:
	CreateDVD_Thread(MWindow *mwindow);
	~CreateDVD_Thread();
	void handle_close_event(int result);
	BC_Window* new_gui();
	int option_presets();
	int create_dvd_jobs(ArrayList<BatchRenderJob*> *jobs,
		const char *tmp_path, const char *asset_title);
	int insert_video_plugin(const char *title, KeyFrame *default_keyframe);
	int resize_tracks();

	MWindow *mwindow;
	CreateDVD_GUI *gui;
	char asset_title[BCTEXTLEN];
	char tmp_path[BCTEXTLEN];
	int use_deinterlace, use_inverse_telecine;
	int use_scale, use_resize_tracks;
	int use_wide_audio, use_wide_aspect;
	int use_histogram, use_label_chapters;
};

class CreateDVD_OK : public BC_OKButton
{
public:
	CreateDVD_OK(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_OK();
	int button_press_event();
	int keypress_event();

	CreateDVD_GUI *gui;
};

class CreateDVD_Cancel : public BC_CancelButton
{
public:
	CreateDVD_Cancel(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_Cancel();
	int button_press_event();

	CreateDVD_GUI *gui;
};


class CreateDVD_DiskSpace : public BC_Title
{
public:
	CreateDVD_DiskSpace(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_DiskSpace();
	int64_t tmp_path_space();
	void update();

	CreateDVD_GUI *gui;
};

class CreateDVD_TmpPath : public BC_TextBox
{
public:
	CreateDVD_TmpPath(CreateDVD_GUI *gui, int x, int y, int w);
	~CreateDVD_TmpPath();
	int handle_event();

	CreateDVD_GUI *gui;
};


class CreateDVD_AssetTitle : public BC_TextBox
{
public:
	CreateDVD_AssetTitle(CreateDVD_GUI *gui, int x, int y, int w);
	~CreateDVD_AssetTitle();

	CreateDVD_GUI *gui;
};

class CreateDVD_Deinterlace : public BC_CheckBox
{
public:
	CreateDVD_Deinterlace(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_Deinterlace();
	int handle_event();

	CreateDVD_GUI *gui;
};

class CreateDVD_InverseTelecine : public BC_CheckBox
{
public:
	CreateDVD_InverseTelecine(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_InverseTelecine();
	int handle_event();

	CreateDVD_GUI *gui;
};

class CreateDVD_Scale : public BC_CheckBox
{
public:
	CreateDVD_Scale(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_Scale();

	CreateDVD_GUI *gui;
};

class CreateDVD_ResizeTracks : public BC_CheckBox
{
public:
	CreateDVD_ResizeTracks(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_ResizeTracks();

	CreateDVD_GUI *gui;
};

class CreateDVD_Histogram : public BC_CheckBox
{
public:
	CreateDVD_Histogram(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_Histogram();

	CreateDVD_GUI *gui;
};

class CreateDVD_LabelChapters : public BC_CheckBox
{
public:
	CreateDVD_LabelChapters(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_LabelChapters();

	CreateDVD_GUI *gui;
};

class CreateDVD_WideAudio : public BC_CheckBox
{
public:
	CreateDVD_WideAudio(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_WideAudio();

	CreateDVD_GUI *gui;
};

class CreateDVD_WideAspect : public BC_CheckBox
{
public:
	CreateDVD_WideAspect(CreateDVD_GUI *gui, int x, int y);
	~CreateDVD_WideAspect();

	CreateDVD_GUI *gui;
};

class CreateDVD_GUI : public BC_Window
{
public:
	CreateDVD_GUI(CreateDVD_Thread *thread,
		int x, int y, int w, int h);
	~CreateDVD_GUI();

	void create_objects();
	int resize_event(int w, int h);
	int translation_event();
	int close_event();

	int64_t needed_disk_space;
	CreateDVD_Thread *thread;
	int at_x, at_y;
	CreateDVD_AssetTitle *asset_title;
	int tmp_x, tmp_y;
	CreateDVD_TmpPath *tmp_path;
	CreateDVD_DiskSpace *disk_space;
	CreateDVD_Deinterlace *need_deinterlace;
	CreateDVD_InverseTelecine *need_inverse_telecine;
	CreateDVD_Scale *need_scale;
	CreateDVD_ResizeTracks *need_resize_tracks;
	CreateDVD_Histogram *need_histogram;
	CreateDVD_WideAudio *need_wide_audio;
	CreateDVD_WideAspect *need_wide_aspect;
	CreateDVD_LabelChapters *need_label_chapters;
	int ok_x, ok_y, ok_w, ok_h;
	CreateDVD_OK *ok;
	int cancel_x, cancel_y, cancel_w, cancel_h;
	CreateDVD_Cancel *cancel;
};




#endif
