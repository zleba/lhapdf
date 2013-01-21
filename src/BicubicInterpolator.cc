#include "LHAPDF/GridPDF.h"
#include "LHAPDF/BicubicInterpolator.h"
#include <iostream>

namespace LHAPDF {


  double interpolateCubic(double T, double VL, double VDL, double VH, double VDH) {
    // Pre-calculate powers of T
    const double t2 = T*T;
    const double t3 = t2*T;

    // Calculate left point
    const double p0 = (2*t3 - 3*t2 + 1)*VL;
    const double m0 = (t3 - 2*t2 + T)*VDL;

    // Calculate right point
    const double p1 = (-2*t3 + 3*t2)*VH;
    const double m1 = (t3 - t2)*VDH;

    return p0 + m0 + p1 + m1;
  }


  // Provides d/dx at all grid locations
  /// @todo Make into a non-member function (and use an unnamed namespace)
  double BicubicInterpolator::ddx(const GridPDF::KnotArray1F& subgrid, size_t ix, size_t iq2) const {
    /// @todo Re-order this if so that branch prediction will favour the "normal" central case
    if (ix == 0) { //< If at leftmost edge, use forward difference
      return (subgrid.xf(ix+1, iq2) - subgrid.xf(ix, iq2)) / (subgrid.xs()[ix+1] - subgrid.xs()[ix]);
    } else if (ix == subgrid.xs().size() - 1) { //< If at rightmost edge, use backward difference
      return (subgrid.xf(ix, iq2) - subgrid.xf(ix-1, iq2)) / (subgrid.xs()[ix] - subgrid.xs()[ix-1]);
    } else { //< If central, use the central difference
      const double lddx = (subgrid.xf(ix, iq2) - subgrid.xf(ix-1, iq2)) / (subgrid.xs()[ix] - subgrid.xs()[ix-1]);
      const double rddx = (subgrid.xf(ix+1, iq2) - subgrid.xf(ix, iq2)) / (subgrid.xs()[ix+1] - subgrid.xs()[ix]);
      return (lddx + rddx) / 2.0;
    }
  }


  double BicubicInterpolator::interpolateXQ2(int id, double x, double q2) const {
    /// @todo Move the following to the Interpolator interface and implement caching
    // Subgrid lookup
    /// @todo Do this in two stages to cache the KnotArrayNF
    /// @todo Flavour error checking
    const GridPDF::KnotArray1F& subgrid = pdf().subgrid(q2, id);
    // Index look-up
    /// @todo Cache this lookup
    const size_t ix = subgrid.xlow(x);
    const size_t iq2 = subgrid.q2low(q2);
    /// @todo End of section to be moved
    return interpolateXQ2(subgrid, ix, iq2);
  }


  /// @todo Make into a non-member function (and use an unnamed namespace)
  double BicubicInterpolator::interpolateXQ2(const GridPDF::KnotArray1F& subgrid, size_t ix, size_t iq2) const {
    // Distance parameters
    const double dx = subgrid.xs()[ix+1] - subgrid.xs()[ix];
    const double tx = (x - subgrid.xs()[ix]) / dx;
    const double dq_0 = subgrid.q2s()[iq2] - subgrid.q2s()[iq2-1];
    const double dq_1 = subgrid.q2s()[iq2+1] - subgrid.q2s()[iq2];
    const double dq_2 = subgrid.q2s()[iq2+2] - subgrid.q2s()[iq2+1];
    const double dq = dq1;
    const double tq = (q2 - subgrid.q2s()[iq2]) / dq;

    // Points in Q2
    double vl = interpolateCubic(tx, subgrid.xf(ix, iq2), ddx(subgrid, ix, iq2) * dx,
                                     subgrid.xf(ix+1, iq2), ddx(subgrid, ix+1, iq2) * dx;
    double vh = interpolateCubic(tx, subgrid.xf(ix, iq2+1), ddx(subgrid, ix, iq2+1) * dx,
                                     subgrid.xf(ix+1, iq2+1), ddx(subgrid, ix+1, iq2+1) * dx);

    // Derivatives in Q2
    double vdl, vdh;
    if (iq2 == 0) {
      // Forward difference for lower q
      vdl = (vh - vl) / dq_1;
      // Central difference for higher q
      double vhh = interpolateCubic(tx, subgrid.xf(ix, iq2+2), ddx(subgrid, ix, iq2+2) * dx,
                                        subgrid.xf(ix+1, iq2+2), ddx(subgrid, ix+1, iq2+2) * dx);
      vdh = (vdl + (vhh - vh)/dq_2) / 2.0;
    }
    else if (iq2+1 == subgrid.q2s().size()-1) {
      // Backward difference for higher q
      vdh = (vh - vl) / dq_1;
      // Central difference for lower q
      double vll = interpolateCubic(tx, subgrid.xf(ix, iq2-1), ddx(subgrid, ix, iq2-1) * dx,
                                        subgrid.xf(ix+1, iq2-1), ddx(subgrid, ix+1, iq2-1) * dx);
      vdl = (vdh + (vl - vll)/dq_0) / 2.0;
    }
    else {
      // Central difference for both q
      double vll = interpolateCubic(tx, subgrid.xf(ix, iq2-1), ddx(subgrid, ix, iq2-1) * dx,
                                        subgrid.xf(ix+1, iq2-1), ddx(subgrid, ix+1, iq2-1) * dx);
      vdl = ( (vh - vl)/dq_1 + (vl - vll)/dq_0 ) / 2.0;
      double vhh = interpolateCubic(tx, subgrid.xf(ix, iq2+2), ddx(subgrid, ix, iq2+2) * dx,
                                        subgrid.xf(ix+1, iq2+2), ddx(subgrid, ix+1, iq2+2) * dx);
      vdh = ( (vh - vl)/dq_1 + (vhh - vh)/dq_2 ) / 2.0;
    }

    vdl *= dq;
    vdh *= dq;

    return interpolateCubic(tq, vl, vdl, vh, vdh);
  }


}
