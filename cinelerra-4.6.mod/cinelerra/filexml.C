
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "bcsignals.h"
#include "arraylist.h"
#include "cstrdup.h"
#include "format.inc"
#include "filexml.h"
#include "mainerror.h"

// messes up cutads link
#undef eprintf
#define eprintf printf

static const char left_delm = '<', right_delm = '>';

XMLBuffer::XMLBuffer(long buf_size, long max_size, int del)
{
	bsz = buf_size;
	bfr = new unsigned char[bsz];
	inp = outp = bfr;
	lmt = bfr + bsz;
	isz = max_size;
	destroy = del;
}

XMLBuffer::XMLBuffer(const char *buf, long buf_size, int del)
{	// reading
	bfr = (unsigned char *)buf;
	bsz = buf_size;
	outp = bfr;
	inp = bfr+bsz;
	lmt = inp+1;
	isz = bsz;
	destroy = del;
}

XMLBuffer::XMLBuffer(long buf_size, const char *buf, int del)
{	// writing
	bfr = (unsigned char *)buf;
	bsz = buf_size;
	outp = bfr+bsz;
	inp = bfr;
	lmt = inp+1;
	isz = bsz;
	destroy = del;
}

XMLBuffer::~XMLBuffer()
{
	if( destroy ) delete [] bfr;
}

unsigned char *&XMLBuffer::demand(long len)
{
	if( len > bsz ) {
		len += BCTEXTLEN;
		unsigned char *np = new unsigned char[len];
		if( inp > bfr ) memcpy(np,bfr,inp-bfr);
		inp = np + (inp-bfr);
		outp = np + (outp-bfr);
		lmt = np + len;  bsz = len;
		delete [] bfr;   bfr = np;
	}
	return bfr;
}

int XMLBuffer::write(const char *bp, int len)
{
	unsigned char *sp = demand(otell()+len);
	memmove(inp,bp,len);
	inp += len;
	return len;
}

int XMLBuffer::read(char *bp, int len)
{
	long size = inp - outp;
	if( size <= 0 && len > 0 ) return -1;
	if( len > size ) len = size;
	memmove(bp,outp,len);
	outp += len;
	return len;
}

int XMLBuffer::enext(unsigned int v)
{
	if( v >= 0x80 )
		return v > 0xffff ? unibs('U',v,4): unibs('u',v,2);
	switch( v ) {
	case '\n': break; // return uesc('n');
	case '\t': return uesc('t');
	case '\r': return uesc('r');
	case '\b': return uesc('b');
	case '\f': return uesc('f');
	case '\v': return uesc('v');
	case '\a': return uesc('a');
	case '\\': return uesc('\\');
	default:
		if( v < 0x20 || v == 0x7f )
			return unibs('x',v,1);
	}
	next(v);
	return 1;
}

int XMLBuffer::wnext(unsigned int v)
{
	if( v < 0x00000080 ) { next(v);	return 1; }
	int n = v < 0x00000800 ? 2 : v < 0x00010000 ? 3 :
		v < 0x00200000 ? 4 : v < 0x04000000 ? 5 : 6;
	int m = (0xff00 >> n), i = n-1;
	next((v>>(6*i)) | m);
	while( --i >= 0 ) next(((v>>(6*i)) & 0x3f) | 0x80);
	return n;
}

int XMLBuffer::wnext()
{
	int v = 0, n = 0, ch = next();
	if( ch == '\\' ) {
		switch( (ch=next()) ) {
		case 'n': return '\n';
		case 't': return '\t';
		case 'r': return '\r';
		case 'b': return '\b';
		case 'f': return '\f';
		case 'v': return '\v';
		case 'a': return '\a';
		case '0': case '1': case '2': case '3':
		case '4': case '5': case '6': case '7':
			v = ch - '0';
			for( int i=3; --i>0; v=v*8+ch, next() )
				if( (ch=cur()-'0') < 0 || ch >= 8 ) break;
			return v;
		case 'x':	n = 2;	break;
		case 'u':	n = 4;	break;
		case 'U':	n = 8;	break;
		default: return ch;
		}
		for( int i=n; --i>=0; v=v*16+ch, next() ) {
			if( (ch=cur()-'0')>=0 && ch<10 ) continue;
			if( (ch-='A'-'0'-10)>=10 && ch<16 ) continue;
			if( (ch-='a'-'A')<10 || ch>=16 ) break;
		}
	}
	else if( ch >= 0x80 ) {
		static const unsigned char byts[] = {
			1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 5,
		};
		int i = ch - 0xc0;
		n = i<0 ? 0 : byts[i/4];
		for( v=ch, i=n; --i>=0; v+=next() ) v <<= 6;
		static const unsigned int ofs[6] = {
			0x00000000U, 0x00003080U, 0x000E2080U,
			0x03C82080U, 0xFA082080U, 0x82082080U
		};
		v -= ofs[n];
	}
	else
		v = ch;
	return v;
}


