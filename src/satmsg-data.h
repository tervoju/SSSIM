#ifndef _satmsg_data_h
#define _satmsg_data_h


#define  DEPTH_HISTORY_SIZE      6
#define  ENV_LAYERS_COUNT        4

#define  SATMSG_SOURCE           0x01  // LSB

#define  SATMSG_FLOAT_EMPTY      0x00  // LSB: 0
#define  SATMSG_FLOAT_SELF       0x02
#define  SATMSG_FLOAT_GROUP      0x04
#define  SATMSG_FLOAT_ENV        0x08
#define  SATMSG_FLOAT_RANGES     0x10
                                 
#define  SATMSG_BASE_EMPTY       0x01  // LSB: 1
#define  SATMSG_BASE_GO_INIT     0x02
#define  SATMSG_BASE_GO_CYCLE    0x04
#define  SATMSG_BASE_GO_END      0x08
#define  SATMSG_BASE_SELF        0x10
#define  SATMSG_BASE_GROUP       0x20
#define  SATMSG_BASE_ENV         0x40

#define  SATMSG__OK               0x70
#define  SATMSG__ERROR_NO_SIGNAL  0xE0
#define  SATMSG__ERROR_BAD_ID     0xE1
#define  SATMSG__ERROR_BAD_DATA   0xE2


//-----------------------------------------------------------------------------
class floatmsg_t {
	private:
		floatmsg_t(const floatmsg_t & _);
		floatmsg_t & operator= (const floatmsg_t & _);

		char * msg_buf;

		void set(unsigned char msg_type, const char * buf, size_t len);

	public:
		self_status_t * self;
		rel_group_status_t * group;
		env_status_t * env;

		//floatmsg_t(unsigned char msg_type = 0, const char * buf = NULL, size_t len = 0);
		floatmsg_t(const SatMsg * sat_msg = NULL);
		~floatmsg_t();

		unsigned char type() const;
		size_t csize() const;
		const char * cdata();
};


//-----------------------------------------------------------------------------
class basemsg_t {
	private:
		basemsg_t(const basemsg_t & _);
		basemsg_t & operator= (const basemsg_t & _);

		char * msg_buf;

		void set(unsigned char msg_type, const char * buf, size_t len);

	public:
		init_params_t * set_init;
		cycle_params_t * set_cycle;

		std::vector<abs_pos_with_id> group;

		//basemsg_t(unsigned char msg_type = 0, const char * buf = NULL, size_t len = 0);
		basemsg_t(const SatMsg * sat_msg = NULL);
		~basemsg_t();

		unsigned char type() const;
		size_t csize() const;
		const char * cdata();
};


//-----------------------------------------------------------------------------
char * satmsg_type2str(unsigned char msg_type, char * s0, ssize_t max_len);

#endif
