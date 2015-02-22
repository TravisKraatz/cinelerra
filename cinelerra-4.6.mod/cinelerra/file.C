/*
 * CINELERRA
 * Copyright (C) 2010 Adam Williams <broadcast at earthling dot net>
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
#include "bchash.h"
#include "bcsignals.h"
#include "byteorder.h"
#include "cache.inc"
#include "condition.h"
#include "errorbox.h"
#include "fileac3.h"
#include "fileavi.h"
#include "filebase.h"
#include "filecr2.h"
#include "filedb.h"
#include "filedv.h"
#include "fileexr.h"
#include "fileffmpeg.h"
#include "fileflac.h"
#include "filefork.h"
#include "filegif.h"
#include "file.h"
#include "filejpeg.h"
#include "filemov.h"
#include "filempeg.h"
#undef HAVE_STDLIB_H // automake conflict
#include "fileogg.h"
#include "filepng.h"
#include "filescene.h"
#include "fileserver.h"
#include "filesndfile.h"
#include "filetga.h"
#include "filethread.h"
#include "filetiff.h"
#include "filevorbis.h"
#include "filexml.h"
#include "fileyuv.h"
#include "format.inc"
#include "formatwindow.h"
#include "formattools.h"
#include "framecache.h"
#include "language.h"
#include "mutex.h"
#include "mwindow.h"
#include "packagingengine.h"
#include "pluginserver.h"
#include "preferences.h"
#include "samples.h"
#include "stringfile.h"
#include "vframe.h"




File::File()
{
	cpus = 1;
	asset = new Asset;
	format_completion = new Condition(1, "File::format_completion");
	write_lock = new Condition(1, "File::write_lock");
	frame_cache = new FrameCache;
#ifdef USE_FILEFORK
	forked = new Mutex("File::forked",0);
#endif
	reset_parameters();
}

File::~File()
{
	if(getting_options)
	{
		if(format_window) format_window->set_done(0);
		format_completion->lock("File::~File");
		format_completion->unlock();
	}

	if(temp_frame) delete temp_frame;


	close_file(0);

	asset->Garbage::remove_user();
	delete format_completion;
	delete write_lock;
#ifdef USE_FILEFORK
	delete forked;
#endif
	delete frame_cache;
}

void File::reset_parameters()
{
#ifdef USE_FILEFORK
	file_fork = 0;
	is_fork = 0;
#endif

	file = 0;
	audio_thread = 0;
	video_thread = 0;
	getting_options = 0;
	format_window = 0;
	temp_frame = 0;
	current_sample = 0;
	current_frame = 0;
	current_channel = 0;
	current_program = 0;
	current_layer = 0;
	normalized_sample = 0;
	use_cache = 0;
	preferences = 0;
	playback_subtitle = -1;
	interpolate_raw = 1;


	temp_samples_buffer = 0;
	temp_frame_buffer = 0;
	current_frame_buffer = 0;
	audio_ring_buffers = 0;
	video_ring_buffers = 0;
	video_buffer_size = 0;
}

int File::raise_window()
{
	if(getting_options && format_window)
	{
		format_window->raise_window();
		format_window->flush();
	}
	return 0;
}

void File::close_window()
{
	if(getting_options)
	{
		format_window->lock_window("File::close_window");
		format_window->set_done(1);
		format_window->unlock_window();
		getting_options = 0;
	}
}

int File::get_options(FormatTools *format,
	int audio_options, int video_options)
{
	BC_WindowBase *parent_window = format->window;
	ArrayList<PluginServer*> *plugindb = format->plugindb;
	Asset *asset = format->asset;
	const char *locked_compressor = format->locked_compressor;

	getting_options = 1;
	format_completion->lock("File::get_options");
	switch(asset->format)
	{
		case FILE_AC3:
			FileAC3::get_parameters(parent_window,
				asset,
				format_window,
				audio_options,
				video_options);
			break;
		case FILE_RAWDV:
			FileDV::get_parameters(parent_window,
				asset,
				format_window,
				audio_options,
				video_options);
			break;
		case FILE_PCM:
		case FILE_WAV:
		case FILE_AU:
		case FILE_AIFF:
		case FILE_SND:
			FileSndFile::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_MOV:
			FileMOV::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options,
				locked_compressor);
			break;
		case FILE_AMPEG:
		case FILE_VMPEG:
			FileMPEG::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_AVI:
			FileMOV::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options,
				locked_compressor);
			break;
		case FILE_AVI_LAVTOOLS:
		case FILE_AVI_ARNE2:
		case FILE_AVI_ARNE1:
		case FILE_AVI_AVIFILE:
			FileAVI::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options,
				locked_compressor);
			break;
		case FILE_JPEG:
		case FILE_JPEG_LIST:
			FileJPEG::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_EXR:
		case FILE_EXR_LIST:
			FileEXR::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
	        case FILE_YUV:
			FileYUV::get_parameters(parent_window,
				asset,
				format_window,
				video_options,
				format);
			break;
		case FILE_FLAC:
			FileFLAC::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_PNG:
		case FILE_PNG_LIST:
			FilePNG::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_TGA:
		case FILE_TGA_LIST:
			FileTGA::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_TIFF:
		case FILE_TIFF_LIST:
			FileTIFF::get_parameters(parent_window, 
				asset, 
				format_window, 
				audio_options, 
				video_options);
			break;
		case FILE_OGG:
			FileOGG::get_parameters(parent_window,
				asset,
				format_window,
				audio_options,
				video_options);
			break;
		default:
			break;
	}

	if(!format_window)
	{
		ErrorBox *errorbox = new ErrorBox(PROGRAM_NAME ": Error",
			parent_window->get_abs_cursor_x(1),
			parent_window->get_abs_cursor_y(1));
		format_window = errorbox;
		getting_options = 1;
		if(audio_options)
			errorbox->create_objects(_("This format doesn't support audio."));
		else
		if(video_options)
			errorbox->create_objects(_("This format doesn't support video."));
		errorbox->run_window();
		delete errorbox;
	}

	getting_options = 0;
	format_window = 0;
	format_completion->unlock();
	return 0;
}










int File::set_processors(int cpus)   // Set the number of cpus for certain codecs
{
	if( cpus > 8 )		// mpegvideo max_threads = 16, more causes errs
		cpus = 8;	//  8 cpus ought to decode just about anything
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_PROCESSORS, (unsigned char*)&cpus, sizeof(cpus));
		file_fork->read_result();
	}
#endif

// Set all instances so gets work.
	this->cpus = cpus;

	return 0;
}

int File::set_preload(int64_t size)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_PRELOAD, (unsigned char*)&size, sizeof(size));
		file_fork->read_result();
	}

#endif

	this->playback_preload = size;
	return 0;
}

void File::set_subtitle(int value)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_SUBTITLE, (unsigned char*)&value, sizeof(value));
		file_fork->read_result();
	}

#endif
	this->playback_subtitle = value;
	if( file ) file->set_subtitle(value);
}

void File::set_interpolate_raw(int value)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_INTERPOLATE_RAW, (unsigned char*)&value, sizeof(value));
		file_fork->read_result();
	}

#endif

	this->interpolate_raw = value;
}

void File::set_white_balance_raw(int value)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_WHITE_BALANCE_RAW, (unsigned char*)&value, sizeof(value));
		file_fork->read_result();
	}
#endif

	this->white_balance_raw = value;
}


void File::set_cache_frames(int value)
{
// caching only done locally
	if(!video_thread)
		use_cache = value;
}

int File::purge_cache()
{
// caching only done locally
	int result = 0;
	if( frame_cache->cache_items() > 0 )
	{
		frame_cache->remove_all();
		result = 1;
	}
	return result;
}

int File::delete_oldest()
{
// caching only done locally
	return frame_cache->delete_oldest();
}











int File::open_file(Preferences *preferences, 
	Asset *asset, 
	int rd, 
	int wr)
{
	int result = 0;
	const int debug = 0;

	this->preferences = preferences;
	this->asset->copy_from(asset, 1);
	this->rd = rd;
	this->wr = wr;
	file = 0;

	if(debug) printf("File::open_file %d\n", __LINE__);

#ifdef USE_FILEFORK
	if(!is_fork && MWindow::file_server && (rd || wr))
	{
		FileForker this_is(*forked);
// printf("File::open_file %d file_server=%p rd=%d wr=%d %d\n", 
// __LINE__, 
// MWindow::file_server,
// rd, 
// wr, 
// asset->ms_quantization);
		file_fork = MWindow::file_server->new_filefork();
//printf("File::open_file %d\n", __LINE__);

// Send the asset
// Convert to hash table
		BC_Hash table;
		asset->save_defaults(&table, "", 1, 1, 1, 1, 1);
// Convert to string
		char *string = 0;
		table.save_string(string);
		int buffer_size = sizeof(int) * 7 + strlen(string) + 1;
		unsigned char *buffer = new unsigned char[buffer_size];
		int offset = 0;
		*(int*)(buffer + offset) = rd;
		offset += sizeof(int);
		*(int*)(buffer + offset) = wr;
		offset += sizeof(int);
		*(int*)(buffer + offset) = cpus;
		offset += sizeof(int);
		*(int*)(buffer + offset) = white_balance_raw;
		offset += sizeof(int);
		*(int*)(buffer + offset) = interpolate_raw;
		offset += sizeof(int);
		*(int*)(buffer + offset) = playback_subtitle;
		offset += sizeof(int);
		*(int*)(buffer + offset) = current_program;
		offset += sizeof(int);
		memcpy(buffer + offset, string, strlen(string) + 1);
//printf("File::open_file %d\n", __LINE__);
		file_fork->send_command(FileFork::OPEN_FILE, 
			buffer, 
			buffer_size);
		delete [] buffer;
		delete [] string;
//printf("File::open_file %d\n", __LINE__);

// Get the updated asset from the fork
		result = file_fork->read_result();
//printf("File::open_file %d\n", __LINE__);
		if(!result)
		{
			table.load_string((char*)file_fork->result_data);

			asset->load_defaults(&table, "", 1, 1, 1, 1, 1);
			this->asset->load_defaults(&table, "", 1, 1, 1, 1, 1);
//this->asset->dump();
		}
//printf("File::open_file %d\n", __LINE__);


// If it's a scene renderer, close it & reopen it locally to get the 
// full OpenGL support.
// Just doing 2D for now.  Should be forked in case Festival crashes.
// 		if(rd && this->asset->format == FILE_SCENE)
// 		{
// //printf("File::open_file %p %d\n", this, __LINE__);
// 			close_file(0);
// // Lie to get it to work properly
// 			is_fork = 1;
// 		}
// 		else
		{
			return result;
		}
	}
#endif


	if(debug) printf("File::open_file %p %d\n", this, __LINE__);

	switch(this->asset->format)
	{
// get the format now
// If you add another format to case 0, you also need to add another case for the
// file format #define.
		case FILE_UNKNOWN:
			if(FileDB::check_sig(this->asset))
			{
// MediaDb file
				file = new FileDB(this->asset, this);
				break;
			}

			FILE *stream;
			if(!(stream = fopen(this->asset->path, "rb")))
			{
// file not found
				return 1;
			}

			char test[16];
			result = fread(test, 16, 1, stream);

			if(FileScene::check_sig(this->asset, test))
			{
// libsndfile
				fclose(stream);
				file = new FileScene(this->asset, this);
			}
			else
			if(FileDV::check_sig(this->asset))
			{
// libdv
				fclose(stream);
				file = new FileDV(this->asset, this);
			}
			else if(FileSndFile::check_sig(this->asset))
			{
// libsndfile
				fclose(stream);
				file = new FileSndFile(this->asset, this);
			}
			else
			if(FilePNG::check_sig(this->asset))
			{
// PNG file
				fclose(stream);
				file = new FilePNG(this->asset, this);
			}
			else
			if(FileJPEG::check_sig(this->asset))
			{
// JPEG file
				fclose(stream);
				file = new FileJPEG(this->asset, this);
			}
			else
			if(FileGIF::check_sig(this->asset))
			{
// GIF file
				fclose(stream);
				file = new FileGIF(this->asset, this);
			}
			else
			if(FileEXR::check_sig(this->asset, test))
			{
// EXR file
				fclose(stream);
				file = new FileEXR(this->asset, this);
			}
			else
			if(FileYUV::check_sig(this->asset))
			{
// YUV file
				fclose(stream);
				file = new FileYUV(this->asset, this);
			}
			else
			if(FileFLAC::check_sig(this->asset, test))
			{
// FLAC file
				fclose(stream);
				file = new FileFLAC(this->asset, this);
			}
			else
			if(FileCR2::check_sig(this->asset))
			{
// CR2 file
				fclose(stream);
				file = new FileCR2(this->asset, this);
			}
			else
			if(FileTGA::check_sig(this->asset))
			{
// TGA file
				fclose(stream);
				file = new FileTGA(this->asset, this);
			}
			else
			if(FileTIFF::check_sig(this->asset))
			{
// TIFF file
				fclose(stream);
				file = new FileTIFF(this->asset, this);
			}
			else
			if(FileOGG::check_sig(this->asset))
			{
// OGG file
				fclose(stream);
				file = new FileOGG(this->asset, this);
			}
			else
			if(FileVorbis::check_sig(this->asset))
			{
// VorbisFile file
				fclose(stream);
				file = new FileVorbis(this->asset, this);
			}
			else
			if(FileOGG::check_sig(this->asset))
			{
// OGG file.  Doesn't always work with pure audio files.
				fclose(stream);
				file = new FileOGG(this->asset, this);
			}
			else
			if(FileMPEG::check_sig(this->asset))
			{
// MPEG file
				fclose(stream);
				file = new FileMPEG(this->asset, this);
			}
			else
			if( test[0] == '<' && (
				!strncmp(&test[1],"EDL>",4) ||
				!strncmp(&test[1],"HTAL>",5) ||
				!strncmp(&test[1],"?xml",4) ) )
			{
// XML file
				fclose(stream);
				return FILE_IS_XML;
			}    // can't load project file
			else
			if(FileMOV::check_sig(this->asset))
			{
// MOV file
// should be last because quicktime lacks a magic number
				fclose(stream);
				file = new FileMOV(this->asset, this);
			}
			else
// FFMPEG last because it sux
			if(FileFFMPEG::check_sig(this->asset))
			{
				fclose(stream);
				file = new FileFFMPEG(this->asset, this);
			}
			else
			{
// PCM file
				fclose(stream);
				return FILE_UNRECOGNIZED_CODEC;
			}   // need more info
			break;

// format already determined
		case FILE_AC3:
			file = new FileAC3(this->asset, this);
			break;

		case FILE_SCENE:
			file = new FileScene(this->asset, this);
			break;

		case FILE_FFMPEG:
			file = new FileFFMPEG(this->asset, this);
			break;

		case FILE_PCM:
		case FILE_WAV:
		case FILE_AU:
		case FILE_AIFF:
		case FILE_SND:
//printf("File::open_file 1\n");
			file = new FileSndFile(this->asset, this);
			break;

		case FILE_PNG:
		case FILE_PNG_LIST:
			file = new FilePNG(this->asset, this);
			break;

		case FILE_JPEG:
		case FILE_JPEG_LIST:
			file = new FileJPEG(this->asset, this);
			break;

		case FILE_GIF:
		case FILE_GIF_LIST:
			file = new FileGIF(this->asset, this);
			break;

		case FILE_EXR:
		case FILE_EXR_LIST:
			file = new FileEXR(this->asset, this);
			break;

		case FILE_FLAC:
			file = new FileFLAC(this->asset, this);
			break;

		case FILE_CR2:
		case FILE_CR2_LIST:
			file = new FileCR2(this->asset, this);
			break;

		case FILE_TGA_LIST:
		case FILE_TGA:
			file = new FileTGA(this->asset, this);
			break;

		case FILE_TIFF:
		case FILE_TIFF_LIST:
			file = new FileTIFF(this->asset, this);
			break;

		case FILE_DB:
			file = new FileDB(this->asset, this);
			break;

		case FILE_YUV:
			file = new FileYUV(this->asset, this);
			break;

		case FILE_MOV:
			file = new FileMOV(this->asset, this);
			break;

		case FILE_MPEG:
		case FILE_AMPEG:
		case FILE_VMPEG:
			file = new FileMPEG(this->asset, this);
			break;

		case FILE_OGG:
			file = new FileOGG(this->asset, this);
			break;

		case FILE_VORBIS:
			file = new FileVorbis(this->asset, this);
			break;

		case FILE_AVI:
			file = new FileMOV(this->asset, this);
			break;

		case FILE_AVI_LAVTOOLS:
		case FILE_AVI_ARNE2:
		case FILE_AVI_ARNE1:
		case FILE_AVI_AVIFILE:
			file = new FileAVI(this->asset, this);
			break;

		case FILE_RAWDV:
			file = new FileDV(this->asset, this);
			break;

// try plugins
		default:
			return 1;
			break;
	}


// Reopen file with correct parser and get header.
	if(file->open_file(rd, wr))
	{
		delete file;
		file = 0;
	}



// Set extra writing parameters to mandatory settings.
	if(file && wr)
	{
		if(this->asset->dither) file->set_dither();
	}



// Synchronize header parameters
	if(file)
	{
		asset->copy_from(this->asset, 1);
//asset->dump();
	}

	if(debug) printf("File::open_file %d file=%p\n", __LINE__, file);
// sleep(1);

	if(file)
		return FILE_OK;
	else
		return FILE_NOT_FOUND;
}

void File::delete_temp_samples_buffer()
{

	if(temp_samples_buffer)
	{
		for(int j = 0; j < audio_ring_buffers; j++)
		{
			for(int i = 0; i < asset->channels; i++)
			{
				delete temp_samples_buffer[j][i];
			}
			delete [] temp_samples_buffer[j];
		}

		delete [] temp_samples_buffer;
		temp_samples_buffer = 0;
		audio_ring_buffers = 0;
	}
}

void File::delete_temp_frame_buffer()
{
	
	if(temp_frame_buffer)
	{
		for(int k = 0; k < video_ring_buffers; k++)
		{
			for(int i = 0; i < asset->layers; i++)
			{
				for(int j = 0; j < video_buffer_size; j++)
				{
					delete temp_frame_buffer[k][i][j];
				}
				delete [] temp_frame_buffer[k][i];
			}
			delete [] temp_frame_buffer[k];
		}

		delete [] temp_frame_buffer;
		temp_frame_buffer = 0;
		video_ring_buffers = 0;
		video_buffer_size = 0;
	}
}

int File::close_file(int ignore_thread)
{
	const int debug = 0;

#ifdef USE_FILEFORK
	if(debug) printf("File::close_file file=%p file_fork=%p %d\n", file, file_fork, __LINE__);

	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::CLOSE_FILE, 0, 0);
		file_fork->read_result();

		if(asset && wr)
		{
			asset->audio_length = current_sample = *(int64_t*)file_fork->result_data;
			asset->video_length = current_frame = *(int64_t*)(file_fork->result_data + sizeof(int64_t));
		}

		if(debug) printf("File::close_file:%d current_sample=" _LD " current_frame=" _LD "\n", 
			__LINE__,
			current_sample,
			current_frame);

		delete file_fork;
		file_fork = 0;
		
	}
#endif

	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);

	if(!ignore_thread)
	{

		stop_audio_thread();

		stop_video_thread();

	}


	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);
	if(file) 
	{
// The file's asset is a copy of the argument passed to open_file so the
// user must copy lengths from the file's asset.
		if(asset && wr)
		{
			asset->audio_length = current_sample;
			asset->video_length = current_frame;
		}

		file->close_file();

		delete file;

	}
	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);

	delete_temp_samples_buffer();
	delete_temp_frame_buffer();
	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);

#ifdef USE_FILEFORK
	delete file_fork;
#endif
	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);

	reset_parameters();
	if(debug) printf("File::close_file file=%p %d\n", file, __LINE__);
	return 0;
}



int File::get_index(char *index_path)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_INDEX, (unsigned char*)index_path, strlen(index_path) + 1);
		int result = file_fork->read_result();
		return result;
	}
#endif

	if(file)
	{
		return file->get_index(index_path);
	}
	return 1;
}



int File::start_audio_thread(int buffer_size, int ring_buffers)
{
	this->audio_ring_buffers = ring_buffers;

#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		unsigned char buffer[sizeof(int) * 2];
		int *ibfr = (int *)buffer;
		ibfr[0] = buffer_size;
		ibfr[1] = audio_ring_buffers;
		file_fork->send_command(FileFork::START_AUDIO_THREAD, buffer, sizeof(buffer));
		int result = file_fork->read_result();


//printf("File::start_audio_thread %d file_fork->result_data=%p\n", __LINE__, file_fork->result_data);
// Create server copy of buffer
		delete_temp_samples_buffer();
//printf("File::start_audio_thread %d\n", __LINE__);
		temp_samples_buffer = new Samples**[audio_ring_buffers];
//printf("File::start_audio_thread %d\n", __LINE__);
		for(int i = 0; i < audio_ring_buffers; i++)
		{
//printf("File::start_audio_thread %d\n", __LINE__);
			temp_samples_buffer[i] = new Samples*[asset->channels];
//printf("File::start_audio_thread %d\n", __LINE__);
			for(int j = 0; j < asset->channels; j++)
			{
				int offset = i * Samples::filefork_size() * asset->channels +
					j * Samples::filefork_size();
//printf("File::start_audio_thread %d j=%d offset=%d\n", __LINE__, j, offset);
				temp_samples_buffer[i][j] = new Samples;
				temp_samples_buffer[i][j]->from_filefork(
					file_fork->result_data +
					offset);
//printf("File::start_audio_thread %d\n", __LINE__);
			}
		}
		
		return result;
	}
#endif

	
	if(!audio_thread)
	{
		audio_thread = new FileThread(this, 1, 0);
		audio_thread->start_writing(buffer_size, 0, ring_buffers, 0);
	}
	return 0;
}

int File::start_video_thread(int buffer_size, 
	int color_model, 
	int ring_buffers, 
	int compressed)
{
	this->video_ring_buffers = ring_buffers;
	this->video_buffer_size = buffer_size;

#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
// This resets variables
		delete_temp_frame_buffer();

		this->video_ring_buffers = ring_buffers;
		this->video_buffer_size = buffer_size;

		unsigned char buffer[sizeof(int) * 4];
		int *ibfr = (int *)buffer;
		ibfr[0] = buffer_size;
		ibfr[1] = color_model;
		ibfr[2] = video_ring_buffers;
		ibfr[3] = compressed;
// Buffers are allocated
		file_fork->send_command(FileFork::START_VIDEO_THREAD, 
			buffer, 
			sizeof(buffer));
		int result = file_fork->read_result();


// Create server copy of buffer
//printf("File::start_video_thread %d %d\n", __LINE__, video_ring_buffers);
		temp_frame_buffer = new VFrame***[video_ring_buffers];
		for(int i = 0; i < video_ring_buffers; i++)
		{
			temp_frame_buffer[i] = new VFrame**[asset->layers];
			for(int j = 0; j < asset->layers; j++)
			{
				temp_frame_buffer[i][j] = new VFrame*[video_buffer_size];
//printf("File::start_video_thread %d %p\n", __LINE__, temp_frame_buffer[i][j]);
				for(int k = 0; k < video_buffer_size; k++)
				{
					temp_frame_buffer[i][j][k] = new VFrame;
					temp_frame_buffer[i][j][k]->from_filefork(file_fork->result_data + 
						i * asset->layers * video_buffer_size * VFrame::filefork_size() + 
						j * video_buffer_size * VFrame::filefork_size() +
						k * VFrame::filefork_size());
				}
			}
		}


		return result;
	}
#endif



	if(!video_thread)
	{
		video_thread = new FileThread(this, 0, 1);
		video_thread->start_writing(buffer_size, 
			color_model, 
			ring_buffers, 
			compressed);
	}
	return 0;
}

int File::start_video_decode_thread()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::START_VIDEO_DECODE_THREAD, 0, 0);
		file_fork->read_result();
		return 0;
	}
#endif


// Currently, CR2 is the only one which won't work asynchronously, so
// we're not using a virtual function yet.
	if(!video_thread /* && asset->format != FILE_CR2 */)
	{
		video_thread = new FileThread(this, 0, 1);
		video_thread->start_reading();
		use_cache = 0;
	}
	return 0;
}