// Precision in base 10
// for float is 6 significant figures
// for double is 16 significant figures

XMLTag::Property::Property(const char *pp, const char *vp)
{
//printf("Property %s = %s\n",pp, vp);
	prop = cstrdup(pp);
	value = cstrdup(vp);
}

XMLTag::Property::~Property()
{
	delete [] prop;
	delete [] value;
}


XMLTag::XMLTag()
{
	string = 0;
	avail = used = 0;
}

XMLTag::~XMLTag()
{
	properties.remove_all_objects();
	delete [] string;
}

char *&XMLTag::demand(int len)
{
	if( len > avail ) {
		char *np = new char[len+=BCSTRLEN];
		if( used > 0 ) memcpy(np, string, used);
		delete [] string;  string = np;
		string[used] = 0;  avail = len;
	}
	return string;
}

const char *XMLTag::get_property(const char *property, char *value)
{
	int i = properties.size();
	while( --i >= 0 && strcasecmp(properties[i]->prop, property) );
	if( i >= 0 )
		strcpy(value, properties[i]->value);
	else
		*value = 0;
	return value;
}

//getters
const char *XMLTag::get_property_text(int i) {
	return i < properties.size() ? properties[i]->value : "";
}
int XMLTag::get_property_int(int i) {
	return i < properties.size() ? atol(properties[i]->value) : 0;
}
float XMLTag::get_property_float(int i) {
	return i < properties.size() ? atof(properties[i]->value) : 0.;
}
const char* XMLTag::get_property(const char *prop) {
	for( int i=0; i<properties.size(); ++i ) {
		if( !strcasecmp(properties[i]->prop, prop) )
			return properties[i]->value;
	}
	return 0;
}
int32_t XMLTag::get_property(const char *prop, int32_t dflt) {
	const char *cp = get_property(prop);
	return !cp ? dflt : atol(cp);
}
int64_t XMLTag::get_property(const char *prop, int64_t dflt) {
	const char *cp = get_property(prop);
	return !cp ? dflt : strtoll(cp,0,0);
}
float XMLTag::get_property(const char *prop, float dflt) {
	const char *cp = get_property(prop);
	return !cp ? dflt : atof(cp);
}
double XMLTag::get_property(const char *prop, double dflt) {
	const char *cp = get_property(prop);
	return !cp ? dflt : atof(cp);
}

//setters
int XMLTag::set_title(const char *text) {
	strcpy(title, text);
	return 0;
}
int XMLTag::set_property(const char *text, const char *value) {
	properties.append(new XMLTag::Property(text, value));
	return 0;
}
int XMLTag::set_property(const char *text, int32_t value)
{
	char text_value[BCSTRLEN];
	sprintf(text_value, "%d", value);
	set_property(text, text_value);
}
int XMLTag::set_property(const char *text, int64_t value)
{
	char text_value[BCSTRLEN];
	sprintf(text_value, "" _LD "", value);
	set_property(text, text_value);
}
int XMLTag::set_property(const char *text, float value)
{
	char text_value[BCSTRLEN];
	if (value - (float)((int64_t)value) == 0)
		sprintf(text_value, "" _LD "", (int64_t)value);
	else
		sprintf(text_value, "%.6e", value);
	set_property(text, text_value);
}
int XMLTag::set_property(const char *text, double value)
{
	char text_value[BCSTRLEN];
	if (value - (double)((int64_t)value) == 0)
		sprintf(text_value, "" _LD "", (int64_t)value);
	else
		sprintf(text_value, "%.16e", value);
	set_property(text, text_value);
}


int XMLTag::reset_tag()
{
	used = 0;
	properties.remove_all_objects();
	return 0;
}

