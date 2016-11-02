//////////////////////////////////////////////////////////////////////////
// The bitmap class cannot work without a proper definition of a Color Profile
// Because all image systems sooner or later work with the device-dependent RGB color space,
//	we need to offer a robust bridge between the (device-dependent) RGB color space and the
//	reference (device-independent) XYZ color space that is used internally by the Bitmap class
//
// The color profile serves as a bridge between device-dependent color spaces like:
//	� RGB
//	� CMYK
//	� HSL / HSB / HSI
//	� RGBE
//	� YCoCg
//	� YCbCr
// 
// and device-independent color spaces like:
//	� CIE XYZ	(our reference space)
//	� CIE xyY	(a variation on XYZ)
//	� CIE Lab
//
////////////////////////////////////////////////////////////////////////////
//
#pragma once

#include "Types.h"

namespace ImageUtilityLib {

	/// <summary>
	/// Defines a color converter that can handle transforms between XYZ and RGB
	/// Usually implemented by a ColorProfile so the RGB color is fully characterized
	/// </summary>
	class IColorConverter {
	public:
		virtual void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) abstract;
		virtual void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) abstract;
		virtual void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) abstract;
		virtual void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) abstract;
	};

	/// <summary>
	/// The source color for the bitmap
	/// The color profile helps converting between the original color space and the internal CIEXYZ color space used in the Bitmap class
	/// 
	/// For now, only standard profiles like Linear, sRGB, Adobe RGB, ProPhoto RGB or any custom chromaticities are supported.
	/// I believe it would be overkill to include a library for parsing embedded ICC profiles...
	/// </summary>
	class	ColorProfile : IColorConverter {
	public:
		#pragma region CONSTANTS

		static const float2		ILLUMINANT_A;	// Incandescent, tungsten
		static const float2		ILLUMINANT_D50;	// Daylight, Horizon
		static const float2		ILLUMINANT_D55;	// Mid-Morning, Mid-Afternoon
		static const float2		ILLUMINANT_D65;	// Daylight, Noon, Overcast (sRGB reference illuminant)
		static const float2		ILLUMINANT_E;	// Reference

		static const float		GAMMA_EXPONENT_sRGB;// = 2.4f;
		static const float		GAMMA_EXPONENT_ADOBE;// = 2.19921875f;
		static const float		GAMMA_EXPONENT_PRO_PHOTO;// = 1.8f;

		#pragma endregion

	public:
		#pragma region NESTED TYPES

		enum class STANDARD_PROFILE {
			INVALID,		// The profile is invalid (meaning one of the chromaticities was not initialized!)
			CUSTOM,			// No recognizable standard profile (custom)
			LINEAR,			// sRGB with linear gamma
			sRGB,			// sRGB with D65 illuminant
			ADOBE_RGB_D50,	// Adobe RGB with D50 illuminant
			ADOBE_RGB_D65,	// Adobe RGB with D65 illuminant
			PRO_PHOTO,		// ProPhoto with D50 illuminant
			RADIANCE,		// Radiance HDR format with E illuminant
		};

		/// <summary>
		/// Enumerates the various supported gamma curves
		/// </summary>
		enum class GAMMA_CURVE {
			STANDARD,		// Standard gamma curve using a single exponent and no linear slope
			sRGB,			// sRGB gamma with linear slope
			PRO_PHOTO,		// ProPhoto gamma with linear slope
		};

		/// <summary>
		/// Describes the Red, Green, Blue and White Point chromaticities of a simple/standard color profile
		/// </summary>
//		[System.Diagnostics.DebuggerDisplay( "R=({R.x},{R.y}) G=({G.x},{G.y}) B=({B.x},{B.y}) W=({W.x},{W.y}) Prof={RecognizedChromaticity}" )]
		struct	Chromaticities {
			float2		R, G, B, W;

			Chromaticities() {}
			Chromaticities( const float2& r, const float2& g, const float2& b, const float2& w ) {
				R = r;
				G = g;
				B = b;
				W = w;
			}
			Chromaticities( float xr, float yr, float xg, float yg, float xb, float yb, float xw, float yw ) {
				R.Set( xr, yr );
				G.Set( xg, yg );
				B.Set( xb, yb );
				W.Set( xw, yw );
			}

			static const Chromaticities	Empty;
			static const Chromaticities	sRGB;
			static const Chromaticities	AdobeRGB_D50;
			static const Chromaticities	AdobeRGB_D65;
			static const Chromaticities	ProPhoto;
			static const Chromaticities	Radiance;

			/// <summary>
			/// Attempts to recognize the current chromaticities as a standard profile
			/// </summary>
			/// <returns></returns>
			STANDARD_PROFILE	FindRecognizedChromaticity() {
				if ( Equals( sRGB ) )
					return STANDARD_PROFILE::sRGB;
				if ( Equals( AdobeRGB_D65 ) )
					return STANDARD_PROFILE::ADOBE_RGB_D65;
				if ( Equals( AdobeRGB_D50 ) )
					return STANDARD_PROFILE::ADOBE_RGB_D50;
				if ( Equals( ProPhoto ) )
					return STANDARD_PROFILE::PRO_PHOTO;
				if ( Equals( Radiance ) )
					return STANDARD_PROFILE::RADIANCE;

				// Ensure the profile is valid
				return R.x != 0.0f && R.y != 0.0f && G.x != 0.0f && G.y != 0.0f && B.x != 0.0f && B.y != 0.0f && W.x != 0.0f && W.y != 0.0f ? STANDARD_PROFILE::CUSTOM : STANDARD_PROFILE::INVALID;
			}

		private:
			bool	Equals( const Chromaticities& other ) const {
				static const float	EPSILON = 1e-3f;
				return R.Almost( other.R, EPSILON )
					&& G.Almost( other.G, EPSILON )
					&& B.Almost( other.B, EPSILON )
					&& W.Almost( other.W, EPSILON );
			}
		};

	protected:

		#pragma region Internal XYZ<->RGB Converters

		class		InternalColorConverter_sRGB : public IColorConverter {
		public:
			static const float4x4 MAT_RGB2XYZ;
			static const float4x4 MAT_XYZ2RGB;
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_AdobeRGB_D50 : public IColorConverter {
		public:
			static const float4x4 MAT_RGB2XYZ;
			static const float4x4 MAT_XYZ2RGB;
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_AdobeRGB_D65 : public IColorConverter {
		public:
			static const float4x4 MAT_RGB2XYZ;
			static const float4x4 MAT_XYZ2RGB;
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_ProPhoto : public IColorConverter {
		public:
			static const float4x4 MAT_RGB2XYZ;
			static const float4x4 MAT_XYZ2RGB;
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_Radiance : public IColorConverter {
		public:
			static const float4x4 MAT_RGB2XYZ;
			static const float4x4 MAT_XYZ2RGB;
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_Generic_NoGamma : public IColorConverter {
			float4x4	m_RGB2XYZ;
			float4x4	m_XYZ2RGB;

		public:
			InternalColorConverter_Generic_NoGamma( const float4x4& _RGB2XYZ, const float4x4& _XYZ2RGB ) {
				m_RGB2XYZ = _RGB2XYZ;
				m_XYZ2RGB = _XYZ2RGB;
			}
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_Generic_StandardGamma : public IColorConverter {
			float4x4	m_RGB2XYZ;
			float4x4	m_XYZ2RGB;
			float		m_Gamma;
			float		m_InvGamma;

		public:
			InternalColorConverter_Generic_StandardGamma( const float4x4& _RGB2XYZ, const float4x4& _XYZ2RGB, float _Gamma ) {
				m_RGB2XYZ = _RGB2XYZ;
				m_XYZ2RGB = _XYZ2RGB;
				m_Gamma = _Gamma;
				m_InvGamma = 1.0f / _Gamma;
			}
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_Generic_sRGBGamma : public IColorConverter {
			float4x4	m_RGB2XYZ;
			float4x4	m_XYZ2RGB;

		public:
			InternalColorConverter_Generic_sRGBGamma( const float4x4& _RGB2XYZ, const float4x4& _XYZ2RGB ) {
				m_RGB2XYZ = _RGB2XYZ;
				m_XYZ2RGB = _XYZ2RGB;
			}
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		class		InternalColorConverter_Generic_ProPhoto : public IColorConverter {
			float4x4	m_RGB2XYZ;
			float4x4	m_XYZ2RGB;

		public:
			InternalColorConverter_Generic_ProPhoto( const float4x4& _RGB2XYZ, const float4x4& _XYZ2RGB ) {
				m_RGB2XYZ = _RGB2XYZ;
				m_XYZ2RGB = _XYZ2RGB;
			}
			void		XYZ2RGB( const float4& _XYZ, float4& _RGB ) override;
			void		RGB2XYZ( const float4& _RGB, float4& _XYZ ) override;
			void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) override;
			void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) override;
		};

		#pragma endregion

		#pragma endregion

	protected:
		#pragma region FIELDS

		bool				m_profileFoundInFile;
		Chromaticities		m_chromaticities;
		GAMMA_CURVE			m_gammaCurve;
		float				m_gamma;
		float				m_exposureBias;

		float4x4			m_RGB2XYZ;
		float4x4			m_XYZ2RGB;

		IColorConverter*	m_internalConverter;
 
		#pragma endregion

	public:
		#pragma region PROPERTIES

		/// <summary>
		/// Gets the chromaticities attached to the profile
		/// </summary>
		const Chromaticities&	GetChromas() const { return m_chromaticities; }

		/// <summary>
		/// Gets the transform to convert RGB to CIEXYZ
		/// </summary>
		const float4x4&			GetMatrixRGB2XYZ() const { return m_RGB2XYZ; }

		/// <summary>
		/// Gets the transform to convert CIEXYZ to RGB
		/// </summary>
		const float4x4&			GetMatrixXYZ2RGB() const { return m_XYZ2RGB; }

		/// <summary>
		/// Gets or sets the image gamma curve
		/// </summary>
		GAMMA_CURVE				GetGammaCurve() const	{ return m_gammaCurve; }
		void					SetGammaCurve( GAMMA_CURVE value ) {
			m_gammaCurve = value;
			BuildTransformFromChroma( true );
		}

		/// <summary>
		/// Gets or sets the image gamma
		/// </summary>
		float				GetGamma() const { return m_gamma; }
		void				SetGamma( float value ) {
			m_gamma = value;
			BuildTransformFromChroma( true );
		}

		/// <summary>
		/// Gets or sets the image exposure (usually for HDR images)
		/// </summary>
		float				GetExposureBias() const { return m_exposureBias; }
		void				SetExposureBias( float value ) { m_exposureBias = value; }

		/// <summary>
		/// True if the profile was found in the file's metadata and can be considered accurate.
		/// False if it's the default assumed profile and may NOT be the actual image's profile.
		/// </summary>
		bool				GetProfileFoundInFile() const { return m_profileFoundInFile; }

		#pragma endregion

	public:
		#pragma region METHODS

		ColorProfile()
			: m_profileFoundInFile( false )
			, m_chromaticities( Chromaticities::Empty )
			, m_gammaCurve( GAMMA_CURVE::STANDARD )
			, m_gamma( 1.0f )
			, m_exposureBias( 0.0f )
			, m_RGB2XYZ( float4x4::Identity )
			, m_XYZ2RGB( float4x4::Identity )
			, m_internalConverter( nullptr )
		{}
		virtual ~ColorProfile() {
			SAFE_DELETE( m_internalConverter );
		}

		/// <summary>
		/// Build from a standard profile
		/// </summary>
		/// <param name="_Profile"></param>
		ColorProfile( STANDARD_PROFILE _profile ) {
			switch ( _profile ) {
				case STANDARD_PROFILE::LINEAR:
					m_chromaticities = Chromaticities::sRGB;
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					break;
				case STANDARD_PROFILE::sRGB:
					m_chromaticities = Chromaticities::sRGB;
					m_gammaCurve = GAMMA_CURVE::sRGB;
					m_gamma = GAMMA_EXPONENT_sRGB;
					break;
				case STANDARD_PROFILE::ADOBE_RGB_D50:
					m_chromaticities = Chromaticities::AdobeRGB_D50;
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = GAMMA_EXPONENT_ADOBE;
					break;
				case STANDARD_PROFILE::ADOBE_RGB_D65:
					m_chromaticities = Chromaticities::AdobeRGB_D65;
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = GAMMA_EXPONENT_ADOBE;
					break;
				case STANDARD_PROFILE::PRO_PHOTO:
					m_chromaticities = Chromaticities::ProPhoto;
					m_gammaCurve = GAMMA_CURVE::PRO_PHOTO;
					m_gamma = GAMMA_EXPONENT_PRO_PHOTO;
					break;
				case STANDARD_PROFILE::RADIANCE:
					m_chromaticities = Chromaticities::Radiance;
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					break;
				default:
					throw "Unsupported standard profile!";
			}

			BuildTransformFromChroma( true );
		}

/*
		/// <summary>
		/// Creates the color profile from metadata embedded in the image file
		/// </summary>
		/// <param name="_MetaData"></param>
		/// <param name="_FileType"></param>
		ColorProfile( BitmapMetadata _MetaData, Bitmap.FILE_TYPE _FileType ) {
			string	MetaDump = _MetaData != nullptr ? DumpMetaData( _MetaData ) : nullptr;

			bool	bGammaFoundInFile = false;
			switch ( _FileType )
			{
				case Bitmap.FILE_TYPE.JPEG:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 2.2f;							// JPG uses a 2.2 gamma by default
					m_chromaticities = Chromaticities.sRGB;	// Default for JPEGs is sRGB
					EnumerateMetaDataJPG( _MetaData, out m_profileFoundInFile, out bGammaFoundInFile );

					if ( !m_profileFoundInFile && !bGammaFoundInFile )
						bGammaFoundInFile = true;			// Unless specified otherwise, we override the gamma no matter what since JPEGs use a 2.2 gamma by default anyway
					break;

				case Bitmap.FILE_TYPE.PNG:
					m_gammaCurve = GAMMA_CURVE::sRGB;
					m_gamma = GAMMA_EXPONENT_sRGB;
					m_chromaticities = Chromaticities.sRGB;	// Default for PNGs is standard sRGB
					EnumerateMetaDataPNG( _MetaData, out m_profileFoundInFile, out bGammaFoundInFile );
					break;

				case Bitmap.FILE_TYPE.TIFF:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;							// Linear gamma by default
					m_chromaticities = Chromaticities.sRGB;	// Default for TIFFs is sRGB
					EnumerateMetaDataTIFF( _MetaData, out m_profileFoundInFile, out bGammaFoundInFile );
					break;

				case Bitmap.FILE_TYPE.GIF:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					m_chromaticities = Chromaticities.sRGB;	// Default for GIFs is standard sRGB with no gamma
					break;

				case Bitmap.FILE_TYPE.BMP:	// BMP Don't have metadata!
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					m_chromaticities = Chromaticities.sRGB;	// Default for BMPs is standard sRGB with no gamma
					break;

				case Bitmap.FILE_TYPE.CRW:	// Raw files have no correction
				case Bitmap.FILE_TYPE.CR2:
				case Bitmap.FILE_TYPE.DNG:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					m_chromaticities = Chromaticities.sRGB;	// Default for BMPs is standard sRGB with no gamma
					break;
			}

			BuildTransformFromChroma( bGammaFoundInFile );
		}
*/

		/// <summary>
		/// Creates a color profile from chromaticities
		/// </summary>
		/// <param name="_Chromaticities">The chromaticities for this profile</param>
		/// <param name="_GammaCurve">The type of gamma curve to use</param>
		/// <param name="_Gamma">The gamma power</param>
		ColorProfile( const Chromaticities& _chromaticities, GAMMA_CURVE _gammaCurve, float _gamma ) {
			m_chromaticities = _chromaticities;
			m_gammaCurve = _gammaCurve;
			m_gamma = _gamma;

			BuildTransformFromChroma( true );
		}

		#pragma region IColorConverter Members

		/// <summary>
		/// Converts a CIEXYZ color to a RGB color
		/// </summary>
		/// <param name="_XYZ"></param>
		/// <returns></returns>
		void	XYZ2RGB( const float4& _XYZ, float4& _RGB ) {
			m_internalConverter->XYZ2RGB( _XYZ, _RGB );
		}

		/// <summary>
		/// Converts a RGB color to a CIEXYZ color
		/// </summary>
		/// <param name="_RGB"></param>
		/// <returns></returns>
		void	RGB2XYZ( const float4& _RGB, float4& _XYZ ) {
			m_internalConverter->RGB2XYZ( _RGB, _XYZ );
		}

		/// <summary>
		/// Converts a CIEXYZ color to a RGB color
		/// </summary>
		/// <param name="_XYZ"></param>
		void		XYZ2RGB( const float4* _XYZ, float4* _RGB, U32 _length ) {
			m_internalConverter->XYZ2RGB( _XYZ, _RGB, _length );
		}

		/// <summary>
		/// Converts a RGB color to a CIEXYZ color
		/// </summary>
		/// <param name="_RGB"></param>
		void		RGB2XYZ( const float4* _RGB, float4* _XYZ, U32 _length ) {
			m_internalConverter->RGB2XYZ( _RGB, _XYZ, _length );
		}

		#pragma endregion

	public:

		#pragma region Helpers

		/// <summary>
		/// Converts from XYZ to xyY
		/// </summary>
		/// <param name="_XYZ"></param>
		/// <returns></returns>
		static void	XYZ2xyY( const float3& _XYZ, float3& _xyY ) {
			float	InvSum = _XYZ.x + _XYZ.y + _XYZ.z;
					InvSum = InvSum > 1e-8f ? 1.0f / InvSum : 0.0f;
			_xyY.Set( _XYZ.x * InvSum, _XYZ.y * InvSum, _XYZ.y );
		}

		/// <summary>
		/// Converts from xyY to XYZ
		/// </summary>
		/// <param name="_xyY"></param>
		/// <returns></returns>
		static void	xyY2XYZ( const float3& _xyY, float3& _XYZ ) {
			float	Y_y = _xyY.y > 1e-8f ? _xyY.z / _xyY.y : 0.0f;
			_XYZ.Set( _xyY.x * Y_y, _xyY.z, (1.0f - _xyY.x - _xyY.y) * Y_y );
		}

		/// <summary>
		/// Applies gamma correction to the provided color
		/// </summary>
		/// <param name="c">The color to gamma-correct</param>
		/// <param name="_ImageGamma">The gamma correction the image was encoded with (JPEG is 2.2 for example, if not sure use 1.0)</param>
		/// <returns></returns>
		inline static float	GammaCorrect( float c, float _imageGamma ) {
			return powf( c, 1.0f / _imageGamma );
		}

		/// <summary>
		/// Un-aplies gamma correction to the provided color
		/// </summary>
		/// <param name="c">The color to gamma-uncorrect</param>
		/// <param name="_ImageGamma">The gamma correction the image was encoded with (JPEG is 2.2 for example, if not sure use 1.0)</param>
		/// <returns></returns>
		inline static float	GammaUnCorrect( float c, float _imageGamma ) {
			return powf( c, _imageGamma );
		}

		/// <summary>
		/// Converts from linear space to sRGB
		/// Code borrowed from D3DX_DXGIFormatConvert.inl from the DX10 SDK
		/// </summary>
		/// <param name="c"></param>
		/// <returns></returns>
		inline static float		Linear2sRGB( float c ) {
			if ( c < 0.0031308f )
				return c * 12.92f;
			return 1.055f * powf( c, 1.0f / GAMMA_EXPONENT_sRGB ) - 0.055f;
		}

		/// <summary>
		/// Converts from sRGB to linear space
		/// Code borrowed from D3DX_DXGIFormatConvert.inl from the DX10 SDK
		/// </summary>
		/// <param name="c"></param>
		/// <returns></returns>
		inline static float		sRGB2Linear( float c ) {
			if ( c < 0.04045f )
				return c / 12.92f;
			return powf( (c + 0.055f) / 1.055f, GAMMA_EXPONENT_sRGB );
		}

		#pragma endregion

	protected:

		#pragma region Color Space Transforms

		/// <summary>
		/// Builds the RGB<->XYZ transforms from chromaticities
		/// (refer to http://wiki.nuaj.net/index.php/Color_Transforms#XYZ_Matrices for explanations)
		/// </summary>
		void	BuildTransformFromChroma( bool _bCheckGammaCurveOverride ) {
			float3	xyz_R( m_chromaticities.R.x, m_chromaticities.R.y, 1.0f - m_chromaticities.R.x - m_chromaticities.R.y );
			float3	xyz_G( m_chromaticities.G.x, m_chromaticities.G.y, 1.0f - m_chromaticities.G.x - m_chromaticities.G.y );
			float3	xyz_B( m_chromaticities.B.x, m_chromaticities.B.y, 1.0f - m_chromaticities.B.x - m_chromaticities.B.y );
			float3	XYZ_W;
			xyY2XYZ( float3( m_chromaticities.W.x, m_chromaticities.W.y, 1.0f ), XYZ_W );

			float4x4	M_xyz;
			M_xyz.r[0].Set( xyz_R.x, xyz_R.y, xyz_R.z, 0.0f );
			M_xyz.r[1].Set( xyz_G.x, xyz_G.y, xyz_G.z, 0.0f );
			M_xyz.r[2].Set( xyz_B.x, xyz_B.y, xyz_B.z, 0.0f );
			M_xyz.r[3].Set( 0.0f, 0.0f, 0.0f, 1.0f );

			M_xyz.Invert();

			float4	Sum_RGB = float4( XYZ_W, 1.0f ) * M_xyz;

			// Finally, we can retrieve the RGB->XYZ transform
			m_RGB2XYZ.r[0].Set( Sum_RGB.x * xyz_R, 0.0f );
			m_RGB2XYZ.r[1].Set( Sum_RGB.y * xyz_G, 0.0f );
			m_RGB2XYZ.r[2].Set( Sum_RGB.z * xyz_B, 0.0f );

			// And the XYZ->RGB transform
			m_XYZ2RGB = m_RGB2XYZ;
			m_XYZ2RGB.Invert();

			// ============= Attempt to recognize a standard profile ============= 
			STANDARD_PROFILE	recognizedChromaticity = m_chromaticities.FindRecognizedChromaticity();

			if ( _bCheckGammaCurveOverride ) {
				// Also ensure the gamma ramp is correct before assigning a standard profile
				bool	bIsGammaCorrect = true;
				switch ( recognizedChromaticity ) {
					case STANDARD_PROFILE::sRGB:				bIsGammaCorrect = EnsureGamma( GAMMA_CURVE::sRGB, GAMMA_EXPONENT_sRGB ); break;
					case STANDARD_PROFILE::ADOBE_RGB_D50:	bIsGammaCorrect = EnsureGamma( GAMMA_CURVE::STANDARD, GAMMA_EXPONENT_ADOBE ); break;
					case STANDARD_PROFILE::ADOBE_RGB_D65:	bIsGammaCorrect = EnsureGamma( GAMMA_CURVE::STANDARD, GAMMA_EXPONENT_ADOBE ); break;
					case STANDARD_PROFILE::PRO_PHOTO:		bIsGammaCorrect = EnsureGamma( GAMMA_CURVE::PRO_PHOTO, GAMMA_EXPONENT_PRO_PHOTO ); break;
					case STANDARD_PROFILE::RADIANCE:			bIsGammaCorrect = EnsureGamma( GAMMA_CURVE::STANDARD, 1.0f ); break;
				}

				if ( !bIsGammaCorrect )
					recognizedChromaticity = STANDARD_PROFILE::CUSTOM;	// A non-standard gamma curves fails our pre-defined design...
			}


			// ============= Assign the internal converter depending on the profile =============
			switch ( recognizedChromaticity ) {
				case STANDARD_PROFILE::sRGB:
					m_gammaCurve = GAMMA_CURVE::sRGB;
					m_gamma = GAMMA_EXPONENT_sRGB;
					m_internalConverter = new InternalColorConverter_sRGB();
					break;

				case STANDARD_PROFILE::ADOBE_RGB_D50:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = GAMMA_EXPONENT_ADOBE;
					m_internalConverter = new InternalColorConverter_AdobeRGB_D50();
					break;

				case STANDARD_PROFILE::ADOBE_RGB_D65:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = GAMMA_EXPONENT_ADOBE;
					m_internalConverter = new InternalColorConverter_AdobeRGB_D65();
					break;

				case STANDARD_PROFILE::PRO_PHOTO:
					m_gammaCurve = GAMMA_CURVE::PRO_PHOTO;
					m_gamma = GAMMA_EXPONENT_PRO_PHOTO;
					m_internalConverter = new InternalColorConverter_ProPhoto();
					break;

				case STANDARD_PROFILE::RADIANCE:
					m_gammaCurve = GAMMA_CURVE::STANDARD;
					m_gamma = 1.0f;
					m_internalConverter = new InternalColorConverter_Radiance();
					break;

				default:	// Switch to one of our generic converters
					switch ( m_gammaCurve )
					{
						case GAMMA_CURVE::sRGB:
							m_internalConverter = new InternalColorConverter_Generic_sRGBGamma( m_RGB2XYZ, m_XYZ2RGB );
							break;
						case GAMMA_CURVE::PRO_PHOTO:
							m_internalConverter = new InternalColorConverter_Generic_ProPhoto( m_RGB2XYZ, m_XYZ2RGB );
							break;
						case GAMMA_CURVE::STANDARD:
							if ( fabs( m_gamma - 1.0f ) < 1e-3f )
								m_internalConverter = new InternalColorConverter_Generic_NoGamma( m_RGB2XYZ, m_XYZ2RGB );
							else
								m_internalConverter = new InternalColorConverter_Generic_StandardGamma( m_RGB2XYZ, m_XYZ2RGB, m_gamma );
							break;
					}
					break;
			}
		}

		/// <summary>
		/// Ensures the current gamma curve type and value are the ones we want
		/// </summary>
		/// <param name="_Curve"></param>
		/// <param name="_Gamma"></param>
		/// <returns></returns>
		bool	EnsureGamma( GAMMA_CURVE _Curve, float _Gamma ) const {
			return m_gammaCurve == _Curve && fabs( _Gamma - m_gamma ) < 1e-3f;
		}

		#pragma endregion

		#pragma endregion
	};
};