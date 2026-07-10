#include <cstring>
#include <cmath>
#include "ofxImageEffect.h"
#include "ofxParam.h"

// Define Parameter Names
#define PARAM_H_TILES   "horizontalTiles"
#define PARAM_V_TILES   "verticalTiles"
#define PARAM_LINKED    "linkedTiles"
#define PARAM_ANCHOR_X  "anchorX"
#define PARAM_ANCHOR_Y  "anchorY"

// Global suites provided by the host
static OfxPropertySuitesV1*    gPropSuite = nullptr;
static OfxImageEffectSuitesV1* gEffectSuite = nullptr;
static OfxParamSuitesV1*       gParamSuite = nullptr;

// Set host pointers
static void setHost(OfxHost* host) {
    gPropSuite   = (OfxPropertySuitesV1*)host->fetchSuite(host->host, kOfxPropertySuite, 1);
    gEffectSuite = (OfxImageEffectSuitesV1*)host->fetchSuite(host->host, kOfxImageEffectSuite, 1);
    gParamSuite  = (OfxParamSuitesV1*)host->fetchSuite(host->host, kOfxParamSuite, 1);
}

// Describe general plugin attributes (Added plugin metadata here)
static OfxStatus describe(OfxImageEffectHandle effect) {
    OfxPropertySetHandle props;
    gEffectSuite->getPropertySet(effect, &props);
    
    // Set UI Display Names and Description for Host UI Selection Panels
    gPropSuite->propSetString(props, kOfxPropLabel, 0, "Amiga Rulez");
    gPropSuite->propSetString(props, kOfxPropLongLabel, 0, "A tile effect that felt like Amiga Rulez.");
    gPropSuite->propSetString(props, kOfxImageEffectPropPluginDescription, 0, 
        "A retro 90s demoscene style repeat tiling effect with adjustable anchors.");
    
    // Define operational constraints
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthRGBA8Bit);
    
    return kOfxStatOK;
}

// Define UI parameters, constraints, and default states
static OfxStatus describeInContext(OfxImageEffectHandle effect) {
    OfxParamSetHandle paramSet;
    gParamSuite->paramGetParamSet(effect, &paramSet);
    
    OfxPropertySetHandle paramProps;
    
    // Horizontal Tiles
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_H_TILES, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Horizontal Tiles"); // Display Name
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "Number of horizontal tiles");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 20.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 20.00);
    gPropSuite->propSetInt(paramProps, kOfxParamPropCanAnimate, 0, 1);
    
    // Linked Tiles Checkbox
    gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, PARAM_LINKED, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Linked Tiles"); // Display Name
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "Lock vertical tiles to horizontal aspect");
    gPropSuite->propSetInt(paramProps, kOfxParamPropDefault, 0, 1); // Checked by default
    gPropSuite->propSetInt(paramProps, kOfxParamPropCanAnimate, 0, 1);
    
    // Vertical Tiles
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_V_TILES, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Vertical Tiles"); // Display Name
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "Number of vertical tiles");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 1.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 20.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDisplayMin, 0, 0.10);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDisplayMax, 0, 20.00);
    gPropSuite->propSetInt(paramProps, kOfxParamPropCanAnimate, 0, 1);
    
    // Anchor X
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_ANCHOR_X, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Anchor X"); // Display Name
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "Horizontal anchor point center");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.50);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.00);
    gPropSuite->propSetInt(paramProps, kOfxParamPropCanAnimate, 0, 1);
    
    // Anchor Y
    gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, PARAM_ANCHOR_Y, &paramProps);
    gPropSuite->propSetString(paramProps, kOfxPropLabel, 0, "Anchor Y"); // Display Name
    gPropSuite->propSetString(paramProps, kOfxParamPropHint, 0, "Vertical anchor point center");
    gPropSuite->propSetDouble(paramProps, kOfxParamPropDefault, 0, 0.50);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMin, 0, 0.00);
    gPropSuite->propSetDouble(paramProps, kOfxParamPropMax, 0, 1.00);
    gPropSuite->propSetInt(paramProps, kOfxParamPropCanAnimate, 0, 1);
    
    return kOfxStatOK;
}

