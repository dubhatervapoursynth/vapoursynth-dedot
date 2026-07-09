#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <VapourSynth4.h>
#include <VSHelper4.h>


#ifdef _MSC_VER
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif


static FORCE_INLINE int process_chroma_pixel_scalar(
        int pixel_PP,
        int pixel_P,
        int pixel_C,
        int pixel_N,
        int pixel_NN,
        int chroma_t1,
        int chroma_t2) {

    bool lteq_t1 =
            std::abs(pixel_P - pixel_N) <= chroma_t1 &&
            std::abs(pixel_C - pixel_PP) <= chroma_t1 &&
            std::abs(pixel_C - pixel_NN) <= chroma_t1;

    int abs_diff_CP = std::abs(pixel_C - pixel_P);
    int abs_diff_CN = std::abs(pixel_C - pixel_N);

    bool lteq_t1_gt_t2 = lteq_t1 &&
            abs_diff_CP > chroma_t2 &&
            abs_diff_CN > chroma_t2;

    int avg_pc = (pixel_P + pixel_C + 1) >> 1;
    int avg_nc = (pixel_N + pixel_C + 1) >> 1;

    int avg_nc_or_pc = abs_diff_CN <= abs_diff_CP ? avg_nc : avg_pc;

    return lteq_t1_gt_t2 ? avg_nc_or_pc : pixel_C;
}


static FORCE_INLINE int process_luma_pixel_scalar(
        int pixel_current_left,
        int pixel_current,
        int pixel_current_right,
        int pixel_current_2above,
        int pixel_current_2below,
        int pixel_2previous,
        int pixel_previous,
        int pixel_next,
        int pixel_2next,
        int luma_2d,
        int luma_t) {

    int left_right = pixel_current_left + pixel_current_right;
    int above_below = pixel_current_2above + pixel_current_2below;
    int center_center = pixel_current * 2;

    int abs_diff_horizontal = std::abs(left_right - center_center);
    int abs_diff_vertical = std::abs(above_below - center_center);

    int result = pixel_current;

    if (abs_diff_horizontal > luma_2d || abs_diff_vertical > luma_2d) {
        bool temporal_okay =
                std::abs(pixel_previous - pixel_next) <= luma_t &&
                std::abs(pixel_current - pixel_2previous) <= luma_t &&
                std::abs(pixel_current - pixel_2next) <= luma_t;

        if (temporal_okay) {
            int avg_pc = (pixel_previous + pixel_current + 1) >> 1;
            int avg_nc = (pixel_next + pixel_current + 1) >> 1;

            int abs_diff_pc = std::abs(pixel_previous - pixel_current);
            int abs_diff_nc = std::abs(pixel_next - pixel_current);

            int avg_nc_or_avg_pc = abs_diff_nc <= abs_diff_pc ? avg_nc : avg_pc;

            result = avg_nc_or_avg_pc;
        }
    }

    return result;
}


template <typename PixelType>
static void process_chroma_plane(
        const uint8_t *pPP8,
        const uint8_t *pP8,
        const uint8_t *pC8,
        const uint8_t *pN8,
        const uint8_t *pNN8,
        uint8_t *pD8,
        const int width_U,
        const int height_U,
        const ptrdiff_t stride_bytes,
        const int chroma_t1,
        const int chroma_t2) {

    const PixelType *pPP = reinterpret_cast<const PixelType *>(pPP8);
    const PixelType *pP  = reinterpret_cast<const PixelType *>(pP8);
    const PixelType *pC  = reinterpret_cast<const PixelType *>(pC8);
    const PixelType *pN  = reinterpret_cast<const PixelType *>(pN8);
    const PixelType *pNN = reinterpret_cast<const PixelType *>(pNN8);
    PixelType *pD = reinterpret_cast<PixelType *>(pD8);

    const ptrdiff_t stride = stride_bytes / static_cast<ptrdiff_t>(sizeof(PixelType));

    for (int y = 0; y < height_U; y++) {
        for (int x = 0; x < width_U; x++) {
            pD[x] = static_cast<PixelType>(process_chroma_pixel_scalar(
                    pPP[x], pP[x], pC[x], pN[x], pNN[x], chroma_t1, chroma_t2));
        }

        pPP += stride;
        pP += stride;
        pC += stride;
        pN += stride;
        pNN += stride;
        pD += stride;
    }
}


