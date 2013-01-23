/*
 Copyright (C) 2012-2013 Gabor Papp

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

#include <list>
#include <map>

#include "cinder/app/AppBasic.h"
#include "cinder/audio/Io.h"
#include "cinder/audio/Output.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"
#include "cinder/Cinder.h"
#include "cinder/ConcurrentCircularBuffer.h"
#include "cinder/ImageIo.h"
#include "cinder/Timeline.h"
#include "cinder/Rand.h"

#include "ciMsaFluidDrawerGl.h"
#include "ciMsaFluidSolver.h"

#include "Resources.h"

#include "CiNI.h"

#include "DynaStroke.h"
#include "Gallery.h"
#include "HandCursor.h"
#include "Particles.h"
#include "PParams.h"
#include "Utils.h"
#include "TimerDisplay.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class DynaApp : public AppBasic, mndl::ni::UserTracker::Listener
{
	public:
		DynaApp();

		void prepareSettings( Settings *settings );
		void setup();
		void shutdown();

		void resize();

		void keyDown( KeyEvent event );

		void mouseDown( MouseEvent event );
		void mouseDrag( MouseEvent event );
		void mouseUp( MouseEvent event );

		void newUser( mndl::ni::UserTracker::UserEvent event );
		void lostUser( mndl::ni::UserTracker::UserEvent event );
		void calibrationEnd( mndl::ni::UserTracker::UserEvent event );

		void update();
		void draw();

	private:
		params::PInterfaceGl mParams;

		void drawGallery();
		void drawGame();

		bool mLeftButton;
		Vec2i mMousePos;
		Vec2i mPrevMousePos;

		void clearStrokes();
		list< DynaStroke > mDynaStrokes;

		static vector< gl::Texture > sBrushes;
		static vector< gl::Texture > sPoseAnim;
		float mBrushColor;

		TimelineRef mPoseAnimTimeline;
		TimelineRef mGameTimeline;
		void setPoseTimeline();

		ci::Anim< float > mPoseAnimOpacity;
		ci::Anim< int > mPoseAnimFrame;

		float mK;
		float mDamping;
		float mStrokeMinWidth;
		float mStrokeMaxWidth;

		int mParticleMin;
		int mParticleMax;
		float mMaxVelocity;
		float mVelParticleMult;
		float mVelParticleMin;
		float mVelParticleMax;

		ciMsaFluidSolver mFluidSolver;
		ciMsaFluidDrawerGl mFluidDrawer;
		static const int sFluidSizeX = 128;

		const static string SCREENSHOT_FOLDER  = "screenshots/";
		const static string WATERMARKED_FOLDER = "watermarked/";
		fs::path mScreenshotPath;
		fs::path mWatermarkedPath;
		string mScreenshotFolder; // mScreenshotPath as string that params can handle
		string mWatermarkedFolder;
		void sendScreenshot();
		void screenshotThreadFn();
		std::thread mScreenshotThread;
		ConcurrentCircularBuffer< Surface > *mScreenshotSurfaces;
		bool mScreenshotThreadShouldQuit = false;

		Surface mWatermark;

		void setIdleState();
		void endGame();

		void addToFluid( Vec2f pos, Vec2f vel, bool addParticles, bool addForce );

		ParticleManager mParticles;

		ci::Vec2i mPrevMouse;

		gl::Fbo mFbo;
		gl::Fbo mBloomFbo;
		gl::Fbo mOutputFbo;
		gl::GlslProg mBloomShader;
		gl::GlslProg mMixerShader;

		int mBloomIterations;
		float mBloomStrength;

		std::thread mKinectThread;
		std::mutex mKinectMutex;
		std::string mKinectProgress;
		void openKinect( const fs::path &path = fs::path() );

		mndl::ni::OpenNI mNI;
		mndl::ni::UserTracker mNIUserTracker;
		gl::Texture mColorTexture;
		gl::Texture mDepthTexture;
		bool mVideoMirrored;
		float mZClip;
		float mVideoOpacity;
		float mVideoNoise;
		float mVideoNoiseFreq;

		bool mEnableVignetting;
		bool mEnableTvLines;

		ci::Anim< float > mFlash;
		float mFps;
		bool mShowHands;

		struct UserStrokes
		{
			UserStrokes()
				: mInitialized( false )
			{
				for (int i = 0; i < JOINTS; i++)
				{
					mPrevActive[i] = false;
					mStrokes[i] = NULL;
				}
			}

			static const int JOINTS = 2;

			DynaStroke *mStrokes[JOINTS];

			bool mActive[JOINTS];
			bool mPrevActive[JOINTS];
			Vec2f mHand[JOINTS];

			bool mInitialized; // start gesture detected
		};
		map< unsigned, UserStrokes > mUserStrokes;

		// start gesture is recognized, the user can draw
		// if the hands are active
		struct UserInit
		{
			UserInit()
			{
				reset();
				mRecognized = false;
				setBrush( Rand::randInt( 0, sBrushes.size() ) );
			}

			void setBrush( int i )
			{
				mBrushIndex = i % sBrushes.size();
				mBrush = sBrushes[ mBrushIndex ];
			}

			void reset()
			{
				for (int i = 0; i < JOINTS; i++)
				{
					mPoseTimeStart[i] = -1;
					mJointMovement[i].set( 0, 0, 0, 0 );
				}
			}

			bool mRecognized; // gesture recognized
			static const int JOINTS = 2;

			double mPoseTimeStart[JOINTS];
			Rectf mJointMovement[JOINTS];

			gl::Texture mBrush;
			int mBrushIndex;
		};

		map< unsigned, UserInit > mUserInitialized;

		audio::SourceRef mAudioShutter;

		float mPoseDuration; // maximum duration to hold start pose
		float mPoseHoldDuration; // current pose hold duration
		float mPoseHoldAreaThr; // hand movement area threshold during pose
		float mSkeletonSmoothing;
		float mGameDuration;
		Anim< float > mGameTimer;

		TimerDisplay mPoseTimerDisplay;
		TimerDisplay mGameTimerDisplay;

		vector< HandCursor > mHandCursors;
		float mHandPosCoeff;
		float mHandTransparencyCoeff;

		enum
		{
			STATE_IDLE = 0,
			STATE_IDLE_POSE,
			STATE_GAME,
			STATE_GAME_POSE,
			STATE_GAME_SHOW_DRAWING
		} States;
		int mState;

		GalleryRef mGallery;

		vector< Surface > mNewImages;
		std::recursive_mutex mMutex;
};

vector< gl::Texture > DynaApp::sBrushes;
vector< gl::Texture > DynaApp::sPoseAnim;

void DynaApp::prepareSettings(Settings *settings)
{
	settings->setWindowSize(640, 480);
}

DynaApp::DynaApp() :
	mBrushColor( .3 ),
	mK( .06 ),
	mDamping( .7 ),
	mStrokeMinWidth( 6 ),
	mStrokeMaxWidth( 16 ),
	mMaxVelocity( 40 ),
	mParticleMin( 0 ),
	mParticleMax( 40 ),
	mVelParticleMult( .26 ),
	mVelParticleMin( 1 ),
	mVelParticleMax( 60 ),
	mBloomIterations( 8 ),
	mBloomStrength( .8 ),
	mZClip( 1085 ),
	mPoseHoldAreaThr( 4000 ),
	mVideoOpacity( .55 ),
	mVideoNoise( .065 ),
	mVideoNoiseFreq( 11. ),
	mEnableVignetting( true ),
	mEnableTvLines( true ),
	mFlash( .0 ),
	mPoseDuration( 2. ),
	mGameDuration( 20. ),
	mHandPosCoeff( 500. ),
	mHandTransparencyCoeff( 465. ),
	mState( STATE_IDLE ),
	mShowHands( true ),
	mPoseAnimTimeline( Timeline::create() ),
	mGameTimeline( Timeline::create() )
{
}

void DynaApp::setup()
{
	GLint n;
	glGetIntegerv( GL_MAX_COLOR_ATTACHMENTS_EXT, &n );
	console() << "max fbo color attachments " << n << endl;

	gl::disableVerticalSync();

	// params
	params::PInterfaceGl::load( "params.xml" );

	mParams = params::PInterfaceGl("Parameters", Vec2i(320, 760));
	mParams.addPersistentSizeAndPosition();

	mKinectProgress = "Connecting...\0\0\0\0\0\0\0\0\0";
	mParams.addParam( "Kinect", &mKinectProgress, "", true );
	mParams.addPersistentParam( "Screenshot folder", &mScreenshotFolder, "", "", true );
	mParams.addButton( "Choose screenshot folder",
			[ this ]()
			{
				fs::path newScreenshotPath = app::App::get()->getFolderPath( this->mScreenshotPath );
				if ( !newScreenshotPath.empty() )
					this->mScreenshotFolder = newScreenshotPath.string();
			} );
	mParams.addPersistentParam( "Watermarked folder", &mWatermarkedFolder, "", "", true );
	mParams.addButton( "Choose watermarked folder",
			[ this ]()
			{
				fs::path newWatermarkedPath = app::App::get()->getFolderPath( this->mWatermarkedPath );
				if ( !newWatermarkedPath.empty() )
					this->mWatermarkedFolder = newWatermarkedPath.string();
			} );
	mParams.addSeparator();

	mParams.addText("Tracking");
	mParams.addPersistentParam( "Mirror", &mVideoMirrored, true );
	mParams.addPersistentParam("Z clip", &mZClip, mZClip, "min=1 max=10000");
	mParams.addPersistentParam("Skeleton smoothing", &mSkeletonSmoothing, 0.7,
			"min=0 max=1 step=.05");
	mParams.addPersistentParam("Start pose movement", &mPoseHoldAreaThr, mPoseHoldAreaThr,
			"min=10 max=10000 "
			"help='allowed area of hand movement during start pose without losing the pose'");
	mParams.addSeparator();

	mParams.addText("Brush simulation");
	mParams.addPersistentParam("Brush color", &mBrushColor, mBrushColor, "min=.0 max=1 step=.02");
	mParams.addPersistentParam("Stiffness", &mK, mK, "min=.01 max=.2 step=.01");
	mParams.addPersistentParam("Damping", &mDamping, mDamping, "min=.25 max=.999 step=.02");
	mParams.addPersistentParam("Stroke min", &mStrokeMinWidth, mStrokeMinWidth, "min=0 max=50 step=.5");
	mParams.addPersistentParam("Stroke width", &mStrokeMaxWidth, mStrokeMaxWidth, "min=-50 max=50 step=.5");

	mParams.addSeparator();
	mParams.addText("Particles");
	mParams.addPersistentParam("Particle min", &mParticleMin, mParticleMin, "min=0 max=50");
	mParams.addPersistentParam("Particle max", &mParticleMax, mParticleMax, "min=0 max=50");
	mParams.addPersistentParam("Velocity max", &mMaxVelocity, mMaxVelocity, "min=1 max=100");
	mParams.addPersistentParam("Velocity particle multiplier", &mVelParticleMult, mVelParticleMult, "min=0 max=2 step=.01");
	mParams.addPersistentParam("Velocity particle min", &mVelParticleMin, mVelParticleMin, "min=1 max=100 step=.5");
	mParams.addPersistentParam("Velocity particle max", &mVelParticleMax, mVelParticleMax, "min=1 max=100 step=.5");

	mParams.addSeparator();
	mParams.addText("Visuals");
	mParams.addPersistentParam("Video opacity", &mVideoOpacity, mVideoOpacity, "min=0 max=1. step=.05");
	mParams.addPersistentParam("Video noise", &mVideoNoise, mVideoNoise, "min=0 max=1. step=.005");
	mParams.addPersistentParam("Video noise freq", &mVideoNoiseFreq, mVideoNoiseFreq, "min=0 max=11. step=.01");
	mParams.addPersistentParam("Vignetting", &mEnableVignetting, mEnableVignetting);
	mParams.addPersistentParam("TV lines", &mEnableTvLines, mEnableTvLines);
	mParams.addSeparator();

	mParams.addPersistentParam("Bloom iterations", &mBloomIterations, mBloomIterations, "min=0 max=8");
	mParams.addPersistentParam("Bloom strength", &mBloomStrength, mBloomStrength, "min=0 max=1. step=.05");
	mParams.addSeparator();

	mParams.addPersistentParam("Enable cursors", &mShowHands, mShowHands);
	mParams.addPersistentParam("Cursor persp", &mHandPosCoeff, mHandPosCoeff, "min=100. max=20000. step=1");
	mParams.addPersistentParam("Cursor transparency", &mHandTransparencyCoeff, mHandTransparencyCoeff, "min=100. max=20000. step=1");

	mParams.addSeparator();
	mParams.addText("Game logic");
	mParams.addPersistentParam("Pose duration", &mPoseDuration, mPoseDuration, "min=1. max=10 step=.5");
	mParams.addPersistentParam("Game duration", &mGameDuration, mGameDuration, "min=10 max=200");

	mParams.addSeparator();
	mParams.addText("Debug");
	mParams.addParam("Fps", &mFps, "", true);

	// fluid
	mFluidSolver.setup( sFluidSizeX, sFluidSizeX );
	mFluidSolver.enableRGB(false).setFadeSpeed(0.002).setDeltaT(.5).setVisc(0.00015).setColorDiffusion(0);
	mFluidSolver.setWrap( false, true );
	mFluidDrawer.setup( &mFluidSolver );

	mParticles.setFluidSolver( &mFluidSolver );

	gl::Fbo::Format format;
	format.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );

	mFbo = gl::Fbo( 1024, 768, format );
	mFluidSolver.setSize( sFluidSizeX, sFluidSizeX / mFbo.getAspectRatio() );
	mFluidDrawer.setup( &mFluidSolver );
	mParticles.setWindowSize( mFbo.getSize() );

	format.enableColorBuffer( true, 8 );
	format.enableDepthBuffer( false );
	format.setWrap( GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE );

	mBloomFbo = gl::Fbo( mFbo.getWidth() / 4, mFbo.getHeight() / 4, format );
	mBloomShader = gl::GlslProg( loadResource( RES_KAWASE_BLOOM_VERT ),
								 loadResource( RES_KAWASE_BLOOM_FRAG ) );
	mBloomShader.bind();
	mBloomShader.uniform( "tex", 0 );
	mBloomShader.uniform( "pixelSize", Vec2f( 1. / mBloomFbo.getWidth(), 1. / mBloomFbo.getHeight() ) );
	mBloomShader.unbind();

	format = gl::Fbo::Format();
	format.enableDepthBuffer( false );
	mOutputFbo = gl::Fbo( 1024, 768, format );

	mMixerShader = gl::GlslProg( loadResource( RES_PASSTHROUGH_VERT ),
								 loadResource( RES_MIXER_FRAG ) );
	mMixerShader.bind();
	int texUnits[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
	mMixerShader.uniform("tex", texUnits, 9);
	mMixerShader.unbind();

	sBrushes = loadTextures("brushes");
	sPoseAnim = loadTextures("pose-anim");

	mWatermark = loadImage( loadResource( RES_WATERMARK ) );

	// audio
	mAudioShutter = audio::load( loadResource( RES_SHUTTER ) );

	// OpenNI
	mKinectThread = thread( bind( &DynaApp::openKinect, this, fs::path() ) );

	fs::path poseTimerGfxPath( "gfx/pose/" );
	mPoseTimerDisplay = TimerDisplay( poseTimerGfxPath / "pose-bottom-left.png",
									  poseTimerGfxPath / "pose-bottom-middle.png",
									  poseTimerGfxPath / "pose-bottom-right.png",
									  poseTimerGfxPath / "pose-dot-0.png",
									  poseTimerGfxPath / "pose-dot-1.png" );

	fs::path gameTimerGfxPath( "gfx/game/" );
	mGameTimerDisplay = TimerDisplay( gameTimerGfxPath / "game-bottom-left.png",
									  gameTimerGfxPath / "game-bottom-middle.png",
									  gameTimerGfxPath / "game-bottom-right.png",
									  gameTimerGfxPath / "game-dot-0.png",
									  gameTimerGfxPath / "game-dot-1.png" );

	Rand::randomize();

	// gallery
	// initialize directory names on first run
	if ( ( mScreenshotFolder == "" ) || ( mWatermarkedFolder == "" ) )
	{
		mScreenshotPath = getAppPath();
#ifdef CINDER_MAC
		mScreenshotPath /= "..";
		mScreenshotPath = fs::canonical( mScreenshotPath );
#endif
		mWatermarkedPath = mScreenshotPath;
		mScreenshotPath /= SCREENSHOT_FOLDER;
		mWatermarkedPath /= WATERMARKED_FOLDER;
		mScreenshotFolder = mScreenshotPath.string();
		mWatermarkedFolder = mWatermarkedPath.string();
	}

	// restore path from strings saved in params
	mScreenshotPath = mScreenshotFolder;
	mWatermarkedPath = mWatermarkedFolder;

	fs::create_directory( mScreenshotPath );
	fs::create_directory( mWatermarkedPath );

	mGallery = Gallery::create( mScreenshotPath );

	// screenshots
	mScreenshotSurfaces = new ConcurrentCircularBuffer<Surface>( 16 );
	mScreenshotThread = thread( bind( &DynaApp::screenshotThreadFn, this ) );

	timeline().add( mGameTimeline );
	setPoseTimeline();

	setFullScreen( true );
	hideCursor();
	params::PInterfaceGl::showAllParams( false );
}

void DynaApp::openKinect( const fs::path &path )
{
	try
	{
		mndl::ni::OpenNI kinect;
		if ( path.empty() )
			kinect = mndl::ni::OpenNI( mndl::ni::OpenNI::Device() );
		else
			kinect = mndl::ni::OpenNI( path );
		{
			std::lock_guard< std::mutex > lock( mKinectMutex );
			mNI = kinect;
		}
	}
	catch ( ... )
	{
		if ( path.empty() )
			mKinectProgress = "No device detected";
		else
			mKinectProgress = "Recording not found";
		return;
	}

	if ( path.empty() )
		mKinectProgress = "Connected";
	else
		mKinectProgress = "Recording loaded";

	{
		std::lock_guard< std::mutex > lock( mKinectMutex );
		mNI.setDepthAligned();
		mNI.start();
		mNIUserTracker = mNI.getUserTracker();
		mNIUserTracker.addListener( this );
	}
}

void DynaApp::setPoseTimeline()
{
	// pose anim
	mPoseAnimTimeline->clear();
	mPoseAnimTimeline->setDefaultAutoRemove( false );
	mPoseAnimTimeline->apply( &mPoseAnimOpacity, 0.f, 0.f, 4.f );
	mPoseAnimTimeline->appendTo( &mPoseAnimOpacity, 0.f, 1.f, .9f );

	mPoseAnimTimeline->apply( &mPoseAnimFrame, 0, 0, 5.f );
	mPoseAnimTimeline->appendTo( &mPoseAnimFrame, 0, (int)(sPoseAnim.size() - 1), .9f );

	mPoseAnimTimeline->appendTo( &mPoseAnimOpacity, 1.f, 1.f, 2.f );
	mPoseAnimTimeline->appendTo( &mPoseAnimOpacity, 1.f, 0.f, 1.f );
	mPoseAnimTimeline->appendTo( &mPoseAnimOpacity, 0.f, 0.f, 1.f );
	mPoseAnimTimeline->setLoop( true );
	timeline().add( mPoseAnimTimeline );
}

void DynaApp::shutdown()
{
	params::PInterfaceGl::save();

	mKinectThread.join();

	mScreenshotThreadShouldQuit = true;
	mScreenshotThread.join();
}

void DynaApp::sendScreenshot()
{
	// flash
	mFlash = 0;
	mGameTimeline->apply( &mFlash, .9f, .2f, EaseOutQuad() );
	mGameTimeline->appendTo( &mFlash, .0f, .4f, EaseInQuad() );

	audio::Output::play( mAudioShutter );

	Surface snapshot( mOutputFbo.getTexture() );
	mScreenshotSurfaces->pushFront( snapshot );
}

void DynaApp::screenshotThreadFn()
{
	while ( !mScreenshotThreadShouldQuit )
	{
		if ( mScreenshotSurfaces->isNotEmpty() )
		{
			Surface snapshot;
			mScreenshotSurfaces->popBack( &snapshot );

			string filename = "snap-" + timeStamp() + ".png";
			fs::path pngPath( mScreenshotFolder / fs::path( filename ) );

			try
			{
				if (!pngPath.empty())
				{
					writeImage( pngPath, snapshot );

					{
						lock_guard<recursive_mutex> lock( mMutex );
						mNewImages.push_back( snapshot );
					}
				}
			}
			catch ( ... )
			{
				console() << "unable to save image file " << pngPath << endl;
			}

			// watermarked
			Surface snapshotw = snapshot.clone();
			snapshotw.copyFrom( mWatermark, mWatermark.getBounds(),
						snapshotw.getSize() - mWatermark.getSize() );
			fs::path pngPath2 = mWatermarkedPath / fs::path( "w" + filename );
			try
			{
				if (!pngPath2.empty())
				{
					writeImage( pngPath2, snapshotw );
				}
			}
			catch ( ... )
			{
				console() << "unable to save image file " << pngPath2 << endl;
			}
		}
	}
}

void DynaApp::newUser( mndl::ni::UserTracker::UserEvent event )
{
	console() << "app new " << event.id << endl;
}

void DynaApp::calibrationEnd( mndl::ni::UserTracker::UserEvent event )
{
	console() << "app calib end " << event.id << endl;

	map< unsigned, UserInit >::iterator it;
	it = mUserInitialized.find( event.id );
	if ( it == mUserInitialized.end() )
		mUserInitialized[ event.id ] = UserInit();
}

void DynaApp::lostUser( mndl::ni::UserTracker::UserEvent event )
{
	console() << "app lost " << event.id << endl;
	mUserStrokes.erase( event.id );
	mUserInitialized.erase( event.id );
}

void DynaApp::resize()
{
	/*
	mFluidSolver.setSize( sFluidSizeX, sFluidSizeX / event.getAspectRatio() );
	mFluidDrawer.setup( &mFluidSolver );
	mParticles.setWindowSize( event.getSize() );
	*/
}