int File::stop_audio_thread()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		file_fork->send_command(FileFork::STOP_AUDIO_THREAD, 0, 0);
		file_fork->read_result();
		return 0;
	}
#endif

	if(audio_thread)
	{
		audio_thread->stop_writing();
		delete audio_thread;
		audio_thread = 0;
	}
	return 0;
}

int File::stop_video_thread()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::STOP_VIDEO_THREAD, 0, 0);
		file_fork->read_result();
		return 0;
	}
#endif

	if(video_thread)
	{
		video_thread->stop_reading();
		video_thread->stop_writing();
		delete video_thread;
		video_thread = 0;
	}
	return 0;
}

FileThread* File::get_video_thread()
{
	return video_thread;
}

int File::set_channel(int channel) 
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
// Set it locally for get_channel
		current_channel = channel;
		file_fork->send_command(FileFork::SET_CHANNEL, (unsigned char*)&channel, sizeof(channel));
		int result = file_fork->read_result();
		return result;
	}
#endif

	if(file && channel < asset->channels)
	{
		current_channel = channel;
		return 0;
	}
	else
		return 1;
}

int File::get_channel()
{
	return current_channel;
}

// if no>=0, sets new program
//  returns current program
int File::set_program(int no)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_PROGRAM, (unsigned char*)&no, sizeof(no));
		int result = file_fork->read_result();
		current_program = no < 0 ? result : no;
		return result;
	}
