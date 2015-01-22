

#include "bcmenuitem.h"
#include "bctimer.h"
#include "commercials.h"
#include "dbwindow.h"
#include "keys.h"
#include "mediadb.h"
#include "mwindow.h"
#include "mwindowgui.h"
#include "mutex.h"
#include "theme.h"

#include <string.h>
#include <strings.h>

static inline int min(int a,int b) { return a<b ? a : b; }
static inline int max(int a,int b) { return a>b ? a : b; }


DbWindow::
DbWindow(MWindow *mwindow)
 : Thread(1, 0, 0)
{
	this->mwindow = mwindow;
	window_lock = new Mutex("DbWindow::window_lock");
	db_lock = new Mutex("DbWindow::db_lock");
	mdb = new MDb(this);
	gui = 0;
}

DbWindow::~DbWindow()
{
	stop();
	delete gui;
	mdb->remove_user();
	delete window_lock;
	delete db_lock;
}

void DbWindow::
start()
{
	window_lock->lock("DbWindow::start1");
	if( Thread::running() ) {
		gui->lock_window("DbWindow::start2");
		gui->raise_window();
		gui->unlock_window();
	}
	else {
		delete gui;
		gui = new DbWindowGUI(this);
		Thread::start();
	}
	window_lock->unlock();
}


void DbWindow::
stop()
{
	if( Thread::running() ) {
		window_lock->lock("DbWindow::stop");
		if( gui ) gui->set_done(1);
		window_lock->unlock();
		Thread::cancel();
		Thread::join();
	}
}

void DbWindow::
run()
{
	gui->lock_window("DbWindow::run");
	gui->create_objects();
	gui->unlock_window();
	gui->run_window();
	window_lock->lock("DbWindow::stop");
	delete gui;  gui = 0;
	window_lock->unlock();
}


DbWindowTitleText::
DbWindowTitleText(DbWindowGUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->title_text_enable, "titles")
{
	this->gui = gui;
}

DbWindowTitleText::~DbWindowTitleText()
{
}

int DbWindowTitleText::
handle_event()
{
	gui->title_text_enable = get_value();
	if( !gui->title_text_enable ) gui->info_text->update(1);
	return 1;
}


DbWindowInfoText::
DbWindowInfoText(DbWindowGUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->info_text_enable, "info")
{
	this->gui = gui;
}

DbWindowInfoText::~DbWindowInfoText()
{
}

int DbWindowInfoText::
handle_event()
{
	gui->info_text_enable = get_value();
	if( !gui->info_text_enable ) gui->title_text->update(1);
	return 1;
}


DbWindowMatchCase::
DbWindowMatchCase(DbWindowGUI *gui, int x, int y)
 : BC_CheckBox(x, y, &gui->match_case_enable, "match case")
{
	this->gui = gui;
}

DbWindowMatchCase::~DbWindowMatchCase()
{
}

int DbWindowMatchCase::
handle_event()
{
	gui->match_case_enable = get_value();
	return 1;
}


DbWindowText::
DbWindowText(DbWindowGUI *gui, int x, int y, int w)
 : BC_TextBox(x, y, w, 1, "") 
{
	this->gui = gui;
}

DbWindowText::~DbWindowText()
{
}


int DbWindowText::
handle_event()
{
	return 1;
}

int DbWindowText::
keypress_event()
{
	switch(get_keypress())
	{
	case RETURN:
		gui->search();
		return 1;
	}

	return BC_TextBox::keypress_event();
}


DbWindowScan::
DbWindowScan(MWindow *mwindow)
 : BC_MenuItem("Media DB", "Shift-M", 'M')
{
	set_shift();
	this->mwindow = mwindow;
}

DbWindowScan::~DbWindowScan()
{
}

int DbWindowScan::
handle_event()
{
	mwindow->gui->db_window->start();
	return 1;
}

DbWindowStart::
DbWindowStart(DbWindowGUI *gui, int x, int y)
 : BC_GenericButton(x, y, "Search")
{
	this->gui = gui;
}

DbWindowStart::~DbWindowStart()
{
}

int DbWindowStart::
handle_event()
{
	gui->search();
	return 1;
}

