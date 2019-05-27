/*
 * Copyright (c) 2019 Freeman Tham(gregtham91@gmail.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __CFGFILE_H__
#define __CFGFILE_H__

#ifdef __cplusplus
       extern "C" {
#endif

#include <fcntl.h>
#include <stdarg.h>
#include <sys/types.h>


#define	CFG_FLOCK_BSD	0		//enable flock or not
#define	CFG_FLOCK_POSIX	1		//enable flock or not, using posix standard
#define	CFG_FMagic	0			//check file Magic
#define	CFG_MyFileLock	0		//my file lock for the whole /mnt/mtd/*.conf
#define CFG_MAX_LINE_LENGTH 1024
#define	CFG_ThreadLock	 1	// 0:no lock, 1:lock

#define	NULL_STR	'\0'
#define	NULL_VAL	0xFFFF
#define	MAX_FILE_NAME		128

#define	MTD_DIR			"/mnt/mtd/"
#define	MTD_CACHE_DIR		"/tmp/mtd/"

#if (CFG_MyFileLock==1)
typedef struct
{
	char LockFlag;
	char FileName[MAX_FILE_NAME];
}MyFileLock;

#define My_RDLCK		1
#define My_WRLCK		2
#define My_UNLCK		0

#define MyMaxCfgFile		32
#endif

/* configuration file entry */
typedef struct TCFGENTRY
  {
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;
  }
TCFGENTRY, *PCFGENTRY;

/* values for flags */
#define CFE_MUST_FREE_SECTION	0x8000
#define CFE_MUST_FREE_ID	0x4000
#define CFE_MUST_FREE_VALUE	0x2000
#define CFE_MUST_FREE_COMMENT	0x1000

/* configuration file */
typedef struct TCFGDATA
  {
    char *fileName;		// Current file name
    int fd;			//file handler, add by yingcai

    int dirty;			// Did we make modifications?

    char *image;		// In-memory copy of the file 
    size_t size;		// Size of this copy (excl. \0) 
    time_t mtime;		// Modification time 

    unsigned int numEntries;
    unsigned int maxEntries;
    PCFGENTRY entries;

    /* Compatibility */
    unsigned int cursor;
    char *section;
    char *id;
    char *value;
    char *comment;
    unsigned short flags;

  }
TCONFIG, *PCONFIG;

#define CFG_VALID		0x8000
#define CFG_EOF			0x4000

#define CFG_ERROR		0x0000
#define CFG_SECTION		0x0001
#define CFG_DEFINE		0x0002
#define CFG_CONTINUE		0x0003

#define CFG_TYPEMASK		0x000F
#define CFG_TYPE(X)		((X) & CFG_TYPEMASK)
#define cfg_valid(X)	((X) != NULL && ((X)->flags & CFG_VALID))
#define cfg_eof(X)	((X)->flags & CFG_EOF)
#define cfg_section(X)	(CFG_TYPE((X)->flags) == CFG_SECTION)
#define cfg_define(X)	(CFG_TYPE((X)->flags) == CFG_DEFINE)
#define cfg_cont(X)	(CFG_TYPE((X)->flags) == CFG_CONTINUE)

void Cfg_GetVer(void);
char * remove_quotes(const char *szString);
int GetPrivateProfileString (char * lpszSection, char * lpszEntry,   char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,    char * lpszFilename);
int GetPrivateProfileInt (char * lpszSection, char * lpszEntry,    int iDefault, char * lpszFilename);
int WritePrivateProfileString (char * lpszSection, char * lpszEntry,    char * lpszString, char * lpszFilename);

int cfg_init (PCONFIG * ppconf, const char *filename, int doCreate);
int cfg_done (PCONFIG pconfig);

int cfg_freeimage (PCONFIG pconfig);
int cfg_refresh (PCONFIG pconfig);
int cfg_storeentry (PCONFIG pconfig, char *section, char *id,char *value, char *comment, int dynamic);
int cfg_rewind (PCONFIG pconfig);
int cfg_nextentry (PCONFIG pconfig);
int cfg_find (PCONFIG pconfig, char *section, char *id);
int cfg_next_section (PCONFIG pconfig);
int cfg_write (PCONFIG pconfig, char *section, char *id, char *value);
int cfg_writelong ( PCONFIG pconfig,char *section,char *id,long value);
int cfg_writeint( PCONFIG pconfig,char *section,char *id,int value);
int cfg_writeshort ( PCONFIG pconfig,char *section,char *id,unsigned short value);
int cfg_writebyte ( PCONFIG pconfig,char *section,char *id,unsigned char value);
int cfg_commit (PCONFIG pconfig);
int cfg_copy(PCONFIG pconfig,char *newfilename);
int cfg_getstring (PCONFIG pconfig, char *section, char *id, char *valptr);
int cfg_getlong (PCONFIG pconfig, char *section, char *id, long *valptr);
int cfg_getint (PCONFIG pconfig, char *section, char *id, int *valptr);
int cfg_getshort (PCONFIG pconfig, char *section, char *id, unsigned short *valptr);
int cfg_getbyte(PCONFIG pconfig, char *section, char *id, unsigned char *valptr);
int cfg_get_item (PCONFIG pconfig, char *section, char *id, char * fmt, ...);
int cfg_write_item(PCONFIG pconfig, char *section, char *id, char * fmt, ...);

int list_entries (PCONFIG pCfg, const char * lpszSection, char * lpszRetBuffer, int cbRetBuffer);
int list_sections (PCONFIG pCfg, char * lpszRetBuffer, int cbRetBuffer);

