#include "libintl.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int _nl_msg_cat_cntr = 0;

typedef struct DomainBinding {
    char domain[128];
    char directory[1024];
    char codeset[64];
    struct DomainBinding *next;
} DomainBinding;

typedef struct MoEntry {
    const char *id;
    uint32_t id_len;
    const char *translation;
    uint32_t translation_len;
} MoEntry;

typedef struct MoCatalog {
    char domain[128];
    char language[64];
    char path[2048];
    unsigned char *data;
    size_t data_size;
    MoEntry *entries;
    uint32_t entry_count;
    struct MoCatalog *next;
} MoCatalog;

static char current_domain[128] = "messages";
static DomainBinding *bindings = NULL;
static MoCatalog *catalogs = NULL;

static void copy_string(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, destination_size - 1);
    destination[destination_size - 1] = '\0';
}

static DomainBinding *binding_for_domain(const char *domain, int create)
{
    const char *effective_domain = (domain && *domain) ? domain : current_domain;
    for (DomainBinding *binding = bindings; binding; binding = binding->next) {
        if (strcmp(binding->domain, effective_domain) == 0) {
            return binding;
        }
    }
    if (!create) {
        return NULL;
    }

    DomainBinding *binding = (DomainBinding *)calloc(1, sizeof(DomainBinding));
    if (!binding) {
        return NULL;
    }
    copy_string(binding->domain, sizeof(binding->domain), effective_domain);
    copy_string(binding->codeset, sizeof(binding->codeset), "UTF-8");
    binding->next = bindings;
    bindings = binding;
    return binding;
}

static uint32_t read_u32(const unsigned char *data, int big_endian)
{
    if (big_endian) {
        return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
    }
    return ((uint32_t)data[3] << 24) | ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
}

static void normalize_language(char *language)
{
    if (!language) {
        return;
    }
    for (char *p = language; *p; ++p) {
        if (*p == '-') {
            *p = '_';
        }
        if (*p == '.' || *p == '@') {
            *p = '\0';
            break;
        }
    }
}

static int add_language(char languages[][64], int count, int maximum, const char *language)
{
    if (!language || !*language || count >= maximum) {
        return count;
    }

    char normalized[64];
    copy_string(normalized, sizeof(normalized), language);
    normalize_language(normalized);
    if (!*normalized || strcmp(normalized, "C") == 0 || strcmp(normalized, "POSIX") == 0) {
        return count;
    }

    for (int i = 0; i < count; ++i) {
        if (strcmp(languages[i], normalized) == 0) {
            return count;
        }
    }
    copy_string(languages[count++], 64, normalized);

    char *separator = strchr(normalized, '_');
    if (separator && count < maximum) {
        *separator = '\0';
        int exists = 0;
        for (int i = 0; i < count; ++i) {
            if (strcmp(languages[i], normalized) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists) {
            copy_string(languages[count++], 64, normalized);
        }
    }
    return count;
}

static int collect_languages(char languages[][64], int maximum)
{
    int count = 0;
    const char *language_env = getenv("LANGUAGE");
    if (language_env && *language_env) {
        char buffer[512];
        copy_string(buffer, sizeof(buffer), language_env);
        char *context = NULL;
        char *token = strtok_s(buffer, ":", &context);
        while (token) {
            count = add_language(languages, count, maximum, token);
            token = strtok_s(NULL, ":", &context);
        }
    }

    if (count == 0) {
        count = add_language(languages, count, maximum, getenv("LC_ALL"));
        count = add_language(languages, count, maximum, getenv("LC_MESSAGES"));
        count = add_language(languages, count, maximum, getenv("LANG"));
    }

    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH];
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
        char locale_utf8[64];
        if (WideCharToMultiByte(CP_UTF8, 0, locale_name, -1, locale_utf8, sizeof(locale_utf8), NULL, NULL) > 0) {
            count = add_language(languages, count, maximum, locale_utf8);
        }
    }

    count = add_language(languages, count, maximum, "en_US");
    return count;
}