// Handles the checkbox interaction to enable/disable Vertical Tiles dynamically
static OfxStatus instanceChanged(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    char* type = nullptr;
    char* name = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropType, 0, &type);
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &name);
    
    // Listen strictly to initialization or changes on the linked checkbox
    if (type && name && strcmp(type, kOfxTypeParameter) == 0 && 
       (strcmp(name, PARAM_LINKED) == 0 || strcmp(name, kOfxActionInstanceChanged) == 0)) {
        OfxParamSetHandle paramSet;
        gParamSuite->paramGetParamSet(instance, &paramSet);
        
        OfxParamHandle linkedParam, vTilesParam;
        gParamSuite->paramGetHandle(paramSet, PARAM_LINKED, &linkedParam, nullptr);
        gParamSuite->paramGetHandle(paramSet, PARAM_V_TILES, &vTilesParam, nullptr);
        
        int isLinked = 1;
        OfxTime time;
        gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
        gParamSuite->paramGetValueAtTime(linkedParam, time, &isLinked);
        
        // If linked is checked, gray out and disable Vertical Tiles
        gParamSuite->paramSetEnabled(vTilesParam, !isLinked);
    }
    return kOfxStatOK;
}

// Image rendering math logic
static OfxStatus render(OfxImageEffectHandle instance, OfxPropertySetHandle inArgs) {
    OfxTime time;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);
    
    // Fetch system inputs/outputs
    OfxImageEffectRectI renderWindow;
    gPropSuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, &renderWindow.x1);
    
    OfxImageClipHandle sourceClip, outputClip;
    gEffectSuite->clipGetHandle(instance, kOfxImageEffectSimpleSourceClipName, &sourceClip, nullptr);
    gEffectSuite->clipGetHandle(instance, kOfxImageEffectSimpleDestClipName, &outputClip, nullptr);
    
    OfxPropertySetHandle sourceImg, outputImg;
    gEffectSuite->clipGetImage(sourceClip, time, nullptr, &sourceImg);
    gEffectSuite->clipGetImage(outputClip, time, nullptr, &outputImg);
    
    // Get Pixel Data Buffers
    void *srcBuffer = nullptr;
    void *dstBuffer = nullptr;
    int srcRowBytes = 0;
    int dstRowBytes = 0;
    OfxImageEffectRectI bounds;
    
    gPropSuite->propGetPointer(sourceImg, kOfxImageEffectPropData, 0, &srcBuffer);
    gPropSuite->propGetPointer(outputImg, kOfxImageEffectPropData, 0, &dstBuffer);
    gPropSuite->propGetInt(sourceImg, kOfxImageEffectPropRowBytes, 0, &srcRowBytes);
    gPropSuite->propGetInt(outputImg, kOfxImageEffectPropRowBytes, 0, &dstRowBytes);
    gPropSuite->propGetIntN(sourceImg, kOfxImageEffectPropBounds, 4, &bounds.x1);
    
    // Extract Parameter values at current timeline context frame
    OfxParamSetHandle paramSet;
    gParamSuite->paramGetParamSet(instance, &paramSet);
    
    OfxParamHandle hTilesP, vTilesP, linkedP, anchXP, anchYP;
    gParamSuite->paramGetHandle(paramSet, PARAM_H_TILES, &hTilesP, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_V_TILES, &vTilesP, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_LINKED, &linkedP, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_ANCHOR_X, &anchXP, nullptr);
    gParamSuite->paramGetHandle(paramSet, PARAM_ANCHOR_Y, &anchYP, nullptr);
    
    double hTiles = 1.0, vTiles = 1.0, anchX = 0.5, anchY = 0.5;
    int linked = 1;
    gParamSuite->paramGetValueAtTime(hTilesP, time, &hTiles);
    gParamSuite->paramGetValueAtTime(vTilesP, time, &vTiles);
    gParamSuite->paramGetValueAtTime(linkedP, time, &linked);
    gParamSuite->paramGetValueAtTime(anchXP, time, &anchX);
    gParamSuite->paramGetValueAtTime(anchYP, time, &anchY);
    
    if (linked) {
        vTiles = hTiles; // Force 1:1 mapping when locked
    }
    
    int width = bounds.x2 - bounds.x1;
    int height = bounds.y2 - bounds.y1;
    
    if (width <= 0 || height <= 0 || !srcBuffer || !dstBuffer) {
        if (sourceImg) gEffectSuite->clipReleaseImage(sourceImg);
        if (outputImg) gEffectSuite->clipReleaseImage(outputImg);
        return kOfxStatFailed;
    }

    // Pixel Processing Loop (RGBA 8-Bit)
    for (int y = renderWindow.y1; y < renderWindow.y2; ++y) {
        auto* dstRow = (unsigned char*)((char*)dstBuffer + (y - renderWindow.y1) * dstRowBytes);
        
        for (int x = renderWindow.x1; x < renderWindow.x2; ++x) {
            // Normalized coordinates (0.0 to 1.0) relative to bounds
            double normX = (double)(x - bounds.x1) / width;
            double normY = (double)(y - bounds.y1) / height;
            
            // Shift coordinate origin space to requested anchor
            double relX = normX - anchX;
            double relY = normY - anchY;
            
            // Apply scale tiling calculations
            double tileX = relX * hTiles;
            double tileY = relY * vTiles;
            
            // Re-wrap via positive fraction math (Amiga repeat wrap logic)
            double fracX = tileX - std::floor(tileX);
            double fracY = tileY - std::floor(tileY);
            
            // Translate back into absolute source coordinate spaces
            double finalNormX = fracX + anchX;
            double finalNormY = fracY + anchY;
            
            // Mirror repeat protection boundary conditions
            finalNormX = finalNormX - std::floor(finalNormX);
            finalNormY = finalNormY - std::floor(finalNormY);
            
            int srcX = bounds.x1 + (int)(finalNormX * width);
            int srcY = bounds.y1 + (int)(finalNormY * height);
            
            // Boundary clamping safety checks
            if (srcX < bounds.x1) srcX = bounds.x1;
            if (srcX >= bounds.x2) srcX = bounds.x2 - 1;
            if (srcY < bounds.y1) srcY = bounds.y1;
            if (srcY >= bounds.y2) srcY = bounds.y2 - 1;
            
            auto* srcPixel = (unsigned char*)((char*)srcBuffer + (srcY - bounds.y1) * srcRowBytes + (srcX - bounds.x1) * 4);
            auto* dstPixel = &dstRow[(x - renderWindow.x1) * 4];
            
            // Copy color components
            dstPixel = srcPixel; // R
            dstPixel = srcPixel; // G
            dstPixel = srcPixel; // B
            dstPixel = srcPixel; // A
        }
    }
    
    gEffectSuite->clipReleaseImage(sourceImg);
    gEffectSuite->clipReleaseImage(outputImg);
    return kOfxStatOK;
}

// Global Plugin Entry Main Pointer Engine dispatcher
static OfxStatus mainEntry(const char* action, const void* handle, OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    if (strcmp(action, kOfxActionLoad) == 0) {
        // Handled globally outside
    } else if (strcmp(action, kOfxImageEffectActionDescribe) == 0) {
        return describe((OfxImageEffectHandle)handle);
    } else if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
        return describeInContext((OfxImageEffectHandle)handle);
    } else if (strcmp(action, kOfxImageEffectActionRender) == 0) {
        return render((OfxImageEffectHandle)handle, inArgs);
    } else if (strcmp(action, kOfxActionInstanceChanged) == 0) {
        return instanceChanged((OfxImageEffectHandle)handle, inArgs);
    }
    return kOfxStatReplyDefault;
}

// Registry layout descriptors
static OfxPlugin amgPlugin = {
    kOfxImageEffectPluginApi,
    1,
    "com.boutiqueofx.offsetwrap",
    1, 0,
    setHost,
    mainEntry
};

extern "C" OfxPlugin* OfxGetPlugin(int nth) {
    if (nth == 0) return &amgPlugin;
    return nullptr;
}

extern "C" int OfxGetNumberOfPlugins() {
    return 1;
}
