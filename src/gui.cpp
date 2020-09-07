/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <netinet/in.h>
#include <netcdfcpp.h>
#ifdef FREEGLUT
#include <GL/freeglut.h>
#else
#include <GL/glut.h>
#endif
#include <svl/SVL.h>

#include "sssim.h"
#include "sssim-structs.h"
#include "udp.h"
#include "sea-data.h"
#include "gui-sea.h"
#include "util-gl.h"



#ifdef INC_LIBGD
#include "gd.h"
uint32_t ss_time = 0;
bool ss_record = false;
#endif

FILE * drifterLogFile;

unsigned int DEPTH_SCALE = 20;

UDPsocket *udp = NULL;
int16_t client_id;
sockaddr_in env_addr;

GuiSea *baltic;

unsigned int drifter_count = 0;
FloatState drifters[DRIFTER_MAX_COUNT];

struct T_pos_est
{
	Vec2 pos;
	Vec2 err;
};

std::map<int, std::vector<T_pos_est> > pred;

static GLuint seabottom_south, seabottom_north;

static GLdouble pov_zoom = 1.0;
static Vec3 pov_tgt(vl_zero), pov_tgt0(vl_zero);
static Vec2 pov_rot(vl_zero), pov_rot0(vl_zero);

static int window_w = 300, window_h = 300;

int mouse_x0, mouse_y0;
bool mouse_rb_down = false;

GLenum bg_mode = GL_LINE;
enum DisplayMode
{
	Screen = 0,
	Paper = 1
} display_mode = Screen;
enum BGMode
{
	Plain,
	Rainbow
} bg_color_mode = Plain;
enum Mode
{
	NoVariable,
	Temperature,
	Salinity
} var_mode = NoVariable;
enum ShowLayers
{
	AllLayers,
	TopLayers,
	BottomLayers
} show_layers = AllLayers;
enum ShowFloatData
{
	None,
	One,
	All
} pos_markers = None,
  show_history = One;

bool
	env_active = true,
	depth_buffer = true,
	show_flow_vectors = false,
	show_text = true,
	pov_tracking = false;

unsigned int anim_step = 0;
unsigned int drifter_i = 0;

HistoryBuffer<Vec3> drifter_pos_history[DRIFTER_MAX_COUNT];
HistoryBuffer<Vec3> drifter_pos_pred_history[DRIFTER_MAX_COUNT];

//-----------------------------------------------------------------------------
void take_screenshot(const char *ss_filename = NULL)
{
#ifdef INC_LIBGD
	char fn[64];
	if (!ss_filename || !ss_filename[0])
		sprintf(fn, "screenshots/%08u.png", static_cast<unsigned int>(baltic->t_now));
	else
		strncpy(fn, ss_filename, 64);
	unsigned char *px_buf = new unsigned char[window_w * window_h * 3];
	glReadBuffer(GL_BACK);
	glReadPixels(0, 0, window_w, window_h, GL_RGB, GL_UNSIGNED_BYTE, px_buf);

	gdImagePtr img = gdImageCreateTrueColor(window_w, window_h);
	for (int y = 0; y < window_h; ++y)
	{
		int yw = (window_h - y - 1) * window_w;
		for (int x = 0; x < window_w; ++x)
		{
			int color = gdTrueColor(
				px_buf[3 * (yw + x)],
				px_buf[3 * (yw + x) + 1],
				px_buf[3 * (yw + x) + 2]);
			gdImageSetPixel(img, x, y, color);
		}
	}

	delete[] px_buf;

	FILE *ss_file;
	if ((ss_file = fopen(fn, "wb")))
	{
		gdImagePng(img, ss_file);
		fclose(ss_file);
	}

	gdImageDestroy(img);

	char *ti_s = time2str(baltic->t_now);
	std::cout << ti_s << "  saved PNG screenshot (" << window_w << 'x' << window_h << ") as " << fn << '\n';
	free(ti_s);
#endif
}

//-----------------------------------------------------------------------------
#define GRIDCOLOR_GREEN1 0.08f, 0.45f, 0.30f, 0.6f
#define GRIDCOLOR_GREEN2 0.10f, 0.40f, 0.10f, 0.6f
#define GRIDCOLOR_BLUE 0.10f, 0.30f, 0.20f, 0.4f
#define GRIDCOLOR_BROWN 0.45f, 0.30f, 0.08f, 0.4f
//#define GRIDCOLOR_GREEN2 0.06f, 0.24f, 0.06f, 1.0f
//#define GRIDCOLOR_BLUE   0.04f, 0.12f, 0.08f, 1.0f