#endif
	int result = file ? file->set_program(no) : current_program;
	current_program = no < 0 ? result : no;
	return result;
}

int File::get_cell_time(int no, double &time)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_CELL_TIME, (unsigned char*)&no, sizeof(no));
		int result = file_fork->read_result();
		time = *(double *)file_fork->result_data;
		return result;
	}
#endif

	return file ? file->get_cell_time(no, time) : -1;
}

int File::get_system_time(int64_t &tm)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_STT_TIME, 0, 0);
		int result = file_fork->read_result();
		tm = *(int64_t *)file_fork->result_data;
		return result;
	}
#endif

	return file ? file->get_system_time(tm) : -1;
}

int File::get_audio_for_video(int stream, int64_t &channels, int layer)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		unsigned char buffer[2*sizeof(int)];
		int offset = 0;
		*(int*)(buffer + offset) = stream;
		offset += sizeof(int);
		*(int*)(buffer + offset) = layer;
		file_fork->send_command(FileFork::GET_AUDIO4VIDEO, buffer, sizeof(buffer));
		int result = file_fork->read_result();
		channels = *(int64_t *)file_fork->result_data;
		return result;
	}
#endif

	return file ? file->get_audio_for_video(stream, channels, layer) : -1;
}

int File::get_video_pid(int track)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_VIDEO_PID,
				(unsigned char*)&track, sizeof(track));
		int result = file_fork->read_result();
		return result;
	}
