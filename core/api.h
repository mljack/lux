/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#ifndef LUX_API_H
#define LUX_API_H 1

#ifdef __cplusplus
extern "C" {
#endif

typedef const char *LuxToken;
typedef const char *LuxPointer;
#define LUX_NULL NULL

const char *luxVersion();
void luxInit();
int luxParse(const char *filename);
/* allows for parsing of partial files, caller does error handling */
int luxParsePartial(const char *filename);
void luxCleanup();

/* Basic control flow, scoping, stacks */
void luxIdentity();
void luxTranslate(float dx, float dy, float dz);
void luxRotate(float angle, float ax, float ay, float az);
void luxScale(float sx, float sy, float sz);
void luxLookAt(float ex, float ey, float ez, float lx, float ly, float lz, float ux, float uy, float uz);
void luxConcatTransform(float transform[16]);
void luxTransform(float transform[16]);
void luxCoordinateSystem(const char *);
void luxCoordSysTransform(const char *);
void luxPixelFilter(const char *name, ...);
void luxPixelFilterV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxFilm(const char *name, ...);
void luxFilmV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxSampler(const char *name, ...);
void luxSamplerV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxAccelerator(const char *name, ...);
void luxAcceleratorV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxSurfaceIntegrator(const char *name, ...);
void luxSurfaceIntegratorV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxVolumeIntegrator(const char *name, ...);
void luxVolumeIntegratorV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxCamera(const char *name, ...);
void luxCameraV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxWorldBegin();
void luxAttributeBegin();
void luxAttributeEnd();
void luxTransformBegin();
void luxTransformEnd();
void luxTexture(const char *name, const char *type, const char *texname, ...);
void luxTextureV(const char *name, const char *type, const char *texname, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxMaterial(const char *name, ...);
void luxMaterialV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxMakeNamedMaterial(const char *name, ...);
void luxMakeNamedMaterialV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxNamedMaterial(const char *name);
void luxLightSource(const char *name, ...);
void luxLightSourceV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxAreaLightSource(const char *name, ...);
void luxAreaLightSourceV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxPortalShape(const char *name, ...);
void luxPortalShapeV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxShape(const char *name, ...);
void luxShapeV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxRenderer(const char *name, ...);
void luxRendererV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxReverseOrientation();
void luxMakeNamedVolume(const char *id, const char *name, ...);
void luxMakeNamedVolumeV(const char *id, const char *name, unsigned int n,
	const LuxToken tokens[], const LuxPointer params[]);
void luxVolume(const char *name, ...);
void luxVolumeV(const char *name, unsigned int n, const LuxToken tokens[], const LuxPointer params[]);
void luxExterior(const char *name);
void luxInterior(const char *name);
void luxObjectBegin(const char *name);
void luxObjectEnd();
void luxObjectInstance(const char *name);
void luxPortalInstance(const char *name);
void luxMotionInstance(const char *name, float startTime, float endTime, const char *toTransform);
void luxWorldEnd();

/* Load/Save FLM file */
void luxLoadFLM(const char* name);
void luxSaveFLM(const char* name);
/* Overrides the resume settings of the Film in the next scene to resume from the given FLM file, or use filename from scene if empty */
void luxOverrideResumeFLM(const char *name);
/* Overrides the Film output filename */
void luxOverrideFilename(const char *name);

/* Write film to a floating point OpenEXR image */
void luxSaveEXR(const char* name, bool useHalfFloat, bool includeZBuffer, int compressionType, bool tonemapped);

/* User interactive thread functions */
void luxStart();
void luxPause();
void luxExit();
void luxAbort();
void luxWait();

void luxSetHaltSamplesPerPixel(int haltspp, bool haveEnoughSamplesPerPixel, bool suspendThreadsWhenDone);

/* Controlling number of threads */
unsigned int luxAddThread();
void luxRemoveThread();

/* Set the minimum and maximum value used for epsilon */
void luxSetEpsilon(const float minValue, const float maxValue);

/* Framebuffer access */
void luxUpdateFramebuffer();
unsigned char* luxFramebuffer();
float* luxFloatFramebuffer();
float* luxAlphaBuffer();

/* Histogram access */
void luxGetHistogramImage(unsigned char *outPixels, unsigned int width, unsigned int height, int options);
//histogram drawing options
#define    LUX_HISTOGRAM_RGB    	1
#define    LUX_HISTOGRAM_RGB_ADD	2
#define    LUX_HISTOGRAM_RED    	4
#define    LUX_HISTOGRAM_GREEN  	8
#define    LUX_HISTOGRAM_BLUE   	16
#define    LUX_HISTOGRAM_VALUE  	32
#define    LUX_HISTOGRAM_LOG    	64


/* Parameter access */
// Exposed Parameters

enum luxComponent {			LUX_FILM
};

enum luxComponentParameters {	LUX_FILM_TM_TONEMAPKERNEL,
				LUX_FILM_TM_REINHARD_PRESCALE,
				LUX_FILM_TM_REINHARD_POSTSCALE,
				LUX_FILM_TM_REINHARD_BURN,
				LUX_FILM_TM_LINEAR_SENSITIVITY,
				LUX_FILM_TM_LINEAR_EXPOSURE,
				LUX_FILM_TM_LINEAR_FSTOP,
				LUX_FILM_TM_LINEAR_GAMMA,
				LUX_FILM_TM_CONTRAST_YWA,
				LUX_FILM_TORGB_X_WHITE,
				LUX_FILM_TORGB_Y_WHITE,
				LUX_FILM_TORGB_X_RED,
				LUX_FILM_TORGB_Y_RED,
				LUX_FILM_TORGB_X_GREEN,
				LUX_FILM_TORGB_Y_GREEN,
				LUX_FILM_TORGB_X_BLUE,
				LUX_FILM_TORGB_Y_BLUE,
				LUX_FILM_TORGB_GAMMA,
				LUX_FILM_UPDATEBLOOMLAYER,
				LUX_FILM_DELETEBLOOMLAYER,
				LUX_FILM_BLOOMRADIUS,
				LUX_FILM_BLOOMWEIGHT,
				LUX_FILM_VIGNETTING_ENABLED,
				LUX_FILM_VIGNETTING_SCALE,
				LUX_FILM_ABERRATION_ENABLED,
				LUX_FILM_ABERRATION_AMOUNT,
				LUX_FILM_UPDATEGLARELAYER,
				LUX_FILM_DELETEGLARELAYER,
				LUX_FILM_GLARE_AMOUNT,
				LUX_FILM_GLARE_RADIUS,
				LUX_FILM_GLARE_BLADES,
				LUX_FILM_HISTOGRAM_ENABLED,
				LUX_FILM_NOISE_CHIU_ENABLED,
				LUX_FILM_NOISE_CHIU_RADIUS,
				LUX_FILM_NOISE_CHIU_INCLUDECENTER,
				LUX_FILM_NOISE_GREYC_ENABLED,
				LUX_FILM_NOISE_GREYC_AMPLITUDE,
				LUX_FILM_NOISE_GREYC_NBITER,
				LUX_FILM_NOISE_GREYC_SHARPNESS,
				LUX_FILM_NOISE_GREYC_ANISOTROPY,
				LUX_FILM_NOISE_GREYC_ALPHA,
				LUX_FILM_NOISE_GREYC_SIGMA,
				LUX_FILM_NOISE_GREYC_FASTAPPROX,
				LUX_FILM_NOISE_GREYC_GAUSSPREC,
				LUX_FILM_NOISE_GREYC_DL,
				LUX_FILM_NOISE_GREYC_DA,
				LUX_FILM_NOISE_GREYC_INTERP,
				LUX_FILM_NOISE_GREYC_TILE,
				LUX_FILM_NOISE_GREYC_BTILE,
				LUX_FILM_NOISE_GREYC_THREADS,
				LUX_FILM_LG_COUNT,
				LUX_FILM_LG_ENABLE,
				LUX_FILM_LG_NAME,
				LUX_FILM_LG_SCALE,
				LUX_FILM_LG_SCALE_RED,
				LUX_FILM_LG_SCALE_BLUE,
				LUX_FILM_LG_SCALE_GREEN,
				LUX_FILM_LG_TEMPERATURE,
				LUX_FILM_LG_SCALE_X,
				LUX_FILM_LG_SCALE_Y,
				LUX_FILM_LG_SCALE_Z,
				LUX_FILM_GLARE_THRESHOLD,
				LUX_FILM_CAMERA_RESPONSE_ENABLED,
				LUX_FILM_CAMERA_RESPONSE_FILE
};

/* Parameter Access functions */
void luxSetParameterValue(luxComponent comp, luxComponentParameters param, double value, unsigned int index = 0);
double luxGetParameterValue(luxComponent comp, luxComponentParameters param, unsigned int index = 0);
double luxGetDefaultParameterValue(luxComponent comp, luxComponentParameters param, unsigned int index = 0);
void luxSetStringParameterValue(luxComponent comp, luxComponentParameters param, const char* value, unsigned int index = 0);
// an 0-terminated string is copied to dst (a buffer of at least dstlen chars), the length of the entire string is returned
unsigned int luxGetStringParameterValue(luxComponent comp, luxComponentParameters param, char* dst, unsigned int dstlen, unsigned int index = 0);
unsigned int luxGetDefaultStringParameterValue(luxComponent comp, luxComponentParameters param, char* dst, unsigned int dstlen, unsigned int index = 0);

/* Queryable objects */
const char* luxGetAttributes(); /* Returns an XML string containing all queryable data of the current context */
bool luxHasObject(const char * objectName); /* Returns true if the given object exists in the registry */
bool luxHasAttribute(const char * objectName, const char * attributeName); /* Returns true if object has the given attribute */
bool luxHasAttributeDefaultValue(const char * objectName, const char * attributeName); /* Returns true if attribute has a default value */

const char* luxGetStringAttribute(const char * objectName, const char * attributeName); 
const char* luxGetStringAttributeDefault(const char * objectName, const char * attributeName); 
void luxSetStringAttribute(const char * objectName, const char * attributeName, const char * value);
float luxGetFloatAttribute(const char * objectName, const char * attributeName); /* Returns the value of a float attribute */
float luxGetFloatAttributeDefault(const char * objectName, const char * attributeName); /* Returns the default value of a float attribute */
void luxSetFloatAttribute(const char * objectName, const char * attributeName, float value); /* Sets an float attribute value */
double luxGetDoubleAttribute(const char * objectName, const char * attributeName); /* Returns the value of a double attribute */
double luxGetDoubleAttributeDefault(const char * objectName, const char * attributeName); /* Returns the default value of a double attribute */
int luxGetIntAttribute(const char * objectName, const char * attributeName); /* Returns the value of an int attribute */
int luxGetIntAttributeDefault(const char * objectName, const char * attributeName); /* Returns the default value of an int attribute */
void luxSetIntAttribute(const char * objectName, const char * attributeName, int value); /* Sets an int attribute value */
bool luxGetBoolAttribute(const char * objectName, const char * attributeName); /* Returns the value of a bool attribute */
bool luxGetBoolAttributeDefault(const char * objectName, const char * attributeName); /* Returns the default value of a bool attribute */
void luxSetBoolAttribute(const char * objectName, const char * attributeName, bool value); /* Sets a bool attribute value */
void luxSetAttribute(const char * objectName, const char * attributeName, int n, void *values); /* Sets an attribute value */

/* Networking */
void luxAddServer(const char * name);
void luxRemoveServer(const char * name);
unsigned int luxGetServerCount();
void luxUpdateFilmFromNetwork();
void luxSetNetworkServerUpdateInterval(int updateInterval);
int luxGetNetworkServerUpdateInterval();

struct RenderingServerInfo {
	int serverIndex;

	// Dade - connection information
	const char *name; // Dade - name/ip address of the server
	const char *port; // Dade - tcp port of the server
	const char *sid; // Dade - session id for the server

	double numberOfSamplesReceived;
	unsigned int secsSinceLastContact;
};
// Dade - return the number of rendering servers and fill the info buffer with
// information about the servers
unsigned int luxGetRenderingServersStatus(RenderingServerInfo *info, unsigned int maxInfoCount);

/* Informations and statistics */
double luxStatistics(const char *statName);
const char* luxPrintableStatistics(const bool add_total);
const char* luxCustomStatistics(const char *custom_template);

// Dade - enable debug mode
void luxEnableDebugMode();
void luxDisableRandomMode();

/* Error Handlers */
extern int luxLastError; /*  Keeps track of the last error code */
extern void luxErrorFilter(int severity); /* Sets the minimal level of severity to report */
typedef void (*LuxErrorHandler)(int code, int severity, const char *msg);
extern void luxErrorHandler(LuxErrorHandler handler);
extern void luxErrorAbort(int code, int severity, const char *message);
extern void luxErrorIgnore(int code, int severity, const char *message);
extern void luxErrorPrint(int code, int severity, const char *message);
extern LuxErrorHandler luxError;


/*
 Error codes

 1 - 10     System and File errors
 11 - 20     Program Limitations
 21 - 40     State Errors
 41 - 60     Parameter and Protocol Errors
 61 - 80     Execution Errors
 */

#define LUX_NOERROR         0

#define LUX_NOMEM           1       /* Out of memory */
#define LUX_SYSTEM          2       /* Miscellaneous system error */
#define LUX_NOFILE          3       /* File nonexistant */
#define LUX_BADFILE         4       /* Bad file format */
#define LUX_BADVERSION      5       /* File version mismatch */
#define LUX_DISKFULL        6       /* Target disk is full */

#define LUX_UNIMPLEMENT    12       /* Unimplemented feature */
#define LUX_LIMIT          13       /* Arbitrary program limit */
#define LUX_BUG            14       /* Probably a bug in renderer */

#define LUX_NOTSTARTED     23       /* luxInit() not called */
#define LUX_NESTING        24       /* Bad begin-end nesting - jromang will be used in API v2 */
#define LUX_NOTOPTIONS     25       /* Invalid state for options - jromang will be used in API v2 */
#define LUX_NOTATTRIBS     26       /* Invalid state for attributes - jromang will be used in API v2 */
#define LUX_NOTPRIMS       27       /* Invalid state for primitives - jromang will be used in API v2 */
#define LUX_ILLSTATE       28       /* Other invalid state - jromang will be used in API v2 */
#define LUX_BADMOTION      29       /* Badly formed motion block - jromang will be used in API v2 */
#define LUX_BADSOLID       30       /* Badly formed solid block - jromang will be used in API v2 */

#define LUX_BADTOKEN       41       /* Invalid token for request */
#define LUX_RANGE          42       /* Parameter out of range */
#define LUX_CONSISTENCY    43       /* Parameters inconsistent */
#define LUX_BADHANDLE      44       /* Bad object/light handle */
#define LUX_NOPLUGIN       45       /* Can't load requested plugin */
#define LUX_MISSINGDATA    46       /* Required parameters not provided */
#define LUX_SYNTAX         47       /* Declare type syntax error */

#define LUX_MATH           61       /* Zerodivide, noninvert matrix, etc. */

/* Error severity levels */

#define LUX_DEBUG			-1		/* Debugging output */

#define LUX_INFO            0       /* Rendering stats & other info */
#define LUX_WARNING         1       /* Something seems wrong, maybe okay */
#define LUX_ERROR           2       /* Problem.  Results may be wrong */
#define LUX_SEVERE          3       /* So bad you should probably abort */

#ifdef __cplusplus
}
#endif

#endif /* LUX_API_H */
