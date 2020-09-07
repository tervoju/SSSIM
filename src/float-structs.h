#ifndef _float_structs_h
#define _float_structs_h

#define  DEPTH_HISTORY_SIZE      6
#define  DEPTHCODE_UNKNOWN  0xFF

//-----------------------------------------------------------------------------
struct depthcode {
	uint8_t data;
	unsigned int depth() const { return (data & 0x3F) << 1; } // 6 bits, range 0..126m
	bool surfacing() const { return data & 0x80; }
	bool bottom_dive() const { return data & 0x40; }
	bool is_unknown() const { return data == DEPTHCODE_UNKNOWN; }
	depthcode(unsigned int depth = 0, bool surfacing = false, bool bottom_dive = false):
		data(((depth >> 1) & 0x3F) | (surfacing ? 0x80 : 0x00) | (bottom_dive ? 0x40 : 0x00)) { }
};


//-----------------------------------------------------------------------------
struct abs_pos_t {
	long int          x : 20;  // meters east
	long int          y : 20;  // meters north
	long unsigned int t : 24;  // seconds from start  OR  padding
	           // total : 64 -> sizeof() == 8

	//static const long unsigned int invalid_t = 0xFFFFFF;
	
	abs_pos_t(long int _x = 0, long int _y = 0, long unsigned int _t = 0xFFFFFF): x(_x), y(_y), t(_t) { }
	abs_pos_t(const gps_data_t & gps): x(gps.x), y(gps.y), t(gps.t) { }
	abs_pos_t(const abs_pos_t & pos): x(pos.x), y(pos.y), t(pos.t) { }
	//virtual ~abs_pos_t() { }

	bool t_valid() const { return t < 0xFFFFFF; }

	const void * read(const void * src, bool include_t = true, size_t * len = NULL);
	void * write(void * tgt, bool include_t = true, size_t * len = NULL) const; // returns pointer to next write position
};

// SATMSG_BASE_GROUP
struct abs_pos_with_id: public abs_pos_t {
	int16_t id;

	abs_pos_with_id(int16_t _id = -1, long int _x = 0, long int _y = 0, long unsigned int _t = 0xFFFFFF): abs_pos_t(_x, _y, _t), id(_id) { }
	abs_pos_with_id(int16_t _id, const abs_pos_t & pos): abs_pos_t(pos), id(_id) { }
	~abs_pos_with_id() { }

	const void * read(const void * src, bool include_t = true, size_t * len = NULL);
	void * write(void * tgt, bool include_t = true, size_t * len = NULL) const; // returns pointer to next write position

	static size_t char_len(bool include_t = true) { return sizeof(id) + sizeof(abs_pos_t); }
};

//-----------------------------------------------------------------------------
struct rel_status_t {
	unsigned int flag_header  :  2;  // always 11b
	unsigned int flags        :  6;
	unsigned int bearing_conf :  2;  // 0: little, 1: some, 2: lots
	bool         range_meas   :  1;  // true iff range based on own or other's current measurement
	unsigned int range_2m     : 13;  // in units of 2 meters, use range() for meters
	unsigned int bearing      :  8;  // [0..255]/256 cycles east of north
	                 // total : 32 -> sizeof() == 4

	rel_status_t(unsigned char _flags = 0,
		unsigned int _range = 0, bool _range_meas = false,
		unsigned char _bearing = 0, unsigned char _bearing_conf = 0 
	):
		flag_header(0x3),
		flags(_flags),
		bearing_conf(_bearing_conf),
		range_meas(_range_meas),
		range_2m(_range >> 1),
		bearing(_bearing)
	{ }
	//virtual ~rel_status_t() { }

	unsigned int range() const { return range_2m << 1; }

	size_t char_len() const { return 2 + (flags ? 1 : 0) + (bearing_conf ? 1 : 0); }
	unsigned char * char_ptr() { return reinterpret_cast<unsigned char *>(this); }

