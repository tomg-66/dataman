/* ***************************************************************
 *
 * PROCEDURE:	datafield_impl.cc
 *
 * PROJECT:		dataman client side c++ routine
 * 
 * DATE:		Wed Jul  7 16:41:40 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Wed Jul 28 18:39:13 MDT 2004
 * 				added new functions to do strcpy, and so forth
 * 				tomg
 *
 * 				Sat Mar 26 15:31:52 MST 2005
 * 				added support for blobs.  added the put_blob
 * 				function, and made checks for blobs in the other
 * 				operators to make sure you can't do stuff you
 * 				ought not.
 * 				tomg
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */
/*
 * this implements the interface routines to the datafield type
 * it includes +,*,/,=, and [].  There, of course should be some
 * new ones coming any time soon.
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <datafield.hh>
#include <datarecord.hh>
#include <fileEdit.hh>

using namespace Dataman;

#include "../../server/errors.h"

#if !defined min
#define min(x,y)	((x)<(y)?(x):(y))
#endif

extern void db_err(int, const char *, ...);

static int trmlen(const char *s)
{
	if (s) {
		const char *p = s+strlen(s)-1;
		while(*p == ' ' && p >= s)
			p--;
		p++;
		return(p-s);
	}
	return(0);
}

datafield::datafield(void) {
	length = 0;
	type = type_non;
	data = NULL;
}
//
//construct from string, offset, length
//
datafield::datafield(const char *p, int offset = 0, int len = 0) {
	if (p) {
		if (len == 0 && offset ==0)
			len = strlen(p);
		if (len > strlen(p+offset))
			len = strlen(p+offset);
				
		length = len;
		data = new char[len+1];
		::memcpy(data, p+offset, len);
		*(data+len) = '\0';
		type = type_chr;
	} else {
		length = 0;
		data = NULL;
		type = type_non;
	}
}
//
//standard copy constructor
//
datafield::datafield(const datafield&d) {
	this->length = d.length;
	data = new char[this->length+1];
	::memcpy(this->data, d.data, this->length);
	*(this->data+this->length) = '\0';
	this->type = d.type;
}
//
//destructor
//
datafield::~datafield() {
	if (data)
		delete[] data;
}
/*
 * is this datafield a work record or master record, and what
 * field number is it? < 0 it's in the workfile, == not a
 * field.  > 0 is in the master record.
 */