static int file_exists(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static void append_path(char *destination, size_t destination_size, const char *left, const char *right)
{
    copy_string(destination, destination_size, left);
    size_t length = strlen(destination);
    if (length > 0 && destination[length - 1] != '\\' && destination[length - 1] != '/') {
        strncat(destination, "\\", destination_size - strlen(destination) - 1);
    }
    strncat(destination, right, destination_size - strlen(destination) - 1);
}

static int application_locale_dir(char *directory, size_t directory_size)
{
    wchar_t module_path[MAX_PATH];
    DWORD length = GetModuleFileNameW(NULL, module_path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return 0;
    }

    for (wchar_t *p = module_path + wcslen(module_path); p > module_path; --p) {
        if (*p == L'\\' || *p == L'/') {
            *p = L'\0';
            break;
        }
    }

    char app_dir[1024];
    if (WideCharToMultiByte(CP_UTF8, 0, module_path, -1, app_dir, sizeof(app_dir), NULL, NULL) <= 0) {
        return 0;
    }
    append_path(directory, directory_size, app_dir, "data\\locale");
    return 1;
}

static void make_catalog_path(char *path, size_t path_size, const char *locale_dir, const char *language, const char *domain)
{
    char language_dir[1400];
    char messages_dir[1600];
    char file_name[256];

    append_path(language_dir, sizeof(language_dir), locale_dir, language);
    append_path(messages_dir, sizeof(messages_dir), language_dir, "LC_MESSAGES");
    snprintf(file_name, sizeof(file_name), "%s.mo", domain);
    append_path(path, path_size, messages_dir, file_name);
}

static int read_file(const char *path, unsigned char **data, size_t *data_size)
{
    FILE *file = fopen(path, "rb");
    if (!file) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return 0;
    }
    rewind(file);

    unsigned char *buffer = (unsigned char *)malloc((size_t)size);
    if (!buffer) {
        fclose(file);
        return 0;
    }
    if (fread(buffer, 1, (size_t)size, file) != (size_t)size) {
        free(buffer);
        fclose(file);
        return 0;
    }
    fclose(file);
    *data = buffer;
    *data_size = (size_t)size;
    return 1;
}

static MoCatalog *parse_catalog(const char *domain, const char *language, const char *path)
{
    unsigned char *data = NULL;
    size_t data_size = 0;
    if (!read_file(path, &data, &data_size) || data_size < 28) {
        return NULL;
    }

    uint32_t magic_little = read_u32(data, 0);
    int big_endian = 0;
    if (magic_little == 0x950412deU) {
        big_endian = 0;
    } else if (magic_little == 0xde120495U) {
        big_endian = 1;
    } else {
        free(data);
        return NULL;
    }

    uint32_t count = read_u32(data + 8, big_endian);
    uint32_t original_offset = read_u32(data + 12, big_endian);
    uint32_t translation_offset = read_u32(data + 16, big_endian);
    if ((size_t)original_offset + ((size_t)count * 8U) > data_size ||
        (size_t)translation_offset + ((size_t)count * 8U) > data_size) {
        free(data);
        return NULL;
    }

    MoEntry *entries = (MoEntry *)calloc(count ? count : 1, sizeof(MoEntry));
    if (!entries) {
        free(data);
        return NULL;
    }

    for (uint32_t i = 0; i < count; ++i) {
        const unsigned char *original = data + original_offset + (i * 8U);
        const unsigned char *translation = data + translation_offset + (i * 8U);
        uint32_t id_len = read_u32(original, big_endian);
        uint32_t id_offset = read_u32(original + 4, big_endian);
        uint32_t translation_len = read_u32(translation, big_endian);
        uint32_t translated_offset = read_u32(translation + 4, big_endian);
        if ((size_t)id_offset + id_len >= data_size || (size_t)translated_offset + translation_len >= data_size) {
            continue;
        }
        entries[i].id = (const char *)(data + id_offset);
        entries[i].id_len = id_len;
        entries[i].translation = (const char *)(data + translated_offset);
        entries[i].translation_len = translation_len;
    }

    MoCatalog *catalog = (MoCatalog *)calloc(1, sizeof(MoCatalog));
    if (!catalog) {
        free(entries);
        free(data);
        return NULL;
    }
    copy_string(catalog->domain, sizeof(catalog->domain), domain);
    copy_string(catalog->language, sizeof(catalog->language), language);
    copy_string(catalog->path, sizeof(catalog->path), path);
    catalog->data = data;
    catalog->data_size = data_size;
    catalog->entries = entries;
    catalog->entry_count = count;
    catalog->next = catalogs;
    catalogs = catalog;
    return catalog;
}