#endif

	return file ? file->get_video_pid(track) : -1;
}



int File::get_video_info(int track, int &pid, double &framerate,
		int &width, int &height, char *title)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_VIDEO_INFO,
				(unsigned char*)&track, sizeof(track));
		int result = file_fork->read_result();
		if( !result ) {
			unsigned char *bp = file_fork->result_data;
			framerate = *(double*)bp;  bp += sizeof(framerate);
			pid       = *(int *)  bp;  bp += sizeof(pid);
			width     = *(int *)  bp;  bp += sizeof(width);
			height    = *(int *)  bp;  bp += sizeof(height);
			strcpy(title, (char *)bp);
		}
		return result;
	}
#endif

	return !file ? -1 :
		 file->get_video_info(track, pid, framerate, width, height, title);
}


int File::get_thumbnail(int stream,
	int64_t &position, unsigned char *&thumbnail, int &ww, int &hh)
{
	return file->get_thumbnail(stream, position, thumbnail, ww, hh);
}

int File::set_skimming(int track, int skim, skim_fn fn, void *vp)
{
	return file->set_skimming(track, skim, fn, vp);
}

int File::skim_video(int track, void *vp, skim_fn fn)
{
	return file->skim_video(track, vp, fn);
}


int File::set_layer(int layer, int is_thread) 
{
#ifdef USE_FILEFORK
// thread should only call in the fork
	if(file_fork && !is_fork && !is_thread)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_LAYER, (unsigned char*)&layer, sizeof(layer));
		int result = file_fork->read_result();
		current_layer = layer;
		return result;
	}
#endif

	if(file && layer < asset->layers)
	{
		if(!is_thread && video_thread)
		{
			video_thread->set_layer(layer);
		}
		else
		{
			current_layer = layer;
		}
		return 0; 
	}
	else
		return 1;
}

int64_t File::get_audio_length()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_AUDIO_LENGTH, 0, 0);
		int64_t result = file_fork->read_result();
		return result;
	}
#endif

	int64_t result = asset->audio_length;
	int64_t base_samplerate = -1;
	if(result > 0)
	{
		if(base_samplerate > 0)
			return (int64_t)((double)result / asset->sample_rate * base_samplerate + 0.5);
		else
			return result;
	}
	else
		return -1;
}

int64_t File::get_video_length()
{ 
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_VIDEO_LENGTH, 0, 0);
		int64_t result = file_fork->read_result();
		return result;
	}
#endif


	int64_t result = asset->video_length;
	float base_framerate = -1;
	if(result > 0)
	{
		if(base_framerate > 0)
			return (int64_t)((double)result / asset->frame_rate * base_framerate + 0.5); 
		else
			return result;
	}
	else
		return -1;  // infinity
}


int64_t File::get_video_position() 
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_VIDEO_POSITION, 0, 0);
		int64_t result = file_fork->read_result();
		return result;
	}
#endif

	float base_framerate = -1;
	if(base_framerate > 0)
		return (int64_t)((double)current_frame / asset->frame_rate * base_framerate + 0.5);
	else
		return current_frame;
}

int64_t File::get_audio_position() 
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_AUDIO_POSITION, 0, 0);
		int64_t result = file_fork->read_result();
		return result;
	}
#endif


// 	int64_t base_samplerate = -1;
// 	if(base_samplerate > 0)
// 	{
// 		if(normalized_sample_rate == base_samplerate)
// 			return normalized_sample;
// 		else
// 			return (int64_t)((double)current_sample / 
// 				asset->sample_rate * 
// 				base_samplerate + 
// 				0.5);
// 	}
// 	else
		return current_sample;
}



int File::set_audio_position(int64_t position) 
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::SET_AUDIO_POSITION, 
			(unsigned char*)&position, 
			sizeof(position));
		int result = file_fork->read_result();
		return result;
	}
#endif

	int result = 0;

	if(!file) return 1;

#define REPOSITION(x, y) \
	(labs((x) - (y)) > 1)

	float base_samplerate = asset->sample_rate;
		current_sample = normalized_sample = position;

// printf("File::set_audio_position %d normalized_sample=%ld\n", 
// __LINE__, 
// normalized_sample);
		result = file->set_audio_position(current_sample);

		if(result)
			printf("File::set_audio_position position=" _LD
				" base_samplerate=%f asset=%p asset->sample_rate=%d\n",
				position, base_samplerate, asset, asset->sample_rate);
//	}

//printf("File::set_audio_position %d %d %d\n", current_channel, current_sample, position);

	return result;
}

int File::set_video_position(int64_t position, 
	int is_thread) 
{
#ifdef USE_FILEFORK
// Thread should only call in the fork
	if(file_fork && !is_fork && !is_thread)
	{
		FileForker this_is(*forked);
//printf("File::set_video_position %d %lld\n", __LINE__, position);
		file_fork->send_command(FileFork::SET_VIDEO_POSITION, (unsigned char*)&position, sizeof(position));
		int result = file_fork->read_result();
		return result;
	}
#endif

	int result = 0;
	if(!file) return 0;

// Convert to file's rate
// 	if(base_framerate > 0)
// 		position = (int64_t)((double)position / 
// 			base_framerate * 
// 			asset->frame_rate + 
// 			0.5);


	if(video_thread && !is_thread)
	{
// Call thread.  Thread calls this again to set the file state.
		video_thread->set_video_position(position);
	}
	else
	if(current_frame != position)
	{
		if(file)
		{
			current_frame = position;
			result = file->set_video_position(current_frame);
		}
	}

	return result;
}

