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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <pthread.h>

#include "cfgfile.h"
#define	DBG_MSG  1

#ifdef __cplusplus
       extern "C" {
#endif

#if (CFG_ThreadLock==1)
pthread_mutex_t  gThreadLock;
#endif

static char *sLibVer="libcfgfile_ver 2.2";

void Cfg_GetVer(void)
{
	char pLine[256];

	sprintf(pLine,"%s\n",sLibVer);
	printf("%s",pLine);
}

//static int _SetPosixLock(int fd,  int cmd,  int type,  off_t start,  int whence,  off_t len)
static int _SimpleSetPosixLock(int fd,int type)
{
	struct flock lock;
	
	lock.l_type=type;
	lock.l_pid=getpid();
	lock.l_whence=SEEK_SET;
	lock.l_start=0;
	lock.l_len=0;
	return (fcntl(fd, F_SETLKW, &lock));		//block if busy
}

#if 0
static int SearchLine(FILE *fd, char *pLine, char *sKey, char *sValue)
{
	char CutStr[] = {'=',',','\n',' ','\0'};	//use to cut one string line
	char *pToken[2];
	int Ret=-1;

        pToken[0] = strtok(pLine, CutStr);
        pToken[1] = strtok(NULL, CutStr);

	if  (strcmp(pToken[0],sKey) == 0)		//find the key
	{
		strcpy(sValue,pToken[1]);		//copy it out
		Ret=0;
	}

	return Ret;
}


static int  SearchFile(char *FileName, char *sKey,char *sValue)
{
	char line[256],*p;
	int  i=0,Ret=-1;
	FILE *fp=NULL;

	fp = fopen(FileName, "rb");
	if (fp)
	{
		rewind(fp);

		for(;;) 
		{
	        	if (fgets(line, sizeof(line), fp) == NULL)		//get one line, if reach EOF, return NULL
	            		break;
	        	p = line;
			i++;
	        	while (isspace(*p)) 
	            		p++;
	        	if (*p == '\0' || *p == '#')		//pass blank line and comment line
	            		continue;

			if (!SearchLine(fp,p,sKey,sValue))
			{
				Ret = 0;
				break;
			}
		}

		fclose(fp);
	}
	else
	{
		Ret=-2;
	}

	return Ret;
}

/*
config file is like: sConfigKey=sConfigValue format
This func is used to get sConfigValue according sConfigKey
example:
	
*/
int  GetConfigValue(unsigned char *sCfgFileName,unsigned char *sConfigKey,unsigned char *sConfigValue)
{
	int ret=-1;

	ret = SearchFile(sCfgFileName,sConfigKey,sConfigValue);

	return ret;
}
#endif

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>


static PCFGENTRY _cfg_poolalloc (PCONFIG p, unsigned int count);
static int _cfg_parse (PCONFIG pconfig);

/*** READ MODULE ****/

#ifndef O_BINARY
#define O_BINARY 0
#endif

#if 0
//#ifdef _MAC
static int strcasecmp (const char *s1, const char *s2)
{
  int cmp;

  while (*s1)
    {
      if ((cmp = toupper (*s1) - toupper (*s2)) != 0)
	return cmp;
      s1++;
      s2++;
    }
  return (*s2) ? -1 : 0;
}
#endif

#if (CFG_MyFileLock==1)
static MyFileLock gMyFileLockArray[MyMaxCfgFile];

static int SetMyFileLock(char *filename,int type)
{
	int ret=-1;
	int i=0;
	int Count=0;

	for (i=0;i<MyMaxCfgFile;i++)
	{
		if (gMyFileLockArray[i].FileName==NULL)
		{
			strcpy(gMyFileLockArray[i].FileName,filename);	//do not support too long file name!
			gMyFileLockArray[i].LockFlag=type;
			ret=0;
			break;	//create the file record the exit
		}
		
		if (!strcmp(filename,gMyFileLockArray[i].FileName))	//find the record
		{
			if (gMyFileLockArray[i].LockFlag==My_UNLCK)	//if unlock
			{
				gMyFileLockArray[i].LockFlag=type;			//set the lock flag directly
				ret=0;
			}
			else
			{
				do {
				usleep(100000);	// sleep 100ms to check again
				
				}while ( (Count++<5)&&(gMyFileLockArray[i].LockFlag!=My_UNLCK) );

				if (gMyFileLockArray[i].LockFlag==My_UNLCK)	//if unlock
				{
					gMyFileLockArray[i].LockFlag=type;			//set the lock flag directly
					ret=0;
				}
				else
					ret=-3;
			}
		}
	}

	if (i==MyMaxCfgFile)
		ret=-2;

	return ret;
}

#endif

char * remove_quotes(const char *szString)
{
  char *szWork, *szPtr;

  while (*szString == '\'' || *szString == '\"')
    szString += 1;

  if (!*szString)
    return NULL;
  szWork = strdup (szString);		//string duplicate
  szPtr = strchr (szWork, '\'');		//find the first substring
  if (szPtr)
    *szPtr = 0;
  szPtr = strchr (szWork, '\"');
  if (szPtr)
    *szPtr = 0;

  return szWork;
}

/*
 *  Initialize a configuration, read file into memory
 */
int cfg_init (PCONFIG *ppconf, const char *filename, int doCreate)
{
	PCONFIG pconfig=NULL;
	int ret=0;
	int fd=-1;
	int exist=-1;

	if ( (!filename)||(ppconf==NULL) )		//check input parameters
	{
		return -1;
	}
	
	*ppconf = NULL;	//init output

	ret=access(filename, F_OK);	//check if exist or not
	if (ret!=0)	//no exist
	{
		if (!doCreate)	//not create new file
		{
			return -2;
		}
		else
		{
			exist=2;		//need to create
		}
	}
	else
	{
		exist=1;		//
	}

	if ((pconfig = (PCONFIG) calloc (1, sizeof (TCONFIG))) == NULL)
	{
		ret=-3;
		goto End;
	}	

	pconfig->fileName = strdup (filename);
	pconfig->size=0;		//add by yingcai
	pconfig->fd=-1;

	// If the file does not exist, try to create it 
	if (exist==2)
	{
		fd = creat (filename, 0644);		//create new file
		if (fd>=0){close(fd);}
	}

	//add here

	pconfig->fd=-1;

#if 0
	if ((fd = open (pconfig->fileName, O_RDWR | O_BINARY))<0)
	{
		ret=-4;
		goto End;
	}
	
	pconfig->fd=fd;
	
#if (CFG_FLOCK_BSD==1)			//yingcai add
	ret=flock(fd,LOCK_EX);		//flock(fp,LOCK_EX|LOCK_NB);		//block write
	if (ret!=0)		//can not get R_lock when writting
	{
		ret=-100;
		goto End;
	}
#elif (CFG_FLOCK_POSIX==1)
	ret=_SimpleSetPosixLock(fd,F_WRLCK);
	if (ret<0)		//can not get R_lock when writting
	{
		ret=-100;
		goto End;
	}
#endif		
#endif

	ret=cfg_refresh (pconfig);
	if (ret !=0)		//read in
	{
		ret=-11;
		goto End;
	}

	ret=0;
	*ppconf = pconfig;

End:  
	if (ret<0)
	{
		cfg_done (pconfig);
		*ppconf=NULL;
#ifdef DBG_MSG
		printf("error:%s,ret=%d.filename=%s\n",(char *)__FUNCTION__,ret,filename);
#endif	
	}
	else
	{
	#if (CFG_ThreadLock==1)
		pthread_mutex_init(&gThreadLock, NULL);
	#endif
	}
	
	return ret;
}


/*
 *  Free all data associated with a configuration
 */
int cfg_done (PCONFIG pconfig)
{
	int ret=0;
	
	if (pconfig)
	{
		cfg_freeimage (pconfig);
		if (pconfig->fileName)
		{
			free (pconfig->fileName);
		}
#if 0		
#if (CFG_FLOCK_BSD==1)			//yingcai add
		if (pconfig->fd>=0)
			flock(pconfig->fd,LOCK_UN);
#elif (CFG_FLOCK_POSIX==1)
		if (pconfig->fd>=0)
			ret=_SimpleSetPosixLock(pconfig->fd,F_UNLCK);
#endif
		if (pconfig->fd>=0)
			close (pconfig->fd);
#endif
		free (pconfig);
		pconfig=NULL;	  
		ret=0;
	#if (CFG_ThreadLock==1)		
		pthread_mutex_destroy(&gThreadLock);
	#endif	
	}
	else
		ret=-1;
	
	return ret;
}


/*
 *  Free the content specific data of a configuration
 */
int cfg_freeimage (PCONFIG pconfig)
{
  char *saveName;
  PCFGENTRY e;
  unsigned int i;

  if (pconfig->image)
  {
    	free (pconfig->image);
	pconfig->image=NULL;
  }
  if (pconfig->entries)
  {
      e = pconfig->entries;
      for (i = 0; i < pconfig->numEntries; i++, e++)
	{
	  if (e->flags & CFE_MUST_FREE_SECTION)
	    free (e->section);
	  if (e->flags & CFE_MUST_FREE_ID)
	    free (e->id);
	  if (e->flags & CFE_MUST_FREE_VALUE)
	    free (e->value);
	  if (e->flags & CFE_MUST_FREE_COMMENT)
	    free (e->comment);
	}
      free (pconfig->entries);
  }

  saveName = pconfig->fileName;
  memset (pconfig, 0, sizeof (TCONFIG));
  pconfig->fileName = saveName;

  return 0;
}


/*
 *  This procedure reads an copy of the file into memory
 *  catching the content based on stat
 */
int cfg_refresh (PCONFIG pconfig)
{
	int ret=0;
	struct stat sb;
	char *mem;
	int fd=-1;

	if (pconfig == NULL)
		return -1;
	
	if ((fd = open (pconfig->fileName, O_RDONLY | O_BINARY))<0)
  	{
  		ret=-2;
		goto End;
  	}
#if 1  
#if (CFG_FLOCK_BSD==1)			//yingcai add
	ret=flock(fd,LOCK_SH);
	if (ret!=0)		//can not get R_lock when writting
	{
		ret=-100;
		goto End;
	}
#elif (CFG_FLOCK_POSIX==1)
	ret=_SimpleSetPosixLock(fd,F_RDLCK);
	if (ret<0)		//can not get R_lock when writting
	{
		ret=-100;
		goto End;
	}
#endif
#endif

	//stat (pconfig->fileName, &sb);		//it will open twice, do not use this func.
	ret=fstat(fd,&sb);
	if (ret)
	{
		ret=-3;
		goto End;
	}

	/*
	*  Check to see if our incore image is still valid
	*/
	if (pconfig->image && sb.st_size == pconfig->size
	&& sb.st_mtime == pconfig->mtime)
	{
		ret=0;		//still valid, do NOT need to read in
		goto End;
	}

	/*
	*  If our image is dirty, ignore all local changes
	*  and force a reread of the image, thus ignoring all mods
	*/
	if (pconfig->dirty)
		cfg_freeimage (pconfig);

	/*
	*  Now read the full image
	*/
	mem = (char *) malloc (sb.st_size + 1);
	if (mem == NULL || read (fd, mem, sb.st_size) != sb.st_size)		//read into RAM
	{
		free (mem);mem=NULL;
		ret=-4;
		goto End;
	}
	mem[sb.st_size] = 0;

	/*
	*  Store the new copy
	*/
	cfg_freeimage (pconfig);
	pconfig->image = mem;
	pconfig->size = sb.st_size;
	pconfig->mtime = sb.st_mtime;

	if (_cfg_parse (pconfig)<0)
	{
		cfg_freeimage (pconfig);
		ret=-5;
		goto End;
	}

	ret=0;	//success!

End:
	if (fd>=0)
	{
#if 1	
#if (CFG_FLOCK_BSD==1)
		flock(fd,LOCK_UN);
#elif (CFG_FLOCK_POSIX==1)
		_SimpleSetPosixLock(fd,F_UNLCK);
#endif
#endif
		close (fd);
	}
	if (ret<0)		//failed
	{
		pconfig->image = NULL;
	  	pconfig->size = 0;
#ifdef DBG_MSG
		printf("error:%s,ret=%d\n",(char *)__FUNCTION__,ret);
#endif	
	}
  	return ret;
}

/*
strchr()功能：查找字符串s中首次出现字符c的位置
*/
#define iseolchar(C) (strchr ("\n\r\x1a", C) != NULL)
#define iswhite(C) (strchr ("\f\t ", C) != NULL)

static char *_cfg_skipwhite (char *s)
{
  while (*s && iswhite (*s))
    s++;
  return s;
}


static int _cfg_getline (char **pCp, char **pLinePtr)
{
  char *start;
  char *cp = *pCp;

  while (*cp && iseolchar (*cp))
    cp++;
  start = cp;
  if (pLinePtr)
    *pLinePtr = cp;

  while (*cp && !iseolchar (*cp))
    cp++;
  if (*cp)
    {
      *cp++ = 0;
      *pCp = cp;

      while (--cp >= start && iswhite (*cp));
      cp[1] = 0;
    }
  else
    *pCp = cp;

  return *start ? 1 : 0;
}


static char *rtrim (char *str)
{
  char *endPtr;

  if (str == NULL || *str == '\0')
    return NULL;

  for (endPtr = &str[strlen (str) - 1]; endPtr >= str && isspace (*endPtr);endPtr--)
  	;
  endPtr[1] = 0;		//should be endPrt[1]='\0' ??

  return endPtr >= str ? endPtr : NULL;
}


/*
 *  Parse the in-memory copy of the configuration data
 */
static int _cfg_parse (PCONFIG pconfig)
{
  int ret=0;
  int isContinue, inString;
  char *imgPtr;
  char *endPtr;
  char *lp;
  char *section;
  char *id;
  char *value;
  char *comment;

  if (cfg_valid (pconfig))
    return 0;

  endPtr = pconfig->image + pconfig->size;
  for (imgPtr = pconfig->image; imgPtr < endPtr;)
    {
      if (!_cfg_getline (&imgPtr, &lp))
	continue;

      section = id = value = comment = NULL;

      /*
         *  Skip leading spaces
       */
      if (iswhite (*lp))
	{
	  lp = _cfg_skipwhite (lp);
	  isContinue = 1;
	}
      else
	isContinue = 0;

      /*
       *  Parse Section
       */
      if (*lp == '[')
	{
	  section = _cfg_skipwhite (lp + 1);
	  if ((lp = strchr (section, ']')) == NULL)
	    continue;
	  *lp++ = 0;
	  if (rtrim (section) == NULL)
	    {
	      section = NULL;
	      continue;
	    }
	  lp = _cfg_skipwhite (lp);
	}
      else if (*lp != ';')
	{
	  /* Try to parse
	   *   1. Key = Value
	   *   2. Value (iff isContinue)
	   */
	  if (!isContinue)
	    {
	      /* Parse `<Key> = ..' */
	      id = lp;
	      if ((lp = strchr (id, '=')) == NULL)
		continue;
	      *lp++ = 0;
	      rtrim (id);
	      lp = _cfg_skipwhite (lp);
	    }

	  /* Parse value */
	  inString = 0;
	  value = lp;
	  while (*lp)
	    {
	      if (inString)
		{
		  if (*lp == inString)
		    inString = 0;
		}
	      else if (*lp == '"' || *lp == '\'')
		inString = *lp;
	      else if (*lp == ';' && iswhite (lp[-1]))
		{
		  *lp = 0;
		  comment = lp + 1;
		  rtrim (value);
		  break;
		}
	      lp++;
	    }
	}

      /*
       *  Parse Comment
       */
      if (*lp == ';')
	comment = lp + 1;

      if (cfg_storeentry (pconfig, section, id, value, comment,0) <0)
	{
	  pconfig->dirty = 1;
	  return -1;
	}
    }

  pconfig->flags |= CFG_VALID;

  return 0;
}


int cfg_storeentry (PCONFIG pconfig,char *section,char *id,char *value,char *comment, int dynamic)
{
	PCFGENTRY data;

	if ((data = _cfg_poolalloc (pconfig, 1)) == NULL)
		return -1;

	data->flags = 0;
	if (dynamic)
	{
		if (section)
			section = strdup (section);
		if (id)
			id = strdup (id);
		if (value)
			value = strdup (value);
		if (comment)
			comment = strdup (value);

		if (section)
			data->flags |= CFE_MUST_FREE_SECTION;
		if (id)
			data->flags |= CFE_MUST_FREE_ID;
		if (value)
			data->flags |= CFE_MUST_FREE_VALUE;
		if (comment)
			data->flags |= CFE_MUST_FREE_COMMENT;
	}

	data->section = section;
	data->id = id;
	data->value = value;
	data->comment = comment;

	return 0;
}


static PCFGENTRY _cfg_poolalloc (PCONFIG p, unsigned int count)
{
  PCFGENTRY newBase;
  unsigned int newMax;

  if (p->numEntries + count > p->maxEntries)
    {
      newMax =
	  p->maxEntries ? count + p->maxEntries + p->maxEntries / 2 : count +
	  4096 / sizeof (TCFGENTRY);
      newBase = (PCFGENTRY) malloc (newMax * sizeof (TCFGENTRY));
      if (newBase == NULL)
	return NULL;
      if (p->entries)
	{
	  memcpy (newBase, p->entries, p->numEntries * sizeof (TCFGENTRY));
	  free (p->entries);
	}
      p->entries = newBase;
      p->maxEntries = newMax;
    }

  newBase = &p->entries[p->numEntries];
  p->numEntries += count;

  return newBase;
}


/*** COMPATIBILITY LAYER ***/


int
cfg_rewind (PCONFIG pconfig)
{
  if (!cfg_valid (pconfig))
    return -1;

  pconfig->flags = CFG_VALID;
  pconfig->cursor = 0;

  return 0;
}


/*
 *  returns:
 *	 0 success
 *	-1 no next entry
 *
 *	section	id	value	flags		meaning
 *	!0	0	!0	SECTION		[value]
 *	!0	!0	!0	DEFINE		id = value|id="value"|id='value'
 *	!0	0	!0	0		value
 *	0	0	0	EOF		end of file encountered
 */
int cfg_nextentry (PCONFIG pconfig)
{
  PCFGENTRY e;

  if (!cfg_valid (pconfig) || cfg_eof (pconfig))
    return -1;

  pconfig->flags &= ~(CFG_TYPEMASK);
  pconfig->id = pconfig->value = NULL;

  while (1)
  {
     if (pconfig->cursor >= pconfig->numEntries)
	 {
	   pconfig->flags |= CFG_EOF;
	   return -1;
	 }
     e = &pconfig->entries[pconfig->cursor++];

     if (e->section)
	 {
	   pconfig->section = e->section;
	   pconfig->flags |= CFG_SECTION;
	   return 0;
	 }
     if (e->value)
	 {
	  pconfig->value = e->value;
	  if (e->id)
	  {
	      pconfig->id = e->id;
	      pconfig->flags |= CFG_DEFINE;
	  }
	  else
	    pconfig->flags |= CFG_CONTINUE;
	  return 0;
	 }
   }
}

int cfg_find (PCONFIG pconfig, char *section, char *id)
{
  int atsection=0;

  if (!cfg_valid (pconfig) || cfg_rewind (pconfig))
    return -1;

  while (cfg_nextentry (pconfig) == 0)
  {
     if (atsection)
	 {
	   if (cfg_section (pconfig))
	     return -1;
	   else if (cfg_define (pconfig))
	   {
	      char *szId = remove_quotes (pconfig->id);
	      int bSame;
	      if (szId)
		  {
		    //bSame = !strcasecmp (szId, id);
		    bSame = !strcmp (szId, id);
		    free (szId);
		    if (bSame)
		     return 0;
		  }
	   }
	  }

	 else if (cfg_section (pconfig)
	  //&& !strcasecmp (pconfig->section, section))
	  && !strcmp (pconfig->section, section))
	 {
	   if (id == NULL)
	     return 0;
	   atsection = 1;
	 }

	 if (!section)
	 	atsection=1;
   }
   return -1;
}


/*** WRITE MODULE ****/


/*
 *  Change the configuration
 *
 *  section id    value		action
 *  --------------------------------------------------------------------------
 *   value  value value		update '<entry>=<string>' in section <section>
 *   value  value NULL		delete '<entry>' from section <section>
 *   value  NULL  NULL		delete section <section>
 *   NULL   value value     no [section] item added
 */
int cfg_write ( PCONFIG pconfig,char *section,char *id,char *value)
{
  int ret=0;
  PCFGENTRY e, e2, eSect=0;
  int idx;
  int i;
  char sValue[CFG_MAX_LINE_LENGTH]={0};

	if (!cfg_valid (pconfig))
    	return -1;

	// search the section 
	e = pconfig->entries;
	i = pconfig->numEntries;
	eSect = 0;

	if (!section){
		while (i--){
			if (e->id && !strcmp (e->id, id)){
				/* found key - do update */
				if (e->value && (e->flags & CFE_MUST_FREE_VALUE))
				{
					e->flags &= ~CFE_MUST_FREE_VALUE;
					free (e->value);
				}
				pconfig->dirty = 1;
				if ((e->value = strdup (value)) == NULL)
					return -1;
				e->flags |= CFE_MUST_FREE_VALUE;
				return 0;
			}
			e++;
		}
	}

//  ret=cfg_getstring(pconfig,section,id,sValue);		//we allow value=NULL to delete key
//  if ((ret==0)&&(!strcmp(value,sValue)))	//if equal, do not need to write, add on 2013-03-27
//  	return 0;

  while (i--)
  {
      //if (e->section && !strcasecmp (e->section, section))
      if (e->section && !strcmp (e->section, section))
	{
	  eSect = e;
	  break;
	}
      e++;
  }

  // did we find the section? no
  if (!eSect)
  {
      // check for delete operation on a nonexisting section 
      if (!id || !value)
	    return 0;

      // add section first 
      if (cfg_storeentry (pconfig, section, NULL, NULL, NULL,1) == -1  
	  	|| cfg_storeentry (pconfig, NULL, id, value, NULL,1) == -1)
	     return -1;

      pconfig->dirty = 1;
      return 0;
  }

  // ok - we have found the section - let's see what we need to do 
Found:
  if (id)
  {
     if (value)
	 {
	  /* add / update a key */
	  while (i--)
	  {
	      e++;
	      /* break on next section */
	     if (e->section)
		{
		  /* insert new entry before e */
		  idx = e - pconfig->entries;
		  if (_cfg_poolalloc (pconfig, 1) == NULL)
		    return -1;
		  memmove (e + 1, e,
		      (pconfig->numEntries - idx) * sizeof (TCFGENTRY));
		  e->section = NULL;
		  e->id = strdup (id);
		  e->value = strdup (value);
		  e->comment = NULL;
		  if (e->id == NULL || e->value == NULL)
		    return -1;
		  e->flags |= CFE_MUST_FREE_ID | CFE_MUST_FREE_VALUE;
		  pconfig->dirty = 1;
		  return 0;
		}

	      //if (e->id && !strcasecmp (e->id, id))
	    if (e->id && !strcmp (e->id, id))
		{
		  /* found key - do update */
		  if (e->value && (e->flags & CFE_MUST_FREE_VALUE))
		    {
		      e->flags &= ~CFE_MUST_FREE_VALUE;
		      free (e->value);
		    }
		  pconfig->dirty = 1;
		  if ((e->value = strdup (value)) == NULL)
		    return -1;
		  e->flags |= CFE_MUST_FREE_VALUE;
		  return 0;
		}
	    }

	  /* last section in file - add new entry */
	  if (cfg_storeentry (pconfig, NULL, id, value, NULL,1) == -1)
	    return -1;
	  pconfig->dirty = 1;
	  return 0;
	}
      else
	{
	  /* delete a key */
	  while (i--)
	    {
	      e++;
	      /* break on next section */
	      if (e->section)
		return 0;	/* not found */

	      //if (e->id && !strcasecmp (e->id, id))
	      if (e->id && !strcmp (e->id, id))
		{
		  /* found key - do delete */
		  eSect = e;
		  e++;
		  goto doDelete;
		}
	    }
	  /* key not found - that' ok */
	  return 0;
	}
    }
  else
    {
      /* delete entire section */

      /* find e : next section */
      while (i--)
	{
	  e++;
	  /* break on next section */
	  if (e->section)
	    break;
	}
      if (i < 0)
	e++;

      /* move up e while comment */
      e2 = e - 1;
      while (e2->comment && !e2->section && !e2->id && !e2->value
	  && (iswhite (e2->comment[0]) || e2->comment[0] == ';'))
	  e2--;
      e = e2 + 1;

 doDelete:
      /* move up eSect while comment */
      e2 = eSect - 1;
      while (e2->comment && !e2->section && !e2->id && !e2->value
	  && (iswhite (e2->comment[0]) || e2->comment[0] == ';'))
	  e2--;
      eSect = e2 + 1;

      /* delete everything between eSect .. e */
      for (e2 = eSect; e2 < e; e2++)
	{
	  if (e2->flags & CFE_MUST_FREE_SECTION)
	    free (e2->section);
	  if (e2->flags & CFE_MUST_FREE_ID)
	    free (e2->id);
	  if (e2->flags & CFE_MUST_FREE_VALUE)
	    free (e2->value);
	  if (e2->flags & CFE_MUST_FREE_COMMENT)
	    free (e2->comment);
	}
      idx = e - pconfig->entries;
      memmove (eSect, e, (pconfig->numEntries - idx) * sizeof (TCFGENTRY));
      pconfig->numEntries -= e - eSect;
      pconfig->dirty = 1;
    }

    return 0;
}

int cfg_writelong ( PCONFIG pconfig,char *section,char *id,long value)
{
	int ret=0;
	char sTemp[16]={0};

	sprintf(sTemp,"%ld",value);
	ret=cfg_write(pconfig,section,id,sTemp);

	return ret;
}
int cfg_writeint( PCONFIG pconfig,char *section,char *id,int value)
{
	int ret=0;
	char sTemp[16]={0};

	sprintf(sTemp,"%d",value);
	ret=cfg_write(pconfig,section,id,sTemp);

	return ret;
}
int cfg_writeshort ( PCONFIG pconfig,char *section,char *id,unsigned short value)
{
	int ret=0;
	char sTemp[16]={0};

	sprintf(sTemp,"%d",value);
	ret=cfg_write(pconfig,section,id,sTemp);

	return ret;
}

int cfg_writebyte ( PCONFIG pconfig,char *section,char *id,unsigned char value)
{
	int ret=0;
	char sTemp[16]={0};

	sprintf(sTemp,"%d",value);
	ret=cfg_write(pconfig,section,id,sTemp);

	return ret;
}

/*
key=value	//no space before/after = to avoid some comptible problem.
*/
static void
_cfg_output (PCONFIG pconfig, FILE *fd)
{
  PCFGENTRY e = pconfig->entries;
  int i = pconfig->numEntries;
  int m = 0;
  int j, l;
  int skip = 0;

  while (i--)
    {
      if (e->section)
	{
	  /* Add extra line before section, unless comment block found */
	  if (skip)
	    fprintf (fd, "\n");
	  fprintf (fd, "[%s]", e->section);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);

	  /* Calculate m, which is the length of the longest key */
	  m = 0;
	  for (j = 1; j <= i; j++)
	    {
	      if (e[j].section)
		break;
	      if (e[j].id && (l = strlen (e[j].id)) > m)
		m = l;
	    }

	  /* Add an extra lf next time around */
	  skip = 1;
	}
      /*
       *  Key = value
       */
      else if (e->id && e->value)
	{
	  if (m)
	    fprintf (fd, "%s=%s",e->id, e->value);
	  else
	    fprintf (fd, "%s=%s", e->id, e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Value only (continuation)
       */
      else if (e->value)
	{
	  fprintf (fd, "%s", e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Comment only - check if we need an extra lf
       *
       *  1. Comment before section gets an extra blank line before
       *     the comment starts.
       *
       *          previousEntry = value
       *          <<< INSERT BLANK LINE HERE >>>
       *          ; Comment Block
       *          ; Sticks to section below
       *          [new section]
       *
       *  2. Exception on 1. for commented out definitions:
       *     (Immediate nonwhitespace after ;)
       *          [some section]
       *          v1 = 1
       *          ;v2 = 2   << NO EXTRA LINE >>
       *          v3 = 3
       *
       *  3. Exception on 2. for ;; which certainly is a section comment
       *          [some section]
       *          definitions
       *          <<< INSERT BLANK LINE HERE >>>
       *          ;; block comment
       *          [new section]
       */
      else if (e->comment)
	{
	  if (skip && (iswhite (e->comment[0]) || e->comment[0] == ';'))
	    {
	      for (j = 1; j <= i; j++)
		{
		  if (e[j].section)
		    {
		      fprintf (fd, "\n");
		      skip = 0;
		      break;
		    }
		  if (e[j].id || e[j].value)
		    break;
		}
	    }
	  fprintf (fd, ";%s", e->comment);
	}
      fprintf (fd, "\n");
      e++;
    }
}



/*
 *  Write a formatted copy of the configuration to a file
 *
 *  This assumes that the inifile has already been parsed
 */
static void
_cfg_outputformatted (PCONFIG pconfig, FILE *fd)
{
  PCFGENTRY e = pconfig->entries;
  int i = pconfig->numEntries;
  int m = 0;
  int j, l;
  int skip = 0;

  while (i--)
    {
      if (e->section)
	{
	  /* Add extra line before section, unless comment block found */
	  if (skip)
	    fprintf (fd, "\n");
	  fprintf (fd, "[%s]", e->section);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);

	  /* Calculate m, which is the length of the longest key */
	  m = 0;
	  for (j = 1; j <= i; j++)
	    {
	      if (e[j].section)
		break;
	      if (e[j].id && (l = strlen (e[j].id)) > m)
		m = l;
	    }

	  /* Add an extra lf next time around */
	  skip = 1;
	}
      /*
       *  Key = value
       */
      else if (e->id && e->value)
	{
	  if (m)
	    fprintf (fd, "%-*.*s = %s", m, m, e->id, e->value);
	  else
	    fprintf (fd, "%s = %s", e->id, e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Value only (continuation)
       */
      else if (e->value)
	{
	  fprintf (fd, "  %s", e->value);
	  if (e->comment)
	    fprintf (fd, "\t;%s", e->comment);
	}
      /*
       *  Comment only - check if we need an extra lf
       *
       *  1. Comment before section gets an extra blank line before
       *     the comment starts.
       *
       *          previousEntry = value
       *          <<< INSERT BLANK LINE HERE >>>
       *          ; Comment Block
       *          ; Sticks to section below
       *          [new section]
       *
       *  2. Exception on 1. for commented out definitions:
       *     (Immediate nonwhitespace after ;)
       *          [some section]
       *          v1 = 1
       *          ;v2 = 2   << NO EXTRA LINE >>
       *          v3 = 3
       *
       *  3. Exception on 2. for ;; which certainly is a section comment
       *          [some section]
       *          definitions
       *          <<< INSERT BLANK LINE HERE >>>
       *          ;; block comment
       *          [new section]
       */
      else if (e->comment)
	{
	  if (skip && (iswhite (e->comment[0]) || e->comment[0] == ';'))
	    {
	      for (j = 1; j <= i; j++)
		{
		  if (e[j].section)
		    {
		      fprintf (fd, "\n");
		      skip = 0;
		      break;
		    }
		  if (e[j].id || e[j].value)
		    break;
		}
	    }
	  fprintf (fd, ";%s", e->comment);
	}
      fprintf (fd, "\n");
      e++;
    }
}

#if 0
/*
 *  Write the changed file back
 */
int
cfg_commit (PCONFIG pconfig)
{
  int ret=0;
  FILE *fp=NULL;

  if (!cfg_valid (pconfig))
  {
	ret=-11;
	goto Failed;
  }

  if (pconfig->dirty)
  {
       if ((fp = fopen (pconfig->fileName, "w")) == NULL)
       {
	   ret=-2;
	   goto Failed;
       }

#if (CFG_FLOCK_BSD==1)			//yingcai add
	ret=flock(fp,LOCK_EX);		//flock(fp,LOCK_EX|LOCK_NB);		//block write
	if (ret!=0)		//can not get R_lock when writting
	{
		ret=-100;
		goto Failed;
	}
 #elif (CFG_FLOCK_POSIX==1)
   ret=_SimpleSetPosixLock((int)fp,F_WRLCK);
   if (ret<0)		//can not get R_lock when writting
   {
   	ret=-100;
	goto Failed;
   }
 #endif
   
      _cfg_outputformatted (pconfig, fp);

#if (CFG_FLOCK_BSD==1)			//yingcai add
	flock(fp,LOCK_UN);
#elif (CFG_FLOCK_POSIX==1)
   	ret=_SimpleSetPosixLock((int)fp,F_UNLCK);
   	if (ret<0)		//can not get R_lock when writting
   	{
   		ret=-101;
		goto Failed;
   	}
#endif

      pconfig->dirty = 0;
      ret=0;	  
    }
  

Failed:
	if (fp)
		fclose (fp);
#ifdef DBG_MSG
	printf("%s,ret=%d\n",(char *)__FUNCTION__,ret);
#endif	

	return ret;
}

#else
/*
 *  Write the changed file back
 */
int cfg_commit (PCONFIG pconfig)
{
	int ret=0;
	
	int fd=-1;
	FILE *fp=NULL;

#if (CFG_ThreadLock==1)		
//	ret=pthread_mutex_trylock(&gThreadLock);
//	if (ret==EBUSY)
//		return -11;
	pthread_mutex_lock(&gThreadLock);
#endif	
	
	if (!cfg_valid (pconfig))
	{
		ret=-1;
		goto End;
	}

	if (pconfig->dirty)
	{

#if 1
		if ((fd = open (pconfig->fileName, O_RDWR | O_BINARY)) <0)
		{
			ret=-4;
			goto End;
		}
		
#if (CFG_FLOCK_BSD==1)			//yingcai add
		ret=flock(fd,LOCK_EX);		//flock(fp,LOCK_EX|LOCK_NB);		//block write
		if (ret!=0)		//can not get R_lock when writting
		{
			ret=-100;
			goto End;
		}
#elif (CFG_FLOCK_POSIX==1)
		ret=_SimpleSetPosixLock(fd,F_WRLCK);
		if (ret<0)		//can not get R_lock when writting
		{
			ret=-100;
			goto End;
		}
#endif		
#endif
		if ((fp = fopen (pconfig->fileName, "w")) == NULL)
		{
			ret=-2;
			goto End;
		}
   
      		//_cfg_outputformatted (pconfig,fp);
      		_cfg_output(pconfig,fp);
		if (fp){
			fflush(fp);
			fclose(fp);
		}

		pconfig->dirty = 0;
      		ret=0;	  
    	}
  
End:

#if 1		
#if (CFG_FLOCK_BSD==1)
	if (fd>=0)
		flock(fd,LOCK_UN);
#elif (CFG_FLOCK_POSIX==1)
	if (fd>=0)
		_SimpleSetPosixLock(fd,F_UNLCK);
#endif
	close (fd);
#endif	
	if (ret<0)
	{
#ifdef DBG_MSG
		printf("error:%s,ret=%d\n",(char *)__FUNCTION__,ret);
#endif	
	}
	
#if (CFG_ThreadLock==1)
	pthread_mutex_unlock(&gThreadLock);
#endif
	
	return ret;
}
#endif

int cfg_copy(PCONFIG pconfig,char *newfilename)
{
	int ret=0;

	int fd=-1;
	FILE *fp=NULL;

#if (CFG_ThreadLock==1)		
//	ret=pthread_mutex_trylock(&gThreadLock);
//	if (ret==EBUSY)
//		return -11;
	pthread_mutex_lock(&gThreadLock);
#endif	
	
	if (!cfg_valid (pconfig))
	{
		ret=-1;
		goto End;
	}

//	if (pconfig->dirty)
	{

#if 1
		if ((fd = open (newfilename, O_RDWR | O_BINARY)) <0)
		{
			ret=-4;
			goto End;
		}
		
#if (CFG_FLOCK_BSD==1)			//yingcai add
		ret=flock(fd,LOCK_EX);		//flock(fp,LOCK_EX|LOCK_NB);		//block write
		if (ret!=0)		//can not get R_lock when writting
		{
			ret=-100;
			goto End;
		}
#elif (CFG_FLOCK_POSIX==1)
		ret=_SimpleSetPosixLock(fd,F_WRLCK);
		if (ret<0)		//can not get R_lock when writting
		{
			ret=-100;
			goto End;
		}
#endif		
#endif
		if ((fp = fopen (newfilename, "w")) == NULL)
		{
			ret=-2;
			goto End;
		}
   
      		_cfg_outputformatted (pconfig,fp);
		if (fp){
			fflush(fp);
			fclose(fp);
		}

//		pconfig->dirty = 0;
      		ret=0;	  
    	}
  
End:

#if 1		
#if (CFG_FLOCK_BSD==1)
	if (fd>=0)
		flock(fd,LOCK_UN);
#elif (CFG_FLOCK_POSIX==1)
	if (fd>=0)
		_SimpleSetPosixLock(fd,F_UNLCK);
#endif
	close (fd);
#endif	
	if (ret<0)
	{
#ifdef DBG_MSG
		printf("error:%s,ret=%d\n",(char *)__FUNCTION__,ret);
#endif	
	}
	
#if (CFG_ThreadLock==1)
	pthread_mutex_unlock(&gThreadLock);
#endif

	return ret;
}


int
cfg_next_section(PCONFIG pconfig)
{
  PCFGENTRY e;

  do
    if (0 != cfg_nextentry (pconfig))
      return -1;
  while (!cfg_section (pconfig));

  return 0;
}


int
list_sections (PCONFIG pCfg, char * lpszRetBuffer, int cbRetBuffer)
{
  int curr = 0, sect_len = 0;
  lpszRetBuffer[0] = 0;

  if (0 == cfg_rewind (pCfg))
    {
      while (curr < cbRetBuffer && 0 == cfg_next_section (pCfg)
	  && pCfg->section)
	{
	  sect_len = strlen (pCfg->section) + 1;
	  sect_len =
	      sect_len > cbRetBuffer - curr ? cbRetBuffer - curr : sect_len;

	  memmove (lpszRetBuffer + curr, pCfg->section, sect_len);

	  curr += sect_len;
	}
      if (curr < cbRetBuffer)
	lpszRetBuffer[curr] = 0;
      return curr;
    }
  return 0;
}


int
list_entries (PCONFIG pCfg, const char * lpszSection, char * lpszRetBuffer, int cbRetBuffer)
{
  int curr = 0, sect_len = 0;
  lpszRetBuffer[0] = 0;

  if (0 == cfg_rewind (pCfg))
    {
      while (curr < cbRetBuffer && 0 == cfg_nextentry (pCfg))
	{
	  if (cfg_define (pCfg)
	      && !strcmp (pCfg->section, lpszSection) && pCfg->id)
	    {
	      sect_len = strlen (pCfg->id) + 1;
	      sect_len =
		  sect_len >
		  cbRetBuffer - curr ? cbRetBuffer - curr : sect_len;

	      memmove (lpszRetBuffer + curr, pCfg->id, sect_len);

	      curr += sect_len;
	    }
	}
      if (curr < cbRetBuffer)
	lpszRetBuffer[curr] = 0;
      return curr;
    }
  return 0;
}

int cfg_getstring (PCONFIG pconfig, char *section, char *id, char *valptr)
{
//	if(!pconfig || !section || !id || !valptr) return -1;
	if(cfg_find(pconfig,section,id) <0)
	{ 
		*valptr=NULL_STR;
		return -1;
	}
	strcpy(valptr,pconfig->value);
	return 0;
}

int cfg_getlong (PCONFIG pconfig, char *section, char *id, long *valptr)
{
//	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) <0)
	{
//		*valptr =NULL_VAL;
		return -2;
	}
	*valptr = atoi(pconfig->value);
	return 0;
}

int cfg_getint (PCONFIG pconfig, char *section, char *id, int *valptr)
{
	return cfg_getlong(pconfig,section,id,(long *)valptr);
}

int cfg_getshort (PCONFIG pconfig, char *section, char *id, unsigned short *valptr)
{
	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) <0)
	{
	//	*valptr =NULL_VAL;
		return -2;
	}		
	*valptr = atoi(pconfig->value);
	return 0;
}
	
int cfg_getbyte(PCONFIG pconfig, char *section, char *id, unsigned char *valptr)
{
	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) <0)
	{
		*valptr =0xFF;
		return -2;
	}		
	*valptr = atoi(pconfig->value);
	return 0;
}

int GetPrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,
    char * lpszFilename)
{
  char *defval = (char *) lpszDefault, *value = NULL;
  int len = 0;
  PCONFIG pCfg;

  /*
   *  Sorry for this one -- Windows cannot handle a default value of
   *  "" in GetPrivateProfileString, so it is passed as " " instead.
   */
  if (!lpszDefault || (lpszDefault[0] == '\0') || (lpszDefault[0] == ' ' && lpszDefault[1] == '\0')) value = "";
  else value = lpszDefault;
  
  strncpy (lpszRetBuffer, value, cbRetBuffer - 1);

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 0))
    {
      goto fail;
    }

  /*
   *  Check whether someone else has modified the .ini file
   */
  cfg_refresh (pCfg);

  if (!cfg_find (pCfg, (char *) lpszSection, (char *) lpszEntry)){
    value = pCfg->value;
    strncpy (lpszRetBuffer, value, cbRetBuffer - 1);
  }

  cfg_done (pCfg);