int XMLTag::write_tag(FileXML *xml)
{
	XMLBuffer *buf = xml->buffer;
// title header
	buf->next(left_delm);
	buf->write(title, strlen(title));

// properties
	for( int i=0; i<properties.size(); ++i ) {
		const char *prop = properties[i]->prop;
		const char *value = properties[i]->value;
		int plen = strlen(prop), vlen = strlen(value);
		bool need_quotes = !vlen || strchr(value,' ');
		buf->next(' ');
		xml->append_text(prop, plen);
		buf->next('=');
		if( need_quotes ) buf->next('\"');
		xml->append_text(value, vlen);
		if( need_quotes ) buf->next('\"');
	}

	buf->next(right_delm);
	return 0;
}

int XMLTag::read_tag(FileXML *xml)
{
	XMLBuffer *buf = xml->buffer;
	int len, term;
	long prop_start, prop_end;
	long value_start, value_end;
	long ttl;
	int ch = buf->wnext();
// skip ws
	while( ch>=0 && ws(ch) ) ch = buf->wnext();
	if( ch < 0 ) { printf("err %d\n",__LINE__); return 1; }
	
// read title
	ttl = buf->itell() - 1;
	for( int i=0; i<MAX_TITLE && ch>=0; ++i, ch=buf->wnext() ) {
		if( ch == right_delm || ch == '=' || ws(ch) ) break;
	}
	if( ch < 0 ) { printf("err %d\n",__LINE__); return 1; }
	len = buf->itell()-1 - ttl;
	if( len >= MAX_TITLE ) { printf("err %d\n",__LINE__); return 1; }
// if title
	if( ch != '=' ) {
		memmove(title, buf->pos(ttl), len);
		title[len] = 0;
	}
// no title but first property.
	else {
		title[0] = 0;
		buf->iseek(ttl);
		ch = buf->wnext();
	}
// read properties
	while( ch >= 0 && ch != right_delm ) {
// find tag start, skip header leadin
		while( ch >= 0 && (ch==left_delm || ws(ch)) )
			ch = buf->wnext();
// find end of property name
		prop_start = buf->itell()-1;
		while( ch >= 0 && (ch!=right_delm && ch!='=' && !ws(ch)) )
			ch = buf->wnext();
		if( ch < 0 ) { printf("err %d\n",__LINE__); return 1; }
		prop_end = buf->itell()-1;
// skip ws = ws
		while( ch >= 0 && ws(ch) )
			ch = buf->wnext();
		if( ch == '=' ) ch = buf->wnext();
		while( ch >= 0 && ws(ch) )
			ch = buf->wnext();
		if( ch < 0 ) { printf("err %d\n",__LINE__); return 1; }
// set terminating char
		if( ch == '\"' ) {
			term = ch;
			ch = buf->wnext();
		}
		else
			term = ' ';
		value_start = buf->itell()-1;
		while( ch >= 0 && (ch!=term && ch!=right_delm && ch!='\n') )
			ch = buf->wnext();
		if( ch < 0 ) { printf("err %d\n",__LINE__); return 1; }
		value_end = buf->itell()-1;
// add property
		int plen = prop_end-prop_start;
		if( !plen ) continue;
		int vlen = value_end-value_start;
		char prop[plen+1], value[vlen+1];
		const char *coded_prop = (const char *)buf->pos(prop_start);
		const char *coded_value = (const char *)buf->pos(value_start);
// props should not have coded text
		memcpy(prop, coded_prop, plen);
		prop[plen] = 0;
		xml->decode(value, coded_value, vlen);
		if( prop_end > prop_start ) {
			Property *property = new Property(prop, value);
			properties.append(property);
		}
// skip the terminating char
		if( ch != right_delm ) ch = buf->wnext();
	}
	if( !properties.size() && !title[0] ) { printf("err %d\n",__LINE__); return 1; }
	return 0;
}



FileXML::FileXML()
{
	output = 0;
	output_length = 0;
	buffer = new XMLBuffer();
	decode = decode_data;
	encode = encode_data;
	coded_length = encoded_length;
}

FileXML::~FileXML()
{
	delete buffer;
	delete [] output;
}


int FileXML::terminate_string()
{
	append_text("", 1);
	return 0;
}

int FileXML::rewind()
{
	terminate_string();
	buffer->iseek(0);
	return 0;
}


int FileXML::append_newline()
{
	append_text("\n", 1);
	return 0;
}

int FileXML::append_tag()
{
	tag.write_tag(this);
	append_text(tag.string, tag.used);
	tag.reset_tag();
	return 0;
}

int FileXML::append_text(const char *text)
{
	append_text(text, strlen(text));
	return 0;
}

int FileXML::append_data(const char *text, long len)
{
	if( text != 0 && len > 0 )
		buffer->write(text, len);
	return 0;
}

