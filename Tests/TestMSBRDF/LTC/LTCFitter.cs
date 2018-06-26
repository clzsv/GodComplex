﻿//////////////////////////////////////////////////////////////////////////
// Fitter class for Linearly-Transformed Cosines
// From "Real-Time Polygonal-Light Shading with Linearly Transformed Cosines" (https://eheitzresearch.wordpress.com/415-2/)
// This is a C# re-implementation of the code provided by Heitz et al.
// UPDATE: Using code from Stephen Hill's github repo instead (https://github.com/selfshadow/ltc_code/tree/master/fit)
//////////////////////////////////////////////////////////////////////////
//
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

using SharpMath;

namespace TestMSBRDF.LTC
{
	public class LTCFitter {

		#region CONSTANTS

		const int		MAX_ITERATIONS = 1000;
		const float		FIT_EXPLORE_DELTA = 0.05f;
		const float		TOLERANCE = 1e-5f;
		const float		MIN_ALPHA = 0.0001f;		// minimal roughness (avoid singularities)

		#endregion

		#region NESTED TYPES

		[System.Diagnostics.DebuggerDisplay( "m11={m11}, m22={m22}, m13={m13}, m23={m23} - Amplitude = {amplitude}" )]
		public class LTC {

			// lobe amplitude
			public float		amplitude = 1;

			// Average Schlick Fresnel term
			public float		fresnel = 1;

			// parametric representation
			public float		m11 = 1, m22 = 1, m13 = 0, m23 = 0;
			public float3		X = float3.UnitX;
			public float3		Y = float3.UnitY;
			public float3		Z = float3.UnitZ;

			// Matrix representation
			public float3x3		M;
			public float3x3		invM;
			public float		detM;
//			public float		detInvM;	// Simply 1/det(M)

			public LTC() {
				Update();
			}
			public LTC( System.IO.BinaryReader R ) {
				Read( R );
				Update();
			}

			/// <summary>
			/// Sets the coefficients for the M matrix (warning! NOT the inverse matrix used at runtime!)
			/// </summary>
			/// <param name="_parameters"></param>
			/// <param name="_isotropic"></param>
			public void	Set( double[] _parameters, bool _isotropic ) {
				float	tempM11 = (float) Math.Max( _parameters[0], 1e-7 );
				float	tempM22 = (float) Math.Max( _parameters[1], 1e-7 );
				float	tempM13 = (float) _parameters[2];
				float	tempM23 = _parameters.Length > 3 ? (float) _parameters[3] : 0;

				if ( _isotropic ) {
					m11 = tempM11;
					m22 = tempM11;
					m13 = 0.0f;
					m23 = 0.0f;
				} else {
					m11 = tempM11;
					m22 = tempM22;
					m13 = tempM13;
					m23 = tempM23;
				}
				// Update the matrix
				Update();
			}
			public void		Update() {
				M = new float3x3( X, Y, Z );
				float3x3	temp = new float3x3(	m11,	0,		m13,
													0,		m22,	0,
													m23,	0,		1 );
				M *= temp;
				invM = M.Inverse;
				detM = M.Determinant;
// 				detInvM = invM.Determinant;
// 				if ( Math.Abs( detInvM * detM - 1.0f ) > 1e-4f )
// 					throw new Exception( "KOIN!" );
			}

// 			/// <summary>
// 			/// Sets the coefficients for the invese M matrix (warning! NOT the matrix used at fitting time!)
// 			/// </summary>
// 			/// <param name="_parameters"></param>
// 			/// <param name="_isotropic"></param>
// 			public void	SetInverse( double[] _parameters, bool _isotropic ) {
// 				float	tempM11 = (float) Math.Max( _parameters[0], 1e-7 );
// 				float	tempM22 = (float) Math.Max( _parameters[1], 1e-7 );
// 				float	tempM13 = (float) _parameters[2];
// //				float	tempM31 = (float) _parameters[3];
// float	tempM31 = 0;
// 
// 				if ( _isotropic ) {
// 					m11 = tempM11;
// 					m22 = tempM11;
// 					m13 = 0.0f;
// 					m23 = 0.0f;
// 				} else {
// 					m11 = tempM11;
// 					m22 = tempM22;
// 					m13 = tempM13;
// 					m23 = tempM31;
// 				}
// 				// Update the inverse matrix
// 				UpdateInverse();
// 			}
// 			public void		UpdateInverse() {
// 				invM = new float3x3( X, Y, Z );
// 				float3x3	temp = new float3x3(	m11,	0,		0,
// 													0,		m22,	0,
// 													m13,	m23,	1 );
// 				invM *= temp;
// 				M = invM.Inverse;
// 				detM = M.Determinant;
// // 				detInvM = invM.Determinant;
// // 				if ( Math.Abs( detInvM * detM - 1.0f ) > 1e-4f )
// // 					throw new Exception( "KOIN!" );
// 			}

