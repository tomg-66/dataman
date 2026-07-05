/*
 * this is an analyzer and editor for database files
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
#include <stdlib.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include <inttypes.h>

#include "srv_index.h"
#include "lock.h"
#include "misc.h"

/* these are the master file parameters */

short *m_desc;			/* description of master file */
short m_head;			/* length of the file header */
int64_t m_end;			/* pointer to end of master file */
int64_t m_cur;			/* pointer ot current master record */
int64_t m_prev;			/* pointer to prev master record */
int64_t m_next;			/* pointer to next master record */
int m_chan;				/* channel number of master file */
int  m_len;				/* length of current record */
short m_longest;		/* length of longest record */
char m_fmt;				/* curr master record format number */
char **mfld;			/* pointer to the individual fields */
char *_m_rec_;			/* actual data record read from file */
int protected;
int deleted;

char MSK1 [] = "__________0_";

#define F1  0273
#define F2  0274
#define F3  0275
#define F4  0276
#define F5  0277
#define F6  0300
#define F7  0301
#define F8  0302
#define F9  0303
#define F10	0304
#define HOME    0307

#define TOP	"\377"
#define EOL	"\376"
#define DEL 0200				/* this is the mask for a deleted record */
#define PRTCT 0100
#define MASTER	0
#define NUMERIC 3
#define ENDLIST	-1

#define accept(row,col,buf,mode)        if (acept(row,col,buf,mode)) ;

int fno;			/* field number */
char dirty;			/* another dirty flag */

int dbgsw;
char *_progname;

extern void show(int, int, char *, ...);
extern void pause(int, int, char *);
extern int acept(int, int, char *, int);
extern int get_datafile_desc(FILES *);
extern void db_err(int, char *, ...);
extern void init_dwin(void);
extern ssize_t read(int, void *, size_t);
extern ssize_t write(int, const void *, size_t);

extern short get_short(char *);
extern void put_short(char *, short);

extern int64_t get_ll(void *);
extern void put_ll(void *, int64_t);

char *mask(int64_t, char *);
static int get_rec(FILEDESC *);
static int update(FILEDESC *);
static int disp_it(int);

void alrm_handle(int sig)
{
	if (sig != SIGALRM)
		return;
	printf("Can't obtain lock on file\n");
	exit(0);
}

int main(int argc, char *argv[])

