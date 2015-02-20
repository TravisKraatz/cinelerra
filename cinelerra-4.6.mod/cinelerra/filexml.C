
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

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bcsignals.h"
#include "format.inc"
#include "filexml.h"
#include "mainerror.h"

// messes up cutads link
#undef eprintf
#define eprintf printf

// Precision in base 10
// for float is 6 significant figures
// for double is 16 significant figures



FileXML::FileXML(char left_delimiter, char right_delimiter)
{
	tag.set_delimiters(left_delimiter, right_delimiter);
	this->left_delimiter = left_delimiter;
	this->right_delimiter = right_delimiter;
	available = 64;
	string = new char[available];
	string[0] = 0;
	position = length = 0;
	output = 0;
	output_length = 0;
	share_string = 0;
}

FileXML::~FileXML()
{
	if(!share_string) delete [] string;
	delete [] output;
}

void FileXML::dump()
{
	printf("FileXML::dump:\n%s\n", string);
}

int FileXML::terminate_string()
{
	append_text("", 1);
	return 0;
}

int FileXML::rewind()
{
	terminate_string();
	length = strlen(string);
	position = 0;
	return 0;
}


int FileXML::append_newline()
{
	append_text("\n", 1);
	return 0;
}

int FileXML::append_tag()
{
	tag.write_tag();
	append_text(tag.string, tag.len);
	tag.reset_tag();
	return 0;
}

int FileXML::append_text(const char *text)
{
	append_text(text, strlen(text));
	return 0;
}

int FileXML::append_text(const char *text, long len)
{
	if(position + len > available) {
		long size = available;
		while(position + len > size) size *= 2;
		reallocate_string(size);
	}

	for(int i = 0; i < len; i++, position++) {
		string[position] = text[i];
	}
	return 0;
}

int FileXML::encode_text(const char *text)
{
// We have to encode at least the '<' char
// We encode three things:
// '<' -> '<'
// '>' -> '>'
// '&' -> '&'
	char leftb[] = "<";
	char rightb[] = ">";
	char amp[] = "&";
	char *replacement;
	int len = strlen(text);
	int lastpos = 0;
	for (int i = 0; i < len; i++) {
		switch (text[i]) {
			case '<': replacement = leftb; break;
			case '>': replacement = rightb; break;
			case '&': replacement = amp; break;
			default: replacement = 0; break;
		}
		if (replacement) {
			if (i - lastpos > 0)
				append_text(text + lastpos, i - lastpos);
			append_text(replacement, strlen(replacement));
			lastpos = i + 1;
		}
	}
	append_text(text + lastpos, len - lastpos);
	return 0;
}



int FileXML::reallocate_string(long new_available)
{
	if(!share_string) {
		char *new_string = new char[new_available];
		long size = new_available;
		if( position < size ) size = position;
		for(int i = 0; i < size; i++) new_string[i] = string[i];
		available = new_available;
		delete [] string;
		string = new_string;
	}
	return 0;
}

char* FileXML::get_ptr()
{
	return string + position;
}

char* FileXML::read_text()
{
// use < to mark end of text and start of tag
	char *bp = string + position, *ep = bp + length;
// filter out first char is new line
	if( bp < ep && *bp == '\n' ) ++bp;
	char *cp = bp;
// scan for end of bfr, end of string, or delimiter
	while( cp < ep && *cp != 0 && *cp != left_delimiter ) ++cp;

	long len = XMLTag::encoded_length(bp, cp-bp);
	if( len > output_length ) {
		delete [] output;
		output_length = len;
		output = new char[output_length + 1];
	}
	XMLTag::encode_data(output, bp, cp-bp);

	return output;
}

int FileXML::read_tag()
{
// scan to next tag
	while(position < length && string[position] != left_delimiter)
	{
		position++;
	}
	tag.reset_tag();
	if(position >= length) return 1;
//printf("FileXML::read_tag %s\n", &string[position]);
	return tag.read_tag(string, position, length);
}

