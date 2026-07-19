#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxProperty.h"
#include "ofxParam.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// Math configurations
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Parameter Constant String Definition Blocks
#define PARAM_PEAKS "peaks"
#define PARAM_AMPLITUDE "amplitude"
#define PARAM_SPEED "speed"
#define PARAM_OFFSET "offset"
#define PARAM_ORIENTATION "orientation"
#define PARAM_EDGES "edges"

enum EdgeType {
    EDGE_NONE = 0,
    EDGE_TILE = 1,
    EDGE_REFLECT = 2,
    EDGE_CLAMP = 3
};

// Global Pointers for standard OFX Core Functional Suites
static OfxImageEffectSuiteV1 *effectSuite = nullptr;
static OfxPropertySuiteV1    *propSuite   = nullptr;
static OfxParamSuiteV1       *paramSuite  = nullptr;

// Inline boundary management for SIMD pipeline consistency
inline int HandleEdgeSIMD(int val, int maxVal, int edgeType) {
    if (val >= 0 && val < maxVal) return val;
    if (edgeType == EDGE_CLAMP) return (val < 0) ? 0 : (maxVal - 1);
    if (edgeType == EDGE_TILE) return (val % maxVal + maxVal) % maxVal;
    if (edgeType == EDGE_REFLECT) {
        int doubleMax = maxVal * 2;
        int mod = (val % doubleMax + doubleMax) % doubleMax;
        return (mod >= maxVal) ? doubleMax - 1 - mod : mod;
    }
    return -1; // EDGE_NONE
}

// Global Plugin Core Render Routine
static OfxStatus Render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    OfxTime time;
    propSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    // Dynamic Clip Resolution
    OfxImageClipHandle srcClip, dstClip;
    effectSuite->clipGetHandle(instance, kOfxImageEffectSourceClipName, &srcClip, nullptr);
    effectSuite->clipGetHandle(instance, kOfxImageEffectOutputClipName, &dstClip, nullptr);

    OfxPropertySetHandle srcImg, dstImg;
    effectSuite->clipGetImage(srcClip, time, nullptr, &srcImg);
    effectSuite->clipGetImage(dstClip, time, nullptr, &dstImg);

    // Memory Alignment Extraction
    float *srcBuf = nullptr, *dstBuf = nullptr;
    int srcRowBytes, dstRowBytes;
    OfxRectI renderWindow;

    propSuite->propGetPointer(srcImg, kOfxImagePropData, 0, (void**)&srcBuf);
    propSuite->propGetPointer(dstImg, kOfxImagePropData, 0, (void**)&dstBuf);
    propSuite->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    propSuite->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    propSuite->propGetIntN(outArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    // Param Target Value Parsing
    OfxParamSetHandle paramSet;
    paramSuite->paramGetSet(instance, &paramSet);
    OfxParamHandle pPeaks, pAmp, pSpeed, pOffset, pOrient, pEdges;
    paramSuite->paramGetHandle(paramSet, PARAM_PEAKS, &pPeaks, nullptr);
    paramSuite->paramGetHandle(paramSet, PARAM_AMPLITUDE, &pAmp, nullptr);
    paramSuite->paramGetHandle(paramSet, PARAM_SPEED, &pSpeed, nullptr);
    paramSuite->paramGetHandle(paramSet, PARAM_OFFSET, &pOffset, nullptr);
    paramSuite->paramGetHandle(paramSet, PARAM_ORIENTATION, &pOrient, nullptr);
    paramSuite->paramGetHandle(paramSet, PARAM_EDGES, &pEdges, nullptr);

    double peaks, amp, speed, offset, orient;
    int edgeType;
    paramSuite->paramGetValueAtTime(pPeaks, time, &peaks);
    paramSuite->paramGetValueAtTime(pAmp, time, &amp);
    paramSuite->paramGetValueAtTime(pSpeed, time, &speed);
    paramSuite->paramGetValueAtTime(pOffset, time, &offset);
    paramSuite->paramGetValueAtTime(pOrient, time, &orient);
    paramSuite->paramGetValueAtTime(pEdges, time, &edgeType);

    // Math Matrix Coefficient Assembly
    double radOrient = orient * (M_PI / 180.0);
    double radOffset = offset * (M_PI / 180.0);
    float waveX = (float)cos(radOrient);
    float waveY = (float)sin(radOrient);
    float timePhase = (float)(time * (speed * 0.05) + radOffset);
    
    int width = renderWindow.x2 - renderWindow.x1;
    int height = renderWindow.y2 - renderWindow.y1;
    float invHeight = (height > 0) ? 1.0f / height : 1.0f;
    float spatialScale = (float)(peaks * 2.0 * M_PI * invHeight);
    float fAmp = (float)amp;

    int srcRowFloats = srcRowBytes / sizeof(float);
    int dstRowFloats = dstRowBytes / sizeof(float);

    // High Density SIMD Processing Core Layout Loop
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        float *dstRow = dstBuf + (y - renderWindow.y1) * dstRowFloats;
        
        #pragma omp simd
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            float spacePhase = (x * waveX + y * waveY) * spatialScale;
            float displacement = sinf(spacePhase + timePhase) * fAmp;

            // Direct perpendicular projection relative to Orientation angles
            int srcX = (int)roundf(x - displacement * waveY);
            int srcY = (int)roundf(y + displacement * waveX);

            int mapX = HandleEdgeSIMD(srcX, width, edgeType);
            int mapY = HandleEdgeSIMD(srcY, height, edgeType);

            int dstIdx = (x - renderWindow.x1) * 4;

            if (mapX != -1 && mapY != -1) {
                int srcIdx = mapY * srcRowFloats + mapX * 4;
                // Linear cache layout allows automatic AVX vector packing registers
                dstRow[dstIdx + 0] = srcBuf[srcIdx + 0]; // R
                dstRow[dstIdx + 1] = srcBuf[srcIdx + 1]; // G
                dstRow[dstIdx + 2] = srcBuf[srcIdx + 2]; // B
                dstRow[dstIdx + 3] = srcBuf[srcIdx + 3]; // A
            } else {
                // Clear state for EDGE_NONE boundaries
                dstRow[dstIdx + 0] = 0.0f;
                dstRow[dstIdx + 1] = 0.0f;
                dstRow[dstIdx + 2] = 0.0f;
                dstRow[dstIdx + 3] = 0.0f;
            }
        }
    }

    effectSuite->clipReleaseImage(srcImg);
    effectSuite->clipReleaseImage(dstImg);
    return kOfxStatOK;
}