fail:
  len = strlen (lpszRetBuffer);
  return len;
}


int SafeGetPrivateProfileString(char * lpszSection, char * lpszEntry,
    char * lpszDefault, char * lpszRetBuffer, int cbRetBuffer,char * lpszFilename)
{
	return GetPrivateProfileString(lpszSection, lpszEntry,lpszDefault,lpszRetBuffer,cbRetBuffer,lpszFilename);
}	

int GetPrivateProfileInt (char * lpszSection, char * lpszEntry,
    int iDefault, char * lpszFilename)
{
  int ret=iDefault;
  PCONFIG pCfg;

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 0))
    {
      return iDefault;
    }

  if (!cfg_find (pCfg, (char *) lpszSection, (char *) lpszEntry)){
    ret = atoi(pCfg->value);
  }

  cfg_done (pCfg);

  return ret;
}

int SafeGetPrivateProfileInt (char * lpszSection, char * lpszEntry,
    int iDefault, char * lpszFilename)
{
	return GetPrivateProfileInt(lpszSection,lpszEntry,iDefault,lpszFilename);
}

int WritePrivateProfileString (char * lpszSection, char * lpszEntry,
    char * lpszString, char * lpszFilename)
{
  int ret;
  PCONFIG pCfg;

  /* If error during reading the file */
  if (cfg_init (&pCfg, lpszFilename, 1))
    {
      return -1;
    }
  ret = cfg_write (pCfg, lpszSection, lpszEntry, lpszString);
  cfg_commit (pCfg);
  cfg_done (pCfg);
  return ret;
}

