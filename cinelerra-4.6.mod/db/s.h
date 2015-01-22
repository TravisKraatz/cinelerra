#ifndef _S_H_
#define _S_H_
#include <cstdio>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "tdb.h"


// Video_frame
DbObj(Video_frame)
  basic_def(double,Frame_mean);
  basic_def(double,Frame_std_dev);
  basic_def(double,Frame_cx);
  basic_def(double,Frame_cy);
  basic_def(double,Frame_moment);
  varray_def(unsigned char,Frame_data);
};

DbLoc(Video_frame)
  basic_ref(double,Frame_mean);
  basic_ref(double,Frame_std_dev);
  basic_ref(double,Frame_cx);
  basic_ref(double,Frame_cy);
  basic_ref(double,Frame_moment);
  varray_ref(unsigned char,Frame_data);

  class ikey_Frame_weight : public Db::iKey { public:
    Video_frameObj::t_Frame_mean v_Frame_mean;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Frame_weight(ObjectLoc &loc,
        double Frame_mean, int id=-1)
    : iKey("Frame_weight",loc,cmpr),
      v_Frame_mean(Frame_mean),
      v_id(id) {}
  };
  class rkey_Frame_weight : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Frame_weight(ObjectLoc &loc) : rKey("Frame_weight",loc,cmpr) {}
  };

  class ikey_Frame_center : public Db::iKey { public:
    Video_frameObj::t_Frame_moment v_Frame_moment;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Frame_center(ObjectLoc &loc,
        double Frame_moment, int id=-1)
    : iKey("Frame_center",loc,cmpr),
      v_Frame_moment(Frame_moment),
      v_id(id) {}
  };
  class rkey_Frame_center : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Frame_center(ObjectLoc &loc) : rKey("Frame_center",loc,cmpr) {}
  };

  int Allocate();
  int Construct();
  int Destruct();
  void Deallocate();
};
// Timeline
DbObj(Timeline)
  basic_def(unsigned int,Clip_id);
  basic_def(unsigned int,Sequence_no);
  basic_def(unsigned int,Frame_id);
  basic_def(unsigned int,Group);
  basic_def(double,Time_offset);
};

DbLoc(Timeline)
  basic_ref(unsigned int,Clip_id);
  basic_ref(unsigned int,Sequence_no);
  basic_ref(unsigned int,Frame_id);
  basic_ref(unsigned int,Group);
  basic_ref(double,Time_offset);

  class ikey_Timelines : public Db::iKey { public:
    TimelineObj::t_Frame_id v_Frame_id;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Timelines(ObjectLoc &loc,
        unsigned int Frame_id, int id=-1)
    : iKey("Timelines",loc,cmpr),
      v_Frame_id(Frame_id),
      v_id(id) {}
  };
  class rkey_Timelines : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Timelines(ObjectLoc &loc) : rKey("Timelines",loc,cmpr) {}
  };

  class ikey_Sequences : public Db::iKey { public:
    TimelineObj::t_Clip_id v_Clip_id;
    TimelineObj::t_Sequence_no v_Sequence_no;
    static int cmpr(char *a, char *b);
    ikey_Sequences(ObjectLoc &loc,
        unsigned int Clip_id,
        unsigned int Sequence_no)
    : iKey("Sequences",loc,cmpr),
      v_Clip_id(Clip_id),
      v_Sequence_no(Sequence_no) {}
  };
  class rkey_Sequences : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Sequences(ObjectLoc &loc) : rKey("Sequences",loc,cmpr) {}
  };

  int Allocate();
  int Construct();
  int Destruct();
  void Deallocate();
};
// Clip_set
DbObj(Clip_set)
  sarray_def(char,Title);
  sarray_def(char,Asset_path);
  basic_def(double,Position);
  basic_def(double,Framerate);
  basic_def(double,Average_weight);
  basic_def(unsigned int,Frames);
  basic_def(unsigned int,Prefix_size);
  basic_def(unsigned int,Suffix_size);
  varray_def(unsigned char,Weights);
  basic_def(long,System_time);
  basic_def(long,Creation_time);
};

