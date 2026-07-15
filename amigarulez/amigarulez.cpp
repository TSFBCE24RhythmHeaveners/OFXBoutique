#include <immintrin.h> // For AVX2 SIMD
#include <cmath>
#include <algorithm>
#include <cstring>
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxParam.h"

// --- Plugin Identifiers ---
#define PLUGIN_ID "com.boutiqueofx.amigarulez"
#define PLUGIN_VERSION_MAJOR 1
#define PLUGIN_VERSION_MINOR 0
#define PLUGIN_NAME "BTQ_AmigaRulez"
#define PLUGIN_DESC "Amiga Rulez like offset & wrap effect."
#define PLUGIN_GROUP "Boutique Plugins"

// --- Parameter Names ---
#define PARAM_H_TILES "hTiles"
#define PARAM_V_TILES "vTiles"
#define PARAM_LINK    "linkTiles"
#define PARAM_ANCHOR  "anchor"
#define PARAM_OFFSET  "offset"

// Global host suites
static OfxImageEffectSuiteV1 *gEffectSuite = nullptr;
static OfxParameterSuiteV1   *gParamSuite  = nullptr;
static OfxPropertySuiteV1    *gPropSuite   = nullptr;

// Helper function to safely wrap textures (Repeat mode)
inline float wrapCoordinate(float coord) {
    float wrapped = std::fmod(coord, 1.0f);
    if (wrapped < 0.0f) wrapped += 1.0f;
    return wrapped;
}

// Core SIMD Processing Block (AVX2)
void processLineSIMD(float* dstPtr, const float* srcPtr, int width, int height, int y, 
                     float hTiles, float vTiles, float anchorX, float anchorY, float offsetX, float offsetY) {
    
    int x = 0;
    
    // AVX2 constants
    __m256 v_width    = _mm256_set1_ps(static_cast<float>(width));
    __m256 v_height   = _mm256_set1_ps(static_cast<float>(height));
    __m256 v_hTiles   = _mm256_set1_ps(hTiles);
    __m256 v_vTiles   = _mm256_set1_ps(vTiles);
    __m256 v_anchorX  = _mm256_set1_ps(anchorX);
    __m256 v_anchorY  = _mm256_set1_ps(anchorY);
    __m256 v_offsetX  = _mm256_set1_ps(offsetX);
    __m256 v_offsetY  = _mm256_set1_ps(offsetY);
    __m256 v_one      = _mm256_set1_ps(1.0f);
    __m256 v_zero     = _mm256_set1_ps(0.0f);
    
    // Process 4 pixels at a time (RGBA float = 4 floats per pixel, 4 pixels = 16 floats = 2 source AVX registers)
    for (; x <= width - 4; x += 4) {
        // Current X coordinates for 4 sequential pixels
        __m256 v_x = _mm256_set_ps(static_cast<float>(x+3), static_cast<float>(x+2), static_cast<float>(x+1), static_cast<float>(x+0));
        __m256 v_y = _mm256_set1_ps(static_cast<float>(y));
        
        // Normalize coordinates to range
        __m256 v_u = _mm256_div_ps(v_x, v_width);
        __m256 v_v = _mm256_div_ps(v_y, v_height);
        
        // Transform Math: U_new = ((U - AnchorX) * hTiles) + AnchorX + OffsetX
        __m256 v_u_new = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(_mm256_sub_ps(v_u, v_anchorX), v_hTiles), v_anchorX), v_offsetX);
        __m256 v_v_new = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(_mm256_sub_ps(v_v, v_anchorY), v_v_tiles), v_anchorY), v_offsetY);
        
        // Manual wrapping logic via AVX2 (simulate fmod and negative correction)
        __m256 v_u_div  = _mm256_div_ps(v_u_new, v_one);
        __m256 v_u_floor= _mm256_floor_ps(v_u_div);
        __m256 v_u_wrap = _mm256_sub_ps(v_u_new, _mm256_mul_ps(v_u_floor, v_one));
        __m256 mask_u   = _mm256_cmp_ps(v_u_wrap, v_zero, _CMP_LT_OQ);
        v_u_wrap        = _mm256_blendv_ps(v_u_wrap, _mm256_add_ps(v_u_wrap, v_one), mask_u);

        __m256 v_v_div  = _mm256_div_ps(v_v_new, v_one);
        __m256 v_v_floor= _mm256_floor_ps(v_v_div);
        __m256 v_v_wrap = _mm256_sub_ps(v_v_new, _mm256_mul_ps(v_v_floor, v_one));
        __m256 mask_v   = _mm256_cmp_ps(v_v_wrap, v_zero, _CMP_LT_OQ);
        v_v_wrap        = _mm256_blendv_ps(v_v_wrap, _mm256_add_ps(v_v_wrap, v_one), mask_v);
        
        // Denormalize back to pixel array indices
        __m256 v_srcX_f = _mm256_mul_ps(v_u_wrap, v_width);
        __m256 v_srcY_f = _mm256_mul_ps(v_v_wrap, v_height);
        
        // Convert floating indices to integers with truncation
        __m256i v_srcX = _mm256_cvttps_epi32(v_srcX_f);
        __m256i v_srcY = _mm256_cvttps_epi32(v_srcY_f);
        
        // Extract indices and sample pixels to the destination
        alignas(32) int srcX_arr;
        alignas(32) int srcY_arr;
        _mm256_store_si256((__m256i*)srcX_arr, v_srcX);
        _mm256_store_si256((__m256i*)srcY_arr, v_srcY);
        
        for (int i = 0; i < 4; ++i) {
            int clampedX = std::max(0, std::min(srcX_arr[i], width - 1));
            int clampedY = std::max(0, std::min(srcY_arr[i], height - 1));
            
            const float* srcPixel = srcPtr + (clampedY * width + clampedX) * 4;
            float* dstPixel = dstPtr + (y * width + (x + i)) * 4;
            
            std::memcpy(dstPixel, srcPixel, sizeof(float) * 4);
        }
    }
    
    // Fallback Loop for edge/remainder pixels (width % 4 != 0)
    for (; x < width; ++x) {
        float u = static_cast<float>(x) / static_cast<float>(width);
        float v = static_cast<float>(y) / static_cast<float>(height);
        
        float u_new = wrapCoordinate(((u - anchorX) * hTiles) + anchorX + offsetX);
        float v_new = wrapCoordinate(((v - anchorY) * vTiles) + anchorY + offsetY);
        
        int srcX = std::max(0, std::min(static_cast<int>(u_new * width), width - 1));
        int srcY = std::max(0, std::min(static_cast<int>(v_new * height), height - 1));
        
        std::memcpy(dstPtr + (y * width + x) * 4, srcPtr + (srcY * width + srcX) * 4, sizeof(float) * 4);
    }
}

