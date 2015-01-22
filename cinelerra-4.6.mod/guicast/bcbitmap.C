
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

#include "bcbitmap.h"
#include "bcipc.h"
#include "bcresources.h"
#include "bcsignals.h"
#include "bcwindow.h"
#include "bccmodels.h"
#include "vframe.h"

#include <string.h>
#include <unistd.h>
#include <X11/extensions/Xvlib.h>

int BC_Bitmap::max_active_buffers = 0;
int BC_Bitmap::zombies = 0;


BC_BitmapImage::BC_BitmapImage(BC_Bitmap *bitmap, int index)
{
	this->index = index;
	this->bitmap = bitmap;
	this->top_level = bitmap->top_level;
        this->drawable = 0;
        this->data = 0;
        this->row_data = 0;
	this->dataSize = 0;
        this->bytesPerLine = 0;
        this->bitsPerPixel = 0;
}

BC_BitmapImage::~BC_BitmapImage()
{
	delete [] data;
	delete [] row_data;
}

bool BC_BitmapImage::is_avail()
{
	return owner == &bitmap->avail;
}

BC_BitmapImage *BC_Bitmap::cur_bfr()
{
	if( !active_bfr ) {
		avail_lock->lock("BC_Bitmap::cur_bfr 1");
		if( (!use_shm || top_level->is_running()) &&
		     (active_bfr=avail.first) != 0 )
			avail.remove_pointer(active_bfr);
		else {
			update_buffers(buffer_count+1, 0);
			if( (active_bfr=avail.first) != 0 )
				avail.remove_pointer(active_bfr);
			else
				active_bfr = new_buffer(type, -1);
		}
		avail_lock->unlock();
	}
	return active_bfr;
}

int BC_BitmapImage::
read_frame_rgb(VFrame *frame)
{
	int w = bitmap->w, h = bitmap->h;
	if( frame->get_w() != w || frame->get_h() != h ||
	    frame->get_color_model() != BC_RGB888 )
		frame->reallocate(0, 0, 0, 0, 0, w, h, BC_RGB888, -1);
	uint8_t *dp = frame->get_data();
	int rmask = ximage->red_mask;
	int gmask = ximage->green_mask;
	int bmask = ximage->blue_mask;
//due to a bug in the xserver, the mask is being cleared by XShm.C:402
//  when a faulty data in a XShmGetImage reply returns a zero visual
//workaround, use the original visual for the mask data
	if ( !rmask ) rmask = bitmap->top_level->vis->red_mask;
	if ( !gmask ) gmask = bitmap->top_level->vis->green_mask;
	if ( !bmask ) bmask = bitmap->top_level->vis->blue_mask;
	double fr = 255.0 / rmask;
	double fg = 255.0 / gmask;
	double fb = 255.0 / bmask;
	int nbyte = (ximage->bits_per_pixel+7)/8;
	int b0 = ximage->byte_order == MSBFirst ? 0 : nbyte-1;
	int bi = ximage->byte_order == MSBFirst ? 1 : -1;
	uint8_t *data = (uint8_t *)ximage->data;

	for( int y=0;  y<h; ++y ) {
		uint8_t *cp = data + y*ximage->bytes_per_line + b0;
		for( int x=0; x<w; ++x, cp+=nbyte ) {
			uint8_t *bp = cp;  uint32_t n = *bp;
			for( int i=nbyte; --i>0; n=(n<<8)|*bp ) bp += bi;
			*dp++ = (int)((n&rmask)*fr + 0.5);
			*dp++ = (int)((n&gmask)*fg + 0.5);
			*dp++ = (int)((n&bmask)*fb + 0.5);
		}
	}
	return 0;
}

void BC_Bitmap::reque(BC_BitmapImage *bfr)
{
	avail_lock->lock("BC_Bitmap::reque");
	--active_buffers;
	avail.append(bfr);
	avail_lock->unlock();
}