void bottomVertex(double x, double y, double z)
{
	static GLfloat
		plain_surface_color[] = {GRIDCOLOR_GREEN2},
		rainbow_surface_color[] = {1.0f, 1.0f, 1.0f, 0.5f},
		plain_depth_color[] = {GRIDCOLOR_BLUE};

	if (bg_color_mode == Plain)
	{
		if (z > 0)
			glColor4fv(plain_depth_color);
		else
			glColor4fv(plain_surface_color);
	}
	else
	{
		if (z > 0)
			glColor_depth(z, 1.0, 0.5);
		else
			glColor4fv(rainbow_surface_color);
	}
	glVertex3d(x, z, y);
}
void drawSeabottomStrip(int y)
{
	GLdouble
		x_d = baltic->bottom_pos[0][1],
		x_pos = baltic->bottom_pos[0][0],
		y_d = baltic->bottom_pos[1][1],
		y_pos = baltic->bottom_pos[1][0] + y * y_d;
	glBegin(GL_QUAD_STRIP);
	for (unsigned int x = 0; x < SEA_DEPTH_NX; ++x)
	{
		bottomVertex(x_pos, y_pos, -1 * baltic->bottom[y][x]);
		bottomVertex(x_pos, y_pos + y_d, -1 * baltic->bottom[y + 1][x]);
		x_pos += x_d;
	}
	glEnd();
}

void createSeabottomDLs()
{
	seabottom_south = glGenLists(1);
	glNewList(seabottom_south, GL_COMPILE);
	glDisable(GL_CULL_FACE);
	for (int y = SEA_DEPTH_NY - 2; y >= 0; --y)
		drawSeabottomStrip(y);
	glEndList();

	seabottom_north = glGenLists(1);
	glNewList(seabottom_north, GL_COMPILE);
	glDisable(GL_CULL_FACE);
	for (unsigned int y = 0; y < SEA_DEPTH_NY - 1; ++y)
		drawSeabottomStrip(y);
	glEndList();
}

