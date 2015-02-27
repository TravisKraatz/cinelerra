
/*
 * CINELERRA
 * Copyright (C) 2011 Adam Williams <broadcast at earthling dot net>
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

#include "asset.h"
#include "batchrender.h"
#include "bcdisplayinfo.h"
#include "bcsignals.h"
#include "confirmsave.h"
#include "cstrdup.h"
#include "bchash.h"
#include "edits.h"
#include "edit.h"
#include "edl.h"
#include "edlsession.h"
#include "errorbox.h"
#include "filesystem.h"
#include "filexml.h"
#include "format.inc"
#include "keyframe.h"
#include "keys.h"
#include "labels.h"
#include "language.h"
#include "mainerror.h"
#include "mainundo.h"
#include "mainsession.h"
#include "mwindow.h"
#include "mwindowgui.h"
#include "packagedispatcher.h"
#include "packagerenderer.h"
#include "plugin.h"
#include "pluginset.h"
#include "preferences.h"
#include "render.h"
#include "theme.h"
#include "tracks.h"
#include "transportque.h"
#include "vframe.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/statfs.h>



static const char *list_titles[] = 
{
	"Enabled", 
	"Output",
	"EDL",
	"Elapsed"
};

static int list_widths[] =
{
	50,
	100,
	200,
	100
};

BatchRenderMenuItem::BatchRenderMenuItem(MWindow *mwindow)
 : BC_MenuItem(_("Batch Render..."), "Shift-B", 'B')
{
	set_shift(1); 
	this->mwindow = mwindow;
}

int BatchRenderMenuItem::handle_event()
{
	mwindow->batch_render->start();
	return 1;
}








BatchRenderJob::BatchRenderJob(Preferences *preferences)
{
	this->preferences = preferences;
	asset = new Asset;
	edl_path[0] = 0;
	strategy = 0;
	enabled = 1;
	elapsed = 0;
}

BatchRenderJob::~BatchRenderJob()
{
	asset->Garbage::remove_user();
}

void BatchRenderJob::copy_from(BatchRenderJob *src)
{
	asset->copy_from(src->asset, 0);
	strcpy(edl_path, src->edl_path);
	strategy = src->strategy;
	enabled = src->enabled;
	elapsed = 0;
}

void BatchRenderJob::load(FileXML *file)
{
	int result = 0;

	edl_path[0] = 0;
	file->tag.get_property("EDL_PATH", edl_path);
	strategy = file->tag.get_property("STRATEGY", strategy);
	enabled = file->tag.get_property("ENABLED", enabled);
	elapsed = file->tag.get_property("ELAPSED", elapsed);
	fix_strategy();

	result = file->read_tag();
	if(!result)
	{
		if(file->tag.title_is("ASSET"))
		{
			file->tag.get_property("SRC", asset->path);
			asset->read(file, 0);
// The compression parameters are stored in the defaults to reduce
// coding maintenance.  The defaults must now be stuffed into the XML for
// unique storage.
			BC_Hash defaults;
			defaults.load_string(file->read_text());
			asset->load_defaults(&defaults,
				"",
				0,
				1,
				0,
				0,
				0);
		}
	}
}

void BatchRenderJob::save(FileXML *file)
{
	file->tag.set_property("EDL_PATH", edl_path);
	file->tag.set_property("STRATEGY", strategy);
	file->tag.set_property("ENABLED", enabled);
	file->tag.set_property("ELAPSED", elapsed);
	file->append_tag();
	file->append_newline();
	asset->write(file,
		0,
		"");

// The compression parameters are stored in the defaults to reduce
// coding maintenance.  The defaults must now be stuffed into the XML for
// unique storage.
	BC_Hash defaults;
	asset->save_defaults(&defaults, 
		"",
		0,
		1,
		0,
		0,
		0);
	char *string;
	defaults.save_string(string);
	file->append_text(string);
	delete [] string;
	file->tag.set_title("/JOB");
	file->append_tag();
	file->append_newline();
}

void BatchRenderJob::fix_strategy()
{
	strategy = Render::fix_strategy(strategy, preferences->use_renderfarm);
}










BatchRenderThread::BatchRenderThread(MWindow *mwindow)
 : BC_DialogThread()
{
	this->mwindow = mwindow;
	current_job = 0;
	rendering_job = -1;
	is_rendering = 0;
	default_job = 0;
	file_entries = 0;
}

BatchRenderThread::BatchRenderThread()
 : BC_DialogThread()
{
	mwindow = 0;
	current_job = 0;
	rendering_job = -1;
	is_rendering = 0;
	default_job = 0;
	file_entries = 0;
}

void BatchRenderThread::handle_close_event(int result)
{
// Save settings
	char path[BCTEXTLEN];
	path[0] = 0;
	save_jobs(path);
	save_defaults(mwindow->defaults);
	delete default_job;
	default_job = 0;
	jobs.remove_all_objects();
	if(file_entries)
	{
		file_entries->remove_all_objects();
		delete file_entries;
		file_entries = 0;
	}
}

BC_Window* BatchRenderThread::new_gui()
{
	current_start = 0.0;
	current_end = 0.0;
	default_job = new BatchRenderJob(mwindow->preferences);
	
	
	if(!file_entries)
	{
		file_entries = new ArrayList<BC_ListBoxItem*>;
		FileSystem fs;
		char string[BCTEXTLEN];
	// Load current directory
		fs.update(getcwd(string, BCTEXTLEN));
		for(int i = 0; i < fs.total_files(); i++)
		{
			file_entries->append(
				new BC_ListBoxItem(
					fs.get_entry(i)->get_name()));
		}
	}

	char path[BCTEXTLEN];
	path[0] = 0;
	load_jobs(path, mwindow->preferences);
	load_defaults(mwindow->defaults);
	this->gui = new BatchRenderGUI(mwindow, 
		this,
		mwindow->session->batchrender_x,
		mwindow->session->batchrender_y,
		mwindow->session->batchrender_w,
		mwindow->session->batchrender_h);
	this->gui->create_objects();
	return this->gui;
}


void BatchRenderThread::load_jobs(char *path, Preferences *preferences)
{
	FileXML file;
	int result = 0;

	jobs.remove_all_objects();
	if(path[0])
		file.read_from_file(path);
	else
		file.read_from_file(create_path(path));

	while(!result)
	{
		if(!(result = file.read_tag()))
		{
			if(file.tag.title_is("JOB"))
			{
				BatchRenderJob *job;
				jobs.append(job = new BatchRenderJob(preferences));
				job->load(&file);
			}
		}
	}
}

void BatchRenderThread::save_jobs(char *path)
{
	FileXML file;

	for(int i = 0; i < jobs.total; i++)
	{
		file.tag.set_title("JOB");
		jobs.values[i]->save(&file);
	}

	if(path[0])
		file.write_to_file(path);
	else
		file.write_to_file(create_path(path));
}

void BatchRenderThread::load_defaults(BC_Hash *defaults)
{
	if(default_job)
	{
		default_job->asset->load_defaults(defaults,
			"BATCHRENDER_",
			1,
			1,
			1,
			1,
			1);
		default_job->fix_strategy();
	}

	for(int i = 0; i < BATCHRENDER_COLUMNS; i++)
	{
		char string[BCTEXTLEN];
		sprintf(string, "BATCHRENDER_COLUMN%d", i);
		column_width[i] = defaults->get(string, list_widths[i]);
	}
}

void BatchRenderThread::save_defaults(BC_Hash *defaults)
{
	if(default_job)
	{
		default_job->asset->save_defaults(defaults,
			"BATCHRENDER_",
			1,
			1,
			1,
			1,
			1);
		defaults->update("BATCHRENDER_STRATEGY", default_job->strategy);
	}
	for(int i = 0; i < BATCHRENDER_COLUMNS; i++)
	{
		char string[BCTEXTLEN];
		sprintf(string, "BATCHRENDER_COLUMN%d", i);
		defaults->update(string, column_width[i]);
	}
//	defaults->update("BATCHRENDER_JOB", current_job);
	if(mwindow)
		mwindow->save_defaults();
	else
		defaults->save();
}

char* BatchRenderThread::create_path(char *string)
{
	FileSystem fs;
	sprintf(string, "%s", BCASTDIR);
	fs.complete_path(string);
	strcat(string, BATCH_PATH);
	return string;
}

void BatchRenderThread::new_job()
{
	BatchRenderJob *result = new BatchRenderJob(mwindow->preferences);
	result->copy_from(get_current_job());
	jobs.append(result);
	current_job = jobs.total - 1;
	gui->create_list(1);
	gui->change_job();
}

void BatchRenderThread::delete_job()
{
	if(current_job < jobs.total && current_job >= 0)
	{
		jobs.remove_object_number(current_job);
		if(current_job > 0) current_job--;
		gui->create_list(1);
		gui->change_job();
	}
}

void BatchRenderThread::use_current_edl()
{
// printf("BatchRenderThread::use_current_edl %d %p %s\n", 
// __LINE__, 
// mwindow->edl->path, 
// mwindow->edl->path);

	strcpy(get_current_edl(), mwindow->edl->path);
	gui->create_list(1);
	gui->edl_path_text->update(get_current_edl());
}

void BatchRenderThread::update_selected_edl()
{
        FileXML xml_file;
	char *path = get_current_edl();
	EDL *edl = mwindow->edl;
        edl->save_xml(&xml_file, path, 0, 0);
        xml_file.terminate_string();
        if( xml_file.write_to_file(path) ) {
		char msg[BCTEXTLEN];
		sprintf(msg, "Unable to save: %s", path);
		MainError::show_error(msg);
	}
}

BatchRenderJob* BatchRenderThread::get_current_job()
{
	BatchRenderJob *result;
	if(current_job >= jobs.total || current_job < 0)
	{
		result = default_job;
	}
	else
	{
		result = jobs.values[current_job];
	}
	return result;
}


Asset* BatchRenderThread::get_current_asset()
{
	return get_current_job()->asset;
}

char* BatchRenderThread::get_current_edl()
{
	return get_current_job()->edl_path;
}


// Test EDL files for existence
int BatchRenderThread::test_edl_files()
{
	for(int i = 0; i < jobs.total; i++)
	{
		if(jobs.values[i]->enabled)
		{
			const char *path = jobs.values[i]->edl_path;
			if( *path == '@' ) ++path;
			FILE *fd = fopen(path, "r");
			if(!fd)
			{
				char string[BCTEXTLEN];
				sprintf(string, _("EDL %s not found.\n"), jobs.values[i]->edl_path);
				if(mwindow)
				{
					ErrorBox error_box(PROGRAM_NAME ": Error",
						mwindow->gui->get_abs_cursor_x(1),
						mwindow->gui->get_abs_cursor_y(1));
					error_box.create_objects(string);
					error_box.run_window();
					gui->button_enable();
				}
				else
				{
					fprintf(stderr, 
						"%s",
						string);
				}

				is_rendering = 0;
				return 1;
			}
			else
			{
				fclose(fd);
			}
		}
	}
	return 0;
}

void BatchRenderThread::calculate_dest_paths(ArrayList<char*> *paths,
	Preferences *preferences)
{
	for(int i = 0; i < jobs.total; i++)
	{
		BatchRenderJob *job = jobs.values[i];
		if(job->enabled && *job->edl_path != '@')
		{
			PackageDispatcher *packages = new PackageDispatcher;

// Load EDL
			TransportCommand *command = new TransportCommand;
			FileXML *file = new FileXML;
			file->read_from_file(job->edl_path);

// Use command to calculate range.
			command->command = NORMAL_FWD;
			command->get_edl()->load_xml(file, 
				LOAD_ALL);
			command->change_type = CHANGE_ALL;
			command->set_playback_range();
			command->adjust_playback_range();

// Create test packages
			packages->create_packages(mwindow,
				command->get_edl(),
				preferences,
				job->strategy, 
				job->asset, 
				command->start_position, 
				command->end_position,
				0);

// Append output paths allocated to total
			for(int j = 0; j < packages->get_total_packages(); j++)
			{
				RenderPackage *package = packages->get_package(j);
				paths->append(cstrdup(package->path));
			}

// Delete package harness
			delete packages;
			delete command;
			delete file;
		}
	}
}


void BatchRenderThread::start_rendering(char *config_path,
	char *batch_path)
{
	BC_Hash *boot_defaults;
	Preferences *preferences;
	Render *render;
	BC_Signals *signals = new BC_Signals;

//PRINT_TRACE
// Initialize stuff which MWindow does.
	signals->initialize();
	MWindow::init_defaults(boot_defaults, config_path);
	load_defaults(boot_defaults);
	preferences = new Preferences;
	preferences->load_defaults(boot_defaults);
	MWindow::init_plugins(0, preferences, 0);
	BC_WindowBase::get_resources()->vframe_shm = 1;
	MWindow::init_fileserver(preferences);


//PRINT_TRACE
	load_jobs(batch_path, preferences);
	save_jobs(batch_path);
	save_defaults(boot_defaults);

//PRINT_TRACE
// Test EDL files for existence
	if(test_edl_files()) return;

//PRINT_TRACE

// Predict all destination paths
	ArrayList<char*> paths;
	paths.set_array_delete();
	calculate_dest_paths(&paths, preferences);

//PRINT_TRACE
	int result = ConfirmSave::test_files(0, &paths);
	paths.remove_all_objects();
// Abort on any existing file because it's so hard to set this up.
	if(result) return;

//PRINT_TRACE
	render = new Render(0);
//PRINT_TRACE
	render->start_batches(&jobs, 
		boot_defaults,
		preferences);
//PRINT_TRACE
}

void BatchRenderThread::start_rendering()
{
	if(is_rendering) return;

	is_rendering = 1;
	char path[BCTEXTLEN];
	path[0] = 0;
	save_jobs(path);
	save_defaults(mwindow->defaults);
	gui->button_disable();

// Test EDL files for existence
	if(test_edl_files()) return;

// Predict all destination paths
	ArrayList<char*> paths;
	calculate_dest_paths(&paths,
		mwindow->preferences);

// Test destination files for overwrite
	int result = ConfirmSave::test_files(mwindow, &paths);
	paths.remove_all_objects();

// User cancelled
	if(result)
	{
		is_rendering = 0;
		gui->button_enable();
		return;
	}

	mwindow->render->start_batches(&jobs);
}

void BatchRenderThread::stop_rendering()
{
	if(!is_rendering) return;
	mwindow->render->stop_operation();
	is_rendering = 0;
}

void BatchRenderThread::update_active(int number)
{
	gui->lock_window("BatchRenderThread::update_active");
	if(number >= 0)
	{
		current_job = number;
		rendering_job = number;
	}
	else
	{
		rendering_job = -1;
		is_rendering = 0;
	}
	gui->create_list(1);
	gui->unlock_window();
}

void BatchRenderThread::update_done(int number, 
	int create_list, 
	double elapsed_time)
{
	gui->lock_window("BatchRenderThread::update_done");
	if(number < 0)
	{
		gui->button_enable();
	}
	else
	{
		jobs.values[number]->enabled = 0;
		jobs.values[number]->elapsed = elapsed_time;
		if(create_list) gui->create_list(1);
	}
	gui->unlock_window();
}

void BatchRenderThread::move_batch(int src, int dst)
{
	BatchRenderJob *src_job = jobs.values[src];
	if(dst < 0) dst = jobs.total - 1;

	if(dst != src)
	{
		for(int i = src; i < jobs.total - 1; i++)
			jobs.values[i] = jobs.values[i + 1];
//		if(dst > src) dst--;
		for(int i = jobs.total - 1; i > dst; i--)
			jobs.values[i] = jobs.values[i - 1];
		jobs.values[dst] = src_job;
		gui->create_list(1);
	}
}







BatchRenderGUI::BatchRenderGUI(MWindow *mwindow, 
	BatchRenderThread *thread,
	int x,
	int y,
	int w,
	int h)
 : BC_Window(PROGRAM_NAME ": Batch Render", 
	x,
	y,
	w, 
	h, 
	50, 
	50, 
	1,
	0, 
	1)
{
	this->mwindow = mwindow;
	this->thread = thread;
}

BatchRenderGUI::~BatchRenderGUI()
{
	lock_window("BatchRenderGUI::~BatchRenderGUI");
	delete format_tools;
	unlock_window();
}


void BatchRenderGUI::create_objects()
{
	lock_window("BatchRenderGUI::create_objects");
	mwindow->theme->get_batchrender_sizes(this, get_w(), get_h());
	create_list(0);

	int x = mwindow->theme->batchrender_x1;
	int y = 5;
	int x1 = mwindow->theme->batchrender_x1;
	int x2 = mwindow->theme->batchrender_x2;
	//int x3 = mwindow->theme->batchrender_x3;
	int y1 = y;
	int y2;

// output file
	add_subwindow(output_path_title = new BC_Title(x1, y, _("Output path:")));
	y += 20;
	format_tools = new BatchFormat(mwindow,
					this, 
					thread->get_current_asset());
	format_tools->set_w(get_w() / 2);
	format_tools->create_objects(x, 
						y, 
						1, 
						1, 
						1, 
						1, 
						0, 
						1, 
						0, 
						0, 
						&thread->get_current_job()->strategy, 
						0);

	x2 = x;
	y2 = y + 10;
	x += format_tools->get_w();
	y = y1;
	x1 = x;
	//x3 = x + 80;

// input EDL
	x = x1;
	add_subwindow(edl_path_title = new BC_Title(x, y, _("EDL Path:")));
	y += 20;
	add_subwindow(edl_path_text = new BatchRenderEDLPath(
		thread, 
		x, 
		y, 
		get_w() - x - 40, 
		thread->get_current_edl()));

	x += edl_path_text->get_w();
	add_subwindow(edl_path_browse = new BrowseButton(
		mwindow,
		this,
		edl_path_text, 
		x, 
		y, 
		thread->get_current_edl(),
		_("Input EDL"),
		_("Select an EDL to load:"),
		0));

	x = x1;

	y += 30;
	add_subwindow(update_selected_edl = new BatchRenderUpdateEDL(thread,
		x,
		y));
	y += update_selected_edl->get_h() + mwindow->theme->widget_border;

	add_subwindow(new_batch = new BatchRenderNew(thread, 
		x, 
		y));
	x += new_batch->get_w() + 10;

	add_subwindow(delete_batch = new BatchRenderDelete(thread, 
		x, 
		y));
	x = new_batch->get_x();
	y += new_batch->get_h() + mwindow->theme->widget_border;
	add_subwindow(use_current_edl = new BatchRenderCurrentEDL(thread,
		x,
		y));
	if( !mwindow->edl || !mwindow->edl->path[0] ) use_current_edl->disable();

	x = x2;
	y = y2;
	add_subwindow(list_title = new BC_Title(x, y, _("Batches to render:")));
	y += 20;
	add_subwindow(batch_list = new BatchRenderList(thread, 
		x, 
		y,
		get_w() - x - 10,
		get_h() - y - BC_GenericButton::calculate_h() - 15));

	y += batch_list->get_h() + 10;
	add_subwindow(start_button = new BatchRenderStart(thread, 
	    x, 
	    y));
	x = get_w() / 2 -
		BC_GenericButton::calculate_w(this, _("Stop")) / 2;
	add_subwindow(stop_button = new BatchRenderStop(thread, 
		x, 
		y));
	x = get_w() - 
		BC_GenericButton::calculate_w(this, _("Close")) - 
		10;
	add_subwindow(cancel_button = new BatchRenderCancel(thread, 
		x, 
		y));

	show_window(1);
	unlock_window();
}

void BatchRenderGUI::button_disable()
{
	new_batch->disable();
	delete_batch->disable();
	use_current_edl->disable();
	update_selected_edl->disable();
}

void BatchRenderGUI::button_enable()
{
	new_batch->enable();
	delete_batch->enable();
	if( mwindow->edl && mwindow->edl->path[0] )
		use_current_edl->enable();
	update_selected_edl->enable();
}

int BatchRenderGUI::resize_event(int w, int h)
{
	mwindow->session->batchrender_w = w;
	mwindow->session->batchrender_h = h;
	mwindow->theme->get_batchrender_sizes(this, w, h);

	int x = mwindow->theme->batchrender_x1;
	int y = 5;
	int x1 = mwindow->theme->batchrender_x1;
	int x2 = mwindow->theme->batchrender_x2;
	//int x3 = mwindow->theme->batchrender_x3;
	int y1 = y;
	int y2;

	output_path_title->reposition_window(x1, y);
	y += 20;
	format_tools->reposition_window(x, y);
	x2 = x;
	y2 = y + 10;
	y = y1;
	x += format_tools->get_w();
	x1 = x;
	//x3 = x + 80;

	x = x1;
	edl_path_title->reposition_window(x, y);
	y += 20;
	edl_path_text->reposition_window(x, y, w - x - 40);
	x += edl_path_text->get_w();
	edl_path_browse->reposition_window(x, y);

 	x = x1;
// 	y += 30;
// 	status_title->reposition_window(x, y);
// 	x = x3;
// 	status_text->reposition_window(x, y);
// 	x = x1;
// 	y += 30;
// 	progress_bar->reposition_window(x, y, w - x - 10);

	y += 30;
	update_selected_edl->reposition_window(x, y);
	y += update_selected_edl->get_h() + mwindow->theme->widget_border;
	new_batch->reposition_window(x, y);
	x += new_batch->get_w() + 10;
	delete_batch->reposition_window(x, y);
	x = new_batch->get_x();
	y += new_batch->get_h() + mwindow->theme->widget_border;
	use_current_edl->reposition_window(x, y);

	x = x2;
	y = y2;
	int y_margin = get_h() - batch_list->get_h();
	list_title->reposition_window(x, y);
	y += 20;
	batch_list->reposition_window(x, y, w - x - 10, h - y_margin);

	y += batch_list->get_h() + 10;
	start_button->reposition_window(x, y);
	x = w / 2 - 
		stop_button->get_w() / 2;
	stop_button->reposition_window(x, y);
	x = w -
		cancel_button->get_w() - 
		10;
	cancel_button->reposition_window(x, y);
	return 1;
}

int BatchRenderGUI::translation_event()
{
	mwindow->session->batchrender_x = get_x();
	mwindow->session->batchrender_y = get_y();
	return 1;
}

int BatchRenderGUI::close_event()
{
// Stop batch rendering
	unlock_window();
	thread->stop_rendering();
	lock_window("BatchRenderGUI::close_event");
	set_done(1);
	return 1;
}

void BatchRenderGUI::create_list(int update_widget)
{
	for(int i = 0; i < BATCHRENDER_COLUMNS; i++)
	{
		list_columns[i].remove_all_objects();
	}

	for(int i = 0; i < thread->jobs.total; i++)
	{
		BatchRenderJob *job = thread->jobs.values[i];
		char string[BCTEXTLEN];
		BC_ListBoxItem *enabled = new BC_ListBoxItem(job->enabled ? 
			(char*)"X" : 
			(char*)" ");
		BC_ListBoxItem *item1 = new BC_ListBoxItem(job->asset->path);
		BC_ListBoxItem *item2 = new BC_ListBoxItem(job->edl_path);
		BC_ListBoxItem *item3;
		if(job->elapsed)
			item3 = new BC_ListBoxItem(
				Units::totext(string,
					job->elapsed,
					TIME_HMS2));
		else
			item3 = new BC_ListBoxItem(_("Unknown"));
		list_columns[0].append(enabled);
		list_columns[1].append(item1);
		list_columns[2].append(item2);
		list_columns[3].append(item3);
		if(i == thread->current_job)
		{
			enabled->set_selected(1);
			item1->set_selected(1);
			item2->set_selected(1);
			item3->set_selected(1);
		}
		if(i == thread->rendering_job)
		{
			enabled->set_color(RED);
			item1->set_color(RED);
			item2->set_color(RED);
			item3->set_color(RED);
		}
	}

	if(update_widget)
	{
		batch_list->update(list_columns,
						list_titles,
						thread->column_width,
						BATCHRENDER_COLUMNS,
						batch_list->get_xposition(),
						batch_list->get_yposition(), 
						batch_list->get_highlighted_item(),  // Flat index of item cursor is over
						1,     // set all autoplace flags to 1
						1);
	}
}

void BatchRenderGUI::change_job()
{
	BatchRenderJob *job = thread->get_current_job();
	format_tools->update(job->asset, &job->strategy);
	edl_path_text->update(job->edl_path);
}








BatchFormat::BatchFormat(MWindow *mwindow,
			BatchRenderGUI *gui,
			Asset *asset)
 : FormatTools(mwindow, gui, asset)
{
	this->gui = gui;
	this->mwindow = mwindow;
}

BatchFormat::~BatchFormat()
{
}


int BatchFormat::handle_event()
{
	gui->create_list(1);
	return 1;
}











BatchRenderEDLPath::BatchRenderEDLPath(BatchRenderThread *thread, 
	int x, 
	int y, 
	int w, 
	char *text)
 : BC_TextBox(x, 
		y, 
		w, 
		1,
		text)
{
	this->thread = thread;
}


int BatchRenderEDLPath::handle_event()
{
// Suggestions
	calculate_suggestions(thread->file_entries);

	strcpy(thread->get_current_edl(), get_text());
	thread->gui->create_list(1);
	return 1;
}






BatchRenderNew::BatchRenderNew(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, y, _("New"))
{
	this->thread = thread;
}

int BatchRenderNew::handle_event()
{
	thread->new_job();
	return 1;
}

BatchRenderDelete::BatchRenderDelete(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, y, _("Delete"))
{
	this->thread = thread;
}

int BatchRenderDelete::handle_event()
{
	thread->delete_job();
	return 1;
}






BatchRenderCurrentEDL::BatchRenderCurrentEDL(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, y, _("Use Current EDL"))
{
	this->thread = thread;
}

int BatchRenderCurrentEDL::handle_event()
{
	thread->use_current_edl();
	return 1;
}

BatchRenderUpdateEDL::BatchRenderUpdateEDL(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, y, _("Save to EDL Path"))
{
	this->thread = thread;
}

int BatchRenderUpdateEDL::handle_event()
{
	thread->update_selected_edl();
	return 1;
}




BatchRenderList::BatchRenderList(BatchRenderThread *thread, 
	int x, 
	int y,
	int w,
	int h)
 : BC_ListBox(x, 
 	y, 
	w, 
	h, 
	LISTBOX_TEXT,
	thread->gui->list_columns,
	list_titles,
	thread->column_width,
	BATCHRENDER_COLUMNS,
	0,
	0,
	LISTBOX_SINGLE,
	ICON_LEFT,
	1)
{
	this->thread = thread;
	dragging_item = 0;
	set_process_drag(0);
}

int BatchRenderList::handle_event()
{
	return 1;
}

int BatchRenderList::selection_changed()
{
	thread->current_job = get_selection_number(0, 0);
	thread->gui->change_job();
	if(get_cursor_x() < thread->column_width[0])
	{
		BatchRenderJob *job = thread->get_current_job();
		job->enabled = !job->enabled;
		thread->gui->create_list(1);
	}
	return 1;
}

int BatchRenderList::column_resize_event()
{
	for(int i = 0; i < BATCHRENDER_COLUMNS; i++)
	{
		thread->column_width[i] = get_column_width(i);
	}
	return 1;
}

int BatchRenderList::drag_start_event()
{
	if(BC_ListBox::drag_start_event())
	{
		dragging_item = 1;
		return 1;
	}

	return 0;
}

int BatchRenderList::drag_motion_event()
{
	if(BC_ListBox::drag_motion_event())
	{
		return 1;
	}
	return 0;
}

int BatchRenderList::drag_stop_event()
{
	if(dragging_item)
	{
		int src = get_selection_number(0, 0);
		int dst = get_highlighted_item();
		if(src != dst)
		{
			thread->move_batch(src, dst);
		}
		BC_ListBox::drag_stop_event();
	}
	return 0;
}













BatchRenderStart::BatchRenderStart(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, 
 	y, 
	_("Start"))
{
	this->thread = thread;
}

int BatchRenderStart::handle_event()
{
	thread->start_rendering();
	return 1;
}

BatchRenderStop::BatchRenderStop(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, 
 	y, 
	_("Stop"))
{
	this->thread = thread;
}

int BatchRenderStop::handle_event()
{
	unlock_window();
	thread->stop_rendering();
	lock_window("BatchRenderStop::handle_event");
	return 1;
}


BatchRenderCancel::BatchRenderCancel(BatchRenderThread *thread, 
	int x, 
	int y)
 : BC_GenericButton(x, 
 	y, 
	_("Close"))
{
	this->thread = thread;
}

int BatchRenderCancel::handle_event()
{
	unlock_window();
	thread->stop_rendering();
	lock_window("BatchRenderCancel::handle_event");
	thread->gui->set_done(1);
	return 1;
}

int BatchRenderCancel::keypress_event()
{
	if(get_keypress() == ESC) 
	{
		unlock_window();
		thread->stop_rendering();
		lock_window("BatchRenderCancel::keypress_event");
		thread->gui->set_done(1);
		return 1;
	}
	return 0;
}





// DVD Creation

const int64_t CreateDVD_Thread::DVD_SIZE = 4700000000;
const int CreateDVD_Thread::DVD_STREAMS = 1;
const int CreateDVD_Thread::DVD_WIDTH = 720;
const int CreateDVD_Thread::DVD_HEIGHT = 480;
const double CreateDVD_Thread::DVD_ASPECT_WIDTH = 4.;
const double CreateDVD_Thread::DVD_ASPECT_HEIGHT = 3.;
const double CreateDVD_Thread::DVD_WIDE_ASPECT_WIDTH = 16.;
const double CreateDVD_Thread::DVD_WIDE_ASPECT_HEIGHT = 9.;
const double CreateDVD_Thread::DVD_FRAMERATE = 30000. / 1001.;
const int CreateDVD_Thread::DVD_MAX_BITRATE = 8000000;
const int CreateDVD_Thread::DVD_CHANNELS = 2;
const int CreateDVD_Thread::DVD_WIDE_CHANNELS = 6;
const double CreateDVD_Thread::DVD_SAMPLERATE = 48000;
const double CreateDVD_Thread::DVD_KAUDIO_RATE = 224;


CreateDVD_MenuItem::CreateDVD_MenuItem(MWindow *mwindow)
 : BC_MenuItem(_("DVD Render..."), "Shift-D", 'D')
{
	set_shift(1); 
	this->mwindow = mwindow;
}

int CreateDVD_MenuItem::handle_event()
{
	mwindow->create_dvd->start();
	return 1;
}


CreateDVD_Thread::CreateDVD_Thread(MWindow *mwindow)
 : BC_DialogThread()
{
	this->mwindow = mwindow;
	this->gui = 0;
	this->use_deinterlace = 0;
	this->use_inverse_telecine = 0;
	this->use_scale = 0;
	this->use_resize_tracks = 0;
	this->use_histogram = 0;
	this->use_wide_audio = 0;
	this->use_wide_aspect = 0;
	this->use_label_chapters = 0;
}

CreateDVD_Thread::~CreateDVD_Thread()
{
}

int CreateDVD_Thread::create_dvd_jobs(ArrayList<BatchRenderJob*> *jobs,
	const char *tmp_path, const char *asset_title)
{
	EDL *edl = mwindow->edl;
	if( !edl || !edl->session ) {
		char msg[BCTEXTLEN];
                sprintf(msg, "No EDL/Session");
                MainError::show_error(msg);
                return 1;
        }
	EDLSession *session = edl->session;

	double total_length = edl->tracks->total_length();
        if( total_length <= 0 ) {
		char msg[BCTEXTLEN];
                sprintf(msg, "No content: %s", asset_title);
                MainError::show_error(msg);
                return 1;
        }

	char asset_dir[BCTEXTLEN];
	sprintf(asset_dir, "%s/%s", tmp_path, asset_title);

	if( mkdir(asset_dir, 0777) ) {
		char err[BCTEXTLEN], msg[BCTEXTLEN];
		strerror_r(errno, err, sizeof(err));
		sprintf(msg, "Unable to create directory: %s\n-- %s", asset_dir, err);
		MainError::show_error(msg);
		return 1;
	}

	double old_samplerate = session->sample_rate;
	double old_framerate = session->frame_rate;

        session->video_channels = DVD_STREAMS;
        session->video_tracks = DVD_STREAMS;
        session->frame_rate = DVD_FRAMERATE;
        session->output_w = DVD_WIDTH;
        session->output_h = DVD_HEIGHT;
        session->aspect_w = use_wide_aspect ? DVD_WIDE_ASPECT_WIDTH : DVD_ASPECT_WIDTH;
        session->aspect_h = use_wide_aspect ? DVD_WIDE_ASPECT_HEIGHT : DVD_ASPECT_HEIGHT;
        session->sample_rate = DVD_SAMPLERATE;
        session->audio_channels = session->audio_tracks =
		use_wide_audio ? DVD_WIDE_CHANNELS : DVD_CHANNELS;

	char script_filename[BCTEXTLEN];
	sprintf(script_filename, "%s/dvd.sh", asset_dir);
	int fd = open(script_filename, O_WRONLY+O_CREAT+O_TRUNC, 0755);
	FILE *fp = fdopen(fd, "w");
	if( !fp ) {
		char err[BCTEXTLEN], msg[BCTEXTLEN];
		strerror_r(errno, err, sizeof(err));
		sprintf(msg, "Unable to save: %s\n-- %s", script_filename, err);
		MainError::show_error(msg);
		return 1;
	}
	fprintf(fp,"#!/bin/bash\n");
	fprintf(fp,"echo \"running %s\" $# $*\n", script_filename);
	fprintf(fp,"\n");
	fprintf(fp,"mplex -f 8 -o $1/dvd.mpg $1/dvd.m2v $1/dvd.ac3\n");
	fprintf(fp,"\n");
	fprintf(fp,"rm -rf $1/iso\n");
	fprintf(fp,"mkdir -p $1/iso\n");
	fprintf(fp,"\n");
	fprintf(fp,"dvdauthor -x - <<eof\n");
	fprintf(fp,"<dvdauthor dest=\"$1/iso\">\n");
	fprintf(fp,"  <vmgm>\n");
	fprintf(fp,"    <fpc> jump title 1; </fpc>\n");
	fprintf(fp,"  </vmgm>\n");
	fprintf(fp,"  <titleset>\n");
	fprintf(fp,"    <titles>\n");
	fprintf(fp,"    <video format=\"ntsc\" aspect=\"%d:%d\" resolution=\"%dx%d\"/>\n",
		(int)session->aspect_w, (int)session->aspect_h,
		session->output_w, session->output_h);
	fprintf(fp,"    <audio format=\"ac3\" lang=\"en\"/>\n");
	fprintf(fp,"    <pgc>\n");
	fprintf(fp,"      <vob file=\"$1/dvd.mpg\" chapters=\"");
	if( use_label_chapters && edl->labels ) {
		Label *label = edl->labels->first;
		while( label ) {
			int secs = label->position;
			int mins = secs / 60;
			int frms = (label->position-secs) * session->frame_rate;
			fprintf(fp,"%d:%02d:%02d.%d", mins/60, mins%60, secs%60, frms);
			if( (label=label->next) != 0 ) fprintf(fp, ",");
		}
	}
	else {
		int mins = 0;
		for( int secs=0 ; secs<total_length; secs+=10*60 ) {
			mins = secs / 60;
			fprintf(fp,"%d:%02d:00,", mins/60, mins%60);
		}
		fprintf(fp,"%d:%02d:00", mins/60, mins%60);
	}
	fprintf(fp,"\"/>\n");
	fprintf(fp,"    </pgc>\n");
	fprintf(fp,"    </titles>\n");
	fprintf(fp,"  </titleset>\n");
	fprintf(fp,"</dvdauthor>\n");
	fprintf(fp,"eof\n");
	fprintf(fp,"\n");
	fprintf(fp,"echo To burn dvd, load blank media and run:\n");
	fprintf(fp,"echo growisofs -dvd-compat -Z /dev/dvd -dvd-video $1/iso\n");
	fprintf(fp,"\n");
	fclose(fp);

	if( use_wide_audio ) {
        	session->audio_channels = session->audio_tracks = DVD_WIDE_CHANNELS;
		session->achannel_positions[0] = 90;
		session->achannel_positions[1] = 150;
		session->achannel_positions[2] = 30;
		session->achannel_positions[3] = 210;
		session->achannel_positions[4] = 330;
		session->achannel_positions[5] = 270;
		if( edl->tracks->recordable_audio_tracks() == DVD_WIDE_CHANNELS )
			mwindow->remap_audio(MWindow::AUDIO_1_TO_1);
	}
	else {
        	session->audio_channels = session->audio_tracks = DVD_CHANNELS;
		session->achannel_positions[0] = 180;
		session->achannel_positions[1] = 0;
		if( edl->tracks->recordable_audio_tracks() == DVD_WIDE_CHANNELS )
			mwindow->remap_audio(MWindow::AUDIO_5_1_TO_2);
	}

	double new_samplerate = session->sample_rate;
	double new_framerate = session->frame_rate;
	edl->rechannel();
	edl->resample(old_samplerate, new_samplerate, TRACK_AUDIO);
	edl->resample(old_framerate, new_framerate, TRACK_VIDEO);

	int64_t aud_size = ((DVD_KAUDIO_RATE * total_length)/8 + 1000-1) * 1000;
	int64_t vid_size = DVD_SIZE*0.96 - aud_size;
	int vid_bitrate = (vid_size * 8) / total_length;
	vid_bitrate /= 1000;  vid_bitrate *= 1000;
	if( vid_bitrate > DVD_MAX_BITRATE ) vid_bitrate = DVD_MAX_BITRATE;

	char xml_filename[BCTEXTLEN];
	sprintf(xml_filename, "%s/dvd.xml", asset_dir);
        FileXML xml_file;
        edl->save_xml(&xml_file, xml_filename, 0, 0);
        xml_file.terminate_string();
        if( xml_file.write_to_file(xml_filename) ) {
		char msg[BCTEXTLEN];
		sprintf(msg, "Unable to save: %s", xml_filename);
		MainError::show_error(msg);
		return 1;
	}

	BatchRenderJob *job = new BatchRenderJob(mwindow->preferences);
	jobs->append(job);
	strcpy(&job->edl_path[0], xml_filename);
	Asset *asset = job->asset;

	sprintf(&asset->path[0],"%s/dvd.m2v", asset_dir);
        asset->video_data = 1;
	asset->format = FILE_VMPEG;
	asset->layers = DVD_STREAMS;
	asset->frame_rate = session->frame_rate;
	asset->width = session->output_w;
	asset->height = session->output_h;
	asset->aspect_ratio = session->aspect_w / session->aspect_h;
	asset->vmpeg_cmodel = BC_YUV420P;
	asset->vmpeg_fix_bitrate = 1;
	asset->vmpeg_bitrate = vid_bitrate;
	asset->vmpeg_quantization = 15;
        asset->vmpeg_iframe_distance = 15;
	asset->vmpeg_progressive = 0;
	asset->vmpeg_denoise = 0;
	asset->vmpeg_seq_codes = 0;
	asset->vmpeg_derivative = 2;
	asset->vmpeg_preset = 8;
	asset->vmpeg_field_order = 0;
	asset->vmpeg_pframe_distance = 0;

	job = new BatchRenderJob(mwindow->preferences);
	jobs->append(job);
	strcpy(&job->edl_path[0], xml_filename);
	asset = job->asset;

	sprintf(&asset->path[0],"%s/dvd.ac3", asset_dir);
        asset->audio_data = 1;
	asset->format = FILE_AC3;
	asset->channels = session->audio_channels;
	asset->sample_rate = session->sample_rate;
	asset->bits = 16;
	asset->byte_order = 0;
	asset->signed_ = 1;
	asset->header = 0;
	asset->dither = 0;
	asset->ac3_bitrate = DVD_KAUDIO_RATE;

	job = new BatchRenderJob(mwindow->preferences);
	jobs->append(job);
	job->edl_path[0] = '@';
	strcpy(&job->edl_path[1], script_filename);
	strcpy(&job->asset->path[0], asset_dir);

	return 0;
}

void CreateDVD_Thread::handle_close_event(int result)
{
	if( result ) return;
	mwindow->batch_render->load_defaults(mwindow->defaults);
        mwindow->undo->update_undo_before();
	KeyFrame keyframe;  char data[BCTEXTLEN];
	if( use_deinterlace ) {
		sprintf(data,"<DEINTERLACE MODE=1>");
		keyframe.set_data(data);
		insert_video_plugin("Deinterlace", &keyframe);
	}
	if( use_inverse_telecine ) {
		sprintf(data,"<IVTC FRAME_OFFSET=0 FIRST_FIELD=0 "
			"AUTOMATIC=1 AUTO_THRESHOLD=2.0e+00 PATTERN=2>");
		keyframe.set_data(data);
		insert_video_plugin("Inverse Telecine", &keyframe);
	}
	if( use_scale ) {
		sprintf(data,"<SCALE TYPE=1 X_FACTOR=1 Y_FACTOR=1 "
			"WIDTH=%d HEIGHT=%d CONSTRAIN=0>", DVD_WIDTH, DVD_HEIGHT);
		keyframe.set_data(data);
		insert_video_plugin("Scale", &keyframe);
	}
	if( use_resize_tracks )
		resize_tracks();
	if( use_histogram ) {
#if 0
		sprintf(data, "<HISTOGRAM OUTPUT_MIN_0=0 OUTPUT_MAX_0=1 "
			"OUTPUT_MIN_1=0 OUTPUT_MAX_1=1 "
			"OUTPUT_MIN_2=0 OUTPUT_MAX_2=1 "
			"OUTPUT_MIN_3=0 OUTPUT_MAX_3=1 "
			"AUTOMATIC=0 THRESHOLD=9.0-01 PLOT=0 SPLIT=0>"
			"<POINTS></POINTS><POINTS></POINTS><POINTS></POINTS>"
			"<POINTS><POINT X=6.0e-02 Y=0>"
				"<POINT X=9.4e-01 Y=1></POINTS>");
#else
		sprintf(data, "<HISTOGRAM AUTOMATIC=0 THRESHOLD=1.0e-01 "
			"PLOT=0 SPLIT=0 W=440 H=500 PARADE=0 MODE=3 "
			"LOW_OUTPUT_0=0 HIGH_OUTPUT_0=1 LOW_INPUT_0=0 HIGH_INPUT_0=1 GAMMA_0=1 "
			"LOW_OUTPUT_1=0 HIGH_OUTPUT_1=1 LOW_INPUT_1=0 HIGH_INPUT_1=1 GAMMA_1=1 "
			"LOW_OUTPUT_2=0 HIGH_OUTPUT_2=1 LOW_INPUT_2=0 HIGH_INPUT_2=1 GAMMA_2=1 "
			"LOW_OUTPUT_3=0 HIGH_OUTPUT_3=1 LOW_INPUT_3=0.06 HIGH_INPUT_3=0.94 "
			"GAMMA_3=1>");
#endif
		keyframe.set_data(data);
		insert_video_plugin("Histogram", &keyframe);
	}
	create_dvd_jobs(&mwindow->batch_render->jobs, tmp_path, asset_title);
	mwindow->save_backup();
	mwindow->undo->update_undo_after(_("create dvd"), LOAD_ALL);
	mwindow->resync_guis();
	mwindow->batch_render->handle_close_event(0);
	mwindow->batch_render->start();
}

BC_Window* CreateDVD_Thread::new_gui()
{
	memset(tmp_path,0,sizeof(tmp_path));
	strcpy(tmp_path,"/tmp");
	memset(asset_title,0,sizeof(asset_title));
	time_t dt;      time(&dt);
	struct tm dtm;  localtime_r(&dt, &dtm);
	sprintf(asset_title, "dvd_%02d%02d%02d-%02d%02d%02d",
		dtm.tm_year+1900, dtm.tm_mon+1, dtm.tm_mday,
		dtm.tm_hour, dtm.tm_min, dtm.tm_sec);
	use_deinterlace = 0;
	use_inverse_telecine = 0;
	use_scale = 0;
	use_resize_tracks = 0;
	use_histogram = 0;
	use_wide_audio = 0;
	use_wide_aspect = 0;
	use_label_chapters = 0;
	option_presets();
        int scr_x = mwindow->gui->get_screen_x(0, -1);
        int scr_w = mwindow->gui->get_screen_w(0, -1);
        int scr_h = mwindow->gui->get_screen_h(0, -1);
        int w = 500, h = 250;
	int x = scr_x + scr_w/2 - w/2, y = scr_h/2 - h/2;

	gui = new CreateDVD_GUI(this, x, y, w, h);
	gui->create_objects();
	return gui;
}


CreateDVD_OK::CreateDVD_OK(CreateDVD_GUI *gui, int x, int y)
 : BC_OKButton(x, y)
{
        this->gui = gui;
        set_tooltip("end setup, start batch render");
}

CreateDVD_OK::~CreateDVD_OK()
{
}

int CreateDVD_OK::button_press_event()
{
        if(get_buttonpress() == 1 && is_event_win() && cursor_inside()) {
                gui->set_done(0);
                return 1;
        }
        return 0;
}

int CreateDVD_OK::keypress_event()
{
        return 0;
}


CreateDVD_Cancel::CreateDVD_Cancel(CreateDVD_GUI *gui, int x, int y)
 : BC_CancelButton(x, y)
{
        this->gui = gui;
}

CreateDVD_Cancel::~CreateDVD_Cancel()
{
}

int CreateDVD_Cancel::button_press_event()
{
        if(get_buttonpress() == 1 && is_event_win() && cursor_inside()) {
                gui->set_done(1);
                return 1;
        }
        return 0;
}


CreateDVD_DiskSpace::CreateDVD_DiskSpace(CreateDVD_GUI *gui, int x, int y)
 : BC_Title(x, y, "", MEDIUMFONT, GREEN)
{
        this->gui = gui;
}

CreateDVD_DiskSpace::~CreateDVD_DiskSpace()
{
}

int64_t CreateDVD_DiskSpace::tmp_path_space()
{
	const char *path = gui->tmp_path->get_text();
	if( access(path,R_OK+W_OK) ) return 0;
	struct statfs sfs;
	if( statfs(path, &sfs) ) return 0;
	return (int64_t)sfs.f_bsize * sfs.f_bfree;
}

void CreateDVD_DiskSpace::update()
{
//	gui->disk_space->set_color(get_bg_color());
	int64_t disk_space = tmp_path_space();
	int color = disk_space<gui->needed_disk_space ? RED : GREEN;
	static const char *suffix[] = { "", "KB", "MB", "GB", "TB", "PB" };
	int i = 0;
	for( int64_t space=disk_space; i<5 && (space/=1000)>0; disk_space=space, ++i );
	char text[BCTEXTLEN];
	sprintf(text, "disk space: " _LDv(3) "%s", disk_space, suffix[i]);
	gui->disk_space->BC_Title::update(text);
	gui->disk_space->set_color(color);
}

CreateDVD_TmpPath::CreateDVD_TmpPath(CreateDVD_GUI *gui, int x, int y, int w)
 : BC_TextBox(x, y, w, 1, -sizeof(gui->thread->tmp_path),
		gui->thread->tmp_path, 1, MEDIUMFONT)
{
        this->gui = gui;
}

CreateDVD_TmpPath::~CreateDVD_TmpPath()
{
}

int CreateDVD_TmpPath::handle_event()
{
	gui->disk_space->update();
        return 1;
}


CreateDVD_AssetTitle::CreateDVD_AssetTitle(CreateDVD_GUI *gui, int x, int y, int w)
 : BC_TextBox(x, y, w, 1, 0, gui->thread->asset_title, 1, MEDIUMFONT)
{
        this->gui = gui;
}

CreateDVD_AssetTitle::~CreateDVD_AssetTitle()
{
}


CreateDVD_Deinterlace::CreateDVD_Deinterlace(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_deinterlace, "Deinterlace")
{
	this->gui = gui;
}

CreateDVD_Deinterlace::~CreateDVD_Deinterlace()
{
}

int CreateDVD_Deinterlace::handle_event()
{
	if( get_value() ) {
		gui->need_inverse_telecine->set_value(0);
		gui->thread->use_inverse_telecine = 0;
	}
	return BC_CheckBox::handle_event();
}


CreateDVD_InverseTelecine::CreateDVD_InverseTelecine(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_inverse_telecine, "Inverse Telecine")
{
	this->gui = gui;
}

CreateDVD_InverseTelecine::~CreateDVD_InverseTelecine()
{
}

int CreateDVD_InverseTelecine::handle_event()
{
	if( get_value() ) {
		gui->need_deinterlace->set_value(0);
		gui->thread->use_deinterlace = 0;
	}
	return BC_CheckBox::handle_event();
}


CreateDVD_Scale::CreateDVD_Scale(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_scale, "Scale")
{
	this->gui = gui;
}

CreateDVD_Scale::~CreateDVD_Scale()
{
}


CreateDVD_ResizeTracks::CreateDVD_ResizeTracks(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_resize_tracks, "Resize Tracks")
{
	this->gui = gui;
}

CreateDVD_ResizeTracks::~CreateDVD_ResizeTracks()
{
}


CreateDVD_Histogram::CreateDVD_Histogram(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_histogram, "Histogram")
{
	this->gui = gui;
}

CreateDVD_Histogram::~CreateDVD_Histogram()
{
}

CreateDVD_LabelChapters::CreateDVD_LabelChapters(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_label_chapters, "Chapters at Labels")
{
	this->gui = gui;
}

CreateDVD_LabelChapters::~CreateDVD_LabelChapters()
{
}

CreateDVD_WideAudio::CreateDVD_WideAudio(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_wide_audio, "Audio 5.1")
{
	this->gui = gui;
}

CreateDVD_WideAudio::~CreateDVD_WideAudio()
{
}

CreateDVD_WideAspect::CreateDVD_WideAspect(CreateDVD_GUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->thread->use_wide_aspect, "Aspect 16x9")
{
	this->gui = gui;
}

CreateDVD_WideAspect::~CreateDVD_WideAspect()
{
}




CreateDVD_GUI::CreateDVD_GUI(CreateDVD_Thread *thread, int x, int y, int w, int h)
 : BC_Window(PROGRAM_NAME ": Create DVD", x, y, w, h, 50, 50, 1, 0, 1)
{
	this->thread = thread;
	at_x = at_y = tmp_x = tmp_y = 0;
	ok_x = ok_y = ok_w = ok_h = 0;
	cancel_x = cancel_y = cancel_w = cancel_h = 0;
	asset_title = 0;
	tmp_path = 0;
	disk_space = 0;
	needed_disk_space = 15e9;
	need_deinterlace = 0;
	need_inverse_telecine = 0;
	need_scale = 0;
	need_resize_tracks = 0;
	need_histogram = 0;
	need_wide_audio = 0;
	need_wide_aspect = 0;
	need_label_chapters = 0;
	ok = 0;
	cancel = 0;
}

CreateDVD_GUI::~CreateDVD_GUI()
{
}

void CreateDVD_GUI::create_objects()
{
	lock_window("CreateDVD_GUI::create_objects");
	int pady = BC_TextBox::calculate_h(this, MEDIUMFONT, 0, 1) + 5;
	int padx = BC_Title::calculate_w(this, (char*)"X", MEDIUMFONT);
	int x = padx/2, y = pady/2;
	BC_Title *title = new BC_Title(x, y, "Title:", MEDIUMFONT, YELLOW);
	add_subwindow(title);
	at_x = x + title->get_w();  at_y = y;
	asset_title = new CreateDVD_AssetTitle(this, at_x, at_y, get_w()-at_x-10);
	add_subwindow(asset_title);
	y += title->get_h() + pady/2;
	title = new BC_Title(x, y, "tmp path:", MEDIUMFONT, YELLOW);
	add_subwindow(title);
	tmp_x = x + title->get_w();  tmp_y = y;
	tmp_path = new CreateDVD_TmpPath(this, tmp_x, tmp_y,  get_w()-tmp_x-10);
	add_subwindow(tmp_path);
	y += title->get_h() + pady/2;
	disk_space = new CreateDVD_DiskSpace(this, x, y);
	add_subwindow(disk_space);
	disk_space->update();
	y += disk_space->get_h() + pady/2;
	need_deinterlace = new CreateDVD_Deinterlace(this, x, y);
	add_subwindow(need_deinterlace);
	int x1 = x + 150, x2 = x1 + 150;
	need_inverse_telecine = new CreateDVD_InverseTelecine(this, x1, y);
	add_subwindow(need_inverse_telecine);
	y += need_deinterlace->get_h() + pady/2;
	need_scale = new CreateDVD_Scale(this, x, y);
	add_subwindow(need_scale);
	need_wide_audio = new CreateDVD_WideAudio(this, x1, y);
	add_subwindow(need_wide_audio);
	need_resize_tracks = new CreateDVD_ResizeTracks(this, x2, y);
	add_subwindow(need_resize_tracks);
	y += need_scale->get_h() + pady/2;
	need_histogram = new CreateDVD_Histogram(this, x, y);
	add_subwindow(need_histogram);
	need_wide_aspect = new CreateDVD_WideAspect(this, x1, y);
	add_subwindow(need_wide_aspect);
	need_label_chapters = new CreateDVD_LabelChapters(this, x2, y);
	add_subwindow(need_label_chapters);
	ok_w = BC_OKButton::calculate_w();
	ok_h = BC_OKButton::calculate_h();
	ok_x = 10;
	ok_y = get_h() - ok_h - 10;
	ok = new CreateDVD_OK(this, ok_x, ok_y);
	add_subwindow(ok);
	cancel_w = BC_CancelButton::calculate_w();
	cancel_h = BC_CancelButton::calculate_h();
	cancel_x = get_w() - cancel_w - 10,
	cancel_y = get_h() - cancel_h - 10;
	cancel = new CreateDVD_Cancel(this, cancel_x, cancel_y);
	add_subwindow(cancel);
	show_window();
	unlock_window();
}

int CreateDVD_GUI::resize_event(int w, int h)
{
	asset_title->reposition_window(at_x, at_y, get_w()-at_x-10);
	tmp_path->reposition_window(tmp_x, tmp_y,  get_w()-tmp_x-10);
        ok_y = h - ok_h - 10;
        ok->reposition_window(ok_x, ok_y);
	cancel_x = w - cancel_w - 10,
	cancel_y = h - cancel_h - 10;
        cancel->reposition_window(cancel_x, cancel_y);
	return 0;
}

int CreateDVD_GUI::translation_event()
{
	return 1;
}

int CreateDVD_GUI::close_event()
{
        set_done(1);
        return 1;
}

int CreateDVD_Thread::
insert_video_plugin(const char *title, KeyFrame *default_keyframe)
{
	Tracks *tracks = mwindow->edl->tracks;
	for( Track *vtrk=tracks->first; vtrk; vtrk=vtrk->next ) {
		if( vtrk->data_type != TRACK_VIDEO ) continue;
		if( !vtrk->record ) continue;
		vtrk->expand_view = 1;
		PluginSet *plugin_set = new PluginSet(mwindow->edl, vtrk);
		vtrk->plugin_set.append(plugin_set);
		Edits *edits = vtrk->edits;
		for( Edit *edit=edits->first; edit; edit=edit->next ) {
			plugin_set->insert_plugin(title,
				edit->startproject, edit->length,
				PLUGIN_STANDALONE, 0, default_keyframe, 0);
		}
		vtrk->optimize();
	}
        return 0;
}

int CreateDVD_Thread::
resize_tracks()
{
        Tracks *tracks = mwindow->edl->tracks;
	int max_w = 0, max_h = 0;
        for( Track *vtrk=tracks->first; vtrk; vtrk=vtrk->next ) {
		if( vtrk->data_type != TRACK_VIDEO ) continue;
		if( !vtrk->record ) continue;
		Edits *edits = vtrk->edits;
		for( Edit *edit=edits->first; edit; edit=edit->next ) {
			Indexable *indexable = edit->get_source();
			int w = indexable->get_w();
			if( w > max_w ) max_w = w;
			int h = indexable->get_h();
			if( h > max_h ) max_h = h;
		}
        }
        for( Track *vtrk=tracks->first; vtrk; vtrk=vtrk->next ) {
                if( vtrk->data_type != TRACK_VIDEO ) continue;
                if( !vtrk->record ) continue;
		vtrk->track_w = max_w;
		vtrk->track_h = max_h;
	}
        return 0;
}

int CreateDVD_Thread::
option_presets()
{
	if( !mwindow->edl ) return 1;
        Tracks *tracks = mwindow->edl->tracks;
	int max_w = 0, max_h = 0;
	int has_deinterlace = 0, has_scale = 0;
        for( Track *trk=tracks->first; trk; trk=trk->next ) {
		if( !trk->record ) continue;
		Edits *edits = trk->edits;
		switch( trk->data_type ) {
		case TRACK_VIDEO:
			for( Edit *edit=edits->first; edit; edit=edit->next ) {
				Indexable *indexable = edit->get_source();
				int w = indexable->get_w();
				if( w > max_w ) max_w = w;
				if( w != DVD_WIDTH ) use_scale = 1;
				int h = indexable->get_h();
				if( h > max_h ) max_h = h;
				if( h != DVD_HEIGHT ) use_scale = 1;
			}
			for( int i=0; i<trk->plugin_set.size(); ++i ) {
				for(Plugin *plugin = (Plugin*)trk->plugin_set[i]->first;
                                                plugin;
                                                plugin = (Plugin*)plugin->next) {
					if( !strcmp(plugin->title, "Deinterlace") )
						has_deinterlace = 1;
					if( !strcmp(plugin->title, "Scale") )
						has_scale = 1;
				}
			}
			break;
		}
        }
	if( has_scale ) use_scale = 0;
        for( Track *trk=tracks->first; trk && !use_resize_tracks; trk=trk->next ) {
		if( !trk->record ) continue;
		switch( trk->data_type ) {
		case TRACK_VIDEO:
			if( trk->track_w != max_w ) use_resize_tracks = 1;
			if( trk->track_h != max_h ) use_resize_tracks = 1;
			break;
		}
        }
	if( !has_deinterlace && max_h > 2*DVD_HEIGHT ) use_deinterlace = 1;
	Labels *labels = mwindow->edl->labels;
	use_label_chapters = labels && labels->first ? 1 : 0;
	float w, h;
	MWindow::create_aspect_ratio(w, h, max_w, max_h);
	if( w == DVD_WIDE_ASPECT_WIDTH && h == DVD_WIDE_ASPECT_HEIGHT )
		use_wide_aspect = 1;
	if( tracks->recordable_audio_tracks() == DVD_WIDE_CHANNELS )
		use_wide_audio = 1;
	return 0;
}