DbWindowDeleteItems::
DbWindowDeleteItems(DbWindowGUI *gui, int x, int y)
 : BC_GenericButton(x, y, "Delete")
{
	this->gui = gui;
}

DbWindowDeleteItems::~DbWindowDeleteItems()
{
}

int DbWindowDeleteItems::
handle_event()
{
	gui->search_list->stop_view_popup();
	gui->delete_items();
	return 1;
}


DbWindowCancel::
DbWindowCancel(DbWindowGUI *gui, int x, int y)
 : BC_CancelButton(x, y)
{
	this->gui = gui;
}

DbWindowCancel::~DbWindowCancel()
{
}

int DbWindowCancel::
handle_event()
{
	gui->set_done(1);
	return 1;
}


DbWindowList::
DbWindowList(DbWindowGUI *gui, int x, int y, int w, int h)
 : BC_ListBox(x, y, w, h, LISTBOX_TEXT, &gui->search_items[0],
	&gui->search_column_titles[0], &gui->search_column_widths[0],
	sizeof_col, 0, 0, LISTBOX_MULTIPLE)
{
	this->gui = gui;
	this->thread = gui->ticon_thread;
	this->view_popup = 0;
	this->view_idx = -1;
	view_ticon = new DbWindowTIcon(gui, &DbWindowTIcon::draw_popup);
	set_sort_column(gui->sort_column);
	set_sort_order(gui->sort_order);
	set_allow_drag_column(1);
}

DbWindowList::~DbWindowList()
{
	thread->stop_drawing();
	delete view_ticon;
	delete view_popup;
}

int DbWindowList::
handle_event()
{
	return 1;
}

int DbWindowList::
stop_view_popup()
{
	if( !view_popup ) return 0;
	BC_Popup *vp = view_popup;
	view_popup = 0;
	delete vp;
	view_idx = -1;
	return 1;
}

int DbWindowList::
keypress_event()
{
	switch(get_keypress()) {
	case ESC:
		if( stop_view_popup() ) return 1;
		break;
	case DELETE:
		gui->del_items->handle_event();
		break;
	}
	return 0;
}

int DbWindowList::
sort_order_event()
{
	gui->stop_drawing(1);
	gui->sort_events(get_sort_column(), get_sort_order());
	gui->start_drawing(1);
	return 1;
}

int DbWindowList::
move_column_event()
{
	gui->stop_drawing(1);
	gui->move_column(get_from_column(), get_to_column());
	gui->start_drawing(1);
	return 1;
}

int DbWindowList::update_images()
{
	thread->t_heap.remove_all();
	if( view_popup ) {
		view_ticon->age = 0;
		thread->t_heap.append(view_ticon);
	}

	int i = get_first_visible();
	int k = sizeof_col;
	while( --k >= 0 && gui->search_columns[k] != col_ticon );
	if( k >= 0 && i >= 0 ) {
		int sz = gui->search_results.size();
		int last = get_last_visible();
		if( last < sz ) sz = last+1;
		for( int n=0; i<sz; ++i, ++n ) {
			BC_ListBoxItem *item = gui->search_items[k][i];
			int x = item->get_text_x() - get_xposition() + 7;
			int y = item->get_text_y() + get_title_h() - get_yposition() + 5;
			DbWindowTIcon *ticon = thread->get_ticon(n);
			ticon->update_image(gui->search_results[i]->id, x, y);
		}
	}
	return 0;
}

int DbWindowList::draw_images()
{
	DbWindowTIconThread *thread = gui->ticon_thread;
	thread->image_update = 1;
	thread->start_drawing();
	return 0;
}

int DbWindowList::
update()
{
	BC_ListBox::update(gui->search_items, &gui->search_column_titles[0],
			&gui->search_column_widths[0], sizeof_col);
	return 0;
}