// No resampling here.
int File::write_samples(Samples **buffer, int64_t len)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		int entry_size = Samples::filefork_size();
		int buffer_size = entry_size * asset->channels + sizeof(int64_t);
		unsigned char fork_buffer[buffer_size];
		for(int i = 0; i < asset->channels; i++)
		{
			buffer[i]->to_filefork(fork_buffer + entry_size * i);
		}

		*(int64_t*)(fork_buffer + 
			entry_size * asset->channels) = len;

		file_fork->send_command(FileFork::WRITE_SAMPLES, 
			fork_buffer, 
			buffer_size);
		int result = file_fork->read_result();
		return result;
	}
#endif




	int result = 1;

	if(file)
	{
		write_lock->lock("File::write_samples");

// Convert to arrays for backwards compatability
		double *temp[asset->channels];
		for(int i = 0; i < asset->channels; i++)
		{
			temp[i] = buffer[i]->get_data();
		}

		result = file->write_samples(temp, len);
		current_sample += len;
		normalized_sample += len;
		asset->audio_length += len;
		write_lock->unlock();
	}
	return result;
}





// Can't put any cmodel abstraction here because the filebase couldn't be
// parallel.
int File::write_frames(VFrame ***frames, int len)
{
//printf("File::write_frames %d\n", __LINE__);
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
//printf("File::write_frames %d\n", __LINE__);
		int entry_size = frames[0][0]->filefork_size();
		unsigned char fork_buffer[entry_size * asset->layers * len + sizeof(int)];
		for(int i = 0; i < asset->layers; i++)
		{
			for(int j = 0; j < len; j++)
			{
// printf("File::write_frames %d " _LD " %d\n", 
// __LINE__, 
// frames[i][j]->get_number(), 
// frames[i][j]->get_keyframe());
				frames[i][j]->to_filefork(fork_buffer + 
					sizeof(int) +
					entry_size * len * i +
					entry_size * j);
			}
		}

		
//PRINT_TRACE
// Frames per layer
		int *fbfr = (int *)fork_buffer;
		fbfr[0] = len;
//PRINT_TRACE

		file_fork->send_command(FileFork::WRITE_FRAMES, 
			fork_buffer, 
			sizeof(fork_buffer));
//PRINT_TRACE
		int result = file_fork->read_result();


//printf("File::write_frames %d\n", __LINE__);
		return result;
	}


#endif // USE_FILEFORK


//PRINT_TRACE
// Store the counters in temps so the filebase can choose to overwrite them.
	int result;
	int current_frame_temp = current_frame;
	int video_length_temp = asset->video_length;

	write_lock->lock("File::write_frames");

//PRINT_TRACE
	result = file->write_frames(frames, len);
//PRINT_TRACE

	current_frame = current_frame_temp + len;
	asset->video_length = video_length_temp + len;
	write_lock->unlock();
//PRINT_TRACE
	return result;
}

// Only called by FileThread
int File::write_compressed_frame(VFrame *buffer)
{
	int result = 0;
	write_lock->lock("File::write_compressed_frame");
	result = file->write_compressed_frame(buffer);
	current_frame++;
	asset->video_length++;
	write_lock->unlock();
	return result;
}


int File::write_audio_buffer(int64_t len)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::WRITE_AUDIO_BUFFER, (unsigned char*)&len, sizeof(len));
		int result = file_fork->read_result();
		return result;
	}
#endif

	int result = 0;
	if(audio_thread)
	{
		result = audio_thread->write_buffer(len);
	}
	return result;
}

int File::write_video_buffer(int64_t len)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
// Copy over sequence numbers for background rendering
// frame sizes for direct copy
//printf("File::write_video_buffer %d\n", __LINE__);
		int fork_buffer_size = sizeof(int64_t) +
			VFrame::filefork_size() * asset->layers * len;
		unsigned char fork_buffer[fork_buffer_size];
		int64_t *fbfr = (int64_t *)fork_buffer;
		fbfr[0] = len;

		for(int i = 0; i < asset->layers; i++)
		{
			for(int j = 0; j < len; j++)
			{
// Send memory state
				current_frame_buffer[i][j]->to_filefork(fork_buffer + 
					sizeof(int64_t) +
					VFrame::filefork_size() * (len * i + j));
// printf("File::write_video_buffer %d size=%d %d %02x %02x %02x %02x %02x %02x %02x %02x\n", 
// __LINE__, 
// current_frame_buffer[i][j]->get_shmid(),
// current_frame_buffer[i][j]->get_compressed_size(),
// current_frame_buffer[i][j]->get_data()[0],
// current_frame_buffer[i][j]->get_data()[1],
// current_frame_buffer[i][j]->get_data()[2],
// current_frame_buffer[i][j]->get_data()[3],
// current_frame_buffer[i][j]->get_data()[4],
// current_frame_buffer[i][j]->get_data()[5],
// current_frame_buffer[i][j]->get_data()[6],
// current_frame_buffer[i][j]->get_data()[7]);
			}
		}

//printf("File::write_video_buffer %d\n", __LINE__);
		file_fork->send_command(FileFork::WRITE_VIDEO_BUFFER, 
			fork_buffer, 
			fork_buffer_size);
//printf("File::write_video_buffer %d\n", __LINE__);
		int result = file_fork->read_result();
//printf("File::write_video_buffer %d\n", __LINE__);
		return result;
	}
#endif

	int result = 0;
	if(video_thread)
	{
		result = video_thread->write_buffer(len);
	}

	return result;
}

Samples** File::get_audio_buffer()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		file_fork->send_command(FileFork::GET_AUDIO_BUFFER, 0, 0);
		int result = file_fork->read_result();

// Read parameters for a Samples buffer & create it in File
//		delete_temp_samples_buffer();
// 		if(!temp_samples_buffer) 
// 		{
// 			temp_samples_buffer = new Samples**[ring_buffers];
// 			for(int i = 0; i < ring_buffers; i++) temp_samples_buffer[i] = 0;
// 		}
// 		
// 		
// 		temp_samples_buffer  = new Samples*[asset->channels];
// 		for(int i = 0; i < asset->channels; i++)
// 		{
// 			temp_samples_buffer[i] = new Samples;
// 			temp_samples_buffer[i]->from_filefork(file_fork->result_data + 
// 				i * Samples::filefork_size());
// 		}

		return temp_samples_buffer[result];
	}
#endif

	if(audio_thread) return audio_thread->get_audio_buffer();
	return 0;
}

VFrame*** File::get_video_buffer()
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);

		file_fork->send_command(FileFork::GET_VIDEO_BUFFER, 0, 0);
		int result = file_fork->read_result();

// Read parameters for a VFrame buffer & create it in File
//		delete_temp_frame_buffer();


// 		temp_frame_size = *(int*)(file_fork->result_data + 
// 			file_fork->result_bytes - 
// 			sizeof(int));
// 
// //printf("File::get_video_buffer %d %p %d\n", __LINE__, this, asset->layers);
// 		temp_frame_buffer = new VFrame**[asset->layers];
// 
// 		for(int i = 0; i < asset->layers; i++)
// 		{
// 
// 			temp_frame_buffer[i] = new VFrame*[temp_frame_size];
// 
// 			for(int j = 0; j < temp_frame_size; j++)
// 			{
// 
// 				temp_frame_buffer[i][j] = new VFrame;
// printf("File::get_video_buffer %d %p\n", __LINE__, temp_frame_buffer[i][j]);
// 
// 				temp_frame_buffer[i][j]->from_filefork(file_fork->result_data + 
// 					i * temp_frame_size * VFrame::filefork_size() +
// 					j * VFrame::filefork_size());
// 
// 			}
// 		}
// 

		current_frame_buffer = temp_frame_buffer[result];

		return current_frame_buffer;
	}