			public float Eval( ref float3 _tsLight ) {
				// Transform into original distribution space
				float3	Loriginal = invM * _tsLight;
				float	l = Loriginal.Length;
						Loriginal /= l;

				// Estimate original distribution (a clamped cosine lobe)
				float	D = Mathf.INVPI * Math.Max( 0.0f, Loriginal.z ); 

				// Compute the Jacobian, roundDwo / roundDw
				float	detInvM  = 1.0f / detM;
				float	jacobian = detInvM / (l*l*l);

				// Scale distribution
				float	res = amplitude * D * jacobian;
				return res;
			}

			public void	GetSamplingDirection( float _U1, float _U2, ref float3 _direction ) {
				float	theta = Mathf.Asin( Mathf.Sqrt( _U1 ) );
				float	phi = Mathf.TWOPI * _U2;
				_direction = M * new float3( Mathf.Sin(theta)*Mathf.Cos(phi), Mathf.Sin(theta)*Mathf.Sin(phi), Mathf.Cos(theta) );
				_direction.Normalize();
			}

			public double	TestNormalization() {
				double	sum = 0;
				float	dtheta = 0.005f;
				float	dphi = 0.005f;
				float3	L = new float3();
				for( float theta = 0.0f; theta <= Mathf.PI; theta+=dtheta ) {
					for( float phi = 0.0f; phi <= Mathf.TWOPI; phi+=dphi ) {
						L.Set( Mathf.Sin(theta)*Mathf.Cos(phi), Mathf.Sin(theta)*Mathf.Sin(phi), Mathf.Cos(theta) );
						sum += Mathf.Sin(theta) * Eval( ref L );
					}
				}
				sum *= dtheta * dphi;
				return sum;
			}

			public void	Read( System.IO.BinaryReader R ) {
				m11 = R.ReadSingle();
				m22 = R.ReadSingle();
				m13 = R.ReadSingle();
				m23 = R.ReadSingle();
				amplitude = R.ReadSingle();
				fresnel = R.ReadSingle();

				X.x = R.ReadSingle();
				X.y = R.ReadSingle();
				X.z = R.ReadSingle();
				Y.x = R.ReadSingle();
				Y.y = R.ReadSingle();
				Y.z = R.ReadSingle();
				Z.x = R.ReadSingle();
				Z.y = R.ReadSingle();
				Z.z = R.ReadSingle();
			}

			public void	Write( System.IO.BinaryWriter W ) {
				W.Write( m11 );
				W.Write( m22 );
				W.Write( m13 );
				W.Write( m23 );
				W.Write( amplitude );
				W.Write( fresnel );

				W.Write( X.x );
				W.Write( X.y );
				W.Write( X.z );
				W.Write( Y.x );
				W.Write( Y.y );
				W.Write( Y.z );
				W.Write( Z.x );
				W.Write( Z.y );
				W.Write( Z.z );
			}
		}

		#endregion

		FitterForm	m_debugForm = null;

