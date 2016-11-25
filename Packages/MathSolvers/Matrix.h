//////////////////////////////////////////////////////////////////////////
// Defines generic, arbitrary-sized vectors and matrices
//
#pragma once

namespace MathSolvers {

	class Vector {
	public:
		U32			length;
		bool		ownedPtr;
		double*		m;

	public:
		Vector() : length(0), m(nullptr), ownedPtr(false)								{}
		Vector( U32 _length ) : length(0), m(nullptr), ownedPtr(false)					{ Init( _length ); }
		Vector( U32 _length, double* _ptr ) : length(0), m(nullptr), ownedPtr(false)	{ Init( _length, _ptr ); }
		~Vector()																		{ Exit(); }

		void			Init( U32 _length, double* _ptr=NULL );
		void			Exit();
		void			Clear( double v=0.0 );
		void			CopyTo( Vector& _target ) const;
		void			Swap( Vector& _other );	// Swaps pointers with other vector without copying anything

		double&			operator[]( U32 i )			{ ASSERT( i < length, "Index out of range!" ); return m[i]; }
		const double&	operator[]( U32 i ) const	{ ASSERT( i < length, "Index out of range!" ); return m[i]; }
	};

	class Matrix {
	public:
		U32			rows, columns;
		double*		m_raw;
		Vector*		m;

	public:
		Matrix() : rows( 0 ), columns( 0 ), m_raw( nullptr ), m(nullptr)							{}
		Matrix( U32 _rows, U32 _columns ) : rows( 0 ), columns( 0 ), m_raw( nullptr ), m(nullptr)	{ Init( _rows, _columns ); }
		~Matrix()																					{ Exit(); }

		void			Init( U32 _rows, U32 _columns );
		void			Exit();
		void			Clear( double v=0.0 );
		void			CopyTo( Matrix& _target ) const;

		Vector&			operator[]( U32 row )		{ ASSERT( row < rows, "Index out of range!" ); return m[row]; }
		const Vector&	operator[]( U32 row ) const	{ ASSERT( row < rows, "Index out of range!" ); return m[row]; }
	};

}	// namespace MathSolvers