//-----------------------------------------------------------------------------
void drawTextStatus()
{
	static int frame = 0, timebase = 0;
	static char
		time_s[18] = "time: ",
		depth_s[12] = "depth: ",
		var_s[24] = "variable: ",
		fps_s[12] = "FPS: ",
		id_s[16] = "ID: ";

	char *ti_s = time2str(baltic->t_now);
	strcpy(&time_s[6], ti_s);
	free(ti_s);

	sprintf(&depth_s[7], "%.3lgm", pov_tgt[1] / DEPTH_SCALE);

	switch (var_mode)
	{
	case Temperature:
		sprintf(&var_s[10], "Temperature");
		break;
	case Salinity:
		sprintf(&var_s[10], "Salinity");
		break;
	default:
		sprintf(&var_s[10], "None");
	}

	++frame;
	int time = glutGet(GLUT_ELAPSED_TIME);
	if (time - timebase > 1000)
	{
		float fps = frame * 1000.0 / (time - timebase);
		sprintf(&fps_s[5], "%4.2f", fps);
		timebase = time;
		frame = 0;
	}

	if (drifter_i < drifter_count)
		sprintf(&id_s[4], "%i (%i)", drifter_i, drifters[drifter_i].id);
	else
		sprintf(&id_s[4], "-");

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluOrtho2D(0, window_w, 0, window_h);
	glScalef(1, -1, 1);			   // invert the y axis, down is positive
	glTranslatef(0, -window_h, 0); // move the origin from the bottom left corner to the upper left corner
	glColor4f(0.45f, 0.30f, 0.08f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	int x = 5, y = 12;
	drawCharArray(time_s, x, y);
	y += 13;
	drawCharArray(depth_s, x, y);
	y += 13;
	drawCharArray(var_s, x, y);
	y += 13;
	drawCharArray(fps_s, x, y);
	y += 13;
	drawCharArray(id_s, x, y);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

//-----------------------------------------------------------------------------
void drawDataLayer(GLdouble z_offset, SeaLayerData *ptr_layer, GLdouble var_min, GLdouble var_range)
{
	GLdouble
		rgba[4] = {1.0, 1.0, 1.0, 0.08},
		x_d = baltic->var_pos[0][1],
		x_pos = baltic->var_pos[0][0],
		y_d = baltic->var_pos[1][1],
		y_pos = baltic->var_pos[1][0],
		h;

	glDisable(GL_CULL_FACE);
	for (unsigned int y = 0; y < SEA_NY - 1; ++y)
	{
		glBegin(GL_QUAD_STRIP);
		x_pos = baltic->var_pos[0][0];
		for (unsigned int x = 0; x < SEA_NX; ++x)
		{
			if (!(*ptr_layer)[y][x] || !(*ptr_layer)[y + 1][x])
				continue;

			for (unsigned int i = 0; i < 2; ++i)
			{
				h = ((*ptr_layer)[y + i][x] - var_min) / var_range;
				while (h > 1.0)
					--h;
				util_HSL_to_RGB(rgba, h, 0.7, 0.5);
				glColor4dv(rgba);
				glVertex3d(x_pos, z_offset, y_pos + i * y_d);
			}

			x_pos += x_d;
		}
		glEnd();
		y_pos += y_d;
	}
}

void drawFlowDots(unsigned int z)
{
	GLdouble
		x_d = baltic->var_pos[0][1],
		y_d = baltic->var_pos[1][1],
		anim_scale = 500.0 * anim_step;
	double w_max = -SEA_FLOW_W_MAX < SEA_FLOW_W_MIN ? SEA_FLOW_W_MAX : -1 * SEA_FLOW_W_MIN;
	if (w_max <= 0)
		w_max = 1.0;

	int
		y = 0,
		y_inc = 1,
		y_lim = SEA_NY;

	if (fabs(pov_rot[0]) > vl_pi / 2)
	{
		y = SEA_NY - 1;
		y_inc = -1;
		y_lim = -1;
	}

	glPointSize(2.0);

	glPushMatrix();
	glTranslated(baltic->var_pos[0][0], baltic->depth[z], baltic->var_pos[1][0]);
	glBegin(GL_POINTS);
	while (y != y_lim)
	{
		for (unsigned int x = 0; x < SEA_NX; ++x)
		{
			GLdouble &u = baltic->flow[0][z][y][x];
			GLdouble &v = baltic->flow[1][z][y][x];
			if (u == 0.0 && v == 0.0)
				continue;
			glColor_depth(baltic->depth[z], 1.0 - display_mode, 0.5);
			glVertex3d(
				x * x_d + u * anim_scale,
				baltic->flow[2][z][y][x] * anim_scale,
				y * y_d + v * anim_scale);
		}
		y += y_inc;
	}
	glEnd();
	glPopMatrix();
}

//-----------------------------------------------------------------------------
void renderScene()
{
	//static unsigned int anim_step = 0;
	if (++anim_step > 100)
		anim_step = 0;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	if (pov_tracking)
		pov_tgt = Vec3(drifters[drifter_i].pos[0], pov_tgt[1] /*drifters[drifter_i].pos[2] * DEPTH_SCALE*/, drifters[drifter_i].pos[1]);

	if (mouse_rb_down)
	{
		// move on time, not rendersings ?
		Vec2 move_xy = Rot2(-pov_rot[0]) * Vec2(0.0, -0.01 * 10000);
		pov_tgt[0] += move_xy[0];
		pov_tgt[2] += move_xy[1];
	}

	//pov_tgt = Vec3(1.25157e+06, 20, 6.60198e+06);
	Vec3 pov = Rot3(vl_y, pov_rot[0]) * Rot3(vl_x, pov_rot[1]) * Vec3(0.0, 0.0, pov_zoom) + pov_tgt;
	gluLookAt(
		EL3(pov),
		EL3(pov_tgt),
		0.0, -1.0, 0.0);

	glScaled(1.0, DEPTH_SCALE, 1.0);

	GLUquadric *q = gluNewQuadric();
	double cw = Clip(120 * 1e-5 * pov_zoom, 10.0, 240.0);

	// draw floats
	glDisable(GL_CULL_FACE);
	for (unsigned int i = 0; i < drifter_count; ++i)
	{
		if ((show_history == All) || ((show_history == One) && (i == drifter_i)))
		{
			Vec3 pt_prev = vl_zero;
			int t_max = drifter_pos_history[i].maxlen();
			glBegin(GL_LINE_STRIP);
			for (int t = 0; t < t_max; ++t)
			{
				Vec3 pt = drifter_pos_history[i].at(t);
				if (t && (sqrlen(pt_prev - pt) > 1000000.0))
				{
					glEnd();
					glBegin(GL_LINE_STRIP);
				}
				if (display_mode == Screen)
					glColor_depth(pt[2], 1.0, 0.5);
				else
					glColor4d(0.0, 0.0, 0.0, 0.8); //glColor_depth(pt[2], 0.0, 0.8);
				glVertex3d(pt[0], pt[2], pt[1]);
				pt_prev = pt;
			}
			glEnd();

			/*
			int t_pred_max = drifter_pos_pred_history[i].maxlen();
			//if (drifters[i].pred_pos != vl_zero) {
			if (t_pred_max > 1) {
				glBegin(GL_LINE_STRIP);
					for ( int t = 0; t < t_pred_max; ++t ) {
						Vec3 pt = drifter_pos_pred_history[i].at(t);
						//if ( t && ( sqrlen( pt_prev - pt ) > 1000000.0 ) ) { glEnd(); glBegin(GL_LINE_STRIP); }
						glColor_depth(pt[2], 1.0 - display_mode, 0.5);
						glVertex3d( pt[0], pt[2], pt[1] );
						pt_prev = pt;
					}
				glEnd();
			}*/
		}

		double &pos_x = drifters[i].pos[0];
		double &pos_y = drifters[i].pos[1];
		double &pos_z = drifters[i].pos[2];

		if ((pos_markers == All) || ((pos_markers == One) && (i == drifter_i)))
		{
			if (display_mode == Screen)
				glColor4d(1.0, 1.0, 1.0, 0.2);
			else
				glColor4d(0.0, 0.0, 0.0, 0.5);
			glBegin(GL_LINES);
			glVertex3d(pos_x, 0.0, pos_y);
			glVertex3d(pos_x, drifters[i].bottom_depth, pos_y);
			glEnd();
			glPointSize(3.0);
			glBegin(GL_POINTS);
			glVertex3d(pos_x, 0.0, pos_y);
			glVertex3d(pos_x, drifters[i].bottom_depth, pos_y);
			glEnd();
		}

		glColor_depth(pos_z, 1.0 - display_mode);
		glPushMatrix();
		glTranslated(pos_x, pos_z, pos_y);
		glRotated(90, 1.0, 0.0, 0.0);
		glTranslated(0.0, 0.0, -1.0);
		gluCylinder(q, cw, cw, 2.0, 8, 1);
		gluDisk(q, 0.0, cw, 8, 1);
		glPopMatrix();

		if (pred.count(drifter_i) && (i < pred[drifter_i].size()))
		{
			Vec3 pred_pos = Vec3(pred[drifter_i][i].pos, pos_z);
			if ((pred_pos[0] < 5e4) || (pred_pos[1] < 5e4))
			{
				pred_pos[0] += drifters[drifter_i].pos[0];
				pred_pos[1] += drifters[drifter_i].pos[1];
				//pred_pos[2] = 0.0;
			}
			Vec2 pred_err = 50 * pred[drifter_i][i].err;
			if (pred_err != vl_zero)
			{
				if (i == drifter_i)
					glColor4d(0.1, 0.1, 1.0, 0.5);
				else
					glColor4d(0.1, 1.0, 0.1, 0.2);
				glBegin(GL_LINE_STRIP);
				glVertex3d(pos_x, pos_z, pos_y);
				if (pred_pos[2] != pos_z)
					glVertex3d(pos_x, pred_pos[2], pos_y);
				glVertex3d(pred_pos[0], pred_pos[2], pred_pos[1]);
				glEnd();
				glPushMatrix();
				glTranslated(pred_pos[0], pred_pos[2], pred_pos[1]);
				glRotated(90, 1.0, 0.0, 0.0);
				glTranslated(0.0, 0.0, -1.0);
				gluCylinder(q, cw, cw, 2.0, 8, 1);
				//gluDisk(q, 0.0, cw, 8, 1);
				if (pred_err[0] > 0)
				{
					if (i == drifter_i)
						glColor4d(0.1, 0.1, 1.0, 0.2);
					else
						glColor4d(0.1, 1.0, 0.1, 0.1);
					glTranslated(0.0, 0.0, 1.0);
					glScaled(1.0, pred_err[1] / pred_err[0], 1.0);
					//const double hw = 0.0;
					const double hw = (pred_err[0] > 2 * cw) ? pred_err[0] - 2 * cw : 0.0;
					gluDisk(q, hw, pred_err[0], 12, 1);
				}
				glPopMatrix();
			}
		}
	}

	// draw background
	if (bg_mode)
	{
		glPushMatrix();
		glPolygonMode(GL_FRONT_AND_BACK, bg_mode);
		glCallList((pov[2] - pov_tgt[2] < 0) ? seabottom_south : seabottom_north);
		glPopMatrix();
	}

	// draw variable volume
	SeaData *var_ptr = NULL;
	GLdouble var_max = 0.0;
	switch (var_mode)
	{
	case Temperature:
		var_ptr = &baltic->temp;
		var_max = SEA_TEMP_MAX;
		break;
	case Salinity:
		var_ptr = &baltic->salt;
		var_max = SEA_SALT_MAX;
		break;
	case NoVariable:
	default:
		break;
	}

	int lz;
	double
		pov_z = pov[1] / DEPTH_SCALE,
		tgt_z = pov_tgt[1] / DEPTH_SCALE;

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);

	glCullFace(GL_FRONT);
	for (lz = SEA_NZ - 1; lz >= 0; --lz)
	{
		if (baltic->depth[lz] >= pov_z)
			break;
		if (
			(show_layers == AllLayers) ||
			((show_layers == BottomLayers) && baltic->depth[lz] >= tgt_z) ||
			((show_layers == TopLayers) && baltic->depth[lz] <= tgt_z))
		{
			if (var_mode == Temperature || var_mode == Salinity)
				drawDataLayer(baltic->depth[lz], &(*var_ptr)[lz], 0.0, var_max);
			if (show_flow_vectors)
				drawFlowDots(lz);
		}
	}

	glCullFace(GL_BACK);
	for (int z = 0; z <= lz; ++z)
		if (
			(show_layers == AllLayers) ||
			((show_layers == BottomLayers) && baltic->depth[z] >= tgt_z) ||
			((show_layers == TopLayers) && baltic->depth[z] <= tgt_z))
		{
			if (var_mode == Temperature || var_mode == Salinity)
				drawDataLayer(baltic->depth[z], &(*var_ptr)[z], 0.0, var_max);
			if (show_flow_vectors)
				drawFlowDots(z);
		}

	if (show_text)
		drawTextStatus();

#ifdef INC_LIBGD
	if (ss_record && (ss_time != static_cast<unsigned int>(baltic->t_now)))
	{
		take_screenshot();
		ss_time = static_cast<uint32_t>(baltic->t_now);
	}
#endif

	glutSwapBuffers();
}

//-----------------------------------------------------------------------------
void keypressNormal(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 27: // ESC
	case 'q':
		udp->sendto(&env_addr, MSG_QUIT);
		// fallthrough
	case 'Q':
		exit(EXIT_SUCCESS);
	case ' ':
		udp->sendto(&env_addr, env_active ? MSG_PAUSE : MSG_PLAY);
		env_active = !env_active;
		break;
	//~ case 'z':
	//~ 	env->send(SSS_CTRL_NEW_DRIFTER);
	//~ 	break;
	//case '`':	show_text = !show_text;		break;
	case '\t':
		if (++drifter_i >= drifter_count)
			drifter_i = 0;
	case '`':
		if (!drifter_count)
			break;
		pov_tgt = Vec3(drifters[drifter_i].pos[0], drifters[drifter_i].pos[2] * DEPTH_SCALE, drifters[drifter_i].pos[1]);
		break;
	case '~':
		pov_tracking = !pov_tracking;
		break;
	case 'r':
		pov_tgt = Vec3(
			baltic->var_pos[0][0] + (SEA_NX - 1) * 0.35 * baltic->var_pos[0][1],
			0.0,
			baltic->var_pos[1][0] + (SEA_NY - 1) * 0.35 * baltic->var_pos[1][1]);
		pov_zoom = 0.1 * 100000;
		pov_rot = Vec2(M_PI, 0.6); // horizontal, vertical
		pov_rot0 = pov_rot;
		show_layers = AllLayers;
		//baltic->SetTime(0,true);
		if (depth_buffer)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		break;
	case 's':
		//DEPTH_SCALE = (DEPTH_SCALE > 1) ? 1 : 20;
		DEPTH_SCALE = (DEPTH_SCALE > 0) ? 0 : 20;
		if (DEPTH_SCALE == 0)
			pov_rot = Vec2(-M_PI * 0.999, M_PI_2 * 0.999);
		break;
	case '0':
		anim_step = 0;
		break;
	case 'd':
		depth_buffer = !depth_buffer;
		if (depth_buffer)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		break;
	case 'h':
		switch (show_history)
		{
		case One:
			show_history = All;
			break;
		case All:
			show_history = None;
			break;
		default:
			show_history = One;
		}
		break;
	case 'b':
		switch (bg_mode)
		{
		case GL_FILL:
			bg_mode = GL_LINE;
			break;
		case GL_LINE:
			bg_mode = 0;
			break;
		//case GL_POINT:	bg_mode = 0;	break;
		default:
			bg_mode = GL_FILL;
			break;
		}
		break;
	case 'w':
		switch (display_mode)
		{
		case Screen:
			display_mode = Paper;
			glClearColor(1.0, 1.0, 1.0, 0.0);
			break;
		case Paper:
			display_mode = Screen;
			glClearColor(0.0, 0.0, 0.0, 0.0);
			break;
		}
		break;
	case 'g':
		switch (bg_color_mode)
		{
		case Plain:
			bg_color_mode = Rainbow;
			break;
		case Rainbow:
			bg_color_mode = Plain;
			break;
		}
		createSeabottomDLs();
		break;
	case 'p':
		switch (pos_markers)
		{
		case One:
			pos_markers = All;
			break;
		case All:
			pos_markers = None;
			break;
		default:
			pos_markers = One;
		}
		break;
	case 'v':
		switch (var_mode)
		{
		case Temperature:
			var_mode = Salinity;
			break;
		case Salinity:
			var_mode = NoVariable;
			break;
		default:
			var_mode = Temperature;
		}
		break;
	case 'f':
		show_flow_vectors = !show_flow_vectors;
		return;
#ifdef INC_LIBGD
	case '=':
		ss_record = !ss_record;
		break;
#endif
	}
}

