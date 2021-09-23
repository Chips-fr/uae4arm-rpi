/********************************************************************
 * Preferences handling. This is just a convenient place to put it  *
 ********************************************************************/

bool check_prefs_changed_comp (bool checkonly)
{
  bool changed = 0;

	if (currprefs.compfpu != changed_prefs.compfpu ||
		currprefs.fpu_strict != changed_prefs.fpu_strict ||
		currprefs.cachesize != changed_prefs.cachesize)
		changed = 1;

	if (checkonly)
		return changed;

	currprefs.compfpu = changed_prefs.compfpu;
	currprefs.fpu_strict = changed_prefs.fpu_strict;

  if (currprefs.cachesize != changed_prefs.cachesize) {
	  currprefs.cachesize = changed_prefs.cachesize;
	  alloc_cache();
	  changed = 1;
  }

	if (changed)
		write_log (_T("JIT: cache=%d. fpu=%d\n"),
		currprefs.cachesize,
		currprefs.compfpu);

  return changed;
}
