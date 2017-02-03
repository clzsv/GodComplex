#include "stdafx.h"

// #pragma unmanaged
// #include "..\ImageUtilityLib\ImageFile.h"
// #pragma managed

#include "ImageFile.h"
#include "ImagesMatrix.h"

using namespace ImageUtility;

// Helper to wrap a bunch of images into a managed array
array< ImageFile^ >^	WrapNativeImages( U32 _imagesCount, ImageUtilityLib::ImageFile*& _images, bool _deleteNativeImages ) {
	// Wrap our managed version of ImageFiles around returned images
	array< ImageFile^ >^	result = gcnew array< ImageFile^ >( _imagesCount );
	for ( U32 imageIndex=0; imageIndex < _imagesCount; imageIndex++ ) {
		result[imageIndex] = gcnew ImageFile( _images[imageIndex], true );
	}

	if ( _deleteNativeImages )
		delete[] _images;

	return result;
}

// Creates a bitmap from a System::Drawing.Bitmap and a color profile
ImageFile::ImageFile( System::Drawing::Bitmap^ _bitmap, ImageUtility::ColorProfile^ _colorProfile ) {
	m_ownedObject = true;
	m_nativeObject = new ImageUtilityLib::ImageFile();

	// Load the bitmap's content
	int	width, height;
	array< Byte >^	bitmapContent = LoadBitmap( _bitmap, width, height );
	if ( bitmapContent == nullptr )
		throw gcnew Exception( "Failed to load bitmap content into an RGBA[]!" );

	// Initialize an empty native object
	m_nativeObject->Init( width, height, ImageUtilityLib::ImageFile::PIXEL_FORMAT::RGBA8, *_colorProfile->m_nativeObject );

	// Copy bitmap content
	U8*		target = (U8*) m_nativeObject->GetBits();
	int		sourceIndex = 0;
	for ( int i=width*height; i > 0; i-- ) {
		*target++ = bitmapContent[sourceIndex++];
		*target++ = bitmapContent[sourceIndex++];
		*target++ = bitmapContent[sourceIndex++];
		*target++ = bitmapContent[sourceIndex++];
	}
}
ImageFile::~ImageFile() {
	Exit();
	if ( m_ownedObject ) {
		SAFE_DELETE( m_nativeObject );
	}
}

void	ImageFile::Init( U32 _width, U32 _height, PIXEL_FORMAT _format, ImageUtility::ColorProfile^ _colorProfile ) {
	m_nativeObject->Init( _width, _height, ImageUtilityLib::ImageFile::PIXEL_FORMAT( _format ), *_colorProfile->m_nativeObject );
}

void	ImageFile::Exit() {
	m_nativeObject->Exit();
}

// Load from a file or memory
void	ImageFile::Load( System::IO::FileInfo^ _fileName ) {
	if ( !_fileName->Exists )
		throw gcnew System::IO::FileNotFoundException( "File \"" + _fileName + "\" not found!" );
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	m_nativeObject->Load( nativeFileName );
}
void	ImageFile::Load( System::IO::FileInfo^ _fileName, FILE_FORMAT _format ) {
	if ( !_fileName->Exists )
		throw gcnew System::IO::FileNotFoundException( "File \"" + _fileName + "\" not found!" );
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	m_nativeObject->Load( nativeFileName, ImageUtilityLib::ImageFile::FILE_FORMAT( _format ) );

// Or alternatively, we could load the stream
// 	using ( System::IO::FileStream ImageStream = _fileName.Open( System::IO::FileMode.Open, System::IO::FileAccess.Read, System::IO::FileShare.Read ) )
// 		Load( ImageStream, _format );
}
void	ImageFile::Load( System::IO::Stream^ _imageStream, FILE_FORMAT _format ) {
	// Read the file's content
	array< Byte >^	imageContent = gcnew array< Byte >( int( _imageStream->Length ) );
	_imageStream->Read( imageContent, 0, (int) _imageStream->Length );

	// Read from memory
	Load( gcnew NativeByteArray( imageContent ), _format );
}
void	ImageFile::Load( NativeByteArray^ _fileContent, FILE_FORMAT _format ) {
	// Call native method
	m_nativeObject->Load( _fileContent->AsBytePointer.ToPointer(), _fileContent->Length, ImageUtilityLib::ImageFile::FILE_FORMAT( _format ) );
}