/*
int datafield::locate(void)
{
	int i;
	FILEDESC *fptr;
	i = (char *)this - (char *)(master.field);
	i /= sizeof(class datafield);
	fptr = master.get_desc();
	if (this - i == master.field) {
		if (fptr && (i < 1 || i > fptr->record_desc[master.getfmt()-1].n_fields))
			return(0);
		return(i);
	} else {
		fptr = workfile.get_desc();
		i = (char *)this - (char *)workfile.field;
		i /= sizeof(class datafield);
		if (this -i == workfile.field) {
			if (fptr && (i < 1 || i > fptr->record_desc[workfile.getfmt()-1].n_fields))
				return(0);
			return(-i);
		} else {
			return(0);
		}
	}
}
*/
//
//assignment from another datafield
//
void datafield::operator=(const datafield& d)
{
	if (this->type == type_blob || d.type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (this->data) {
		memset(this->data, ' ', this->length);
		::strncpy(this->data, d.data, min(this->length, d.length));
	} else {
		this->length = d.length;
		this->data = new char[d.length+1];
		strcpy(this->data, d.data);
	}
	this->type = d.type;
	if (this->which == MASTER)
		master.setdirty(true);
	else
		workfile.setdirty(true);
}

//
//assign a datafield from a character string
//
void datafield::operator=(const char *s)
{
	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (this->data) {
		memset(this->data, ' ', this->length);
		if (s)
			::strncpy(this->data, s, min(this->length, strlen(s)));
	} else if (s) {
		this->length = strlen(s);
		this->data = new char[this->length+1];
		strcpy(this->data, s);
	}
	this->type = type_chr;
	if (this->which == MASTER)
		master.setdirty(true);
	else
		workfile.setdirty(true);

}

//
//assign a datafield from an int
//
void datafield::operator=(int i)
{
	char buff[32];
	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	sprintf(buff, "%d", i);
	if (this->data) {
		memset(this->data, ' ', this->length);
		::strncpy(this->data, buff, min(this->length, strlen(buff)));
	} else {
		this->length = strlen(buff);
		this->data = new char[this->length+1];
		strcpy(this->data, buff);
	}
	this->type = type_int;
	if (this->which == MASTER)
		master.setdirty(true);
	else
		workfile.setdirty(true);
}

//
//assign a datafield from a float
//
void datafield::operator=(float f)
{
	char buff[64];
	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	sprintf(buff, "%f", f);
	if (this->data) {
		memset(this->data, ' ', this->length);
		::strncpy(this->data, buff, min(this->length, strlen(buff)));
	} else {
		this->length = strlen(buff);
		this->data = new char[this->length+1];
		strcpy(this->data, buff);
	}
	this->type = type_flt;
	if (this->which == MASTER)
		master.setdirty(true);
	else
		workfile.setdirty(true);
}

//
// make a new field in in_rec.  this is only used there.
//
void datafield::make_field(const char *ptr, int len, int w)
{
	if (len == 0)
		return;

	if (len < 0) {
		this->length = -len;
		this->data = new char[this->length];
		::memcpy(this->data, (char *)ptr, this->length);
		this->type = type_blob;
	} else {
		this->data = new char[len+1];
		::memcpy(this->data, (char *)ptr, len);
		*(this->data+len) = '\0';
		this->length = len;
		this->type = type_chr;
	}
	this->which = w;
}

/*
 * put a blob to the field.  we can't just assign it via = because
 * we need to know what the length of the blob is.
 */
int datafield::put_blob(const void *ptr, int len)
{
	if (this->type != type_blob && this->type != type_non)
		return(0);
	if (this->data != NULL)
		delete[] (this->data);
	this->length = len;
	this->data = new char[len];
	::memcpy(this->data, ptr, len);
	if (this->which == MASTER)
		master.setdirty(true);
	else
		workfile.setdirty(true);
	this->type = type_blob;
	return(1);
}

//
//start the 'addition' operators here.  a string + string is a 
//concatanation.  adding an int or float to a string converts the 
//string to an int or float and performs the op.  if the first op
//is either an int or float, then convert the second op to the
//appropriate op before performing the operation.
//
datafield datafield::operator+(const datafield& d)
{
	datafield ret;

	if (this->type == type_blob || d.type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	int sw = ((this->type & TYPE_MASK) << 3) | d.type;

	switch(sw) {
		int i, j;
		float f;
		char buff[64];

		case 000:
			i = trmlen(this->data);
			j = min(i, trmlen(d.data));
			ret.type = type_chr;
			ret.length = i+j;
			ret.data = new char[i+j+1];
			::memcpy(ret.data, this->data, i);
			::strncpy(ret.data+i, d.data, j);
			break;
		case 001:
		case 011:
			i = atoi(this->data) + atoi(d.data);
			sprintf(buff, "%d", i);
			ret.type = type_int;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case 002:
		case 022:
			f = (float)atof(this->data) + (float)atof(d.data);
			sprintf(buff, "%f", f);
			ret.type = type_flt;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case 010:
			if ((i = atoi(d.data)) == 0) {
				ret = *this;
				break;
			}
			i += atoi(this->data);
			sprintf(buff, "%d", i);
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case 020:
			if ((f = atof(d.data)) == 0.0) {
				ret = *this;
				break;
			}
			f += (float)atof(this->data);
			sprintf(buff, "%f", f);
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case 012:
			i = atoi(this->data) + (int)((float)atof(d.data));
			sprintf(buff, "%d", i);
			ret.type = type_int;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case 021:
			f = (float)atof(ret.data) + atoi(d.data);
			sprintf(buff, "%f", f);
			ret.type = type_flt;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
	}
	return(ret);
}


datafield datafield::operator+(const char *s)
{
	datafield ret;

	if (!s)
		return(*this);

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);

	switch(this->type) {
		int i, j;
		float f;
		char buff[64];
		case type_chr:
			i = trmlen(this->data);
			j = strlen(s);
			ret.type = type_chr;
			ret.length = i+j;
			ret.data = new char[i+j+1];
			::memcpy(ret.data, this->data, i);
			::strncpy(ret.data+i, s, j);
			break;
		case type_int:
			i = atoi(this->data) + atoi(s);
			sprintf(buff, "%d", i);
			ret.type = type_int;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case type_flt:
			f = (float)atof(this->data) + (float)atof(s);
			sprintf(buff, "%f", f);
			ret.type = type_flt;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
	}
	return(ret);
}

datafield datafield::operator+(int v)
{
	datafield ret;

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	switch(this->type) {
		int i;
		float f;
		char buff[64];
		case type_chr:
		case type_int:
			i = atoi(this->data) + v;
			sprintf(buff, "%d", i);
			ret.type = type_int;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case type_flt:
			f = (float)atof(this->data) + v;
			sprintf(buff, "%f", f);
			ret.type = type_flt;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
	}
	return(ret);
}

datafield datafield::operator+(float v)
{
	datafield ret;

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	switch(this->type) {
		int i;
		float f;
		char buff[64];
		case type_int:
			i = (int)((float)atof(this->data) + v);
			sprintf(buff, "%d", i);
			ret.type = type_int;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
		case type_chr:
		case type_flt:
			f = (float)atof(this->data) + v;
			sprintf(buff, "%f", f);
			ret.type = type_flt;
			i = strlen(buff);
			ret.data = new char[i+1];
			strcpy(ret.data, buff);
			ret.length = i;
			break;
	}
	return(ret);
}

datafield datafield::operator*(const datafield& f)
{
	datafield ret;
	
	if (this->type == type_blob || f.type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (strchr(this->data, '.') || strchr(f.data, '.')) {
		float result =  atof(this->data) * atof(f.data);
		ret = result;
	} else {
		int result = atoi(this->data) * atoi(f.data);
		ret = result;
	}
	return(ret);
}

datafield datafield::operator*(const char * s)
{
	datafield ret;
	
	if (!s)
		return(*this);

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);

	if (strchr(this->data, '.') || strchr(s, '.')) {
		float result =  atof(this->data) * atof(s);
		ret = result;
	} else {
		int result = atoi(this->data) * atoi(s);
		ret = result;
	}
	return(ret);
}

datafield datafield::operator*(int i)
{
	datafield ret;

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (strchr(this->data, '.')) {
		float result = atof(this->data) * (float)i;
		ret = result;
	} else {
		int result = atoi(this->data) * i;
		ret = result;
	}
	return(ret);
}

datafield datafield::operator*(float f)
{
	datafield ret;
	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	ret = (const float)(atof(this->data) * f);
	return(ret);
}

//
//division operators
//
datafield datafield::operator/(const datafield& f)
{
	datafield ret;
	
	if (this->type == type_blob || f.type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (atof(f.data) == 0.0) {
		fprintf(stderr, "attempt to divide by zero!\n");
		exit(0);
	}
	if (strchr(this->data, '.') || strchr(f.data, '.')) {
		float result =  atof(this->data) / atof(f.data);
		ret = result;
	} else {
		int result = atoi(this->data) / atoi(f.data);
		ret = result;
	}
	return(ret);
}

datafield datafield::operator/(const char * s)
{
	datafield ret;
	
	if (!s)
		return(*this);

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);

	if (atof(s) == 0.0) {
		fprintf(stderr, "attempt to divide by zero!\n");
		exit(0);
	}
	if (strchr(this->data, '.') || strchr(s, '.')) {
		float result =  atof(this->data) / atof(s);
		ret = result;
	} else {
		int result = atoi(this->data) / atoi(s);
		ret = result;
	}
	return(ret);
}

datafield datafield::operator/(int i)
{
	datafield ret;

	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (i == 0) {
		fprintf(stderr, "attempt to divide by zero!\n");
		exit(0);
	}
	if (strchr(this->data, '.')) {
		float result = atof(this->data) / (float)i;
		ret = result;
	} else {
		int result = atoi(this->data) / i;
		ret = result;
	}
	return(ret);
}

datafield datafield::operator/(float f)
{
	if (this->type == type_blob)
		db_err(EBLOBTYP, "", NULL);
	if (f == 0.0) {
		fprintf(stderr, "attempt to divide by zero!\n");
		exit(0);
	}
	datafield ret;
	ret = (const float)(atof(this->data) / f);
	return(ret);
}

// relational operators
bool datafield::operator==(const datafield &d)
{
	return(strcmp(this->data, d.data) == 0);
}

/*
 * let a single space be equilivent to all spaces
 */
bool datafield::operator==(const char *s)
{
	if (!s)
		return(false);
	if (strcmp(s, " "))
		return(strcmp(this->data, s) == 0);
	char tmp[this->length+1];
	memset(tmp, ' ', this->length);
	*(tmp+this->length) = '\0';
	return(strcmp(s, tmp) == 0);
}

bool datafield::operator==(int i)
{
	return(atoi(this->data) == i);
}

bool datafield::operator==(float f)
{
	return(atof(this->data) == f);
}

bool datafield::operator!=(const datafield &d)
{
	return(strcmp(this->data, d.data) != 0);
}

/*
 * let a single space be equilivent to all spaces
 */
bool datafield::operator!=(const char *s)
{
	if (!s)
		return(true);
	if (strcmp(s, " "))
		return(strcmp(this->data, s) != 0);
	char tmp[this->length+1];
	memset(tmp, ' ', this->length);
	*(tmp+this->length) = '\0';
	return(strcmp(s, tmp) != 0);
}

bool datafield::operator!=(int i)
{
	return(atoi(this->data) != i);
}

bool datafield::operator!=(float f)
{
	return(atof(this->data) != f);
}


const char *strcpy(datafield& d, const char *s)
{
	d = s;
	return(d.getptr());
}

char *strcpy(char *s, datafield& d)
{
	return(strcpy(s, d.getptr()));
}

char *strncpy(char *s, datafield& d, int i)
{
	return(strncpy(s, d.getptr(), i));
}

const char *Dataman::strncpy(datafield& d, const char *s, int i)
{
	if (s) {
		if (i > d.datalen())
			i = d.datalen();
		if (strlen(s) < i)
			i = strlen(s);
		::strncpy(d.data, s, i);
		if (d.get_which() == MASTER)
			master.setdirty(true);
		else
			workfile.setdirty(true);
	}
	return(d.getptr());
}

char *strcat(char *s, datafield& d)
{
	return(strcat(s, d.getptr()));
}

char *strncat(char *s, datafield&d, int i)
{
	return(strncat(s, d.getptr(), i));
}

int atoi(datafield& d)
{
	return(atoi(d.getptr()));
}

const void *Dataman::memcpy (datafield& d, const char *s, int i)
{
	if (s) {
		if (i > d.datalen())
			i = d.datalen();
		::memcpy(d.data, s, i);
		if (d.get_which() == MASTER)
			master.setdirty(true);
		else
			workfile.setdirty(true);
	}
	return((const void *)d.getptr());
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
