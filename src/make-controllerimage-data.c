#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

// this whole program is kinda slapdash atm.

typedef struct DeviceItem
{
    int type;
    int image;
} DeviceItem;

typedef struct DeviceInfo
{
    int devid;
    int inherits;
    int num_items;
    DeviceItem *items;
} DeviceInfo;

static int num_strings = 0;
static char **strings = NULL;
static int num_devices = 0;
static DeviceInfo *devices = NULL;

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

    void *ptr = realloc(strings, (num_strings + 1) * sizeof (char *));
    if (!ptr) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    strings = (char **) ptr;
    strings[num_strings] = strdup(str);
    if (!strings[num_strings]) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }

    num_strings++;

    return num_strings - 1;
}

static int cache_file_string(const char *path)
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
        void *ptr = realloc(buf, buflen + chunksize);
        if (!ptr) {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
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

    const int retval = cache_string(buf);
    free(buf);
    return retval;
}

static void process_gamepad_dir(const char *devid, const char *path)
{
    if (num_devices >= 0xFFFF) {  // currently stored in a Uint16 in the data file.
        fprintf(stderr, "Too many unique devices! We need to alter the data file format!\n");
        exit(1);
    }

    void *ptr = realloc(devices, (num_devices + 1) * sizeof (DeviceInfo));
    if (!ptr) {
        fprintf(stderr, "Out of memory!\n");
        exit(1);
    }
    devices = (DeviceInfo *) ptr;
    DeviceInfo *device = &devices[num_devices];
    num_devices++;

    DIR *dirp = opendir(path);
    if (!dirp) {
        fprintf(stderr, "Couldn't opendir '%s': %s\n", path, strerror(errno));
        exit(1);
    }

    memset(device, '\0', sizeof (*device));
    device->devid = cache_string(devid);

    struct dirent *dent;
    while ((dent = readdir(dirp)) != NULL) {
        char *node = dent->d_name;
        if (strcmp(node, ".") == 0) { continue; }
        if (strcmp(node, "..") == 0) { continue; }

        const size_t slen = strlen(path) + strlen(node) + 2;
        char *fullpath = (char *) malloc(slen);
        snprintf(fullpath, slen, "%s/%s", path, node);

        if (strcmp(node, "inherits") == 0) {
            device->inherits = cache_file_string(fullpath);
            free(fullpath);
            continue;
        }

        char *ext = strrchr(node, '.');
        if (ext && (strcmp(ext, ".svg") == 0)) {
            if (device->num_items >= 0xFFFF) {  // currently stored in a Uint16 in the data file.
                fprintf(stderr, "Too many unique device items! We need to alter the data file format!\n");
                exit(1);
            }

            *ext = '\0';
            ptr = realloc(device->items, (device->num_items + 1) * sizeof (DeviceItem));
            if (!ptr) {
                fprintf(stderr, "Out of memory!\n");
                exit(1);
            }
            device->items = (DeviceItem *) ptr;
            DeviceItem *item = &device->items[device->num_items++];
            item->type = cache_string(node);
            item->image = cache_file_string(fullpath);
        }

        free(fullpath);
    }

    closedir(dirp);
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

int main(int argc, char **argv)
{
    const char *basedir = "data/gamepad";
    const char *binfile = "controllerimages.bin";

    // add something, just to make sure the string index 0 isn't something that could be nullable.
    // this could be more clever, but for an extra byte in the data file, it's good enough.
    cache_string("");

    DIR *dirp = opendir(basedir);
    if (!dirp) {
        fprintf(stderr, "Couldn't opendir '%s': %s\n", basedir, strerror(errno));
        return 1;
    }

    struct dirent *dent;
    while ((dent = readdir(dirp)) != NULL) {
        const char *devid = dent->d_name;
        if (strcmp(devid, ".") == 0) { continue; }
        if (strcmp(devid, "..") == 0) { continue; }
        char fullpath[512];
        snprintf(fullpath, sizeof (fullpath), "%s/%s", basedir, devid);
        process_gamepad_dir(devid, fullpath);
    }

    FILE *f = fopen(binfile, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", binfile, strerror(errno));
        closedir(dirp);
        return 1;
    }
    closedir(dirp);

    static const char magic[8] = { 'C', 'T', 'I', 'M', 'G', '\r', '\n', '\0' };

    fwrite(magic, 1, sizeof (magic), f);
    writeui16(f, 1);  // version number
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
        for (int j = 0; j < device->num_items; j++) {
            const DeviceItem *item = &device->items[j];
            writeui16(f, item->type);
            writeui16(f, item->image);
        }
    }

    if (fclose(f) == EOF) {
        fprintf(stderr, "Failed to fclose '%s': %s\n", binfile, strerror(errno));
        remove(binfile);
        exit(1);
    }

    printf("Num devices: %d\n", num_devices);
    printf("Num strings: %d\n", num_strings);

    for (int i = 0; i < num_strings; i++) {
        free(strings[i]);
    }
    free(strings);

    for (int i = 0; i < num_devices; i++) {
        free(devices[i].items);
    }
    free(devices);

    return 0;
}

