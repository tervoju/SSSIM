/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <stdint.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <deque>
#include <pthread.h>

#include "util-leastsquares.h"
#include "util-convert.h"
#include "client-print.h" // dprint
#include "ctd-tracker.h"
#include "ctd-estimate.h"

double salt_est[] = {
	6.1252307891845703, 6.1252307891845703, 6.1264242172241214, 6.1288110733032228, 6.1311979293823242, 6.1368541717529297,   // first: 0 m
	6.1425104141235352, 6.1550354957580566, 6.1675605773925781, 6.1778645515441895, 6.1881685256958008, 6.2043288092214164,
	6.2204890927470320, 6.2374983623482807, 6.2560843177319301, 6.2746702731155803, 6.2934159839970230, 6.3151970371481791,
	6.3369780902993353, 6.3587591434504924, 6.3826600059144702, 6.4072674714827231, 6.4318749370509760, 6.4564824026192280,
	6.4813089631901066, 6.5081073704268420, 6.5349057776635773, 6.5617041849003117, 6.5885025921370470, 6.6153009993737824,
	6.6427473034118885, 6.6760246539471320, 6.7093020044823755, 6.7425793550176190, 6.7758567055528616, 6.8091340560881051,
	6.8430088837941492, 6.8822610060373943, 6.9215131282806395, 6.9607652505238855, 7.0000173727671307, 7.0392694950103758,
	7.0788624922434487, 7.1215233643849691, 7.1641842365264896, 7.2068451086680092, 7.2495059808095297, 7.2921668529510502,
	7.3354800065358479, 7.3846636931101486, 7.4338473796844484, 7.4830310662587483, 7.5322147528330490, 7.5813984394073488,
	7.6302599906921387, 7.6762223243713379, 7.7221846580505371, 7.7681469917297363, 7.8141093254089355, 7.8600716590881348,
	7.9065249284108479, 7.9573966185251868, 8.0082683086395257, 8.0591399987538654, 8.1100116888682052, 8.1608833789825432,
	8.2114699840545651, 8.2594908237457272, 8.3075116634368893, 8.3555325031280514, 8.4035533428192135, 8.4515741825103756,
	8.4987026214599606, 8.5377994537353512, 8.5768962860107418, 8.6159931182861325, 8.6550899505615231, 8.6941867828369137,
	8.7318001747131344, 8.7560626029968258, 8.7803250312805172, 8.8045874595642086, 8.8288498878479000, 8.8531123161315914,
	8.8773747444152828, 8.9016371726989743, 8.9258996009826657, 8.9501620292663571, 8.9744244575500485, 8.9986868858337399,
	9.0229493141174313, 9.0472117424011227, 9.0714741706848141, 9.0957365989685055, 9.1199990272521969, 9.1442614555358883,
	9.1686801211039217, 9.1945049222310384, 9.2203297233581534, 9.2461545244852701, 9.2719793256123850, 9.2978041267395017,
	9.3236289278666185, 9.3494537289937334, 9.3752785301208501, 9.4011033312479650, 9.4269281323750818, 9.4527529335021967,
	9.4785777346293134, 9.5044025357564283, 9.5302273368835451, 9.5560521380106600, 9.5818769391377767, 9.6077017402648917,
	9.6335265413920084, 9.6593513425191233, 9.6851761436462400, 9.7110009447733567, 9.7368257459004717, 9.7626505470275884,
	9.7884753481547033, 9.8143001492818200, 9.8401249504089350, 9.8659497515360517, 9.8917745526631666, 9.9175993537902833,
	9.9436142412821447, 9.9713399060567216, 9.9990655708312985, 10.026791235605875, 10.054516900380452, 10.082242565155029,
	10.109968229929606, 10.137693894704183, 10.165419559478760, 10.193145224253337, 10.220870889027914, 10.248596553802489,
	10.276322218577066, 10.304047883351643, 10.331773548126220, 10.359499212900797, 10.387224877675374, 10.414950542449951,
	10.442676207224528, 10.470401871999105, 10.498127536773682, 10.525853201548259, 10.553578866322836, 10.581304531097413 }; // last: 149 m