// Save to a file or memory
void	ImageFile::Save( System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	m_nativeObject->Save( nativeFileName );
}
void	ImageFile::Save( System::IO::FileInfo^ _fileName, FILE_FORMAT _format ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	m_nativeObject->Save( nativeFileName, ImageUtilityLib::ImageFile::FILE_FORMAT( _format ) );
}
void	ImageFile::Save( System::IO::FileInfo^ _fileName, FILE_FORMAT _format, SAVE_FLAGS _options ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	m_nativeObject->Save( nativeFileName, ImageUtilityLib::ImageFile::FILE_FORMAT( _format ), ImageUtilityLib::ImageFile::SAVE_FLAGS( _options ) );
}
void	ImageFile::Save( System::IO::Stream^ _imageStream, FILE_FORMAT _format, SAVE_FLAGS _options ) {
	// Save into a temporary array
	NativeByteArray^	temp = Save( _format, _options );

	// Dump to stream
	_imageStream->Write( temp->AsByteArray, 0, temp->Length );

	delete temp;
}
NativeByteArray^	ImageFile::Save( FILE_FORMAT _format, SAVE_FLAGS _options ) {
	// Save to native memory
	void*	nativeBuffer = nullptr;
	U64		nativeBufferSize = 0;
	m_nativeObject->Save( ImageUtilityLib::ImageFile::FILE_FORMAT( _format ), ImageUtilityLib::ImageFile::SAVE_FLAGS( _options ), nativeBufferSize, nativeBuffer );

	// Copy to managed array
	return gcnew NativeByteArray( int( nativeBufferSize ), nativeBuffer );
}

 System::Drawing::Bitmap^	ImageFile::AsBitmap::get() {
	int	W = Width;
	int	H = Height;

	// Convert source bitmap to a compatible format
	ImageFile^	source = this;
	if ( PixelFormat != PIXEL_FORMAT::RGB8 && PixelFormat != PIXEL_FORMAT::RGBA8 ) {
		source = gcnew ImageFile();
		source->ConvertFrom( this, PIXEL_FORMAT::RGBA8 );
	}

	System::Drawing::Bitmap^	result = gcnew System::Drawing::Bitmap( W, H, System::Drawing::Imaging::PixelFormat::Format32bppArgb );

	const U8*		sourcePtr = (U8*) source->Bits.ToPointer();

	System::Drawing::Imaging::BitmapData^	lockedBitmap = result->LockBits( System::Drawing::Rectangle( 0, 0, W, H ), System::Drawing::Imaging::ImageLockMode::ReadOnly, System::Drawing::Imaging::PixelFormat::Format32bppArgb );

	pin_ptr<void>	scan0Ptr = lockedBitmap->Scan0.ToPointer();

	if ( source->PixelFormat == PIXEL_FORMAT::RGBA8 ) {
		// 32 bpp
		for ( int Y=0; Y < H; Y++ ) {
			Byte*	targetPtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
			for ( int X=0; X < W; X++ ) {
				*targetPtr++ = *sourcePtr++;	// B
				*targetPtr++ = *sourcePtr++;	// G
				*targetPtr++ = *sourcePtr++;	// R
				*targetPtr++ = *sourcePtr++;	// A
			}
		}
	} else {
		// 24 bpp
		for ( int Y=0; Y < H; Y++ ) {
			Byte*	targetPtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
			for ( int X=0; X < W; X++ ) {
				*targetPtr++ = *sourcePtr++;	// B
				*targetPtr++ = *sourcePtr++;	// G
				*targetPtr++ = *sourcePtr++;	// R
				*targetPtr++ = 0xFFU;			// A
			}
		}
	}

	result->UnlockBits( lockedBitmap );

	if ( source != this )
		delete source;	// We had to make a temporary conversion so now we must delete it

	return result;
}

 System::Drawing::Bitmap^	ImageFile::AsTiledBitmap( UInt32 _width, UInt32 _height ) {
	int	W = Width;
	int	H = Height;

	// Convert source bitmap to a compatible format
	ImageFile^	source = this;
	if ( PixelFormat != PIXEL_FORMAT::RGB8 && PixelFormat != PIXEL_FORMAT::RGBA8 ) {
		source = gcnew ImageFile();
		source->ConvertFrom( this, PIXEL_FORMAT::RGBA8 );
	}

	const U8*	sourcePtr = (U8*) source->Bits.ToPointer();

	System::Drawing::Bitmap^	result = gcnew System::Drawing::Bitmap( _width, _height, System::Drawing::Imaging::PixelFormat::Format32bppArgb );
	System::Drawing::Imaging::BitmapData^	lockedBitmap = result->LockBits( System::Drawing::Rectangle( 0, 0, W, H ), System::Drawing::Imaging::ImageLockMode::ReadOnly, System::Drawing::Imaging::PixelFormat::Format32bppArgb );

	pin_ptr<void>	scan0Ptr = lockedBitmap->Scan0.ToPointer();

	if ( source->PixelFormat == PIXEL_FORMAT::RGBA8 ) {
		// 32 bpp
		for ( UInt32 Y=0; Y < _height; Y++ ) {
			Byte*		targetPtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
			const U8*	sourceScanlinePtr = sourcePtr + (Y % H) * 4*W;
			for ( UInt32 X=0; X < _width; X++ ) {
				int	tiledX = X % W;
				*targetPtr++ = sourceScanlinePtr[4*tiledX+0];	// B
				*targetPtr++ = sourceScanlinePtr[4*tiledX+1];	// G
				*targetPtr++ = sourceScanlinePtr[4*tiledX+2];	// R
				*targetPtr++ = sourceScanlinePtr[4*tiledX+3];	// A
			}
		}
	} else {
		// 24 bpp
		for ( UInt32 Y=0; Y < _height; Y++ ) {
			Byte*	targetPtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
			const U8*	sourceScanlinePtr = sourcePtr + (Y % H) * 3*W;
			for ( UInt32 X=0; X < _width; X++ ) {
				int	tiledX = X % W;
				*targetPtr++ = sourceScanlinePtr[3*tiledX+0];	// B
				*targetPtr++ = sourceScanlinePtr[3*tiledX+1];	// G
				*targetPtr++ = sourceScanlinePtr[3*tiledX+2];	// R
				*targetPtr++ = 0xFFU;	// A
			}
		}
	}

	result->UnlockBits( lockedBitmap );

	if ( source != this )
		delete source;	// We had to make a temporary conversion so now we must delete it

	return result;
}