static MoCatalog *catalog_for_domain_language(const char *domain, const char *language)
{
    for (MoCatalog *catalog = catalogs; catalog; catalog = catalog->next) {
        if (strcmp(catalog->domain, domain) == 0 && strcmp(catalog->language, language) == 0) {
            return catalog;
        }
    }

    const DomainBinding *binding = binding_for_domain(domain, 0);
    char path[2048];
    if (binding && binding->directory[0]) {
        make_catalog_path(path, sizeof(path), binding->directory, language, domain);
        if (file_exists(path)) {
            return parse_catalog(domain, language, path);
        }
    }

    char locale_dir[1024];
    if (application_locale_dir(locale_dir, sizeof(locale_dir))) {
        make_catalog_path(path, sizeof(path), locale_dir, language, domain);
        if (file_exists(path)) {
            return parse_catalog(domain, language, path);
        }
    }

    return NULL;
}

static const char *lookup_message(const char *domain, const char *msgid, size_t msgid_len, uint32_t *translation_len)
{
    if (!domain || !*domain) {
        domain = current_domain;
    }

    char languages[8][64];
    int language_count = collect_languages(languages, 8);
    for (int i = 0; i < language_count; ++i) {
        MoCatalog *catalog = catalog_for_domain_language(domain, languages[i]);
        if (!catalog) {
            continue;
        }
        for (uint32_t j = 0; j < catalog->entry_count; ++j) {
            const MoEntry *entry = &catalog->entries[j];
            if (entry->id && entry->id_len == msgid_len && memcmp(entry->id, msgid, msgid_len) == 0) {
                if (entry->translation && entry->translation_len > 0) {
                    if (translation_len) {
                        *translation_len = entry->translation_len;
                    }
                    return entry->translation;
                }
                return NULL;
            }
        }
    }
    return NULL;
}

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
    if (!msgid) {
        return "";
    }

    const char *translated = lookup_message(domainname, msgid, strlen(msgid), NULL);
    return (char *)(translated ? translated : msgid);
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
    if (!msgid1) {
        return "";
    }

    const size_t first_len = strlen(msgid1);
    const size_t second_len = msgid2 ? strlen(msgid2) : 0;
    const size_t key_len = first_len + 1U + second_len;
    char *key = (char *)malloc(key_len ? key_len : 1U);
    if (!key) {
        return fallback_message(msgid1, msgid2, n);
    }
    memcpy(key, msgid1, first_len);
    key[first_len] = '\0';
    if (msgid2) {
        memcpy(key + first_len + 1U, msgid2, second_len);
    }

    uint32_t translation_len = 0;
    const char *translated = lookup_message(domainname, key, key_len, &translation_len);
    free(key);
    if (!translated) {
        return fallback_message(msgid1, msgid2, n);
    }

    if (n == 1) {
        return (char *)translated;
    }
    const char *second = memchr(translated, '\0', translation_len);
    if (second && (uint32_t)(second + 1 - translated) < translation_len) {
        return (char *)(second + 1);
    }
    return (char *)translated;
}

char *dcngettext(const char *domainname, const char *msgid1, const char *msgid2, unsigned long int n, int category)
{
    (void)category;
    return dngettext(domainname, msgid1, msgid2, n);
}

char *textdomain(const char *domainname)
{
    if (domainname && *domainname) {
        copy_string(current_domain, sizeof(current_domain), domainname);
    }
    return current_domain;
}

char *bindtextdomain(const char *domainname, const char *dirname)
{
    DomainBinding *binding = binding_for_domain(domainname, 1);
    if (!binding) {
        return "";
    }
    if (dirname) {
        copy_string(binding->directory, sizeof(binding->directory), dirname);
        ++_nl_msg_cat_cntr;
    }
    return binding->directory;
}

char *bind_textdomain_codeset(const char *domainname, const char *codeset)
{
    DomainBinding *binding = binding_for_domain(domainname, 1);
    if (!binding) {
        return "";
    }
    if (codeset) {
        copy_string(binding->codeset, sizeof(binding->codeset), codeset);
    }
    return binding->codeset;
}
