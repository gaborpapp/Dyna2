#pragma once

#include "cinder/gl/Texture.h"
#include "cinder/FileSystem.h"

class TimerDisplay
{
	public:
		TimerDisplay() {}
		TimerDisplay( const ci::fs::path &bottomLeft,
					  const ci::fs::path &bottomMiddle,
					  const ci::fs::path &bottomRight,
					  const ci::fs::path &dot0,
					  const ci::fs::path &dot1 );
		void draw( float u );

	private:
		ci::gl::Texture mBottomLeft;
		ci::gl::Texture mBottomMiddle;
		ci::gl::Texture mBottomRight;
		ci::gl::Texture mDot0;
		ci::gl::Texture mDot1;
};