		public LTCFitter( Form _ownerForm ) {
			if ( _ownerForm == null )
				return;	// No debug...

			m_debugForm = new FitterForm( this );
			m_debugForm.Show( _ownerForm );
		}

		public LTC[,]	Fit( IBRDF _brdf, int _tableSize, System.IO.FileInfo _tableFileName ) {

			NelderMead	NMFitter = new NelderMead( 4 );

 			double[]	startFit = new double[4];
 			double[]	resultFit = new double[4];
			double[]	runtimeCoefficients = new double[4];

// float3x3	invM = new float3x3( new float[] { 499.999756f, 0, 0, 0, 499.999756f, 0, 0, 0, 1 } );
// float3x3	M = invM.Inverse;
// float3x3	test = M * invM;
// float3x3	invM2 = new float3x3( new float[] { 500.003906f, 0, -0.074939f, 0, 502.811340f, 0, 37.469978f, 0, 1} );
// float3x3	M2 = invM2.Inverse;
// float3x3	test2 = M2 * invM2;


			LTC[,]		result = new LTC[_tableSize,_tableSize];

			if ( _tableFileName.Exists )
				result = Load( _tableFileName );

			// loop over theta and alpha
			int	count = 0;
//			for ( int roughnessIndex=_tableSize-1; roughnessIndex >= 0; --roughnessIndex ) {
for ( int roughnessIndex=20; roughnessIndex >= 0; --roughnessIndex ) {

				for ( int thetaIndex=0; thetaIndex <= _tableSize-1; ++thetaIndex ) {
					if ( result[roughnessIndex,thetaIndex] != null )
						continue;	// Already computed!

// 					float	theta = t * Mathf.HALFPI / (_tableSize-1);
// 					float3	V = new float3( Mathf.Sin(theta), 0 , Mathf.Cos(theta) );

					// parameterised by sqrt(1 - cos(theta))
					float	x = (float) thetaIndex / (_tableSize - 1);
					float	cosTheta = 1.0f - x*x;
							cosTheta = Mathf.Max( 3.7540224885647058065387021283285e-4f, cosTheta );	// Clamp to cos(1.57)
					float3	V = new float3( Mathf.Sqrt( 1 - cosTheta*cosTheta ), 0, cosTheta );

					// alpha = perceptualRoughness^2  (perceptualRoughness = "sRGB" representation of roughness, as painted by artists)
					float perceptualRoughness = (float) roughnessIndex / (_tableSize-1);
					float alpha = Mathf.Max( MIN_ALPHA, perceptualRoughness * perceptualRoughness );

					LTC	ltc = new LTC();
					result[roughnessIndex,thetaIndex] = ltc;

					float3	averageDir;
					ComputeAverageTerms( _brdf, ref V, alpha, out ltc.amplitude, out ltc.fresnel, out averageDir );		

					// 1. first guess for the fit
					// init the hemisphere in which the distribution is fitted
					// if theta == 0 the lobe is rotationally symmetric and aligned with Z = (0 0 1)
					bool	isotropic;
					if ( thetaIndex == 0 ) {
						ltc.X = float3.UnitX;
						ltc.Y = float3.UnitY;
						ltc.Z = float3.UnitZ;

						if ( roughnessIndex == _tableSize-1 || result[roughnessIndex+1,thetaIndex] == null ) {
							// roughness = 1 or no available result
							ltc.m11 = 1.0f;
							ltc.m22 = 1.0f;
						} else {
							// init with roughness of previous fit
							ltc.m11 = Mathf.Max( result[roughnessIndex+1,thetaIndex].m11, MIN_ALPHA );
							ltc.m22 = Mathf.Max( result[roughnessIndex+1,thetaIndex].m22, MIN_ALPHA );
						}

						ltc.m13 = 0;
						ltc.m23 = 0;
						ltc.Update();

						isotropic = true;
					} else {
						// otherwise use previous configuration as first guess
//						ltc.X.Set( averageDir.z, 0, -averageDir.x );
//						ltc.Y = float3.UnitY;
//						ltc.Z = averageDir;
						ltc.X = float3.UnitX;
						ltc.Y = float3.UnitY;
						ltc.Z = float3.UnitZ;

						// Copy previous result
						LTC	previousLTC = result[roughnessIndex,thetaIndex-1];
						ltc.m11 = previousLTC.m11;
						ltc.m22 = previousLTC.m22;
						ltc.m13 = previousLTC.m13;
						ltc.m23 = previousLTC.m23;

						ltc.Update();

						isotropic = false;
					}

					// 2. fit (explore parameter space and refine first guess)
					startFit[0] = ltc.m11;
					startFit[1] = ltc.m22;
					startFit[2] = ltc.m13;
					startFit[3] = ltc.m23;

					// Find best-fit LTC lobe (scale, alphax, alphay)
//					double	error = 0;
					double	error = NMFitter.FindFit( resultFit, startFit, FIT_EXPLORE_DELTA, TOLERANCE, MAX_ITERATIONS, ( double[] _parameters ) => {
						ltc.Set( _parameters, isotropic );

						double	currentError = ComputeError( ltc, _brdf, ref V, alpha );
						return currentError;
					} );

					// Update LTC with best fitting values
					ltc.Set( resultFit, isotropic );

//					ltc.TestNormalization();

/*					// Now, at runtime, we need the inverse matrix
// 					ltc.X = float3.UnitX;
// 					ltc.Y = float3.UnitY;
// 					ltc.Z = float3.UnitZ;

					ltc.M.r0.y = 0;
					ltc.M.r1.x = 0;
					ltc.M.r1.z = 0;
					ltc.M.r2.y = 0;
					ltc.invM = ltc.M.Inverse;

					runtimeCoefficients[0] = ltc.invM.r0.x;
					runtimeCoefficients[1] = ltc.invM.r1.y;
					runtimeCoefficients[2] = ltc.invM.r2.x;
					runtimeCoefficients[3] = ltc.invM.r2.y;

// 				        // normalize by the middle element
// 					runtimeCoefficients[0] /= runtimeCoefficients[1];
// 					runtimeCoefficients[2] /= runtimeCoefficients[1];
// 					runtimeCoefficients[3] /= runtimeCoefficients[1];
// 					runtimeCoefficients[1] = 1.0;

					ltc.SetInverse( runtimeCoefficients, isotropic );
//*/
					// Show debug form
					++count;
//					if ( m_debugForm != null && (count & 0xF) == 1 ) {
					if ( m_debugForm != null ) {
						m_debugForm.ShowBRDF( (float) count / (_tableSize*_tableSize), (float) error, Mathf.Acos( cosTheta ), alpha, _brdf, ltc );
					}
				}

				Save( _tableFileName, result );
			}

			return result;
		}