#endif

	if(video_thread) 
	{
		VFrame*** result = video_thread->get_video_buffer();

		return result;
	}

	return 0;
}


int File::read_samples(Samples *samples, int64_t len)
{
// Never try to read more samples than exist in the file
	if (current_sample + len > asset->audio_length) {
		len = asset->audio_length - current_sample;
	}
	if(len <= 0) return 0;

	int result = 0;
	const int debug = 0;
	if(debug) PRINT_TRACE

#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		int buffer_bytes = Samples::filefork_size() + sizeof(int64_t);
		unsigned char buffer[buffer_bytes];
		samples->to_filefork(buffer);
		*(int64_t*)(buffer + Samples::filefork_size()) = len;
		if(debug) PRINT_TRACE
		file_fork->send_command(FileFork::READ_SAMPLES, 
			buffer, 
			buffer_bytes);
		if(debug) PRINT_TRACE
		int result = file_fork->read_result();

// Crashed
		if(result && !file_fork->child_running())
		{
			delete file_fork;
			result = open_file(preferences, asset, rd, wr);
		}

		return result;
	}
#endif

	if(debug) PRINT_TRACE

	double *buffer = samples->get_data();

	int64_t base_samplerate = asset->sample_rate;

	if(file)
	{
// Resample recursively calls this with the asset sample rate
		if(base_samplerate == 0) base_samplerate = asset->sample_rate;

		if(debug) PRINT_TRACE
		result = file->read_samples(buffer, len);

		if(debug) PRINT_TRACE
		current_sample += len;

		normalized_sample += len;
	}
	if(debug) PRINT_TRACE

	return result;
}


int File::read_frame(VFrame *frame, int is_thread)
{
	const int debug = 0;

	if(debug) PRINT_TRACE

#ifdef USE_FILEFORK
// is_thread is only true in the fork
	if(file_fork && !is_fork && !is_thread)
	{
		FileForker this_is(*forked);
		unsigned char fork_buffer[VFrame::filefork_size()];
		if(debug) PRINT_TRACE

		frame->to_filefork(fork_buffer);
		file_fork->send_command(FileFork::READ_FRAME, 
			fork_buffer, 
			VFrame::filefork_size());

		int result = file_fork->read_result();


// Crashed
		if(result && !file_fork->child_running())
		{
			delete file_fork;
			result = open_file(preferences, asset, rd, wr);
		}
		else
		if(!result && 
			frame->get_color_model() == BC_COMPRESSED)
		{
// Get compressed data from socket
//printf("File::read_frame %d %d\n", __LINE__, file_fork->result_bytes);
			int header_size = sizeof(int) * 2;
			if(file_fork->result_bytes > header_size)
			{
//printf("File::read_frame %d %d\n", __LINE__, file_fork->result_bytes);
				frame->allocate_compressed_data(file_fork->result_bytes - header_size);
				frame->set_compressed_size(file_fork->result_bytes - header_size);
				frame->set_keyframe(*(int*)(file_fork->result_data + sizeof(int)));
				memcpy(frame->get_data(), 
					file_fork->result_data + header_size,
					file_fork->result_bytes - header_size);
			}
			else
// Get compressed data size
			{
				frame->set_compressed_size(*(int*)file_fork->result_data);
				frame->set_keyframe(*(int*)(file_fork->result_data + sizeof(int)));
//printf("File::read_frame %d %d\n", __LINE__, *(int*)(file_fork->result_data + sizeof(int)));
			}
		}

		return result;
	}
#endif


//printf("File::read_frame %d\n", __LINE__);

	if(video_thread && !is_thread) return video_thread->read_frame(frame);

//printf("File::read_frame %d\n", __LINE__);
	if(debug) PRINT_TRACE
	if(file)
	{
		if(debug) PRINT_TRACE
		int supported_colormodel = colormodel_supported(frame->get_color_model());
		int advance_position = 1;

// Test cache
		if(use_cache && !is_fork &&
			frame_cache->get_frame(frame,
				current_frame,
				current_layer,
				asset->frame_rate))
		{
// Can't advance position if cache used.
//printf("File::read_frame %d\n", __LINE__);
			advance_position = 0;
		}
		else
// Need temp
		if(frame->get_color_model() != BC_COMPRESSED &&
			(supported_colormodel != frame->get_color_model() ||
			frame->get_w() != asset->width ||
			frame->get_h() != asset->height))
		{

//			printf("File::read_frame %d\n", __LINE__);
// Can't advance position here because it needs to be added to cache
			if(temp_frame)
			{
				if(!temp_frame->params_match(asset->width, asset->height, supported_colormodel))
				{
					delete temp_frame;
					temp_frame = 0;
				}
			}

//			printf("File::read_frame %d\n", __LINE__);
			if(!temp_frame)
			{
				temp_frame = new VFrame(0,
					-1,
					asset->width,
					asset->height,
					supported_colormodel,
					-1);
			}

//			printf("File::read_frame %d\n", __LINE__);
			temp_frame->copy_stacks(frame);
			file->read_frame(temp_frame);
//for(int i = 0; i < 1000 * 1000; i++) ((float*)temp_frame->get_rows()[0])[i] = 1.0;
// printf("File::read_frame %d %d %d %d %d %d\n", 
// temp_frame->get_color_model(), 
// temp_frame->get_w(),
// temp_frame->get_h(),
// frame->get_color_model(),
// frame->get_w(),
// frame->get_h());
			cmodel_transfer(frame->get_rows(), 
				temp_frame->get_rows(),
				frame->get_y(),
				frame->get_u(),
				frame->get_v(),
				temp_frame->get_y(),
				temp_frame->get_u(),
				temp_frame->get_v(),
				0, 
				0, 
				temp_frame->get_w(), 
				temp_frame->get_h(),
				0, 
				0, 
				frame->get_w(), 
				frame->get_h(),
				temp_frame->get_color_model(), 
				frame->get_color_model(),
				0,
				temp_frame->get_w(),
				frame->get_w());
//			printf("File::read_frame %d\n", __LINE__);
		}
		else
		{
// Can't advance position here because it needs to be added to cache
//printf("File::read_frame %d\n", __LINE__);
			file->read_frame(frame);
//for(int i = 0; i < 100 * 1000; i++) ((float*)frame->get_rows()[0])[i] = 1.0;
		}

//printf("File::read_frame %d use_cache=%d\n", __LINE__, use_cache);
		if(use_cache && !is_fork)
			frame_cache->put_frame(frame,
				current_frame, current_layer,
				asset->frame_rate, 1, 0);
//printf("File::read_frame %d\n", __LINE__);

		if(advance_position) current_frame++;
		if(debug) PRINT_TRACE
		return 0;
	}
	else
		return 1;
}

int File::can_copy_from(Asset *asset, 
	int64_t position, 
	int output_w, 
	int output_h)
{
	if(!asset) return 0;

#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		FileXML xml;
		asset->write(&xml, 1, "");
		xml.terminate_string();
		const char *xml_string = xml.string();
		long xml_length = strlen(xml_string);
		int buffer_size = xml_length + 1 + 
			sizeof(int64_t) +
			sizeof(int) + 
			sizeof(int);
		unsigned char *buffer = new unsigned char[buffer_size];
		*(int64_t*)(buffer) = position;
		*(int*)(buffer + sizeof(int64_t)) = output_w;
		*(int*)(buffer + sizeof(int64_t) + sizeof(int)) = output_h;
		memcpy(buffer + 
			sizeof(int64_t) +
			sizeof(int) + 
			sizeof(int), 
			xml_string, 
			xml_length + 1);

		file_fork->send_command(FileFork::CAN_COPY_FROM, 
			buffer, 
			buffer_size);
		int result = file_fork->read_result();
		return result;
	}
#endif


	if(file)
	{
		return asset->width == output_w &&
			asset->height == output_h &&
			file->can_copy_from(asset, position);
	}
	else
		return 0;
}

// Fill in queries about formats when adding formats here.


int File::strtoformat(const char *format)
{
	return strtoformat(0, format);
}

