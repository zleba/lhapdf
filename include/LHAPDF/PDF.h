// -*- C++ -*-
//
// This file is part of LHAPDF
// Copyright (C) 2012-2013 The LHAPDF collaboration (see AUTHORS for details)
//
#pragma once

#include "LHAPDF/Info.h"
#include "LHAPDF/PDFIndex.h"
#include "LHAPDF/Factories.h"
#include "LHAPDF/AlphaS.h"
#include "LHAPDF/Utils.h"
#include "LHAPDF/Paths.h"
#include "LHAPDF/Exceptions.h"

namespace LHAPDF {


  /// @brief PDF is the general interface for access to parton density information.
  ///
  /// The PDF interface declares the general form of all PDF types, such as Grid based or analytic.
  class PDF {
  protected: //< These constructors should only be called by subclasses

    /// Internal convenience typedef for the AlphaS object handle
    // typedef AlphaS* AlphaSPtr;
    typedef auto_ptr<AlphaS> AlphaSPtr;


    /// @name Creation and deletion
    //@{

    /// Default constructor.
    PDF() { }

    /// @brief Constructor from a file path.
    ///
    /// This constructor reads the member, set, and global metadata and is hence
    /// most useful for being called from the constructors of derived PDF types, e.g.
    /// GridPDF.
    PDF(const std::string& path) {
      _loadInfo(path);
    }

    /// @brief Constructor from a set name and member ID.
    ///
    /// This constructor reads the member, set, and global metadata and is hence
    /// most useful for being called from the constructors of derived PDF types, e.g.
    /// GridPDF.
    PDF(const std::string& setname, int member) {
      path searchpath = findFile(pdfmempath(setname, member));
      /// @todo Load info lazily?
      _loadInfo(searchpath.string());
    }

    /// @brief Constructor from an LHAPDF ID code.
    ///
    /// This constructor reads the member, set, and global metadata and is hence
    /// most useful for being called from the constructors of derived PDF types, e.g.
    /// GridPDF.
    PDF(int lhaid) {
      const pair<string,int> setname_memid = lookupPDF(lhaid);
      if (setname_memid.second == -1)
        throw IndexError("Can't find a PDF with LHAPDF ID = " + to_str(lhaid));
      path searchpath = pdfmempath(setname_memid.first, setname_memid.second);
      /// @todo Load info lazily?
      _loadInfo(searchpath.string());
    }


  public:

    /// Virtual destructor, to allow unfettered inheritance
    virtual ~PDF() { }

    //@}


  protected:

    void _loadInfo(const path& mempath) {
      _mempath = findFile(mempath);
      _info.loadFull(mempath);
    }


  public:

    /// @name PDF values
    //@{

    /// @brief Get the PDF xf(x) value at (x,q2) for the given PID.
    ///
    /// All grids are defined in Q2 rather than Q since the natural value
    /// in MC programs is squared, so we typically avoid an expensive sqrt() call.
    ///
    /// @param id PDG parton ID
    /// @param x Momentum fraction
    /// @param q2 Squared energy (renormalization) scale
    /// @return The value of xf(x,q2)
    double xfxQ2(int id, double x, double q2) const {
      // Physical x range check
      if (!inPhysicalRangeX(x)) {
        throw RangeError("Unphysical x given: " + to_str(x));
      }
      // Physical Q2 range check
      if (!inPhysicalRangeQ2(q2)) {
        throw RangeError("Unphysical Q2 given: " + to_str(q2));
      }
      // Treat PID = 0 as always equivalent to a gluon: query as PID = 21
      const int id2 = (id != 0) ? id : 21;
      // Undefined PIDs
      if (!hasFlavor(id2)) {
        /// @todo Should look up the UndefFlavourAction flag, but efficiency hit for top?
        //throw FlavorError("Undefined flavour requested: " + to_str(id));
        return 0.0;
      }
      // Call the delegated method in the concrete PDF object to calculate the in-range value
      return _xfxQ2(id2, x, q2);
    }


    /// @brief Get the PDF xf(x) value at (x,q) for the given PID.
    ///
    /// xfxQ will square the given q and return the value from xfxQ2.
    /// All grids are defined in q2 rather than q since the natural value
    /// in MC programs is squared, so we typically avoid an expensive sqrt() call.
    ///
    /// @param id PDG parton ID
    /// @param x Momentum fraction
    /// @param q Energy (renormalization) scale
    /// @return The value of xf(x,q2)
    double xfxQ(int id, double x, double q) const {
      return xfxQ2(id, x, q*q);
    }