// --- Main OFX Logic Functions ---

static OfxStatus Describe(OfxImageEffectHandle effect) {
    OfxPropertySetHandle effectProps;
    gEffectSuite->getPropertySet(effect, &effectProps);
    
    // Plugin Metadata setup
    gPropSuite->propSetString(effectProps, kOfxPropLabel, 0, PLUGIN_NAME);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPluginPropGrouping, 0, PLUGIN_GROUP);
    gPropSuite->propSetString(effectProps, kOfxPropPluginDescription, 0, PLUGIN_DESC);
    
    // Set rendering architectures
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    return kOfxStatOK;
}

static OfxStatus DescribeInContext(OfxImageEffectHandle effect) {
    // Define input clip (Source) and output clip (Output)
    OfxParamSetHandle paramSet;
    gEffectSuite->getParamSet(effect, &paramSet);
    
    // --- Define UI Parameters ---
    OfxPropertySetHandle paramProps;
    
    // Horizontal Tiles Slider
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_H_TILES, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Horizontal Tiles");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 20.00);
    
    // Vertical Tiles Slider
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_V_TILES, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Vertical Tiles");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 20.00);
    
    // Linked Tiles Checkbox
    gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, PARAM_LINK, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Linked Tiles");
    gPropSuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 1); // Checked by default
    
    // Anchor X & Y (2D Position Vector)
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble2D, PARAM_ANCHOR, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Anchor");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.50);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 1, 0.50);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 1, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 1, 1.00);
    
    // Offset X & Y (2D Position Vector)
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble2D, PARAM_OFFSET, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Offset");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 1, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, -1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 1, -1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 1, 1.00);

    return kOfxStatOK;
}

// Instance Changed Action (Handles disabling/enabling Vertical slider if Linked is checked)
static OfxStatus InstanceChanged(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    char* propName;
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &propName);
    
    if (std::strcmp(propName, PARAM_LINK) == 0 || std::strcmp(propName, PARAM_H_TILES) == 0) {
        OfxParamSetHandle paramSet;
        gEffectSuite->getParamSet(effect, &paramSet);
        
        OfxParamHandle hLink, hHTiles, hVTiles;
        gParamSuite->paramGetHandle(paramSet, PARAM_LINK, &hLink, nullptr);
        gParamSuite->paramGetHandle(paramSet, PARAM_H_TILES, &hHTiles, nullptr);
        gParamSuite->paramGetHandle(paramSet, PARAM_V_TILES, &hVTiles, nullptr);
        
        int linked;
        double hTilesVal;
        gParamSuite->paramGetValue(hLink, &linked);
        gParamSuite->paramGetValue(hHTiles, &hTilesVal);
        
        OfxPropertySetHandle vTilesProps;
        gParamSuite->paramGetPropertySet(hVTiles, &vTilesProps);
        
        if (linked) {
            // Push horizontal parameter value to vertical parameter value and disable the UI element
            gParamSuite->paramSetValue(hVTiles, hTilesVal);
            gPropSuite->propSetInt(vTilesProps, kOfxParamPropEnabled, 0, 0);
        } else {
            gPropSuite->propSetInt(vTilesProps, kOfxParamPropEnabled, 0, 1);
        }
    }
    return kOfxStatOK;
}