void keypressSpecial(int key, int x, int y)
{
	Vec2 move_xy(-1000, 0.0);
	switch (key)
	{
	case GLUT_KEY_END:
		switch (show_layers)
		{
		case AllLayers:
			show_layers = TopLayers;
			break;
		case TopLayers:
			show_layers = BottomLayers;
			break;
		default:
			show_layers = AllLayers;
		}
		return;
	case GLUT_KEY_PAGE_UP:
		pov_tgt[1] -= 2.0 * DEPTH_SCALE;
		if (pov_tgt[1] < 0.0)
			pov_tgt[1] = 0;
		//std::cout << "Depth: " << pov_tgt[1] / DEPTH_SCALE << '\n';
		return;
	case GLUT_KEY_PAGE_DOWN:
		pov_tgt[1] += 2.0 * DEPTH_SCALE;
		if (pov_tgt[1] > 156.0 * DEPTH_SCALE)
			pov_tgt[1] = 156.0 * DEPTH_SCALE;
		//std::cout << "Depth: " << pov_tgt[1] / DEPTH_SCALE << '\n';
		return;

	case GLUT_KEY_LEFT:
		move_xy *= -1;
		break;
	case GLUT_KEY_RIGHT:
		break;
	case GLUT_KEY_UP:
		move_xy = -1 * cross(move_xy);
		break;
	case GLUT_KEY_DOWN:
		move_xy = cross(move_xy);
		break;

#ifdef FREEGLUT
	case GLUT_KEY_F11:
		glutFullScreenToggle();
		break;
#endif

	default:
		return;
	}

	move_xy = Rot2(-pov_rot[0]) * move_xy;
	pov_tgt[0] += move_xy[0];
	pov_tgt[2] += move_xy[1];
}

