# TPV - Terminal Picture Viewer

TPV is a simple tool for printing images in the terminal, basically a CAT for image files.

## Supported formats

The range of formats is decided by stb_image which is used for the decoding.

By their own quick documentation:

```
JPEG baseline & progressive (12 bpc/arithmetic not supported, same as stock IJG lib)
PNG 1/2/4/8/16-bit-per-channel
TGA (not sure what subset, if a subset)
BMP non-1bpp, non-RLE
PSD (composited view only, no extra channels, 8/16 bit-per-channel)
GIF (*comp always reports as 4-channel)
HDR (radiance rgbE format)
PIC (Softimage PIC)
PNM (PPM and PGM binary only)
```

Even though GIF is supported by stb_image, it is not supported by TPV.

More detailed information in [their project](https://github.com/nothings/stb).

## How to use

Usage of TPV is really simple, just provide the path to an image and it will be printed to the console.

Options are availible for printing in different color depths, grayscale etc. All options can be listed by running

```
tpv -h
```

or

```
tpv --help
```

More information about the options can be found in the manpage for TPV.

## Installation

Each release of TPV is distributed precompiled for Debian as a DEB package. However, it should compile on any glibc-system. Just download the source code and run

```
make install
```

and TPV will be installed along with its documentation.

## Contact

### Issues

Please open an issue if you encounter a bug or weird behaviour. Also open an issue if you have a request or suggestion for improvements.

All opened issues for bugs should include your used terminal emulator, shell and OS. If the issue is with a specific image, please provide that as well as long as it is not unreasonably private.

### Feedback

I welcome all feedback on the tool, whether it be praise or lambast. And while it is not mandatory to inform me, I would love to hear if you are including this in another project or package.

## License

This project is subject to the BSD-3-Clause license. Copyright (c) 2026 Emil Stenström. Full license information in the LICENSE file.

stb_image is subject to the MIT license. Copyright (c) 2017 Sean Barrett. Full license information and list of contributors included in stb_image.h and also available on the [projects github](https://github.com/nothings/stb?tab=License-1-ov-file).