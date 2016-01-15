﻿using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;

using RendererManaged;
using Nuaj.Cirrus.Utility;

namespace TestMSBSDF
{
	public partial class Form1 : Form
	{
		const int				HEIGHTFIELD_SIZE = 256;
		static readonly double	SQRT2 = Math.Sqrt( 2.0 );

		private Device		m_Device = new Device();

		[System.Runtime.InteropServices.StructLayout( System.Runtime.InteropServices.LayoutKind.Sequential )]
		private struct CB_Main {
		}

		[System.Runtime.InteropServices.StructLayout( System.Runtime.InteropServices.LayoutKind.Sequential )]
		private struct CB_Camera {
			public float4x4		_Camera2World;
			public float4x4		_World2Camera;
			public float4x4		_Proj2World;
			public float4x4		_World2Proj;
			public float4x4		_Camera2Proj;
			public float4x4		_Proj2Camera;
		}

		private ConstantBuffer<CB_Main>			m_CB_Main = null;
		private ConstantBuffer<CB_Camera>		m_CB_Camera = null;

		private Shader				m_Shader_RenderHeightField = null;
		private Texture2D			m_Tex_Heightfield = null;
		private Primitive			m_Prim_Heightfield = null;

		private Camera				m_Camera = new Camera();
		private CameraManipulator	m_Manipulator = new CameraManipulator();

		public Form1() {
			InitializeComponent();

			m_Camera.CameraTransformChanged += new EventHandler( Camera_CameraTransformChanged );

			Application.Idle += new EventHandler( Application_Idle );
		}

		#region Open/Close

		protected override void OnLoad( EventArgs e )
		{
			base.OnLoad( e );

			try
			{
				m_Device.Init( panelOutput.Handle, false, true );
			}
			catch ( Exception _e )
			{
				m_Device = null;
				MessageBox( "Failed to initialize DX device!\n\n" + _e.Message );
				return;
			}

			m_CB_Main = new ConstantBuffer<CB_Main>( m_Device, 0 );
			m_CB_Camera = new ConstantBuffer<CB_Camera>( m_Device, 1 );

			try {
				m_Shader_RenderHeightField = new Shader( m_Device, new ShaderFile( new System.IO.FileInfo( "Shaders/RenderHeightField.hlsl" ) ), VERTEX_FORMAT.P3, "VS", null, "PS", null );;
			} catch ( Exception _e ) {
				MessageBox( "Shader \"RenderHeightField\" failed to compile!\n\n" + _e.Message );
				m_Shader_RenderHeightField = null;
			}

			BuildPrimHeightfield();

			BuildBeckmannSurfaceTexture( 0.8f, 1000 );

			// Setup camera
			m_Camera.CreatePerspectiveCamera( (float) (60.0 * Math.PI / 180.0), (float) panelOutput.Width / panelOutput.Height, 0.01f, 100.0f );
			m_Manipulator.Attach( panelOutput, m_Camera );
			m_Manipulator.InitializeCamera( new float3( 0, 1, 4 ), new float3( 0, 0, 0 ), float3.UnitY );
		}

		void	BuildPrimHeightfield() {
			VertexP3[]	Vertices = new VertexP3[HEIGHTFIELD_SIZE*HEIGHTFIELD_SIZE];
			for ( uint Y=0; Y < HEIGHTFIELD_SIZE; Y++ ) {
				float	y = -1.0f + 2.0f * Y / (HEIGHTFIELD_SIZE-1);
				for ( uint X=0; X < HEIGHTFIELD_SIZE; X++ ) {
					float	x = -1.0f + 2.0f * X / (HEIGHTFIELD_SIZE-1);
					Vertices[256*Y+X].P.Set( x, y, 0.0f );
				}
			}

			List< uint >	Indices = new List< uint >();
			for ( uint Y=0; Y < HEIGHTFIELD_SIZE-1; Y++ ) {
				uint	IndexStart0 = HEIGHTFIELD_SIZE*Y;		// Start index of top band
				uint	IndexStart1 = HEIGHTFIELD_SIZE*(Y+1);	// Start index of bottom band
				for ( uint X=0; X < HEIGHTFIELD_SIZE; X++ ) {
					Indices.Add( IndexStart0++ );
					Indices.Add( IndexStart1++ );
				}
				if ( Y != HEIGHTFIELD_SIZE-1 ) {
					Indices.Add( IndexStart1-1 );				// Double current band's last index (first degenerate triangle => finish current band)
					Indices.Add( IndexStart0 );					// Double next band's first index (second degenerate triangle => start new band)
				}
			}


			m_Prim_Heightfield = new Primitive( m_Device, HEIGHTFIELD_SIZE*HEIGHTFIELD_SIZE, VertexP3.FromArray( Vertices ), Indices.ToArray(), Primitive.TOPOLOGY.TRIANGLE_STRIP, VERTEX_FORMAT.P3 );
		}

		double	GenerateNormalDistributionHeight() {
			double	U = WMath.SimpleRNG.GetUniform();	// Uniform distribution in ]0,1[
			double	errfinv = WMath.Functions.erfinv( 2.0 * U - 1.0 );
			double	h = SQRT2 * errfinv;
			return h;
		}

