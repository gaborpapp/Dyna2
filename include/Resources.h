#pragma once
#include "cinder/CinderResources.h"

#define RES_KAWASE_BLOOM_VERT	CINDER_RESOURCE(../resources/, shaders/KawaseBloom.vert, 128, GLSL)
#define RES_KAWASE_BLOOM_FRAG	CINDER_RESOURCE(../resources/, shaders/KawaseBloom.frag, 129, GLSL)

#define RES_PASSTHROUGH_VERT	CINDER_RESOURCE(../resources/, shaders/PassThrough.vert, 131, GLSL)
#define RES_MIXER_FRAG			CINDER_RESOURCE(../resources/, shaders/Mixer.frag, 133, GLSL)

#define RES_GALLERY_FRAG		CINDER_RESOURCE(../resources/, shaders/Gallery.frag, 148, GLSL)

#define RES_SHUTTER				CINDER_RESOURCE(../resources/, audio/72714__horsthorstensen__shutter-photo.mp3, 135, MP3)

#define RES_CURSOR_LEFT		CINDER_RESOURCE(../resources/, gfx/cursors/lefthand.png, 146, PNG)
#define RES_CURSOR_RIGHT	CINDER_RESOURCE(../resources/, gfx/cursors/righthand.png, 147, PNG)

#define RES_WATERMARK		CINDER_RESOURCE(../resources/, gfx/ilovetelekom.png, 149, PNG)