// Converts the source image to a target format
void	ImageFile::ConvertFrom( ImageFile^ _source, PIXEL_FORMAT _targetFormat ) {
	m_nativeObject->ConvertFrom( *_source->m_nativeObject, ImageUtilityLib::ImageFile::PIXEL_FORMAT( _targetFormat ) );
}

// Tone maps a HDR image into a LDR RGBA8 format
void	ImageFile::ToneMapFrom( ImageFile^ _source, ToneMapper^ _toneMapper ) {

	// Get a function pointer to the delegate
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _toneMapper );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _toneMapper );

	m_nativeObject->ToneMapFrom( *_source->m_nativeObject, static_cast< ImageUtilityLib::ImageFile::toneMapper_t >( ip.ToPointer() ) );

	// release reference to delegate  
	gch.Free();  
}

// Retrieves the image file type based on the image file name
ImageFile::FILE_FORMAT	ImageFile::GetFileTypeFromExistingFileContent( System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	return FILE_FORMAT( ImageUtilityLib::ImageFile::GetFileTypeFromExistingFileContent( nativeFileName ) );
}
ImageFile::FILE_FORMAT	ImageFile::GetFileTypeFromFileNameOnly( System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	return FILE_FORMAT( ImageUtilityLib::ImageFile::GetFileTypeFromFileNameOnly( nativeFileName ) );
}

