/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cmath>

#include "util-convert.h"


/*	pressure <> depth calculations based on
 *	Leroy & Prathiot 1997: "Depth-pressure relationships in the oceans and seas"
 *	using corrective terms for Baltic Sea
 *	depth in meters, gauge pressure in millibars
 */
unsigned int pressure_from_depth( double depth ) {
	//g_phi = 9.819176693;
	//k = ( g_phi - z * 2e-5 ) / ( 9.80612 - z * 2e-5 );
	//h_45 = depth * ( 1.00818e-2 + depth * ( 2.465e-8 + depth * ( -1.25e-13 + depth * 2.8e-19 ) ) );
	//h_baltic = 1.8e-4 * z;
	//P = h_45 * k - h_baltic; // MPa

	double k = ( 9.819176693 - depth * 2e-5 ) / ( 9.80612 - depth * 2e-5 );
	double h_45 = depth * ( 1.00818e-2 + depth * ( 2.465e-8 + depth * ( -1.25e-13 + depth * 2.8e-19 ) ) );
	return 1e4 * h_45 * k - depth * 1.8e-4;
}

// alternative shallow freshwater depth approximation: depth (m) = gauge pressure (dbar) * 1.019716
double depth_from_pressure( unsigned int pressure ) {
	double P = (double)pressure * 1e-4;	// MPa
	//z = ( 9.7266e2 * P - 2.512e-1 * P * P + 2.28e-4 * P * P * P - 1.8e-7 * P * P * P * P ) /
	//    ( 9.8191767 + 1.1e-4 * P ) + 1.8 * P;
	return ( P * ( 1.8 +
		( 972.66 + P * ( P * ( 2.28e-4 - P * 1.8e-7 ) - 0.2512 ) ) /
		( 9.8191767 + 1.1e-4 * P )
	) );
}


inline double salinity_ratio_temp( double T ) {
	return 0.6766097 + T * ( 2.00564e-2 + T * ( 1.104259e-4 + T * ( -6.9698e-7 + T * 1.0031e-9 ) ) );
}

// R_2 = sqrt(Rt)
double salinity_RT( double R_2, double T ) {
	double a = 0.0080 + R_2 * ( -0.1692 + R_2 * ( 25.3851 + R_2 * ( 14.0941 + R_2 * ( -7.0261 + R_2 *  2.7081 ) ) ) );
	double b = 0.0005 + R_2 * ( -0.0056 + R_2 * ( -0.0066 + R_2 * ( -0.0375 + R_2 * (  0.0636 + R_2 * -0.0144 ) ) ) );
	return ( a + b * (T-15) / ( 1 + (T-15) * 0.0162 ) );
}
double deriv_salinity_RT( double R_2, double T ) {
	double a = -0.1692 + R_2 * ( 50.7702 + R_2 * ( 42.2823 + R_2 * ( -28.1044 + R_2 * 13.5405 ) ) );
	double b = -0.0056 + R_2 * ( -0.0132 + R_2 * ( -0.1125 + R_2 * (   0.2544 + R_2 * -0.0720 ) ) );
	return ( a + b * (T-15) / ( 1 + (T-15) * 0.0162 ) );
}

// in: conductivity (mS/cm), temperature (ITS-90), gauge pressure (millibar)
// out: salinity (PSS-78)
double salinity_from_CTD( double conduct, double temp, unsigned int pressure ) {
	double
		T = temp * CONVERT_ITS90_TO_IPTS68,
		P = (double)pressure * 1e-2,	// decibar
		R = conduct / 42.9140,	// raw conductivity ratio, wrt S 35, temp 15, depth 0
		Rp;

	if ( R < 0.0005 ) return 0.0;

	// pressure correction
	Rp = 1 + (
		P * ( 2.07e-5 + P * ( -6.37e-10 + P * 3.989e-15 ) ) /
		( 1 + R * 0.4215 + T * ( 3.426e-2 + T * 4.464e-4 + R * -3.107e-3 ) )
	);

	return salinity_RT( sqrt( R / ( Rp * salinity_ratio_temp(T) ) ), T );
}