BC_XvShmImage::BC_XvShmImage(BC_Bitmap *bitmap, int index,
	int w, int h, int color_model)
 : BC_BitmapImage(bitmap, index)
{
	Display *display = top_level->get_display();
	int id = BC_WindowBase::get_cmodels()->bc_to_x(color_model);
// Create the XvImage
	xv_image = XvShmCreateImage(display, bitmap->xv_portid, id,
			0, w, h, &shm_info);
	dataSize = xv_image->data_size;
// Create the shared memory
	shm_info.shmid = shmget(IPC_PRIVATE, 
		dataSize + 8, IPC_CREAT | 0777);
	if(shm_info.shmid < 0)
		perror("BC_XvShmImage::BC_XvShmImage shmget");
	data = (unsigned char *)shmat(shm_info.shmid, NULL, 0);
// This causes it to automatically delete when the program exits.
	shmctl(shm_info.shmid, IPC_RMID, 0);
// setting ximage->data stops BadValue
	xv_image->data = shm_info.shmaddr = (char*)data;
	shm_info.readOnly = 0;
// Get the real parameters
	w = xv_image->width;
	h = xv_image->height;
	if(!XShmAttach(top_level->display, &shm_info))
		perror("BC_XvShmImage::BC_XvShmImage XShmAttach");
	if( color_model == BC_YUV422 ) {
	 	bytesPerLine = w*2;
		bitsPerPixel = 12;
		row_data = new unsigned char*[h];
		for( int i=0; i<h; ++i )
			row_data[i] = &data[i*bytesPerLine];
	}
}

BC_XvShmImage::~BC_XvShmImage()
{
	data = 0;
	XFree(xv_image);
	XShmDetach(top_level->display, &shm_info);
	shmdt(shm_info.shmaddr);
}


BC_XShmImage::BC_XShmImage(BC_Bitmap *bitmap, int index,
	int w, int h, int color_model)
 : BC_BitmapImage(bitmap, index)
{
	Display *display = top_level->display;
	Visual *visual = top_level->vis;
	int default_depth = bitmap->get_default_depth();
    	ximage = XShmCreateImage(display, visual, 
		default_depth, default_depth == 1 ? XYBitmap : ZPixmap, 
		(char*)NULL, &shm_info, w, h);
// Create shared memory
 	bytesPerLine = ximage->bytes_per_line;
	bitsPerPixel = ximage->bits_per_pixel;
	dataSize = h * bytesPerLine;
	shm_info.shmid = shmget(IPC_PRIVATE, 
		dataSize + 8, IPC_CREAT | 0777);
	if(shm_info.shmid < 0) 
		perror("BC_XShmImage::BC_XShmImage shmget");
	data = (unsigned char *)shmat(shm_info.shmid, NULL, 0);
// This causes it to automatically delete when the program exits.
	shmctl(shm_info.shmid, IPC_RMID, 0);
	ximage->data = shm_info.shmaddr = (char*)data;
	shm_info.readOnly = 0;
// Get the real parameters
	if(!XShmAttach(top_level->display, &shm_info))
		perror("BC_XShmImage::BC_XShmImage XShmAttach");
	row_data = new unsigned char*[h];
	for( int i=0; i<h; ++i )
		row_data[i] = &data[i*bytesPerLine];
}

BC_XShmImage::~BC_XShmImage()
{
	data = 0;
	ximage->data = 0;
	XDestroyImage(ximage);
	XShmDetach(top_level->display, &shm_info);
	XFlush(top_level->display);
	shmdt(shm_info.shmaddr);
}



BC_XvImage::BC_XvImage(BC_Bitmap *bitmap, int index,
	int w, int h, int color_model)
 : BC_BitmapImage(bitmap, index)
{
	Display *display = top_level->display;
	int id = BC_WindowBase::get_cmodels()->bc_to_x(color_model);
	xv_image = XvCreateImage(display, bitmap->xv_portid, id, 0, w, h);
	dataSize = xv_image->data_size;
	data = new unsigned char[dataSize + 8];
	xv_image->data = (char *) data;
	w = xv_image->width;
	h = xv_image->height;
	if( color_model == BC_YUV422 ) {
	 	int bytesPerLine = w*2;
		row_data = new unsigned char*[h];
		for( int i=0; i<h; ++i )
			row_data[i] = &data[i*bytesPerLine];
	}
}