double temp_est[] = {
	16.285240173339844, 16.285240173339844, 16.270661926269533, 16.241505432128907, 16.212348937988281, 16.155048370361328,   // first: 0 m
	16.097747802734375, 15.940177917480469, 15.782608032226562, 15.529661655426025, 15.276715278625488, 15.000703178116119,
	14.724691077606748, 14.412858638490398, 14.034502824935450, 13.656147011380501, 13.277691827478384, 12.897348614561357,
	12.517005401644330, 12.136662188727303, 11.784455924321620, 11.441628642753049, 11.098801361184478, 10.755974079615907,
	10.418302997106366, 10.127037509435468, 9.8357720217645692, 9.5445065340936708, 9.2532410464227723, 8.9619755587518739,
	8.6757482101938059, 8.4348639214621848, 8.1939796327305654, 7.9530953439989442, 7.7122110552673240, 7.4713267665357037,
	7.2350466251373291, 7.0402038097381592, 6.8453609943389893, 6.6505181789398193, 6.4556753635406494, 6.2608325481414795,
	6.0727203687032061, 5.9451839129130040, 5.8176474571228027, 5.6901110013326006, 5.5625745455423985, 5.4350380897521973,
	5.3122854550679524, 5.2325872103373205, 5.1528889656066896, 5.0731907208760578, 4.9934924761454260, 4.9137942314147951,
	4.8388479550679522, 4.8066693941752119, 4.7744908332824707, 4.7423122723897295, 4.7101337114969892, 4.6779551506042480,
	4.6489391803741453, 4.6483865261077879, 4.6478338718414305, 4.6472812175750731, 4.6467285633087156, 4.6461759090423582,
	4.6465072949727375, 4.6547950426737463, 4.6630827903747560, 4.6713705380757649, 4.6796582857767737, 4.6879460334777834,
	4.6965460777282715, 4.7079567909240723, 4.7193675041198730, 4.7307782173156738, 4.7421889305114746, 4.7535996437072754,
	4.7648812982771132, 4.7750014252132837, 4.7851215521494543, 4.7952416790856258, 4.8053618060217964, 4.8154819329579670,
	4.8256020598941376, 4.8357221868303091, 4.8458423137664797, 4.8559624407026503, 4.8660825676388209, 4.8762026945749914,
	4.8863228215111629, 4.8964429484473335, 4.9065630753835041, 4.9166832023196747, 4.9268033292558462, 4.9369234561920168,
	4.9473003387451175, 4.9599880218505863, 4.9726757049560550, 4.9853633880615238, 4.9980510711669925, 5.0107387542724613,
	5.0234264373779300, 5.0361141204833988, 5.0488018035888675, 5.0614894866943363, 5.0741771697998050, 5.0868648529052738,
	5.0995525360107425, 5.1122402191162113, 5.1249279022216800, 5.1376155853271488, 5.1503032684326175, 5.1629909515380863,
	5.1756786346435550, 5.1883663177490238, 5.2010540008544925, 5.2137416839599613, 5.2264293670654300, 5.2391170501708988,
	5.2518047332763675, 5.2644924163818363, 5.2771800994873050, 5.2898677825927738, 5.3025554656982425, 5.3152431488037113,
	5.3276668580373130, 5.3377148024241130, 5.3477627468109130, 5.3578106911977130, 5.3678586355845130, 5.3779065799713131,
	5.3879545243581140, 5.3980024687449140, 5.4080504131317140, 5.4180983575185140, 5.4281463019053140, 5.4381942462921140,
	5.4482421906789140, 5.4582901350657140, 5.4683380794525149, 5.4783860238393149, 5.4884339682261150, 5.4984819126129150,
	5.5085298569997150, 5.5185778013865150, 5.5286257457733150, 5.5386736901601159, 5.5487216345469159, 5.5587695789337159 }; // last: 149 m


//-----------------------------------------------------------------------------
inline double max_abs(double a, double b) { a = fabs(a); b = fabs(b); return a > b ? a : b; }

inline double mix(double a, double b, double s) { return (1.0 - s) * a + s * b; }


