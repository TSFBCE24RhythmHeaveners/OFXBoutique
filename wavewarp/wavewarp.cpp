#include <cmath>
#include <cstring>
#include <algorithm>
#include <immintrin.h> // Header for AVX2 SIMD Intrinsics
#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxPixels.h"

// --- Global Suite Pointers ---
static OfxHost*               gHost;
static OfxImageEffectSuiteV1* gEffectSuite;
static OfxPropertySuiteV1*    gPropSuite;
static OfxParameterSuiteV1*   gParamSuite;

// --- Constant Definitions ---
#define PARAM_PEAKS       "waveWarpPeaks"
#define PARAM_AMPLITUDE   "waveWarpAmplitude"
#define PARAM_SPEED       "waveWarpSpeed"
#define PARAM_OFFSET      "waveWarpOffset"
#define PARAM_ORIENTATION "waveWarpOrientation"
#define PARAM_ADAPT_ASPECT "waveWarpAdaptAspect" // New boolean property key

constexpr float DEG_TO_RAD = 0.0174532925f;

// --- Forward Declarations ---
OfxStatus pluginMainEntry(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);

// --- OFX Plugin Bootstrap Hooks ---
extern "C" {
    EXPORT OfxStatus OfxGetNumberOfPlugins(void) { return 1; }
    EXPORT OfxPlugin* OfxGetPlugin(int nth) {
        static OfxPlugin effectPlugin;
        if (nth == 0) {
            effectPlugin.pluginApi         = kOfxImageEffectApi;
            effectPlugin.apiVersion        = 1;
            effectPlugin.pluginVersionMajor = 1;
            effectPlugin.pluginVersionMinor = 0;
            effectPlugin.pluginIdentifier  = "com.boutiquefx.wavewarp";
            effectPlugin.setHost           = [](OfxHost* host) { 
                gHost = host; 
                gEffectSuite = (OfxImageEffectSuiteV1*)host->fetchSuite(host->host, kOfxImageEffectSuite, 1);
                gPropSuite   = (OfxPropertySuiteV1*)host->fetchSuite(host->host, kOfxPropertySuite, 1);
                gParamSuite  = (OfxParameterSuiteV1*)host->fetchSuite(host->host, kOfxParameterSuite, 1);
            };
            effectPlugin.mainEntry         = pluginMainEntry;
            return &effectPlugin;
        }
        return nullptr;
    }
}

