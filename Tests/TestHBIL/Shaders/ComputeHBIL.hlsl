////////////////////////////////////////////////////////////////////////////////
// Shaders to compute HBIL
////////////////////////////////////////////////////////////////////////////////
//
//#define AVERAGE_COSINES 1

#include "Global.hlsl"
#include "HBIL.hlsl"

static const float	GATHER_SPHERE_MAX_RADIUS_P = 100.0;	// Maximum radius (in pixels) that we allow our sphere to get

#define MAX_ANGLES	1									// Amount of circle subdivisions per pixel
#define MAX_SAMPLES	32									// Maximum amount of samples per circle subdivision

Texture2D< float >	_tex_depth : register(t0);			// Depth or distance buffer (here we're given depth)
Texture2D< float4 >	_tex_sourceRadiance : register(t1);	// Last frame's reprojected radiance buffer
Texture2D< float4 >	_tex_normal : register(t2);			// Camera-space normal vectors
Texture2D< float >	_tex_blueNoise : register(t3);

cbuffer CB_HBIL : register( b3 ) {
	float	_gatherSphereMaxRadius_m;		// Radius of the sphere that will gather our irradiance samples (in meters)
	float2	_bilateralValues;
};

float4	VS( float4 __Position : SV_POSITION ) : SV_POSITION { return __Position; }

////////////////////////////////////////////////////////////////////////////////
// Implement the methods expected by the HBIL header
float	FetchDepth( float2 _pixelPosition, float _mipLevel ) {
//	return _tex_sourceRadiance.SampleLevel( LinearClamp, _pixelPosition / _resolution, _mipLevel ).w;
	return Z_FAR * _tex_depth.SampleLevel( LinearClamp, _pixelPosition / _resolution, _mipLevel );
}

float3	FetchRadiance( float2 _pixelPosition, float _mipLevel ) {
	return _tex_sourceRadiance.SampleLevel( LinearClamp, _pixelPosition / _resolution, _mipLevel ).xyz;
}

float	BilateralFilterDepth( float _centralZ, float _neighborZ, float _radius_m ) {
//return 1.0;	// Accept always

	float	deltaZ = _neighborZ - _centralZ;

	#if 1
		// Relative test
		float	relativeZ = abs( deltaZ ) / _centralZ;
//		return smoothstep( _bilateralValues.y, _bilateralValues.x, relativeZ );
		return smoothstep( 0.4, 0.0, relativeZ );	// Discard when deltaZ is larger than 40% central Z (empirical value)
	#elif 0
		// Absolute test
		return smoothstep( _bilateralValues.y, _bilateralValues.x, abs(deltaZ) );
		return smoothstep( 1.0, 0.0, abs(deltaZ) );
	#else
		// Reject if outside of gather sphere radius
		float	r = _radius_m / _gatherSphereMaxRadius_m;
		float	sqSphereZ = 1.0 - r*r;
		return smoothstep( _bilateralValues.y*_bilateralValues.y * sqSphereZ, _bilateralValues.x*_bilateralValues.x * sqSphereZ, deltaZ*deltaZ );
		return smoothstep( 0.1*0.1 * sqSphereZ, 0.0 * sqSphereZ, deltaZ*deltaZ );	// Empirical values
	#endif
}
float	BilateralFilterRadiance( float _centralZ, float _neighborZ, float _radius_m ) {
//return 1.0;	// Accept always
//return 0.0;	// Reject always

	float	deltaZ = _neighborZ - _centralZ;

	#if 1
		// Relative test
		float	relativeZ = abs( deltaZ ) / _centralZ;
//		return smoothstep( 0.1*_bilateralValues.y, 0.1*_bilateralValues.x, relativeZ );	// Discard when deltaZ is larger than 1% central Z
//		return smoothstep( 0.015, 0.0, relativeZ );	// Discard when deltaZ is larger than 1.5% central Z (empirical value)
		return smoothstep( 0.15, 0.0, relativeZ );	// Discard when deltaZ is larger than 1.5% central Z (empirical value)
	#elif 0
		// Absolute test
		return smoothstep( 1.0, 0.0, abs(deltaZ) );
	#else
		// Reject if outside of gather sphere radius
		float	r = _radius_m / _gatherSphereMaxRadius_m;
		float	sqSphereZ = 1.0 - r*r;
		return smoothstep( _bilateralValues.y*_bilateralValues.y * sqSphereZ, _bilateralValues.x*_bilateralValues.x * sqSphereZ, deltaZ*deltaZ );
		return smoothstep( 0.1*0.1 * sqSphereZ, 0.0 * sqSphereZ, deltaZ*deltaZ );	// Empirical values
	#endif
}


