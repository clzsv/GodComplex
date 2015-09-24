﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

using RendererManaged;

namespace TestConvexHull
{
	public struct Plane {
		public float3	position;
		public float3	normal;
	}

	public partial class TestForm : Form, IComparer< Plane >
	{
		const int			PLANES_COUNT = 64;

		public Plane[]		m_planes = new Plane[PLANES_COUNT];
		public Plane[]		m_convexHull = null;

		public TestForm()
		{
			InitializeComponent();

			panelOutput.m_Owner = this;

			// Generate random planes
			Random	RNG = new Random( 1 );
			float3	probeProsition = float3.Zero;

			List< Plane >	tempPlanes = new List< Plane >();
			for ( int planeIndex=0; planeIndex < PLANES_COUNT; planeIndex++ ) {
				double	distance = 5.0 + 5.0 * (1.0 + Math.Sin( 8.0 * Math.PI * planeIndex / PLANES_COUNT ));
				double	refAngle = 2.0 * Math.PI * planeIndex / PLANES_COUNT;
				double	deltaAngle = Math.Atan( -5.0 * 8.0 * Math.PI * Math.Cos( 8.0 * Math.PI * planeIndex / PLANES_COUNT ) / PLANES_COUNT );
				double	angle = refAngle + deltaAngle;

				float3	direction = new float3( (float) Math.Cos( refAngle ), (float) Math.Sin( refAngle ), 0.0f );
				tempPlanes.Add( new Plane() { normal = new float3( (float) Math.Cos( angle ), (float) Math.Sin( angle ), 0.0f ), position = probeProsition - (float) distance * direction } );
			}
			for ( int i=0; i < tempPlanes.Count; i++ ) {
				int	j = RNG.Next( tempPlanes.Count );
				Plane	temp = tempPlanes[i];
				tempPlanes[i] = tempPlanes[j];
				tempPlanes[j] = temp;
			}
			m_planes = tempPlanes.ToArray();

			// Go!
			m_convexHull = BuildConvexHull( float3.Zero, new Plane[] { m_planes[0] }, m_planes, (float) Math.PI / 20.0f );

			panelOutput.UpdateBitmap();
		}

		Plane[]	BuildConvexHull( float3 _Center, Plane[] _ExistingPlanes, Plane[] _PlanesForHull, float _MinAngleBetweenPlane ) {
			m_center = _Center;
			float	cosMinAngle = (float) Math.Cos( _MinAngleBetweenPlane );

			List< Plane >	planes = new List< Plane >( _PlanesForHull );	// List of candidate planes for hull
			List< Plane >	results = new List< Plane >( _ExistingPlanes );	// List of planes already used for hull

// 			planes.Sort( this );
// 			foreach ( Plane P0 in planes ) {
// 				bool	isValid = true;
// 				foreach ( Plane P1 in results ) {
// 					float	cosAngle = P1.normal.Dot( P0.normal );
// 					if ( cosAngle > cosMinAngle ) {
// 						isValid = false;	// Too close to an existing plane!
// 						break;
// 					}
// 				}
// 
// 				if ( isValid ) {
// 					results.Add( P0 );	// Now part of the convex hull!
// 				}
// 			}

			while ( planes.Count > 0 ) {

				float	bestCandidateDistance = 0.0f;
				int		bestCandidateIndex = -1;
				for ( int candidateIndex=0; candidateIndex < planes.Count; candidateIndex++ ) {
					Plane	candidate = planes[candidateIndex];

					// Check if the candidate's normal is far enough from existing hull's normals
					bool	isValid = true;
					foreach ( Plane P in results ) {
						float	cosAngle = P.normal.Dot( candidate.normal );
						if ( cosAngle > cosMinAngle ) {
							isValid = false;	// Too close to an existing plane!
							break;
						}
					}
					if ( !isValid ) {
						// Remove the plane from the list of candidates
						planes.RemoveAt( candidateIndex );
						candidateIndex--;
						continue;
					}

					// Keep the best plane with the largest distance from the center
					float	candidateDistance = (m_center - candidate.position).Dot( candidate.normal );
					if ( candidateDistance > bestCandidateDistance ) {
						bestCandidateDistance = candidateDistance;
						bestCandidateIndex = candidateIndex;
					}
				}

				if ( bestCandidateIndex >= 0 )
					results.Add( planes[bestCandidateIndex] );
			}

			return results.ToArray();
		}

		#region IComparer<Plane> Members

		float3	m_center;
		public int Compare( Plane x, Plane y ) {
			float	Dx = (m_center - x.position).Dot( x.normal );
			float	Dy = (m_center - y.position).Dot( y.normal );
			return Comparer<float>.Default.Compare( Dy, Dx );
		}

		#endregion
	}
}