template <typename PixelType>
static void process_luma_plane(
        const uint8_t *pPP8,
        const uint8_t *pP8,
        const uint8_t *pC8,
        const uint8_t *pN8,
        const uint8_t *pNN8,
        uint8_t *pD8,
        const int width,
        const int height,
        const ptrdiff_t stride_bytes,
        const int luma_2d,
        const int luma_t) {

    const PixelType *pPP = reinterpret_cast<const PixelType *>(pPP8);
    const PixelType *pP  = reinterpret_cast<const PixelType *>(pP8);
    const PixelType *pC  = reinterpret_cast<const PixelType *>(pC8);
    const PixelType *pN  = reinterpret_cast<const PixelType *>(pN8);
    const PixelType *pNN = reinterpret_cast<const PixelType *>(pNN8);
    PixelType *pD = reinterpret_cast<PixelType *>(pD8);

    const ptrdiff_t stride = stride_bytes / static_cast<ptrdiff_t>(sizeof(PixelType));

    for (int y = 0; y < 2; y++) {
        memcpy(pD, pC, width * sizeof(PixelType));

        pD += stride;
        pC += stride;
    }

    for (int y = 2; y < height - 2; y++) {
        pD[0] = pC[0];

        for (int x = 1; x < width - 1; x++) {
            int pixel_current_left  = pC[x - 1];
            int pixel_current =       pC[x];
            int pixel_current_right = pC[x + 1];

            int pixel_current_2above = pC[x - stride * 2];
            int pixel_current_2below = pC[x + stride * 2];

            int pixel_previous =  pP[x];
            int pixel_next =      pN[x];
            int pixel_2previous = pPP[x];
            int pixel_2next =     pNN[x];

            pD[x] = static_cast<PixelType>(process_luma_pixel_scalar(
                    pixel_current_left, pixel_current, pixel_current_right,
                    pixel_current_2above, pixel_current_2below,
                    pixel_2previous, pixel_previous, pixel_next, pixel_2next,
                    luma_2d, luma_t));
        }

        pD[width - 1] = pC[width - 1];

        pPP += stride;
        pP += stride;
        pC += stride;
        pN += stride;
        pNN += stride;
        pD += stride;
    }

    for (int y = height - 2; y < height; y++) {
        memcpy(pD, pC, width * sizeof(PixelType));

        pD += stride;
        pC += stride;
    }
}


typedef struct DedotData {
    VSNode *clip;
    const VSVideoInfo *vi;

    int process[3];

    int chroma_t1;
    int chroma_t2;
    int luma_2d;
    int luma_t;

} DedotData;

static const VSFrame *VS_CC dedotGetFrame(int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    (void)frameData;

    const DedotData *d = (const DedotData *) instanceData;

    if (activationReason == arInitial) {
        for (int i = std::max(0, n - 2); i <= std::min(n + 2, d->vi->numFrames - 1); i++)
            vsapi->requestFrameFilter(i, d->clip, frameCtx);
    } else if (activationReason == arAllFramesReady) {
        const VSFrame *srcPP = vsapi->getFrameFilter(std::max(0, n - 2), d->clip, frameCtx);
        const VSFrame *srcP = vsapi->getFrameFilter(std::max(0, n - 1), d->clip, frameCtx);
        const VSFrame *srcC = vsapi->getFrameFilter(n, d->clip, frameCtx);
        const VSFrame *srcN = vsapi->getFrameFilter(std::min(n + 1, d->vi->numFrames - 1), d->clip, frameCtx);
        const VSFrame *srcNN = vsapi->getFrameFilter(std::min(n + 2, d->vi->numFrames - 1), d->clip, frameCtx);

        const VSFrame *plane_src[3] = {
            d->process[0] ? nullptr : srcC,
            d->process[1] ? nullptr : srcC,
            d->process[2] ? nullptr : srcC
        };

        int planes[3] = { 0, 1, 2 };

        VSFrame *dst = vsapi->newVideoFrame2(&d->vi->format, d->vi->width, d->vi->height, plane_src, planes, srcC, core);

        const int bytesPerSample = d->vi->format.bytesPerSample;

        if (d->vi->format.colorFamily != cfGray && d->process[1] && d->process[2]) {
            for (int plane = 1; plane < 3; plane++) {
                const uint8_t *pPP = vsapi->getReadPtr(srcPP, plane);
                const uint8_t *pP = vsapi->getReadPtr(srcP, plane);
                const uint8_t *pC = vsapi->getReadPtr(srcC, plane);
                const uint8_t *pN = vsapi->getReadPtr(srcN, plane);
                const uint8_t *pNN = vsapi->getReadPtr(srcNN, plane);
                uint8_t *pD = vsapi->getWritePtr(dst, plane);

                int width = vsapi->getFrameWidth(srcC, plane);
                int height = vsapi->getFrameHeight(srcC, plane);
                ptrdiff_t stride = vsapi->getStride(srcC, plane);

                if (bytesPerSample == 1)
                    process_chroma_plane<uint8_t>(
                                pPP, pP, pC, pN, pNN, pD,
                                width, height, stride,
                                d->chroma_t1, d->chroma_t2);
                else
                    process_chroma_plane<uint16_t>(
                                pPP, pP, pC, pN, pNN, pD,
                                width, height, stride,
                                d->chroma_t1, d->chroma_t2);
            }
        }

        if (d->process[0]) {
            int width = vsapi->getFrameWidth(srcC, 0);
            int height = vsapi->getFrameHeight(srcC, 0);
            ptrdiff_t stride = vsapi->getStride(srcC, 0);

            const uint8_t *pPP = vsapi->getReadPtr(srcPP, 0) + 2 * stride;
            const uint8_t *pP = vsapi->getReadPtr(srcP, 0) + 2 * stride;
            const uint8_t *pC = vsapi->getReadPtr(srcC, 0);
            const uint8_t *pN = vsapi->getReadPtr(srcN, 0) + 2 * stride;
            const uint8_t *pNN = vsapi->getReadPtr(srcNN, 0) + 2 * stride;
            uint8_t *pD = vsapi->getWritePtr(dst, 0);

            if (bytesPerSample == 1)
                process_luma_plane<uint8_t>(
                            pPP, pP, pC, pN, pNN, pD,
                            width, height, stride,
                            d->luma_2d, d->luma_t);
            else
                process_luma_plane<uint16_t>(
                            pPP, pP, pC, pN, pNN, pD,
                            width, height, stride,
                            d->luma_2d, d->luma_t);
        }

        vsapi->freeFrame(srcPP);
        vsapi->freeFrame(srcP);
        vsapi->freeFrame(srcC);
        vsapi->freeFrame(srcN);
        vsapi->freeFrame(srcNN);

        return dst;
    }

    return NULL;
}