DbLoc(Clip_set)
  sarray_ref(char,Title);
  sarray_ref(char,Asset_path);
  basic_ref(double,Position);
  basic_ref(double,Framerate);
  basic_ref(double,Average_weight);
  basic_ref(unsigned int,Frames);
  basic_ref(unsigned int,Prefix_size);
  basic_ref(unsigned int,Suffix_size);
  varray_ref(unsigned char,Weights);
  basic_ref(long,System_time);
  basic_ref(long,Creation_time);

  class ikey_Clip_title : public Db::iKey { public:
    Clip_setObj::t_Title v_Title;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Clip_title(ObjectLoc &loc,
        const Clip_setObj::t_Title &Title, int id=-1)
    : iKey("Clip_title",loc,cmpr),
      v_Title(Title),
      v_id(id) {}
  };
  class rkey_Clip_title : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Clip_title(ObjectLoc &loc) : rKey("Clip_title",loc,cmpr) {}
  };

  class ikey_Clip_system_time : public Db::iKey { public:
    Clip_setObj::t_System_time v_System_time;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Clip_system_time(ObjectLoc &loc,
        long System_time, int id=-1)
    : iKey("Clip_system_time",loc,cmpr),
      v_System_time(System_time),
      v_id(id) {}
  };
  class rkey_Clip_system_time : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Clip_system_time(ObjectLoc &loc) : rKey("Clip_system_time",loc,cmpr) {}
  };

  class ikey_Clip_creation_time : public Db::iKey { public:
    Clip_setObj::t_Creation_time v_Creation_time;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Clip_creation_time(ObjectLoc &loc,
        long Creation_time, int id=-1)
    : iKey("Clip_creation_time",loc,cmpr),
      v_Creation_time(Creation_time),
      v_id(id) {}
  };
  class rkey_Clip_creation_time : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Clip_creation_time(ObjectLoc &loc) : rKey("Clip_creation_time",loc,cmpr) {}
  };

  class ikey_Clip_path_pos : public Db::iKey { public:
    Clip_setObj::t_Asset_path v_Asset_path;
    Clip_setObj::t_Position v_Position;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Clip_path_pos(ObjectLoc &loc,
        const Clip_setObj::t_Asset_path &Asset_path,
        double Position, int id=-1)
    : iKey("Clip_path_pos",loc,cmpr),
      v_Asset_path(Asset_path),
      v_Position(Position),
      v_id(id) {}
  };
  class rkey_Clip_path_pos : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Clip_path_pos(ObjectLoc &loc) : rKey("Clip_path_pos",loc,cmpr) {}
  };

  int Allocate();
  int Construct();
  int Destruct();
  void Deallocate();
};
// Clip_views
DbObj(Clip_views)
  basic_def(unsigned int,Access_clip_id);
  basic_def(long,Access_time);
  basic_def(unsigned int,Access_count);
};

DbLoc(Clip_views)
  basic_ref(unsigned int,Access_clip_id);
  basic_ref(long,Access_time);
  basic_ref(unsigned int,Access_count);

  class ikey_Clip_access : public Db::iKey { public:
    Clip_viewsObj::t_Access_clip_id v_Access_clip_id;
    static int cmpr(char *a, char *b);
    ikey_Clip_access(ObjectLoc &loc,
        unsigned int Access_clip_id)
    : iKey("Clip_access",loc,cmpr),
      v_Access_clip_id(Access_clip_id) {}
  };
  class rkey_Clip_access : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Clip_access(ObjectLoc &loc) : rKey("Clip_access",loc,cmpr) {}
  };

  class ikey_Last_view : public Db::iKey { public:
    Clip_viewsObj::t_Access_time v_Access_time;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Last_view(ObjectLoc &loc,
        long Access_time, int id=-1)
    : iKey("Last_view",loc,cmpr),
      v_Access_time(Access_time),
      v_id(id) {}
  };
  class rkey_Last_view : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Last_view(ObjectLoc &loc) : rKey("Last_view",loc,cmpr) {}
  };

  class ikey_Total_views : public Db::iKey { public:
    Clip_viewsObj::t_Access_count v_Access_count;
    Clip_viewsObj::t_Access_clip_id v_Access_clip_id;
    int v_id;
    static int cmpr(char *a, char *b);
    ikey_Total_views(ObjectLoc &loc,
        unsigned int Access_count,
        unsigned int Access_clip_id, int id=-1)
    : iKey("Total_views",loc,cmpr),
      v_Access_count(Access_count),
      v_Access_clip_id(Access_clip_id),
      v_id(id) {}
  };
  class rkey_Total_views : public Db::rKey { public:
    static int cmpr(char *a, char *b);
    rkey_Total_views(ObjectLoc &loc) : rKey("Total_views",loc,cmpr) {}
  };

  int Allocate();
  int Construct();
  int Destruct();
  void Deallocate();
};


class theDb : public Db {
  int dfd, dkey, no_atime;
  int db_create();
  int db_open();
  int db_access();
public:
  Objects objects;
  Entity Video_frame;  Video_frameLoc video_frame;
  Entity Timeline;  TimelineLoc timeline;
  Entity Clip_set;  Clip_setLoc clip_set;
  Entity Clip_views;  Clip_viewsLoc clip_views;

  int create(const char *dfn);
  int open(const char *dfn, int key=-1);
  int access(const char *dfn, int key=-1, int rw=0);
  void close();
  int attach(int rw=0) { return Db::attach(rw); }
  int detach() { return Db::detach(); }

  theDb();
  ~theDb() { finit(objects); }
};

#endif