float2	ComputeMipLevel( float2 _radius, float2 _radialStepSizes ) {
return 0;
	float	radiusPixel = _radius.x;
	float	deltaRadius = _radialStepSizes.x;
	float	pixelArea = PI / (2.0 * MAX_ANGLES) * 2.0 * radiusPixel * deltaRadius;
//	return 0.5 * log2( pixelArea ) * _bilateralValues;
	return 0.5 * log2( pixelArea ) * float2( 0, 2 );	// Unfortunately, sampling lower mips for depth gives very nasty halos! Maybe use max depth? Meh. Not conclusive either...
	return 1.5 * 0.5 * log2( pixelArea );
	return 0.5 * log2( pixelArea );
}



float3	SampleIrradiance_TEMP( float2 _csPosition, float4x3 _localCamera2World, float2 _mipLevel, float2 _integralFactors, inout float3 _previousRadiance, inout float _maxCos ) {

	// Transform camera-space position into screen space
	float3	wsPosition = _localCamera2World[3] + _csPosition.x * _localCamera2World[0] + _csPosition.y * _localCamera2World[1];
//	float3	deltaPos = wsPosition = _Camera2World[3].xyz;
//	float2	gcsPosition = float3( dot( deltaPos, _Camera2World[0].xyz ), dot( deltaPos, _Camera2World[1].xyz ), dot( deltaPos, _Camera2World[2].xyz ) );

// #TODO: Optimize!
float4	projPosition = mul( float4( wsPosition, 1.0 ), _World2Proj );
		projPosition.xyz /= projPosition.w;
float2	ssPosition = float2( 0.5 * (1.0 + projPosition.x) * _resolution.x, 0.5 * (1.0 - projPosition.y) * _resolution.y );


	// Sample new height and update horizon angle
	float	neighborZ = FetchDepth( ssPosition, _mipLevel.x );

	float3	csView = BuildCameraRay( _ssPosition / _resolution );
	float3	wsView = mul( float4( csView, 0 ), _Camera2World ).xyz;
	float3	wsPos = _Camera2World[3].xyz + neighborZ * wsView;
			wsPos -= _wsPos;	// Delta!
	float3	csPos = float3( dot( wsPos, _wsRight ), dot( wsPos, _wsUp ), dot( wsPos, _wsAt ) );

	float	radius = length( csPos.xy );
//			csPos.z *= BilateralFilterDepth( 0.0, csPos.z, _radius );

	float	H2 = csPos.z * csPos.z;
	float	hyp2 = radius * radius + H2;			// Square hypotenuse
	float	cosHorizon = csPos.z / sqrt( hyp2 );	// Cosine to horizon angle
	if ( cosHorizon <= _maxCos )
		return 0.0;	// Below the horizon... No visible contribution.

	#if SAMPLE_NEIGHBOR_RADIANCE
// NOW USELESS I THINK...
//		// Sample neighbor's incoming radiance value, only if difference in depth is not too large
//		float	bilateralWeight = BilateralFilterRadiance( _H0, neighborH, _radius );
//		if ( bilateralWeight > 0.0 )
//			_previousRadiance = lerp( _previousRadiance, FetchRadiance( _ssPosition ), bilateralWeight );	// Accept new height and its radiance value

		// Sample always (actually, it's okay now we accepted the height through the first bilateral filter earlier)
		_previousRadiance = FetchRadiance( _ssPosition, _mipLevel.y );
	#endif

	// Integrate over horizon difference (always from smallest to largest angle otherwise we get negative results!)
	float3	incomingRadiance = _previousRadiance * IntegrateSolidAngle( _integralFactors, cosHorizon, _maxCos );

// #TODO: Integrate with linear interpolation of irradiance as well??
// #TODO: Integrate with Fresnel F0!

	_maxCos = cosHorizon;		// Register a new positive horizon

	return incomingRadiance;
}

