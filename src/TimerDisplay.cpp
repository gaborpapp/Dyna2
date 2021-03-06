#include "cinder/app/App.h"
#include "cinder/ImageIo.h"
#include "cinder/Rect.h"
#include "cinder/CinderMath.h"

#include "TimerDisplay.h"
#include "Resources.h"

using namespace std;
using namespace ci;


TimerDisplay::TimerDisplay( const fs::path &bottomLeft,
					  const fs::path &bottomMiddle,
					  const fs::path &bottomRight,
					  const fs::path &dot0,
					  const fs::path &dot1 )
{
	mBottomLeft = loadImage( app::loadAsset( bottomLeft ) );
	mBottomMiddle = loadImage( app::loadAsset( bottomMiddle ) );
	mBottomRight = loadImage( app::loadAsset( bottomRight ) );
	mDot0 = loadImage( app::loadAsset( dot0 ) );
	mDot1 = loadImage( app::loadAsset( dot1 ) );
}

void TimerDisplay::draw( float u )
{
	u = math< float >::clamp( u );
	int width = app::getWindowWidth();
	int height = app::getWindowHeight();
	int barHeight = mBottomMiddle.getHeight();

	gl::draw( mBottomLeft, Vec2f( 0, height - barHeight ) );
	gl::draw( mBottomMiddle, Rectf( mBottomLeft.getWidth(), height - barHeight,
									width - mBottomRight.getWidth(), height ) );
	gl::draw( mBottomRight, Vec2f( width - mBottomRight.getWidth(), height - barHeight ) );

	int barWidth = width - mBottomRight.getWidth() - mBottomLeft.getWidth();
	int dotWidth = mDot0.getWidth();
	int dots = barWidth / dotWidth;
	float dotStep = barWidth / (float)dots;
	float txtStep = 1. / (float)( dots + 1 );
	Vec2f pos( mBottomLeft.getWidth() + ( dotStep - dotWidth ) / 2., height - barHeight );
	float txtU = txtStep;
	for ( int i = 0; i < dots; i++)
	{
		if ( txtU > u )
			gl::draw( mDot0, pos );
		else
			gl::draw( mDot1, pos );

		pos += Vec2f( dotStep, 0 );
		txtU += txtStep;
	}
}