BC_XvImage::~BC_XvImage()
{
	XFree(xv_image);
}


BC_XImage::BC_XImage(BC_Bitmap *bitmap, int index,
	int w, int h, int color_model)
 : BC_BitmapImage(bitmap, index)
{
	Display *display = top_level->display;
	Visual *visual = top_level->vis;
	int default_depth = bitmap->get_default_depth();
	ximage = XCreateImage(display, visual, default_depth, 
		default_depth == 1 ? XYBitmap : ZPixmap, 
		0, (char*)data, w, h, 8, 0);
 	bytesPerLine = ximage->bytes_per_line;
	bitsPerPixel = ximage->bits_per_pixel;
	dataSize = h * bytesPerLine;
	data = new unsigned char[dataSize + 8];
	ximage->data = (char*) data;
	row_data = new unsigned char*[h];
	for( int i=0; i<h; ++i )
		row_data[i] = &data[i*bytesPerLine];
}

BC_XImage::~BC_XImage()
{
	delete [] data;
	data = 0;
	ximage->data = 0;
	XDestroyImage(ximage);
}


BC_Bitmap::BC_Bitmap(BC_WindowBase *parent_window, unsigned char *png_data)
{
// Decompress data into a temporary vframe
	VFrame frame;

	frame.read_png(png_data);

	avail_lock = 0;
// Initialize the bitmap
	initialize(parent_window, 
		frame.get_w(), 
		frame.get_h(), 
		parent_window->get_color_model(), 
		0);

// Copy the vframe to the bitmap
	read_frame(&frame, 0, 0, w, h);
}

BC_Bitmap::BC_Bitmap(BC_WindowBase *parent_window, VFrame *frame)
{
	avail_lock = 0;
// Initialize the bitmap
	initialize(parent_window, 
		frame->get_w(), 
		frame->get_h(), 
		parent_window->get_color_model(), 
		0);

// Copy the vframe to the bitmap
	read_frame(frame, 0, 0, w, h);
}

BC_Bitmap::BC_Bitmap(BC_WindowBase *parent_window, 
	int w, int h, int color_model, int use_shm)
{
	avail_lock = 0;
	initialize(parent_window, w, h, color_model, use_shm);
}

BC_Bitmap::~BC_Bitmap()
{
	delete_data();
	delete avail_lock;
}

int BC_Bitmap::initialize(BC_WindowBase *parent_window, 
	int w, int h, int color_model, int use_shm)
{
	BC_Resources *resources = parent_window->get_resources();
	this->parent_window = parent_window;
	this->top_level = parent_window->top_level;
	this->xv_portid = resources->use_xvideo ? top_level->xvideo_port_id : -1;
	this->w = w;
	this->h = h;
	this->color_model = color_model;
	this->use_shm = !use_shm ? 0 : need_shm();
        this->shm_reply = this->use_shm && resources->shm_reply ? 1 : 0;
	// dont use shm for less than one page
	this->bg_color = parent_window->bg_color;
	if( !this->avail_lock )
		this->avail_lock = new Mutex("BC_Bitmap::avail_lock");
	else
		this->avail_lock->reset();
	this->buffers = 0;
	this->last_pixmap_used = 0;
	this->last_pixmap = 0;

	this->active_bfr = 0;
	this->buffer_count = 0;
	this->max_buffer_count = 0;
	this->active_buffers = 0;
	allocate_data();
	return 0;
}

int BC_Bitmap::match_params(int w, int h, int color_model, int use_shm)
{
	if( use_shm ) use_shm = need_shm();
	if(this->w /* != */ < w || this->h /* != */ < h ||
	   this->color_model != color_model || this->use_shm != use_shm) {
		delete_data();
		initialize(parent_window, w, h, color_model, use_shm);
	}

	return 0;
}