// distance from z0 to find v0 in v[z]
double z_distance(double v0, double z0, double v[], double zd_max) {
	double zd = zd_max;

	// below
	int z_below_lim = z0 + zd_max;
	for (int z = z0; z < z_below_lim; ++z) {
		if (z >= zvar_count - 1) break;
		if ((v[z] <= v0) ^ (v[z+1] < v0)) {
			double dv = fabs(v[z+1] - v[z]);
			if (dv <= 1e-5) {
				zd = z - z0;
				if (zd < 0.0) zd = 0.0;
			} else {
				zd = (z + fabs(v0 - v[z]) / dv) - z0;
			}
			break;
		}
	}

	// above
	int z_above_lim = floor(z0 - zd);
	for (int z = z0; z > z_above_lim; --z) {
		if (z <= 0) break;
		if ((v[z] <= v0) ^ (v[z-1] < v0)) {
			double dv = fabs(v[z-1] - v[z]);
			if (dv <= 1e-5) {
				zd = z - z0;
			} else {
				double zda = (z - fabs(v0 - v[z]) / dv) - z0;
				if (-zda < zd) zd = zda;
			}
			break;
		}
	}

	return zd;
}

double z_distance(double v0, double z0, double v[], bool * zd_max) {
	static double zd_max_dist = 5.0;
	double zd = z_distance(v0, z0, v, zd_max_dist);
	*zd_max = (zd >= zd_max_dist);
	return zd;
}


void ctd_minmax_depth(uint32_t t_lim, const ctd_iter_t & it_end, ctd_iter_t & it, double & z0, double & z1) {
	unsigned int pressure_min = 1e9, pressure_max = 0;
	do {
		if (it->pressure > pressure_max) pressure_max = it->pressure;
		if (it->pressure < pressure_min) pressure_min = it->pressure;
		if (it->time <= t_lim) break;
	} while (++it != it_end);
	if (it == it_end) --it;

	z0 = ctd.depth(pressure_min);
	if (z0 < 0.0) z0 = 0.0;
	z1 = ctd.depth(pressure_max);
	if (z1 < z0) z1 = z0;
}


//-----------------------------------------------------------------------------
bool _cmp_depth(const zvt_struct & a, const zvt_struct & b) { return a.depth < b.depth; }
void density_filter(zvt_vector & dv) {
	if (dv.empty()) return;
	std::sort(dv.begin(), dv.end(), _cmp_depth);

	// maximise surface value & set its depth to surface depth
	zvt_iter it = dv.begin(), max_it = it;
	while ((it != dv.end()) && (it->depth < DRIFTER_SURFACE_DEPTH)) {
		if (it->value > max_it->value) max_it = it;
		++it;
	}
	if (max_it + 1 < it) dv.erase(max_it + 1, it);
	if (max_it != dv.begin()) dv.erase(dv.begin(), max_it);
	if (dv.begin()->depth < DRIFTER_SURFACE_DEPTH) dv.begin()->depth = DRIFTER_HALF_HEIGHT;
	if (dv.empty()) return;

	// force monotonic increase by removing older data
	zvt_iter prev = dv.begin();
	for (it = dv.begin() + 1; it != dv.end(); ++it) {
		if (it->value < prev->value) it = dv.erase(it->timestamp < prev->timestamp ? it : prev);
		if (it == dv.end()) return;
		prev = it;
	}
}

// assumes a density_filter()'ed density vector
zvt_vector salt_vector(zvt_vector & dv) {
	zvt_vector sv;

	unsigned int n = dv.size();
	if (n) {
		zvt_const_iter it = dv.begin();
		if (it->depth < DRIFTER_SURFACE_DEPTH) { ++it; --n; }
		if (n) {
			sv.reserve(n);
			for (; it != dv.end(); ++it) {
				const double & z = it->depth;
				sv.push_back(zvt_struct(z, estimate_salinity(z, estimate_temperature(z), it->value), it->timestamp));
			}
		}
	}
	return sv;
}


//-----------------------------------------------------------------------------
double _estimate_var(double var[], double z) {
	double z_int_d, z_fract = modf(z, &z_int_d);
	int z_int = z_int_d;
	if (z_int < 0)               return var[0];
	if (z_int >= zvar_count - 1) return var[zvar_count - 1];
	                             return var[z_int] + z_fract * (var[z_int + 1] - var[z_int]);
}