int FileXML::read_text_until(const char *tag_end, char *output, int max_len)
{
// read to next tag
	int out_position = 0;
	int test_position1, test_position2;
	int result = 0;
	
	while(!result && position < length && out_position < max_len - 1)
	{
		while(position < length && string[position] != left_delimiter)
		{
//printf("FileXML::read_text_until 1 %c\n", string[position]);
			output[out_position++] = string[position++];
		}
		
		if(position < length && string[position] == left_delimiter)
		{
// tag reached
// test for tag_end
			result = 1;         // assume end
			
			for(test_position1 = 0, test_position2 = position + 1;   // skip < 
				test_position2 < length &&
				tag_end[test_position1] != 0 &&
				result; 
				test_position1++, test_position2++)
			{
// null result when first wrong character is reached
//printf("FileXML::read_text_until 2 %c\n", string[test_position2]);
				if(tag_end[test_position1] != string[test_position2]) result = 0;
			}

// no end tag reached to copy <
			if(!result)
			{
				output[out_position++] = string[position++];
			}
		}
	}
	output[out_position] = 0;
// if end tag is reached, position is left on the < of the end tag
	return 0;
}


int FileXML::write_to_file(const char *filename)
{
	strcpy(this->filename, filename);
	FILE *out = fopen(filename, "wb");
	if( out != 0 )
	{
		fprintf(out, "<?xml version=\"1.0\"?>\n");
// Position may have been rewound after storing so we use a strlen
		if(!fwrite(string, strlen(string), 1, out) && strlen(string))
		{
			eprintf("write_to_file %d \"%s\": %m\n", __LINE__, filename);
			fclose(out);
			return 1;
		}
		else
		{
		}
	}
	else
	{
		eprintf("write_to_file %d \"%s\": %m\n", __LINE__, filename);
		return 1;
	}
	fclose(out);
	return 0;
}

int FileXML::write_to_file(FILE *file)
{
	strcpy(filename, "");
	fprintf(file, "<?xml version=\"1.0\"?>\n");
// Position may have been rewound after storing
	if(fwrite(string, strlen(string), 1, file) || !strlen(string))
	{
		return 0;
	}
	else
	{
		eprintf("\"%s\": %m\n", filename);
		return 1;
	}
	return 0;
}

int FileXML::read_from_file(const char *filename, int ignore_error)
{
	
	strcpy(this->filename, filename);
	FILE *in = fopen(filename, "rb");
	if( in != 0 )
	{
		fseek(in, 0, SEEK_END);
		int new_length = ftell(in);
		fseek(in, 0, SEEK_SET);
		reallocate_string(new_length + 1);
		(void)fread(string, new_length, 1, in);
		string[new_length] = 0;
		position = 0;
		length = new_length;
	}
	else
	{
		if(!ignore_error) 
			eprintf("\"%s\" %m\n", filename);
		return 1;
	}
	fclose(in);
	return 0;
}

int FileXML::read_from_string(char *string)
{
	strcpy(this->filename, "");
	reallocate_string(strlen(string) + 1);
	strcpy(this->string, string);
	length = strlen(string);
	position = 0;
	return 0;
}

int FileXML::set_shared_string(char *shared_string, long available)
{
	strcpy(this->filename, "");
	if(!share_string)
	{
		delete [] string;
		share_string = 1;
		string = shared_string;
		this->available = available;
		length = available;
		position = 0;
	}
	return 0;
}



// ================================ XML tag


XMLTag::XMLTag()
{
	total_properties = 0;
	len = 0;
}

XMLTag::~XMLTag()
{
	reset_tag();
}

int XMLTag::set_delimiters(char left_delimiter, char right_delimiter)
{
	this->left_delimiter = left_delimiter;
	this->right_delimiter = right_delimiter;
	return 0;
}

int XMLTag::reset_tag()     // clear all structures
{
	len = 0;
	for(int i = 0; i < total_properties; i++) delete [] tag_properties[i];
	for(int i = 0; i < total_properties; i++) delete [] tag_property_values[i];
	total_properties = 0;
	return 0;
}