int File::strtoformat(ArrayList<PluginServer*> *plugindb, const char *format)
{
	if(!strcasecmp(format, _(AC3_NAME))) return FILE_AC3;
	if(!strcasecmp(format, _(SCENE_NAME))) return FILE_SCENE;
	if(!strcasecmp(format, _(WAV_NAME))) return FILE_WAV;
	if(!strcasecmp(format, _(PCM_NAME))) return FILE_PCM;
	if(!strcasecmp(format, _(AU_NAME))) return FILE_AU;
	if(!strcasecmp(format, _(AIFF_NAME))) return FILE_AIFF;
	if(!strcasecmp(format, _(SND_NAME))) return FILE_SND;
	if(!strcasecmp(format, _(PNG_NAME))) return FILE_PNG;
	if(!strcasecmp(format, _(PNG_LIST_NAME))) return FILE_PNG_LIST;
	if(!strcasecmp(format, _(TIFF_NAME))) return FILE_TIFF;
	if(!strcasecmp(format, _(TIFF_LIST_NAME))) return FILE_TIFF_LIST;
	if(!strcasecmp(format, _(JPEG_NAME))) return FILE_JPEG;
	if(!strcasecmp(format, _(JPEG_LIST_NAME))) return FILE_JPEG_LIST;
	if(!strcasecmp(format, _(EXR_NAME))) return FILE_EXR;
	if(!strcasecmp(format, _(EXR_LIST_NAME))) return FILE_EXR_LIST;
	if(!strcasecmp(format, _(YUV_NAME))) return FILE_YUV;
	if(!strcasecmp(format, _(FLAC_NAME))) return FILE_FLAC;
	if(!strcasecmp(format, _(CR2_NAME))) return FILE_CR2;
	if(!strcasecmp(format, _(CR2_LIST_NAME))) return FILE_CR2_LIST;
	if(!strcasecmp(format, _(MPEG_NAME))) return FILE_MPEG;
	if(!strcasecmp(format, _(AMPEG_NAME))) return FILE_AMPEG;
	if(!strcasecmp(format, _(VMPEG_NAME))) return FILE_VMPEG;
	if(!strcasecmp(format, _(TGA_NAME))) return FILE_TGA;
	if(!strcasecmp(format, _(TGA_LIST_NAME))) return FILE_TGA_LIST;
	if(!strcasecmp(format, _(MOV_NAME))) return FILE_MOV;
	if(!strcasecmp(format, _(AVI_NAME))) return FILE_AVI;
	if(!strcasecmp(format, _(AVI_LAVTOOLS_NAME))) return FILE_AVI_LAVTOOLS;
	if(!strcasecmp(format, _(AVI_ARNE2_NAME))) return FILE_AVI_ARNE2;
	if(!strcasecmp(format, _(AVI_ARNE1_NAME))) return FILE_AVI_ARNE1;
	if(!strcasecmp(format, _(AVI_AVIFILE_NAME))) return FILE_AVI_AVIFILE;
	if(!strcasecmp(format, _(OGG_NAME))) return FILE_OGG;
	if(!strcasecmp(format, _(VORBIS_NAME))) return FILE_VORBIS;
	if(!strcasecmp(format, _(RAWDV_NAME))) return FILE_RAWDV;
	if(!strcasecmp(format, _(FFMPEG_NAME))) return FILE_FFMPEG;
	if(!strcasecmp(format, _(DBASE_NAME))) return FILE_DB;

	return 0;
}


const char* File::formattostr(int format)
{
	return formattostr(0, format);
}

const char* File::formattostr(ArrayList<PluginServer*> *plugindb, int format)
{
	switch(format)
	{
		case FILE_SCENE:	return _(SCENE_NAME);
		case FILE_AC3:		return _(AC3_NAME);
		case FILE_WAV:		return _(WAV_NAME);
		case FILE_PCM:		return _(PCM_NAME);
		case FILE_AU:		return _(AU_NAME);
		case FILE_AIFF:		return _(AIFF_NAME);
		case FILE_SND:		return _(SND_NAME);
		case FILE_PNG:		return _(PNG_NAME);
		case FILE_PNG_LIST:	return _(PNG_LIST_NAME);
		case FILE_JPEG:		return _(JPEG_NAME);
		case FILE_JPEG_LIST:	return _(JPEG_LIST_NAME);
		case FILE_CR2:		return _(CR2_NAME);
		case FILE_CR2_LIST:	return _(CR2_LIST_NAME);
		case FILE_FLAC:		return _(FLAC_NAME);
		case FILE_EXR_LIST:	return _(EXR_LIST_NAME);
		case FILE_YUV:		return _(YUV_NAME);
		case FILE_MPEG:		return _(MPEG_NAME);
		case FILE_AMPEG:	return _(AMPEG_NAME);
		case FILE_VMPEG:	return _(VMPEG_NAME);
		case FILE_TGA:		return _(TGA_NAME);
		case FILE_TGA_LIST:	return _(TGA_LIST_NAME);
		case FILE_TIFF:		return _(TIFF_NAME);
		case FILE_TIFF_LIST:	return _(TIFF_LIST_NAME);
		case FILE_MOV:		return _(MOV_NAME);
		case FILE_AVI_LAVTOOLS:	return _(AVI_LAVTOOLS_NAME);
		case FILE_AVI:		return _(AVI_NAME);
		case FILE_AVI_ARNE2:	return _(AVI_ARNE2_NAME);
		case FILE_AVI_ARNE1:	return _(AVI_ARNE1_NAME);
		case FILE_AVI_AVIFILE:	return _(AVI_AVIFILE_NAME);
		case FILE_OGG:		return _(OGG_NAME);
		case FILE_VORBIS:	return _(VORBIS_NAME);
		case FILE_RAWDV:	return _(RAWDV_NAME);
		case FILE_FFMPEG:	return _(FFMPEG_NAME);
		case FILE_DB:		return _(DBASE_NAME);
	}
	return "Unknown";
}

int File::strtobits(const char *bits)
{
	if(!strcasecmp(bits, _(NAME_8BIT))) return BITSLINEAR8;
	if(!strcasecmp(bits, _(NAME_16BIT))) return BITSLINEAR16;
	if(!strcasecmp(bits, _(NAME_24BIT))) return BITSLINEAR24;
	if(!strcasecmp(bits, _(NAME_32BIT))) return BITSLINEAR32;
	if(!strcasecmp(bits, _(NAME_ULAW))) return BITSULAW;
	if(!strcasecmp(bits, _(NAME_ADPCM))) return BITS_ADPCM;
	if(!strcasecmp(bits, _(NAME_FLOAT))) return BITSFLOAT;
	if(!strcasecmp(bits, _(NAME_IMA4))) return BITSIMA4;
	return BITSLINEAR16;
}

const char* File::bitstostr(int bits)
{
//printf("File::bitstostr\n");
	switch(bits)
	{
		case BITSLINEAR8:	return (NAME_8BIT);
		case BITSLINEAR16:	return (NAME_16BIT);
		case BITSLINEAR24:	return (NAME_24BIT);
		case BITSLINEAR32:	return (NAME_32BIT);
		case BITSULAW:		return (NAME_ULAW);
		case BITS_ADPCM:	return (NAME_ADPCM);
		case BITSFLOAT:		return (NAME_FLOAT);
		case BITSIMA4:		return (NAME_IMA4);
	}
	return "Unknown";
}



int File::str_to_byteorder(const char *string)
{
	if(!strcasecmp(string, _("Lo Hi"))) return 1;
	return 0;
}

const char* File::byteorder_to_str(int byte_order)
{
	if(byte_order) return _("Lo Hi");
	return _("Hi Lo");
}

int File::bytes_per_sample(int bits)
{
	switch(bits)
	{
		case BITSLINEAR8:	return 1;
		case BITSLINEAR16:	return 2;
		case BITSLINEAR24:	return 3;
		case BITSLINEAR32:	return 4;
		case BITSULAW:		return 1;
		case BITSIMA4:		return 1;
	}
	return 1;
}





int File::get_best_colormodel(int driver)
{
	return get_best_colormodel(asset, driver);
}