int cfg_get_item (PCONFIG pconfig, char *section, char *id, char * fmt, ...)
{
	int ret;
	va_list ap;
	
	if(!pconfig || !section || !id) return -1;
	if(cfg_find(pconfig,section,id) <0) return -1;
	va_start(ap, fmt);
	ret = vsscanf(pconfig->value, fmt, ap );
	va_end(ap);
	if(ret > 0) return 0;
	else return -1;
}

int cfg_write_item(PCONFIG pconfig, char *section, char *id, char * fmt, ...)
{
	int ret;
	char buf[CFG_MAX_LINE_LENGTH];
	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(buf, CFG_MAX_LINE_LENGTH, fmt, ap);
	va_end(ap);
	if(ret < 0) return -1;
	return cfg_write(pconfig,section,id,buf);
}

//===================API==================================

int Cfg_ReadStr(char *File,char *Section,char *Entry,char *valptr)
{ 
	PCONFIG pCfg;
	int ret=-1;

	ret=cfg_init (&pCfg, File, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,valptr);
		if (ret)	//fail
			*valptr=NULL_STR;
		cfg_done(pCfg);
	}

	return ret;
}

//decimal number(not 0xHHHH)
int  Cfg_ReadInt(char *File,char *Section,char *Entry,long *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;

	ret=cfg_init (&pCfg, File, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (ret==0)	//success
			*valptr = atoi(sValue);		//decimal number 
//		else
//			*valptr = NULL_VAL;
		cfg_done(pCfg);
	}
	return ret;
}