//-----------------------------------------------------------------------------
double estimate_temperature(double z) { return _estimate_var(temp_est, z); }

bool new_temperature_estimate(double depth, double temp, double range, double temp_new_est[]) {
	static double zd_max = 5.0;

	double zd = z_distance(temp, depth, temp_est, zd_max);
	if (fabs(zd) >= zd_max) zd = 0.0;
	double td = temp - estimate_temperature(depth + zd);

	int zr_lim_above = depth - range;     if (zr_lim_above < 0) zr_lim_above = 0;
	int zr_lim_below = depth + range + 1; if (zr_lim_below > zvar_count) zr_lim_below = zvar_count;

	int z;
	for (z = 0; z <= zr_lim_above; ++z) temp_new_est[z] = estimate_temperature(z);
	for ( ; z < zr_lim_below; ++z) {
		double zf = 1.0 - fabs(depth - z) / range;
		temp_new_est[z] = estimate_temperature(z + zd * zf) + td * zf;
	}
	for ( ; z < zvar_count; ++z) temp_new_est[z] = estimate_temperature(z);

	return true;
}

bool new_temperature_estimate(uint32_t t0, double range, double temp_new_est[], bool debug_print = false) {
	static int z_adj_range = 3;
	static double zd_max = 5.0;

	ctd.lock_data();
		ctd_iter_t ctd_last = ctd.data.begin();
		double z0, z1;
		ctd_minmax_depth(t0, ctd.data.end(), ctd_last, z0, z1);

		int depth_above = floor(z0), depth_below = ceil(z1);
		UpdateSum * temp_adj = new UpdateSum[depth_below - depth_above + 1];
		for (int i = 0; i <= depth_below - depth_above; ++i) {
			temp_adj[i].value = 0.0;
			temp_adj[i].weight = 0.0;
		}
		int depth_0 = depth_above;

		ctd_iter_t ctd_i;
		for (ctd_i = ctd.data.begin(); ctd_i <= ctd_last; ++ctd_i) {
			double d_i = depth_from_pressure(ctd_i->pressure - ctd.atm_p);

			int d_above = floor(d_i);
			double w_above = 1 - fabs(d_i - d_above);
			temp_adj[d_above - depth_0].add(ctd_i->temperature, w_above);

			int d_below = ceil(d_i);
			double w_below = 1 - fabs(d_below - d_i);
			temp_adj[d_below - depth_0].add(ctd_i->temperature, w_below);
		}
	ctd.unlock_data();

	while ((temp_adj[depth_above - depth_0].weight < 0.5) && (depth_above <= depth_below)) ++depth_above;
	while ((temp_adj[depth_below - depth_0].weight < 0.5) && (depth_above <= depth_below)) --depth_below;
	if (depth_above >= depth_below) {
		//printf("T: fail, valid only from %d to %d m\n", depth_above, depth_below);
		delete[] temp_adj;
		return false;
	}

	if (debug_print) dprint_stdout("NT: z %im .. %im\n", depth_above, depth_below);

	for (int i = depth_above - depth_0; i <= depth_below - depth_0; ++i) temp_adj[i].normalize();

	// interpolate missing data
	for (int i = depth_above - depth_0 + 1; i < depth_below - depth_0; ++i) {
		if (temp_adj[i].weight > 0.0) continue;
		int jump = 1;
		while (temp_adj[i + jump].weight <= 0.0) ++jump;
		double dv = (temp_adj[i + jump].value - temp_adj[i - 1].value) / (jump+1);
		for (int j = 0; j < jump; ++j) temp_adj[i + j].value = temp_adj[i - 1].value + (j+1) * dv;
		i += jump;
	}

	for (int z = depth_above; z <= depth_below; ++z) temp_new_est[z] = temp_adj[z - depth_0].value;
	delete[] temp_adj;

	// find mean error in z for above & below ends of new measurements wrt prev estimate
	UpdateSum zd_above = {0.0, 0.0}, zd_below = {0.0, 0.0};
	for (int z = depth_above; z <= depth_below; ++z) {
		if ((z >= depth_above + z_adj_range) && (z <= depth_below - z_adj_range)) continue;

		double zd = z_distance(temp_new_est[z], z, temp_est, zd_max);
		if (fabs(zd) < zd_max) {
			if (z < depth_above + z_adj_range) zd_above.add(zd, 1.0);
			if (z > depth_below - z_adj_range) zd_below.add(zd, 1.0);
		}
	}
	zd_above.normalize();
	zd_below.normalize();

	double td_above = temp_new_est[depth_above] - estimate_temperature(depth_above + zd_above.value);
	double td_below = temp_new_est[depth_below] - estimate_temperature(depth_below + zd_below.value);

	//printf("T: %dm:Z%+.2f,T%+.2f  %dm:Z%+.2f,T%+.2f\n",
	//	depth_above, zd_above.value, td_above,
	//	depth_below, zd_below.value, td_below);

	int z;
	for (z = 0; z <= depth_above - range; ++z) temp_new_est[z] = estimate_temperature(z);
	for ( ; z < depth_above; ++z) {
		double zf = 1.0 - (depth_above - z) / range;
		temp_new_est[z] = estimate_temperature(z + zd_above.value * zf) + td_above * zf;
	}

	z = depth_below + 1;
	for (int i = 0; (i < range) && (z < zvar_count); ++i, ++z) {
		double zf = 1.0 - (z - depth_below) / range;
		temp_new_est[z] = estimate_temperature(z + zd_below.value * zf) + td_below * zf;
	}
	for ( ; z < zvar_count; ++z) temp_new_est[z] = estimate_temperature(z);
	//for (int z = depth_below + 1; z < zvar_count; ++z)
		//temp_new_est[z] = estimate_temperature(z + zd_below.value) + td_below;

	return true;
}