// in: salinity (PSS-78), temperature (ITS-90), gauge pressure (millibar)
// out: conductivity (mS/cm)
double conductivity_from_STD( double S, double temp, unsigned int pressure ) {
 	double
		T = temp * CONVERT_ITS90_TO_IPTS68,
		P = (double)pressure * 1e-2,
		Rt_2 = sqrt( S / 35.0 ),
		SI = salinity_RT(Rt_2,T),
		Rt_rt,
		a = 0.4215 - T * 3.107e-3,
		b = 1.0 + T * ( 3.426e-2 + T * 4.464e-4 ),
		c = P * ( 2.070e-5 + P * ( -6.37e-10 + P * 3.989e-15 ) );
	int i;

	if ( S < 0.02 ) return 0.0;

	// Newton-Raphson method
	for ( i = 0; i < 10; ++i ) {
		Rt_2 += ( S - SI ) / deriv_salinity_RT(Rt_2,T);
		SI = salinity_RT(Rt_2,T);
		if ( fabs( SI - S ) < 1.0e-4 ) break;
	}

	Rt_rt = salinity_ratio_temp(T) * Rt_2 * Rt_2;
	c = Rt_rt * ( b + c );
	b -= Rt_rt * a;

	return 42.9140 *
		(sqrt(fabs( b*b + 4*a*c )) - b) / (2*a);
}


/*	using UNESCO International Equation of State (1980)
 *	see UNESCO Technical Papers in Marine Science, Vol. 44
 *
 *	in: Salinity (PSS-78), temperature (ITS-90), gauge pressure (millibar)
 *	out: density (kg/m^3)
 */
double density_from_STD( double S, double temp, unsigned int pressure ) {
	double
		T = temp * CONVERT_ITS90_TO_IPTS68,
		P = (double)pressure * 1e-3,	// bar
		S_2 = sqrt(S),
		rho_0 = 999.842594 +
			T * ( 6.793952e-02 + T * ( -9.09529e-03 + T * ( 1.001685e-04 + T * ( -1.120083e-06 + T * 6.536332e-09 ) ) ) ) +	// pure water
			S * (
				0.824493 + T * ( -4.0899e-03 + T * ( 7.6438e-05 + T * ( -8.2467e-07 + T * 5.3875e-09 ) ) ) +
				S_2 * ( -5.72466e-03 + T * ( 1.0227e-04 + T * -1.6546e-06 ) ) +
				S * 4.8314e-04
			),
		K = 19652.21 + T * ( 148.4206 + T * ( -2.327105 + T * ( 1.360477e-2 + T * -5.155288e-5 ) ) ) +		// Kw
			S * (
				54.6746 + T * ( -0.603459 + T * ( 1.09987e-2 + T * -6.1670e-5 ) ) +	// A1
				S_2 * ( 7.944e-2 + T * ( 1.6483e-2 + T * -5.3009e-4 ) ) +	// B1
				P * (
					2.2838e-3 + T * ( -1.0981e-5 + T * -1.6078e-6 ) +	// C
					S_2 * 1.91075e-4 +	// D
					P * ( -9.9348e-7 + T * ( 2.0816e-8 + T * 9.1697e-10 ) )	// E
				)
			) +
			P * (
				3.239908 + T * ( 1.43713e-3 + T * ( 1.16092e-4 + T * -5.77905e-7 ) ) +	// Aw
				P * ( 8.50935e-5 + T * ( -6.12293e-6 + T * 5.2787e-8 ) )				// Bw
			);

	return rho_0 / ( 1 - P/K );
/*
	static double rho_35_0 = 1028.106331;	// rho(35,0,0)
	double V, DK, K35, GAM, SVA;
	rho_0 -= rho_35_0;
	V = 1.0 / rho_35_0;	// V(35,0,P)

	DK = -1930.06 + T * ( 148.4206 + T * ( -2.327105 + T * ( 1.360477e-2 + T * -5.155288e-5 ) ) ) +	// Kw
		S * (
			54.6746 + T * ( -0.603459 + T * ( 1.09987e-2 + T * -6.1670e-5 ) ) +	// A1
			S_2 * ( 7.944e-2 + T * ( 1.6483e-2 + T * -5.3009e-4 ) ) +	// B1
			P * (
				2.2838e-3 + T * ( -1.0981e-5 + T * -1.6078e-6 ) +	// C
				S_2 * 1.91075e-4 +	// D
				P * ( -9.9348e-7 + T * ( 2.0816e-8 + T * 9.1697e-10 ) )	// E
			)
		) +
		P * (
			-0.1194975 + T * ( 1.43713e-3 + T * ( 1.16092e-4 + T * -5.77905e-7 ) ) +	// Aw
			P * ( 3.47718e-5 + T * ( -6.12293e-6 + T * 5.2787e-8 ) )				// Bw
		);
	K35 = 21582.27 + P * ( 3.359406 + P * 5.03217e-5 );	// K(35,0,P)
	GAM = P / K35;

	SVA = -rho_0 * V / ( rho_35_0 + rho_0 );	// P=0
	SVA = SVA * ( 1.0 - GAM ) + P * ( V + SVA ) * DK / ( K35 * ( K35 + DK ) );
	V *= ( 1.0 - GAM );	// V(35,0,P)

	return rho_35_0 +
		GAM / V +	// rho_a(35,0,P)
		-SVA / ( V * ( V + SVA ) );	// rho var. involving spec.vol.anomaly
*/
}