//decimal number(not 0xHHHH)
int Cfg_ReadShort(char *File,char *Section,char *Entry,unsigned short *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;
	char *p=sValue;

	ret=cfg_init (&pCfg, File, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (!ret)
		{
			*valptr = atoi(p);		//decimal number 
			//*valptr = strtol(sValue,(char**)NULL,10);
			//*valptr = strtol(sValue,(char**)NULL,16);
		}
	//	else
	//		*valptr=NULL_VAL;

		cfg_done(pCfg);
	}

	return ret;
}

//decimal number(not 0xHHHH)
int Cfg_Readbyte(char *File,char *Section,char *Entry,unsigned char *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;

	ret=cfg_init (&pCfg, File, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (!ret)
		{
			//*valptr = atoi(pCfg->value);		//decimal number 
			*valptr = atoi(sValue);		//decimal number 
		}
		else
			*valptr=0xFF;
		
		cfg_done(pCfg);
	}
	return ret;
}


//decimal number(not 0xHHHH)
int Cfg_WriteInt(char *File,char *Section,char *Entry,long Value)
{ 
	PCONFIG pCfg;
	int ret=-1;
	
	char sValue[64];
	sprintf(sValue,"%ld",Value);		//convert to string, decimal number

	ret=cfg_init (&pCfg, File, 1);		//create it if not exist
	if (!ret)
	{
		cfg_write (pCfg, Section, Entry, sValue);
		ret = cfg_commit(pCfg);	//save
		cfg_done(pCfg);	//close handle 
	}
	return ret;
}