// button in { 0:left, middle, right, mousewheel_up, 4:mousewheel_down }
void mouseClick(int button, int state, int x, int y)
{
	switch (button)
	{
	case GLUT_RIGHT_BUTTON:
		mouse_rb_down = (state == GLUT_DOWN) ? true : false;
	case GLUT_LEFT_BUTTON:
		if (state == GLUT_DOWN)
		{
			mouse_x0 = x;
			mouse_y0 = y;
			pov_rot0 = pov_rot;
			pov_tgt0 = pov_tgt;
		}
		break;
	case GLUT_MOUSEWHEEL_UP:
		pov_zoom *= 0.95; /*if ( pov_zoom < 0.01 ) pov_zoom = 0.01;*/
		break;
	case GLUT_MOUSEWHEEL_DOWN:
		pov_zoom *= 1.05; /*if ( pov_zoom > 80.0 ) pov_zoom = 80.0;*/
		break;
	}
	//std::cout << "pov zoom " << pov_zoom << '\n';
}

void mouseMoveActive(int x, int y)
{
	if (DEPTH_SCALE == 0)
	{
		pov_rot = Vec2(-M_PI * 0.999, M_PI_2 * 0.999);
		pov_tgt[0] = pov_tgt0[0] - 1.0e-3 * pov_zoom * (x - mouse_x0);
		pov_tgt[2] = pov_tgt0[2] + 1.0e-3 * pov_zoom * (y - mouse_y0);
	}
	else
	{
		pov_rot[0] = pov_rot0[0] + 2 * static_cast<double>(x - mouse_x0) / window_w;
		pov_rot[1] = pov_rot0[1] + 2 * static_cast<double>(y - mouse_y0) / window_h;
		for (int i = 0; i < 2; ++i)
		{
			while (pov_rot[i] > vl_pi)
				pov_rot[i] -= 2 * vl_pi;
			while (pov_rot[i] < -vl_pi)
				pov_rot[i] += 2 * vl_pi;
		}
	}
	//std::cout << "pov rot " << pov_rot << '\n';
	//std::cout << "p0/pi : " << pov_rot[0] / vl_pi << '\n';
}