float3	GatherIrradiance_TEMP( float2 _ssPosition, float2 _ssDirection, float2 _csDirection, float4x3 _localCamera2World, float3 _csNormal, float _stepSize_meters, uint _stepsCount, float3 _centralRadiance, out float3 _csBentNormal, out float2 _coneAngles, inout float4 _DEBUG ) {

	// Pre-compute factors for the integrals
	float2	integralFactors_Front = ComputeIntegralFactors( _csDirection, _csNormal );
	float2	integralFactors_Back = ComputeIntegralFactors( -_csDirection, _csNormal );

	// Compute initial cos(angle) for front & back horizons
	// We do that by projecting the camera-space direction csDirection onto the tangent plane given by the normal
	//	then the cosine of the angle from the Z axis is simply given by the Pythagorean theorem:
	//                             P
	//			   N\  |Z		  -*-
	//				 \ |	  ---  ^
	//				  \|  ---      |
	//             --- *..........>+ csDirection
	//        --- 
	//
	float	hitDistance_Front = -dot( _csDirection, _csNormal.xy ) / _csNormal.z;
	float	maxCos_Front = hitDistance_Front / sqrt( hitDistance_Front*hitDistance_Front + 1.0 );	// Assuming length(_csDirection) == 1
	float	maxCos_Back = -maxCos_Front;	// Back cosine is simply the mirror value

	// Gather irradiance from front & back directions while updating the horizon angles at the same time
	float3	sumRadiance = 0.0;
	float3	previousRadiance_Front = _centralRadiance;
	float3	previousRadianceBack = _centralRadiance;
	float	radius_meters = 0.0;
//	float2	ssStep = _radialStepSizes.x * _ssDirection;
	float2	csStep = _stepSize_meters * _csDirection;
	float2	csPosition_Front = 0.0;
	float2	csPosition_Back = 0.0;
	for ( uint stepIndex=0; stepIndex < _stepsCount; stepIndex++ ) {
		radius_meters += _stepSize_meters;
		csPosition_Front += csStep;
		csPosition_Back -= csStep;

//		float2	mipLevel = ComputeMipLevel( radius, _radialStepSizes );
		float2	mipLevel = 0.0;

		sumRadiance += SampleIrradiance_TEMP( csPosition_Front, _localCamera2World, mipLevel, integralFactors_Front, previousRadiance_Front, maxCos_Front );
		sumRadiance += SampleIrradiance_TEMP( csPosition_Back, _localCamera2World, mipLevel, integralFactors_Back, previousRadianceBack, maxCos_Back );
	}

	// Accumulate bent normal direction by rebuilding and averaging the front & back horizon vectors
	#if USE_NUMERICAL_INTEGRATION
		// Half brute force where we perform the integration numerically as a sum...
		// This solution is prefered to the analytical integral that shows some precision artefacts unfortunately...
		//
		float	thetaFront = acos( maxCos_Front );
		float	thetaBack = -acos( maxCos_Back );

		_csBentNormal = 0.001 * _N;
		for ( uint i=0; i < USE_NUMERICAL_INTEGRATION; i++ ) {
			float	theta = lerp( thetaBack, thetaFront, (i+0.5) / USE_NUMERICAL_INTEGRATION );
			float	sinTheta, cosTheta;
			sincos( theta, sinTheta, cosTheta );
			float3	csUnOccludedDirection = float3( sinTheta * _csDirection, cosTheta );

			float	cosAlpha = saturate( dot( csUnOccludedDirection, _N ) );

			float	weight = cosAlpha * abs(sinTheta);		// cos(alpha) * sin(theta).dTheta  (be very careful to take abs(sin(theta)) because our theta crosses the pole and becomes negative here!)
			_csBentNormal += weight * csUnOccludedDirection;
		}

		float	dTheta = (thetaFront - thetaBack) / USE_NUMERICAL_INTEGRATION;
		_csBentNormal *= dTheta;
	#else
		// Analytical solution
		float	cosTheta0 = maxCos_Front;
		float	cosTheta1 = maxCos_Back;
		float	sinTheta0 = sqrt( 1.0 - cosTheta0*cosTheta0 );
		float	sinTheta1 = sqrt( 1.0 - cosTheta1*cosTheta1 );
		float	cosTheta0_3 = cosTheta0*cosTheta0*cosTheta0;
		float	cosTheta1_3 = cosTheta1*cosTheta1*cosTheta1;
		float	sinTheta0_3 = sinTheta0*sinTheta0*sinTheta0;
		float	sinTheta1_3 = sinTheta1*sinTheta1*sinTheta1;

		float2	sliceSpaceNormal = float2( dot( _csNormal.xy, _csDirection ), _csNormal.z );

		float	averageX = sliceSpaceNormal.x * (cosTheta0_3 + cosTheta1_3 - 3.0 * (cosTheta0 + cosTheta1) + 4.0)
						 + sliceSpaceNormal.y * (sinTheta0_3 - sinTheta1_3);

		float	averageY = sliceSpaceNormal.x * (sinTheta0_3 - sinTheta1_3)
						 + sliceSpaceNormal.y * (2.0 - cosTheta0_3 - cosTheta1_3);

		_csBentNormal = float3( averageX * _csDirection, averageY );
	#endif

	_csBentNormal = normalize( _csBentNormal );

	// Compute cone angles
	float3	csHorizon_Front = float3( sqrt( 1.0 - maxCos_Front*maxCos_Front ) * _csDirection, maxCos_Front );
	float3	csHorizon_Back = float3( -sqrt( 1.0 - maxCos_Back*maxCos_Back ) * _csDirection, maxCos_Back );
	#if USE_FAST_ACOS
		_coneAngles.x = FastPosAcos( saturate( dot( _csBentNormal, csHorizon_Front ) ) );
		_coneAngles.y = FastPosAcos( saturate( dot( _csBentNormal, csHorizon_Back ) ) ) ;
	#else
		_coneAngles.x = acos( saturate( dot( _csBentNormal, csHorizon_Front ) ) );
		_coneAngles.y = acos( saturate( dot( _csBentNormal, csHorizon_Back ) ) );
	#endif


#if AVERAGE_COSINES
_coneAngles = float2( saturate( dot( _csBentNormal, csHorizon_Front ) ), saturate( dot( _csBentNormal, csHorizon_Back ) ) );
#endif


//_DEBUG = float4( _coneAngles, 0, 0 );
_DEBUG = _coneAngles.x / (0.5*PI);


	return sumRadiance;
}