    /// @brief Get the PDF xf(x) value at (x,q2) for all supported PIDs.
    ///
    /// This version fills a user-supplied map to avoid container construction
    /// costs on every call.
    ///
    /// @param x Momentum fraction
    /// @param q2 Squared energy (renormalization) scale
    /// @return The value of xf(x,q2)
    void xfxQ2(double x, double q2, std::map<int, double>& rtn) const {
      rtn.clear();
      foreach (int id, flavors()) rtn[id] = xfxQ2(id, x, q2);
    }


    /// @brief Get the PDF xf(x) value at (x,q) for all supported PIDs.
    ///
    /// This version fills a user-supplied map to avoid container construction
    /// costs on every call.
    ///
    /// @param x Momentum fraction
    /// @param q Energy (renormalization) scale
    /// @return The value of xf(x,q)
    void xfxQ(double x, double q, std::map<int, double>& rtn) const {
      xfxQ2(x, q*q, rtn);
    }


    /// @brief Get the PDF xf(x) value at (x,q2) for "standard" PIDs.
    ///
    /// This version fills a user-supplied vector to avoid container
    /// construction costs on every call.
    ///
    /// The filled vector follows the LHAPDF5 convention, with 13 entries
    /// running in the PDF ID order [-6, -5, ..., -1, 21, 1, ... 5, 6], i.e.
    /// quark PDF values will be at vector index pid+6 and the gluon at index 6.
    ///
    /// @param x Momentum fraction
    /// @param q2 Squared energy (renormalization) scale
    /// @return The value of xf(x,q2)
    void xfxQ2(double x, double q2, std::vector<double>& rtn) const {
      rtn.clear();
      rtn.resize(13);
      for (int i = 0; i < 13; ++i) {
        const int id = i-6; // PID = 0 is automatically treated as PID = 21
        rtn[i] = xfxQ2(id, x, q2);
      }
    }


    /// @brief Get the PDF xf(x) value at (x,q) for "standard" PIDs.
    ///
    /// This version fills a user-supplied vector to avoid container
    /// construction costs on every call.
    ///
    /// The filled vector follows the LHAPDF5 convention, with 13 entries
    /// running in the PDF ID order [-6, -5, ..., -1, 21, 1, ... 5, 6], i.e.
    /// quark PDF values will be at vector index pid+6 and the gluon at index 6.
    ///
    /// @param x Momentum fraction
    /// @param q Energy (renormalization) scale
    /// @return The value of xf(x,q)
    void xfxQ(double x, double q, std::vector<double>& rtn) const {
      xfxQ2(x, q*q, rtn);
    }


    /// @brief Get the PDF xf(x) value at (x,q2) for all supported PIDs.
    ///
    /// This version creates a new map on every call: prefer to use the
    /// fill-in-place version with a user-supplied map for many calls.
    ///
    /// @param x Momentum fraction
    /// @param q2 Squared energy (renormalization) scale
    /// @return The value of xf(x,q2)
    std::map<int, double> xfxQ2(double x, double q2) const {
      std::map<int, double> rtn;
      xfxQ2(x, q2, rtn);
      return rtn;
    }

    /// @brief Get the PDF xf(x) value at (x,q) for all supported PIDs.
    ///
    /// This version creates a new map on every call: prefer to use the
    /// fill-in-place version with a user-supplied map for many calls.
    ///
    /// xfxQ will square the given q and return the value from xfxQ2.
    /// All grids are defined in q2 rather than q since the natural value
    /// in MC programs is squared, so we typically avoid an expensive sqrt() call.
    ///
    /// @param x Momentum fraction
    /// @param q Energy (renormalization) scale
    /// @return The value of xf(x,q2)
    std::map<int, double> xfxQ(double x, double q) const {
      return xfxQ2(x, q*q);
    }


  protected:

    /// @brief Calculate the PDF xf(x) value at (x,q2) for the given PID.
    ///
    /// This is the key function to be overridden in concrete PDF types, since
    /// it actually does the calculation of xf(x,Q2) by analytic, interpolation,
    /// or other means. The user-called xfxQ2 method exists so that the physical
    /// range and PID checks need only be done in one place, rather than need to
    /// be re-implemented in each concrete implementation.
    ///
    /// @param id Parton ID in the PDG scheme
    /// @param x Momentum fraction
    /// @param q2 Squared energy (renormalization) scale
    /// @return the value of xf(x,q2)
    virtual double _xfxQ2(int id, double x, double q2) const = 0;