void DbWindowGUI::
create_objects()
{
	int pady = BC_TextBox::calculate_h(this, MEDIUMFONT, 0, 1) + 5;
	int padx = BC_Title::calculate_w(this, (char*)"X", MEDIUMFONT);
	int x = padx/2, y = pady/4;
	text_x = x;  text_y = y;
	BC_Title *title = new BC_Title(text_x, text_y, "Text:", MEDIUMFONT, YELLOW);
	add_subwindow(title);  x += title->get_w();
	search_text = new DbWindowText(this, x, y, get_w()-x-10);
	add_subwindow(search_text);
	text = search_text->get_text();
	x = padx;  y += pady + 5;
	title_text = new DbWindowTitleText(this, x, y);
	add_subwindow(title_text);  x += title_text->get_w() + padx;
	info_text = new DbWindowInfoText(this, x, y);
	add_subwindow(info_text);  x += title_text->get_w() + padx;
	match_case = new DbWindowMatchCase(this, x, y);
	add_subwindow(match_case); x += match_case->get_w() + padx;
	x = padx;  y += pady + 5;
	search_x = 10;
	search_y = get_h() - DbWindowStart::calculate_h() - 10;
	search_start = new DbWindowStart(this, search_x, search_y);
	add_subwindow(search_start);
	del_items_x = search_x + DbWindowStart::calculate_w(this, search_start->get_text()) + 15;
	del_items_y = search_y;
	del_items = new DbWindowDeleteItems(this, del_items_x, del_items_y);
	add_subwindow(del_items);
	cancel_w = DbWindowCancel::calculate_w();
	cancel_h = DbWindowCancel::calculate_h();
	cancel_x = get_w() - cancel_w - 10;
	cancel_y = get_h() - cancel_h - 10;
	cancel = new DbWindowCancel(this, cancel_x, cancel_y);
	add_subwindow(cancel);
	ticon_thread = new DbWindowTIconThread(this);
	list_x = x;  list_y = y;
	int list_w = get_w()-10 - list_x;
	int list_h = min(search_y, cancel_y)-10 - list_y;
	search_list = new DbWindowList(this, list_x, list_y, list_w, list_h);
	add_subwindow(search_list);
	canvas_x = 0;
	canvas_y = search_list->get_title_h();
	canvas_w = search_list->get_w();
	canvas_h = search_list->get_h() - canvas_y;
	canvas = new DbWindowCanvas(this, canvas_x, canvas_y, canvas_w, canvas_h);
	canvas->use_auxwindow(search_list);
	canvas->create_objects(0);
	set_icon(dwindow->mwindow->theme->get_image("record_icon"));
}


DbWindowGUI::
DbWindowGUI(DbWindow *dwindow)
 : BC_Window(PROGRAM_NAME ": DbWindow",
	dwindow->mwindow->gui->get_abs_cursor_x(1) - 900 / 2,
	dwindow->mwindow->gui->get_abs_cursor_y(1) - 600 / 2,
	900, 600, 400, 400)
{
	this->dwindow = dwindow;

	search_start = 0;
	search_list = 0;
	cancel = 0;

	search_x = search_y = 0;
	cancel_x = cancel_y = cancel_w = cancel_h = 0;
	del_items_x = del_items_y = 0;
	list_x = list_y = list_w = list_h = 0;
	sort_column = 1;  sort_order = 0;

	title_text_enable = 1;
	info_text_enable = 1;
	match_case_enable = 1;

	search_columns[col_ticon] = col_ticon;
	search_columns[col_id] = col_id;
	search_columns[col_length] = col_length;
	search_columns[col_source] = col_source;
	search_columns[col_title] = col_title;
	search_columns[col_start_time] = col_start_time;
	search_columns[col_access_time] = col_access_time;
	search_columns[col_access_count] = col_access_count;
	search_column_titles[col_ticon] = "ticon";
	search_column_titles[col_id] = "Id";
	search_column_titles[col_length] = "length";
	search_column_titles[col_source] = "Source";
	search_column_titles[col_title] = "Title";
	search_column_titles[col_start_time] = "Start time";
	search_column_titles[col_access_time] = "Access time";
	search_column_titles[col_access_count] = "count";
	search_column_widths[col_ticon] = 90;
	search_column_widths[col_id] = 60;
	search_column_widths[col_length] = 80;
	search_column_widths[col_source] = 50;
	search_column_widths[col_title] = 160;
	search_column_widths[col_start_time] = 140;
	search_column_widths[col_access_time] = 140;
	search_column_widths[col_access_count] = 60;
	int wd = 0;
	for( int i=0; i<sizeof_col; ++i )
		if( i != col_title ) wd += search_column_widths[i];
	search_column_widths[col_title] = get_w() - wd - 40;
}