////////////////////////////////////////////////////////////////////////////////
// Computes bent cone & irradiance gathering from pixel's surroundings
struct PS_OUT {
	float4	irradiance : SV_TARGET0;
	float4	bentCone : SV_TARGET1;
};

PS_OUT	PS( float4 __Position : SV_POSITION ) {
	float2	UV = __Position.xy / _resolution;
	uint2	pixelPosition = uint2( floor( __Position.xy ) );
	float	noise = _tex_blueNoise[pixelPosition & 0x3F];

	// Setup camera ray
	float3	csView = BuildCameraRay( UV );
	float	Z2Distance = length( csView );
			csView /= Z2Distance;
	float3	wsView = mul( float4( csView, 0.0 ), _Camera2World ).xyz;

	// Read back depth, normal & central radiance value from last frame
	float	Z = FetchDepth( pixelPosition, 0.0 );

			Z -= 1e-2;	// !IMPORTANT! Prevent acnea by offseting the central depth a tiny bit closer

//	float	distance = Z * Z2Distance;
	float3	wsNormal = normalize( _tex_normal[pixelPosition].xyz );

	float3	centralRadiance = _tex_sourceRadiance[pixelPosition].xyz;	// Read back last frame's radiance value that we can use as a fallback for neighbor areas

	// Compute local camera-space
	float3		wsPos = _Camera2World[3].xyz + Z * Z2Distance * wsView;
	float3		wsRight = normalize( cross( wsView, _Camera2World[1].xyz ) );
	float3		wsUp = cross( wsRight, wsView );
	float3		wsAt = -wsView;
	float4x3	localCamera2World = float4x3( wsRight, wsUp, wsAt, wsPos );

	// Compute local camera-space normal
	float3	N = float3( dot( wsNormal, wsRight ), dot( wsNormal, wsUp ), dot( wsNormal, wsAt ) );
			N.z = max( 1e-4, N.z );	// Make sure it's never 0!
//	float3	T, B;
//	BuildOrthonormalBasis( N, T, B );

	// Compute screen radius of gather sphere
	float	screenSize_m = 2.0 * Z * TAN_HALF_FOV;	// Vertical size of the screen in meters when extended to distance Z
	float	sphereRadius_pixels = _resolution.y * _gatherSphereMaxRadius_m / screenSize_m;
			sphereRadius_pixels = min( GATHER_SPHERE_MAX_RADIUS_P, sphereRadius_pixels );							// Prevent it to grow larger than our fixed limit
	float	radiusStepSize_pixels = max( 1.0, sphereRadius_pixels / MAX_SAMPLES );									// This gives us our radial step size in pixels
	uint	samplesCount = clamp( uint( ceil( sphereRadius_pixels / radiusStepSize_pixels ) ), 1, MAX_SAMPLES );	// Reduce samples count if possible
	float	radiusStepSize_meters = sphereRadius_pixels * screenSize_m / (samplesCount * _resolution.y);			// This gives us our radial step size in meters

	// Start gathering radiance and bent normal by subdividing the screen-space disk around our pixel into N slices
	float4	GATHER_DEBUG = 0.0;
	float3	sumIrradiance = 0.0;
	float3	csAverageBentNormal = 0.0;
	float	averageConeAngle = 0.0;
	float	varianceConeAngle = 0.0;
#if MAX_ANGLES > 1
	for ( uint angleIndex=0; angleIndex < MAX_ANGLES; angleIndex++ )
#else
	uint	angleIndex = 0;
#endif
	{
		float	phi = (angleIndex + noise) * PI / MAX_ANGLES;

phi = 0.0;

		// Build camera-space and screen-space walk directions
		float2	csDirection;
		sincos( phi, csDirection.y, csDirection.x );

		float3	wsDirection = csDirection.x * wsRight + csDirection.y * wsUp;
		float2	ssDirection = normalize( float2( dot( wsDirection, _Camera2World[0] ), dot( wsDirection, _Camera2World[1] ) );

		// Gather irradiance and average cone direction for that slice
		float3	csBentNormal;
		float2	coneAngles;
		sumIrradiance += GatherIrradiance_TEMP( __Position.xy, ssDirection, csDirection, localCamera2World, N, radiusStepSize_meters, samplesCount, centralRadiance, csBentNormal, coneAngles, GATHER_DEBUG );

		csAverageBentNormal += csBentNormal;

		// We're using running variance computation from https://www.johndcook.com/blog/standard_deviation/
		//	Avg(N) = Avg(N-1) + [V(N) - Avg(N-1)] / N
		//	S(N) = S(N-1) + [V(N) - Avg(N-1)] * [V(N) - Avg(N)]
		// And variance = S(finalN) / (finalN-1)
		//
		float	previousAverageConeAngle = averageConeAngle;
		averageConeAngle += (coneAngles.x - averageConeAngle) / (2*angleIndex+1);
		varianceConeAngle += (coneAngles.x - previousAverageConeAngle) * (coneAngles.x - averageConeAngle);

		previousAverageConeAngle = averageConeAngle;
		averageConeAngle += (coneAngles.y - averageConeAngle) / (2*angleIndex+2);
		varianceConeAngle += (coneAngles.y - previousAverageConeAngle) * (coneAngles.y - averageConeAngle);
	}

	// Finalize bent cone & irradiance
	#if MAX_ANGLES > 1
		varianceConeAngle /= 2.0*MAX_ANGLES - 1.0;
	#endif
	csAverageBentNormal = normalize( csAverageBentNormal );
	float	stdDeviation = sqrt( varianceConeAngle );



#if AVERAGE_COSINES
averageConeAngle = acos( averageConeAngle );
varianceConeAngle = acos( varianceConeAngle );
#endif



	sumIrradiance *= PI / MAX_ANGLES;

//sumIrradiance += float3( 0.1, 0, 0 );

	sumIrradiance = max( 0.0, sumIrradiance );

//sumIrradiance = float3( 1, 0, 1 );

float3	DEBUG_VALUE = float3( 1,0,1 );
DEBUG_VALUE = csAverageBentNormal;
DEBUG_VALUE = csAverageBentNormal.x * wsRight - csAverageBentNormal.y * wsUp - csAverageBentNormal.z * wsAt;	// World-space normal
//DEBUG_VALUE = cos( averageConeAngle );
//DEBUG_VALUE = dot( ssAverageBentNormal, N );
//DEBUG_VALUE = 0.01 * Z;
//DEBUG_VALUE = sphereRadius_pixels / GATHER_SPHERE_MAX_RADIUS_P;
//DEBUG_VALUE = 0.1 * (radiusStepSize_pixels-1);
//DEBUG_VALUE = 0.5 * float(samplesCount) / MAX_SAMPLES;
//DEBUG_VALUE = varianceConeAngle;
//DEBUG_VALUE = stdDeviation;
//DEBUG_VALUE = float3( GATHER_DEBUG.xy, 0 );
//DEBUG_VALUE = float3( GATHER_DEBUG.zw, 0 );
DEBUG_VALUE = GATHER_DEBUG.xyz;
//DEBUG_VALUE = N;

	PS_OUT	Out;
	Out.irradiance = float4( sumIrradiance, 0 );
	Out.bentCone = float4( max( 0.01, cos( averageConeAngle ) ) * csAverageBentNormal, 1.0 - stdDeviation / (0.5 * PI) );

//Out.bentCone = float4( DEBUG_VALUE, 1 );

	return Out;
}