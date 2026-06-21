#include "libintl.h"

#include <string.h>

int _nl_msg_cat_cntr = 0;

static char current_domain[128] = "messages";
static char current_directory[1024] = "";
static char current_codeset[64] = "UTF-8";

static char *fallback_message(const char *singular, const char *plural, unsigned long int n)
{
    if (n == 1 || !plural) {
        return (char *)(singular ? singular : "");
    }
    return (char *)plural;
}

char *gettext(const char *msgid)
{
    return dgettext(current_domain, msgid);
}

char *dgettext(const char *domainname, const char *msgid)
{
    (void)domainname;
    return fallback_message(msgid, NULL, 1);
}

char *dcgettext(const char *domainname, const char *msgid, int category)
{
    (void)category;
    return dgettext(domainname, msgid);
}

char *ngettext(const char *msgid1, const char *msgid2, unsigned long int n)
{
    return dngettext(current_domain, msgid1, msgid2, n);
}

char *dngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n)
{
    (void)domainname;
    return fallback_message(msgid1, msgid2, n);
}

char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n, int category)
{
    (void)category;
    return dngettext(domainname, msgid1, msgid2, n);
}

char *textdomain(const char *domainname)
{
    if (domainname && *domainname) {
        strncpy(current_domain, domainname, sizeof(current_domain) - 1);
        current_domain[sizeof(current_domain) - 1] = '\0';
    }
    return current_domain;
}

char *bindtextdomain(const char *domainname, const char *dirname)
{
    (void)domainname;
    if (dirname) {
        strncpy(current_directory, dirname, sizeof(current_directory) - 1);
        current_directory[sizeof(current_directory) - 1] = '\0';
    }
    return current_directory;
}

char *bind_textdomain_codeset(const char *domainname, const char *codeset)
{
    (void)domainname;
    if (codeset) {
        strncpy(current_codeset, codeset, sizeof(current_codeset) - 1);
        current_codeset[sizeof(current_codeset) - 1] = '\0';
    }
    return current_codeset;
}
