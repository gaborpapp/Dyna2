#pragma once
#include <vector>

#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"

#include "cinder/Area.h"
#include "cinder/Cinder.h"
#include "cinder/Filesystem.h"
#include "cinder/Rect.h"
#include "cinder/Timeline.h"

#include "PParams.h"

typedef std::shared_ptr< class Gallery > GalleryRef;

class Gallery : public std::enable_shared_from_this< Gallery >
{
	public:
		void setup( ci::fs::path &folder, int rows = 3, int columns = 4 );

		static GalleryRef create( ci::fs::path &folder, int rows = 3, int columns = 4 )
		{
			GalleryRef gallery( new Gallery() );
			gallery->setup( folder, rows, columns );
			return gallery;
		}

		void resize( int rows, int columns);
		void refreshList();
		void refreshPictures();
		void setFolder( ci::fs::path &folder );
		const ci::fs::path &getFolder() const { return mGalleryFolder; }

		void addImage( ci::fs::path imagePath, int pictureIndex = -1 );
		void zoomImage( int pictureIndex );

		void reset();
		void update();
		void render( const ci::Area &area );

		int getSize() { return mRows * mColumns; }
		int getWidth() { return mColumns; }
		int getHeight() { return mRows; }

		class Picture
		{
			public:
				Picture( GalleryRef g );

				void reset();
				void render( const ci::Rectf &rect );

				void flip();
				bool isFlipping() { return flipping; }
				bool isZooming() { return zooming; }

				void setTexture( ci::gl::Texture &texture ) { mTexture = texture; }
				void startZoom();

			private:
				void setRandomTexture();

				ci::gl::Texture mTexture;
				GalleryRef mGallery;

				double flipStart;
				bool flipping;
				bool flipTextureChanged;
				static float sFlipDuration;

				double appearanceTime; // before this time, the picture is hidden
				bool zooming; // picture is zoomed out
				ci::Anim< float > mZoom;

				friend class Gallery;
		};

		void setHorizontalMargin( float v ) { mHorizontalMargin = v; }
		void setVerticalMargin( float v ) { mVerticalMargin = v; }
		void setCellHorizontalSpacing( float v ) { mCellHorizontalSpacing = v; }
		void setCellVerticalSpacing( float v ) { mCellVerticalSpacing = v; }

		void setNoiseFreq( float v ) { mRandFreqMultiplier = v; }
		void enableTvLines( bool enable = true ) { mEnableTvLines = enable; }
		void enableVignetting( bool enable = true ) { mEnableVignetting = enable; }

	private:
		ci::params::PInterfaceGl mParams;

		ci::fs::path mGalleryFolder;
		std::vector< ci::fs::path > mFiles;
		std::vector< ci::gl::Texture > mTextures;
		int mRows, mLastRows = -1;
		int mMaxTextures;
		int mColumns, mLastColumns = -1;

		float mHorizontalMargin;
		float mVerticalMargin;
		float mCellHorizontalSpacing;
		float mCellVerticalSpacing;
		float mRandFreqMultiplier;
		bool mEnableTvLines;
		bool mEnableVignetting;
		float mFlipFrequency;
		double mLastFlip;

		std::vector< Picture > mPictures;

		ci::gl::GlslProg mGalleryShader;

		ci::TimelineRef mTimeline;
};