// --- Render Logic ---
static OfxStatus Render(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    OfxTime time;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    
    OfxParamSetHandle paramSet;
    gEffectSuite->getParamSet(effect, &paramSet);
    
    // Retrieve Parameter Values
    OfxParamHandle hHTiles, hVTiles, hAnchor, hOffset;
    gParamSuite->paramGetHandle(paramSet, PARAM_H_TILES, &hHTiles, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_V_TILES, &hVTiles, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_ANCHOR, &hAnchor, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_OFFSET, &hOffset, nullptr);
    
    double hTiles, vTiles, anchorX, anchorY, offsetX, offsetY;
    gParamSuite->paramGetValueAtTime(hHTiles, time, &hTiles);
    gParamSuite->paramGetValueAtTime(hVTiles, time, &vTiles);
    gParamSuite->paramGetValueAtTime(hAnchor, time, &anchorX, &anchorY);
    gParamSuite->paramGetValueAtTime(hOffset, time, &offsetX, &offsetY);
    
    // Obtain Image Handles
    OfxImageClipHandle srcClip, dstClip;
    gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &srcClip, nullptr);
    gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleOutputClipName, &dstClip, nullptr);
    
    OfxPropertySetHandle srcImg, dstImg;
    gEffectSuite->clipGetImage(srcClip, time, nullptr, &srcImg);
    gEffectSuite->clipGetImage(dstClip, time, nullptr, &dstImg);
    
    // Fetch data pointers and frame bounds
    float* srcPtr = nullptr;
    float* dstPtr = nullptr;
    gPropSuite->propGetPointer(srcImg, kOfxImageEffectPropDataPtr, 0, (void**)&srcPtr);
    gPropSuite->propGetPointer(dstImg, kOfxImageEffectPropDataPtr, 0, (void**)&dstPtr);
    
    int bounds;
    gPropSuite->propGetIntN(dstImg, kOfxImageEffectPropBounds, 4, bounds);
    
    int width = bounds - bounds;
    int height = bounds - bounds;
    
    // Render loop processing scanlines in parallel
    #pragma omp parallel for
    for (int y = 0; y < height; ++y) {
        processLineSIMD(dstPtr, srcPtr, width, height, y, 
                         static_cast<float>(hTiles), static_cast<float>(vTiles), 
                         static_cast<float>(anchorX), static_cast<float>(anchorY), 
                         static_cast<float>(offsetX), static_cast<float>(offsetY));
    }
    
    gEffectSuite->clipReleaseImage(srcImg);
    gEffectSuite->clipReleaseImage(dstImg);
    
    return kOfxStatOK;
}

// --- Main OFX Main Entry Point Function ---
static OfxStatus MainEntryPoint(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    if (std::strcmp(action, kOfxActionLoad) == 0) return kOfxStatOK;
    
    if (std::strcmp(action, kOfxImageEffectActionDescribe) == 0) {
        return Describe((OfxImageEffectHandle)handle);
    }
    if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
        return DescribeInContext((OfxImageEffectHandle)handle);
    }
    if (std::strcmp(action, kOfxImageEffectActionInstanceChanged) == 0) {
        return InstanceChanged((OfxImageEffectHandle)handle, inArgs);
    }
    if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
        return Render((OfxImageEffectHandle)handle, inArgs);
    }
    
    return kOfxStatReplyDefault;
}

// --- Registry Struct Needed By OFX Hosts ---
static OfxPlugin pluginStruct = {
    kOfxImageEffectPluginApi,
    1,
    PLUGIN_ID,
    PLUGIN_VERSION_MAJOR,
    PLUGIN_VERSION_MINOR,
    MainEntryPoint,
    nullptr
};

extern "C" OfxPlugin* OfxGetPlugin(int nth) {
    if (nth == 0) return &pluginStruct;
    return nullptr;
}

extern "C" int OfxGetNumberOfPlugins(void) { return 1; }

// --- Hook Host Suites on Startup ---
extern "C" void OfxSetHost(OfxHost *host) {
    gEffectSuite = (OfxImageEffectSuiteV1*)host->fetchSuite(host->host, kOfxImageEffectSuite, 1);
    gParamSuite  = (OfxParameterSuiteV1*)host->fetchSuite(host->host, kOfxParameterSuite, 1);
    gPropSuite   = (OfxPropertySuiteV1*)host->fetchSuite(host->host, kOfxPropertySuite, 1);
}