void	ImageFile::ReadScanline( UInt32 _Y, array< float4 >^ _color, UInt32 _startX ) {
	pin_ptr<float4>	color = &_color[0];
	m_nativeObject->ReadScanline( _Y, (bfloat4*) color, _startX, _color->Length );
}
void	ImageFile::ReadPixels( PixelReadWrite^ _reader, UInt32 _startX, UInt32 _startY, UInt32 _width, UInt32 _height ) {
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _reader );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _reader );
	m_nativeObject->ReadPixels( static_cast< ImageUtilityLib::ImageFile::pixelReaderWriter_t >( ip.ToPointer() ), _startX, _startY, _width, _height );
	gch.Free();  
}

void	ImageFile::WriteScanline( UInt32 _Y, array< float4 >^ _color, UInt32 _startX ) {
	pin_ptr<float4>	color = &_color[0];
	m_nativeObject->WriteScanline( _Y, (bfloat4*) color, _startX, _color->Length );
}
void	ImageFile::WritePixels( PixelReadWrite^ _reader, UInt32 _startX, UInt32 _startY, UInt32 _width, UInt32 _height ) {
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _reader );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _reader );
	m_nativeObject->WritePixels( static_cast< ImageUtilityLib::ImageFile::pixelReaderWriter_t >( ip.ToPointer() ), _startX, _startY, _width, _height );
	gch.Free();  
}

array< Byte >^	ImageFile::LoadBitmap( System::Drawing::Bitmap^ _bitmap, int& _width, int& _height ) {
	_width = _bitmap->Width;
	_height = _bitmap->Height;

	array< System::Byte >^	result = gcnew array< System::Byte >( 4*_width*_height );

	System::Drawing::Imaging::BitmapData^	lockedBitmap = _bitmap->LockBits( System::Drawing::Rectangle( 0, 0, _width, _height ), System::Drawing::Imaging::ImageLockMode::ReadOnly, System::Drawing::Imaging::PixelFormat::Format32bppArgb );

	pin_ptr<void>	scan0Ptr = lockedBitmap->Scan0.ToPointer();

	Byte	R, G, B, A;
	int		targetIndex = 0;
	for ( int Y=0; Y < _height; Y++ ) {
		Byte*	scanlinePtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
		for ( int X=0; X < _width; X++ ) {
			// Read in shitty order
			B = *scanlinePtr++;
			G = *scanlinePtr++;
			R = *scanlinePtr++;
			A = *scanlinePtr++;

			// Write in correct order
			result[targetIndex++] = R;
			result[targetIndex++] = G;
			result[targetIndex++] = B;
			result[targetIndex++] = A;
		}
	}

	_bitmap->UnlockBits( lockedBitmap );

	return result;
}

System::Drawing::Bitmap^	ImageFile::AsCustomBitmap( ColorTransformer^ _transformer ) {
	System::Drawing::Bitmap^	result = gcnew System::Drawing::Bitmap( Width, Height, System::Drawing::Imaging::PixelFormat::Format32bppArgb );
	AsCustomBitmap( result, _transformer );
	return result;
}