int Cfg_WriteHexInt(char *File,char *Section,char *Entry,int Value)
{ 
	PCONFIG pCfg;
	int ret=-1;
	
	char sValue[64];
	sprintf(sValue,"0x%0x",Value);

	ret=cfg_init (&pCfg, File, 1);		//create it if not exist
	if (!ret)
	{
		cfg_write (pCfg, Section, Entry, sValue);
		ret = cfg_commit(pCfg);	//save
		cfg_done(pCfg);	//close handle 
	}
	return ret;
}

//decimal number(not 0xHHHH)
int Cfg_WriteStr(char *File,char *Section,char *Entry,char *valptr)
{ 
	PCONFIG pCfg;
	int ret=-1;

	ret=cfg_init (&pCfg, File, 1);		//create it if not exist
	if (!ret)
	{
		cfg_write (pCfg, Section, Entry, valptr);
		ret = cfg_commit(pCfg);	//save
		cfg_done(pCfg);	//close handle 
	}
	return ret;
}

/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 0:success, -nnnn:err code.
*/
int ini_setstr(char *filename, char *key, char *val, int bLock)
{
	PCONFIG pCfg=NULL;
	int ret=-1;

	ret=cfg_init (&pCfg, filename, 1);
	if (!ret)
	{
		cfg_write (pCfg, NULL, key, val);
		ret = cfg_commit(pCfg);
		cfg_done(pCfg);
	}

	return ret;
}

