#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <map>

// test csv    ./a.out < data.csv
// get strings ./a.out < /dev/null xgettext.po
// gen xlation ./a.out < data.csv xgettext.po new.po

using namespace std;

//csv = comma seperated value file
#define SEP ','

// converts libreoffice csv to string (with quotes attached)
static int xlat1(uint8_t *&in, uint8_t *out)
{
  int ch;
  while( (ch=*in++) != 0 && ch == ' ');
  if( ch != '\"' ) return 1;
  while( (ch=*in++) != 0 ) {
    if( ch == '\"' ) {
      if( *in != '\"' ) break;
      *out++ = '\\';  ++in;
    }
    *out++ = ch;
  }
  if( ch != '\"' ) return 1;
  *out = 0;
  return 0;
}

static inline int gch(uint8_t *&in) {
  int ch = *in++;
  if( ch == '\\' ) {
    switch( (ch=*in++) ) {
    case 'a':  ch = '\a';  break;
    case 'b':  ch = '\b';  break;
    case 'f':  ch = '\f';  break;
    case 'n':  ch = '\n';  break;
    case 'r':  ch = '\r';  break;
    case 't':  ch = '\t';  break;
    case 'v':  ch = '\v';  break;
    }
  }
  return !ch ? -1 : ch;
}

// converts string (with quotes attached) to c string
static int xlat2(uint8_t *in, uint8_t *out)
{
  int ch = gch(in);
  if( !ch ) return 1;
  int term = ch == '\"' ? ch : 0;
  if( !term ) *out++ = ch;
  while( (ch=gch(in)) >= 0 && ch != term ) *out++ = ch;
  if( ch >= 0 ) ++in;
  *out = 0;
  return 0;
}

// converts c++ string to c string text
static int xlat3(const string &s, uint8_t *out)
{
  *out++ = '\"';
  for( uint8_t *bp=(uint8_t*)s.c_str(); *bp; ++bp ) {
    int ch = *bp;
    switch( ch ) {
    case '"':   ch = '\"'; break;
    case '\a':  ch = 'a';  break;
    case '\b':  ch = 'b';  break;
    case '\f':  ch = 'f';  break;
    case '\n':  ch = 'n';  break;
    case '\r':  ch = 'r';  break;
    case '\t':  ch = 't';  break;
    case '\v':  ch = 'v';  break;
    default: *out++ = ch;  continue;
    }
    *out++ = '\\';  *out++ = ch;
  }
  *out++ = '\"'; *out++ = '\n'; *out++ = 0;
  return 0;
}

// parses input to c++ string
static string xlat(uint8_t *&in)
{
  uint8_t bfr[1024]; xlat1(in, bfr);
  uint8_t str[1024]; xlat2(bfr, str);
  return string((const char*)str);
}

typedef map<string,string> Trans;
static Trans trans;

static inline bool prefix_is(uint8_t *bp, const char *cp)
{
  return !strncmp((const char *)bp, cp, strlen(cp));
}
static inline uint8_t *bgets(uint8_t *bp, int len, FILE *fp)
{
  return (uint8_t*)fgets((char*)bp, len, fp);
}
static inline int bputs(uint8_t *bp, FILE *fp)
{
  return !fp ? 0 : fputs((const char*)bp, fp);
}

int main(int ac, char **av)
{
  int no = 0;
  uint8_t ibfr[1024], tbfr[1024];

  while( bgets(ibfr, sizeof(ibfr), stdin) ) {
    ++no;
    uint8_t *inp = ibfr;
    string key = xlat(inp);
    if( *inp++ != SEP ) {
      fprintf(stderr, "missing sep at line %d: %s", no, ibfr);
      exit(1);
    }
    string val = xlat(inp);
    //fprintf(stderr, "key = \"%s\", val = \"%s\"", key->c_str(), val->c_str());
    //if( !key->compare(*val) ) fprintf(stderr, " ** matches");
    //fprintf(stderr, "\n");
    trans.insert(Trans::value_type(key, val));
  }

  if( ac == 1 ) {
    for( Trans::iterator it = trans.begin(); it!=trans.end(); ++it ) {
      uint8_t str[1024];  xlat3(it->first, str);
      printf("key = \"%s\", val = \"%s\"\n", it->first.c_str(), (char *)str);
    }
    return 0;
  }

  FILE *ifp = fopen(av[1],"r");
  FILE *ofp = ac > 2 ? fopen(av[2],"w") : 0;
  no = 0;

  while( bgets(ibfr, sizeof(ibfr), ifp) ) {
    ++no;
    if( !prefix_is(ibfr, "msgid ") ) {
      bputs(ibfr, ofp);
      continue;
    }
    uint8_t str[1024]; xlat2(&ibfr[6], str);
    string key((const char*)str);
    if( !fgets((char*)tbfr, sizeof(tbfr), ifp) ) {
      fprintf(stderr, "file truncated line %d: %s", no, ibfr);
      exit(1);
    }
    ++no;
    bputs(ibfr, ofp);
   
    while( tbfr[0] == '"' ) {
      bputs(tbfr, ofp);
      xlat2(&tbfr[0], str);  key.append((const char*)str);
      if( !fgets((char*)tbfr, sizeof(tbfr), ifp) ) {
        fprintf(stderr, "file truncated line %d: %s", no, ibfr);
        exit(1);
      }
      ++no;
    }
    if( !prefix_is(tbfr, "msgstr ") ) {
      fprintf(stderr, "file truncated line %d: %s", no, ibfr);
      exit(1);
    }

    if( ac == 2 ) {
      xlat3(key, str);
      printf("%s", str);
      continue;
    }

    if( ac == 3 ) {
      Trans::iterator it = trans.lower_bound(key);
      if( it == trans.end() || it->first.compare(key) ) {
        printf("no trans line %d: %s", no, ibfr);
      }
      else
        xlat3(it->second, &tbfr[7]);
      bputs(tbfr, ofp);
    }
  }

  fclose(ifp);
  if( ofp ) fclose(ofp);
  return 0;
}