DbWindowGUI::~DbWindowGUI()
{
	for( int i=0; i<sizeof_col; ++i )
		search_items[i].remove_all_objects();
	delete ticon_thread;
	delete canvas;
}

int DbWindowGUI::
resize_event(int w, int h)
{
	stop_drawing(1);
	int cancel_x = w - BC_CancelButton::calculate_w() - 10;
	int cancel_y = h - BC_CancelButton::calculate_h() - 10;
	cancel->reposition_window(cancel_x, cancel_y);
	search_x = 10;
	search_y = h - BC_GenericButton::calculate_h() - 10;
	search_start->reposition_window(search_x, search_y);
	del_items_x = search_x + DbWindowStart::calculate_w(this, search_start->get_text()) + 15;
	del_items_y = search_y;
	del_items->reposition_window(del_items_x,del_items_y);
	int list_w = w-10 - list_x;
	int list_h = min(search_y, cancel_y)-10 - list_y;
	canvas_w = SWIDTH;  canvas_h = SHEIGHT;
	canvas_x = cancel_x - canvas_w - 15;
	canvas_y = get_h() - canvas_h - 10;
	canvas->reposition_window(0, canvas_x, canvas_y, canvas_w, canvas_h);
//	int wd = 0;
//	for( int i=0; i<sizeof_col; ++i )
//		if( i != col_title ) wd += search_column_widths[i];
//	search_column_widths[col_title] = w - wd - 40;
	search_list->reposition_window(list_x, list_y, list_w, list_h);
	start_drawing(1);
	return 1;
}

int DbWindowGUI::
close_event()
{
	set_done(1);
	return 1;
}

int DbWindowGUI::
stop_drawing(int locked)
{
	if( locked ) unlock_window();
	ticon_thread->stop_drawing();
	if( locked ) lock_window("DbWindowGUI::");
	return 0;
}

int DbWindowGUI::
start_drawing(int update)
{
	if( update ) ticon_thread->list_update = 1;
	ticon_thread->start_drawing();
	return 0;
}

int DbWindowGUI::search(const char *sp)
{
	if( sp ) {
		int n = strlen(text);
		for( const char *cp=sp; *cp!=0; ++cp ) {
			if( match_case_enable ? !strncmp(text, cp, n) :
				!strncasecmp(text, cp, n) ) return 1;
		}
	}
	return 0;
}

void DbWindowGUI::search_clips(MediaDb *mdb)
{
	//Db::write_blocked by(mdb->db.Clip_set);
	if( !mdb->clips_first_id() ) do {
		if( (title_text_enable && search(mdb->clip_title())) ||
		    (info_text_enable  && search(mdb->clip_path()) ) ) {
			int id = mdb->clip_id();
			double system_time = mdb->clip_system_time();
			double start_time = 0;
			if( system_time > 0 )
				start_time = system_time + mdb->clip_position();
			double access_time = 0;  int access_count = 0;
			if( !mdb->views_clip_id(id) ) {
				 access_time = mdb->views_access_time();
				 access_count =  mdb->views_access_count();
			}
			double length = mdb->clip_frames() / mdb->clip_framerate();
			search_results.append(new DbWindowItem(id, mdb->clip_title(),
				mdb->clip_path(), length, start_time, access_time, access_count));
		}
	} while( !mdb->clips_next_id() );
}

void DbWindowGUI::search()
{
	stop_drawing(1);
        search_results.remove_all();
	DbWindow::MDb *mdb = dwindow->mdb;
	if( !mdb->attach_rd() ) {
		search_clips(mdb);
		mdb->detach();
	}
	start_drawing(1);
}

int DbWindowGUI::delete_selection(MediaDb *mdb)
{
	int n = 0;
	for( int k=0; k<search_results.size(); ++k ) {
		if( !search_items[0][k]->get_selected() ) continue;
		int id = search_results[k]->id;
		if( mdb->del_clip_set(id) ) {
			printf("failed delete clip id %d\n",id);
			continue;
		}
		search_results.remove_object_number(k);
		for( int i=0; i<sizeof_col; ++i )
			search_items[i].remove_object_number(k);
		++n;  --k;
	}
	if( n > 0 ) mdb->commitDb();
	return 0;
}

