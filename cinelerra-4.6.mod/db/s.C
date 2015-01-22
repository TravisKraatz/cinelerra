#include "s.h"


int Video_frameLoc::ikey_Frame_weight::
cmpr(char *a, char *b) { return 0; }

int Video_frameLoc::rkey_Frame_weight::
cmpr(char *a, char *b) { return 0; }

int Video_frameLoc::ikey_Frame_center::
cmpr(char *a, char *b) { return 0; }

int Video_frameLoc::rkey_Frame_center::
cmpr(char *a, char *b) { return 0; }

int Video_frameLoc::Allocate() { return 0; }
int Video_frameLoc::Construct() { return 0; }
int Video_frameLoc::Destruct() { return 0; }
void Video_frameLoc::Deallocate() {}


int TimelineLoc::ikey_Timelines::
cmpr(char *a, char *b) { return 0; }

int TimelineLoc::rkey_Timelines::
cmpr(char *a, char *b) { return 0; }

int TimelineLoc::ikey_Sequences::
cmpr(char *a, char *b) { return 0; }

int TimelineLoc::rkey_Sequences::
cmpr(char *a, char *b) { return 0; }

int TimelineLoc::Allocate() { return 0; }
int TimelineLoc::Construct() { return 0; }
int TimelineLoc::Destruct() { return 0; } 
void TimelineLoc::Deallocate() {}


int Clip_setLoc::ikey_Clip_title::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::rkey_Clip_title::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::ikey_Clip_system_time::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::rkey_Clip_system_time::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::ikey_Clip_creation_time::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::rkey_Clip_creation_time::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::ikey_Clip_path_pos::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::rkey_Clip_path_pos::
cmpr(char *a, char *b) { return 0; }

int Clip_setLoc::Allocate() { return 0; }
int Clip_setLoc::Construct() { return 0; }
int Clip_setLoc::Destruct() { return 0; }
void Clip_setLoc::Deallocate() {}

int Clip_viewsLoc::ikey_Clip_access::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::rkey_Clip_access::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::ikey_Last_view::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::rkey_Last_view::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::ikey_Total_views::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::rkey_Total_views::
cmpr(char *a, char *b) { return 0; }

int Clip_viewsLoc::Allocate() { return 0; }
int Clip_viewsLoc::Construct() { return 0; }
int Clip_viewsLoc::Destruct() { return 0; }
void Clip_viewsLoc::Deallocate() {}

int theDb:: create(const char *dfn) { return -1; }
int theDb:: db_create() { return -1; }

theDb:: theDb() :
   Video_frame(this), video_frame(Video_frame),
   Timeline(this), timeline(Timeline),
   Clip_set(this), clip_set(Clip_set),
   Clip_views(this), clip_views(Clip_views)
{
}

int theDb:: open(const char *dfn, int key) { return -1; }
int theDb:: db_open() { return -1; }
void theDb:: close() {}
int theDb:: access(const char *dfn, int key, int rw) { return -1; }
int theDb:: db_access() { return -1; }