int BC_Bitmap::params_match(int w, int h, int color_model, int use_shm)
{
	int result = 0;
	if( use_shm ) use_shm = need_shm();
	if(this->w == w && this->h == h && this->color_model == color_model) {
		if(use_shm == this->use_shm || use_shm == BC_INFINITY)
			result = 1;
	}
	return result;
}

BC_BitmapImage *BC_Bitmap::new_buffer(int type, int idx)
{
	BC_BitmapImage *buffer = 0;
	if( idx < 0 ) {
		if( type == bmXShmImage ) type = bmXImage;
		else if( type ==  bmXvShmImage ) type = bmXvImage;
	}
	switch( type ) {
	default:
	case bmXImage:
		buffer = new BC_XImage(this, idx, w, h, color_model);
		break;
	case bmXvImage:
		buffer = new BC_XvImage(this, idx, w, h, color_model);
		break;
	case bmXShmImage:
		buffer = new BC_XShmImage(this, idx, w, h, color_model);
		break;
	case bmXvShmImage:
		buffer = new BC_XvShmImage(this, idx, w, h, color_model);
		break;
	}
	if( buffer->is_zombie() ) ++zombies;
	return buffer;
}

void BC_Bitmap::
update_buffers(int count, int lock_avail)
{
	int i;
	// can deadlock in XReply (libxcb bug) without this lock
	//top_level->lock_window("BC_Bitmap::update_buffers");
	if( lock_avail ) avail_lock->lock("BC_Bitmap::update_buffers");
	if( count > max_buffer_count ) count = max_buffer_count;
	BC_BitmapImage **new_buffers = !count ? 0 : new BC_BitmapImage *[count];
	if( buffer_count < count ) {
		for( i=0; i<buffer_count; ++i )
			new_buffers[i] = buffers[i];
		while( i < count ) {
			BC_BitmapImage *buffer = new_buffer(type, i);
			new_buffers[i] = buffer;
			avail.append(buffer);
			++i;
		}
	}
	else {
		for( i=0; i<count; ++i )
			new_buffers[i] = buffers[i];
		while( i < buffer_count ) {
			BC_BitmapImage *buffer = buffers[i];
			if( buffer == active_bfr ) active_bfr = 0;
			if( buffer->is_avail() ) {
				avail.remove_pointer(buffer);
				delete buffer;
			}
			else {
				++zombies;
				buffer->index = -1;
			}
			++i;
		}
	}
	if( lock_avail ) avail_lock->unlock();
	delete [] buffers;
	buffers = new_buffers;
	buffer_count = count;
	//top_level->unlock_window();
}

int BC_Bitmap::allocate_data()
{
	int count = 1;
	max_buffer_count = MAX_BITMAP_BUFFERS;
	if(use_shm) { // Use shared memory.
		int bsz = best_buffer_size();
		if( bsz >= 0x800000 ) max_buffer_count = 2;
		else if( bsz >= 0x400000 ) max_buffer_count /= 8;
		else if( bsz >= 0x100000 ) max_buffer_count /= 4;
		else if( bsz >= 0x10000 ) max_buffer_count /= 2;
		type = hardware_scaling() ? bmXvShmImage : bmXShmImage;
		count = MIN_BITMAP_BUFFERS;
	}
	else // use unshared memory.
		type = hardware_scaling() ? bmXvImage : bmXImage;
	update_buffers(count);
	return 0;
}

int BC_Bitmap::delete_data()
{
//printf("BC_Bitmap::delete_data 1\n");
	if( last_pixmap_used && xv_portid >= 0 )
		XvStopVideo(top_level->display, xv_portid, last_pixmap);
	update_buffers(0);
	if( xv_portid >= 0 )
		XvUngrabPort(top_level->display, xv_portid, CurrentTime);
	last_pixmap_used = 0;
	active_bfr = 0;
	buffer_count = 0;
	max_buffer_count = 0;
	return 0;
}

