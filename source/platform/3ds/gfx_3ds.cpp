#ifdef BACKEND_3DS

#include <malloc.h>
#include <math.h>
#include <string.h>

#include "platform/common/menu.h"
#include "platform/gfx.h"
#include "platform/input.h"

#include <3ds.h>

#include <citrus/gpu.hpp>
#include <citrus/gput.hpp>

using namespace ctr;

static u32* screenBuffer;

static int prevScaleMode = -1;
static int prevScaleFilter = -1;
static int prevGameScreen = -1;
static bool borderChanged = false;

static bool fastForward = false;

static u32 texture = 0;
static u32 vbo = 0;

static u32 borderVbo = 0;
static u32 borderTexture = 0;

static u32 borderWidth = 0;
static u32 borderHeight = 0;
static u32 gpuBorderWidth = 0;
static u32 gpuBorderHeight = 0;

bool gfxInit() {
    // Allocate and clear the screen buffer.
    screenBuffer = (u32*) gpu::galloc(256 * 256 * sizeof(u32));
    memset(screenBuffer, 0, 256 * 256 * sizeof(u32));

    // Setup the GPU state.
    gpu::setCullMode(gpu::CULL_BACK_CCW);

    // Create the VBO.
    gpu::createVbo(&vbo);
    gpu::setVboAttributes(vbo, ATTRIBUTE(0, 3, gpu::ATTR_FLOAT) | ATTRIBUTE(1, 2, gpu::ATTR_FLOAT) | ATTRIBUTE(2, 4, gpu::ATTR_FLOAT), 3);

    // Create the texture.
    gpu::createTexture(&texture);

    return true;
}

void gfxCleanup() {
    // Free texture.
    if(texture != 0) {
        gpu::freeTexture(texture);
        texture = 0;
    }

    // Free VBO.
    if(vbo != 0) {
        gpu::freeVbo(vbo);
        vbo = 0;
    }

    // Free border texture.
    if(borderTexture != 0) {
        gpu::freeTexture(borderTexture);
        borderTexture = 0;
    }

    // Free border VBO.
    if(borderVbo != 0) {
        gpu::freeVbo(borderVbo);
        borderVbo = 0;
    }

    // Free screen buffer.
    if(screenBuffer != NULL) {
        gpu::gfree(screenBuffer);
        screenBuffer = NULL;
    }
}

bool gfxGetFastForward() {
    return fastForward || (!isMenuOn() && inputKeyHeld(FUNC_KEY_FAST_FORWARD));
}

void gfxSetFastForward(bool fastforward) {
    fastForward = fastforward;
}

void gfxToggleFastForward() {
    fastForward = !fastForward;
}

void gfxLoadBorder(u8* imgData, u32 imgWidth, u32 imgHeight) {
    if(imgData == NULL) {
        if(borderTexture != 0) {
            gpu::freeTexture(borderTexture);
            borderTexture = 0;
        }

        if(borderVbo != 0) {
            gpu::freeVbo(borderVbo);
            borderVbo = 0;
        }

        borderWidth = 0;
        borderHeight = 0;
        gpuBorderWidth = 0;
        gpuBorderHeight = 0;

        return;
    }

    // Adjust the texture to power-of-two dimensions.
    borderWidth = imgWidth;
    borderHeight = imgHeight;
    gpuBorderWidth = (u32) pow(2, ceil(log(borderWidth) / log(2)));
    gpuBorderHeight = (u32) pow(2, ceil(log(borderHeight) / log(2)));

    // Create the texture.
    if(borderTexture == 0) {
        gpu::createTexture(&borderTexture);
    }

    // Update texture info.
    gpu::setTextureInfo(borderTexture, gpuBorderWidth, gpuBorderHeight, gpu::PIXEL_RGBA8, TEXTURE_MIN_FILTER(gpu::FILTER_LINEAR) | TEXTURE_MAG_FILTER(gpu::FILTER_LINEAR));

    // Get texture contents.
    u8* borderBuffer;
    gpu::getTextureData(borderTexture, (void**) &borderBuffer);

    // Update texture contents.
    for(u32 x = 0; x < borderWidth; x++) {
        for(u32 y = 0; y < borderHeight; y++) {
            u32 src = (y * borderWidth + x) * 4;
            u32 dst = TEXTURE_INDEX(x, y, gpuBorderWidth, gpuBorderHeight) * 4;
            borderBuffer[dst + 0] = imgData[src + 3];
            borderBuffer[dst + 1] = imgData[src + 2];
            borderBuffer[dst + 2] = imgData[src + 1];
            borderBuffer[dst + 3] = imgData[src + 0];
        }
    }

    borderChanged = true;
}

u32* gfxGetLineBuffer(int line) {
    return screenBuffer + line * 256;
}