    //@}


  public:

    /// @name Ranges of validity
    //@{

    /// Minimum valid x value for this PDF.
    virtual double xMin() {
      return info().metadata<double>("XMin");
    }

    /// Maximum valid x value for this PDF.
    virtual double xMax() {
      return info().metadata<double>("XMax");
    }

    /// Minimum valid Q2 value for this PDF (in GeV2).
    virtual double q2Min() {
      return info().metadata<double>("Q2Min");
    }

    /// Maximum valid Q2 value for this PDF (in GeV2).
    virtual double q2Max() {
      return info().metadata<double>("Q2Max");
    }

    /// Minimum valid Q value for this PDF (in GeV).
    /// @note This function calls sqrt(q2Min()). For better CPU efficiency and accuracy use q2Min() directly.
    virtual double qMin() {
      return std::sqrt(this->q2Min());
    }

    /// @brief Maximum valid Q value for this PDF (in GeV).
    /// @note This function calls sqrt(q2Max()). For better CPU efficiency and accuracy use q2Max() directly.
    virtual double qMax() {
      return std::sqrt(this->q2Max());
    }

    /// @brief Check whether the given x is physically valid
    ///
    /// Returns false for x less than 0 or greater than 1, since it
    /// is a momentum fraction and not valid outside those values.
    bool inPhysicalRangeX(double x) const {
      return x >= 0.0 && x <= 1.0;
    }

    /// @brief Check whether the given Q2 is physically valid
    ///
    /// Returns false for Q2 less than 0 (Q must be real-valued).
    bool inPhysicalRangeQ2(double q2) const {
      return q2 >= 0.0;
    }

    /// @brief Check whether the given Q is physically valid
    ///
    /// Returns false for Q less than 0 (Q must be positive).
    bool inPhysicalRangeQ(double q) const {
      return inPhysicalRangeQ2(q*q);
    }

    /// Check whether the given (x,Q2) is physically valid
    bool inPhysicalRangeXQ2(double x, double q2) const {
      return inPhysicalRangeX(x) && inPhysicalRangeQ2(q2);
    }

    /// Check whether the given (x,Q) is physically valid
    bool inPhysicalRangeXQ(double x, double q) const {
      return inPhysicalRangeX(x) && inPhysicalRangeQ(q);
    }

    /// @brief Grid range check for Q
    ///
    /// Return true when given Q is in the coverage range of this PDF.
    /// It actually squares the given Q and returns value from inRangeQ2.
    ///
    /// @param q Energy scale
    /// @return Whether q is in range
    virtual bool inRangeQ(double q) const {
      return inRangeQ2(q*q);
    }

    /// @brief Grid range check for Q2
    ///
    /// Return true when given Q2 is in the coverage range of this PDF.
    ///
    /// @param q2 Squared energy scale
    /// @return Whether q2 is in range
    virtual bool inRangeQ2(double q2) const = 0;

    /// @brief Grid range check for x
    ///
    /// Return true when given x is in the coverage range of this PDF.
    ///
    /// @param x Momentum fraction
    /// @return Whether x is in range
    virtual bool inRangeX(double x) const = 0;

    /// Combined range check for x and Q
    virtual bool inRangeXQ(double x, double q) const {
      return inRangeX(x) && inRangeQ(q);
    }

    /// Combined range check for x and Q2
    bool inRangeXQ2(double x, double q2) const {
      return inRangeX(x) && inRangeQ2(q2);
    }

    //@}


    /// @name Generic member-level metadata (including cascaded metadata from set & config level)
    //@{

    /// Get the info class that actually stores and handles the metadata
    Info& info() { return _info; }

    /// Get the info class that actually stores and handles the metadata (const version)
    const Info& info() const { return _info; }

    //@}


    /// @name Set-level metadata
    /// @todo Move all these into PDFSet (without the "set" prefixes) when/if it appears
    //@{

    /// @brief PDF set name
    ///
    /// Obtained from the member file path, not Info-based metadata.
    /// @todo Move into PDFSet when/if it appears
    std::string setname() const {
      return _mempath.parent_path().filename().string();
    }

    /// Description of the set
    /// @todo Move into PDFSet when/if it appears
    std::string setdescription() const {
      return info().metadata("SetDesc");
    }

    /// Get the type of PDF errors in this set (replica, symmhessian, asymmhessian, none)
    /// @todo Move into PDFSet when/if it appears
    std::string errorType() const {
      return to_lower_copy(info().metadata("ErrorType"));
    }