//int GetPrivateProfileString (char * lpszSection, char * lpszEntry, char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer, char * lpszFilename);
//int GetPrivateProfileInt (char * lpszSection, char * lpszEntry,int iDefault, char * lpszFilename);
//int WritePrivateProfileString (char * lpszSection, char * lpszEntry, char * lpszString, char * lpszFilename);
int SafeGetPrivateProfileString(char * lpszSection, char * lpszEntry,char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,char * lpszFilename);
int SafeGetPrivateProfileInt (char * lpszSection, char * lpszEntry,int iDefault, char * lpszFilename);

//APIs//decimal number(not 0xHHHH)
int Cfg_ReadStr(char *File,char *Section,char *Entry,char *valptr);
int  Cfg_ReadInt(char *File,char *Section,char *Entry,long *valptr);
int Cfg_ReadShort(char *File,char *Section,char *Entry,unsigned short *valptr);
int Cfg_Readbyte(char *File,char *Section,char *Entry,unsigned char *valptr);
int Cfg_WriteInt(char *File,char *Section,char *Entry,long Value);
int Cfg_WriteHexInt(char *File,char *Section,char *Entry,int Value);
int Cfg_WriteStr(char *File,char *Section,char *Entry,char *valptr);

/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 0:success, -nnnn:err code.
*/
int ini_setstr(char *filename, char *key, char *val, int bLock);
/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 1:found. 0:not found. -nnnn:err code.
*/
int ini_getstr(char *filename, char *key, char *val, int bLock);

//for compliant purpose
//int SafeWritePrivateProfileString(char *sect,char *key,char *value,char *ini);
int Cfg_WriteString(char *sect,char *key,char *value,char *ini);
//int SafeGetPrivateProfileString(char *sec,char *key,char *def,char *str,unsigned int size,char *ini);
int Cfg_ReadString(char *sec,char *key,char *def,char *str,unsigned int size,char *ini);

//int  ConfigGetKey(void *CFG_file, void *section, void *key, void *buf); 
int Cfg_GetKey(void *CFG_file, void *section, void *key, void *buf); 
//int  ConfigSetKey(void *CFG_file, void *section, void *key, void *buf); 
int  Cfg_SetKey(void *CFG_file, void *section, void *key, void *buf); 
//int  ConfigGetSections(void *CFG_file, char *sections[]); 
int  Cfg_GetSections(void *CFG_file, char *sections[]); 
//int  ConfigGetKeys(void *CFG_file, void *section, char *keys[]);
int  Cfg_GetKeys(void *CFG_file, void *section, char *keys[]);
//int  ConfigRemoveKey(void *CFG_file, void *section, void *key);
int  Cfg_RemoveKey(void *CFG_file, void *section, void *key);


#define	CFG_MTD_CACHE
#ifdef CFG_MTD_CACHE
/*
Desc:cache operation, use to read /mnt/mtd/xxx.ini file. cache is /tmp/mtd/xxx.ini
read opersion is from /tm/mtd/xxx.ini, write is using write-through mode, write to /mnt/mtd/xxx.ini and /tmp/mtd/xxx.ini both
NOTE:char *FileOnly		//not full file name, no directory path
example: int ret=Cfg_mtdReadStr("ptzrun.conf","common","curzoom",&zoomrate); 

*/
int Cfg_mtdReadStr(char *FileOnly,char *Section,char *Entry,char *valptr);
//decimal number(not 0xHHHH)
int  Cfg_mtdReadInt(char *FileOnly,char *Section,char *Entry,long *valptr);
//decimal number(not 0xHHHH)
int Cfg_mtdReadShort(char *FileOnly,char *Section,char *Entry,unsigned short *valptr);
//decimal number(not 0xHHHH)
int Cfg_mtdReadbyte(char *FileOnly,char *Section,char *Entry,unsigned char *valptr);
//decimal number(not 0xHHHH), using write-through mode
int Cfg_mtdWriteInt(char *FileOnly,char *Section,char *Entry,long Value);
//decimal number(not 0xHHHH), using write-through mode
int Cfg_mtdWriteStr(char *FileOnly,char *Section,char *Entry,char *valptr);
#endif		//CFG_MTD_CACHE

//#define	CFG_MD5
#ifdef	CFG_MD5
#define MD5_DIGEST_SIZE		16
#define MD5_HMAC_BLOCK_SIZE	64
#define MD5_BLOCK_WORDS		16
#define MD5_HASH_WORDS		4

//typedef unsigned char		u8;
//typedef unsigned short		u16;
//typedef unsigned int		u32;
//typedef unsigned long long 	u64;

typedef	struct
{
	unsigned int hash[MD5_HASH_WORDS];
	unsigned int block[MD5_BLOCK_WORDS];
	unsigned long long byte_count;
}MD5CTX_S;


/* example:
{
	char buf[512];
	MD5CTX_S ctx;
	unsigned char sum[16];
	int sz=0;
	
	Cfg_MD5Init(&ctx);

	while(1)
	{
		sz=read(fd,buf,sizeof(buf));
		if(sz<=0)
			break;

		Cfg_MD5Update(&ctx,buf,sz);

	} //end while

	Cfg_MD5Final(&ctx,sum);
}
*/
void Cfg_MD5Init(void *ctx);
void Cfg_MD5Update(void *ctx, char *data,int len);
void Cfg_MD5Final(void *ctx, unsigned char *out);
#endif	//#ifdef	CFG_MD5

int utl_fgets (char *fileName,char *line, int bLock);
int utl_fputs (char *fileName,char *line, int bLock);

#ifdef __cplusplus
}
#endif 

#endif			//__CFGFILE_H__

