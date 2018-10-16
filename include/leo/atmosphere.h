/* -*- c++ -*- */
/*
 * gr-leo: SatNOGS GNU Radio Out-Of-Tree Module
 *
 *  Copyright (C) 2018, Libre Space Foundation <https://libre.space/>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INCLUDED_LEO_ATMOSPHERE_H
#define INCLUDED_LEO_ATMOSPHERE_H

#include <leo/api.h>
#include <gnuradio/random.h>

namespace gr
{
  namespace leo
  {

    /**
     * @brief
     *
     */

    class LEO_API atmosphere
    {

    public:

      atmosphere (float frequency, float watervap, float temperature);

      ~atmosphere ();

      float
      get_atmo_gases_attenuation (float elevation);

    private:

      float d_frequency;
      float d_watervap;
      float d_temperature;
      float d_af, d_bf, d_cf;
      float d_azf, d_bzf, d_czf;
      float d_atmo_attenuation;

      gr::random d_rng;

      typedef std::tuple<float, float, float, float> atmo_coefficients_t;

      /**
       * Data taken from Ippolito - Radiowave Propagation in Satellite Communications
       * Table 3-1  Coefficients for the Calculation of specific attenuation due
       * to gaseous absorption.
       */
      std::vector<atmo_coefficients_t> d_atmo_gases_coeff
        { atmo_coefficients_t (1, 0.00588, 0.0000178, 0.0000517),
            atmo_coefficients_t (4, 0.00802, 0.000141, 0.0000850),
            atmo_coefficients_t (6, 0.00824, 0.000300, 0.0000895),
            atmo_coefficients_t (12, 0.00898, 0.00137, 0.000108) };

      /**
       * Data taken from Ippolito - Radiowave Propagation in Satellite Communications
       * Table 3-2  Coefficients for the Calculation of Total Zenith Atmospheric Attenuation.
       */
      std::vector<atmo_coefficients_t> d_atmo_gases_coeff_zenith
        { atmo_coefficients_t (1, 0.0334, 0.00000276, 0.000112),
            atmo_coefficients_t (4, 0.0397, 0.000276, 0.000176),
            atmo_coefficients_t (6, 0.0404, 0.000651, 0.000196),
            atmo_coefficients_t (12, 0.0436, 0.00318, 0.000315), };

      float
      m (float y1, float y2, float f1, float f2);

      float
      calc_atmo_coeff (float y1, float y2, float f1, float f2, float f0);

      atmosphere::atmo_coefficients_t
      get_atmo_coeff (float frequency, std::vector<atmo_coefficients_t>* coeff_table);

    };

  } // namespace leo
} // namespace gr

#endif /* INCLUDED_LEO_ATMOSPHERE_H */