		#region I/O

		LTC[,]	Load( System.IO.FileInfo _tableFileName ) {
			LTC[,]	result = null;
			using ( System.IO.FileStream S = _tableFileName.OpenRead() )
				using ( System.IO.BinaryReader R = new System.IO.BinaryReader( S ) ) {
					result = new LTC[R.ReadUInt32(), R.ReadUInt32()];
					for ( uint Y=0; Y < result.GetLength( 1 ); Y++ ) {
						for ( uint X=0; X < result.GetLength( 0 ); X++ ) {
							if ( R.ReadBoolean() ) {
								result[X,Y] = new LTC( R );
							}
						}
					}
				}

			return result;
		}

		void	Save( System.IO.FileInfo _tableFileName, LTC[,] _table ) {
			using ( System.IO.FileStream S = _tableFileName.Create() )
				using ( System.IO.BinaryWriter W = new System.IO.BinaryWriter( S ) ) {
					W.Write( _table.GetLength( 0 ) );
					W.Write( _table.GetLength( 1 ) );
					for ( uint Y=0; Y < _table.GetLength( 1 ); Y++ )
						for ( uint X=0; X < _table.GetLength( 0 ); X++ ) {
							LTC	ltc = _table[X,Y];
							if ( ltc == null ) {
								W.Write( false );
								continue;
							}

							W.Write( true );
							ltc.Write( W );
						}
				}
		}