// --- Parameter & Context Description (Describe Action) ---
OfxStatus DescribePlugin(OfxImageEffectHandle descriptor) {
    OfxPropertySetHandle effectProps;
    gEffectSuite->getPropertySet(descriptor, &effectProps);

    // Setup visual identity and plugin hierarchy
    gPropSuite->propSetString(effectProps, kOfxPropLabel, 0, "BoutiqueFX Wave Warp");
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropPluginDescription, 0, "Distorts the visuals in a sine wave.");
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropPluginGrouping, 0, "Boutique Plugins");
    
    // Define universal rendering behavior capabilities
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat); // 32-bit Float RGBA

    // Declare input video source slots
    OfxPropertySetHandle sourceProps;
    gEffectSuite->clipDefine(descriptor, "Source", &sourceProps);
    gPropSuite->propSetString(sourceProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);

    // Declare target output clip destination slots
    OfxPropertySetHandle outputProps;
    gEffectSuite->clipDefine(descriptor, "Output", &outputProps);
    gPropSuite->propSetString(outputProps, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);

    // Create parameter definitions
    OfxPropertySetHandle paramProps;
    
    // Peaks Parameter Definition
    gParamSuite->paramDefine(descriptor, kOfxParamTypeDouble, PARAM_PEAKS, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Peaks");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 5.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 20.00);

    // Amplitude Parameter Definition
    gParamSuite->paramDefine(descriptor, kOfxParamTypeDouble, PARAM_AMPLITUDE, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Amplitude");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 20.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 100.00);

    // Speed Parameter Definition
    gParamSuite->paramDefine(descriptor, kOfxParamTypeDouble, PARAM_SPEED, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Speed");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 20.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 100.00);

    // Offset Parameter Definition
    gParamSuite->paramDefine(descriptor, kOfxParamTypeDouble, PARAM_OFFSET, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Offset");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 360.00);

    // Orientation Parameter Definition
    gParamSuite->paramDefine(descriptor, kOfxParamTypeDouble, PARAM_ORIENTATION, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Orientation");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 360.00);

    // Adapt Amplitude to Aspect Ratio Checkbox Definition (Default: Enabled)
    gParamSuite->paramDefine(descriptor, kOfxParamTypeBoolean, PARAM_ADAPT_ASPECT, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Adapt Amplitude to Aspect Ratio");
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "When enabled, balances wave intensity uniformly across vertical and widescreen targets.");
    gPropSuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 1); // 1 = True / On

    return kOfxStatOK;
}

// --- Image Processing Pipeline Execution Logic (Render Action) ---
OfxStatus RenderEffect(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    double time = 0.0;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

    // Fetch active clip handles
    OfxImageClipHandle sourceClip, outputClip;
    gEffectSuite->clipGetHandle(instance, "Source", &sourceClip);
    gEffectSuite->clipGetHandle(instance, "Output", &outputClip);

    // Fetch image data frames for the given timeline position
    OfxPropertySetHandle sourceImg, outputImg;
    gEffectSuite->clipGetImage(sourceClip, time, nullptr, &sourceImg);
    gEffectSuite->clipGetImage(outputClip, time, nullptr, &outputImg);

    // Extract runtime variables and spatial parameters
    double pPeaks, pAmp, pSpeed, pOffset, pOrient;
    int pAdaptAspect = 1;
    
    OfxParamHandle hPeaks, hAmp, hSpeed, hOffset, hOrient, hAdaptAspect;
    gParamSuite->paramGetHandle(instance, PARAM_PEAKS, &hPeaks);
    gParamSuite->paramGetHandle(instance, PARAM_AMPLITUDE, &hAmp);
    gParamSuite->paramGetHandle(instance, PARAM_SPEED, &hSpeed);
    gParamSuite->paramGetHandle(instance, PARAM_OFFSET, &hOffset);
    gParamSuite->paramGetHandle(instance, PARAM_ORIENTATION, &hOrient);
    gParamSuite->paramGetHandle(instance, PARAM_ADAPT_ASPECT, &hAdaptAspect);

    gParamSuite->paramGetValueAtTime(hPeaks, time, &pPeaks);
    gParamSuite->paramGetValueAtTime(hAmp, time, &pAmp);
    gParamSuite->paramGetValueAtTime(hSpeed, time, &pSpeed);
    gParamSuite->paramGetValueAtTime(hOffset, time, &pOffset);
    gParamSuite->paramGetValueAtTime(hOrient, time, &pOrient);
    gParamSuite->paramGetValueAtTime(hAdaptAspect, time, &pAdaptAspect);

    // Fetch memory pointers and resolution boundaries
    float *srcData = nullptr;
    float *dstData = nullptr;
    OfxRectI renderWindow;
    int srcRowBytes, dstRowBytes;

    gPropSuite->propGetPointer(sourceImg, kOfxImagePropData, 0, (void**)&srcData);
    gPropSuite->propGetPointer(outputImg, kOfxImagePropData, 0, (void**)&dstData);
    gPropSuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropSuite->propGetInt(outputImg, kOfxImagePropRowBytes, 0, &dstRowBytes);
    gPropSuite->propGetIntN(outputImg, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);

    int width  = renderWindow.x2 - renderWindow.x1;
    int height = renderWindow.y2 - renderWindow.y1;

    // Retrieve global project pixel properties to handle non-square aspect metrics
    double pixelAspectRatio = 1.0;
    gPropSuite->propGetDouble(sourceImg, kOfxImagePropPixelAspectRatio, 0, &pixelAspectRatio);

    // Precalculate wave properties
    float radOrient = (float)pOrient * DEG_TO_RAD;
    float cosOrient = std::cos(radOrient);
    float sinOrient = std::sin(radOrient);
    
    // Wave animation offset over time
    float timePhase = ((float)pOffset * DEG_TO_RAD) + ((float)time * ((float)pSpeed * 0.05f));
    float waveScale = ((float)pPeaks * 2.0f * 3.14159265f) / (float)width;
    
    // Establish structural sizing scales
    float ampScaleX = 1.0f;
    float ampScaleY = 1.0f;

    if (pAdaptAspect != 0 && height > 0 && width > 0) {
        float physicalAspect = ((float)width / (float)height) * (float)pixelAspectRatio;
        // Normalize aspect distortions by scaling displacement components individually
        ampScaleX = 1.0f / (float)pixelAspectRatio;
        ampScaleY = physicalAspect;
    }

    float ampPixelsX = (float)pAmp * ampScaleX;
    float ampPixelsY = (float)pAmp * ampScaleY;

    // --- AVX2 SIMD Pixel Execution Loop ---
    for (int y = 0; y < height; ++y) {
        float *dstRow = (float*)((char*)dstData + (y * dstRowBytes));
        
        int x = 0;
        // Process 8 pixels simultaneously using 256-bit AVX vectors
        for (; x <= width - 8; x += 8) {
            alignas(32) float x_vals;
            for(int i = 0; i < 8; ++i) x_vals[i] = (float)(x + i);

            __m256 vx = _mm256_load_ps(x_vals);
            __m256 vy = _mm256_set1_ps((float)y);

            // Spatial calculation: phase = (x * cosOrient + y * sinOrient) * waveScale + timePhase
            __m256 vCos = _mm256_set1_ps(cosOrient);
            __m256 vSin = _mm256_set1_ps(sinOrient);
            __m256 vScale = _mm256_set1_ps(waveScale);
            __m256 vPhaseOffset = _mm256_set1_ps(timePhase);

            __m256 vProj = _mm256_add_ps(_mm256_mul_ps(vx, vCos), _mm256_mul_ps(vy, vSin));
            __m256 vPhase = _mm256_add_ps(_mm256_mul_ps(vProj, vScale), vPhaseOffset);

            // Compute structural displacement vector approximations via scalar fallback inside lanes
            alignas(32) float phases;
            _mm256_store_ps(phases, vPhase);

            alignas(32) float disp_x;
            alignas(32) float disp_y;

            for(int i = 0; i < 8; ++i) {
                float sineVal = std::sin(phases[i]);
                // Apply separate horizontal and vertical aspect-calibrated scales
                disp_x[i] = x_vals[i] - (sineVal * ampPixelsX * sinOrient);
                disp_y[i] = (float)y + (sineVal * ampPixelsY * cosOrient);

                // Bound check checking constraints to prevent memory leakage faults
                disp_x[i] = std::clamp(disp_x[i], 0.0f, (float)(width - 1));
                disp_y[i] = std::clamp(disp_y[i], 0.0f, (float)(height - 1));

                // Remap sample pointers using nearest neighbor calculation routines
                int sampleX = (int)(disp_x[i] + 0.5f);
                int sampleY = (int)(disp_y[i] + 0.5f);

                float *srcPixel = (float*)((char*)srcData + (sampleY * srcRowBytes)) + (sampleX * 4);
                float *dstPixel = dstRow + ((x + i) * 4);

                // Copy over 4 RGBA channels
                dstPixel = srcPixel;
                dstPixel = srcPixel;
                dstPixel = srcPixel;
                dstPixel = srcPixel;
            }
        }

        // Clean up remaining scalar pixels at boundaries
        for (; x < width; ++x) {
            float phase = ((float)x * cosOrient + (float)y * sinOrient) * waveScale + timePhase;
            float sineVal = std::sin(phase);

            float sampleX = (float)x - (sineVal * ampPixelsX * sinOrient);
            float sampleY = (float)y + (sineVal * ampPixelsY * cosOrient);

            int sX = std::clamp((int)(sampleX + 0.5f), 0, width - 1);
            int sY = std::clamp((int)(sampleY + 0.5f), 0, height - 1);

            float *srcPixel = (float*)((char*)srcData + (sY * srcRowBytes)) + (sX * 4);
            float *dstPixel = dstRow + (x * 4);

            std::memcpy(dstPixel, srcPixel, sizeof(float) * 4);
        }
    }

    // Free references
    gEffectSuite->clipReleaseImage(sourceImg);
    gEffectSuite->clipReleaseImage(outputImg);
    return kOfxStatOK;
}

// --- Multiplexer Central Hub Pipeline Engine Entry Point ---
OfxStatus pluginMainEntry(const char *action, const void *handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    if (strcmp(action, kOfxImageEffectActionDescribe) == 0) {
        return DescribePlugin((OfxImageEffectHandle)handle);
    }
    else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
        return RenderEffect((OfxImageEffectHandle)handle, inArgs);
    }
    return kOfxStatReplyDefault;
}