void DbWindowGUI::delete_items()
{
	stop_drawing();
	DbWindow::MDb *mdb = dwindow->mdb;
	if( !mdb->attach_wr() ) {
		delete_selection(mdb);
		mdb->detach();
	}
	start_drawing(1);
}

void DbWindowGUI::
update()
{

	for( int i=0; i<sizeof_col; ++i )
		search_items[i].remove_all_objects();

	char text[BCTEXTLEN];
	int n = search_results.size();
	for( int i=0; i<n; ++i ) {
		DbWindowItem *item = search_results[i];
		for( int k=0; k<sizeof_col; ++k ) {
			const char *cp = 0;  text[0] = 0;
			switch( search_columns[k] ) {
			case col_ticon:
				cp = "";
				break;
			case col_id: {
				sprintf(text,"%4d", item->id);
				cp = text;
				break; }
			case col_length: {
				sprintf(text, "%8.3f", item->length);
				cp = text;
				break; }
			case col_source:
				cp = item->source;
				break;
			case col_title:
				cp = item->title;
				break;
			case col_start_time: {
				time_t st = item->start_time;
				if( st > 0 ) {
					struct tm stm;  localtime_r(&st,&stm);
					sprintf(text,"%02d:%02d:%02d %02d/%02d/%02d",
 						stm.tm_hour, stm.tm_min, stm.tm_sec,
						stm.tm_year%100, stm.tm_mon, stm.tm_mday);
				}
				cp = text;
				break; }
			case col_access_time: {
				time_t at = item->access_time;
				if( at > 0 ) {
					struct tm atm;  localtime_r(&at,&atm);
					sprintf(text,"%02d/%02d/%02d %02d:%02d:%02d",
						atm.tm_year%100, atm.tm_mon, atm.tm_mday,
						atm.tm_hour, atm.tm_min, atm.tm_sec);
				}
				cp = text;
				break; }
			case col_access_count: {
				sprintf(text,"%4d", item->access_count);
				cp = text;
				break; }
			}
			if( !cp ) continue;
			BC_ListBoxItem *item = new BC_ListBoxItem(cp, LTYELLOW);
			if( search_columns[k] == col_ticon ) {
				item->set_text_w(SWIDTH+10);
				item->set_text_h(SHEIGHT+5);
				item->set_searchable(0);
			}
			search_items[k].append(item);
		}
	}

	search_list->update();
}


DbWindowItem::
DbWindowItem(int id, const char *source, const char *title,
	double length, double start_time,
	double access_time, int access_count)
{
	this->id = id;
	if( !source ) source = "";
	this->source = new char[strlen(source)+1];
	strcpy(this->source,source);
	if( !title ) title = "";
	this->title = new char[strlen(title)+1];
	strcpy(this->title,title);
	this->length = length;
	this->start_time = start_time;
	this->access_time = access_time;
	this->access_count = access_count;
}

DbWindowItem::
~DbWindowItem()
{
	delete source;
	delete title;
}

#define CmprFn(nm,key) int DbWindowGUI:: \
cmpr_##nm(const void *a, const void *b) { \
  DbWindowItem *&ap = *(DbWindowItem **)a; \
  DbWindowItem *&bp = *(DbWindowItem **)b; \
  int n = key; if( !n ) n = ap->no-bp->no; \
  return n; \
}

extern long timezone;

CmprFn(id_dn, ap->id - bp->id)
CmprFn(id_up, bp->id - ap->id)
CmprFn(length_dn, ap->length - bp->length)
CmprFn(length_up, bp->length - ap->length)
CmprFn(Source_dn, strcasecmp(ap->source, bp->source))
CmprFn(source_dn, strcmp(ap->source, bp->source))
CmprFn(Source_up, strcasecmp(bp->source, ap->source))
CmprFn(source_up, strcmp(bp->source, ap->source))
CmprFn(Title_dn, strcasecmp(ap->title, bp->title))
CmprFn(title_dn, strcmp(ap->title, bp->title))
CmprFn(Title_up, strcasecmp(bp->title, ap->title))
CmprFn(title_up, strcmp(bp->title, ap->title))
CmprFn(start_time_dn, fmod(ap->start_time-timezone,24*3600) - fmod(bp->start_time-timezone,24*3600))
CmprFn(start_time_up, fmod(bp->start_time-timezone,24*3600) - fmod(ap->start_time-timezone,24*3600))
CmprFn(access_time_dn, ap->access_time - bp->access_time)
CmprFn(access_time_up, bp->access_time - ap->access_time)
CmprFn(access_count_dn, ap->access_count - bp->access_count)
CmprFn(access_count_up, bp->access_count - ap->access_count)