		#endregion

		#region Objective Function

		const int	SAMPLES_COUNT = 50;			// number of samples used to compute the error during fitting

		// compute the average direction of the BRDF
		void	ComputeAverageTerms( IBRDF brdf, ref float3 V, float alpha, out float _norm, out float _fresnel, out float3 _averageDir ) {
			_norm = 0.0f;
			_fresnel = 0.0f;
			_averageDir = float3.Zero;

			float	weight, pdf, eval;
			float3	L = float3.Zero;
			float3	H = float3.Zero;
			for ( int j = 0 ; j < SAMPLES_COUNT ; ++j ) {
				for ( int i = 0 ; i < SAMPLES_COUNT ; ++i ) {
					float U1 = (i+0.5f) / SAMPLES_COUNT;
					float U2 = (j+0.5f) / SAMPLES_COUNT;

					// sample
					brdf.GetSamplingDirection( ref V, alpha, U1, U2, ref L );

					// eval
					eval = brdf.Eval( ref V, ref L, alpha, out pdf );
					if ( pdf == 0.0f )
						continue;

					H = (V + L).Normalized;

					// accumulate
					weight = eval / pdf;

					_norm += weight;
					_fresnel += weight * Mathf.Pow( 1 - V.Dot( H ), 5.0f );
					_averageDir += weight * L;
				}
			}
			_norm /= SAMPLES_COUNT*SAMPLES_COUNT;
			_fresnel /= SAMPLES_COUNT*SAMPLES_COUNT;

			// clear y component, which should be zero with isotropic BRDFs
			_averageDir.y = 0.0f;
			_averageDir.Normalize();
		}

		// compute the error between the BRDF and the LTC
		// using Multiple Importance Sampling
		double	ComputeError( LTC _ltc, IBRDF _BRDF, ref float3 _tsView, float _alpha ) {
			float3	tsLight = float3.Zero;

			float	pdf_brdf, eval_brdf;
			float	pdf_ltc, eval_ltc;

			double	error = 0.0;
			for ( int j = 0 ; j < SAMPLES_COUNT ; ++j ) {
				for ( int i = 0 ; i < SAMPLES_COUNT ; ++i ) {
					float	U1 = (i+0.5f) / SAMPLES_COUNT;
					float	U2 = (j+0.5f) / SAMPLES_COUNT;

					// importance sample LTC
					{
						// sample
						_ltc.GetSamplingDirection( U1, U2, ref tsLight );
				
						// error with MIS weight
						eval_brdf = _BRDF.Eval( ref _tsView, ref tsLight, _alpha, out pdf_brdf );
						eval_ltc = _ltc.Eval( ref tsLight );
						pdf_ltc = eval_ltc / _ltc.amplitude;
						double	error_ = Math.Abs( eval_brdf - eval_ltc );
								error_ = error_*error_*error_;
						error += error_ / (pdf_ltc + pdf_brdf);
					}

					// importance sample BRDF
					{
						// sample
						_BRDF.GetSamplingDirection( ref _tsView, _alpha, U1, U2, ref tsLight );

						// error with MIS weight
						eval_brdf = _BRDF.Eval( ref _tsView, ref tsLight, _alpha, out pdf_brdf );			
						eval_ltc = _ltc.Eval( ref tsLight );
						pdf_ltc = eval_ltc / _ltc.amplitude;
						double	error_ = Math.Abs( eval_brdf - eval_ltc );
								error_ = error_*error_*error_;
						error += error_ / (pdf_ltc + pdf_brdf);
					}
				}
			}

			error /= SAMPLES_COUNT * SAMPLES_COUNT;
			return error;
		}

		#endregion
	}
}
