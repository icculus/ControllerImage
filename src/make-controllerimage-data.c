/*
 * ControllerImage; A simple way to obtain game controller images.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN 1
    #include <windows.h>

    // THIS ONLY DEALS WITH ASCII CHARS. DON'T USE ON FILES WITH ANYTHING THAT ISN'T BASIC AMERICAN ENGLISH FROM THE 1970s!

    typedef struct DirHandle
    {
        HANDLE handle;
        WIN32_FIND_DATAW data;
        char *charbuf;
        size_t charbuflen;
        int first;
    } DirHandle;

    static void *xmalloc(size_t len);
    static void *xrealloc(void *ptr, size_t len);

    static DirHandle *OpenDir(const char *path)
    {
        DirHandle *dirp = (DirHandle *) xmalloc(sizeof (DirHandle));

        const size_t slen = strlen(path);
        WCHAR *wstr = (WCHAR *) xmalloc((slen + 1) * sizeof (WCHAR));
        for (int i = 0; path[i]; i++) {
            wstr[i] = (WCHAR) path[i];
        }
        wstr[slen] = '\0';

        dirp->handle = FindFirstFileW(wstr, &dirp->data);
        free(wstr);
        if (dirp->handle == INVALID_HANDLE_VALUE) {
            free(dirp);
            return NULL;
        }

        dirp->charbuf = NULL;
        dirp->charbuflen = 0;
        dirp->first = 1;

        return dirp;
    }

    static void CloseDir(DirHandle *dirp)
    {
        if (dirp) {
            CloseHandle(dirp->handle);
            free(dirp->charbuf);
            free(dirp);
        }
    }

    static const char *ReadDir(DirHandle *dirp)
    {
        const char *retval = NULL;
        if (dirp) {
            if (dirp->first) {
                dirp->first = 0;  // we already read the first entry, return that.
            } else if (FindNextFileW(dirp->handle, &dirp->data) == 0) {  // out of entries.
                return NULL;
            }

            size_t slen; for (slen = 0; dirp->data.cFileName[slen]; slen++) { /* spin */ }

            if (slen > dirp->charbuflen) {
                dirp->charbuf = xrealloc(dirp->charbuf, slen + 1);
                dirp->charbuflen = slen;
            }

            for (size_t i = 0; i < slen; i++) {
                dirp->charbuf[i] = (char) dirp->data.cFileName[i];
            }
            dirp->charbuf[slen] = '\0';
            retval = dirp->charbuf;
        }
        return retval;
    }

#else
    #include <sys/types.h>
    #include <dirent.h>

    typedef DIR DirHandle;

    static DirHandle *OpenDir(const char *path)
    {
        return opendir(path);
    }

    static void CloseDir(DirHandle *dirp)
    {
        closedir(dirp);
    }

    static const char *ReadDir(DirHandle *dirp)
    {
        struct dirent *dent = readdir(dirp);
        return dent ? dent->d_name : NULL;
    }
#endif

// this whole program is kinda slapdash atm.

typedef struct DeviceItem
{
    int type;
    int image;
} DeviceItem;

typedef struct Guid
{
    unsigned char data[16];
} Guid;

typedef struct DeviceInfo
{
    int devid;
    int inherits;
    int num_items;
    int num_guids;
    DeviceItem *items;
    Guid *guids;
} DeviceInfo;

static int num_strings = 0;
static char **strings = NULL;
static int num_devices = 0;
static DeviceInfo *devices = NULL;
static int num_guids = 0;

static void *xrealloc(void *ptr, size_t len)
{
    ptr = realloc(ptr, len);
    if (!ptr) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    return ptr;
}

static void *xmalloc(size_t len)
{
    void *ptr = malloc(len);
    if (!ptr) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    return ptr;
}