{
	unsigned char inpt[999];	/* basic buffer for evrything */
	unsigned char path[512];	/* path to file to open */
	char *cp;					/* pointer returned from getenv() */

	int64_t tmp;					/* misc usage */

	int idx;

	char c;

	FILES f_desc;					/* file description */
	FILEDESC *p_desc;				/* pointer to the parsed description */

	_progname = strdup(argv[0]);
	dirty = 0;			/* start false */
	if (argc != 2) {
		printf("Usage: dfedit data_file\n");
		exit(0);
	}

	cp = getenv("ROOT");
	if (cp == NULL) {
		printf("ROOT not Defined\n");
		exit(0);
	}
	strcpy(inpt,cp);

	sprintf(path, "%s/files/%s", cp, argv[1]);
	f_desc._fname = strdup(path);

#if !defined __gnu_linux__
	check_endian();
#endif

/*
 * this opens up and parses the datafile header.  after that we want
 * an exclusive lock on the file so that no other competing process
 * can do anything to the file.
 */
	if ((idx = get_datafile_desc(&f_desc)) < 0) {
		db_err(idx, "dfedit: can't open and parse file");
	}
/*
 * get_datafield_desc -DOES NOT- get the pointer to the first record in
 * the file, so we need to get it ourselves.
 */
	m_next = f_desc._hlen + sizeof(short);
	m_chan = f_desc._chan;
	p_desc = f_desc._filedesc;

	llseek(m_chan, m_next, SEEK_SET);
	if (read(m_chan, path, sizeof(int64_t)) != sizeof(int64_t)) {
		printf("Can't read first record position\n");
		exit(0);
	}
	m_next = get_ll(path);
	m_longest = f_desc._longest;
	m_prev = 0;
/*
 * we need some buffer space to read a record into while we parse it.
 */
	if ((_m_rec_ = (char *)malloc(m_longest)) == NULL) {
		db_err(0, "Can't allocate record buffer");
	}

	init_dwin();
	memset(inpt,'_',80);		/* screen seperator */
	*(inpt+80) = '\0';			/* terminate string */
/*
 * display the main menu
 */
	show(0,0,TOP,0,27,"Editing File ",0,40,argv[1],
		2,23,"Enter Record Number:",
		4,9,"F1- Go To Prior Record",	4,49,"F5- Edit Field",
		5,9,"F2- Go To Next Record",	5,49,"F6- Change Back Link",
		6,9,"F3- Go To Prior Field",	6,49,"F7- Change Next Link",
		7,9,"F4- Go To Next Field",		7,49,"F8- Delete Record",
	    8,33,"F9- End",
		9,0,inpt,
		10,3,"Back Pointer:", 10,30,"Record Number:",
		11,3,"Next Pointer:", 11,30,"Format #:",11,58,"Field #:",
		ENDLIST);
/*
 * this is what accept does for function key returns....
 *
	if (buf > KEY_F0 && buf <= KEY_F(12)) {
			c = 0200 | (buf - 0316);
*/
	while (1) {
		memset(inpt,' ',10);
		*(inpt+10) = '\0';
		accept(2,44,inpt,NUMERIC) else
			continue;			/* must use end key to exit */

		switch(*inpt) {
/*
 * move to the prior record
 */
			case F1:
				if(m_prev == 0) {
					pause(20,31,"On First Record!");
					show(20,0,EOL,ENDLIST);
					break;
				}
				update(p_desc);
				m_cur = m_prev;
				get_rec(p_desc);
				break;
/*
 * move to the next record
 */
			case F2:
				if (m_next == 0) {
					pause (20,31,"On Last Record!");
					show(20,0,EOL,ENDLIST);
					break;
				}
				update(p_desc);
				m_cur = m_next;
				get_rec(p_desc);
				break;
/*
 * point to the prior  datafield
 */
			case F3:
				if (m_cur == 0) {
					pause(20,32,"Before First Record!");
					break;
				}
				if (fno == 1) {
					pause(20,32,"On First Field!");
					show(20,0,EOL,ENDLIST);
					break;
				}
				fno--;
				disp_it(2);
				break;
/*
 * point to the next datafield
 */
			case F4:
				if (m_cur == 0) {
					pause(20,32,"Before First Record!");
					break;
				}
				if (mfld[++fno] == NULL) {
					pause(20,32,"On Last Field!");
					show(20,0,EOL,ENDLIST);
					fno--;
					break;
				}
				disp_it(2);
				break;
/*
 * edit the field data
 */
			case F5:
				if (*(mfld[fno]) == '\0') {
					pause(20,30, "Can't edit Blob Data");
					show(20,0,EOL, ENDLIST);
					break;
				}
				memset(inpt,' ',strlen(mfld[fno]));
				*(inpt+strlen(mfld[fno])) = '\0';
				accept(18,0,inpt,0) else {
					show(18,0,EOL,19,0,EOL,20,0,EOL,ENDLIST);
					break;
				}
				dirty = 1;
				memset(mfld[fno],' ',strlen(mfld[fno]));
				strncpy(mfld[fno],inpt,strlen(inpt));
				show(18,0,EOL,19,0,EOL,20,0,EOL,13,0,mfld[fno],ENDLIST);
				break;
/*
 * change the back pointer
 */
			case F6:
				memset(inpt,' ',12);
				*(inpt+12) = '\0';
				accept(10,17,inpt,3) else
					sprintf(inpt, "%"PRId64, m_prev);
				m_prev = strtoll(inpt, NULL, 10);
				strcpy(inpt,MSK1);
				show(10,17,mask(m_prev,inpt),ENDLIST);
				put_ll(inpt,m_prev);
				llseek(m_chan,m_cur+OFFSET_TO_PREV,SEEK_SET);
				write(m_chan,inpt,sizeof(int64_t));
				break;
/*
 * change the next pointer
 */
			case F7:
				memset(inpt,' ',12);
				*(inpt+12) = '\0';
				accept(11,17,inpt,3) else
					sprintf(inpt, "%"PRId64, m_next);
				m_next = strtoll(inpt, NULL, 10);
				strcpy(inpt,MSK1);
				show(11,17,mask(m_next,inpt),ENDLIST);
				llseek(m_chan,m_cur+OFFSET_TO_NEXT,0);
				put_ll(inpt, m_next);
				write(m_chan,inpt,sizeof(int64_t));
				break;
/*
 * delete a record from the database
 */
			case F8:
				if (m_next == 0 && m_prev == 0) {
					pause(20,22,"Cannot Delete Only Record In File!");
					show(20,0,EOL);
					break;
				}
				tmp = 0;
				llseek(m_chan,m_cur,SEEK_SET);
				read(m_chan, inpt, DATARECORD_FLAG_LENGTH);
				*inpt |= DEL;
				llseek(m_chan, m_cur, SEEK_SET);
				write(m_chan,inpt,DATARECORD_FLAG_LENGTH);
				if (m_next == 0) {
					llseek(m_chan,m_prev+OFFSET_TO_NEXT,SEEK_SET);
					put_ll(inpt,m_next);
					write(m_chan,inpt,sizeof(int64_t));
					m_cur = m_prev;
				} else if (m_prev == 0) {
					llseek(m_chan,m_next+OFFSET_TO_PREV,SEEK_SET);
					put_ll(inpt,m_prev);
					write(m_chan,inpt,sizeof(int64_t));
					m_cur = m_next;
					tmp = m_next;
				} else {
					llseek(m_chan,m_prev+OFFSET_TO_NEXT,SEEK_SET);
					put_ll(inpt,m_next);
					write(m_chan,inpt,sizeof(int64_t));
					llseek(m_chan,m_next+OFFSET_TO_PREV,SEEK_SET);
					put_ll(inpt,m_prev);
					write(m_chan,inpt,sizeof(int64_t));
					m_cur = m_next;
				}
				if (tmp) {
					llseek(m_chan,m_head+2,0);
					put_ll(inpt,tmp);
					write(m_chan,inpt,sizeof(int64_t));
				}
				get_rec(p_desc);
				break;
/*
 * the user wants to quit
 */
			case F9:
				update(p_desc);
				exit(0);
/*
 * the user input a record number
 */
			default:
				tmp = atoi(inpt);
				if (tmp == 0)
					break;
				update(p_desc);
				m_cur = tmp;
				get_rec(p_desc);
				break;
		}
	}
}