int FileXML::append_text(const char *text, long len)
{
	if( text != 0 && len > 0 ) {
		int size = coded_length(text, len);
		char coded_text[size+1];
		encode(coded_text, text, len);
		buffer->write(coded_text, size);
	}
	return 0;
}


char* FileXML::get_data()
{
	long ofs = buffer->itell();
	return (char *)buffer->pos(ofs);
}
char* FileXML::string()
{
	return (char *)buffer->pos();
}

char* FileXML::read_text()
{
	int ch = buffer->wnext();
// filter out first char is new line
	if( ch == '\n' ) ch = buffer->wnext();
	long ipos = buffer->itell()-1;
// scan for delimiter
	while( ch >= 0 && ch != left_delm ) ch = buffer->wnext();
	long len = buffer->itell()-1 - ipos;
	if( len > output_length ) {
		delete [] output;
		output_length = len;
		output = new char[output_length+1];
	}
	decode(output,(const char *)buffer->pos(ipos), len);
	return output;
}

int FileXML::read_tag()
{
	int ch = buffer->wnext();
// scan to next tag
	while( ch >= 0 && ch != left_delm ) ch = buffer->wnext();
	if( ch < 0 ) return 1;
	tag.reset_tag();
	return tag.read_tag(this);
}

int FileXML::read_data_until(const char *tag_end, char *out, int len)
{
	long ipos = buffer->itell();
	int opos = 0, pos = -1;
	int ch = buffer->wnext();
	for( int olen=len-1; ch>=0 && opos<olen; ch=buffer->wnext() ) {
		if( pos < 0 ) { // looking for next tag
			if( ch == left_delm ) {
				ipos = buffer->itell()-1;
				pos = 0;
			}
			else
				out[opos++] = ch;
			continue;
		}
		if( tag_end[pos] != ch ) { // mismatched, copy prefix to out
			out[opos++] = left_delm;
			for( int i=0; i<pos && opos<olen; ++i )
				out[opos++] = tag_end[i];
			pos = -1;
			continue;
		}
		if( !tag_end[++pos] ) break;
	}
// if end tag is reached, pos is left on the < of the end tag
	if( pos >= 0 && !tag_end[pos] )
		buffer->iseek(ipos);
	return opos;
}

int FileXML::read_text_until(const char *tag_end, char *out, int len)
{
	char data[len+1];
	int opos = read_data_until(tag_end, data, len);
	decode(out, data, opos);
}

int FileXML::write_to_file(const char *filename)
{
	strcpy(this->filename, filename);
	FILE *out = fopen(filename, "wb");
	if( !out ) {
		eprintf("write_to_file %d \"%s\": %m\n", __LINE__, filename);
		return 1;
	}
// Position may have been rewound after storing
	const char *str = string();
	long len = strlen(str);
	fprintf(out, "<?xml version=\"1.0\"?>\n");
	if( len && !fwrite(str, len, 1, out) ) {
		eprintf("write_to_file %d \"%s\": %m\n", __LINE__, filename);
		fclose(out);
		return 1;
	}
	fclose(out);
	return 0;
}

int FileXML::write_to_file(FILE *file)
{
	strcpy(filename, "");
	fprintf(file, "<?xml version=\"1.0\"?>\n");
	const char *str = string();
	long len = strlen(str);
// Position may have been rewound after storing
	if( len && !fwrite(str, len, 1, file) ) {
		eprintf("\"%s\": %m\n", filename);
		return 1;
	}
	return 0;
}

int FileXML::read_from_file(const char *filename, int ignore_error)
{
	
	strcpy(this->filename, filename);
	FILE *in = fopen(filename, "rb");
	if( !in ) {
		if(!ignore_error) 
			eprintf("\"%s\" %m\n", filename);
		return 1;
	}
	fseek(in, 0, SEEK_END);
	long length = ftell(in);
	fseek(in, 0, SEEK_SET);
	char *fbfr = new char[length+1];
	delete buffer;
	(void)fread(fbfr, length, 1, in);
	fbfr[length] = 0;
	buffer = new XMLBuffer(fbfr, length, 1);
	fclose(in);
	return 0;
}

int FileXML::read_from_string(char *string)
{
        strcpy(this->filename, "");
	long length = strlen(string);
	char *sbfr = new char[length+1];
	strcpy(sbfr, string);
	delete buffer;
	buffer = new XMLBuffer(sbfr, length, 1);
        return 0;
}

