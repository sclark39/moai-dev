// Copyright (c) 2010-2011 Zipline Games, Inc. All Rights Reserved.
// http://getmoai.com

#include "pch.h"
#include <moai-sim/MOAIFrameBufferTexture.h>
#include <moai-sim/MOAIGfxDevice.h>
#include <moai-sim/MOAIImage.h>
#include <moai-sim/MOAIPvrHeader.h>
#include <moai-sim/MOAISim.h>
#include <moai-sim/MOAITextureBase.h>
#include <moai-sim/MOAIMultiTexture.h>

//================================================================//
// local
//================================================================//

//----------------------------------------------------------------//
/**	@name	getSize
	@text	Returns the width and height of the texture's source image.
			Avoid using the texture width and height to compute UV
			coordinates from pixels, as this will prevent texture
			resolution swapping.
	
	@in		MOAITextureBase self
	@out	number width
	@out	number height
*/
int MOAITextureBase::_getSize ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAITextureBase, "U" )
	
	lua_pushnumber ( state, self->mWidth );
	lua_pushnumber ( state, self->mHeight );
	
	return 2;
}

//----------------------------------------------------------------//
/**	@name	release
	@text	Releases any memory associated with the texture.
	
	@in		MOAITextureBase self
	@out	nil
*/
int MOAITextureBase::_release ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAITextureBase, "U" )
	
	self->MOAITextureBase::Clear ();
	
	return 0;
}

//----------------------------------------------------------------//
/**	@name	setFilter
	@text	Set default filtering mode for texture.
	
	@in		MOAITextureBase self
	@in		number min			One of MOAITextureBase.GL_LINEAR, MOAITextureBase.GL_LINEAR_MIPMAP_LINEAR, MOAITextureBase.GL_LINEAR_MIPMAP_NEAREST,
								MOAITextureBase.GL_NEAREST, MOAITextureBase.GL_NEAREST_MIPMAP_LINEAR, MOAITextureBase.GL_NEAREST_MIPMAP_NEAREST
	@opt	number mag			Defaults to value passed to 'min'.
	@out	nil
*/
int MOAITextureBase::_setFilter ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAITextureBase, "UN" )

	int min = state.GetValue < int >( 2, ZGL_SAMPLE_LINEAR );
	int mag = state.GetValue < int >( 3, min );

	self->SetFilter ( min, mag );

	return 0;
}

//----------------------------------------------------------------//
/**	@name	setWrap
	@text	Set wrapping mode for texture.
	
	@in		MOAITextureBase self
	@in		boolean wrap		Texture will wrap if true, clamp if not.
	@out	nil
*/
int MOAITextureBase::_setWrap ( lua_State* L ) {
	MOAI_LUA_SETUP ( MOAITextureBase, "UB" )
	
	bool wrap = state.GetValue < bool >( 2, false );
	
	self->mWrap = wrap ? ZGL_WRAP_MODE_REPEAT : ZGL_WRAP_MODE_CLAMP;

	return 0;
}

//================================================================//
// MOAITextureBase
//================================================================//