static void VS_CC dedotFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    (void)core;

    DedotData *d = (DedotData *)instanceData;

    vsapi->freeNode(d->clip);
    free(d);
}


static void VS_CC dedotCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    (void)userData;

    DedotData d;
    memset(&d, 0, sizeof(d));

    int err;

    d.luma_2d = vsapi->mapGetIntSaturated(in, "luma_2d", 0, &err);
    if (err)
        d.luma_2d = 20;

    d.luma_t = vsapi->mapGetIntSaturated(in, "luma_t", 0, &err);
    if (err)
        d.luma_t = 20;

    d.chroma_t1 = vsapi->mapGetIntSaturated(in, "chroma_t1", 0, &err);
    if (err)
        d.chroma_t1 = 15;

    d.chroma_t2 = vsapi->mapGetIntSaturated(in, "chroma_t2", 0, &err);
    if (err)
        d.chroma_t2 = 5;


    if (d.luma_2d < 0 || d.luma_2d > 510) {
        vsapi->mapSetError(out, "Dedot: luma_2d must be between 0 and 510 (inclusive).");
        return;
    }

    if (d.luma_t < 0 || d.luma_t > 255) {
        vsapi->mapSetError(out, "Dedot: luma_t must be between 0 and 255 (inclusive).");
        return;
    }

    if (d.chroma_t1 < 0 || d.chroma_t1 > 255) {
        vsapi->mapSetError(out, "Dedot: chroma_t1 must be between 0 and 255 (inclusive).");
        return;
    }

    if (d.chroma_t2 < 0 || d.chroma_t2 > 255) {
        vsapi->mapSetError(out, "Dedot: chroma_t2 must be between 0 and 255 (inclusive).");
        return;
    }

    if ((d.luma_2d == 510 || d.luma_t == 0) && d.chroma_t2 == 255) {
        vsapi->mapSetError(out, "Dedot: chroma_t2 can't be 255 when luma_2d is 510 or when luma_t is 0 because then all the planes would be returned unchanged.");
        return;
    }


    d.clip = vsapi->mapGetNode(in, "clip", 0, NULL);
    d.vi = vsapi->getVideoInfo(d.clip);


    if ((d.vi->format.colorFamily != cfGray && d.vi->format.colorFamily != cfYUV) ||
        d.vi->format.sampleType != stInteger ||
        d.vi->format.bitsPerSample < 8 ||
        d.vi->format.bitsPerSample > 16 ||
        d.vi->width == 0 ||
        d.vi->height == 0) {
        vsapi->mapSetError(out, "Dedot: the input clip must be 8..16 bit integer YUV or Gray with constant format and dimensions.");
        vsapi->freeNode(d.clip);
        return;
    }


    d.process[0] = d.luma_2d < 510 && d.luma_t > 0;
    d.process[1] = d.process[2] = d.chroma_t2 < 255 && d.vi->format.colorFamily != cfGray;

    const int shift = d.vi->format.bitsPerSample - 8;
    d.luma_2d <<= shift;
    d.luma_t <<= shift;
    d.chroma_t1 <<= shift;
    d.chroma_t2 <<= shift;

    DedotData *data = (DedotData *)malloc(sizeof(d));
    *data = d;

    VSFilterDependency deps[1] = { {data->clip, rpGeneral} };

    vsapi->createVideoFilter(out, "Dedot", data->vi, dedotGetFrame, dedotFree, fmParallel, deps, 1, data, core);
}


VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) {
    vspapi->configPlugin("com.nodame.dedot", "dedot", "Temporal dotcrawl and rainbow remover", VS_MAKE_VERSION(3, 0), VAPOURSYNTH_API_VERSION,  0, plugin);
    vspapi->registerFunction("Dedot",
            "clip:vnode;"
            "luma_2d:int:opt;"
            "luma_t:int:opt;"
            "chroma_t1:int:opt;"
            "chroma_t2:int:opt;",
            "clip:vnode;"
            , dedotCreate, 0, plugin);
}