/*
	input: int bLock	// if bLock=1 will lock this file.
	return: 1:found. 0:not found. -nnnn:err code.
*/
int ini_getstr(char *filename, char *key, char *val, int bLock)
{
	int ret=0;
#if 0
	char line[CFG_MAX_LINE_LENGTH];
	char *p=NULL,*p1=NULL,name[64]={0};
	FILE *fp=NULL;
	int fd=-1;

	if (bLock==1){
		if ((fd = open (filename, O_RDONLY | O_BINARY)) <0)
		{
			ret=-2;
			goto End;
		}
		
		ret=_SimpleSetPosixLock(fd,F_RDLCK);
		if (ret<0)
		{
			ret=-100;
			goto End;
		}
	}
	
	fp = fopen(filename, "rb");
	if (fp)
	{
		rewind(fp);

		for(;;) 
		{
	        if (fgets(line, sizeof(line), fp) == NULL)		//get one line, if reach EOF, return NULL
	        	break;
	        p = line;
			
			while (isspace(*p)) 
	        	p++;
	        if (*p == '\0' || *p == '#' ||*p == ';')		//pass blank line and comment line
	        	continue;

			p1=strstr(p,"=");		// name=value
			if (p1){
				strncpy(name,p,p1-p);
				if (!strcmp(name,key)){
					strcpy(val,p1+1);
					ret=1;break;
				}
			}
		}

		fclose(fp);
	}

End:
	if (fd>=0){
		_SimpleSetPosixLock(fd,F_UNLCK);
		close (fd);
	}
#else
	PCONFIG pCfg=NULL;

	ret=cfg_init (&pCfg, filename, 0);
	if (!ret)
	{
		ret=cfg_getstring(pCfg, NULL, key,val);
		cfg_done(pCfg);
	}
#endif
	return ret;
}