void adjust_temperature_estimate(uint32_t t0, bool debug_print) {
	double temp_new_est[zvar_count];
	bool new_temp_ok = new_temperature_estimate(t0, TEMP_UPDATE_RANGE, temp_new_est, debug_print);
	if (new_temp_ok) memcpy(temp_est, temp_new_est, sizeof(temp_est));
}

//-----------------------------------------------------------------------------
double estimate_salinity(double z) { return _estimate_var(salt_est, z); }

double estimate_salinity(double salt_guess, double temp, unsigned int pressure, double d0) {
	static double ds_dd = 1.30;
	static double d_err_max = 1e-5;
	static int max_loops = 5;

	//double d0 = get_own_density();
	//if (d0 <= 0.0) return -1.0;

	double
		s1 = salt_guess,
		d1 = density_from_STD(s1, temp, pressure),
		d_err = fabs(d0 - d1);
	if (d_err < d_err_max) return s1;

	double
		s2 = s1 - (d1 - d0) * ds_dd,
		d2 = density_from_STD(s2, temp, pressure);
	d_err = fabs(d0 - d2);
	if (d_err < d_err_max) return s2;

	double s_new, d_new;
	for (int i = 0; i < max_loops; ++i) {
		ds_dd = (s2 - s1) / (d2 - d1);
		s_new = s1 - (d1 - d0) * ds_dd;
		d_new = density_from_STD(s_new, temp, pressure);
		double d_new_err = fabs(d0 - d_new);
		//printf(" ~%i S %.3f  D %.3f  𝚫D %f", i+1, s_new, d_new, d_new_err);
		if (d_new_err > d_err) {
			// fail. badness.
			return (fabs(d0 - d1) < fabs(d0 - d2)) ? s1 : s2;
		}
		d_err = d_new_err;
		if (d_err < d_err_max) return s_new;
		if ((d1 < d0) ^ (d2 < d0)) {
			// d0 between d1, d2
			if ((d1 < d0) ^ (d_new < d0)) {
				s2 = s_new;
				d2 = d_new;
			} else {
				s1 = s_new;
				d1 = d_new;
			}
		} else {
			// d0 outside range d1, d2
			if ((d1 < d0) ^ (d1 < d2)) {
				s2 = s_new;
				d2 = d_new;
			} else {
				s1 = s_new;
				d1 = d_new;
			}
		}
	}

	return s_new;
}

double estimate_salinity(double z, double temp, double density) {
	if (z < DRIFTER_SURFACE_DEPTH) return estimate_salinity(z);
	return estimate_salinity(estimate_salinity(z), temp, pressure_from_depth(z), density);
}