//-----------------------------------------------------------------------------
void update_float_positions(const char *buf, unsigned int len, uint32_t time)
{
	static int timeCnt = 60;

	if (len % sizeof(FloatState))
	{
		printf("RX bad status data! (%i %% %lu != 0)\n", len, sizeof(FloatState));
		return;
	}

	drifter_count = len / sizeof(FloatState);
	if (drifter_count > 0)
		memcpy(drifters, buf, len);

	baltic->SetTime(time, show_flow_vectors || (var_mode != NoVariable));
	for (unsigned int i = 0; i < drifter_count; ++i) {
		drifter_pos_history[i].push(drifters[i].pos);
		// JTE change: real simulated positions 
		if (timeCnt == 60) {
			fprintf(drifterLogFile,"%d, drifter_%d,%f,%f,%f,%f,%f,%f,%f,%f\n", time, i, drifters[i].pos[0], drifters[i].pos[1], drifters[i].pos[2], drifters[i].vel[0], drifters[i].vel[1], drifters[i].vel[2], drifters[i].volume, drifters[i].bottom_depth );
			
		}
	}
	if (++timeCnt > 60)
		timeCnt = 0;
}

void update_float_predictions(const char *buf, unsigned int len, int16_t rx_id)
{
	if (len % sizeof(T_pos_est))
	{
		printf("RX bad pred. data from #%i! (len %i %% %zu != 0)\n", rx_id, len, sizeof(T_pos_est));
		return;
	}
	int id = -1;
	for (unsigned int i = 0; i < drifter_count; ++i)
		if (drifters[i].id == rx_id)
		{
			id = i;
			break;
		}
	if (id >= 0)
	{
		const T_pos_est *pred0 = reinterpret_cast<const T_pos_est *>(buf);
		const T_pos_est *pred1 = reinterpret_cast<const T_pos_est *>(buf + len);
		pred[id] = std::vector<T_pos_est>(pred0, pred1);
		//Vec3 pred_pos = Vec3(pred[id][id].pos[0], pred[id][id].pos[1], 0);
		//drifter_pos_pred_history[id].push(pred_pos);
	}
}