int BC_Bitmap::get_default_depth()
{
	return color_model == BC_TRANSPARENCY ? 1 : top_level->default_depth;
}

long BC_Bitmap::best_buffer_size()
{
	long pixelsize = BC_WindowBase::get_cmodels()->
		calculate_pixelsize(get_color_model());
	return pixelsize * w * h;
}

int BC_Bitmap::need_shm()
{
// dont use shm for less than one page
	return best_buffer_size() < 0x1000 ? 0 :
		parent_window->get_resources()->use_shm > 0 ? 1 : 0;
}

int BC_Bitmap::set_bg_color(int color)
{
	this->bg_color = color;
	return 0;
}

int BC_Bitmap::invert()
{
	for( int j=0; j<buffer_count; ++j ) {
		unsigned char **rows = buffers[j]->get_row_data();
		if( !rows ) continue;
		long bytesPerLine = buffers[j]->bytes_per_line();
		for( int k=0; k<h; ++k ) {
			unsigned char *bp = rows[k];
			for( int i=0; i<bytesPerLine; ++i )
				*bp++ ^= 0xff;
		}
	}
	return 0;
}

int BC_XShmImage::get_shm_size()
{
	return bytesPerLine * bitmap->h;
}

int BC_Bitmap::write_drawable(Drawable &pixmap, GC &gc,
	int dest_x, int dest_y, int source_x, int source_y, 
	int dest_w, int dest_h, int dont_wait)
{
	return write_drawable(pixmap, gc,
		source_x, source_y, get_w() - source_x, get_h() - source_y,
		dest_x, dest_y, dest_w, dest_h, dont_wait);
}

int BC_XImage::write_drawable(Drawable &pixmap, GC &gc,
	int source_x, int source_y, int source_w, int source_h,
	int dest_x, int dest_y, int dest_w, int dest_h)
{
	XPutImage(top_level->display, pixmap, gc,
		ximage, source_x, source_y,
		dest_x, dest_y, dest_w, dest_h);
	return 0;
}

int BC_XShmImage::write_drawable(Drawable &pixmap, GC &gc,
	int source_x, int source_y, int source_w, int source_h,
	int dest_x, int dest_y, int dest_w, int dest_h)
{
	XShmPutImage(top_level->display, pixmap, gc,
		ximage, source_x, source_y,
		dest_x, dest_y, dest_w, dest_h, bitmap->shm_reply);
	return 0;
}

int BC_XvImage::write_drawable(Drawable &pixmap, GC &gc,
	int source_x, int source_y, int source_w, int source_h,
	int dest_x, int dest_y, int dest_w, int dest_h)
{
	XvPutImage(top_level->display,
		bitmap->xv_portid, pixmap, gc, xv_image,
		source_x, source_y, source_w, source_h,
		dest_x, dest_y, dest_w, dest_h);
	return 0;
}

int BC_XvShmImage::write_drawable(Drawable &pixmap, GC &gc,
	int source_x, int source_y, int source_w, int source_h,
	int dest_x, int dest_y, int dest_w, int dest_h)
{
	XvShmPutImage(top_level->display,
		bitmap->xv_portid, pixmap, gc, xv_image,
		source_x, source_y, source_w, source_h,
		dest_x, dest_y, dest_w, dest_h, bitmap->shm_reply);
	return 0;
}

int BC_Bitmap::write_drawable(Drawable &pixmap, GC &gc,
		int source_x, int source_y, int source_w, int source_h,
		int dest_x, int dest_y, int dest_w, int dest_h, 
		int dont_wait)
{
//printf("BC_Bitmap::write_drawable 1 %p %d\n", this, current_ringbuffer); fflush(stdout);
	//if( dont_wait ) XSync(top_level->display, False);
	BC_BitmapImage *bfr = cur_bfr();
	if( !bfr->is_zombie() && is_shared() && shm_reply ) {
//printf("activate %p %08lx\n",bfr,bfr->get_shmseg());
		top_level->active_bitmaps.insert(bfr, pixmap);
		if( ++active_buffers > max_active_buffers )
			max_active_buffers = active_buffers;
	}
	bfr->write_drawable(pixmap, gc,
		source_x, source_y, source_w, source_h,
		dest_x, dest_y, dest_w, dest_h);
	XFlush(top_level->display);
	avail_lock->lock(" BC_Bitmap::write_drawable");
	if( bfr->is_zombie() ) {
		delete bfr;
		--zombies;
	}
	else if( is_unshared() || !shm_reply )
		avail.append(bfr);
	active_bfr = 0;
	avail_lock->unlock();
	last_pixmap = pixmap;
	last_pixmap_used = 1;
	if( !dont_wait && !shm_reply )
		XSync(top_level->display, False);
	return 0;
}


