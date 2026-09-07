#ifndef IMAGEINFO_H
#define IMAGEINFO_H

typedef struct {
    int w, h, c;
    unsigned char *data;
} ImageInfo;

#define IMAGEINFO_MT "ImageInfo"

#endif