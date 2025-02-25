#include <assert.h>
#include "X11/Xlib.h"
#include "errors.h"
#include "drawing.h"
#include "resourceTypes.h"
#include "window.h"
#include "display.h"
#include "gc.h"
#include "colors.h"

// Inspired by https://github.com/csulmone/X11/blob/59029dc09211926a5c95ff1dd2b828574fefcde6/libX11-1.5.0/src/ImUtil.c

XImage* XCreateImage(Display* display, Visual* visual, unsigned int depth, int format, int offset,
                     char* data, unsigned int width, unsigned int height, int bitmap_pad,
                     int bytes_per_line) {
    // https://tronche.com/gui/x/xlib/utilities/XCreateImage.html
    XImage* image = malloc(sizeof(XImage));
    if (image == NULL) {
        handleOutOfMemory(0, display, 0, 0);
        return NULL;
    }
    LOG("%s: w = %d, h = %d\n", __func__, (int) width, (int) height);
    image->width = width;
    image->height = height;
    image->format = format;
    image->data = data;
    image->byte_order = MSBFirst;
    image->bitmap_unit = 8;
    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
    image->bitmap_bit_order = MSBFirst;
    #else
    image->bitmap_bit_order = LSBFirst;
    #endif
    image->depth = depth;
    image->bitmap_pad = bitmap_pad;
    image->bytes_per_line = bytes_per_line;
    if (format != ZPixmap) {
        image->bits_per_pixel = 1;
    } else {
        if (depth <= 4) {
            image->bits_per_pixel = 4;
        } else if (depth <= 8) {
            image->bits_per_pixel = 8;
        } else if (depth <= 16) {
            image->bits_per_pixel = 16;
        } else {
            image->bits_per_pixel = 32;
        }
    }
    if (bytes_per_line == 0) {
        image->bytes_per_line = width * image->bits_per_pixel / 8;
        assert(image->bytes_per_line > 0);
    }

    XInitImage(image);
    return image;
}

char* getImageDataPointer(XImage* image, unsigned int x, unsigned int y) {
    char* pointer = image->data;
    pointer += image->bytes_per_line * y;
    return pointer + (image->bits_per_pixel / 8) * x;
}

int XPutPixel(XImage* image, int x, int y, unsigned long pixel) {
    // https://tronche.com/gui/x/xlib/utilities/XPutPixel.html
//    LOG("%s on %p: %lu (%ld, %ld, %ld)\n", __func__, image, pixel,
//        (pixel >> 24) & 0xFF, (pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF);
    if (image->data == NULL) {
        LOG("Invalid argument: Got image with NULL data in XPutPixel\n");
        return 0;
    }
    char* pointer;
    switch (image->format) {
        case ZPixmap:
        case XYBitmap:
            pointer = getImageDataPointer(image, x, y);
            if (image->bits_per_pixel == 32) {
                *((unsigned long*) pointer) = pixel;
            }
            break;
        case XYPixmap:
            LOG("Warn: Got unimplemented format XYPixmap\n");
            break;
        default:
            LOG("Warn: Got invalid format %d\n", image->format);
            return 0;
    }
    return 1;
}

unsigned long XGetPixel(XImage* image, int x, int y) {
    // https://tronche.com/gui/x/xlib/utilities/XGetPixel.html
    //LOG("%s from %p: x = %d, y = %d\n", __func__, image, x, y);
    if (image->data == NULL) {
        LOG("Invalid argument: Got image with NULL data in XGetPixel\n");
        return 0; // TODO: throw error
    }
    char* pointer;
    switch (image->format) {
        case ZPixmap:
        case XYBitmap:
            pointer = getImageDataPointer(image, x, y);
//            LOG("%s: bits_per_pixel = %d, value = %x (%d)\n", __func__, image->bits_per_pixel, *pointer & 0xFF, (int) *pointer & 0xFF);
            if (image->bits_per_pixel == 32) {
                return *((unsigned long*) pointer);
            } else if (image->bits_per_pixel == 1) {
                return 0x000000FF | (*pointer << 12) | (*pointer << 8) | (*pointer << 4);
            }
            break;
        case XYPixmap:
            LOG("Warn: Got unimplemented format XYPixmap\n");
            break;
        default:
            LOG("Warn: Got invalid format %d\n", image->format);
    }
    return 0;
}

int destroyImage(XImage* image) {
    // https://tronche.com/gui/x/xlib/utilities/XDestroyImage.html
    if (image->data != NULL) {
        free(image->data);
    }
    free(image);
    return 1;
}

