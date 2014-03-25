// -*- C++ -*-
//
// This file is part of LHAPDF
// Copyright (C) 2012-2013 The LHAPDF collaboration (see AUTHORS for details)
//

#include "LHAPDF/PDFSet.h"
#include <boost/math/distributions/chi_squared.hpp>

namespace LHAPDF {


  PDFSet::PDFSet(const string& setname) {
    _setname = setname;
    const string setinfopath = findpdfsetinfopath(setname);
    if (!file_exists(setinfopath))
      throw ReadError("Data file not found for PDF set '" + setname + "'");
    /// @todo Reinstate this for 6.1.0?
    // /// Print out a banner if sufficient verbosity is enabled
    // const int verbosity = get_entry_as<int>("Verbosity", 1);
    // if (verbosity > 0) {
    //   cout << "Loading PDF set '" << setname << "'" << endl;
    //   print(cout, verbosity);
    // }
    // Load info file
    load(setinfopath);
    /// @todo Check that some mandatory metadata keys have been set: _check() function.
  }


  void PDFSet::print(ostream& os, int verbosity) const {
    stringstream ss;
    if (verbosity > 0)
      ss << name() << ", version " << dataversion() << "; " << size() << " PDF members";
    if (verbosity > 1)
      ss << "\n" << description();
    os << ss.str() << endl;
  }


  double PDFSet::errorConfLevel() const {
    return get_entry_as<double>("ErrorConfLevel", 100*boost::math::erf(1/sqrt(2)));
  }



  PDFUncertainty PDFSet::uncertainty(const vector<double>& values, double inputCL, bool median) const {
    if (size() <= 1)
      throw UserError("Error in LHAPDF::PDFSet::uncertainty. PDF set must contain more than just the central value.");
    if (values.size() != size())
      throw UserError("Error in LHAPDF::PDFSet::uncertainty. Input vector must contain values for all PDF members.");
    const size_t nmem = size()-1;

    // Return value
    PDFUncertainty rtn;
    rtn.central = values[0];

    if (errorType() == "replicas") {

      // Calculate the average and standard deviation using Eqs. (2.3) and (2.4) of arXiv:1106.5788v2.
      double av = 0.0, sd = 0.0;
      for (size_t imem = 1; imem <= nmem; imem++) {
        av += values[imem];
        sd += sqr(values[imem]);
      }
      av /= nmem; sd /= nmem;
      sd = nmem/(nmem-1.0)*(sd-av*av);
      if (sd > 0.0 && nmem > 1) sd = sqrt(sd);
      else sd = 0.0;
      rtn.central = av;
      rtn.errplus = rtn.errminus = rtn.errsymm = sd;

    } else if (errorType() == "symmhessian") {

      double errsymm = 0;
      for (size_t ieigen = 1; ieigen <= nmem; ieigen++)
        errsymm += sqr(values[ieigen]-values[0]);
      rtn.errsymm = sqrt(errsymm);
      rtn.errplus = errsymm;
      rtn.errminus = errsymm;

    } else if (errorType() == "hessian") {

      // Calculate the asymmetric and symmetric Hessian uncertainties
      // using Eqs. (2.1), (2.2) and (2.6) of arXiv:1106.5788v2.
      double errplus = 0, errminus = 0, errsymm = 0;
      for (size_t ieigen = 1; ieigen <= nmem/2; ieigen++) {
        errplus += sqr(max(max(values[2*ieigen-1]-values[0],values[2*ieigen]-values[0]), 0.0));
        errminus += sqr(max(max(values[0]-values[2*ieigen-1],values[0]-values[2*ieigen]), 0.0));
        errsymm += sqr(values[2*ieigen-1]-values[2*ieigen]);
      }
      rtn.errsymm = 0.5*sqrt(errsymm);
      rtn.errplus = sqrt(errplus);
      rtn.errminus = sqrt(errminus);

    } else {
      throw MetadataError("\"ErrorType: " + errorType() + "\" not supported by LHAPDF::PDFSet::uncertainty.");
    }

    // Check that reqCl and errCL both lie between 0 and 1 (using the set CL as target if reqCL > 0)
    const double errCL = errorConfLevel() / 100.0; // convert from percentage
    const double reqCL = (inputCL > 0) ? inputCL / 100.0 : errCL; // convert from percentage
    if (reqCL < 0 || reqCL > 1 || errCL < 0 || errCL > 1) return rtn;

    if (errorType() == "replicas" && median) {

      // Compute median and requested CL directly from probability distribution of replicas
      // Sort "values" into increasing order, ignoring zeroth member (average over replicas)
      vector<double> sorted = values;
      sort(sorted.begin()+1, sorted.end());
      const int nmem = size()-1;
      // Define central value to be median
      if (nmem % 2) { // odd nmem => one middle value
        rtn.central = sorted[nmem/2 + 1];
      } else { // even nmem => average of two middle values
        rtn.central = 0.5*(sorted[nmem/2] + sorted[nmem/2 + 1]);
      }
      // Define uncertainties with a CL given by reqCL
      int upper = round(0.5*(1+reqCL)*nmem); // round to nearest integer
      int lower = 1 + round(0.5*(1-reqCL)*nmem); // round to nearest integer
      rtn.errplus = sorted[upper] - rtn.central;
      rtn.errminus = rtn.central - sorted[lower];
      rtn.errsymm = 0.5*(rtn.errplus + rtn.errminus); // symmetrised

    } else {

      // Calculate the qth quantile of the chi-squared distribution with one degree of freedom.
      // Examples: quantile(dist, q) = {0.988946, 1, 2.70554, 3.84146, 4} for q = {0.68, 1-sigma, 0.90, 0.95, 2-sigma}.
      boost::math::chi_squared dist(1);
      double qerrCL = boost::math::quantile(dist, errCL);
      double qreqCL = boost::math::quantile(dist, reqCL);
      const double scale = sqrt(qreqCL/qerrCL);
      rtn.errplus *= scale;
      rtn.errminus *= scale;
      rtn.errsymm *= scale;
      rtn.scale = scale;

    }

    return rtn;
  }