void DbWindowGUI::
sort_events(int column, int order)
{
	sort_column = column;  sort_order = order;
	if( search_columns[sort_column] == col_ticon ) return;
	int n = search_results.size();
	if( !n ) return;
	DbWindowItem **items = &search_results[0];
	for( int i=0; i<n; ++i ) items[i]->no = i;

	int(*cmpr)(const void *, const void *) = 0;
	switch( search_columns[sort_column] ) {
	case col_id:
		cmpr = sort_order ? cmpr_id_up : cmpr_id_dn;
		break;
	case col_length:
		cmpr = sort_order ? cmpr_length_up : cmpr_length_dn;
		break;
	case col_source:
		cmpr = match_case_enable ?
			(sort_order ? cmpr_Source_up : cmpr_Source_dn) :
			(sort_order ? cmpr_source_up : cmpr_source_dn) ;
		break;
	case col_title:
		cmpr = match_case_enable ?
			(sort_order ? cmpr_Title_up : cmpr_Title_dn) :
			(sort_order ? cmpr_title_up : cmpr_title_dn) ;
		break;
	case col_start_time:
		cmpr = sort_order ? cmpr_start_time_up : cmpr_start_time_dn;
		break;
	case col_access_time:
		cmpr = sort_order ? cmpr_access_time_up : cmpr_access_time_dn;
		break;
	case col_access_count:
		cmpr = sort_order ? cmpr_access_count_up : cmpr_access_count_dn;
		break;
	}

	if( cmpr ) qsort(items, n, sizeof(*items), cmpr);
}

void DbWindowGUI::
move_column(int src, int dst)
{
	if( src == dst ) return;
	int src_column = search_columns[src];
	const char *src_column_title = search_column_titles[src];
	int src_column_width = search_column_widths[src];
	if( src < dst ) {
		for( int i=src; i<dst; ++i ) {
			search_columns[i] = search_columns[i+1];
			search_column_titles[i] = search_column_titles[i+1];
			search_column_widths[i] = search_column_widths[i+1];
		}
	}
	else {
		for( int i=src; i>dst; --i ) {
			search_columns[i] = search_columns[i-1];
			search_column_titles[i] = search_column_titles[i-1];
			search_column_widths[i] = search_column_widths[i-1];
		}
	}
	search_columns[dst] = src_column;
	search_column_titles[dst] = src_column_title;
	search_column_widths[dst] = src_column_width;
	int k = sizeof_col;
	while( --k >= 0 && search_columns[k] != col_ticon );
	if( k >= 0 ) search_list->set_master_column(k,0);
}


DbWindowCanvas::
DbWindowCanvas(DbWindowGUI *gui, int x, int y, int w, int h)
 : Canvas(gui->dwindow->mwindow, gui, x, y, w, h, w, h, 0)
{
	this->gui = gui;
	this->is_fullscreen = 0;
}

DbWindowCanvas::
~DbWindowCanvas()
{
}

void DbWindowCanvas::
flash_canvas()
{
	BC_WindowBase* wdw = get_canvas();
	wdw->lock_window("DbWindowCanvas::flash_canvas");
	wdw->flash();
	wdw->unlock_window();
}

void DbWindowCanvas::
draw_frame(VFrame *frame, int x, int y, int w, int h)
{
	BC_WindowBase* wdw = get_canvas();
	int min_y = gui->search_list->get_title_h();
	int max_y = gui->search_list->get_view_h()+min_y;
	min_y += 2;  max_y -= 2;
	int sx = 0, sy = 0, sw = frame->get_w(), sh = frame->get_h();
	int dy = min_y - y;
	if( dy > 0 ) { sy += dy;  y = min_y;   sh -= dy;  h -= dy; }
	dy = y+h - max_y;
	if( dy > 0 ) { sh -= dy;  h -= dy; }
	sh &= ~1;
	if( sh > 0 ) {
		//wdw->lock_window("DbWindowCanvas::draw_frame");
		wdw->draw_vframe(frame, x,y,w,(h&~1), sx,sy,sw,sh, 0);
		//wdw->unlock_window();
	}
}


