#ifndef _sssim_h
#define _sssim_h

/* requires:
	#include <stdint.h>
 */

#define SIGN(x) (((x) > 0) - ((x) < 0))

#define FOREACH(i, c) for (__typeof__(c.begin()) i = c.begin(); i != c.end(); ++i)
#define FOREACH_REVERSE(i, c) for (__typeof__(c.rbegin()) i = c.rbegin(); i != c.rend(); ++i)

#define FOREACH_CONST(i, c)                  \
	typedef __typeof__(c) c##_CONTAINERTYPE; \
	for (c##_CONTAINERTYPE::const_iterator i = c.begin(); i != c.end(); ++i)
#define FOREACH_REVERSE_CONST(i, c)          \
	typedef __typeof__(c) c##_CONTAINERTYPE; \
	for (c##_CONTAINERTYPE::const_reverse_iterator i = c.rbegin(); i != c.rend(); ++i)

template <typename T>
T CLAMP(const T &value, const T &low, const T &high)
{
	return value < low ? low : (value > high ? high : value);
}

#define LOGDIR_FMT "logs/t_%07u_r%07u", start_time, rand
#define LOGDIR_SYMLINK "logs/latest"
#define LOGFILE_BASE_FMT "base_%02d", client_id
#define LOGFILE_FLOAT_FMT "float_%02d", client_id

#define GRAVITY 9.80665

// rough linearisation, with origo at 58.5N 21.0E, valid in the Gulf of Finland
// based on lat-long <-> UTM correspondances in grid squares 34V & 35V
#define METERS_0_IN_DEGREES_EAST 21.0
#define METERS_0_IN_DEGREES_NORTH 58.5
#define METERS_PER_DEGREE_EAST 56200.0
#define METERS_PER_DEGREE_NORTH 110900.0
inline double degrees_east_to_meters(double d)
{
	return (d - METERS_0_IN_DEGREES_EAST) * METERS_PER_DEGREE_EAST;
}
inline double degrees_north_to_meters(double d) { return (d - METERS_0_IN_DEGREES_NORTH) * METERS_PER_DEGREE_NORTH; }
inline double meters_east_to_degrees(double m) { return m / METERS_PER_DEGREE_EAST + METERS_0_IN_DEGREES_EAST; }
inline double meters_north_to_degrees(double m) { return m / METERS_PER_DEGREE_NORTH + METERS_0_IN_DEGREES_NORTH; }

#define SSS_ENV_ADDRESS "localhost", "5550"

#define MSG_ACK_MASK 0x80  // high bit indicates if msg is an ack for a previous msg
#define MSG_CTRL_MASK 0x70 // 0x7# are control messages
#define MSG_SET_MASK 0x10  // 0x1# and 0x3# are SET messages

#define MSG_QUIT 0x70
#define MSG_TIME 0x71
#define MSG_PLAY 0x72
#define MSG_PAUSE 0x73
#define MSG_NEW_FLOAT 0x74
#define MSG_NEW_BASE 0x75
#define MSG_NEW_LOG 0x76
#define MSG_SLEEP 0x77
#define MSG_ERROR 0x7F

#define MSG_SET_DENSITY 0x10
#define MSG_SATMSG 0x11
#define MSG_SOUNDMSG 0x12
#define MSG_PINGPONG 0x13

#define MSG_SET_ID 0x30
#define MSG_ENV_STATE 0x34
#define MSG_FLOAT_STATE 0x35

#define MSG_GET_CTD 0x00
#define MSG_GET_ECHO 0x01
#define MSG_GET_DENSITY 0x02
#define MSG_GET_GPS 0x03

#define MSG_GET_GUIADDR 0x20
#define MSG_GET_INFO 0x21
#define MSG_GET_ENV 0x22
#define MSG_GET_PROFILE 0x23
#define MSG_GET_DDEPTH 0x24

#define FLAG16_INIT_OK 0x0001
#define FLAG16_ENV_OK 0x0002

#define FLAG16_PISTON_OK 0x0100
#define FLAG16_SENSORS_OK 0x0200
#define FLAG16_POWER_OK 0x0400
#define FLAG16_CPU_OK 0x0800
#define FLAG16_GPS_OK 0x1000
#define FLAG16_SATMODEM_OK 0x2000
#define FLAG16_ACMODEM_OK 0x4000

#define FLAG8_INIT_OK 0x01
#define FLAG8_ENV_OK 0x02
#define FLAG8_HARDWARE_OK 0x04
#define FLAG8_COMMS_OK 0x08

#define DRIFTER_MAX_COUNT 100

#define DRIFTER_HALF_HEIGHT 1.0
#define DRIFTER_MASS 42.0
#define DRIFTER_VERTICAL_AREA 0.05
#define DRIFTER_VERTICAL_DRAG_COEF 1.9
#define DRIFTER_HORIZONTAL_AREA 0.4
#define DRIFTER_HORIZONTAL_DRAG_COEF 1.1

#define DRIFTER_VERTICAL_DRAG_MULTIPLIER (DRIFTER_VERTICAL_AREA * DRIFTER_VERTICAL_DRAG_COEF * 0.5 / DRIFTER_MASS)
#define DRIFTER_HORIZONTAL_DRAG_MULTIPLIER (DRIFTER_HORIZONTAL_AREA * DRIFTER_HORIZONTAL_DRAG_COEF * 0.5 / DRIFTER_MASS)

#define DRIFTER_VOLUME_SPEED 0.000004516
// guess in m3/s, from total volume 0.0013548 m3 in -> out in ~5 min

#define DRIFTER_CTD_DELAY 2
#define DRIFTER_GPS_DELAY (10 + drand48() * 600)
#define DRIFTER_PINGPONG_DELAY 15

#define DRIFTER_PRESSURE_SENSOR_ERROR 80.0
#define DRIFTER_TEMPERATURE_SENSOR_ERROR 0.05
#define DRIFTER_CONDUCTIVITY_SENSOR_ERROR 2.0
#define DRIFTER_GPS_ERROR 5.0

#define ALTIM_MAX_DIST 12.0		 // meters
#define ALTIM_MAX_CONF_DIST 10.0 // meters
#define ALTIM_SILENCE 0xFF
#define DRIFTER_ALTIMETER_MIN_ERROR 0.05
#define DRIFTER_ALTIMETER_MAX_ERROR 0.3

#define ACOUSTIC_PATH_MODEL(ss) ((ss) * (1.0 + drand48() * 0.05))
#define INVERSE_ACOUSTIC_PATH_MODEL(distance) ((distance)*0.9756)
#define ACOUSTIC_NOISE_PROBABILITY_AT_MAX_DIST 0.1 // 0.2 JTE ADD
#define ACOUSTIC_FREAK_MSG_PROBABILITY 0.01

#define SAT_MSG_MAX_QUEUE_TIME 24 * 3600
#define SAT_MSG_TRANSMIT_DELAY 15

typedef uint32_t simtime_t;

struct gps_data_t
{
	double x;
	double y;
	uint32_t t;
	int id;
	bool ok;

	gps_data_t() : x(), y(), t(0), id(-1), ok(false) {}
};

struct T_EnvData
{
	double depth;
	double salinity;
	double temperature;
	double drift_x;
	double drift_y;
};

char *time2str(uint32_t time);
bool is_printable_ascii(const char c);
const char *msg_name(const unsigned char c);
void print(gps_data_t *gps);

#endif