void DynaApp::clearStrokes()
{
	mUserStrokes.clear();
	mDynaStrokes.clear();
}

void DynaApp::setIdleState()
{
	mGallery->reset();
	mState = STATE_IDLE;
}

void DynaApp::endGame()
{
	mState = STATE_GAME_SHOW_DRAWING;

	sendScreenshot();
	clearStrokes();

	// wait a bit then go back to idle state
	// TODO:: bind with function parameter
	mGameTimeline->add( std::bind( &DynaApp::setIdleState, this ), timeline().getCurrentTime() + 1.5 );
	mPoseAnimOpacity = 0;
}

void DynaApp::keyDown( KeyEvent event )
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

		case KeyEvent::KEY_SPACE:
			clearStrokes();
			break;

		case KeyEvent::KEY_RETURN:
			sendScreenshot();
			break;

		case KeyEvent::KEY_ESCAPE:
			quit();
			break;

		default:
			break;
	}
}

void DynaApp::addToFluid( Vec2f pos, Vec2f vel, bool addParticles, bool addForce )
{
	// balance the x and y components of speed with the screen aspect ratio
	float speed = vel.x * vel.x +
		vel.y * vel.y * getWindowAspectRatio() * getWindowAspectRatio();

	if ( speed > 0 )
	{
		pos.x = constrain( pos.x, 0.0f, 1.0f );
		pos.y = constrain( pos.y, 0.0f, 1.0f );

		const float velocityMult = 30;

		if ( addParticles )
		{
			int count = static_cast<int>(
						lmap<float>( vel.length() * mVelParticleMult * getWindowWidth(),
									 mVelParticleMin, mVelParticleMax,
									 mParticleMin, mParticleMax ) );
			if (count > 0)
			{
				mParticles.addParticle( pos * Vec2f( mFbo.getSize() ), count);
			}
		}
		if ( addForce )
			mFluidSolver.addForceAtPos( pos, vel * velocityMult );
	}
}