int Cfg_WriteString(char *sect,char *key,char *value,char *ini)
{
	int ret=0;

	ret=Cfg_WriteStr(ini,sect,key,value);

	return ret;
}

int Cfg_ReadString(char *sec,char *key,char *def,char *str,unsigned int size,char *ini)
{
	int ret=0;
	
	if (!sec || !key ||!def|| !str|| !ini) 
		return -1;

	if (strlen(def)>=size)
		return -2;

	strcpy(str,def);		//set default to return
#if 0
//	ret=Cfg_ReadStr(ini,sec,key,str);
#else
	PCONFIG pCfg=NULL;
	ret=cfg_init (&pCfg, ini, 0);		//do not create new file
	if (!ret)
	{
		if (cfg_find(pCfg,sec,key)<0)
		{
			ret=-5;
			goto End;
		}
		if (strlen(pCfg->value)>=size)
		{
			ret=-6;
			goto End;
		}
		
		strcpy(str,pCfg->value);
		ret=0;
	}
	else
	{
		return -3;
	}
	
End:
	cfg_done(pCfg);
#endif

	return ret;
}

int  Cfg_GetKey(void *CFG_file, void *section, void *key, void *buf)
{
	int ret=0;

	ret=Cfg_ReadStr(CFG_file,section,key,buf);	
	
	return ret;
}

int  Cfg_SetKey(void *CFG_file, void *section, void *key, void *buf)
{
	int ret=0;

	ret=Cfg_WriteStr(CFG_file,section,key,buf);	
	
	return ret;
}

int  Cfg_GetSections(void *CFG_file, char *sections[])
{
	PCONFIG pCfg;
	int ret=-1;
	int n_sections=0;

	ret=cfg_init (&pCfg, CFG_file, 0);		//do not create new file
	if (!ret)
	{
		if (0 == cfg_rewind (pCfg))
		{
			while (0 == cfg_next_section (pCfg)&& pCfg->section)
			{
				if (sections[n_sections])
					strcpy(sections[n_sections],pCfg->section);
				n_sections++;
			}
			ret=n_sections;
		}
		cfg_done(pCfg);
	}

	return ret;
}