//----------------------------------------------------------------//
void MOAITextureBase::CreateTextureFromImage ( MOAIImage& image ) {

	bool error = false;
	if ( !image.IsOK ()) return;
	if ( !MOAIGfxDevice::Get ().GetHasContext ()) return;

	MOAIGfxDevice::Get ().ClearErrors ();

	// get the dimensions before trying to get the OpenGL texture ID
	this->mWidth = image.GetWidth ();
	this->mHeight = image.GetHeight ();

	// warn if not a power of two
	if ( !image.IsPow2 ()) {
		MOAILog ( 0, MOAILogMessages::MOAITexture_NonPowerOfTwo_SDD, ( cc8* )this->mDebugName, this->mWidth, this->mHeight );
	}

	this->mGLTexID = zglCreateTexture ();
	if ( !this->mGLTexID ) return;

	zglBindTexture ( this->mGLTexID );

	USPixel::Format pixelFormat = image.GetPixelFormat ();
	ZLColor::Format colorFormat = image.GetColorFormat ();

	if ( pixelFormat != USPixel::TRUECOLOR ) return; // only support truecolor textures

	// generate mipmaps if set up to use them
	bool genMipMaps = (
		( this->mMinFilter == ZGL_SAMPLE_LINEAR_MIPMAP_LINEAR ) ||
		( this->mMinFilter == ZGL_SAMPLE_LINEAR_MIPMAP_NEAREST ) ||
		( this->mMinFilter == ZGL_SAMPLE_NEAREST_MIPMAP_LINEAR ) ||
		( this->mMinFilter == ZGL_SAMPLE_NEAREST_MIPMAP_NEAREST )
	);

	// GL_ALPHA
	// GL_RGB
	// GL_RGBA
	// GL_LUMINANCE
	// GL_LUMINANCE_ALPHA

	// GL_UNSIGNED_BYTE
	// GL_UNSIGNED_SHORT_5_6_5
	// GL_UNSIGNED_SHORT_4_4_4_4
	// GL_UNSIGNED_SHORT_5_5_5_1

	switch ( colorFormat ) {
		
		case ZLColor::A_8:
			this->mGLInternalFormat = ZGL_PIXEL_FORMAT_ALPHA;
			this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
			break;
		
		case ZLColor::RGB_888:
			this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGB;
			this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
			break;
		
		case ZLColor::RGB_565:
			this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGB;
			this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_SHORT_5_6_5;
			break;
		
		case ZLColor::RGBA_4444:
			this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGBA;
			this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_SHORT_4_4_4_4;
			break;
		
		case ZLColor::RGBA_8888:
			this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGBA;
			this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
			break;
			
		default: return;
	}

	zglTexImage2D (
		0,
		this->mGLInternalFormat,
		this->mWidth,  
		this->mHeight,  
		this->mGLInternalFormat,
		this->mGLPixelType,  
		image.GetBitmap ()
	);
	
	this->mTextureSize = image.GetBitmapSize ();
	
	if ( MOAIGfxDevice::Get ().LogErrors ()) {
		error = true;
	}
	else if ( genMipMaps ) {
	
		u32 mipLevel = 1;
		
		MOAIImage mipmap;
		mipmap.Copy ( image );
		
		while ( mipmap.MipReduce ()) {
			
			zglTexImage2D (
				mipLevel++,  
				this->mGLInternalFormat,
				mipmap.GetWidth (),  
				mipmap.GetHeight (),  
				this->mGLInternalFormat,
				this->mGLPixelType,  
				mipmap.GetBitmap ()
			);
			
			if ( MOAIGfxDevice::Get ().LogErrors ()) {
				error = true;
				break;
			}
			this->mTextureSize += mipmap.GetBitmapSize ();
		}
	}
	
	if ( error ) {
		this->mTextureSize = 0;
		zglDeleteTexture ( this->mGLTexID );
		this->mGLTexID = 0;
		this->Clear ();
		return;
	}
	
	if ( this->mGLTexID ) {
		MOAIGfxDevice::Get ().ReportTextureAlloc ( this->mDebugName, this->mTextureSize );
		this->mIsDirty = true;
	}
}