void	ImageFile::AsCustomBitmap( System::Drawing::Bitmap^ _bitmap, ColorTransformer^ _transformer ) {
	if ( _bitmap == nullptr )
		throw gcnew Exception( "Invalid bitmap!" );
	if ( _bitmap->Width != Width )
		throw gcnew Exception( "Provided bitmap width mismatch!" );
	if ( _bitmap->Height != Height )
		throw gcnew Exception( "Provided bitmap height mismatch!" );

	System::Drawing::Imaging::BitmapData^	lockedBitmap = _bitmap->LockBits( System::Drawing::Rectangle( 0, 0, Width, Height ), System::Drawing::Imaging::ImageLockMode::WriteOnly, System::Drawing::Imaging::PixelFormat::Format32bppArgb );

	pin_ptr<void>	scan0Ptr = lockedBitmap->Scan0.ToPointer();

	array<float4>^	sourceScanline = gcnew array<float4>( Width );
	float4	temp;
	Byte	R, G, B, A;
	int		targetIndex = 0;
	for ( UInt32 Y=0; Y < Height; Y++ ) {
		ReadScanline( Y, sourceScanline );
		Byte*	targetScanlinePtr = reinterpret_cast<Byte*>(scan0Ptr) + Y * lockedBitmap->Stride;
		for ( UInt32 X=0; X < Width; X++ ) {
			// Apply user transform
			temp = sourceScanline[X];
			_transformer( temp );

			R = Byte( Math::Max( 0.0f, Math::Min( 255.0f, 255.0f * temp.x ) ) );
			G = Byte( Math::Max( 0.0f, Math::Min( 255.0f, 255.0f * temp.y ) ) );
			B = Byte( Math::Max( 0.0f, Math::Min( 255.0f, 255.0f * temp.z ) ) );
			A = Byte( Math::Max( 0.0f, Math::Min( 255.0f, 255.0f * temp.w ) ) );

			// Write in shitty order
			*targetScanlinePtr++ = B;
			*targetScanlinePtr++ = G;
			*targetScanlinePtr++ = R;
			*targetScanlinePtr++ = A;
		}
	}

	_bitmap->UnlockBits( lockedBitmap );
}



//////////////////////////////////////////////////////////////////////////
// Plotting helpers
//
void	ImageFile::Clear( SharpMath::float4^ _color ) {
	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	m_nativeObject->Clear( color );
}

void	ImageFile::PlotGraph( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, PlotDelegate^ _delegate ) {
	// Get a function pointer to the delegate
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _delegate );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _delegate );

	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY( _rangeY->x, _rangeY->y );
	m_nativeObject->PlotGraph( color, rangeX, rangeY, static_cast< ImageUtilityLib::ImageFile::PlotDelegate_t >( ip.ToPointer() ) );

	// release reference to delegate  
	gch.Free();  
}

void	ImageFile::PlotGraphAutoRangeY( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2% _rangeY, PlotDelegate^ _delegate ) {
	// Get a function pointer to the delegate
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _delegate );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _delegate );

	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY;
	m_nativeObject->PlotGraphAutoRangeY( color, rangeX, rangeY, static_cast< ImageUtilityLib::ImageFile::PlotDelegate_t >( ip.ToPointer() ) );

	_rangeY.x = rangeY.x;
	_rangeY.y = rangeY.y;

	// release reference to delegate  
	gch.Free();  
}

// void	ImageFile::PlotLogGraph( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, PlotDelegate^ _delegate ) {
// 	PlotLogGraph( _color, _rangeX, _rangeY, _delegate, 10.0f, 10.0f );
// }
void	ImageFile::PlotLogGraph( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, PlotDelegate^ _delegate, float _logBaseX, float _logBaseY ) {
	// Get a function pointer to the delegate
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _delegate );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _delegate );

	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY( _rangeY->x, _rangeY->y );
	m_nativeObject->PlotLogGraph( color, rangeX, rangeY, static_cast< ImageUtilityLib::ImageFile::PlotDelegate_t >( ip.ToPointer() ), _logBaseX, _logBaseY );

	// release reference to delegate  
	gch.Free();  
}

