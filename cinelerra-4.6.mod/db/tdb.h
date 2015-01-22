#ifndef __DB_H__
#define __DB_H__

#include<string.h>

//entity definitions
#define DbObj(nm) \
  class nm##Obj : public Db::Obj { public:

#define DbLoc(nm) \
  class nm##Loc : public Db::ObjectLoc { public: \
    nm##Obj *operator ->() { return (nm##Obj *)addr(); } \
    nm##Loc(Db::Entity *ep) : Db::ObjectLoc(ep) {} \
    nm##Loc(Db::Entity &e) : Db::ObjectLoc(&e) {}

//basic definitions
#define basic_def(ty,n) class t_##n { public: ty v; t_##n() {} \
 t_##n(const ty &i) : v(i) {} \
 t_##n(const t_##n &i) : v(i.v) {} \
 t_##n &operator =(const t_##n &i) { v = i.v; return *this; } \
 ty &operator =(const ty &i) { return v = i; } \
 ty *addr() { return &v; } int size() { return sizeof(v); } \
 } v_##n \

//array definitions
#define array_def(ty,n,l) class t_##n { public: ty v[l]; t_##n() {} \
 t_##n(const t_##n &i) { memcpy(&v,&i.v,sizeof(v)); } \
 t_##n(ty *i) { memcpy(&v,i,sizeof(v)); } \
 t_##n(ty(*i)[l]) { memcpy(&v,i,sizeof(v)); } \
 ty *operator =(const ty *i) { memcpy(&v,i,sizeof(v)); return &v[0]; } \
 ty *addr() { return &v[0]; } int size() { return sizeof(v); } \
 } v_##n \

// variable array definitions
#define varray_def(ty,n) \
 class t_##n { public: char *v; int l; t_##n() {} \
 t_##n(const char *i, int sz) { v = (char *)i; l = sz; } \
 t_##n(const unsigned char *i, int sz) { v = (char *)i; l = sz; } \
 ty *addr() { return (ty *)v; } int size() { return l; } \
 }; Db::varObj v_##n \

// string array definitions
#define sarray_def(ty,n) \
 class t_##n { public: char *v; int l; t_##n() {} \
 t_##n(const char *i, int sz) { v = (char *)i; l = sz; } \
 t_##n(const unsigned char *i, int sz) { v = (char *)i; l = sz; } \
 t_##n(const char *i) { t_##n(i,strlen(i)+1); } \
 t_##n(const unsigned char *i) { t_##n(i,strlen(v)+1); } \
 ty *addr() { return (ty *)v; } int size() { return l; } \
 }; Db::varObj v_##n \

//basic type ref
#define basic_ref(ty,n) \
 ty *_##n() { return (*this)->v_##n.addr(); } \
 ty n() { return *_##n(); } \
 void n(ty i) { _wr(); *_##n() = i; } \
 int size_##n() { return (*this)->v_##n.size(); } \

//array type ref
#define array_ref(ty,n,l) \
 ty *_##n() { return (*this)->v_##n.addr(); } \
 ty (&n())[l] { return *(ty (*)[l])_##n(); } \
 void n(const ty *i,int m) { _wr(); if( m > 0 ) memcpy(n(),i,m); } \
 void n(const ty *i) { n(i,(*this)->v_##n.size()); } \
 int size_##n() { return (*this)->v_##n.size(); } \

//variable array type ref
#define varray_ref(ty,n) \
 ty *_##n() { return (ty *)addr((*this)->v_##n); } \
 ty *_##n(int sz) { size((*this)->v_##n, sz); \
   return sz > 0 ? (ty *)addr_wr((*this)->v_##n) : 0; } \
 ty (&n())[] { return *(ty (*)[])_##n(); } \
 int n(const ty *v, int sz) { ty *vp=_##n(sz); \
  if( vp && sz > 0 ) memcpy(vp, v, sz); return 0; } \
 int size_##n() { return (*this)->v_##n.size(); } \

//string array type ref
#define sarray_ref(ty,n) \
 ty *_##n() { return (ty *)addr((*this)->v_##n); } \
 ty *_##n(int sz) { size((*this)->v_##n, sz); \
   return sz > 0 ? (ty *)addr_wr((*this)->v_##n) : 0; } \
 ty (&n())[] { return *(ty (*)[])_##n(); } \
 int n(const ty *v, int sz) { ty *vp=_##n(sz); \
  if( vp && sz > 0 ) memcpy(vp, v, sz); return 0; } \
 int n(const char *v) { return n((ty *)v,strlen(v)+1); } \
 int n(const unsigned char *v) { return n((const char *)v); } \
 int size_##n() { return (*this)->v_##n.size(); } \


class Db {
public:
  enum { keyLT=-2, keyLE=-1, keyEQ=0, keyGE=1, keyGT=2, };
  class pgRef { public: int id; };
  typedef void *Index;
  typedef int (*CmprFn)(char *,char *);
  class Entity {
  public:
    Entity(Db *const db) {}
  };
  class Obj {};
  class varObj {
  public:
    int size() { return 0; }
  };
  class ObjectLoc {
  public:
    ObjectLoc(Entity *ep) {}
    int id() { return 0; }
    void _wr() {}
    Obj *addr() { return 0; }
    Obj *addr_wr() { return 0; }
    void *addr(varObj &vobj) { return 0; }
    void *addr_wr(varObj &vobj) { return 0; }
    int size(varObj &vobj) { return 0; }
    int size(varObj &vobj, int sz) { return 0; }
    int FindId(int id) { return -1; }
    int LocateId(int op, int id) { return -1; }
    int FirstId() { return -1; }
    int LastId() { return -1; }
    int NextId() { return -1; }
    int FirstId(pgRef &loc) { return -1; }
    int NextId(pgRef &loc) { return -1; }
    int NextLocId(pgRef &loc) { return -1; }
  };
  class iKey {
  public:
    iKey(Index i, ObjectLoc &l, CmprFn c) {}
    iKey(const char *nm, ObjectLoc &l, CmprFn c) {}
    int NextLoc(pgRef &pos) { return -1; }
    int Find() { return -1; }
    int Locate(int op=0) { return -1; }
  };
  class rKey {
  public:
    rKey(Index i, ObjectLoc &l, CmprFn c) : loc(l) {}
    rKey(const char *nm, ObjectLoc &l, CmprFn c) : loc(l) {}
    ObjectLoc &loc;
    int NextLoc(pgRef &pos) { return -1; }
    int First() { return -1; }  int First(pgRef &pos) { return -1; }
    int Next() { return -1; }   int Next(pgRef &pos) { return -1; }
    int Last() { return -1; }
    int Locate(int op=0) { return -1; }
  };
  class ObjectList;
  typedef ObjectList *Objects;
  int attach(int zrw=0) { return -1; }
  int detach() { return -1; }
  static void finit(Objects objects) {}

  int transaction() { return 0; }
  int commit(int force=0) { return 0; }
  int flush () { return 0; }
  int undo() { return 0; }
};

#endif