void DynaApp::mouseDown(MouseEvent event)
{
	if (event.isLeft())
	{
		mDynaStrokes.push_back( DynaStroke( sBrushes[ Rand::randInt( 0, sBrushes.size() ) ] ) );
		DynaStroke *d = &mDynaStrokes.back();
		d->resize( mFbo.getSize() );
		d->setStiffness( mK );
		d->setDamping( mDamping );
		d->setStrokeMinWidth( mStrokeMinWidth );
		d->setStrokeMaxWidth( mStrokeMaxWidth );
		d->setMaxVelocity( mMaxVelocity );

		mLeftButton = true;
		mMousePos = event.getPos();
	}
}

void DynaApp::mouseDrag(MouseEvent event)
{
	if (event.isLeftDown())
	{
		mMousePos = event.getPos();

		Vec2f mouseNorm = Vec2f( mMousePos ) / getWindowSize();
		Vec2f mouseVel = Vec2f( mMousePos - mPrevMousePos ) / getWindowSize();
		addToFluid( mouseNorm, mouseVel, true, true );

		mPrevMousePos = mMousePos;
	}
}

void DynaApp::mouseUp(MouseEvent event)
{
	if (event.isLeft())
	{
		mLeftButton = false;
		mMousePos = mPrevMousePos = event.getPos();
	}
}

