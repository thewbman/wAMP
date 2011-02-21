/*
 * config.h
 *
 *  Created on: Jan 15, 2011
 *      Author: Katiebird
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "FlacConfig.h"

// Read little Endian
#define READ_LITTLE_ENDIAN


// Define this to turn on the WormDebug options.
// More granular options should be tweeked from WormDebug.h
#ifdef DEBUG_CODE
#define DEBUG 1
#endif


// Here we define the various audio variables
#define NUM_CHANNELS 2	// number of supported audio playback channels
#define DEST_FREQ 44100	// playback frequency
#define LOW_SPEED_RATIO_LIM 0.25 // Lower limit for playback speed factor
#define HIGH_SPEED_RATIO_LIM 4.0 // Upper limit for playback speed factor

#define QUOTEME_(x) #x
#define QUOTEME(x) QUOTEME_(x)

#ifdef ON_DEVICE
#define USE_PDL 1
// define where external resources are
#define RESAMPLE_RES_PATH "res/resamp.res" // locate resample coef
#else

#define RESAMPLE_RES_PATH "git/STAGING/audiophile.application/res/resamp.res"

#endif

#endif /* CONFIG_H_ */