// void	ImageFile::PlotLogGraphAutoRangeY( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2% _rangeY, PlotDelegate^ _delegate ) {
// 	PlotLogGraphAutoRangeY( _color, _rangeX, _rangeY, _delegate, 10.0f, 10.0f );
// }
void	ImageFile::PlotLogGraphAutoRangeY( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2% _rangeY, PlotDelegate^ _delegate, float _logBaseX, float _logBaseY ) {
	// Get a function pointer to the delegate
	System::Runtime::InteropServices::GCHandle	gch = System::Runtime::InteropServices::GCHandle::Alloc( _delegate );
	IntPtr		ip = System::Runtime::InteropServices::Marshal::GetFunctionPointerForDelegate( _delegate );

	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY;
	m_nativeObject->PlotLogGraphAutoRangeY( color, rangeX, rangeY, static_cast< ImageUtilityLib::ImageFile::PlotDelegate_t >( ip.ToPointer() ), _logBaseX, _logBaseY );

	_rangeY.x = rangeY.x;
	_rangeY.y = rangeY.y;

	// release reference to delegate  
	gch.Free();  
}

void	ImageFile::PlotAxes( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, float _stepX, float _stepY ) {
	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY( _rangeY->x, _rangeY->y );
	m_nativeObject->PlotAxes( color, rangeX, rangeY, _stepX, _stepY );
}

void	ImageFile::PlotLogAxes( SharpMath::float4^ _color, SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, float _logBaseX, float _logBaseY ) {
	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	rangeX( _rangeX->x, _rangeX->y );
	bfloat2	rangeY( _rangeY->x, _rangeY->y );
	m_nativeObject->PlotLogAxes( color, rangeX, rangeY, _logBaseX, _logBaseY );
}

void	ImageFile::DrawLine( SharpMath::float4^ _color, SharpMath::float2^ _P0, SharpMath::float2^ _P1 ) {
	bfloat4	color( _color->x, _color->y, _color->z, _color->w );
	bfloat2	P0( _P0->x, _P0->y );
	bfloat2	P1( _P1->x, _P1->y );
	m_nativeObject->DrawLine( color, P0, P1 );
}

SharpMath::float2	ImageFile::RangedCoordinates2ImageCoordinates( SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, SharpMath::float2^ _rangedCoordinates ) {
	bfloat2	result;
	m_nativeObject->RangedCoordinates2ImageCoordinates( bfloat2( _rangeX->x, _rangeX->y ), bfloat2( _rangeY->x, _rangeY->y ), bfloat2( _rangedCoordinates->x, _rangedCoordinates->y ), result );
	return float2( result.x, result.y );
}
SharpMath::float2	ImageFile::ImageCoordinates2RangedCoordinates( SharpMath::float2^ _rangeX, SharpMath::float2^ _rangeY, SharpMath::float2^ _imageCoordinates ) {
	bfloat2	result;
	m_nativeObject->ImageCoordinates2RangedCoordinates( bfloat2( _rangeX->x, _rangeX->y ), bfloat2( _rangeY->x, _rangeY->y ), bfloat2( _imageCoordinates->x, _imageCoordinates->y ), result );
	return float2( result.x, result.y );
}


//////////////////////////////////////////////////////////////////////////
// DDS-related methods
ImagesMatrix^	ImageFile::DDSLoadFile( System::IO::FileInfo^ _fileName ) {
	if ( _fileName->Exists )
		throw gcnew System::IO::FileNotFoundException( "File not found!", _fileName->FullName );

	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	ImagesMatrix^	result = gcnew ImagesMatrix();
	ImageUtilityLib::ImageFile::DDSLoadFile( nativeFileName, *result->m_nativeObject );

	return result;
}
ImagesMatrix^	ImageFile::DDSLoadMemory( NativeByteArray^ _imageContent ) {
	ImagesMatrix^	result = gcnew ImagesMatrix();
	ImageUtilityLib::ImageFile::DDSLoadMemory( _imageContent->Length, _imageContent->AsBytePointer.ToPointer(), *result->m_nativeObject );

	return result;
}
void	ImageFile::DDSSaveFile( ImagesMatrix^ _images, System::IO::FileInfo^ _fileName, COMPONENT_FORMAT _componentFormat ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	ImageUtilityLib::ImageFile::DDSSaveFile( *_images->m_nativeObject, nativeFileName, ImageUtilityLib::ImageFile::COMPONENT_FORMAT( _componentFormat ) );
}
NativeByteArray^	ImageFile::DDSSaveMemory( ImagesMatrix^ _images, COMPONENT_FORMAT _componentFormat ) {
	// Generate native byte array
	U64		fileSize = 0;
	void*	fileContent = NULL;
	ImageUtilityLib::ImageFile::DDSSaveMemory( *_images->m_nativeObject, fileSize, fileContent, ImageUtilityLib::ImageFile::COMPONENT_FORMAT( _componentFormat ) );

	NativeByteArray^	result = gcnew NativeByteArray( int(fileSize), fileContent );
	return result;
}