static int cache_string(const char *str)
{
    for (int i = 0; i < num_strings; i++) {
        if (strcmp(strings[i], str) == 0) {
            return i;
        }
    }

    // add a new string.

    if (num_strings >= 0xFFFF) {  // currently stored in a Uint16 in the data file.
        fprintf(stderr, "Too many unique strings! We need to alter the data file format!\n");
        exit(1);
    }

    void *ptr = xrealloc(strings, (num_strings + 1) * sizeof (char *));
    strings = (char **) ptr;
    strings[num_strings] = strdup(str);
    if (!strings[num_strings]) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    num_strings++;

    return num_strings - 1;
}

static char *load_text_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open '%s' for reading: %s\n", path, strerror(errno));
        exit(1);
    }

    char *buf = NULL;
    int buflen = 0;

    while (1) {
        const int chunksize = 32 * 1024;
        void *ptr = xrealloc(buf, buflen + chunksize);
        buf = (char *) ptr;
        const int br = (int) fread(buf + buflen, 1, chunksize - 1, f);
        buflen += br;
        if (br < (chunksize - 1)) {
            if (ferror(f)) {
                fprintf(stderr, "Failed to read from '%s': %s\n", path, strerror(errno));
                free(buf);
                fclose(f);
                exit(1);
            }
            break;
        }
    }

    fclose(f);

    buf[buflen--] = '\0';  // make sure we're null-terminated.

    // trim ending whitespace/newlines...
    while (buflen >= 0) {
        if (!isspace(buf[buflen])) {
            break;
        }
        buf[buflen--] = '\0';
    }

    return buf;
}

static int cache_file_string(const char *path)
{
    char *buf = load_text_file(path);
    const int retval = cache_string(buf);
    free(buf);
    return retval;
}

static char **break_string_into_lines(char *str)
{
    int num_lines = 0;
    char **retval = NULL;
    char *start = str;
    int eof = 0;

    do {
        int eos = 0;
        if (*str == '\0') {
            eof = eos = 1;
        } else if (*str == '\r') {
            eos = 1;
            *str = '\0';
            if (str[1] == '\n') {
                str++;
            }
        } else if (*str == '\n') {
            eos = 1;
            *str = '\0';
        }

        str++;

        if (eos) {
            retval = (char **) xrealloc(retval, sizeof (char *) * (num_lines + 1));
            retval[num_lines] = start;
            num_lines++;
            start = str;
        }
    } while (!eof);

    retval = (char **) xrealloc(retval, sizeof (char *) * (num_lines + 1));
    retval[num_lines] = NULL;
    return retval;
}

static int convert_hex_char(char ch, int *bad)
{
    if ((ch >= 'a') && (ch <= 'f')) {
        return ((int) (ch - 'a')) + 10;
    } else if ((ch >= 'A') && (ch <= 'F')) {
        return ((int) (ch - 'A')) + 10;
    } else if ((ch >= '0') && (ch <= '9')) {
        return ((int) (ch - '0'));
    } else {
        *bad = 1;
    }
    return 0;
}

static void parse_device_guids_file(DeviceInfo *device, const char *path)
{
    char *buf = load_text_file(path);
    char **lines = break_string_into_lines(buf);

    for (int i = 0; lines[i] != NULL; i++) {
        char *start = lines[i];
        char *ptr;

        // trim whitespace on the left.
        while (*start && isspace(*start)) {
            start++;
        }

        // chop out comments.
        for (ptr = start; *ptr; ptr++) {
            if (*ptr == '#') {
                *ptr = '\0';
                break;
            }
        }

        // trim whitespace on the right.
        while (ptr-- != start) {
            if (!isspace(*ptr)) {
                break;
            }
            *ptr = '\0';
        }

        if (*start == '\0') {
            continue;  // it's a line that only had whitespace and/or a comment.
        }

        Guid guid;
        int bad = 0;
        for (int j = 0; (j < 16) && !bad; j++) {
            const char ch1 = start[j*2];
            if (ch1 == '\0') {
                bad = 1;
            } else {
                const char ch2 = start[(j * 2) + 1];
                const int ich1 = convert_hex_char(ch1, &bad);
                const int ich2 = convert_hex_char(ch2, &bad);
                guid.data[j] = (unsigned char) ((ich1 << 4) | ich2);
            }
        }

        if (bad) {
            fprintf(stderr, "WARNING: Invalid GUID in '%s' line %d\n", path, i + 1);
            continue;
        }

        device->guids = (Guid *) xrealloc(device->guids, (device->num_guids + 1) * sizeof (Guid));
        Guid *pguid = &device->guids[device->num_guids++];
        memcpy(pguid, &guid, sizeof (Guid));

        num_guids++;
    }

    free(lines);
    free(buf);
}

