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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gnuradio/io_signature.h>
#include <leo/tracker.h>
#include <chrono>
#include <ctime>

namespace gr
{
  namespace leo
  {

    tracker::tracker_sptr
    tracker::make (satellite::satellite_sptr satellite_info,
                     const float gs_lat, const float gs_lon,
                     const float gs_alt, const std::string& obs_start,
                     const std::string& obs_end, const std::string& name)
    {
      return tracker::tracker_sptr (
          new tracker (satellite_info, gs_lat, gs_lon,
                         gs_alt, obs_start, obs_end, name));
    }

    tracker::tracker (satellite::satellite_sptr satellite_info,
                      const float gs_lat, const float gs_lon,
                      const float gs_alt, const std::string& obs_start,
                      const std::string& obs_end, const std::string& name) :
            d_observer (gs_lat, gs_lon, gs_alt),
            d_tle (Tle (satellite_info->get_tle_title(), satellite_info->get_tle_1(), satellite_info->get_tle_2())),
            d_sgp4 (d_tle),
            d_obs_start (parse_ISO_8601_UTC (obs_start)),
            d_obs_end (parse_ISO_8601_UTC (obs_end)),
            d_obs_elapsed (d_obs_start)
    {
      my_id = base_unique_id++;

    }

    tracker::~tracker ()
    {
    }


    int tracker::base_unique_id = 1;

    int
    tracker::unique_id ()
    {
      return my_id;
    }

    double
    tracker::find_max_elevation (Observer& observer, SGP4& sgp4,
                                 const DateTime& aos, const DateTime& los)
    {

      bool running;

      double time_step = (los - aos).TotalSeconds () / 9.0;
      DateTime current_time (aos); //! current time
      DateTime time1 (aos); //! start time of search period
      DateTime time2 (los); //! end time of search period
      double max_elevation; //! max elevation

      running = true;

      do {
        running = true;
        max_elevation = -99999999999999.0;
        while (running && current_time < time2) {
          /*
           * find position
           */
          Eci eci = sgp4.FindPosition (current_time);
          CoordTopocentric topo = observer.GetLookAngle (eci);

          if (topo.elevation > max_elevation) {
            /*
             * still going up
             */
            max_elevation = topo.elevation;
            /*
             * move time along
             */
            current_time = current_time.AddSeconds (time_step);
            if (current_time > time2) {
              /*
               * dont go past end time
               */
              current_time = time2;
            }
          }
          else {
            /*
             * stop
             */
            running = false;
          }
        }

        /*
         * make start time to 2 time steps back
         */
        time1 = current_time.AddSeconds (-2.0 * time_step);
        /*
         * make end time to current time
         */
        time2 = current_time;
        /*
         * current time to start time
         */
        current_time = time1;
        /*
         * recalculate time step
         */
        time_step = (time2 - time1).TotalSeconds () / 9.0;
      }
      while (time_step > 1.0);

      return max_elevation;
    }

    DateTime
    tracker::find_crossing_point_time (Observer& observer, SGP4& sgp4,
                                       const DateTime& initial_time1,
                                       const DateTime& initial_time2,
                                       bool finding_aos)
    {
      bool running;
      int cnt;

      DateTime time1 (initial_time1);
      DateTime time2 (initial_time2);
      DateTime middle_time;

      running = true;
      cnt = 0;
      while (running && cnt++ < 16) {
        middle_time = time1.AddSeconds ((time2 - time1).TotalSeconds () / 2.0);
        /*
         * calculate satellite position
         */
        Eci eci = sgp4.FindPosition (middle_time);
        CoordTopocentric topo = observer.GetLookAngle (eci);

        if (topo.elevation > 0.0) {
          /*
           * satellite above horizon
           */
          if (finding_aos) {
            time2 = middle_time;
          }
          else {
            time1 = middle_time;
          }
        }
        else {
          if (finding_aos) {
            time1 = middle_time;
          }
          else {
            time2 = middle_time;
          }
        }

        if ((time2 - time1).TotalSeconds () < 1.0) {
          /*
           * two times are within a second, stop
           */
          running = false;
          /*
           * remove microseconds
           */
          int us = middle_time.Microsecond ();
          middle_time = middle_time.AddMicroseconds (-us);
          /*
           * step back into the pass by 1 second
           */
          middle_time = middle_time.AddSeconds (finding_aos ? 1 : -1);
        }
      }

      /*
       * go back/forward 1second until below the horizon
       */
      running = true;
      cnt = 0;
      while (running && cnt++ < 6) {
        Eci eci = sgp4.FindPosition (middle_time);
        CoordTopocentric topo = observer.GetLookAngle (eci);
        if (topo.elevation > 0) {
          middle_time = middle_time.AddSeconds (finding_aos ? -1 : 1);
        }
        else {
          running = false;
        }
      }

      return middle_time;
    }


    double
    tracker::get_slant_range ()
    {
      double elevation;

      Eci eci = d_sgp4.FindPosition (get_elapsed_time ());
      CoordTopocentric topo = d_observer.GetLookAngle (eci);
      elevation = Util::RadiansToDegrees (topo.elevation);
//      std::cout << "Time: " << get_elapsed_time () << " | Elevation: "
//          << elevation << "| Slant Range: " << topo.range << std::endl;
      if (elevation < 3) {
        return 0;
      }
      else {
        return topo.range;
      }
    }

    DateTime
    tracker::parse_ISO_8601_UTC (const std::string& datetime)
    {
      std::tm tm;
      strptime (datetime.c_str (), "%Y-%m-%dT%H:%M:%S", &tm);
      std::chrono::system_clock::time_point tp =
          std::chrono::system_clock::from_time_t (std::mktime (&tm));

      std::time_t t = std::chrono::system_clock::to_time_t (tp);
      std::tm utc_tm = *localtime (&t);

      return DateTime (utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                       utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec);
    }

    void
    tracker::add_elapsed_time (size_t microseconds)
    {
      d_obs_elapsed = d_obs_elapsed.AddTicks (microseconds);
    }

    DateTime
    tracker::get_elapsed_time ()
    {
      return d_obs_elapsed;
    }

    bool
    tracker::is_observation_over ()
    {
      if (d_obs_elapsed >= d_obs_end) {
        return true;
      }
      else {
        return false;
      }
    }

  } /* namespace leo */
} /* namespace gr */