// Host Description, Labeling, Names & Group Initialization Blocks
static OfxStatus Describe(OfxImageEffectHandle effect) {
    OfxPropertySetHandle effectProps;
    effectSuite->getProperties(effect, &effectProps);

    // Custom Group Setup
    propSuite->propSetString(effectProps, kOfxPropLabel, 0, "BoutiqueFX Wave Warp");
    propSuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, "Boutique Plugins");
    propSuite->propSetString(effectProps, kOfxPropPluginDescription, 0, "Warps an image in a sine wave.");

    // Define Input Buffers
    OfxPropertySetHandle srcProps, dstProps;
    effectSuite->clipDefine(effect, kOfxImageEffectSourceClipName, &srcProps);
    effectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &dstProps);
    propSuite->propSetString(srcProps, kOfxImageEffectClipPropSupportedComponents, 0, kOfxImageComponentRGBA);
    propSuite->propSetString(dstProps, kOfxImageEffectClipPropSupportedComponents, 0, kOfxImageComponentRGBA);

    // Parameter Assembly Pipeline
    OfxParamSetHandle paramSet;
    paramSuite->paramGetSet(effect, &paramSet);
    OfxPropertySetHandle p;

    // 1. Peaks
    paramSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_PEAKS, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Peaks");
    propSuite->propSetDouble(p, kOfxParamPropDefault, 0, 5.00);
    propSuite->propSetDouble(p, kOfxParamPropMin, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMax, 0, 20.00);

    // 2. Amplitude
    paramSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_AMPLITUDE, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Amplitude");
    propSuite->propSetDouble(p, kOfxParamPropDefault, 0, 20.00);
    propSuite->propSetDouble(p, kOfxParamPropMin, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMax, 0, 100.00);

    // 3. Speed
    paramSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_SPEED, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Speed");
    propSuite->propSetDouble(p, kOfxParamPropDefault, 0, 20.00);
    propSuite->propSetDouble(p, kOfxParamPropMin, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMax, 0, 100.00);

    // 4. Offset
    paramSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_OFFSET, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Offset");
    propSuite->propSetDouble(p, kOfxParamPropDefault, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMin, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMax, 0, 360.00);

    // 5. Orientation
    paramSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_ORIENTATION, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Orientation");
    propSuite->propSetDouble(p, kOfxParamPropDefault, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMin, 0, 0.00);
    propSuite->propSetDouble(p, kOfxParamPropMax, 0, 360.00);

    // 6. Edges
    paramSuite->paramDefine(paramSet, kOfxParamTypeChoice, PARAM_EDGES, &p);
    propSuite->propSetString(p, kOfxPropLabel, 0, "Edges");
    propSuite->propSetString(p, kOfxParamPropChoiceOption, 0, "None");
    propSuite->propSetString(p, kOfxParamPropChoiceOption, 1, "Tile");
    propSuite->propSetString(p, kOfxParamPropChoiceOption, 2, "Reflect");
    propSuite->propSetString(p, kOfxParamPropChoiceOption, 3, "Clamp");
    propSuite->propSetInt(p, kOfxParamPropDefault, 0, 3);

    return kOfxStatOK;
}

// Master Action Handler Router
static OfxStatus MainEntry(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    OfxImageEffectHandle effect = (OfxImageEffectHandle)handle;
    if (strcmp(action, kOfxImageEffectActionDescribe) == 0) {
        return Describe(effect);
    } else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
        return Render(effect, inArgs, outArgs);
    }
    return kOfxStatReplyDefault;
}

// Core External DLL/Shared Object Entry Point hooks
extern "C" {
    EXPORT OfxStatus OfxGetNumberOfPlugins(void) { return 1; }
    EXPORT OfxStatus OfxGetPlugin(int nth, OfxPluginStruct *ptr) {
        if (nth != 0) return kOfxStatErrBadIndex;
        ptr->pluginApi         = kOfxImageEffectPluginApi;
        ptr->apiVersion        = 1;
        ptr->pluginVersionMajor = 1;
        ptr->pluginVersionMinor = 0;
        ptr->pluginIdentifier   = "com.boutiquefx.wavewarp";
        ptr->setHost           = [](OfxHost *host) {
            effectSuite = (OfxImageEffectSuiteV1*)host->fetchSuite(host->host, kOfxImageEffectSuite, 1);
            propSuite   = (OfxPropertySuiteV1*)host->fetchSuite(host->host, kOfxPropertySuite, 1);
            paramSuite  = (OfxParamSuiteV1*)host->fetchSuite(host->host, kOfxParamSuite, 1);
        };
        ptr->mainEntry         = MainEntry;
        return kOfxStatOK;
    }
}