		/// <summary>
		/// Builds a heightfield whose heights are distributed according to the following probability:
		///		p(height) = exp( -0.5*height^2 ) / sqrt(2PI)
		///	
		///	(a.k.a. the normal distribution with sigma=1 and µ=0)
		///	From "2015 Heitz - Generating Procedural Beckmann Surfaces"
		/// </summary>
		/// <remarks>Only isotropic roughness is supported</remarks>
		/// 
		double[]	m_phi, m_fx, m_fy;
		void	BuildBeckmannSurfaceTexture( float _roughness, int N ) {
			PixelsBuffer	Content = new PixelsBuffer( HEIGHTFIELD_SIZE*HEIGHTFIELD_SIZE*System.Runtime.InteropServices.Marshal.SizeOf(typeof(Single)) );

			// Precompute stuff that resemble a lot to the Box-Muller algorithm to generate normal distribution random values
			m_phi = new double[N];
			m_fx = new double[N];
			m_fy = new double[N];
			for ( int i=0; i < N; i++ ) {
				double	U0 = WMath.SimpleRNG.GetUniform();
				double	U1 = WMath.SimpleRNG.GetUniform();
				double	U2 = WMath.SimpleRNG.GetUniform();

				m_phi[i] = 2.0 * Math.PI * U0;							// Phase

				double	theta = 2.0 * Math.PI * U1;
				double	radius = Math.Sqrt( -Math.Log( U2 ) );
				m_fx[i] = radius * Math.Cos( theta ) * _roughness;		// Frequency in X direction
				m_fy[i] = radius * Math.Sin( theta ) * _roughness;		// Frequency in Y direction
			}
			double	scale = Math.Sqrt( 2.0 / N );

			// Generate heights
			float	range = 128.0f;
			float2	pos;
			float	height;
			float	minHeight = float.MaxValue, maxHeight = -float.MaxValue;
			double	accum;
			using ( BinaryWriter W = Content.OpenStreamWrite() ) {
				for ( int Y=0; Y < HEIGHTFIELD_SIZE; Y++ ) {
					pos.y = range * (2.0f * Y / (HEIGHTFIELD_SIZE-1) - 1.0f);
					for ( int X=0; X < HEIGHTFIELD_SIZE; X++ ) {
						pos.x = range * (2.0f * X / (HEIGHTFIELD_SIZE-1) - 1.0f);

//						height = (float) WMath.SimpleRNG.GetNormal();
//						height = (float) GenerateNormalDistributionHeight();

						accum = 0.0;
						for ( int i=0; i < N; i++ ) {
							accum += Math.Cos( m_phi[i] + pos.x * m_fx[i] + pos.y * m_fy[i] );
						}
						height = (float) (scale * accum);

						minHeight = Math.Min( minHeight, height );
						maxHeight = Math.Max( maxHeight, height );

						W.Write( height );
					}
				}
			}
			Content.CloseStream();

			m_Tex_Heightfield = new Texture2D( m_Device, HEIGHTFIELD_SIZE, HEIGHTFIELD_SIZE, 1, 1, PIXEL_FORMAT.R32_FLOAT, false, false, new PixelsBuffer[] { Content } );
		}

		protected override void OnFormClosed( FormClosedEventArgs e )
		{
			if ( m_Device == null )
				return;

			if ( m_Shader_RenderHeightField != null ) {
				m_Shader_RenderHeightField.Dispose();
			}

			m_Tex_Heightfield.Dispose();
			m_Prim_Heightfield.Dispose();
			m_CB_Main.Dispose();
			m_CB_Camera.Dispose();

			m_Device.Exit();

			base.OnFormClosed( e );
		}

		void Camera_CameraTransformChanged( object sender, EventArgs e )
		{
			m_CB_Camera.m._Camera2World = m_Camera.Camera2World;
			m_CB_Camera.m._World2Camera = m_Camera.World2Camera;

			m_CB_Camera.m._Camera2Proj = m_Camera.Camera2Proj;
			m_CB_Camera.m._Proj2Camera = m_CB_Camera.m._Camera2Proj.Inverse;

			m_CB_Camera.m._World2Proj = m_CB_Camera.m._World2Camera * m_CB_Camera.m._Camera2Proj;
			m_CB_Camera.m._Proj2World = m_CB_Camera.m._Proj2Camera * m_CB_Camera.m._Camera2World;

			m_CB_Camera.UpdateData();
		}

		void	MessageBox( string _Text ) {
			MessageBox( _Text, MessageBoxButtons.OK, MessageBoxIcon.Error );
		}
		void	MessageBox( string _Text, MessageBoxButtons _Buttons, MessageBoxIcon _Icon ) {
			System.Windows.Forms.MessageBox.Show( _Text, "MS BSDF Test", _Buttons, _Icon );
		}

		#endregion

		void Application_Idle( object sender, EventArgs e )
		{
			if ( m_Device == null )
				return;

			// Setup global data
			m_CB_Main.UpdateData();

			m_Tex_Heightfield.Set( 0 );

			// =========== Render scene ===========
			m_Device.Clear( m_Device.DefaultTarget, float4.Zero );
			m_Device.ClearDepthStencil( m_Device.DefaultDepthStencil, 1.0f, 0, true, false );

			// Render heightfield
			if ( m_Shader_RenderHeightField != null && m_Shader_RenderHeightField.Use() ) {
				m_Device.SetRenderTarget( m_Device.DefaultTarget, m_Device.DefaultDepthStencil );
				m_Device.SetRenderStates( RASTERIZER_STATE.CULL_NONE, DEPTHSTENCIL_STATE.READ_WRITE_DEPTH_LESS, BLEND_STATE.DISABLED );
				m_Prim_Heightfield.Render( m_Shader_RenderHeightField );
			}

			// Show!
			m_Device.Present( false );
		}

		private void buttonReload_Click( object sender, EventArgs e )
		{
			m_Device.ReloadModifiedShaders();
		}
	}
}