bool new_salinity_estimate(const zvt_vector & salt_map, double range, double salt_new_est[]) {
	if (salt_map.empty()) {
		//printf("S: fail, empty data!\n");
		return false;
	}

	//printf("S:\n");
	zs_delta_t zs_delta;
	for (zvt_const_iter it = salt_map.begin(); it != salt_map.end(); ++it) {
		const double & z = it->depth;
		const double & s = it->value;
		bool zd_max = true;
		double zd = z_distance(s, z, salt_est, &zd_max);
		if (zd_max) zs_delta.push_back(zs_delta_s(z, 0.0, s - estimate_salinity(z)));
		else        zs_delta.push_back(zs_delta_s(z, zd, 0.0));
		//printf("  %.2fm:%.3f %c%+.2f\n", z, s, zd_max?'S':'Z', zd_max?(s - estimate_salinity(z)):zd);
	}
	if (zs_delta.empty()) {
		//printf("S: fail, all points (%u) too near surface\n", static_cast<int>(salt_map.size()));
		return false;
	}
	//putchar('\n');

	int z = 0;
	zs_delta_t::iterator zs_it = zs_delta.begin();

	// above
	for (z = 0; z <= zs_it->z - range; ++z) salt_new_est[z] = estimate_salinity(z);
	for ( ; z < zs_it->z; ++z) {
		double zf = 1.0 - (zs_it->z - z) / range;
		salt_new_est[z] = estimate_salinity(z + zs_it->zd * zf) + zs_it->sd * zf;
	}

	// between
	zs_delta_t::iterator zs_prev = zs_it;
	while (++zs_it != zs_delta.end()) {
		double fm = 1.0 / (zs_it->z - zs_prev->z);
		double zd_d = zs_it->zd - zs_prev->zd;
		double sd_d = zs_it->sd - zs_prev->sd;
		for ( ; z < zs_it->z; ++z) {
			double f = fm * (z - zs_prev->z);
			salt_new_est[z] = estimate_salinity(z + zs_prev->zd + f * zd_d) + zs_prev->sd + f * sd_d;
		}
		zs_prev = zs_it;
	}

	// below
	for (int i = 0; (i < range) && (z < zvar_count); ++i, ++z) {
		double zf = 1.0 - (z - zs_prev->z) / range;
		salt_new_est[z] = estimate_salinity(z + zs_prev->zd * zf) + zs_prev->sd * zf;
	}
	for ( ; z < zvar_count; ++z) salt_new_est[z] = estimate_salinity(z);

	return true;
}

void adjust_salinity_estimate(zvt_vector & density_map) {
	double salt_new_est[zvar_count];
	density_filter(density_map);
	bool new_salt_ok = new_salinity_estimate(salt_vector(density_map), SALT_UPDATE_RANGE, salt_new_est);
	if (new_salt_ok) memcpy(salt_est, salt_new_est, sizeof(salt_est));
}


//-----------------------------------------------------------------------------
double estimate_density(double z) {
	double salt = estimate_salinity(z);
	double temp = estimate_temperature(z);
	unsigned int pressure = pressure_from_depth(z);
	return density_from_STD(salt, temp, pressure);
}

#define _estimate_density(z)\
	density_from_STD(\
		_estimate_var(new_salt_ok ? salt_new_est : salt_est, z),\
		_estimate_var(new_temp_ok ? temp_new_est : temp_est, z),\
		pressure_from_depth(z)\
	)