int XMLTag::write_tag()
{
	int i, j;
	char *current_property, *current_value;
	int has_space;

// opening bracket
	string[len] = left_delimiter;        
	len++;
	
// title
	for(i = 0; tag_title[i] != 0 && len < MAX_LENGTH; i++, len++) string[len] = tag_title[i];

// properties
	for(i = 0; i < total_properties && len < MAX_LENGTH; i++)
	{
		string[len++] = ' ';         // add a space before every property
		
		current_property = tag_properties[i];

// property title
		for(j = 0; current_property[j] != 0 && len < MAX_LENGTH; j++, len++)
		{
			string[len] = current_property[j];
		}
		
		if(len < MAX_LENGTH) string[len++] = '=';
		
		current_value = tag_property_values[i];

// property value
// search for spaces in value
		for(j = 0, has_space = 0; current_value[j] != 0 && !has_space; j++)
		{
			if(current_value[j] == ' ') has_space = 1;
		}

// Is it 0 length?
		if(current_value[0] == 0) has_space = 1;

// add a quote if space
		if(has_space && len < MAX_LENGTH) string[len++] = '\"';
// write the value
		for(j = 0; current_value[j] != 0 && len < MAX_LENGTH; j++, len++)
		{
			string[len] = current_value[j];
		}
// add a quote if space
		if(has_space && len < MAX_LENGTH) string[len++] = '\"';
		
		
	}     // next property
	
	if(len < MAX_LENGTH) string[len++] = right_delimiter;   // terminating bracket
	return 0;
}

int XMLTag::read_tag(char *input, long &position, long length)
{
	long tag_start;
	int i, j, terminating_char;

// search for beginning of a tag
	while(input[position] != left_delimiter && position < length) position++;
	
	if(position >= length) return 1;

// find the start
	while(position < length &&
		(input[position] == ' ' ||         // skip spaces
		input[position] == '\n' ||	 // also skip new lines
		input[position] == left_delimiter))           // skip <
		position++;

	if(position >= length) return 1;
	
	tag_start = position;
	
// read title
	for(i = 0; 
		i < MAX_TITLE && 
		position < length && 
		input[position] != '=' && 
		input[position] != ' ' &&       // space ends title
		input[position] != right_delimiter;
		position++, i++)
	{
		tag_title[i] = input[position];
	}
	tag_title[i] = 0;
	
	if(position >= length) return 1;
	
	if(input[position] == '=')
	{
// no title but first property
		tag_title[0] = 0;
		position = tag_start;       // rewind
	}

// read properties
	for(i = 0;
		i < MAX_PROPERTIES &&
		position < length &&
		input[position] != right_delimiter;
		i++)
	{
// read a tag
// find the start
		while(position < length &&
			(input[position] == ' ' ||         // skip spaces
			input[position] == '\n' ||         // also skip new lines
			input[position] == left_delimiter))           // skip <
			position++;

// read the property description
		for(j = 0; 
			j < MAX_LENGTH &&
			position < length &&
			input[position] != right_delimiter &&
			input[position] != ' ' &&
			input[position] != '\n' &&	// also new line ends it
			input[position] != '=';
			j++, position++)
		{
			string[j] = input[position];
		}
		string[j] = 0;


// store the description in a property array
		tag_properties[total_properties] = new char[strlen(string) + 1];
		strcpy(tag_properties[total_properties], string);

// find the start of the value
		while(position < length &&
			(input[position] == ' ' ||         // skip spaces
			input[position] == '\n' ||         // also skip new lines
			input[position] == '='))           // skip =
			position++;

// find the terminating char
		if(position < length && input[position] == '\"')
		{
			terminating_char = '\"';     // use quotes to terminate
			if(position < length) position++;   // don't store the quote itself
		}
		else 
			terminating_char = ' ';         // use space to terminate

// read until the terminating char
		for(j = 0;
			j < MAX_LENGTH &&
			position < length &&
			input[position] != right_delimiter &&
			input[position] != '\n' &&
			input[position] != terminating_char;
			j++, position++)
		{
			string[j] = input[position];
		}
		string[j] = 0;

// store the value in a property array
		tag_property_values[total_properties] = new char[strlen(string) + 1];
		strcpy(tag_property_values[total_properties], string);
		
// advance property if one was just loaded
		if(tag_properties[total_properties][0] != 0) total_properties++;

// get the terminating char
		if(position < length && input[position] != right_delimiter) position++;
	}

// skip the >
	if(position < length && input[position] == right_delimiter) position++;

	if(total_properties || tag_title[0]) 
		return 0; 
	return 1;
}

int XMLTag::title_is(const char *title)
{
	if(!strcasecmp(title, tag_title)) return 1;
	else return 0;
}

char* XMLTag::get_title()
{
	return tag_title;
}

int XMLTag::get_title(char *value)
{
	if(tag_title[0] != 0) strcpy(value, tag_title);
	return 0;
}