DbWindowTIconThread::
DbWindowTIconThread(DbWindowGUI *gui)
 : Thread(1, 0, 0)
{
	this->gui = gui;
	mdb = gui->dwindow->mdb;
	mdb->add_user();
	ticon_lock = new Mutex("DbWindowTIconThread::ticon_lock");
	draw_lock = new Condition(0, "DbWindowTIconThread::draw_lock", 0);
	stop_lock = new Condition(0, "DbWindowTIconThread::stop_lock", 0);
	timer = new Timer();
	done = 0;  interrupted = 1;
	list_update = image_update = 0;
	Thread::start();
}

DbWindowTIconThread::
~DbWindowTIconThread()
{
	stop_drawing();
	done = 1;
	draw_lock->unlock();
	if( Thread::running() ) {
		Thread::cancel();
		Thread::join();
	}
	mdb->remove_user();
	t_heap.remove_all();
	ticons.remove_all_objects();
	delete draw_lock;
	delete stop_lock;
}

void DbWindowTIconThread::start_drawing()
{
        if( interrupted ) {
		interrupted = 0;
		draw_lock->unlock();
	}
}

void DbWindowTIconThread::stop_drawing()
{
        if( !interrupted ) {
                interrupted = 1;
                stop_lock->lock("DbWindowTIconThread::stop_draw");
        }
}


DbWindowTIcon::
DbWindowTIcon(DbWindowGUI *gui, int (DbWindowTIcon::*draw)() )
{
	this->gui = gui;
	this->draw = draw;
	age = 0;  x = y = 0;
        swidth = (SWIDTH+1) & ~1;
        sheight = (SHEIGHT+1) & ~1;
	frame = new VFrame(swidth, sheight, BC_YUV420P);
	memset(frame->get_y(),0,swidth * sheight);
	memset(frame->get_u(),0x80,swidth/2 * sheight/2);
	memset(frame->get_v(),0x80,swidth/2 * sheight/2);
        clip_id = -1;  clip_size = 0;
        frame_id = frames = -1;
        seq_no = 0;
        prefix_size = suffix_offset = -1;
	framerate = frame_period = 0;
}

DbWindowTIcon::
~DbWindowTIcon()
{
	delete frame;
}


void DbWindowTIcon::
update_image(int clip_id, int x, int y)
{
	DbWindowTIconThread *thread = gui->ticon_thread;
	DbWindow::MDb *mdb = gui->dwindow->mdb;
	if( !mdb->attach_rd() ) {
		if( !mdb->clip_id(clip_id) ) {
			this->clip_id = clip_id;
			this->x = x;  this->y = y;
			this->clip_size = mdb->clip_size();;
			this->prefix_size = mdb->clip_prefix_size();
			this->suffix_offset = mdb->clip_frames() - clip_size;
			this->framerate = mdb->clip_framerate();;
			this->frame_period = 1000. / framerate;
			thread->add_ticon(this, 0);
		}
	       	mdb->detach();
	}
}

int DbWindowTIcon::
get_seq_frame()
{
	int ret, no = seq_no++;
	if( seq_no >= clip_size ) seq_no = 0;
	DbWindow::MDb *mdb = gui->dwindow->mdb;
	if( !mdb->attach_rd() ) {
		if( !no ) frame_id = -1;
		else if( no >= prefix_size ) no += suffix_offset;
		if( !(ret=mdb->get_sequences(clip_id, no)) ) {
			if( mdb->timeline_sequence_no() == no )
				frame_id = mdb->timeline_frame_id();
			uint8_t *yp = frame->get_y();
			if( frame_id >= 0 ) {
				int sw, sh;
				ret = mdb->get_image(frame_id, yp, sw, sh);
			}
			else
				memset(yp,0,swidth*sheight);
		}
		else if( no > 0 ) ret = 0;
		mdb->detach();
	}
	return ret;
}

int DbWindowTIcon::
draw_frame()
{
	gui->canvas->draw_frame(frame, x, y, swidth, sheight);
	return 0;
}