int  Cfg_GetKeys(void *CFG_file, void *section, char *keys[])
{
	PCONFIG pCfg;
	int ret=-1;
	int n_keys=0;

	ret=cfg_init (&pCfg, CFG_file, 0);		//do not create new file
	if (!ret)
	{
		if (0 == cfg_rewind (pCfg))
		{
			while (0 == cfg_nextentry (pCfg))
			{
				if (cfg_define (pCfg)
				&& !strcmp (pCfg->section, section) && pCfg->id)
				{
					if (keys[n_keys])
						strcpy(keys[n_keys],pCfg->id);
					n_keys++;
				}
			}
			ret=n_keys;
		}
		cfg_done(pCfg);
	}

	return ret;
}

int  Cfg_RemoveKey(void *CFG_file, void *section, void *key)
{
	int ret=0;

	ret=Cfg_WriteStr(CFG_file,section,key,NULL);

	return ret;
}

#ifdef	CFG_MTD_CACHE
/*
Desc:cache operation, use to read /mnt/mtd/xxx.ini file. cache is /tmp/mtd/xxx.ini
read opersion is from /tm/mtd/xxx.ini, write is using write-through mode, write to /mnt/mtd/xxx.ini and /tmp/mtd/xxx.ini both
NOTE:char *FileOnly		//not full file name, no directory path
example: int ret=Cfg_mtdReadStr("ptzrun.conf","common","curzoom",&zoomrate); 

*/
int Cfg_mtdReadStr(char *FileOnly,char *Section,char *Entry,char *valptr)
{ 
	PCONFIG pCfg;
	int ret=-1;
	char filename[MAX_FILE_NAME]={0};

	strcpy(filename,MTD_CACHE_DIR);
	strcat(filename,FileOnly);
	
	ret=cfg_init (&pCfg, filename, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,valptr);
		if (ret)	//fail
			*valptr=NULL_STR;
		cfg_done(pCfg);
	}

	return ret;
}

//decimal number(not 0xHHHH)
int  Cfg_mtdReadInt(char *FileOnly,char *Section,char *Entry,long *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;
	char filename[MAX_FILE_NAME]={0};

	strcpy(filename,MTD_CACHE_DIR);
	strcat(filename,FileOnly);
	
	ret=cfg_init (&pCfg, filename, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (ret==0)	//success
			*valptr = atoi(sValue);		//decimal number 
	//	else
	//		*valptr = NULL_VAL;
		cfg_done(pCfg);
	}
	return ret;
}

//decimal number(not 0xHHHH)
int Cfg_mtdReadShort(char *FileOnly,char *Section,char *Entry,unsigned short *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;
	char *p=sValue;
	char filename[MAX_FILE_NAME]={0};

	strcpy(filename,MTD_CACHE_DIR);
	strcat(filename,FileOnly);
	
	ret=cfg_init (&pCfg, filename, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (!ret)
		{
			*valptr = atoi(p);		//decimal number 
			//*valptr = strtol(sValue,(char**)NULL,10);
			//*valptr = strtol(sValue,(char**)NULL,16);
		}
//		else
//			*valptr=NULL_VAL;

		cfg_done(pCfg);
	}

	return ret;
}

//decimal number(not 0xHHHH)
int Cfg_mtdReadbyte(char *FileOnly,char *Section,char *Entry,unsigned char *valptr)
{ 
	PCONFIG pCfg;
	char sValue[64];
	int ret=-1;
	char filename[MAX_FILE_NAME]={0};

	strcpy(filename,MTD_CACHE_DIR);
	strcat(filename,FileOnly);
	
	ret=cfg_init (&pCfg, filename, 0);		//do not create new file
	if (!ret)
	{
		ret=cfg_getstring(pCfg, Section, Entry,sValue);
		if (!ret)
		{
			//*valptr = atoi(pCfg->value);		//decimal number 
			*valptr = atoi(sValue);		//decimal number 
		}
		else
			*valptr=0xFF;
		
		cfg_done(pCfg);
	}
	return ret;
}

//decimal number(not 0xHHHH), using write-through mode
int Cfg_mtdWriteStr(char *FileOnly,char *Section,char *Entry,char *valptr)
{ 
	PCONFIG pCfg;
	int ret=-1;
	char filename[MAX_FILE_NAME]={0};

	strcpy(filename,MTD_CACHE_DIR);strcat(filename,FileOnly);

	ret=cfg_init (&pCfg, filename, 0);	//do not create file if not exist
	if (!ret)
	{
		cfg_write (pCfg, Section, Entry, valptr);

		//cfg_copy(pCfg,filename);
		cfg_commit(pCfg);
		
		strcpy(filename,MTD_DIR);strcat(filename,FileOnly);
		cfg_copy(pCfg,filename);

		cfg_done(pCfg);
	}
	
	return ret;
}


//decimal number(not 0xHHHH), using write-through mode
int Cfg_mtdWriteInt(char *FileOnly,char *Section,char *Entry,long Value)
{ 
	int ret=0;
	
	char sValue[64];
	
	sprintf(sValue,"%ld",Value);		//convert to string, decimal number
	ret=Cfg_mtdWriteStr(FileOnly,Section,Entry,sValue);

	return ret;
}
#endif		//#ifdef	CFG_MTD_CACHE

#ifdef	CFG_MD5
#include "md5.c"
#endif	//#ifdef	CFG_MD5

int utl_fgets (char *fileName,char *line, int bLock)
{
	int ret=0;
	int fd=-1;
	FILE *fp=NULL;

	if ( (!fileName)||(!line) )
		return -1;
	
	if (bLock==1){
		if ((fd = open (fileName, O_RDONLY | O_BINARY)) <0)
		{
			ret=-2;
			goto End;
		}
		
		ret=_SimpleSetPosixLock(fd,F_RDLCK);
		if (ret<0)
		{
			ret=-100;
			goto End;
		}
	}
	
	fp = fopen(fileName, "rb");
	if (fp)
	{
		//fgets(line, 1024, fp);	//max line size
		if (fgets(line, 1024, fp) == NULL)
			ret=-3;
		else
			ret=1;
		fclose(fp);
	}
  
End:
	if (fd>=0){
		_SimpleSetPosixLock(fd,F_UNLCK);
		close (fd);
	}

	return ret;
}

int utl_fputs (char *fileName,char *line, int bLock)
{
	int ret=0;
	int fd=-1;
	FILE *fp=NULL;

	if ( (!fileName)||(!line) )
		return -1;
#if (CFG_ThreadLock==1)		
	pthread_mutex_lock(&gThreadLock);
#endif	
	if (bLock==1){
		if ((fd = open (fileName, O_RDWR | O_BINARY)) <0)
		{
			ret=-2;
			goto End;
		}
		
		ret=_SimpleSetPosixLock(fd,F_WRLCK);
		if (ret<0)
		{
			ret=-100;
			goto End;
		}
	}
	
	fp = fopen(fileName, "a+");
	if (fp)
	{
		fputs(line, fp);
		fputs("\n", fp);
		fclose(fp);
	}
  
End:
	if (fd>=0){
		_SimpleSetPosixLock(fd,F_UNLCK);
		close (fd);
	}
#if (CFG_ThreadLock==1)		
	pthread_mutex_unlock(&gThreadLock);
#endif	
	return ret;
}


#ifdef __cplusplus
}
#endif