/*

// Compresses a single image
NativeByteArray^	ImageFile::DDSCompress( COMPRESSION_TYPE _compressionType ) {

	// Call native method
	void*	compressedImage;
	U32		compressedImageLength;
	m_nativeObject->DDSCompress( ImageUtilityLib::ImageFile::COMPRESSION_TYPE( _compressionType ), compressedImageLength, compressedImage );

	return gcnew NativeByteArray( compressedImageLength, compressedImage );
}

// Saves a DDS image in memory to disk (usually used after a compression)
void ImageFile::DDSSaveFromMemory( NativeByteArray^ _DDSImage, System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );
	ImageUtilityLib::ImageFile::DDSSaveFromMemory( _DDSImage->Length, _DDSImage->AsBytePointer.ToPointer(), nativeFileName );
}
void ImageFile::DDSSaveFromMemory( NativeByteArray^ _DDSImage, System::IO::Stream^ _imageStream ) {
	_imageStream->Write( _DDSImage->AsByteArray, 0, _DDSImage->Length );
}

// Cube map handling
array< ImageFile^ >^	ImageFile::DDSLoadCubeMap( System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	// Call native method
	U32							imagesCount;
	ImageUtilityLib::ImageFile*	images;
	ImageUtilityLib::ImageFile::DDSLoadFile( nativeFileName, imagesCount, images );

	return WrapNativeImages( imagesCount, images, true );

// Or call streamed version
// 	System::IO::FileStream^	S = _fileName->OpenRead();
// 	array< ImageFile^ >^	result = DDSLoadCubeMap( S );
// 	delete S;
// 	return result;
}
array< ImageFile^ >^	ImageFile::DDSLoadCubeMap( System::IO::Stream^ _imageStream ) {
	// Load stream into memory
	array< Byte >^	fileContent = gcnew array< Byte >( int( _imageStream->Length ) );
	_imageStream->Read( fileContent, 0, int( _imageStream->Length ) );

	NativeByteArray^	temp = gcnew NativeByteArray( fileContent );

	// Call native method
	U32							imagesCount;
	ImageUtilityLib::ImageFile*	images;
	ImageUtilityLib::ImageFile::DDSLoadCubeMapMemory( temp->Length, temp->AsBytePointer.ToPointer(), imagesCount, images );

	// Release native memory
	delete temp;

	return WrapNativeImages( imagesCount, images, true );
}
void	ImageFile::DDSSaveCubeMap( array< ImageFile^ >^ _cubeMapFaces, bool _compressBC6H, System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	const ImageUtilityLib::ImageFile**	nativeCubeMapFaces = new const ImageUtilityLib::ImageFile*[_cubeMapFaces->Length];
	for ( int i=0; i < _cubeMapFaces->Length; i++ )
		nativeCubeMapFaces[i] = _cubeMapFaces[i]->m_nativeObject;

	ImageUtilityLib::ImageFile::DDSSaveCubeMapFile( _cubeMapFaces->Length, nativeCubeMapFaces, _compressBC6H, nativeFileName );

	delete[] nativeCubeMapFaces;
}
void	ImageFile::DDSSaveCubeMap( array< ImageFile^ >^ _cubeMapFaces, bool _compressBC6H, System::IO::Stream^ _imageStream ) {
//	System::Runtime::InteropServices::Marshal::AllocHGlobal( _cubeMapFaces->Length * Sizeof* ); ??
	const ImageUtilityLib::ImageFile**	nativeCubeMapFaces = new const ImageUtilityLib::ImageFile*[_cubeMapFaces->Length];
	for ( int i=0; i < _cubeMapFaces->Length; i++ )
		nativeCubeMapFaces[i] = _cubeMapFaces[i]->m_nativeObject;

	// Call native method
	void*	fileContent = nullptr;
	U32		fileLength = 0;
	ImageUtilityLib::ImageFile::DDSSaveCubeMapMemory( _cubeMapFaces->Length, nativeCubeMapFaces, _compressBC6H, fileLength, fileContent );

	delete[] nativeCubeMapFaces;

	// Write to stream
	NativeByteArray^	temp = gcnew NativeByteArray( fileLength, fileContent );
	_imageStream->Write( temp->AsByteArray, 0, temp->Length );
	delete temp;
}

// 3D Texture handling
array< ImageFile^ >^	ImageFile::DDSLoad3DTexture( System::IO::FileInfo^ _fileName, U32& _slicesCount ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	// Call native method
	U32							imagesCount;
	ImageUtilityLib::ImageFile*	images;
	ImageUtilityLib::ImageFile::DDSLoad3DTextureFile( nativeFileName, imagesCount, images );

	return WrapNativeImages( imagesCount, images, true );

// Or call streamed version
// 	System::IO::FileStream^	S = _fileName->OpenRead();
// 	array< ImageFile^ >^	result = DDSLoadCubeMap( S );
// 	delete S;
// 	return result;
}
array< ImageFile^ >^	ImageFile::DDSLoad3DTexture( System::IO::Stream^ _imageStream ) {
	// Load stream into memory
	array< Byte >^	fileContent = gcnew array< Byte >( int( _imageStream->Length ) );
	_imageStream->Read( fileContent, 0, int( _imageStream->Length ) );

	NativeByteArray^	temp = gcnew NativeByteArray( fileContent );

	// Call native method
	U32							imagesCount;
	ImageUtilityLib::ImageFile*	images;
	ImageUtilityLib::ImageFile::DDSLoad3DTextureMemory( temp->Length, temp->AsBytePointer.ToPointer(), imagesCount, images );

	delete temp;

	return WrapNativeImages( imagesCount, images, true );
}
void	ImageFile::DDSSave3DTexture( array< ImageFile^ >^ _slices, bool _compressBC6H, System::IO::FileInfo^ _fileName ) {
	pin_ptr< const wchar_t >	nativeFileName = PtrToStringChars( _fileName->FullName );

	const ImageUtilityLib::ImageFile**	nativeSlices = new const ImageUtilityLib::ImageFile*[_slices->Length];
	for ( int i=0; i < _slices->Length; i++ )
		nativeSlices[i] = _slices[i]->m_nativeObject;

	ImageUtilityLib::ImageFile::DDSSave3DTextureFile( _slices->Length, nativeSlices, _compressBC6H, nativeFileName );

	delete[] nativeSlices;
}
void	ImageFile::DDSSave3DTexture( array< ImageFile^ >^ _slices, bool _compressBC6H, System::IO::Stream^ _imageStream ) {
	const ImageUtilityLib::ImageFile**	nativeSlices = new const ImageUtilityLib::ImageFile*[_slices->Length];
	for ( int i=0; i < _slices->Length; i++ )
		nativeSlices[i] = _slices[i]->m_nativeObject;

	void*	fileContent = nullptr;
	U32		fileLength = 0;
	ImageUtilityLib::ImageFile::DDSSaveCubeMapMemory( _slices->Length, nativeSlices, _compressBC6H, fileLength, fileContent );

	delete[] nativeSlices;

	// Copy to Byte[]
	array< Byte >^	managedBuffer = gcnew array< Byte >( fileLength );
	System::Runtime::InteropServices::Marshal::Copy( IntPtr(fileContent), managedBuffer, 0, fileLength );

	// Write to stream
	_imageStream->Write( managedBuffer, 0, fileLength );
}
*/
