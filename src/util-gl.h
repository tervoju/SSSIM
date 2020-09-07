#define  GLUT_MOUSEWHEEL_UP    0x0003
#define  GLUT_MOUSEWHEEL_DOWN  0x0004

#define  EL3(v)  v[0], v[1], v[2]


//-----------------------------------------------------------------------------
const int HISTORY_SIZE = 50000;

template< typename T >
class HistoryBuffer {
	private:
		T buffer[HISTORY_SIZE];
		int buffer_pos;
		bool buffer_full;
	public:
		HistoryBuffer() :
			buffer_pos( -1 ),
			buffer_full( false )
		{ }

		void clear() {
			buffer_pos = -1;
			buffer_full = false;
		}

		int maxlen() { return buffer_full ? HISTORY_SIZE : buffer_pos; }

		void push( const T& value ) {
			if ( ++buffer_pos >= HISTORY_SIZE ) {
				buffer_pos = 0;
				buffer_full = true;
			}
			buffer[buffer_pos] = value;
		}

		T at( int seek_pos ) {
			int pos = buffer_pos - seek_pos;
			while ( pos < 0 ) pos += HISTORY_SIZE;
			return buffer[ pos % HISTORY_SIZE ];
		}
};


//-----------------------------------------------------------------------------
// all inputs in [0,1]
// algorithm from http://en.wikipedia.org/wiki/HSL_color_space
void util_HSL_to_RGB( GLdouble * rgb, const GLdouble h, const GLdouble s, const GLdouble l ) {
	GLdouble
		q = ( l < 0.5 ) ? l * ( 1 + s ) : l + s - (l*s),
		p = 2*l - q;
	GLdouble t[3] = { h + 0.3333, h, h - 0.3333 };

	for ( int i = 0; i < 3; ++i ) {
		if ( t[i] < 0 ) ++t[i]; else if ( t[i] > 1 ) --t[i];
		if ( t[i] < 0.1666667 )
			rgb[i] = p + (q-p) * 6 * t[i];
		else if ( t[i] < 0.5 )
			rgb[i] = q;
		else if ( t[i] < 0.6666667 )
			rgb[i] = p + (q-p) * 6 * ( 0.6666667 - t[i] );
		else
			rgb[i] = p;
	}
}

void glColor_depth( const double depth, const double surface_value, const double opac = 1.0 ) {
	if ( depth < 1.05 ) glColor4d(surface_value, surface_value, surface_value, opac);
	else {
		GLdouble rgb[4];
		util_HSL_to_RGB( rgb, depth / 150.0, 1.0, 0.5 );
		rgb[3] = opac;
		glColor4dv( rgb );
	}
}


//-----------------------------------------------------------------------------
void drawOrigo( Vec3 pos ) {
	glPushMatrix();
		glTranslated( EL3(pos) );
		glBegin(GL_LINES);
			glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );	glVertex3i( 0, 0, 0 );	glVertex3f( 0.1, 0, 0 );
			glColor4f( 0.0f, 1.0f, 0.0f, 1.0f );	glVertex3i( 0, 0, 0 );	glVertex3f( 0, 0.1, 0 );
			glColor4f( 0.0f, 0.0f, 1.0f, 1.0f );	glVertex3i( 0, 0, 0 );	glVertex3f( 0, 0, 0.1 );
		glEnd();
	glPopMatrix();
}

void drawCharArray( char * s, int x, int y ) {
	for ( char * c = s; *c; ++c ) {
		glRasterPos2f( x, y );
		glutBitmapCharacter( GLUT_BITMAP_HELVETICA_10, *c );
		x += glutBitmapWidth( GLUT_BITMAP_HELVETICA_10, *c );
	}
}