double estimate_density(double z, zvt_vector & density_map, uint32_t t0) {
	density_filter(density_map);
	double salt_new_est[zvar_count], temp_new_est[zvar_count];
	bool new_temp_ok = new_temperature_estimate(t0, TEMP_ADJUST_RANGE, temp_new_est);
	bool new_salt_ok = new_salinity_estimate(salt_vector(density_map), SALT_ADJUST_RANGE, salt_new_est);

	double de = _estimate_density(z); //density_from_STD(salt, temp, pressure_from_depth(z));

	//double d0 = de;
	//printf("[%.3f:", de - 1000);

	if (density_map.empty()) {
		//putchar('E');
	} else {
		const double & z_above = density_map.begin()->depth;
		if (z <= z_above) {
			// above
			//putchar('A');
			const double & d_above = density_map.begin()->value;
			double e_above = d_above - _estimate_density(z_above);
			if (de > d_above) {
				de = d_above;
				if (density_map.size() > 1) {
					zvt_const_iter it = density_map.begin();
					++it;
					const double & z1 = it->depth;
					const double & d1 = it->value;
					double dd_dz = (d1 - d_above) / (z1 - z_above);
					de -= dd_dz * (z_above - z);
				} else de -= 0.05;
			} else {
				de += e_above;
			}
			//printf(" %.2f~%+.3f@%.0f", d_above-1000,e_above, z_above);
		} else {
			const double & z_below = density_map.rbegin()->depth;
			if (z >= z_below) {
				// below
				//putchar('B');
				const double & d_below = density_map.rbegin()->value;
				double e_below = d_below - _estimate_density(z_below);
				if (de < d_below) {
					de = d_below;
					if (density_map.size() > 1) {
						zvt_vector::const_reverse_iterator it = density_map.rbegin();
						++it;
						const double & z0 = it->depth;
						const double & d0 = it->value;
						double dd_dz = (d_below - d0) / (z_below - z0);
						de += dd_dz * (z - z_below);
					} else de += 0.05;
				} else if (e_below > 0.0) de += e_below;
				//printf(" %.2f~%+.3f@%.0f", d_below-1000, e_below, z_below);
			} else {
				// between
				//putchar('C');
				zvt_const_iter it = density_map.begin();
				zvt_const_iter prev = it;
				while (++it != density_map.end()) {
					if (z > it->depth) { prev = it; continue; }
					const double & z0 = prev->depth;
					const double & d0 = prev->value;
					double e0 = d0 - _estimate_density(z0);
					while ((it != density_map.end()) && (it->value <= d0 + 1e-4)) ++it;
					if (it == density_map.end()) { it = prev; ++it; }
					const double & z1 = it->depth;
					const double & d1 = it->value;
					double e1 = d1 - _estimate_density(z1);
					if ((de < d0) || (de > d1) || (max_abs(e0, e1) > 0.5 * (d1 - d0))) {
						//putchar('$');
						de = mix(d0, d1, (z - z0) / (z1 - z0));
					} else {
						de += mix(e0, e1, (z - z0) / (z1 - z0));
					}
					//printf(" %.2f~%+.3f@%.0f", d0-1000, e0, z0);
					//printf(" %.2f~%+.3f@%.0f", d1-1000, e1, z1);
					break;
				}
			}
		}
	}

	//if (de != d0) printf(":%.3f", de - 1000);
	//putchar(']');

	return de;
}


//-----------------------------------------------------------------------------
bool estimate_soundchannel(int z_max, int * z, int * z_above, int * z_below) {
	const double soundchannel_range = 1.5; // seconds faster than min soundspeed
	double ss[zvar_count];

	//int z_max = get_bottom_depth();
	if (z_max >= zvar_count) z_max = zvar_count - 1;
	double ss_min = 1.0e9;
	int ss_min_z = -1;
	for (int zi = 0; zi <= z_max; ++zi) {
		double ss_z = soundspeed_from_STD(salt_est[zi], temp_est[zi], pressure_from_depth(zi));
		ss[zi] = ss_z;
		if (ss_z < ss_min) {
			ss_min = ss_z;
			ss_min_z = zi;
		}
	}

	*z = ss_min_z;
	if (z_above != NULL) *z_above = -1;
	if (z_below != NULL) *z_below = -1;
	if ((ss_min_z <= 0) || (ss_min_z >= z_max)) return false;

	double ss_max = ss_min + soundchannel_range;

	int za;
	for (za = ss_min_z; za >= 0; --za) if (ss[za] >= ss_max) break;
	if (z_above != NULL) *z_above = za + 1;

	int zb;
	for (zb = ss_min_z; zb <= z_max; ++zb) if (ss[zb] >= ss_max) break;
	if (z_below != NULL) *z_below = zb - 1;

	return ((za > 0) && (zb < z_max));
}