void gfxClearScreenBuffer(u8 r, u8 g, u8 b) {
    if(r == g && r == b) {
        memset(screenBuffer, r, 256 * 256 * sizeof(u32));
    } else {
        wmemset((wchar_t*) screenBuffer, (wchar_t) RGBA32(r, g, b), 256 * 256);
    }
}

#define PIXEL_AT( PX, XX, YY) ((PX)+((((YY)*256)+(XX))))
#define SCALE2XMACRO()       if (Bp!=Hp && Dp!=Fp) {        \
                                if (Dp==Bp) *(E0++) = Dp;   \
                                else *(E0++) = Ep;          \
                                if (Bp==Fp) *(E0++) = Fp;   \
                                else *(E0++) = Ep;          \
                                if (Dp==Hp) *(E0++) = Dp;   \
                                else *(E0++) = Ep;          \
                                if (Hp==Fp) *E0 = Fp;       \
                                else *E0 = Ep;              \
                             } else {                       \
                                *(E0++) = Ep;               \
                                *(E0++) = Ep;               \
                                *(E0++) = Ep;               \
                                *E0 = Ep;                   \
                             }

void gfxScale2x(u32* pixelBuffer) {
    int x, y;
    u32* E, * E0;
    u32 Ep, Bp, Dp, Fp, Hp;

    u32* buffer;
    gpu::getTextureData(texture, (void**) &buffer);

    // Top line and top corners
    E = PIXEL_AT(pixelBuffer, 0, 0);
    E0 = buffer + TEXTURE_INDEX(0, 0, 512, 512);
    Ep = E[0];
    Bp = Ep;
    Dp = Ep;
    Fp = E[1];
    Hp = E[256];
    SCALE2XMACRO();
    for(x = 1; x < 159; x++) {
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = Ep;
        Hp = E[256];
        E0 = buffer + TEXTURE_INDEX((u32) (x * 2), 0, 512, 512);
        SCALE2XMACRO();
    }

    E += 1;
    Dp = Ep;
    Ep = Fp;
    Bp = Ep;
    Hp = E[256];
    E0 = buffer + TEXTURE_INDEX(159 * 2, 0, 512, 512);
    SCALE2XMACRO();

    // Middle Rows and sides
    for(y = 1; y < 143; y++) {
        E = PIXEL_AT(pixelBuffer, 0, y);
        E0 = buffer + TEXTURE_INDEX(0, (u32) (y * 2), 512, 512);
        Ep = E[0];
        Bp = E[-256];
        Dp = Ep;
        Fp = E[1];
        Hp = E[256];
        SCALE2XMACRO();
        for(x = 1; x < 159; x++) {
            E += 1;
            Dp = Ep;
            Ep = Fp;
            Fp = E[1];
            Bp = E[-256];
            Hp = E[256];
            E0 = buffer + TEXTURE_INDEX((u32) (x * 2), (u32) (y * 2), 512, 512);
            SCALE2XMACRO();
        }
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Bp = E[-256];
        Hp = E[256];
        E0 = buffer + TEXTURE_INDEX(159 * 2, (u32) (y * 2), 512, 512);
        SCALE2XMACRO();
    }

    // Bottom Row and Bottom Corners
    E = PIXEL_AT(pixelBuffer, 0, 143);
    E0 = buffer + TEXTURE_INDEX(0, 143 * 2, 512, 512);
    Ep = E[0];
    Bp = E[-256];
    Dp = Ep;
    Fp = E[1];
    Hp = Ep;
    SCALE2XMACRO();
    for(x = 1; x < 159; x++) {
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = E[-256];
        Hp = Ep;
        E0 = buffer + TEXTURE_INDEX((u32) (x * 2), 143 * 2, 512, 512);
        SCALE2XMACRO();
    }

    E += 1;
    Dp = Ep;
    Ep = Fp;
    Bp = E[-256];
    Hp = Ep;
    E0 = buffer + TEXTURE_INDEX(159 * 2, 143 * 2, 512, 512);
    SCALE2XMACRO();
}