// wrapper for salinity_from_CTD and density_from_STD
// in: conductivity (mS/cm), temperature (ITS-90), gauge pressure (millibar)
// out: salinity (PSS-78)
double density_from_CTD(double conduct, double temp, unsigned int pressure) {
	double S = salinity_from_CTD(conduct, temp, pressure);
	return density_from_STD(S, temp, pressure);
}



/*	using method by Chen & Millero (1977) & ITS-90 terms by Wong & Zhu (1995)
 *	see http://resource.npl.co.uk/acoustics/techguides/soundseawater/content.html
 *
 *	in: Salinity (PSS-78), temperature (ITS-90), gauge pressure (millibar)
 *	out: sound speed (m/s)
 */
double soundspeed_from_STD( double S, double temp, unsigned int pressure ) {
	double P = (double)pressure * 1e-3;	// bar

	double a =	 1402.388  + temp * (  5.03830    + temp * ( -5.8109e-2 + temp * (  3.3432e-4  + temp * ( -1.47797e-6 + temp * 3.1419e-9 ) ) ) )
	    + P * (  0.153563  + temp * (  6.8999e-4  + temp * ( -8.1829e-6 + temp * (  1.3632e-7  + temp * -6.1260e-10 ) ) )
	    + P * (  3.1260e-5 + temp * ( -1.7111e-6  + temp * (  2.5986e-8 + temp * ( -2.5353e-10 + temp *  1.0415e-12 ) ) )
	    + P * ( -9.7729e-9 + temp * (  3.8513e-10 + temp * -2.3654e-12 ) ) ) );

	double b =   1.389     + temp * ( -1.262e-2  + temp * (  7.166e-5   + temp * ( 2.008e-6 +  temp * -3.21e-8 ) ) )
	    + P * (  9.4742e-5 + temp * ( -1.2583e-5 + temp * ( -6.4928e-8  + temp * ( 1.0515e-8 + temp * -2.0142e-10 ) ) )
	    + P * ( -3.9064e-7 + temp * (  9.1061e-9 + temp * ( -1.6009e-10 + temp * 7.994e-12 ) )
	    + P * (  1.100e-10 + temp * (  6.651e-12 + temp * -3.391e-13 ) ) ) );

	double c = -1.922e-2 + temp * -4.42e-5 + P * ( 7.3637e-5 + temp * 1.7950e-7 );

	return a + S * ( b + sqrt(S) * c + S * ( 1.727e-3 + P * -7.9836e-6 ) );

/*
void soundspeed_temp_terms( double t[], double temp ) {
	t[0] =  1402.388  + temp * (  5.03830    + temp * ( -5.8109e-2 + temp * (  3.3432e-4  + temp * ( -1.47797e-6 + temp * 3.1419e-9 ) ) ) );
	t[1] =  0.153563  + temp * (  6.8999e-4  + temp * ( -8.1829e-6 + temp * (  1.3632e-7  + temp * -6.1260e-10 ) ) );
	t[2] =  3.1260e-5 + temp * ( -1.7111e-6  + temp * (  2.5986e-8 + temp * ( -2.5353e-10 + temp *  1.0415e-12 ) ) );
	t[3] = -9.7729e-9 + temp * (  3.8513e-10 + temp * -2.3654e-12 );

	t[4] =  1.389     + temp * ( -1.262e-2  + temp * (  7.166e-5   + temp * ( 2.008e-6 +  temp * -3.21e-8 ) ) );
	t[5] =  9.4742e-5 + temp * ( -1.2583e-5 + temp * ( -6.4928e-8  + temp * ( 1.0515e-8 + temp * -2.0142e-10 ) ) );
	t[6] = -3.9064e-7 + temp * (  9.1061e-9 + temp * ( -1.6009e-10 + temp * 7.994e-12 ) );
	t[7] =  1.100e-10 + temp * (  6.651e-12 + temp * -3.391e-13 );
}
*/
/*	double t[8];
	soundspeed_temp_terms( t, temp );

	return (
		t[0] + P * ( t[1] + P * ( t[2] + P * t[3] ) ) +
		S * (
			t[4] + P * ( t[5] + P * ( t[6] + P * t[7] ) ) +
			sqrt(S) * ( -1.922e-2 + temp * -4.42e-5 + P * ( 7.3637e-5 + temp * 1.7950e-7 ) ) +
			S * ( 1.727e-3 + P * -7.9836e-6 )
		)
	);*/
}