//----------------------------------------------------------------//
void MOAITextureBase::CreateTextureFromPVR ( void* data, size_t size ) {
	UNUSED ( data );
	UNUSED ( size );

	#ifdef MOAI_OS_IPHONE

		if ( !MOAIGfxDevice::Get ().GetHasContext ()) return;
		MOAIGfxDevice::Get ().ClearErrors ();

		MOAIPvrHeader* header = MOAIPvrHeader::GetHeader ( data, size );
		if ( !header ) return;
		
		bool compressed = false;
		bool hasAlpha = header->mAlphaBitMask ? true : false;
		
		switch ( header->mPFFlags & MOAIPvrHeader::PF_MASK ) {
			
			case MOAIPvrHeader::OGL_RGBA_4444:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGBA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_SHORT_4_4_4_4;
				break;
		
			case MOAIPvrHeader::OGL_RGBA_5551:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGBA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_SHORT_5_5_5_1;
				break;
			
			case MOAIPvrHeader::OGL_RGBA_8888:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGBA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
			
			case MOAIPvrHeader::OGL_RGB_565:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGB;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_SHORT_5_6_5;
				break;
			
			// NO IMAGE FOR THIS
//			case MOAIPvrHeader::OGL_RGB_555:
//				break;
			
			case MOAIPvrHeader::OGL_RGB_888:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_RGB;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
			
			case MOAIPvrHeader::OGL_I_8:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_LUMINANCE;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
			
			case MOAIPvrHeader::OGL_AI_88:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_LUMINANCE_ALPHA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
			
			case MOAIPvrHeader::OGL_PVRTC2:
				compressed = true;
				this->mGLInternalFormat = hasAlpha ?
					ZGL_PIXEL_TYPE_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG :
					ZGL_PIXEL_TYPE_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
				break;
			
			case MOAIPvrHeader::OGL_PVRTC4:
				compressed = true;
				this->mGLInternalFormat = hasAlpha ?
					ZGL_PIXEL_TYPE_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG :
					ZGL_PIXEL_TYPE_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
				break;
			
			case MOAIPvrHeader::OGL_BGRA_8888:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_BGRA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
			
			case MOAIPvrHeader::OGL_A_8:
				compressed = false;
				this->mGLInternalFormat = ZGL_PIXEL_FORMAT_ALPHA;
				this->mGLPixelType = ZGL_PIXEL_TYPE_UNSIGNED_BYTE;
				break;
		}
		
		
		this->mGLTexID = zglCreateTexture ();
		if ( !this->mGLTexID ) return;

		zglBindTexture ( this->mGLTexID );
		
		this->mTextureSize = 0;
		
		int width = header->mWidth;
		int height = header->mHeight;
		char* imageData = (char*)(header->GetFileData ( data, size));
		if ( header->mMipMapCount == 0 ) {
			
			u32 currentSize = std::max ( 32u, width * height * header->mBitCount / 8u );
			this->mTextureSize += currentSize;
			
			if ( compressed ) {
				zglCompressedTexImage2D ( 0, this->mGLInternalFormat, width, height, currentSize, imageData );
			}
			else {
				zglTexImage2D( 0, this->mGLInternalFormat, width, height, this->mGLInternalFormat, this->mGLPixelType, imageData );
			}
			
			if ( zglGetError () != ZGL_ERROR_NONE ) {
				this->Clear ();
				return;
			}
		}
		else {
			for ( int level = 0; width > 0 && height > 0; ++level ) {
				u32 currentSize = std::max ( 32u, width * height * header->mBitCount / 8u );
			
				if ( compressed ) {
					zglCompressedTexImage2D ( level, this->mGLInternalFormat, width, height, currentSize, imageData );
				}
				else {
					zglTexImage2D( level, this->mGLInternalFormat, width, height, this->mGLInternalFormat, this->mGLPixelType, imageData );
				}
				
				if ( zglGetError () != ZGL_ERROR_NONE ) {
					this->Clear ();
					return;
				}
				
				imageData += currentSize;
				this->mTextureSize += currentSize;
				
				width >>= 1;
				height >>= 1;
			}	
		}			

		if ( this->mGLTexID ) {
			MOAIGfxDevice::Get ().ReportTextureAlloc ( this->mDebugName, this->mTextureSize );
			this->mIsDirty = true;
		}

	#endif
}

//----------------------------------------------------------------//
u32 MOAITextureBase::GetHeight () {
	return this->mHeight;
}

//----------------------------------------------------------------//
u32 MOAITextureBase::GetWidth () {
	return this->mWidth;
}

//----------------------------------------------------------------//
bool MOAITextureBase::IsRenewable () {

	return false;
}

//----------------------------------------------------------------//
bool MOAITextureBase::IsValid () {

	return ( this->mGLTexID != 0 );
}

//----------------------------------------------------------------//
bool MOAITextureBase::LoadGfxState () {

	return MOAIGfxDevice::Get ().SetTexture ( this );
}