void DynaApp::update()
{
	static double poseLostStart = 0;

	mFps = getAverageFps();

	// screenshot folder updated
	if ( mScreenshotPath != mScreenshotFolder )
	{
		mScreenshotPath = mScreenshotFolder;
		mGallery->setFolder( mScreenshotPath );
	}

	if ( mLeftButton && !mDynaStrokes.empty() )
		mDynaStrokes.back().update( Vec2f( mMousePos ) / getWindowSize() );

	{
		std::lock_guard< std::mutex > lock( mKinectMutex );

		if ( mNI )
		{
			mNIUserTracker.setSmoothing( mSkeletonSmoothing );
			if ( mNI.isMirrored() != mVideoMirrored )
				mNI.setMirrored( mVideoMirrored );

			double currentTime = getElapsedSeconds();

			// detect start gesture
			vector< unsigned > users = mNIUserTracker.getUsers();
			mPoseHoldDuration = 0;
			for ( vector< unsigned >::const_iterator it = users.begin();
					it < users.end(); ++it )
			{
				unsigned id = *it;

				map< unsigned, UserInit >::iterator initIt = mUserInitialized.find( id );
				if ( initIt != mUserInitialized.end() )
				{
					UserInit *ui = &(initIt->second);

					if (!ui->mRecognized)
					{
						XnSkeletonJoint jointIds[] = { XN_SKEL_LEFT_HAND,
							XN_SKEL_RIGHT_HAND };

						XnSkeletonJoint limitIds[] = { XN_SKEL_LEFT_SHOULDER,
							XN_SKEL_RIGHT_SHOULDER };

						bool initPoseAll = true;
						for ( int i = 0; i < UserInit::JOINTS; i++ )
						{
							float handConf;
							Vec2f hand = mNIUserTracker.getJoint2d( id, jointIds[i], &handConf );
							float limitConf;
							Vec2f limit = mNIUserTracker.getJoint2d( id, limitIds[i], &limitConf );

							//console() << i << " " << hand << " [" << handConf << "] " << limit << " [" << limitConf << "]" << endl;
							bool initPose = (handConf > .5) && (limitConf > .5) &&
								(hand.y < limit.y) && (ui->mJointMovement[i].calcArea() <= mPoseHoldAreaThr );
							initPoseAll &= initPose;
							if (initPose)
							{
								if (ui->mPoseTimeStart[i] < 0)
									ui->mPoseTimeStart[i] = currentTime;

								if (ui->mJointMovement[i].calcArea() == 0)
								{
									ui->mJointMovement[i] = Rectf( hand, hand + Vec2f( 1, 1 ) );
								}
								else
								{
									ui->mJointMovement[i].include( hand );
								}
							}
							else
							{
								// reset if one hand is lost
								ui->reset();
							}
							//console() << i << " " << initPose << " " << ui->mPoseTimeStart[i] << endl;
						}

						if ( initPoseAll )
							poseLostStart = 0;

						bool init = true;
						float userPoseHoldDuration = 0;
						for ( int i = 0; i < UserInit::JOINTS; i++ )
						{
							init = init && ( ui->mPoseTimeStart[i] > 0 ) &&
								( ( currentTime - ui->mPoseTimeStart[i] ) >= mPoseDuration );

							if (ui->mPoseTimeStart[ i ] > 0)
								userPoseHoldDuration += currentTime - ui->mPoseTimeStart[ i ];
						}

						userPoseHoldDuration /= UserInit::JOINTS;
						if ( mPoseHoldDuration < userPoseHoldDuration)
							mPoseHoldDuration = userPoseHoldDuration;

						ui->mRecognized = init;

						if ( ui->mRecognized )
						{
							// init gesture found clear screen and strokes
							clearStrokes();

							// new brush
							ui->setBrush( ui->mBrushIndex + 1 );

							mState = STATE_GAME;
							// clear pose start
							ui->reset();
							mPoseHoldDuration = 0;

							// add callback when game time ends
							mGameTimer = mGameDuration;
							mFlash = 0;
							mGameTimeline->clear(); // clear old callbacks
							mGameTimeline->apply( &mGameTimer, .0f, mGameDuration ).finishFn( std::bind( &DynaApp::endGame, this ) );
						}
					}
				}
				else /* users were cleared */
				{
				}
				//console() << "id: " << id << " " << mUserInitialized[id].mRecognized << endl;
			}

			// state change
			if ( ( mState == STATE_IDLE ) && ( mPoseHoldDuration > .1 ) )
			{
				mState = STATE_IDLE_POSE;
			}
			else
				if ( ( mState == STATE_GAME ) && ( mPoseHoldDuration > 1. ) )
				{
					mState = STATE_GAME_POSE;
					// clear all user pose start times
					double currentTime = getElapsedSeconds();
					map< unsigned, UserInit >::iterator initIt;
					for ( initIt = mUserInitialized.begin(); initIt != mUserInitialized.end(); ++initIt )
					{
						UserInit *ui = &(initIt->second);
						for ( int i = 0; i < UserInit::JOINTS; i++ )
						{
							ui->mPoseTimeStart[ i ] = currentTime;
						}
					}
				}

			// change state when pose is cancelled
			if ( ( ( mState == STATE_GAME_POSE ) || ( mState == STATE_IDLE_POSE ) ) &&
					( mPoseHoldDuration <= 0 ) )
			{
				if ( poseLostStart == 0 )
				{
					poseLostStart = currentTime;
				}
				else
					if ( ( currentTime - poseLostStart ) > .5 )
					{
						if ( mGameTimer > .0 )
							mState = STATE_GAME;
						else
							mState = STATE_IDLE;
					}
			}

			// NI user hands
			mHandCursors.clear();
			for ( vector< unsigned >::const_iterator it = users.begin();
					it < users.end(); ++it )
			{
				unsigned id = *it;

				//console() << "user hands " << id << " " << mUserInitialized[ id ].mInitialized << endl;
				map< unsigned, UserStrokes >::iterator strokeIt = mUserStrokes.find( id );
				map< unsigned, UserInit >::iterator initIt = mUserInitialized.find( id );
				UserInit *ui = &(initIt->second);

				// check if the user has strokes already
				if ( strokeIt != mUserStrokes.end() )
				{
					UserStrokes *us = &(strokeIt->second);

					XnSkeletonJoint jointIds[] = { XN_SKEL_LEFT_HAND,
						XN_SKEL_RIGHT_HAND };
					for ( int i = 0; i < UserStrokes::JOINTS; i++ )
					{
						Vec2f hand = mNIUserTracker.getJoint2d( id, jointIds[i] );
						Vec3f hand3d = mNIUserTracker.getJoint3d( id, jointIds[i] );
						us->mActive[i] = (hand3d.z < mZClip) && (hand3d.z > 0);

						mHandCursors.push_back( HandCursor( i, hand / Vec2f( 640, 480 ), hand3d.z ) );

						if (us->mActive[i] && !us->mPrevActive[i])
						{
							mDynaStrokes.push_back( DynaStroke( ui->mBrush ) );
							DynaStroke *d = &mDynaStrokes.back();
							d->resize( mFbo.getSize() );
							d->setStiffness( mK );
							d->setDamping( mDamping );
							d->setStrokeMinWidth( mStrokeMinWidth );
							d->setStrokeMaxWidth( mStrokeMaxWidth );
							d->setMaxVelocity( mMaxVelocity );
							us->mStrokes[i] = d;
						}

						if (us->mActive[i])
						{
							hand /= Vec2f( 640, 480 );
							us->mStrokes[i]->update( hand );
							if (us->mPrevActive[i])
							{
								Vec2f vel = Vec2f( hand - us->mHand[i] );
								addToFluid( hand, vel, true, true );
							}
							us->mHand[i] = hand;
						}
						us->mPrevActive[i] = us->mActive[i];
					}
				}
				else
				{
					// if the user has been initialized with start gesture
					if ( mUserInitialized[ id ].mRecognized )
					{
						mUserStrokes[ id ] = UserStrokes();
						mUserInitialized[ id ].mRecognized = false;
					}
				}
			}

			// kinect textures
			if ( mNI.checkNewVideoFrame() )
				mColorTexture = mNI.getVideoImage();
			if ( mNI.checkNewDepthFrame() )
				mDepthTexture = mNI.getDepthImage();
		}
	}


	// fluid & particles
	mFluidSolver.update();

	mParticles.setAging( 0.9 );
	mParticles.update( getElapsedSeconds() );

	// add new images saved from thread to gallery
	{
		lock_guard<recursive_mutex> lock( mMutex );
		for ( auto it = mNewImages.begin(); it != mNewImages.end(); ++it )
		{
			int toPic = -1;
			if ( it == mNewImages.end() - 1 )
			{
				toPic = Rand::randInt( 0, mGallery->getSize() );
				mGallery->zoomImage( toPic );
			}
			mGallery->addImage( *it, toPic );
		}
		mNewImages.clear();
	}
	mGallery->update();
}

