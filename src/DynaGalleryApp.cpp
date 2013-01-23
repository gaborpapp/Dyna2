/*
 Copyright (C) 2012-2013 Gabor Papp, Botond Barna

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <vector>
#include <string>

#include "cinder/Cinder.h"
#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/ImageIo.h"
#include "cinder/Rand.h"

#include "Resources.h"

#include "PParams.h"
#include "Utils.h"

#include "Gallery.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class DynaGalleryApp : public AppBasic
{
	public:
		DynaGalleryApp();

		void prepareSettings( Settings *settings );
		void setup();
		void shutdown();

		void keyDown( KeyEvent event );

		void update();
		void draw();

	private:
		void drawGallery();
		void checkNewPictures();
		void fillPictures();

	private:
		params::PInterfaceGl  mParams;
		gl::Fbo               mOutputFbo;

		float                 mVideoNoiseFreq;
		bool                  mEnableVignetting;
		bool                  mEnableTvLines;

		float                 mFps;

		GalleryRef            mGallery;
		fs::path              mGalleryPath;
		string                mGalleryFolder;
		float                 mGalleryCheckTime;
		double                mLastTime;
		vector< std::string > mPictures;
		std::recursive_mutex  mMutex;
};

void DynaGalleryApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 1024, 768 );
}

DynaGalleryApp::DynaGalleryApp()
: mVideoNoiseFreq( 11. )
, mEnableVignetting( true )
, mEnableTvLines( true )
{
}

void DynaGalleryApp::setup()
{
	gl::disableVerticalSync();

	mGalleryPath = getAppPath();
#ifdef CINDER_MAC
	mGalleryPath /= "..";
#endif
	mGalleryPath /= "screenshots";
	mGalleryFolder = mGalleryPath.string();

	// params
	params::PInterfaceGl::load( "params.xml" );

	mParams = params::PInterfaceGl("Parameters", Vec2i( 300, 200 ), Vec2i( 16, 16 ));
	mParams.addPersistentSizeAndPosition();

	mParams.addSeparator();
	mParams.addText("Visuals");
	mParams.addPersistentParam("Video noise freq", &mVideoNoiseFreq, mVideoNoiseFreq, "min=0 max=11. step=.01");
	mParams.addPersistentParam("Vignetting", &mEnableVignetting, mEnableVignetting);
	mParams.addPersistentParam("TV lines", &mEnableTvLines, mEnableTvLines);
	mParams.addSeparator();
	mParams.addPersistentParam("Gallery folder", &mGalleryFolder, mGalleryFolder, "", "" );
	mParams.addButton( "Choose gallery folder",
			[ this ]()
			{
				fs::path newPath = app::App::getFolderPath( this->mGalleryPath );
				if ( !newPath.empty() )
					this->mGalleryFolder = newPath.string();
			} );
	mParams.addPersistentParam("Check time", &mGalleryCheckTime, 1.0, "min=0.5 max=10.0 step=0.1" );

	mParams.addSeparator();
	mParams.addText("Debug");
	mParams.addParam("Fps", &mFps, "", true);

	gl::Fbo::Format format = gl::Fbo::Format();
	format.enableDepthBuffer( false );
	mOutputFbo = gl::Fbo( 1920, 1080, format );

	Rand::randomize();

	// gallery
	mGalleryPath = mGalleryFolder;
	fs::create_directory( mGalleryPath );
	fillPictures();

	mGallery = Gallery::create( mGalleryPath );

	setFullScreen( true );
	hideCursor();

	mLastTime = getElapsedSeconds();

	params::PInterfaceGl::showAllParams( false );
}

void DynaGalleryApp::shutdown()
{
	params::PInterfaceGl::save();
}

void DynaGalleryApp::keyDown(KeyEvent event)
{
	switch ( event.getCode() )
	{
		case KeyEvent::KEY_f:
			if ( !isFullScreen() )
			{
				setFullScreen( true );
				if ( mParams.isVisible() )
					showCursor();
				else
					hideCursor();
			}
			else
			{
				setFullScreen( false );
				showCursor();
			}
			break;

		case KeyEvent::KEY_s:
			params::PInterfaceGl::showAllParams( !mParams.isVisible() );
			if ( isFullScreen() )
			{
				if ( mParams.isVisible() )
					showCursor();
				else
					hideCursor();
			}
			break;

		case KeyEvent::KEY_ESCAPE:
			quit();
			break;

		default:
			break;
	}
}

void DynaGalleryApp::update()
{
	mFps = getAverageFps();

	if( mGalleryPath != mGalleryFolder )
	{
		if( ! fs::exists( fs::path( mGalleryFolder )))
		{
			mGalleryFolder = mGalleryPath.string();
		}
		else
		{
			mGalleryPath = mGalleryFolder;
			mGallery->setFolder( mGalleryPath );
			fillPictures();
		}
	}

	checkNewPictures();

	mGallery->update();
}

void DynaGalleryApp::fillPictures()
{
	mPictures.clear();

	for( fs::directory_iterator it( mGalleryPath ); it != fs::directory_iterator(); ++it )
	{
		if( fs::is_regular_file( *it ) && ( it->path().extension().string() == ".png" ))
		{
			std::string fileName = it->path().filename().string();
			mPictures.push_back( fileName );
		}
	}
}

void DynaGalleryApp::checkNewPictures()
{
	double time = getElapsedSeconds();
	if( time - mLastTime < mGalleryCheckTime )
		return;

	mLastTime = time;

	{
		lock_guard<recursive_mutex> lock( mMutex );
		for( fs::directory_iterator it( mGalleryPath ); it != fs::directory_iterator(); ++it )
		{
			if( fs::is_regular_file( *it ) && ( it->path().extension().string() == ".png" ))
			{
				std::string fileName = it->path().filename().string();
				if( std::find( mPictures.begin(), mPictures.end(), fileName ) == mPictures.end())
				{
					mPictures.push_back( fileName );

					int toPic = -1;
					toPic = Rand::randInt( 0, mGallery->getSize());
					mGallery->addImage( mGalleryPath / fileName, toPic );
				}
			}
		}
	}
}

void DynaGalleryApp::drawGallery()
{
	static ci::Anim< float > poseOpacity;

	gl::clear( Color::black());

	gl::setMatricesWindow( getWindowSize());
	gl::setViewport( getWindowBounds());

	mGallery->setNoiseFreq( exp( mVideoNoiseFreq ));
	mGallery->enableVignetting( mEnableVignetting );
	mGallery->enableTvLines( mEnableTvLines );

	mGallery->render( getWindowBounds());
}

void DynaGalleryApp::draw()
{
	gl::clear( Color::black());

	drawGallery();

	params::InterfaceGl::draw();
}

CINDER_APP_BASIC(DynaGalleryApp, RendererGl( RendererGl::AA_NONE ))