//-----------------------------------------------------------------------------
void udp_state_msg(char msg_type, const char *msg_body, int msg_len, const sockaddr_in &addr)
{
	switch (msg_type)
	{
	case MSG_ENV_STATE:
	{
		const int hlen = sizeof(uint32_t); // header: time in seconds[uint32_t]
		if (msg_len < hlen)
			break;
		uint32_t time = *reinterpret_cast<const uint32_t *>(msg_body);
		bool first_env_state = (drifter_count == 0);
		update_float_positions(msg_body + hlen, msg_len - hlen, time);
		if (first_env_state)
			keypressNormal('`', 0, 0);
	}
	break;
	case MSG_FLOAT_STATE:
	{
		const int hlen = sizeof(int16_t); // header: id[int16_t]
		if (msg_len <= hlen)
			break;
		int16_t id = *reinterpret_cast<const int16_t *>(msg_body);
		update_float_predictions(msg_body + hlen, msg_len - hlen, id);
	}
	break;

	case MSG_SET_ID:
		break;

	default:
		printf("|| rx %s (%0hhX) from %s\n", msg_name(msg_type), msg_type, inet(addr).str);
	}
}

void *udp_listen(void *unused)
{
	const unsigned int buf_maxlen = 4500;
	char buf[buf_maxlen];
	struct sockaddr_in addr;

	while (1)
	{
		int rx_bytes = udp->recvfrom(&addr, buf, buf_maxlen);
		if (rx_bytes <= 0)
			continue;

		switch (buf[0])
		{
		case MSG_ENV_STATE:
		case MSG_FLOAT_STATE:
			udp_state_msg(buf[0], buf + 1, rx_bytes - 1, addr);
			break;

		case MSG_QUIT:
			exit(EXIT_SUCCESS);
			break;

		case MSG_TIME:
		case MSG_PLAY:
		case MSG_PAUSE:
			break;

		default:
			printf("|| rx %s (0x%02hhX, %dc) from %s\n", msg_name(buf[0]), buf[0], rx_bytes, inet(addr).str);
		}
	}
	return NULL;
}