  double PDFSet::correlation(const vector<double>& valuesA, const vector<double>& valuesB) const {
    if (valuesA.size() != size() || valuesB.size() != size())
      throw UserError("Error in LHAPDF::PDFSet::correlation. Input vectors must contain values for all PDF members.");

    const PDFUncertainty errA = uncertainty(valuesA);
    const PDFUncertainty errB = uncertainty(valuesB);
    const size_t nmem = size()-1;

    double cor = 0.0;
    if (errorType() == "replicas" && nmem > 1) {

      // Calculate the correlation using Eq. (2.7) of arXiv:1106.5788v2.
      for (size_t imem = 1; imem <= nmem; imem++) {
        cor += valuesA[imem] * valuesB[imem];
      }
      cor = (cor/nmem - errA.central*errB.central) / (errA.errsymm*errB.errsymm) * nmem/(nmem-1.0);

    } else if (errorType() == "symmhessian") {

      for (size_t ieigen = 1; ieigen <= nmem; ieigen++) {
        cor += (valuesA[ieigen]-errA.central) * (valuesB[ieigen]-errB.central);
      }
      cor /= errA.errsymm * errB.errsymm;

    } else if (errorType() == "hessian") {

      // Calculate the correlation using Eq. (2.5) of arXiv:1106.5788v2.
      for (size_t ieigen = 1; ieigen <= nmem/2; ieigen++) {
        cor += (valuesA[2*ieigen-1]-valuesA[2*ieigen]) * (valuesB[2*ieigen-1]-valuesB[2*ieigen]);
      }
      cor /= 4.0 * errA.errsymm * errB.errsymm;

    }

    return cor;
  }


  double PDFSet::randomValue(const vector<double>& values, const vector<double>& random, bool symmetrise) const {
    if (values.size() != size())
      throw UserError("Error in LHAPDF::PDFSet::randomValue. Input vector must contain values for all PDF members.");

    double frand = 0.0;
    double sigma = 100 * boost::math::erf(1/sqrt(2));
    double scale = uncertainty(values, sigma).scale;
    size_t nmem = size()-1;

    // Allocate number of eigenvectors based on ErrorType.
    size_t neigen = 0;
    if (errorType() == "hessian") {
      neigen = nmem/2;
    } else if (errorType() == "symmhessian") {
      neigen = nmem;
    } else {
      throw UserError("Error in LHAPDF::PDFSet::randomValue. This PDF set is not in the Hessian format.");
    }

    if (random.size() != neigen)
      throw UserError("Error in LHAPDF::PDFSet::randomValue. Input vector must contain random numbers for all eigenvectors.");

    if (errorType() == "symmhessian") {

      frand = values[0];
      // Loop over number of eigenvectors.
      for (size_t ieigen = 1; ieigen <= neigen; ieigen++) {
        double r = random[ieigen-1]; // Gaussian random number
        frand += r*abs(values[ieigen]-values[0])*scale;
      }

    } else if (errorType() == "hessian") {

      // Use either Eq. (6.4) or Eq. (6.5) of arXiv:1205.4024v2.

      frand = values[0];
      // Loop over number of eigenvectors.
      for (size_t ieigen = 1; ieigen <= neigen; ieigen++) {
        double r = random[ieigen-1]; // Gaussian random number
        if (symmetrise) {
          frand += 0.5*r*abs(values[2*ieigen-1]-values[2*ieigen]) * scale;
        } else { // not symmetrised
          if (r < 0.0) frand -= r*(values[2*ieigen]-values[0]) * scale; // negative direction
          else frand += r*(values[2*ieigen-1]-values[0])*scale; // positive direction
        }
      }

    }

    return frand;
  }


}