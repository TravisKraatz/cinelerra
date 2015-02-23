
/*
 * CINELERRA
 * Copyright (C) 1997-2014 Adam Williams <broadcast at earthling dot net>
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

#include "bcsignals.h"
#include "bcwindowbase.h"
#include "bckeyboard.h"

#include <ctype.h>
#include <dirent.h>
#include <execinfo.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/types.h>

BC_Signals* BC_Signals::global_signals = 0;
static int signal_done = 0;
static int table_id = 0;

static bc_locktrace_t* new_bc_locktrace(void *ptr,
	const char *title,
	const char *location)
{
	bc_locktrace_t *result = (bc_locktrace_t*)malloc(sizeof(bc_locktrace_t));
	result->ptr = ptr;
	result->title = title;
	result->location = location;
	result->is_owner = 0;
	result->id = table_id++;
	result->tid = pthread_self();
	return result;
}


static struct sigaction old_segv = {0, }, old_intr = {0, };
static void handle_dump(int n, siginfo_t * info, void *sc);

const char *BC_Signals::trap_path = 0;
void *BC_Signals::trap_data = 0;
void (*BC_Signals::trap_hook)(FILE *fp, void *data) = 0;
bool BC_Signals::trap_sigsegv = false;
bool BC_Signals::trap_sigintr = false;

static void uncatch_sig(int sig, struct sigaction &old)
{
	struct sigaction act;
	sigaction(sig, &old, &act);
	old.sa_handler = 0;
}

static void catch_sig(int sig, struct sigaction &old)
{
	struct sigaction act;
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = handle_dump;
	act.sa_flags = SA_SIGINFO;
	sigaction(sig, &act, (!old.sa_handler ? &old : 0));
}

static void uncatch_intr() { uncatch_sig(SIGINT, old_intr); }
static void catch_intr() { catch_sig(SIGINT, old_intr); }
static void uncatch_segv() { uncatch_sig(SIGSEGV, old_segv); }
static void catch_segv() { catch_sig(SIGSEGV, old_segv); }

void BC_Signals::set_trap_path(const char *path)
{
	trap_path = path;
}

void BC_Signals::set_trap_hook(void (*hook)(FILE *fp, void *vp), void *data)
{
	trap_data = data;
	trap_hook = hook;
}

void BC_Signals::set_catch_segv(bool v) {
	if( v == trap_sigsegv ) return;
	if( v ) catch_segv();
	else uncatch_segv();
	v = trap_sigsegv;
}

void BC_Signals::set_catch_intr(bool v) {
	if( v == trap_sigintr ) return;
	if( v ) catch_intr();
	else uncatch_intr();
	v = trap_sigintr;
}

typedef struct
{
	int size;
	void *ptr;
	const char *location;
} bc_buffertrace_t;

static bc_buffertrace_t* new_bc_buffertrace(int size, void *ptr, const char *location)
{
	bc_buffertrace_t *result = (bc_buffertrace_t*)malloc(sizeof(bc_buffertrace_t));
	result->size = size;
	result->ptr = ptr;
	result->location = location;
	return result;
}






// Need our own table to avoid recursion with the memory manager
typedef struct
{
	void **values;
	int size;
	int allocation;
// This points to the next value to replace if the table wraps around
	int current_value;
} bc_table_t;

static void* append_table(bc_table_t *table, void *ptr)
{
	if(table->allocation <= table->size)
	{
		if(table->allocation)
		{
			int new_allocation = table->allocation * 2;
			void **new_values = (void**)calloc(new_allocation, sizeof(void*));
			memcpy(new_values, table->values, sizeof(void*) * table->size);
			free(table->values);
			table->values = new_values;
			table->allocation = new_allocation;
		}
		else
		{
			table->allocation = 4096;
			table->values = (void**)calloc(table->allocation, sizeof(void*));
		}
	}

	table->values[table->size++] = ptr;
	return ptr;
}

// Replace item in table pointed to by current_value and advance
// current_value
static void* overwrite_table(bc_table_t *table, void *ptr)
{
	free(table->values[table->current_value]);
	table->values[table->current_value++] = ptr;
	if(table->current_value >= table->size) table->current_value = 0;
	return 0;
}

static void clear_table(bc_table_t *table, int delete_objects)
{
	if(delete_objects)
	{
		for(int i = 0; i < table->size; i++)
		{
			free(table->values[i]);
		}
	}
	table->size = 0;
}

static void clear_table_entry(bc_table_t *table, int number, int delete_object)
{
	if(delete_object) free(table->values[number]);
	for(int i = number; i < table->size - 1; i++)
	{
		table->values[i] = table->values[i + 1];
	}
	table->size--;
}

// Table of functions currently running.
static bc_table_t execution_table = { 0, 0, 0, 0 };

// Table of locked positions
static bc_table_t lock_table = { 0, 0, 0, 0 };

// Table of buffers
static bc_table_t memory_table = { 0, 0, 0, 0 };

static bc_table_t temp_files = { 0, 0, 0, 0 };

// Can't use Mutex because it would be recursive
static pthread_mutex_t *lock = 0;
static pthread_mutex_t *handler_lock = 0;
// incase lock set after task ends
static pthread_t last_lock_thread = 0;
static const char *last_lock_title = 0;
static const char *last_lock_location = 0;
// Don't trace memory until this is true to avoid initialization
static int trace_memory = 0;


static const char* signal_titles[] =
{
	"NULL",
	"SIGHUP",
	"SIGINT",
	"SIGQUIT",
	"SIGILL",
	"SIGTRAP",
	"SIGABRT",
	"SIGBUS",
	"SIGFPE",
	"SIGKILL",
	"SIGUSR1",
	"SIGSEGV",
	"SIGUSR2",
	"SIGPIPE",
	"SIGALRM",
	"SIGTERM"
};

void BC_Signals::dump_stack(FILE *fp)
{
	void *buffer[256];
	int total = backtrace (buffer, 256);
	char **result = backtrace_symbols (buffer, total);
	fprintf(fp, "BC_Signals::dump_stack\n");
	for(int i = 0; i < total; i++)
	{
		fprintf(fp, "%s\n", result[i]);
	}
}

// Kill subprocesses
void BC_Signals::kill_subs()
{
// List /proc directory
	DIR *dirstream;
	struct dirent64 *new_filename;
	struct stat ostat;
	char path[BCTEXTLEN];
	char string[BCTEXTLEN];

	dirstream = opendir("/proc");
	if(!dirstream) return;

	while( (new_filename = readdir64(dirstream)) != 0 )
	{
// All digits are numbers
		char *ptr = new_filename->d_name;
		int got_alpha = 0;
		while(*ptr)
		{
			if(*ptr == '.' || isalpha(*ptr++))
			{
				got_alpha = 1;
				break;
			}
		}

		if(got_alpha) continue;

// Must be a directory
		sprintf(path, "/proc/%s", new_filename->d_name);
		if(!stat(path, &ostat))
		{
			if(S_ISDIR(ostat.st_mode))
			{
// Read process stat
				strcat(path, "/stat");
//printf("kill_subs %d %s\n", __LINE__, path);
				FILE *fd = fopen(path, "r");

// Must search forwards because the file is 0 length
				if(fd)
				{
					while(!feof(fd))
					{
						char c = fgetc(fd);
//printf("kill_subs %d %d\n", __LINE__, c);
						if(c == ')')
						{
// Search for 2 spaces
							int spaces = 0;
							while(!feof(fd) && spaces < 2)
							{
								c = fgetc(fd);
								if(c == ' ')
									spaces++;
							}

// Read in parent process
							ptr = string;
							while(!feof(fd))
							{
								*ptr = fgetc(fd);
								if(*ptr == ' ')
								{
									*ptr = 0;
									break;
								}
								ptr++;
							}

// printf("kill_subs %d process=%d getpid=%d parent_process=%d\n",
// __LINE__,
// atoi(new_filename->d_name),
// getpid(),
// atoi(string));
							int parent_process = atoi(string);
							int child_process = atoi(new_filename->d_name);

// Kill if we're the parent
							if(getpid() == parent_process)
							{
//printf("kill_subs %d: process=%d\n", __LINE__, atoi(new_filename->d_name));
								kill(child_process, SIGKILL);
							}
						}
					}

					fclose(fd);
				}
			}
		}
	}
}

static void signal_entry(int signum)
{
	signal(signum, SIG_DFL);

	pthread_mutex_lock(handler_lock);
	if(signal_done)
	{
		pthread_mutex_unlock(handler_lock);
		exit(0);
	}

	signal_done = 1;
	pthread_mutex_unlock(handler_lock);


	printf("signal_entry: got %s my pid=%d execution table size=%d:\n",
		signal_titles[signum],
		getpid(),
		execution_table.size);

	BC_Signals::kill_subs();
	BC_Signals::dump_traces();
	BC_Signals::dump_locks();
	BC_Signals::dump_buffers();
	BC_Signals::delete_temps();

// Call user defined signal handler
	BC_Signals::global_signals->signal_handler(signum);

	abort();
}

static void signal_entry_recoverable(int signum)
{
	printf("signal_entry_recoverable: got %s my pid=%d\n",
		signal_titles[signum],
		getpid());
}

// used to terminate child processes when program terminates
static void handle_exit(int signum)
{
//printf("child %d exit\n", getpid());
	exit(0);
}

void BC_Signals::set_sighup_exit(int enable)
{
	if( enable ) {
// causes SIGHUP to be generated when parent dies
		signal(SIGHUP, handle_exit);
		prctl(PR_SET_PDEATHSIG, SIGHUP, 0,0,0);
// prevents ^C from signalling child when attached to gdb
		setpgid(0, 0);
		if( isatty(0) ) ioctl(0, TIOCNOTTY, 0);
	}
	else {
		signal(SIGHUP, signal_entry);
		prctl(PR_SET_PDEATHSIG, 0,0,0,0);
	}
}

BC_Signals::BC_Signals()
{
}

void BC_Signals::dump_traces(FILE *fp)
{
// Dump trace table
	if(execution_table.size)
	{
		for(int i = execution_table.current_value; i < execution_table.size; i++)
			fprintf(fp,"    %s\n", (char*)execution_table.values[i]);
		for(int i = 0; i < execution_table.current_value; i++)
			fprintf(fp,"    %s\n", (char*)execution_table.values[i]);
	}

}

void BC_Signals::dump_locks(FILE *fp)
{
// Dump lock table
#ifdef TRACE_LOCKS
	fprintf(fp,"signal_entry: lock table size=%d\n", lock_table.size);
	for(int i = 0; i < lock_table.size; i++)
	{
		bc_locktrace_t *table = (bc_locktrace_t*)lock_table.values[i];
		fprintf(fp,"    %p %s %s %p%s\n", table->ptr,
			table->title, table->location, (void*)table->tid,
			table->is_owner ? " *" : "");
	}
#endif
}

void BC_Signals::dump_buffers(FILE *fp)
{
#ifdef TRACE_MEMORY
	pthread_mutex_lock(lock);
// Dump buffer table
	fprintf(fp,"BC_Signals::dump_buffers: buffer table size=%d\n", memory_table.size);
	for(int i = 0; i < memory_table.size; i++)
	{
		bc_buffertrace_t *entry = (bc_buffertrace_t*)memory_table.values[i];
		fprintf(fp,"    %d %p %s\n", entry->size, entry->ptr, entry->location);
	}
	pthread_mutex_unlock(lock);
#endif
}

void BC_Signals::delete_temps()
{
	pthread_mutex_lock(lock);
	if(temp_files.size) printf("BC_Signals::delete_temps: deleting %d temp files\n", temp_files.size);
	for(int i = 0; i < temp_files.size; i++)
	{
		printf("    %s\n", (char*)temp_files.values[i]);
		remove((char*)temp_files.values[i]);
	}
	pthread_mutex_unlock(lock);
}

void BC_Signals::reset_locks()
{
	pthread_mutex_unlock(lock);
}

void BC_Signals::set_temp(char *string)
{
	char *new_string = strdup(string);
	append_table(&temp_files, new_string);
}

void BC_Signals::unset_temp(char *string)
{
	for(int i = 0; i < temp_files.size; i++)
	{
		if(!strcmp((char*)temp_files.values[i], string))
		{
			clear_table_entry(&temp_files, i, 1);
			break;
		}
	}
}


void BC_Signals::initialize()
{
	BC_Signals::global_signals = this;
	lock = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
	handler_lock = (pthread_mutex_t*)calloc(1, sizeof(pthread_mutex_t));
	pthread_mutex_init(lock, 0);
	pthread_mutex_init(handler_lock, 0);

	initialize2();
}

// callable from debugger
extern "C"
void dump()
{
	BC_Signals::dump_traces();
	BC_Signals::dump_locks();
	BC_Signals::dump_buffers();
}

// kill SIGUSR2
void BC_Signals::signal_dump(int signum)
{
	BC_KeyboardHandler::kill_grabs();
	dump();
	signal(SIGUSR2, signal_dump);
}






void BC_Signals::initialize2()
{
	signal(SIGHUP, signal_entry);
	signal(SIGINT, signal_entry);
	signal(SIGQUIT, signal_entry);
	// SIGKILL cannot be stopped
	// signal(SIGKILL, signal_entry);
	catch_segv();
	signal(SIGTERM, signal_entry);
	signal(SIGFPE, signal_entry);
	signal(SIGPIPE, signal_entry_recoverable);
	signal(SIGUSR2, signal_dump);
}


void BC_Signals::signal_handler(int signum)
{
printf("BC_Signals::signal_handler\n");
//	exit(0);
}

const char* BC_Signals::sig_to_str(int number)
{
	return signal_titles[number];
}

#define TOTAL_TRACES 16

void BC_Signals::new_trace(const char *text)
{
	if(!global_signals) return;
	pthread_mutex_lock(lock);

// Wrap around
	if(execution_table.size >= TOTAL_TRACES)
	{
		overwrite_table(&execution_table, strdup(text));
//		clear_table(&execution_table, 1);
	}
	else
	{
		append_table(&execution_table, strdup(text));
	}
	pthread_mutex_unlock(lock);
}

void BC_Signals::new_trace(const char *file, const char *function, int line)
{
	char string[BCTEXTLEN];
	snprintf(string, BCTEXTLEN, "%s: %s: %d", file, function, line);
	new_trace(string);
}

void BC_Signals::delete_traces()
{
	if(!global_signals) return;
	pthread_mutex_lock(lock);
	clear_table(&execution_table, 0);
	pthread_mutex_unlock(lock);
}

// no canceling with lock held
void BC_Signals::lock_locks(const char *s)
{
	pthread_mutex_lock(lock);
	last_lock_thread = pthread_self();
	last_lock_title = s;
	last_lock_location = 0;
}

void BC_Signals::unlock_locks()
{
	pthread_mutex_unlock(lock);
}

#define TOTAL_LOCKS 256

int BC_Signals::set_lock(void *ptr,
	const char *title,
	const char *location)
{
	if(!global_signals) return 0;
	bc_locktrace_t *table = 0;
	int id_return = 0;

	pthread_mutex_lock(lock);
	last_lock_thread = pthread_self();
	last_lock_title = title;
	last_lock_location = location;
	if(lock_table.size >= TOTAL_LOCKS)
		clear_table(&lock_table, 0);

// Put new lock entry
	table = new_bc_locktrace(ptr, title, location);
	append_table(&lock_table, table);
	id_return = table->id;

	pthread_mutex_unlock(lock);
	return id_return;
}

void BC_Signals::set_lock2(int table_id)
{
	if(!global_signals) return;

	bc_locktrace_t *table = 0;
	pthread_mutex_lock(lock);
	for(int i = lock_table.size - 1; i >= 0; i--)
	{
		table = (bc_locktrace_t*)lock_table.values[i];
// Got it.  Hasn't been unlocked/deleted yet.
		if(table->id == table_id)
		{
			table->is_owner = 1;
			table->tid = pthread_self();
			pthread_mutex_unlock(lock);
			return;
		}
	}
	pthread_mutex_unlock(lock);
}

void BC_Signals::unset_lock2(int table_id)
{
	if(!global_signals) return;

	bc_locktrace_t *table = 0;
	pthread_mutex_lock(lock);
	for(int i = lock_table.size - 1; i >= 0; i--)
	{
		table = (bc_locktrace_t*)lock_table.values[i];
		if(table->id == table_id)
		{
			clear_table_entry(&lock_table, i, 1);
			break;
		}
	}
	pthread_mutex_unlock(lock);
}

void BC_Signals::unset_lock(void *ptr)
{
	if(!global_signals) return;

	bc_locktrace_t *table = 0;
	pthread_mutex_lock(lock);

// Take off currently held entry
	for(int i = 0; i < lock_table.size; i++)
	{
		table = (bc_locktrace_t*)lock_table.values[i];
		if(table->ptr == ptr)
		{
			if(table->is_owner)
			{
				clear_table_entry(&lock_table, i, 1);
				break;
			}
		}
	}

	pthread_mutex_unlock(lock);
}


void BC_Signals::unset_all_locks(void *ptr)
{
	if(!global_signals) return;
	pthread_mutex_lock(lock);
// Take off previous lock entry
	for(int i = 0; i < lock_table.size; )
	{
		bc_locktrace_t *table = (bc_locktrace_t*)lock_table.values[i];
		if(table->ptr == ptr)
		{
			clear_table_entry(&lock_table, i, 1);
			continue;
		}
		++i;
	}
	pthread_mutex_unlock(lock);
}

void BC_Signals::clear_locks_tid(pthread_t tid)
{
	if(!global_signals) return;
	pthread_mutex_lock(lock);
// Take off previous lock entry
	for(int i = 0; i < lock_table.size; )
	{
		bc_locktrace_t *table = (bc_locktrace_t*)lock_table.values[i];
		if(table->tid == tid)
		{
			clear_table_entry(&lock_table, i, 1);
			continue;
		}
		++i;
	}
	pthread_mutex_unlock(lock);
}


void BC_Signals::enable_memory()
{
	trace_memory = 1;
}

void BC_Signals::disable_memory()
{
	trace_memory = 0;
}


void BC_Signals::set_buffer(int size, void *ptr, const char* location)
{
	if(!global_signals) return;
	if(!trace_memory) return;

//printf("BC_Signals::set_buffer %p %s\n", ptr, location);
	pthread_mutex_lock(lock);
	append_table(&memory_table, new_bc_buffertrace(size, ptr, location));
	pthread_mutex_unlock(lock);
}

int BC_Signals::unset_buffer(void *ptr)
{
	if(!global_signals) return 0;
	if(!trace_memory) return 0;

	int ret = 1;
	pthread_mutex_lock(lock);
	for(int i = 0; i < memory_table.size; i++)
	{
		if(((bc_buffertrace_t*)memory_table.values[i])->ptr == ptr)
		{
//printf("BC_Signals::unset_buffer %p\n", ptr);
			clear_table_entry(&memory_table, i, 1);
			ret = 0;
			break;
		}
	}

	pthread_mutex_unlock(lock);
//	fprintf(stderr, "BC_Signals::unset_buffer buffer %p not found.\n", ptr);
	return ret;
}


#include <ucontext.h>
#include <sys/wait.h>
#include "thread.h"

#if __i386__
#define IP eip
#endif
#if __x86_64__
#define IP rip
#endif
#ifndef IP
#error gotta have IP
#endif


static void handle_dump(int n, siginfo_t * info, void *sc)
{
	uncatch_segv();  uncatch_intr();
	ucontext_t *uc = (ucontext_t *)sc;
	struct sigcontext *c = (struct sigcontext *)&uc->uc_mcontext;
	int pid = getpid(), tid = gettid();
	fprintf(stderr,"** %s at %p in pid %d, tid %d\n",
		n==SIGSEGV? "segv" : n==SIGINT? "intr" : "trap",
		(void*)c->IP, pid, tid);
	FILE *fp = 0;
	char fn[PATH_MAX];
	if( BC_Signals::trap_path ) {
		snprintf(fn, sizeof(fn), BC_Signals::trap_path, pid);
		fp = fopen(fn,"w");
	}
	if( fp ) {
		fprintf(stderr,"writing debug data to %s\n", fn);
		fprintf(fp,"** %s at %p in pid %d, tid %d\n",
			n==SIGSEGV? "segv" : n==SIGINT? "intr" : "trap",
			(void*)c->IP, pid, tid);
	}
	else {
		strcpy(fn, "stdout");
		fp = stdout;
	}
	time_t t;  time(&t);
	fprintf(fp,"created on %s", ctime(&t));
	struct passwd *pw = getpwuid(getuid());
	if( pw ) {
		fprintf(fp,"        by %d:%d %s(%s)\n",
			pw->pw_uid, pw->pw_gid, pw->pw_name, pw->pw_gecos);
	}
	fprintf(fp,"\nTHREADS:\n");  Thread::dump_threads(fp);
	fprintf(fp,"\nTRACES:\n");   BC_Signals::dump_traces(fp);
	fprintf(fp,"\nLOCKS:\n");    BC_Signals::dump_locks(fp);
	fprintf(fp,"\nBUFFERS:\n");  BC_Signals::dump_buffers(fp);
	if( BC_Signals::trap_hook ) {
		fprintf(fp,"\nMAIN HOOK:\n");
		BC_Signals::trap_hook(fp, BC_Signals::trap_data);
	}
	fprintf(fp,"\n\n");
	if( fp != stdout ) fclose(fp);
// must be root
	if( getuid() != 0 ) return;
	char cmd[1024], *cp = cmd;
	cp += sprintf(cp, "exec gdb /proc/%d/exe -p %d --batch --quiet "
		"-ex \"thread apply all info registers\" "
		"-ex \"thread apply all bt full\" "
		"-ex \"quit\"", pid, pid);
	if( fp != stdout )
		cp += sprintf(cp," >> \"%s\"", fn);
	cp += sprintf(cp," 2>&1");
//printf("handle_dump:: pid=%d, cmd='%s'  fn='%s'\n",pid,cmd,fn);
        pid = vfork();
        if( pid < 0 ) {
		fprintf(stderr,"** can't start gdb, dump abondoned\n");
		return;
	}
	if( pid > 0 ) {
		waitpid(pid,0,0);
		fprintf(stderr,"** dump complete\n");
		return;
	}
        char *const argv[4] = { (char*) "/bin/sh", (char*) "-c", cmd, 0 };
        execvp(argv[0], &argv[0]);
}





#ifdef TRACE_MEMORY

// void* operator new(size_t size)
// {
// //printf("new 1 %d\n", size);
//     void *result = malloc(size);
// 	BUFFER(size, result, "new");
// //printf("new 2 %d\n", size);
// 	return result;
// }
//
// void* operator new[](size_t size)
// {
// //printf("new [] 1 %d\n", size);
//     void *result = malloc(size);
// 	BUFFER(size, result, "new []");
// //printf("new [] 2 %d\n", size);
// 	return result;
// }
//
// void operator delete(void *ptr)
// {
// //printf("delete 1 %p\n", ptr);
// 	UNBUFFER(ptr);
// //printf("delete 2 %p\n", ptr);
//     free(ptr);
// }
//
// void operator delete[](void *ptr)
// {
// //printf("delete [] 1 %p\n", ptr);
// 	UNBUFFER(ptr);
//     free(ptr);
// //printf("delete [] 2 %p\n", ptr);
// }


#endif
