// -*- C++ -*-
//
// This file is part of LHAPDF
// Copyright (C) 2012-2013 The LHAPDF collaboration (see AUTHORS for details)
//
#pragma once

#include "LHAPDF/Utils.h"
#include "LHAPDF/Exceptions.h"

namespace LHAPDF {


  /// @todo Tidy interface for RC


  /// @brief Calculator interface for computing alpha_s(Q2) in various ways
  ///
  /// The design of the AlphaS classes is that they are substitutible
  /// (cf. polymorphism) and are entirely non-dependent on the PDF and Info
  /// objects: hence they can be used by external code that actually doesn't
  /// want to do anything at all with PDFs, but which just wants to do some
  /// alpha_s interpolation.
  class AlphaS {
  public:

    /// Base class constructor for default param setup
    AlphaS();

    /// Destructor
    virtual ~AlphaS() {};

    /// @name alpha_s values
    //@{

    /// Calculate alphaS(Q)
    double alphasQ(double q) const { return alphasQ2(q*q); }

    /// Calculate alphaS(Q2)
    /// @todo Throw error in this base method if Q < Lambda?
    virtual double alphasQ2(double q2) const = 0;

    //@}


    /// @name alpha_s metadata
    //@{

    /// Calculate the number of active flavours at energy scale Q
    int numFlavorsQ(double q) const { return numFlavorsQ2(q*q); }

    /// Calculate the number of active flavours at energy scale Q2
    virtual int numFlavorsQ2(double q2) const;

    /// Get a quark mass by PDG code
    virtual double qMass(int id);

    /// Set quark masses by PDG code
    ///
    /// Used explicitly in the analytic and ODE solvers.
    virtual void setQMass(int id, double value);

    /// @brief Set the order of QCD (expressed as number of loops)
    ///
    /// Used explicitly in the analytic and ODE solvers.
    void setOrderQCD(int order) { _qcdorder = order; }

    /// @brief Set the Z mass used in this alpha_s
    ///
    /// Used explicitly in the ODE solver.
    virtual void setMZ(double mz) { _mz = mz; }

    /// @brief Set the alpha_s(MZ) used in this alpha_s
    ///
    /// Used explicitly in the ODE solver.
    virtual void setAlphaSMZ(double alphas) { _alphas_mz = alphas; }

    /// @brief Set the @a {i}th Lamda constant for @a i active flavors
    ///
    /// Used explicitly in the analytic solver.
    virtual void setLambda(unsigned int i, double lambda) {};

    //@}


    /// Get the implementation type of this AlphaS
    virtual std::string type() const = 0;


  protected:

    /// @name Calculating beta function values
    //@{

    /// Calculate the i'th beta function given the number of active flavours
    /// Currently limited to 0 <= i <= 3
    /// Calculated using the MSbar scheme
    double _beta(int i, int nf) const;

    /// Calculate a vector of beta functions given the number of active flavours
    /// Currently returns a 4-element vector of beta0 -- beta3
    std::vector<double> _betas(int nf) const;

    //@}


  protected:

    /// Order of QCD evolution (expressed as number of loops)
    int _qcdorder;

    /// Mass of the Z boson in GeV
    double _mz;

    /// Value of alpha_s(MZ)
    double _alphas_mz;

    /// Masses of quarks in GeV
    ///
    /// Used for working out flavor thresholds and the number of quarks that are
    /// active at energy scale Q.
    std::vector<double> _qmasses;

  };



  /// Calculate alpha_s(Q2) by an analytic approximation
  class AlphaS_Analytic : public AlphaS {
  public:

    /// Implementation type of this solver
    std::string type() const { return "analytic"; }

    /// Calculate alphaS(Q2)
    double alphasQ2(double q2) const;

    /// Analytic has its own numFlavorsQ2 which respects the min/max nf set by the Lambdas
    int numFlavorsQ2(double q2) const;

    /// Set lambda_i (for i = flavour number)
    void setLambda(unsigned int i, double lambda);

  private:

    /// Get lambdaQCD for nf
    double _lambdaQCD(int nf) const;

    /// LambdaQCD values. Stored as lambdaQCD_nf = _lambdas[nf-1]
    std::vector<double> _lambdas;

    /// Recalculate min/max flavors in case lambdas have changed
    void _setFlavors();

    /// Max number of flavors
    int _nfmax;
    /// Min number of flavors
    int _nfmin;

  };


  /// Interpolate alpha_s from tabulated points in Q2 via metadata
  /// @todo Add Doxygen strings
  class AlphaS_Ipol : public AlphaS {
  public:

    /// Implementation type of this solver
    std::string type() const { return "ipol"; }

    /// Calculate alphaS(Q2)
    double alphasQ2(double q2) const;

    /// Set the array of Q values for interpolation
    void setQValues(const std::vector<double>& qs) {
      std::vector<double> q2s;
      foreach (double q, qs) q2s.push_back(q*q);
      setQ2Values(q2s);
    }

    /// @brief Set the array of Q2 values for interpolation
    ///
    /// Writes to the same internal array as setQValues, appropriately transformed.
    void setQ2Values(const std::vector<double>& q2s) { _q2s = q2s; }

    /// Set the array of alpha_s(Q2) values for interpolation
    void setAlphaSValues(const std::vector<double>& as) { _as = as; }

  private:

    double _interpolateCubic(double T, double VL, double VDL, double VH, double VDH) const;
    double _ddq_central( size_t i ) const;
    double _ddq_forward( size_t i ) const;
    double _ddq_backward( size_t i ) const;

    std::vector<double> _q2s;
    std::vector<double> _logq2s;
    std::vector<double> _as;

  };


  /// Solve the differential equation in alphaS using an implementation of RK4
  class AlphaS_ODE : public AlphaS {
  public:

    /// Implementation type of this solver
    std::string type() const { return "ode"; }

    /// Calculate alphaS(Q2)
    double alphasQ2( double q2 ) const;

    /// Set MZ, and also the caching flag
    void setMZ( double mz ) { _mz = mz; _calculated = false; }

    /// Set alpha_s(MZ), and also the caching flag
    void setAlphaSMZ( double alphas ) { _alphas_mz = alphas; _calculated = false; }

    /// Set the array of Q values for interpolation, and also the caching flag
    void setQValues(const std::vector<double>& qs) {
      std::vector<double> q2s;
      foreach (double q, qs) q2s.push_back(q*q);
      setQ2Values(q2s);
    }

    /// @brief Set the array of Q2 values for interpolation, and also the caching flag
    ///
    /// Writes to the same internal array as setQValues, appropriately transformed.
    void setQ2Values( std::vector<double> q2s ) { _q2s = q2s; _calculated = false; }

  private:

    /// Calculate the derivative at Q2 = t, alpha_S = y
    double _derivative(double t, double y, const std::vector<double>& beta) const;

    /// Calculate the next step using RK4 with adaptive step size
    void _rk4(double& t, double& y, double h, const double allowed_change, const vector<double>& bs) const;

    /// Solve alpha_s for q2 using RK4
    void _solve(double q2, double& t, double& y, const double& allowed_relative, double h, double accuracy) const;

    /// Create interpolation grid
    void _interpolate() const;

  private:

    /// Vector of Q2s in case specific anchor points are used
    std::vector<double> _q2s;

    /// Whether or not the ODE has been solved yet
    mutable bool _calculated;

    /// The interpolation used to get Alpha_s after the ODE has been solved
    /// @todo Make mutable: only interesting for caching reasons
    mutable AlphaS_Ipol _ipol;

  };


}