void gfxDrawScreen() {
    // Update the viewport.
    if(prevGameScreen != gameScreen) {
        gpu::Screen screen = gameScreen == 0 ? gpu::SCREEN_TOP : gpu::SCREEN_BOTTOM;
        u32 width = gameScreen == 0 ? TOP_WIDTH : BOTTOM_WIDTH;
        u32 height = gameScreen == 0 ? TOP_HEIGHT : BOTTOM_HEIGHT;

        gpu::setViewport(screen, 0, 0, width, height);
        gput::setOrtho(0, width, 0, height, -1, 1);
    }

    // Update the border VBO.
    if(borderTexture != 0 && (borderVbo == 0 || borderChanged || prevGameScreen != gameScreen)) {
        // Create the VBO.
        if(borderVbo == 0) {
            gpu::createVbo(&borderVbo);
            gpu::setVboAttributes(borderVbo, ATTRIBUTE(0, 3, gpu::ATTR_FLOAT) | ATTRIBUTE(1, 2, gpu::ATTR_FLOAT) | ATTRIBUTE(2, 4, gpu::ATTR_FLOAT), 3);
        }

        u32 swidth;
        u32 sheight;
        gpu::getViewportWidth(&swidth);
        gpu::getViewportHeight(&sheight);

        // Calculate VBO points.
        const float x1 = ((int) swidth - (int) borderWidth) / 2.0f;
        const float y1 = ((int) sheight - (int) borderHeight) / 2.0f;
        const float x2 = x1 + borderWidth;
        const float y2 = y1 + borderHeight;

        // Adjust for power-of-two textures.
        float leftHorizMod = 0;
        float rightHorizMod = (float) (gpuBorderWidth - borderWidth) / (float) gpuBorderWidth;
        float vertMod = (float) (gpuBorderHeight - borderHeight) / (float) gpuBorderHeight;

        // Prepare new VBO data.
        const float vboData[] = {
                x1, y1, -0.1f, 0.0f + leftHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y1, -0.1f, 1.0f - rightHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - rightHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - rightHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y2, -0.1f, 0.0f + leftHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y1, -0.1f, 0.0f + leftHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f
        };

        // Update the VBO with the new data.
        gpu::setVboData(borderVbo, vboData, 6 * 9, gpu::PRIM_TRIANGLES);
    }

    // Update VBO data if the size has changed.
    if(prevScaleMode != scaleMode || prevScaleFilter != scaleFilter || prevGameScreen != gameScreen) {
        u32 viewportWidth;
        u32 viewportHeight;
        gpu::getViewportWidth(&viewportWidth);
        gpu::getViewportHeight(&viewportHeight);

        // Calculate the VBO dimensions.
        u32 vboWidth = 160;
        u32 vboHeight = 144;
        if(scaleMode == 1) {
            vboWidth = 200;
            vboHeight = 180;
        } else if(scaleMode == 2) {
            vboWidth = 240;
            vboHeight = 216;
        } else if(scaleMode == 3) {
            vboWidth *= viewportHeight / (float) 144;
            vboHeight = (u32) viewportHeight;
        } else if(scaleMode == 4) {
            vboWidth = (u32) viewportWidth;
            vboHeight = (u32) viewportHeight;
        }

        // Calculate VBO points.
        const float x1 = (float) ((viewportWidth - vboWidth) / 2);
        const float y1 = (float) ((viewportHeight - vboHeight) / 2);
        const float x2 = x1 + vboWidth;
        const float y2 = y1 + vboHeight;

        // Adjust for power-of-two textures.
        static const float baseHorizMod = (256.0f - 160.0f) / 256.0f;
        static const float baseVertMod = (256.0f - 144.0f) / 256.0f;
        static const float filterMod = 0.25f / 256.0f;

        float horizMod = baseHorizMod;
        float vertMod = baseVertMod;
        if(scaleMode != 0 && scaleFilter == 1) {
            horizMod += filterMod;
            vertMod += filterMod;
        }

        // Prepare new VBO data.
        const float vboData[] = {
                x1, y1, -0.1f, 0.0f, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y1, -0.1f, 1.0f - horizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - horizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - horizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y2, -0.1f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y1, -0.1f, 0.0f, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f
        };

        // Update the VBO with the new data.
        gpu::setVboData(vbo, vboData, 6 * 9, gpu::PRIM_TRIANGLES);

        prevScaleMode = scaleMode;
        prevScaleFilter = scaleFilter;
        prevGameScreen = gameScreen;
    }

    // Update the texture with the new frame.
    gpu::TextureFilter filter = scaleFilter >= 1 ? gpu::FILTER_LINEAR : gpu::FILTER_NEAREST;
    if(scaleMode == 0 || scaleFilter <= 1) {
        gpu::setTextureData(texture, screenBuffer, 256, 256, gpu::PIXEL_RGBA8, TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));
    } else {
        gpu::setTextureInfo(texture, 512, 512, gpu::PIXEL_RGBA8, TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));
        gfxScale2x(screenBuffer);
    }

    // Clear the screen.
    gpu::clear();

    // Draw the border.
    if(borderTexture != 0 && borderVbo != 0 && scaleMode != 2) {
        gpu::bindTexture(gpu::TEXUNIT0, borderTexture);
        gpu::drawVbo(borderVbo);
    }

    // Draw the VBO.
    gpu::bindTexture(gpu::TEXUNIT0, texture);
    gpu::drawVbo(vbo);

    // Flush GPU commands.
    gpu::flushCommands();

    // Flush GPU framebuffer.
    gpu::flushBuffer();

    if(inputKeyPressed(FUNC_KEY_SCREENSHOT) && !isMenuOn()) {
        gput::takeScreenshot();
    }

    // Swap buffers and wait for VBlank.
    gpu::swapBuffers(!gfxGetFastForward());
}

void gfxWaitForVBlank() {
    gspWaitForVBlank();
}

#endif