/*
 * set up for a new record
 */
static int get_rec(FILEDESC *p_desc)
{
	void in_rec(FILEDESC *);

	in_rec(p_desc);
	fno = 1;
	disp_it(1);
}

/*
 * verify if the user wants to save the changes to the datarecord
 */
static int update(FILEDESC *p_desc)
{
	void out_rec(FILEDESC *);
	char sel[2];

	if (dirty) {
		*sel = ' ';
		*(sel+1) = '\0';
		show(20,27,"Save Changes (Y,[N])?:",ENDLIST);
		accept(20,50,sel,2);
		show(20,0,EOL,ENDLIST);
		if (*sel == 'Y')
			mfld[0] = (char *)((uintptr_t)mfld[0] | 1);
	}
	out_rec(p_desc);
	dirty = 0;
}

/*
 * do some onscreed display
 */
static int disp_it(int val)
{
	char msk[12];
	char *mask();
	int64_t disp;				/* mask needs a long */

	switch(val) {
		case 1:
			strcpy(msk,MSK1);
			show(10,17,mask(m_prev,msk),ENDLIST);
			if (deleted)
				show(10,45, "Deleted     ", ENDLIST);
			else {
				strcpy(msk,MSK1);
				show(10,45,mask(m_cur,msk),ENDLIST);
			}

			strcpy(msk,MSK1);
			show(11,17,mask(m_next,msk),ENDLIST);

			strcpy(msk,"0_");
			disp = m_fmt;
			show(11,40,mask(disp,msk),ENDLIST);

		case 2:
			strcpy(msk,"_0_");
			disp = fno;
			show(11,72,mask(disp,msk),ENDLIST);
			if (*(mfld[fno]) == '\0') 
				show(13,0,EOL,14,0,EOL,15,0,EOL,13,0,"Blob Data",ENDLIST);
			else
				show(13,0,EOL,14,0,EOL,15,0,EOL,13,0,mfld[fno],ENDLIST);
    }
}

/*
 * read a record in from the database
 */
void in_rec(FILEDESC *desc)
{
	int i, j;
	RFDESC *rfdesc;
	char buff[DATARECORD_HEADER_LENGTH];

	llseek(m_chan, m_cur, SEEK_SET);
	read(m_chan, buff, DATARECORD_HEADER_LENGTH);
	m_fmt = *buff & 077;
	m_prev = get_ll(buff+OFFSET_TO_PREV);
	m_next = get_ll(buff+OFFSET_TO_NEXT);
	deleted = *buff & DEL;
	if (deleted) {
		m_prev = 0;
		m_next = 0;
	}

	rfdesc = desc->record_desc+m_fmt-1;
	read(m_chan, _m_rec_, rfdesc->rf_len);
	mfld = (char **)calloc((rfdesc->n_fields)+2, sizeof(char *));
	*mfld = NULL;
	*(mfld+rfdesc->n_fields+1) = NULL;

	for (i=0, j=0; i < rfdesc->n_fields; i++) {
		mfld[i+1] = calloc(1, rfdesc->field_sizes[i]+1);
		memcpy(mfld[i+1], _m_rec_+j, rfdesc->field_sizes[i]);
		j += rfdesc->field_sizes[i];
	}
}

/*
 * put a record back into the database
 */
void out_rec(FILEDESC *desc) {
	int i, j;

	RFDESC *rfdesc;

	if (mfld == NULL || *mfld == NULL)
		return;
	rfdesc = desc->record_desc+m_fmt-1;
	memset(_m_rec_, '\0', m_longest);
	for (j = 0, i = 0; i < rfdesc->n_fields; i++) {
		memcpy(_m_rec_+j, mfld[i+1], rfdesc->field_sizes[i]);
		free(mfld[i+1]);
		j += rfdesc->field_sizes[i];
	}
	llseek(m_chan, m_cur+DATARECORD_HEADER_LENGTH, SEEK_SET);
	i = write(m_chan, _m_rec_, rfdesc->rf_len);
	free(mfld);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
