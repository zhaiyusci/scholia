#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(BUILDING_LIBINTL_SHIM)
#    define LIBINTL_SHIM_EXPORT __declspec(dllexport)
#  else
#    define LIBINTL_SHIM_EXPORT __declspec(dllimport)
#  endif
#else
#  define LIBINTL_SHIM_EXPORT
#endif

LIBINTL_SHIM_EXPORT extern int _nl_msg_cat_cntr;

LIBINTL_SHIM_EXPORT char *gettext(const char *msgid);
LIBINTL_SHIM_EXPORT char *dgettext(const char *domainname, const char *msgid);
LIBINTL_SHIM_EXPORT char *dcgettext(const char *domainname, const char *msgid, int category);
LIBINTL_SHIM_EXPORT char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n);
LIBINTL_SHIM_EXPORT char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n);
LIBINTL_SHIM_EXPORT char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n, int category);

LIBINTL_SHIM_EXPORT char *textdomain(const char *domainname);
LIBINTL_SHIM_EXPORT char *bindtextdomain(const char *domainname, const char *dirname);
LIBINTL_SHIM_EXPORT char *bind_textdomain_codeset(const char *domainname, const char *codeset);

#ifdef __cplusplus
}
#endif