//----------------------------------------------------------------//
MOAITextureBase::MOAITextureBase () :
	mGLTexID ( 0 ),
	mWidth ( 0 ),
	mHeight ( 0 ),
	mMinFilter ( ZGL_SAMPLE_LINEAR ),
	mMagFilter ( ZGL_SAMPLE_NEAREST ),
	mWrap ( ZGL_WRAP_MODE_CLAMP ),
	mTextureSize ( 0 ),
	mIsDirty ( false ) {
	
	RTTI_BEGIN
		RTTI_EXTEND ( MOAILuaObject )
		RTTI_EXTEND ( MOAIGfxResource )
	RTTI_END
}

//----------------------------------------------------------------//
MOAITextureBase::~MOAITextureBase () {

	this->Clear ();
}

//----------------------------------------------------------------//
void MOAITextureBase::OnBind () {

	if ( !this->mGLTexID ) return;

	zglBindTexture ( this->mGLTexID );
	
	if ( this->mIsDirty ) {
	
		#if USE_OPENGLES1	
			if ( !MOAIGfxDevice::Get ().IsProgrammable ()) {
				zglTexEnvi ( ZGL_TEXTURE_ENV_MODE, ZGL_COMPOSE_MODULATE );
			}
		#endif
		
		zglTexParameteri ( ZGL_TEXTURE_WRAP_S, this->mWrap );
		zglTexParameteri ( ZGL_TEXTURE_WRAP_T, this->mWrap );
		
		zglTexParameteri ( ZGL_TEXTURE_MIN_FILTER, this->mMinFilter );
		zglTexParameteri ( ZGL_TEXTURE_MAG_FILTER, this->mMagFilter );
		
		this->mIsDirty = false;
	}
}

//----------------------------------------------------------------//
void MOAITextureBase::OnClear () {
	
	this->mWidth = 0;
	this->mHeight = 0;
}

//----------------------------------------------------------------//
void MOAITextureBase::OnDestroy () {

	if ( this->mGLTexID ) {
		
		MOAIGfxDevice::Get ().ReportTextureFree ( this->mDebugName, this->mTextureSize );
		MOAIGfxDevice::Get ().PushDeleter ( MOAIGfxDeleter::DELETE_TEXTURE, this->mGLTexID );
		this->mGLTexID = 0;
	}
}

//----------------------------------------------------------------//
void MOAITextureBase::OnInvalidate () {

	this->mGLTexID = 0;
}

//----------------------------------------------------------------//
void MOAITextureBase::RegisterLuaClass ( MOAILuaState& state ) {
	
	MOAIGfxResource::RegisterLuaClass ( state );
	
	state.SetField ( -1, "GL_LINEAR",					( u32 )ZGL_SAMPLE_LINEAR );
	state.SetField ( -1, "GL_LINEAR_MIPMAP_LINEAR",		( u32 )ZGL_SAMPLE_LINEAR_MIPMAP_LINEAR );
	state.SetField ( -1, "GL_LINEAR_MIPMAP_NEAREST",	( u32 )ZGL_SAMPLE_LINEAR_MIPMAP_NEAREST );
	
	state.SetField ( -1, "GL_NEAREST",					( u32 )ZGL_SAMPLE_NEAREST );
	state.SetField ( -1, "GL_NEAREST_MIPMAP_LINEAR",	( u32 )ZGL_SAMPLE_NEAREST_MIPMAP_LINEAR );
	state.SetField ( -1, "GL_NEAREST_MIPMAP_NEAREST",	( u32 )ZGL_SAMPLE_NEAREST_MIPMAP_NEAREST );
	
	state.SetField ( -1, "GL_RGBA4",					( u32 )ZGL_PIXEL_FORMAT_RGBA4 );
	state.SetField ( -1, "GL_RGB5_A1",					( u32 )ZGL_PIXEL_FORMAT_RGB5_A1 );
	state.SetField ( -1, "GL_DEPTH_COMPONENT16",		( u32 )ZGL_PIXEL_FORMAT_DEPTH_COMPONENT16 );
	//***state.SetField ( -1, "GL_DEPTH_COMPONENT24",	( u32 )GL_DEPTH_COMPONENT24 );
	//***state.SetField ( -1, "GL_STENCIL_INDEX1",		( u32 )GL_STENCIL_INDEX1 );
	//***state.SetField ( -1, "GL_STENCIL_INDEX4",		( u32 )GL_STENCIL_INDEX4 );
	state.SetField ( -1, "GL_STENCIL_INDEX8",			( u32 )ZGL_PIXEL_FORMAT_STENCIL_INDEX8 );
	//***state.SetField ( -1, "GL_STENCIL_INDEX16",		( u32 )GL_STENCIL_INDEX16 );
	
	// TODO:
	#ifdef MOAI_OS_ANDROID
		state.SetField ( -1, "GL_RGB565",				( u32 )ZGL_PIXEL_FORMAT_RGB565 );
	#else
		state.SetField ( -1, "GL_RGBA8",				( u32 )ZGL_PIXEL_FORMAT_RGBA8 );
	#endif
}