void DynaApp::drawGallery()
{
	static ci::Anim< float > poseOpacity;

	gl::clear( Color::black() );

	gl::setMatricesWindow( getWindowSize() );
	gl::setViewport( getWindowBounds() );

	mGallery->setNoiseFreq( exp( mVideoNoiseFreq ) );
	mGallery->enableVignetting( mEnableVignetting );
	mGallery->enableTvLines( mEnableTvLines );

	mGallery->render( getWindowBounds() );

	// pose anim
	gl::enableAlphaBlending();
	gl::color( ColorA( 1., 1., 1., mPoseAnimOpacity ) );

	Rectf animRect( sPoseAnim[ 0 ].getBounds() );
	animRect = animRect.getCenteredFit( getWindowBounds(), true );
	animRect.scaleCentered( .5 );
	gl::draw( sPoseAnim[ mPoseAnimFrame ], animRect );
	gl::color( Color::white() );
	gl::disableAlphaBlending();
}

void DynaApp::drawGame()
{
	if ( mState != STATE_GAME_SHOW_DRAWING )
	{
		mFbo.bindFramebuffer();
		gl::setMatricesWindow( mFbo.getSize(), false );
		gl::setViewport( mFbo.getBounds() );

		gl::clear( Color::black() );

		gl::color( Color::gray( mBrushColor ) );

		gl::enableAlphaBlending();
		gl::enable( GL_TEXTURE_2D );
		for (list< DynaStroke >::iterator i = mDynaStrokes.begin(); i != mDynaStrokes.end(); ++i)
		{
			i->draw();
		}
		gl::disableAlphaBlending();
		gl::disable( GL_TEXTURE_2D );

		mParticles.draw();

		mFbo.unbindFramebuffer();

		// bloom
		mBloomFbo.bindFramebuffer();

		gl::setMatricesWindow( mBloomFbo.getSize(), false );
		gl::setViewport( mBloomFbo.getBounds() );

		gl::color( Color::white() );
		mFbo.getTexture().bind();

		mBloomShader.bind();
		for (int i = 0; i < mBloomIterations; i++)
		{
			glDrawBuffer( GL_COLOR_ATTACHMENT0_EXT + i );
			mBloomShader.uniform( "iteration", i );
			gl::drawSolidRect( mBloomFbo.getBounds() );
			mBloomFbo.bindTexture( 0, i );
		}
		mBloomShader.unbind();

		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		mBloomFbo.unbindFramebuffer();

		// final
		mOutputFbo.bindFramebuffer();
		gl::setMatricesWindow( mOutputFbo.getSize(), false );
		gl::setViewport( mOutputFbo.getBounds() );

		gl::enableAlphaBlending();

		mMixerShader.bind();
		mMixerShader.uniform( "mixOpacity", mVideoOpacity );
		mMixerShader.uniform( "flash", mFlash );
		mMixerShader.uniform( "randSeed", static_cast< float >( getElapsedSeconds() ) );
		mMixerShader.uniform( "randFreqMultiplier", exp( mVideoNoiseFreq ) );
		mMixerShader.uniform( "noiseStrength", mVideoNoise );
		mMixerShader.uniform( "enableVignetting", mEnableVignetting );
		mMixerShader.uniform( "enableTvLines", mEnableTvLines );

		gl::enable( GL_TEXTURE_2D );
		if ( mColorTexture )
			mColorTexture.bind( 0 );

		mFbo.getTexture().bind( 1 );
		for (int i = 1; i < mBloomIterations; i++)
		{
			mBloomFbo.getTexture( i ).bind( i + 1 );
		}

		gl::drawSolidRect( mOutputFbo.getBounds() );
		gl::disable( GL_TEXTURE_2D );
		mMixerShader.unbind();

		mOutputFbo.unbindFramebuffer();

	}

	// draw output to window
	gl::setMatricesWindow( getWindowSize() );
	gl::setViewport( getWindowBounds() );

	gl::Texture outputTexture = mOutputFbo.getTexture();
	Rectf outputRect( mOutputFbo.getBounds() );
	Rectf screenRect( getWindowBounds() );
	outputRect = outputRect.getCenteredFit( screenRect, true );
	if ( screenRect.getAspectRatio() > outputRect.getAspectRatio() )
		outputRect.scaleCentered( screenRect.getWidth() / outputRect.getWidth() );
	else
		outputRect.scaleCentered( screenRect.getHeight() / outputRect.getHeight() );

	gl::draw( outputTexture, outputRect );

	if ( mState == STATE_GAME_SHOW_DRAWING )
	{
		gl::enableAdditiveBlending();
		gl::color( ColorA( 1, 1, 1, mFlash ) );
		gl::drawSolidRect( getWindowBounds() );
		gl::color( Color::white() );
		gl::disableAlphaBlending();
	}

	// cursors
	if ( mShowHands && ( mState != STATE_GAME_SHOW_DRAWING ) )
	{
		gl::enable( GL_TEXTURE_2D );
		gl::enableAlphaBlending();

		HandCursor::setBounds( outputRect );
		HandCursor::setZClip( mZClip );
		HandCursor::setPosCoeff( mHandPosCoeff );
		HandCursor::setTransparencyCoeff( mHandTransparencyCoeff );
		for ( vector< HandCursor >::iterator it = mHandCursors.begin();
				it != mHandCursors.end(); ++it )
		{
			it->draw();
		}

		gl::disableAlphaBlending();
		gl::disable( GL_TEXTURE_2D );
	}
}

void DynaApp::draw()
{
	gl::clear( Color::black() );

	switch ( mState )
	{
		case STATE_IDLE:
		case STATE_IDLE_POSE:
			drawGallery();
			break;

		case STATE_GAME:
		case STATE_GAME_POSE:
		case STATE_GAME_SHOW_DRAWING:
			drawGame();
			break;
	}

	gl::enableAlphaBlending();
	switch ( mState )
	{
		case STATE_IDLE_POSE:
		case STATE_GAME_POSE:
			mPoseTimerDisplay.draw( mPoseHoldDuration / mPoseDuration );
			break;

		case STATE_GAME:
			mGameTimerDisplay.draw( 1. - mGameTimer / mGameDuration );
			break;
	}
	gl::disableAlphaBlending();

	params::InterfaceGl::draw();
}

CINDER_APP_BASIC(DynaApp, RendererGl( RendererGl::AA_NONE ))