int File::get_best_colormodel(Asset *asset, int driver)
{
	switch(asset->format)
	{
		case FILE_RAWDV:	return FileDV::get_best_colormodel(asset, driver);
		case FILE_MOV:		return FileMOV::get_best_colormodel(asset, driver);
		case FILE_AVI:		return FileMOV::get_best_colormodel(asset, driver);
		case FILE_MPEG:		return FileMPEG::get_best_colormodel(asset, driver);
		case FILE_JPEG:
		case FILE_JPEG_LIST:	return FileJPEG::get_best_colormodel(asset, driver);
		case FILE_EXR:
		case FILE_EXR_LIST:	return FileEXR::get_best_colormodel(asset, driver);
		case FILE_YUV:		return FileYUV::get_best_colormodel(asset, driver);
		case FILE_PNG:
		case FILE_PNG_LIST:	return FilePNG::get_best_colormodel(asset, driver);
		case FILE_TGA:
		case FILE_TGA_LIST:	return FileTGA::get_best_colormodel(asset, driver);
		case FILE_CR2:
		case FILE_CR2_LIST:	return FileCR2::get_best_colormodel(asset, driver);
		case FILE_DB:		return FileDB::get_best_colormodel(asset, driver);
	}

	return BC_RGB888;
}


int File::colormodel_supported(int colormodel)
{
#ifdef USE_FILEFORK
	if(file_fork)
	{
		FileForker this_is(*forked);
		unsigned char buffer[sizeof(int)];
		int *ibfr = (int *)buffer;
		ibfr[0] = colormodel;

		file_fork->send_command(FileFork::COLORMODEL_SUPPORTED, 
			buffer, 
			sizeof(int));
		int result = file_fork->read_result();
		return result;
	}
#endif


	if(file)
		return file->colormodel_supported(colormodel);

	return BC_RGB888;
}


int64_t File::file_memory_usage()
{
	return file ? file->base_memory_usage() : 0;
}

int64_t File::get_memory_usage() 
{
	int64_t result = 0;

#ifdef USE_FILEFORK
	if(file_fork)
 	{
		FileForker this_is(*forked);
 		file_fork->send_command(FileFork::FILE_MEMORY_USAGE, 0, 0);
 		result = file_fork->read_result();
 	}
	else
#endif
	result += file_memory_usage();
	if(temp_frame) result += temp_frame->get_data_size();
	result += frame_cache->get_memory_usage();
	if(video_thread) result += video_thread->get_memory_usage();

	if(result < MIN_CACHEITEM_SIZE) result = MIN_CACHEITEM_SIZE;
	return result;
}


int File::supports_video(ArrayList<PluginServer*> *plugindb, char *format)
{
	int format_i = strtoformat(plugindb, format);
	
	return supports_video(format_i);
	return 0;
}

int File::supports_audio(ArrayList<PluginServer*> *plugindb, char *format)
{
	int format_i = strtoformat(plugindb, format);

	return supports_audio(format_i);
	return 0;
}


int File::supports_video(int format)
{
//printf("File::supports_video %d\n", format);
	switch(format)
	{
		case FILE_OGG:
		case FILE_MOV:
		case FILE_JPEG:
		case FILE_JPEG_LIST:
		case FILE_CR2:
		case FILE_CR2_LIST:
		case FILE_EXR:
		case FILE_EXR_LIST:
		case FILE_PNG:
		case FILE_PNG_LIST:
		case FILE_TGA:
		case FILE_TGA_LIST:
		case FILE_TIFF:
		case FILE_TIFF_LIST:
		case FILE_VMPEG:
		case FILE_AVI_LAVTOOLS:
		case FILE_AVI_ARNE2:
		case FILE_AVI:
		case FILE_AVI_ARNE1:
		case FILE_AVI_AVIFILE:
	        case FILE_YUV:
		case FILE_DB:
		case FILE_RAWDV:
			return 1;
	}
	return 0;
}

int File::supports_audio(int format)
{
	switch(format)
	{
		case FILE_AC3:
		case FILE_FLAC:
		case FILE_PCM:
		case FILE_WAV:
		case FILE_MOV:
		case FILE_OGG:
		case FILE_VORBIS:
		case FILE_AMPEG:
		case FILE_AU:
		case FILE_AIFF:
		case FILE_SND:
		case FILE_AVI:
		case FILE_AVI_LAVTOOLS:
		case FILE_AVI_ARNE2:
		case FILE_AVI_ARNE1:
		case FILE_AVI_AVIFILE:
			return 1;
	}
	return 0;
}

const char* File::get_tag(int format)
{
	switch(format)
	{
		case FILE_AC3:          return "ac3";
		case FILE_AIFF:         return "aif";
		case FILE_AMPEG:        return "mp3";
		case FILE_AU:           return "au";
		case FILE_AVI:          return "avi";
		case FILE_RAWDV:        return "dv";
		case FILE_DB:           return "db";
		case FILE_EXR:          return "exr";
		case FILE_EXR_LIST:     return "exr";
		case FILE_FLAC:         return "flac";
		case FILE_JPEG:         return "jpg";
		case FILE_JPEG_LIST:    return "jpg";
		case FILE_MOV:          return "mov/mp4";
		case FILE_OGG:          return "ogg";
		case FILE_PCM:          return "pcm";
		case FILE_PNG:          return "png";
		case FILE_PNG_LIST:     return "png";
		case FILE_TGA:          return "tga";
		case FILE_TGA_LIST:     return "tga";
		case FILE_TIFF:         return "tif";
		case FILE_TIFF_LIST:    return "tif";
		case FILE_VMPEG:        return "m2v";
		case FILE_VORBIS:       return "ogg";
		case FILE_WAV:          return "wav";
		case FILE_YUV:          return "m2v";
	}
	return 0;
}

const char* File::get_prefix(int format)
{
	switch(format) {
	case FILE_PCM:		return "PCM";
	case FILE_WAV:		return "WAV";
	case FILE_MOV:		return "MOV";
	case FILE_PNG:		return "PNG";
	case FILE_JPEG:		return "JPEG";
	case FILE_TIFF:		return "TIFF";
	case FILE_GIF:		return "GIF";
	case FILE_JPEG_LIST:	return "JPEG_LIST";
	case FILE_AU:		return "AU";
	case FILE_AIFF:		return "AIFF";
	case FILE_SND:		return "SND";
	case FILE_AVI_LAVTOOLS:	return "AVI_LAVTOOLS";
	case FILE_TGA_LIST:	return "TGA_LIST";
	case FILE_TGA:		return "TGA";
	case FILE_MPEG:		return "MPEG";
	case FILE_AMPEG:	return "AMPEG";
	case FILE_VMPEG:	return "VMPEG";
	case FILE_RAWDV:	return "RAWDV";
	case FILE_AVI_ARNE2:	return "AVI_ARNE2";
	case FILE_AVI_ARNE1:	return "AVI_ARNE1";
	case FILE_AVI_AVIFILE:	return "AVI_AVIFILE";
	case FILE_TIFF_LIST:	return "TIFF_LIST";
	case FILE_PNG_LIST:	return "PNG_LIST";
	case FILE_AVI:		return "AVI";
	case FILE_AC3:		return "AC3";
	case FILE_EXR:		return "EXR";
	case FILE_EXR_LIST:	return "EXR_LIST";
	case FILE_CR2:		return "CR2";
	case FILE_YUV:		return "YUV";
	case FILE_OGG:		return "OGG";
	case FILE_VORBIS:	return "VORBIS";
	case FILE_FLAC:		return "FLAC";
	case FILE_FFMPEG:	return "FFMPEG";
	case FILE_SCENE:	return "SCENE";
	case FILE_CR2_LIST:	return "CR2_LIST";
	case FILE_GIF_LIST:	return "GIF_LIST";
	case FILE_DB:		return "DB";
	}
	return "UNKNOWN";
}


PackagingEngine *File::new_packaging_engine(Asset *asset)
{
	PackagingEngine *result;
	switch (asset->format)
	{
		case FILE_OGG:
			result = (PackagingEngine*)new PackagingEngineOGG();
			break;
		default:
			result = (PackagingEngine*) new PackagingEngineDefault();
			break;
	}

	return result;
}


int File::record_fd()
{
	return file ? file->record_fd() : -1;
}