static void process_gamepad_dir(const char *devid, const char *path)
{
    if (num_devices >= 0xFFFF) {  // currently stored in a Uint16 in the data file.
        fprintf(stderr, "Too many unique devices! We need to alter the data file format!\n");
        exit(1);
    }

    void *ptr = xrealloc(devices, (num_devices + 1) * sizeof (DeviceInfo));
    devices = (DeviceInfo *) ptr;
    DeviceInfo *device = &devices[num_devices];
    num_devices++;

    DirHandle *dirp = OpenDir(path);
    if (!dirp) {
        if (errno == ENOTDIR) {
            return;  // not an error, might be readme.txt or something.
        }
        fprintf(stderr, "Couldn't opendir '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    memset(device, '\0', sizeof (*device));
    device->devid = cache_string(devid);

    const char *node;
    while ((node = ReadDir(dirp)) != NULL) {
        if (strcmp(node, ".") == 0) { continue; }
        if (strcmp(node, "..") == 0) { continue; }

        char *ext = strrchr(node, '.');
        const size_t slen = strlen(path) + strlen(node) + 2;
        char *fullpath = (char *) xmalloc(slen);
        snprintf(fullpath, slen, "%s/%s", path, node);

        if (strcmp(node, "inherits") == 0) {
            device->inherits = cache_file_string(fullpath);
        } else if (strcmp(node, "guids") == 0) {
            parse_device_guids_file(device, fullpath);
        } else if (ext && (strcmp(ext, ".svg") == 0)) {
            if (device->num_items >= 0xFFFF) {  // currently stored in a Uint16 in the data file.
                fprintf(stderr, "Too many unique device items! We need to alter the data file format!\n");
                exit(1);
            }

            *ext = '\0';
            ptr = xrealloc(device->items, (device->num_items + 1) * sizeof (DeviceItem));
            device->items = (DeviceItem *) ptr;
            DeviceItem *item = &device->items[device->num_items++];
            item->type = cache_string(node);
            item->image = cache_file_string(fullpath);
        }

        free(fullpath);
    }

    CloseDir(dirp);
}

static void writeui16(FILE *f, int val)
{
    if ((val < 0) || (val > 0xFFFF)) {
        fprintf(stderr, "BUG: Expected Uint16 value, got %d instead!\n", val);
        fclose(f);
        exit(1);
    }

    const unsigned char ui8[2] = { (((unsigned int) val) >> 8) & 0xFF, (((unsigned int) val) >> 0) & 0xFF };
    fwrite(ui8, 1, 2, f);
}

static void process_devicetype_dir(const char *devicetype, const char *path)
{
    size_t slen = strlen(path) + strlen(devicetype) + 2;
    char *fulltypepath = (char *) xmalloc(slen);
    snprintf(fulltypepath, slen, "%s/%s", path, devicetype);

    DirHandle *dirp = OpenDir(fulltypepath);
    if (!dirp) {
        if (errno == ENOENT) {
            free(fulltypepath);
            return;  // not an error, doesn't exist.
        }
        fprintf(stderr, "Couldn't opendir '%s': %s\n", fulltypepath, strerror(errno));
        exit(1);
    }

    const char *devid;
    while ((devid = ReadDir(dirp)) != NULL) {
        if (strcmp(devid, ".") == 0) { continue; }
        if (strcmp(devid, "..") == 0) { continue; }
        slen = strlen(fulltypepath) + strlen(devid) + 2;
        char *fullpath = (char *) xmalloc(slen);
        snprintf(fullpath, slen, "%s/%s", fulltypepath, devid);
        process_gamepad_dir(devid, fullpath);
        free(fullpath);
    }
    CloseDir(dirp);

    free(fulltypepath);
}

static void process_theme_dir(const char *theme, const char *path)
{
    // add something, just to make sure the string index 0 isn't something that could be nullable.
    // this could be more clever, but for an extra byte in the data file, it's good enough.
    cache_string("");

    process_devicetype_dir("gamepad", path);

    size_t slen;

    const char *binfile_basename = "controllerimage";
    slen = strlen(binfile_basename) + strlen(theme) + 6;
    char *binfile = (char *) xmalloc(slen);
    snprintf(binfile, slen, "%s-%s.bin", binfile_basename, theme);

    FILE *f = fopen(binfile, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", binfile, strerror(errno));
        free(binfile);
        exit(1);
    }

    static const char magic[8] = { 'C', 'T', 'I', 'M', 'G', '\r', '\n', '\0' };

    fwrite(magic, 1, sizeof (magic), f);
    writeui16(f, 2);  // version number
    writeui16(f, num_strings);
    for (int i = 0; i < num_strings; i++) {
        fwrite(strings[i], 1, strlen(strings[i]) + 1, f);
    }
    writeui16(f, num_devices);
    for (int i = 0; i < num_devices; i++) {
        const DeviceInfo *device = &devices[i];
        writeui16(f, device->devid);
        writeui16(f, device->inherits);
        writeui16(f, device->num_items);
        writeui16(f, device->num_guids);
        for (int j = 0; j < device->num_items; j++) {
            const DeviceItem *item = &device->items[j];
            writeui16(f, item->type);
            writeui16(f, item->image);
        }
        for (int j = 0; j < device->num_guids; j++) {
            const Guid *guid = &device->guids[j];
            fwrite(guid->data, 1, sizeof (guid->data), f);
        }
    }

    if (fclose(f) == EOF) {
        fprintf(stderr, "Failed to fclose '%s': %s\n", binfile, strerror(errno));
        remove(binfile);
        free(binfile);
        exit(1);
    }

    printf("Filename: %s\n", binfile);
    printf("Num devices: %d\n", num_devices);
    printf("Num strings: %d\n", num_strings);
    printf("Num GUIDs: %d\n", num_guids);
    printf("\n");

    free(binfile);

    for (int i = 0; i < num_strings; i++) {
        free(strings[i]);
    }
    free(strings);
    strings = NULL;
    num_strings = 0;
    num_guids = 0;

    for (int i = 0; i < num_devices; i++) {
        free(devices[i].items);
        free(devices[i].guids);
    }
    free(devices);
    devices = NULL;
    num_devices = 0;
}

static void usage_and_exit(const char *argv0)
{
    fprintf(stderr, "USAGE: %s <path_to_art_directory>\n", argv0);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *basedir = NULL;
    if (argc >= 2) {
        basedir = argv[1];
    } else {
        usage_and_exit(argv[0]);
    }

    DirHandle *dirp = OpenDir(basedir);
    if (!dirp) {
        fprintf(stderr, "Couldn't opendir '%s': %s\n", basedir, strerror(errno));
        return 1;
    }

    const char *theme;
    while ((theme = ReadDir(dirp)) != NULL) {
        if (strcmp(theme, ".") == 0) { continue; }
        if (strcmp(theme, "..") == 0) { continue; }
        size_t slen = strlen(basedir) + strlen(theme) + 2;
        char *fullpath = (char *) xmalloc(slen);
        snprintf(fullpath, slen, "%s/%s", basedir, theme);
        process_theme_dir(theme, fullpath);
        free(fullpath);
    }
    CloseDir(dirp);

    return 0;
}

