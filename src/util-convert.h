#ifndef _util_convert_h
#define _util_convert_h

#define  CONVERT_ITS90_TO_IPTS68  1.00024

#define  STANDARD_ATMOSPHERIC_PRESSURE  1013.25
// standard atmospheric pressure is 1013.25 millibar


/**
 * Depth > Pressure calculation using corrective terms for Baltic Sea
 * source: C. C. Leroy and F Parthiot, "Depth-pressure relationship in the oceans and seas" (1998) J. Acoust. Soc. Am. 103(3) pp 1346-1352
 *
 * \param[in] depth depth (meters)
 * \return gauge pressure (millibar)
 */
unsigned int pressure_from_depth( double depth );

/**
 * Pressure > Depth calculation using corrective terms for Baltic Sea
 * source: C. C. Leroy and F Parthiot, "Depth-pressure relationship in the oceans and seas" (1998) J. Acoust. Soc. Am. 103(3) pp 1346-1352
 *
 * \param[in] pressure gauge pressure (millibar)
 * \return depth depth (meters)
 */
double depth_from_pressure( unsigned int pressure );


/**
 * CTD > Salinity calculation using UNESCO international equation of state for seawater
 * source: UNESCO Technical Papers in Marine Science, Vol. 44 (1980)
 *
 * \param[in] conduct conductivity (mS/cm)
 * \param[in] temp temperature (ITS-90)
 * \param[in] pressure gauge pressure (millibar)
 * \return salinity (PSS-78)
 */
double salinity_from_CTD( double conduct, double temp, unsigned int pressure );


/**
 * STD > Conductivity calculation using UNESCO international equation of state for seawater
 * source: UNESCO Technical Papers in Marine Science, Vol. 44 (1980)
 *
 * \param[in] S salinity (PSS-78)
 * \param[in] temp temperature (ITS-90)
 * \param[in] pressure gauge pressure (millibar)
 * \return conductivity (mS/cm)
 */
double conductivity_from_STD( double S, double temp, unsigned int pressure );


/**
 * STD > Density calculation using UNESCO international equation of state for seawater
 * source: UNESCO Technical Papers in Marine Science, Vol. 44 (1980)
 *
 * \param[in] S salinity (PSS-78)
 * \param[in] temp temperature (ITS-90)
 * \param[in] pressure gauge pressure (millibar)
 * \return density (kg/m^3)
 */
double density_from_STD( double S, double temp, unsigned int pressure );


/**
 * Wrapper for salinity_from_CTD and density_from_STD
 *
 * \param[in] conduct conductivity (mS/cm)
 * \param[in] temp temperature (ITS-90)
 * \param[in] pressure gauge pressure (millibar)
 * \return density (kg/m^3)
 */
double density_from_CTD(double conduct, double temp, unsigned int pressure);


/**
 * STD > Speed of sound calculation using UNESCO algorithm & ITS-90 terms by Wong & Zhu
 * sources: UNESCO Technical Papers in Marine Science, Vol. 44 (1980),
 * G.S.K. Wong and S Zhu, "Speed of sound in seawater as a function of salinity, temperature and pressure" (1995) J. Acoust. Soc. Am. 97(3) pp 1732-1736
 *
 * \param[in] S salinity (PSS-78)
 * \param[in] temp temperature (ITS-90)
 * \param[in] pressure gauge pressure (millibar)
 * \return sound speed (m/s)
 */
double soundspeed_from_STD( double S, double temp, unsigned int pressure );


/* Potential temperature calculations
 *
 * simple:
 * PT = (T+273.15) *  Math.pow(1000/ P, 0.286) - 273.15;	// P in millibar
 * T = (PT+273.15) / Math.pow(1000/ P, 0.286) - 273.15;		// P in millibar
 *
 * see also:
 * UNESCO #44
 * http://www.es.flinders.edu.au/~mattom/Utilities/pottemp.html
 */

#endif // _util_convert_h
