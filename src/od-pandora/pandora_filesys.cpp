#include <sys/timeb.h>
#include <fcntl.h>
#include <unistd.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "zfile.h"
#include "options.h"


int my_setcurrentdir (const TCHAR *curdir, TCHAR *oldcur)
{
	int ret = 0;
	if (oldcur)
		getcwd(oldcur, MAX_DPATH);
	if (curdir)
		chdir (curdir);
	return ret;
}


int my_mkdir (const char*name)
{
  return mkdir(name, 0777);
}


int my_rmdir (const char*name)
{
  return rmdir(name);
}


int my_unlink (const char* name)
{
  return unlink(name);
}


int my_rename (const char* oldname, const char* newname)
{
  return rename(oldname, newname);
}


struct my_opendir_s {
	void *h;
};


struct my_opendir_s *my_opendir (const char* name)
{
	struct my_opendir_s *mod;

	mod = xmalloc (struct my_opendir_s, 1);
	if (!mod)
		return NULL;
  mod->h = opendir(name);
	if (mod->h == NULL) {
		xfree (mod);
		return NULL;
	}
	return mod;
}


void my_closedir (struct my_opendir_s *mod)
{
	if (mod)
		closedir((DIR *) mod->h);
	xfree (mod);
}


int my_readdir (struct my_opendir_s *mod, char* name)
{
  struct dirent *de;
  
  if (!mod)
	  return 0;
	
	de = readdir((DIR *) mod->h);
	if(de == 0)
	  return 0;
	strncpy(name, de->d_name, MAX_PATH);
	return 1;
}


struct my_openfile_s {
	int h;
};


void my_close (struct my_openfile_s *mos)
{
  if(mos)
    close(mos->h);
  xfree (mos);
}


uae_s64 int my_lseek (struct my_openfile_s *mos, uae_s64 int offset, int pos)
{
  return lseek(mos->h, offset, pos);
}


uae_s64 int my_fsize (struct my_openfile_s *mos)
{
  uae_s64 pos = lseek(mos->h, 0, SEEK_CUR);
  uae_s64 size = lseek(mos->h, 0, SEEK_END);
  lseek(mos->h, pos, SEEK_SET);
	return size;
}


unsigned int my_read (struct my_openfile_s *mos, void *b, unsigned int size)
{
  return read(mos->h, b, size);
}


unsigned int my_write (struct my_openfile_s *mos, void *b, unsigned int size)
{
  return write(mos->h, b, size);
}


int my_existsfile (const char *name)
{
	struct stat st;
	if (lstat (name, &st) == -1) {
		return 0;
	} else {
		if (!S_ISDIR(st.st_mode))
			return 1;
	}
        return 0;
}


int my_existsdir(const char *name)
{
  struct stat st;

  if(lstat(name, &st) == -1) {
    return 0;
  } else {
    if (S_ISDIR(st.st_mode))
      return 1;
  }
  return 0;
}


struct my_openfile_s *my_open (const TCHAR *name, int flags)
{
	struct my_openfile_s *mos;

	mos = xmalloc (struct my_openfile_s, 1);
	if (!mos)
		return NULL;
  if(flags & O_CREAT)
    mos->h = open(name, flags, 0660);
  else
    mos->h = open(name, flags);
	if (!mos->h) {
		xfree (mos);
		mos = NULL;
  }
  return mos;
}


int my_truncate (const TCHAR *name, uae_u64 len)
{
  return truncate(name, len);
}


int my_getvolumeinfo (const char *root)
{
  struct stat st;
  int ret = 0;

  if (lstat (root, &st) == -1)
    return -1;
  if (!S_ISDIR(st.st_mode))
    return -2;
  return ret;
}


FILE *my_opentext (const TCHAR *name)
{
    return fopen (name, "r");
}


bool my_issamepath(const TCHAR *path1, const TCHAR *path2)
{
	if (!_tcsicmp(path1, path2))
		return true;
	return false;
}


const TCHAR *my_getfilepart(const TCHAR *filename)
{
	const TCHAR *p;

	p = _tcsrchr(filename, '\\');
	if (p)
		return p + 1;
	p = _tcsrchr(filename, '/');
	if (p)
		return p + 1;
	return filename;
}


/* Returns 1 if an actual volume-name was found, 2 if no volume-name (so uses some defaults) */
int target_get_volume_name(struct uaedev_mount_info *mtinf, struct uaedev_config_info *ci, bool inserted, bool fullcheck, int cnt)
{
	sprintf(ci->volname, "DH_%c", ci->rootdir[0]);
  return 2;
}