int XPutImage(Display* display, Drawable drawable, GC gc, XImage* image, int src_x, int src_y,
               int dest_x, int dest_y, unsigned int width, unsigned int height) {
    // https://tronche.com/gui/x/xlib/graphics/XPutImage.html
    SET_X_SERVER_REQUEST(display, X_PutImage);
    TYPE_CHECK(drawable, DRAWABLE, display, 0);
    LOG("%s: Drawing %p on %lu\n", __func__, image, drawable);
    // TODO: Implement this: Create Uint32* data, Create Texture from data, rendercopy

    SDL_Renderer* renderer = NULL;
    GET_RENDERER(drawable, renderer);
    if (renderer == NULL) {
        LOG("Failed to create renderer in %s: %s\n", __func__, SDL_GetError());
        handleError(0, display, drawable, 0, BadDrawable, 0);
        return -1;
    }

    Uint32* data = malloc(sizeof(Uint32) * width * height);
    if (data == NULL) {
        handleOutOfMemory(0, display, 0, 0);
        return -1;
    }

    unsigned int x, y;
    if (image->format == XYBitmap && 0) {
        GraphicContext* graphicContext = GET_GC(gc);
        for (x = 0; x < width; x++) {
            for (y = 0; y < height; y++) {
                unsigned long color = graphicContext->background;
                if (XGetPixel(image, src_x + x, src_y + y)) {
                    color = graphicContext->foreground;
                }
                //putPixel(surface, dest_x + x, dest_y + y, color);
                WARN_UNIMPLEMENTED;
            }
        }
    } else {
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                // FIXME: last pixel is crashing sometimes with memory access violation
                if (y == height - 1 && x == width - 1) break;
                unsigned long color = XGetPixel(image, src_x + x, src_y + y);
                //LOG("PIXEL %s: %d %d %lu (%ld, %ld, %ld)\n", __func__, src_x + x, src_y + y, color, (color >> 24) & 0xFF, (color >> 16) & 0xFF, (color >> 8) & 0xFF);
                //putPixel(surface, dest_x + x, dest_y + y, XGetPixel(image, src_x + x, src_y + y));

                data[y * width + x] = GET_RED_FROM_COLOR(color) << 24 | GET_GREEN_FROM_COLOR(color) << 16 | GET_BLUE_FROM_COLOR(color) << 8 | GET_ALPHA_FROM_COLOR(color);
            }
        }
    }

    //SDL_RenderClear(renderer);
    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (texture == NULL) {
        LOG("SDL_CreateTexture failed: %s\n", SDL_GetError());
        return -1;
    }
    if (SDL_UpdateTexture(texture, NULL, data, width * sizeof(Uint32)) < 0)
        printf("Update texture failed %s", SDL_GetError());
    SDL_Rect dst = {dest_x, dest_y, width, height};
    if (SDL_RenderCopy(renderer, texture, NULL, &dst) < 0) {
        LOG("SDL_RenderCopy failed: %s\n", SDL_GetError());
        return -1;
    }
    SDL_DestroyTexture(texture);
    free(data);
    SDL_RenderPresent(renderer);
    return 1;
}

XImage* XGetImage(Display* display, Drawable drawable, int x, int y, unsigned int width,
                  unsigned int height, unsigned long plane_mask, int format) {
    // https://tronche.com/gui/x/xlib/graphics/XGetImage.html
    SET_X_SERVER_REQUEST(display, X_GetImage);
    LOG("%s: From %lu\n", __func__, drawable);
    if (IS_TYPE(drawable, WINDOW) && drawable == SCREEN_WINDOW) {
        LOG("XGetImage called with SCREEN_WINDOW as argument!\n");
        // TODO: Handle error and check INPUTONLY
        return NULL;
    }
    // TODO: Figure this out
    int depth = 8;
//    if (drawable->type == PIXMAP) {
//
//    }
    int bytes_per_line = (depth / 8) * width;
    char* data = malloc(sizeof(char) * width * height);
    if (data == NULL) {
        handleOutOfMemory(0, display, 0, 0);
        return NULL;
    }
    // FIXME: The NULL will cause problems.
    XImage* image = XCreateImage(display, NULL, depth, format, 0, data, width, height,
                                 32, bytes_per_line);
    if (image == NULL) {
        free(data);
        return NULL;
    }
    //TODO: Implement: Read from Textur into data and Convert from data to image type
    //SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, GET_SURFACE(drawable)->pixels, GET_SURFACE(drawable)->pitch);

    SDL_Renderer* renderer = NULL;
    GET_RENDERER(drawable, renderer);
    if (renderer == NULL) {
        LOG("Failed to create renderer in %s: %s\n", __func__, SDL_GetError());
        handleError(0, display, drawable, 0, BadDrawable, 0);
        return NULL;
    }
    SDL_Surface *drawableSurface = getRenderSurface(renderer);

    unsigned int currX, currY;
    // TODO: Worry about XYPixmap
//    if (format == XYPixmap) {
    for (currX = 0; currX < width; currX++) {
        for (currY = 0; currY < height; currY++) {
            data[currY * width + currX] = plane_mask & getPixel(drawableSurface, x + currX, y + currY);
        }
    }
    return image;
}

Status _XInitImageFuncPtrs(XImage *image) {
    image->f.put_pixel = XPutPixel;
    image->f.get_pixel = XGetPixel;
    image->f.create_image = XCreateImage;
    image->f.destroy_image = destroyImage;
    image->f.add_pixel = NULL; // TODO
    image->f.sub_image = NULL; // TODO
    return 1;
}

Status XInitImage(XImage* image) {
    // https://tronche.com/gui/x/xlib/graphics/XInitImage.html
    return _XInitImageFuncPtrs(image);
}