int DbWindowTIcon::
draw_popup()
{
	BC_Popup *vpopup = gui->search_list->view_popup;
	if( !vpopup ) return 1;
	vpopup->lock_window("DbWindowTIcon::draw_popup");
	vpopup->draw_vframe(frame, 0,0,vpopup->get_w(),vpopup->get_h(), 0,0,swidth,sheight, 0);
	vpopup->flash();
	vpopup->unlock_window();
	return 0;
}

DbWindowTIcon *DbWindowTIconThread::get_ticon(int i)
{
	while( i >= ticons.size() ) {
		DbWindowTIcon *ticon = new DbWindowTIcon(gui, &DbWindowTIcon::draw_frame);
		ticons.append(ticon);
	}
	return ticons[i];
}

DbWindowTIcon *DbWindowTIconThread::low_ticon()
{
	mLock by(ticon_lock);
	int sz = t_heap.size();
	if( !sz ) return 0;
	DbWindowTIcon *ticon = t_heap[0];
	int i = 0;
	for( int k; (k=2*(i+1)) < sz; i=k ) {
		if( t_heap[k]->age > t_heap[k-1]->age ) --k;
		t_heap[i] = t_heap[k];
	}
	DbWindowTIcon *last = t_heap[--sz];
	t_heap.remove_number(sz);
	double age = last->age;
	for( int k; i>0 && age<t_heap[k=(i-1)/2]->age; i=k )
		t_heap[i] = t_heap[k];
	t_heap[i] = last;
	return ticon;
}

void DbWindowTIconThread::add_ticon(DbWindowTIcon *ticon, double age)
{
	mLock by(ticon_lock);
	ticon->age = age;
	int i = t_heap.size();  t_heap.append(ticon);
	for( int k; i>0 && age<t_heap[(k=(i-1)/2)]->age; i=k )
		t_heap[i] = t_heap[k];
	t_heap[i] = ticon;
}

int DbWindowList::
selection_changed()
{
	gui->stop_drawing(1);
	delete view_popup;
	view_popup = 0;
	int idx = get_selection_number(0, 0);
	if( idx >= 0 && get_selection_number(0, 1) < 0 ) {
		if( view_idx != idx ) {
			view_idx = idx;
			BC_ListBoxItem *item = gui->search_items[0][idx];
			int x = item->get_text_x() - get_xposition();
			int y = item->get_text_y() + get_title_h() - get_yposition();
			int w = 4*SWIDTH, h = 4*SHEIGHT;
			x += gui->get_x() - 2*SWIDTH;
			y += gui->get_y() - 4*SHEIGHT;
			view_popup = new BC_Popup(this, x, y, w, h, BLACK);
			view_ticon->update_image(gui->search_results[idx]->id, 0, 0);
		}
	}
	else
		view_idx = -1;
	gui->start_drawing(0);
	return 0;
}

void DbWindowTIconThread::
run()
{

	while(!done) {
		draw_lock->lock("DbWindowTIcon::run");
		if( done ) break;
		int do_flash = 0;
		int64_t last_flash = 0;
		while( !interrupted && !done ) {
			double period = 0;
			gui->lock_window("DbWindowTIconThread::run");
			if( list_update ) {
				list_update = 0;
				gui->update();
			}
			if( image_update ) {
				image_update = 0;
				gui->search_list->update_images();
				timer->update();
				do_flash = 0;  last_flash = 0;

			}
			gui->unlock_window();
			DbWindowTIcon *ticon = low_ticon();
			if( !ticon ) interrupted = 1;
			if( interrupted ) break;
			int64_t past = timer->get_difference();
			int64_t delay = ticon->age - past;
			if( delay < 10 ) {
				if( !ticon->get_seq_frame() ) ticon->draw_image();
				period = !ticon->seq_no ? 2000 : ticon->frame_period;
				if( past > last_flash + 100 ) do_flash = 1;
			}
			else
				do_flash = 1;
			if( do_flash ) {
				do_flash = 0;
				last_flash = past;
				gui->canvas->flash_canvas();
			}
			add_ticon(ticon, ticon->age + period);
			if( delay > 0 )
				Timer::delay(delay > 333 ? 333 : delay);
		}
		stop_lock->unlock();
	}

	t_heap.remove_all();
	ticons.remove_all_objects();
}