//-----------------------------------------------------------------------------
void refresh(int unused)
{
	glutTimerFunc(50, refresh, 0); // FPS ~20 Hz
	glutPostRedisplay();
}

void resizeWindow(int w, int h)
{
	if (h == 0)
		h = 1;
	window_w = w;
	window_h = h;
	float ratio = static_cast<float>(w) / h;

	// Reset the coordinate system before modifying
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Set the viewport to be the entire window
	glViewport(0, 0, w, h);

	// Set the correct perspective.
	gluPerspective(45, ratio, SEA_CLIP_MIN, SEA_CLIP_MAX);
	glMatrixMode(GL_MODELVIEW);
}

//-----------------------------------------------------------------------------
void glut_init(int argc, char **argv)
{
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutCreateWindow("SeaSwarmSim :: Baltic");

	//glEnable(GL_DEPTH_TEST);
	glHint(GL_CLIP_VOLUME_CLIPPING_HINT_EXT, GL_FASTEST);
	glCullFace(GL_BACK);
	//glEnable(GL_CULL_FACE);
	//glEnable(GL_POLYGON_SMOOTH);
	glEnable(GL_BLEND);
	//glBlendFunc( GL_SRC_ALPHA, GL_DST_ALPHA );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc( GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA );

	createSeabottomDLs();

	glutDisplayFunc(renderScene);
	glutReshapeFunc(resizeWindow);

	glutTimerFunc(0, refresh, 0);

	//glutIgnoreKeyRepeat(1);
	glutKeyboardFunc(keypressNormal);
	glutSpecialFunc(keypressSpecial);

	glutMouseFunc(mouseClick);
	glutMotionFunc(mouseMoveActive);
}

//-----------------------------------------------------------------------------
void udp_init()
{
	udp = new UDPsocket();
	env_addr = inet(SSS_ENV_ADDRESS).addr;
	int tx_bytes = udp->sendto(&env_addr, MSG_NEW_LOG);
	if (tx_bytes <= 0)
		exit(EXIT_FAILURE);

	timeval tv = {1, 0};
	struct sockaddr_in addr;
	ssize_t rx_bytes = udp->recvfrom(&addr, &client_id, sizeof(client_id), &tv);
	if ((rx_bytes != sizeof(client_id)) || (client_id < 0))
	{
		printf("|| rx bad ID! (%d) exiting.\n", client_id);
		exit(EXIT_FAILURE);
	}

	pthread_t udp_thread;
	pthread_create(&udp_thread, NULL, &udp_listen, NULL);
}

//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	baltic = new GuiSea();
	// JTE change to store simulated positions & timestamps for reference
	drifterLogFile = fopen ("./logs/latest/drifterlog.csv","w"); // additional drifter log file for 

	if ((argc > 1) && !strcmp(argv[1], "info"))
	{
		baltic->PrintInfo();
		exit(0);
	}

	glut_init(argc, argv);
	udp_init();

	keypressNormal('r', 0, 0);
	if (env_active)
		udp->sendto(&env_addr, MSG_PLAY);

	glutMainLoop();

	fclose(drifterLogFile);
	return 0;
}