	const void * read(const void * src, size_t * len = NULL);
	void * write(void * tgt, size_t * len = NULL); // returns pointer to next write position
};

struct rel_status_with_id: public rel_status_t {
	int16_t id;
	//uint8_t mins10_silence; // time in units of 10min since last comms

	rel_status_with_id(int16_t _id = -1, unsigned char _flags = 0,
		unsigned int _range = 0, bool _range_meas = false,
		unsigned char _bearing = 0, unsigned char _bearing_conf = 0 
	): rel_status_t(_flags, _range, _range_meas, _bearing, _bearing_conf), id(_id) { }

	const void * read(const void * src, size_t * len = NULL);
	void * write(void * tgt, size_t * len = NULL); // returns pointer to next write position

	static size_t max_char_len() { return sizeof(id) + sizeof(rel_status_t); }

private:
	size_t char_len() const;
	unsigned char * char_ptr();
};


//-----------------------------------------------------------------------------
// SATMSG_FLOAT_SELF
struct self_status_t {
	abs_pos_t pos;
	uint8_t pos_error; // 0x00: pos is from latest GPS data; 0xFF: pos is unknown; else; position variance in FIXME units
	uint16_t flags;
	depthcode prev_depths[DEPTH_HISTORY_SIZE];

	self_status_t(): pos(), pos_error(0xFF), flags(0x0000), prev_depths() { }
};


//-----------------------------------------------------------------------------
// SATMSG_FLOAT_GROUP
struct rel_group_status_t {
	uint8_t n_floats;
	rel_status_with_id * floats;

	rel_group_status_t(): n_floats(0), floats(NULL) { }
	~rel_group_status_t() { delete[] floats; }

	private:
		rel_group_status_t(const rel_group_status_t & _);
		rel_group_status_t & operator= (const rel_group_status_t & _);
};


//-----------------------------------------------------------------------------
// SATMSG_FLOAT_ENV, SATMSG_BASE_ENV
struct env_layer_status_t {
	uint8_t depth; // meters down
	double temp; // FIXME use uint16_t w/ pre-def units
	double salt; // FIXME use uint16_t w/ pre-def units
	double flow_x; // FIXME use int16_t w/ pre-def units 
	double flow_y; // FIXME use int16_t w/ pre-def units 
};

struct env_status_t {
	uint8_t z_bottom; // meters down, 0: n/a
	uint8_t z_sc; // meters down, 0: n/a
	uint8_t n_layers;
	env_layer_status_t * layers;

	env_status_t(): z_bottom(0), z_sc(0), n_layers(0), layers(NULL) { }
	~env_status_t() { delete[] layers; }

	private:
		env_status_t(const env_status_t & _);
		env_status_t & operator= (const env_status_t & _);
};


//-----------------------------------------------------------------------------
// SATMSG_FLOAT_RANGES


//-----------------------------------------------------------------------------
// SATMSG_BASE_GO_INIT
struct init_params_t {
	double d_start;
	double d_end;
	double d_step;
	uint32_t z_max;
	simtime_t t_length;
	simtime_t t_buffer;

	init_params_t():
		d_start(-1.0), d_end(-1.0), d_step(-1.0),
		z_max(-1),
		t_length(0), t_buffer(0)
	{ }

	bool ok() const { return (d_start > 0.0) && (d_step > 0.0) && (t_length > 0); }
};


//-----------------------------------------------------------------------------
// SATMSG_BASE_GO_CYCLE
struct cycle_params_t {
	simtime_t t_start;
	simtime_t t_period;
	uint32_t  n_floats;
	uint16_t  tx_slot_seconds;

	cycle_params_t(): t_start(0), t_period(0), n_floats(0), tx_slot_seconds(0) { }
};


//-----------------------------------------------------------------------------
// SATMSG_BASE_SELF


//-----------------------------------------------------------------------------


#endif