void FileXML::set_coding(int coding)
{
	coded = coding;
	decode = coded ? decode_data : copy_data;
	encode = coded ? encode_data : copy_data;
	coded_length = coded ? encoded_length : copy_length;
}

int FileXML::get_coding()
{
	return coded;
}

int FileXML::set_shared_input(char *shared_string, long avail, int coded)
{
	strcpy(this->filename, "");
	delete buffer;
	buffer = new XMLBuffer(shared_string, avail, 0);
	set_coding(coded);
        return 0;
}

int FileXML::set_shared_output(char *shared_string, long avail, int coded)
{
	strcpy(this->filename, "");
	delete buffer;
	buffer = new XMLBuffer(avail, shared_string, 0);
	set_coding(coded);
        return 0;
}



// ================================ XML tag



int XMLTag::title_is(const char *tp)
{
	return !strcasecmp(title, tp) ? 1 : 0;
}

char* XMLTag::get_title()
{
	return title;
}

int XMLTag::get_title(char *value)
{
	if( title[0] != 0 ) strcpy(value, title);
	return 0;
}

int XMLTag::test_property(char *property, char *value)
{
	for( int i=0; i<properties.size(); ++i ) {
		if( !strcasecmp(properties[i]->prop, property) &&
		    !strcasecmp(properties[i]->value, value) )
			return 1;
	}
	return 0;
}

static inline int xml_cmp(const char *np, const char *sp)
{
   const char *tp = ++np;
   while( *np ) { if( *np != *sp ) return 0;  ++np; ++sp; }
   return np - tp;
}

char *FileXML::decode_data(char *bp, const char *sp, int n)
{
   char *ret = bp;
   if( n < 0 ) n = strlen(sp);
   const char *ep = sp + n;
   while( sp < ep ) {
      if( (n=*sp++) != '&' ) { *bp++ = n; continue; }
      switch( (n=*sp++) ) {
      case 'a': // &amp;
         if( (n=xml_cmp("amp;", sp)) ) { *bp++ = '&';  sp += n;  continue; }
         break;
      case 'g': // &gt;
         if( (n=xml_cmp("gt;", sp)) ) { *bp++ = '>';  sp += n;  continue; }
         break;
      case 'l': // &lt;
         if( (n=xml_cmp("lt;", sp)) ) { *bp++ = '<';  sp += n;  continue; }
         break;
      case 'q': // &quot;
         if( (n=xml_cmp("quot;", sp)) ) { *bp++ = '"';  sp += n;  continue; }
         break;
      case '#': { // &#<num>;
         char *cp = 0;  int v = strtoul(sp,&cp,10);
         if( *cp == ';' ) { *bp++ = (char)v;  sp = cp+1;  continue; }
         n = cp - sp; }
         break;
      default:
         *bp++ = '&';
         *bp++ = (char)n;
         continue;
      }
      sp -= 2;  n += 2;
      while( --n >= 0 ) *bp++ = *sp++;
   }
   *bp = 0;
   return ret;
}


char *FileXML::encode_data(char *bp, const char *sp, int n)
{
	char *ret = bp;
	if( n < 0 ) n = strlen(sp);
	const char *cp, *ep = sp + n;
	while( sp < ep ) {
		int ch = *sp++;
		switch( ch ) {
		case '<':  cp = "&lt;";    break;
		case '>':  cp = "&gt;";    break;
		case '&':  cp = "&amp;";   break;
		case '"':  cp = "&quot;";  break;
		default:  *bp++ = ch;      continue;
		}
		while( *cp != 0 ) *bp++ = *cp++;
	}
	*bp = 0;
	return ret;
}

long FileXML::encoded_length(const char *sp, int n)
{
	long len = 0;
	if( n < 0 ) n = strlen(sp);
	const char *cp, *ep = sp + n;
	while( sp < ep ) {
		int ch = *sp++;
		switch( ch ) {
		case '<':  len += 4;  break;
		case '>':  len += 4;  break;
		case '&':  len += 5;  break;
		case '"':  len += 6;  break;
		default:   ++len;     break;
		}
	}
	return len;
}

char *FileXML::copy_data(char *bp, const char *sp, int n)
{
	int len = n < 0 ? strlen(sp) : n;
	memmove(bp,sp,len);
	bp[len] = 0;
	return bp;
}

long FileXML::copy_length(const char *sp, int n)
{
	int len = n < 0 ? strlen(sp) : n;
	return len;
}