int BC_XImage::read_drawable(Drawable &pixmap, int source_x, int source_y)
{
	XGetSubImage(top_level->display, pixmap,
		source_x, source_y, bitmap->w, bitmap->h,
		0xffffffff, ZPixmap, ximage, 0, 0);
	return 0;
}

int BC_XShmImage::read_drawable(Drawable &pixmap, int source_x, int source_y)
{
	XShmGetImage(top_level->display, pixmap,
		ximage, source_x, source_y, 0xffffffff);
	return 0;
}

int BC_Bitmap::read_drawable(Drawable &pixmap, int source_x, int source_y, VFrame *frame)
{
	BC_BitmapImage *bfr = cur_bfr();
	int result = bfr->read_drawable(pixmap, source_x, source_y);
	if( !result && frame ) result = bfr->read_frame_rgb(frame);
	return result;
}


// ============================ Decoding VFrames

int BC_Bitmap::read_frame(VFrame *frame, int x1, int y1, int x2, int y2)
{
	return read_frame(frame, 
		0, 0, frame->get_w(), frame->get_h(),
		x1, y1, x2 - x1, y2 - y1);
}


int BC_Bitmap::read_frame(VFrame *frame, 
	int in_x, int in_y, int in_w, int in_h,
	int out_x, int out_y, int out_w, int out_h)
{
	BC_BitmapImage *bfr = cur_bfr();
	if( hardware_scaling() && frame->get_color_model() == color_model ) {
// Hardware accelerated bitmap
		switch(color_model) {
		case BC_YUV420P:
			memcpy(bfr->get_y_data(), frame->get_y(), w * h);
			memcpy(bfr->get_u_data(), frame->get_u(), w * h / 4);
			memcpy(bfr->get_v_data(), frame->get_v(), w * h / 4);
			break;
		case BC_YUV422P:
			memcpy(bfr->get_y_data(), frame->get_y(), w * h);
			memcpy(bfr->get_u_data(), frame->get_u(), w * h / 2);
			memcpy(bfr->get_v_data(), frame->get_v(), w * h / 2);
			break;
		default:
		case BC_YUV422:
			memcpy(get_data(), frame->get_data(), w * h + w * h);
			break;
		}
	}
	else {
// Software only

//printf("BC_Bitmap::read_frame %d -> %d %d %d %d %d -> %d %d %d %d\n",
//  frame->get_color_model(), color_model,
//  in_x, in_y, in_w, in_h, out_x, out_y, out_w, out_h);
//if(color_model == 6 && frame->get_color_model() == 19)
//printf("BC_Bitmap::read_frame 1 %d %d %d %d\n", frame->get_w(), frame->get_h(), get_w(), get_h());
		BC_WindowBase::get_cmodels()->transfer(
			bfr->get_row_data(), frame->get_rows(),
			bfr->get_y_data(), bfr->get_u_data(), bfr->get_v_data(),
			frame->get_y(), frame->get_u(), frame->get_v(),
			in_x, in_y, in_w, in_h,
			out_x, out_y, out_w, out_h,
			frame->get_color_model(), color_model,
			bg_color, frame->get_w(), w);
//if(color_model == 6 && frame->get_color_model() == 19)
//printf("BC_Bitmap::read_frame 2\n");
	}

	return 0;
}

