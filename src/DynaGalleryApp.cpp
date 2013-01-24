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

#include <ctime>
#include <vector>
#include <string>
#include <set>

#include "cinder/Cinder.h"
#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/Texture.h"
#include "cinder/ImageIo.h"
#include "cinder/Timeline.h"
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
		void setLogoTimeline();
		void drawGallery();
		void checkNewPicturesThread();
		void addNewPictures();

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

		TimelineRef           mLogoTimeline;
		ci::Anim< float >     mLogoOpacity;
		gl::Texture           mLogo;

		vector< Surface >     mNewImages;
		std::recursive_mutex  mMutexNewImages;
		set<string>           mFileNames;
		std::recursive_mutex  mMutexFileNames;
		shared_ptr< thread >  mNewPicturesThread;
		bool                  mNewPicturesThreadShouldQuit;
};

void DynaGalleryApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 640, 480 );
}

DynaGalleryApp::DynaGalleryApp()
: mVideoNoiseFreq( 11. )
, mEnableVignetting( true )
, mEnableTvLines( true )
, mLogoTimeline( Timeline::create())
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

	mParams = params::PInterfaceGl("Parameters", Vec2i( 300, 220 ), Vec2i( 16, 16 ));
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
				fs::path newPath = app::App::get()->getFolderPath( this->mGalleryPath );
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

	mNewPicturesThreadShouldQuit = false;
	mNewPicturesThread = shared_ptr< thread >( new thread( bind( &DynaGalleryApp::checkNewPicturesThread, this )));

	fs::path logoPath( "gfx/dynaGallery/logo.png" );
	mLogo = gl::Texture( loadImage( app::loadAsset( logoPath )));
	setLogoTimeline();

	// gallery
	mGalleryPath = mGalleryFolder;
	try
	{
		fs::create_directories( mGalleryPath );
	}
	catch ( fs::filesystem_error &exc )
	{
		console() << exc.what() << endl;
	}

	mGallery = Gallery::create( mGalleryPath );

	//setFullScreen( true );
	//hideCursor();

	params::PInterfaceGl::showAllParams( false );
}

void DynaGalleryApp::shutdown()
{
	mNewPicturesThreadShouldQuit = true;
	mNewPicturesThread->join();

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

			lock_guard<recursive_mutex> lock( mMutexFileNames );
			mFileNames.clear();
		}
	}

	addNewPictures();

	mGallery->update();
}

void DynaGalleryApp::setLogoTimeline()
{
	// logo
	timeline().remove( mLogoTimeline );

	mLogoTimeline->clear();
	mLogoTimeline->setDefaultAutoRemove( false );
	mLogoTimeline->apply( &mLogoOpacity, 0.f, 0.f, 5.f );
	mLogoTimeline->appendTo( &mLogoOpacity, 0.f, 0.9f, .9f );

	mLogoTimeline->appendTo( &mLogoOpacity, 0.9f, 0.9f, 3.f );
	mLogoTimeline->appendTo( &mLogoOpacity, 0.9f, 0.f, 1.f );
	mLogoTimeline->appendTo( &mLogoOpacity, 0.f, 0.f, 5.f );
	mLogoTimeline->setLoop( true );
	timeline().add( mLogoTimeline );
}

void DynaGalleryApp::checkNewPicturesThread()
{
	while( ! mNewPicturesThreadShouldQuit )
	{
		try
		{
			fs::directory_iterator it( mGalleryPath );

			for( ; it != fs::directory_iterator(); ++it )
			{
				if( fs::is_regular_file( *it ) && ( it->path().extension().string() == ".png" ))
				{
					lock_guard<recursive_mutex> lock( mMutexFileNames );
					if( mFileNames.find( it->path().filename().string()) == mFileNames.end())
					{
						try
						{
							Surface surface = loadImage( it->path());

							{
								lock_guard<recursive_mutex> lock( mMutexNewImages );
								mNewImages.push_back( surface );
							}

							mFileNames.insert( it->path().filename().string());
						}
						catch( const ImageIoException &exc )
						{
						}
					}
				}

				// quits sooner if there are a lot of files
				if ( mNewPicturesThreadShouldQuit )
					break;
			}
		}
		catch ( fs::filesystem_error &exc )
		{
		}

		ci::sleep( mGalleryCheckTime * 1000 );
	}
}

void DynaGalleryApp::addNewPictures()
{
	lock_guard<recursive_mutex> lock( mMutexNewImages );
	for( auto it = mNewImages.begin(); it != mNewImages.end(); ++it )
	{
		mGallery->addImage( *it );
	}
	mNewImages.clear();
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

	// logo
	gl::enableAlphaBlending();
	gl::color( ColorA( 1., 1., 1., mLogoOpacity ));

	Rectf rect( mLogo.getBounds() );
	rect = rect.getCenteredFit( getWindowBounds(), true );
	rect.scaleCentered( .5 );
	gl::draw( mLogo, rect );
	gl::color( Color::white() );
	gl::disableAlphaBlending();
}

void DynaGalleryApp::draw()
{
	gl::clear( Color::black());

	drawGallery();

	params::InterfaceGl::draw();
}

CINDER_APP_BASIC(DynaGalleryApp, RendererGl( RendererGl::AA_NONE ))