    /// Number of members in this set
    /// @todo Move into PDFSet when/if it appears
    int numMembers() const {
      return info().metadata<int>("NumMembers");
    }

    //@}


    /// @name Member-level metadata
    //@{

    /// @brief PDF member local ID number
    ///
    /// Obtained from the member file path, not Info-based metadata.
    int memberID() const {
      const string memname = _mempath.stem().string();
      assert(memname.find(setname()) == 0);
      const int memid = lexical_cast<int>(memname.substr(memname.length()-4)); //< Last 4 chars should be the member number
      return memid;
    }

    /// @brief PDF member global LHAPDF ID number
    ///
    /// Obtained from the member ID and the set's LHAPDF ID index
    int lhapdfID() const {
      return lookupLHAPDFID(setname(), memberID());
    }

    /// Description of this PDF member
    std::string description() const {
      return info().metadata("PdfDesc");
    }

    /// Get the type of PDF member that this object represents (central, error)
    std::string type() const {
      return to_lower_copy(info().metadata("PdfType"));
    }

    //@}


    /// @name Parton content and QCD parameters
    //@{

    /// @brief List of flavours defined by this PDF set.
    ///
    /// This list is stored locally after its initial read from the Info object
    /// to avoid unnecessary lookups and string decoding, since e.g. it is
    /// looked at by every call to the GridPDF's Interpolator and Extrapolator
    /// classes.
    ///
    /// @todo Make virtual for AnalyticPDF? Or allow manual setting of the Info?
    const std::vector<int>& flavors() const {
      if (_flavors.empty()) {
        _flavors = info().metadata< vector<int> >("Flavors");
        sort(_flavors.begin(), _flavors.end());
      }
      return _flavors;
    }

    /// Checks whether @a id is a valid parton for this PDF.
    bool hasFlavor(int id) const {
      const vector<int>& ids = flavors();
      return find(ids.begin(), ids.end(), id) != ids.end();
    }

    /// @brief Order of QCD at which this PDF has been constructed
    ///
    /// "Order" is defined here and throughout LHAPDF as the maximum number of
    /// loops included in the matrix elements, in order to have an integer value
    /// for easy use in comparisons, as opposed to "LO", "NLO", etc. strings.
    int qcdOrder() const {
      return info().metadata<int>("QcdOrder");
    }

    /// @brief Value of alpha_s(Q2) used by this PDF set.
    ///
    /// Calculated numerically, analytically, or interpolated according to
    /// metadata, using the AlphaS classes.
    double alphaS(double q2) const {
      /// @todo Treat like Ipol/Xpol objects, with lazy default loading and set methods
      if (_alphas.get() == 0) {
        AlphaS* as = mkAlphaS(info().metadata("AlphaS_Type"));
        /// @todo Move this into new setAlphaS method(s)
        // Configure the QCD params on this AlphaS
        if (info().has_key("MZ")) as->mz = info().metadata<double>("MZ");
        if (info().has_key("AlphaS_MZ")) as->alphas_mz = info().metadata<double>("AlphaS_MZ");
        if (info().has_key("AlphaS_OrderQCD")) as->qcdorder = info().metadata<int>("AlphaS_OrderQCD");
        if (info().has_key("MUp")) as->setQmass(1, info().metadata<double>("MUp"));
        if (info().has_key("MDown")) as->setQmass(2, info().metadata<double>("MDown"));
        if (info().has_key("MStrange")) as->setQmass(3, info().metadata<double>("MStrange"));
        if (info().has_key("MCharm")) as->setQmass(4, info().metadata<double>("MCharm"));
        if (info().has_key("MBottom")) as->setQmass(5, info().metadata<double>("MBottom"));
        if (info().has_key("MTop")) as->setQmass(6, info().metadata<double>("MTop"));
        if (info().has_key("Lambda4")) as->lambda4 = info().metadata<double>("Lambda4");
        if (info().has_key("Lambda5")) as->lambda5 = info().metadata<double>("Lambda5");
        /// @todo How to do type triggering to set ipol points for Alphas_Ipol?
        /// @todo Throw an error if the QCD params are changed after a first alpha_s query? How?
        _alphas.reset(as);
      }
      return _alphas->alphasQ2(q2);
    }

    //@}


  protected:

    /// Member data file path
    path _mempath;

    /// Metadata container
    Info _info;

    /// Locally cached list of supported PIDs
    mutable vector<int> _flavors;

    /// Optionally loaded AlphaS object
    mutable AlphaSPtr _alphas;

  };


}