int XMLTag::test_property(char *property, char *value)
{
	int i, result;
	for(i = 0, result = 0; i < total_properties && !result; i++)
	{
		if(!strcasecmp(tag_properties[i], property) && !strcasecmp(value, tag_property_values[i]))
		{
			return 1;
		}
	}
	return 0;
}

const char* XMLTag::get_property(const char *property, char *value)
{
	int i, result;
	for(i = 0, result = 0; i < total_properties && !result; i++)
	{
		if(!strcasecmp(tag_properties[i], property))
		{
//printf("XMLTag::get_property %s %s\n", tag_properties[i], tag_property_values[i]);
			decode_data(value, tag_property_values[i]);
			result = 1;
		}
	}
	return value;
}

const char* XMLTag::get_property_text(int number)
{
	if(number < total_properties) 
		return tag_properties[number];
	else
		return "";
}

int XMLTag::get_property_int(int number)
{
	if(number < total_properties) 
		return atol(tag_properties[number]);
	else
		return 0;
}

float XMLTag::get_property_float(int number)
{
	if(number < total_properties) 
		return atof(tag_properties[number]);
	else
		return 0;
}

char* XMLTag::get_property(const char *property)
{
	int i, result;
	for(i = 0, result = 0; i < total_properties && !result; i++)
	{
		if(!strcasecmp(tag_properties[i], property))
		{
			return tag_property_values[i];
		}
	}
	return 0;
}


int32_t XMLTag::get_property(const char *property, int32_t default_)
{
	temp_string[0] = 0;
	get_property(property, temp_string);
	if(temp_string[0] == 0) 
		return default_;
	else 
		return atol(temp_string);
}

int64_t XMLTag::get_property(const char *property, int64_t default_)
{
	int64_t result;
	temp_string[0] = 0;
	get_property(property, temp_string);
	if(temp_string[0] == 0) 
		result = default_;
	else 
	{
		result = strtoll(temp_string,0,0);
	}
	return result;
}
// 
// int XMLTag::get_property(char *property, int default_)
// {
// 	temp_string[0] = 0;
// 	get_property(property, temp_string);
// 	if(temp_string[0] == 0) return default_;
// 	else return atol(temp_string);
// }
// 
float XMLTag::get_property(const char *property, float default_)
{
	temp_string[0] = 0;
	get_property(property, temp_string);
	if(temp_string[0] == 0) 
		return default_;
	else 
		return atof(temp_string);
}

double XMLTag::get_property(const char *property, double default_)
{
	temp_string[0] = 0;
	get_property(property, temp_string);
	if(temp_string[0] == 0) 
		return default_;
	else 
		return atof(temp_string);
}

int XMLTag::set_title(const char *text)       // set the title field
{
	strcpy(tag_title, text);
	return 0;
}

int XMLTag::set_property(const char *text, int32_t value)
{
	sprintf(temp_string, "%d", value);
	set_property(text, temp_string);
	return 0;
}

int XMLTag::set_property(const char *text, int64_t value)
{
	sprintf(temp_string, "" _LD "", value);
	set_property(text, temp_string);
	return 0;
}

int XMLTag::set_property(const char *text, float value)
{
	if (value - (float)((int64_t)value) == 0)
		sprintf(temp_string, "" _LD "", (int64_t)value);
	else
		sprintf(temp_string, "%.6e", value);
	set_property(text, temp_string);
	return 0;
}

int XMLTag::set_property(const char *text, double value)
{
	if (value - (double)((int64_t)value) == 0)
		sprintf(temp_string, "" _LD "", (int64_t)value);
	else
		sprintf(temp_string, "%.16e", value);
	set_property(text, temp_string);
	return 0;
}

int XMLTag::set_property(const char *text, const char *value)
{
	tag_properties[total_properties] = new char[strlen(text) + 1];
	strcpy(tag_properties[total_properties], text);
	int len = encoded_length(value);
	tag_property_values[total_properties] = new char[len + 1];
	encode_data(tag_property_values[total_properties], value);
	total_properties++;
	return 0;
}

// closer to the real deal

static inline int xml_cmp(const char *np, const char *sp)
{
   const char *tp = ++np;
   while( *np ) { if( *np != *sp ) return 0;  ++np; ++sp; }
   return np - tp;
}

char *XMLTag::decode_data(char *bp, const char *sp, int n)
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


char *XMLTag::encode_data(char *bp, const char *sp, int n)
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

long XMLTag::encoded_length(const char *sp, int n)
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