//----------------------------------------------------------------//
void MOAITextureBase::RegisterLuaFuncs ( MOAILuaState& state ) {

	MOAIGfxResource::RegisterLuaFuncs ( state );

	luaL_Reg regTable [] = {
		{ "getSize",				_getSize },
		{ "release",				_release },
		{ "setFilter",				_setFilter },
		{ "setWrap",				_setWrap },
		{ NULL, NULL }
	};

	luaL_register ( state, 0, regTable );
}

//----------------------------------------------------------------//
void MOAITextureBase::SerializeIn ( MOAILuaState& state, MOAIDeserializer& serializer ) {
	UNUSED ( state );
	UNUSED ( serializer );
}

//----------------------------------------------------------------//
void MOAITextureBase::SerializeOut ( MOAILuaState& state, MOAISerializer& serializer ) {
	UNUSED ( state );
	UNUSED ( serializer );
}

//----------------------------------------------------------------//
void MOAITextureBase::SetFilter ( int filter ) {

	this->SetFilter ( filter, filter );
}

//----------------------------------------------------------------//
void MOAITextureBase::SetFilter ( int min, int mag ) {

	this->mMinFilter = min;
	this->mMagFilter = mag;
	
	this->mIsDirty = true;
}

//----------------------------------------------------------------//
void MOAITextureBase::SetWrap ( int wrap ) {

	this->mWrap = wrap;
	this->mIsDirty = true;
}

//----------------------------------------------------------------//
void MOAITextureBase::UpdateTextureFromImage ( MOAIImage& image, ZLIntRect rect ) {

	// if the dimensions have changed, clear out the old texture
	if (( this->mWidth != image.GetWidth ()) || ( this->mHeight != image.GetHeight ())) {
	
		MOAIGfxDevice::Get ().ReportTextureFree ( this->mDebugName, this->mTextureSize );
		MOAIGfxDevice::Get ().PushDeleter ( MOAIGfxDeleter::DELETE_TEXTURE, this->mGLTexID );
		this->mGLTexID = 0;	
	}
	
	// if no texture, create a new one from the image
	// otherwise just update the sub-region
	if ( this->mGLTexID ) {

		zglBindTexture ( this->mGLTexID );

		rect.Bless ();
		ZLIntRect imageRect = image.GetRect ();
		imageRect.Clip ( rect );
		
		void* buffer = image.GetBitmap ();
		
		if (( this->mWidth != ( u32 )rect.Width ()) || ( this->mHeight != ( u32 )rect.Height ())) {
			u32 size = image.GetSubImageSize ( rect );
			buffer = alloca ( size );
			image.GetSubImage ( rect, buffer );
		}

		zglTexSubImage2D (
			0,
			rect.mXMin,
			rect.mYMin,
			rect.Width (),
			rect.Height (),
			this->mGLInternalFormat,
			this->mGLPixelType,  
			buffer
		);
		
		MOAIGfxDevice::Get ().LogErrors ();
	}
	else {
	
		this->CreateTextureFromImage ( image );
	}
}
