// SigmaLowEnergy.h is a part of the PYTHIA event generator.
// Copyright (C) 2025 Torbjorn Sjostrand.
// PYTHIA is licenced under the GNU GPL v2 or later, see COPYING for details.
// Please respect the MCnet Guidelines, see GUIDELINES for details.

// Header file for cross sections for of energy hadron-hadron collisions.

#ifndef Pythia8_SigmaLowEnergy_H
#define Pythia8_SigmaLowEnergy_H

#include "Pythia8/HadronWidths.h"
#include "Pythia8/NucleonExcitations.h"
#include "Pythia8/PhysicsBase.h"
#include "Pythia8/SigmaTotal.h"

namespace Pythia8 {

//==========================================================================

// Gets cross sections for hadron-hadron collisions at low energies.

class SigmaLowEnergy : public PhysicsBase {

public:

  // Initialize.
  void init(NucleonExcitations* nucleonExcitationsPtrIn);

  // Get the total cross section for the specified collision.
  double sigmaTotal(int idA, int idB, double eCM, double mA, double mB);
  double sigmaTotal(int idAIn, int idBIn, double eCMIn) {
    double mA0 = particleDataPtr->m0(idAIn), mB0 = particleDataPtr->m0(idBIn);
    return sigmaTotal(idAIn, idBIn, eCMIn, mA0, mB0); }

  // Get the partial cross section for the specified collision and process.
  // proc | 0: total; | 1: nondiff; | 2 : el; | 3: SD (XB); | 4: SD (AX);
  //      | 5: DD;  | 6: CD (AXB, not implemented)
  //      | 7: excitation | 8: annihilation | 9: resonant
  //      | >100: resonant through the specified resonance particle
  double sigmaPartial(int idA, int idB, double eCM,
    double mA, double mB, int proc);
  double sigmaPartial(int idAIn, int idBIn, double eCMIn, int proc) {
    double mA0 = particleDataPtr->m0(idAIn), mB0 = particleDataPtr->m0(idBIn);
    return sigmaPartial(idAIn, idBIn, eCMIn, mA0, mB0, proc); }

  // Gets all partial cross sections for the specified collision.
  // This is used when all cross sections are needed to determine which
  // process to execute. Returns false if no processes are available.
  bool sigmaPartial(int idA, int idB, double eCM, double mA, double mB,
    vector<int>& procsOut, vector<double>& sigmasOut);

  // Picks a process randomly according to their partial cross sections.
  int pickProcess(int idA, int idB, double eCM, double mA, double mB);

  // Picks a resonance according to their partial cross sections.
  int pickResonance(int idA, int idB, double eCM);

  // For NN / N Nbar / Nbar Nbar collisions explicit excitation states.
  // replace a generic smeared-out low-mass diffraction component.
  bool hasExcitation(int idAIn, int idBIn) const { return (abs(idAIn) == 2212
    || abs(idAIn) == 2112) && (abs(idBIn) == 2212 || abs(idBIn) == 2112); }

  // Update the list of internal resonances.
  void updateResonances();

  // Cross sections below threshold are assumed numerically equal to zero.
  static constexpr double TINYSIGMA = 1.e-9;

private:

  NucleonExcitations* nucleonExcitationsPtr;

  // Masses and parameters.
  double mp, sp, s4p, mPi, mK,
         sEffAQM, cEffAQM, bEffAQM, fracEtass, fracEtaPss;

  // Flag for disabling inelastic cross sections.
  bool doInelastic;

  // Mode for calculating total cross sections for pi pi and pi K.
  bool useSummedResonances;

  // List of hadron pairs that are allowed to form resonances.
  set<pair<int, int> > resonatingPairs;

  // Current configuration.
  int idA, idB;
  double mA, mB, eCM;
  int collType;
  bool didFlipSign, didSwapIds;

  // Cached cross sections.
  double sigTot, sigND, sigEl, sigXB, sigAX, sigXX, sigAnn, sigEx, sigResTot;
  vector<pair<int, double>> sigRes;

  // Set current configuration, ordering inputs hadrons in a canonical way.
  void setConfig(int idAIn, int idBIn, double eCMIn, double mAIn, double mBIn);

  // Methods for computing cross sections.
  void calcTot();
  void calcRes();
  double calcRes(int idR) const;
  void calcEla();
  void calcEx();
  void calcDiff();

  // HPR1R2 fit for parameterizing certain total cross sections.
  double HPR1R2(double p, double r1, double r2, double mA, double mB, double s)
    const;

  // HERA/CERN fit for parameterizing certain elastic cross sections.
  double HERAFit(double a, double b, double n, double c, double d, double p)
    const;

  // Additive quark model for generic collisions and for scale factors.
  double nqEffAQM(int id) const;
  double factorAQM() const;
  double totalAQM() const;
  double elasticAQM() const;

  // LowEnergyProcess should have access to nqEffAQM for slope in t.
  friend class LowEnergyProcess;

// Check whether the current configuration allows for explicit resonances.
  bool hasExplicitResonances() const;
  double meltpoint(int idX, int idM) const;

};

//==========================================================================

// Get hadron-hadron cross sections, interpolating the low- and high-energy
// behaviours in SigmaLowEnergy and SigmaSaSDL respectively.

class SigmaCombined : public PhysicsBase {

public:

  // Initialize.
  void init(SigmaLowEnergy* sigmaLowPtrIn);

  // Get total cross section.
  double sigmaTotal(int id1, int id2, double eCM12, double m1, double m2,
    int mixLoHi = 0);

  // Get partial cross sections.
  double sigmaPartial(int id1, int id2, double eCM12, double m1, double m2,
    int type, int mixLoHi = 0);

private:

  // The main objects for low-and high-energy cross sections.
  SigmaLowEnergy* sigmaLowPtr = {};
  SigmaSaSDL sigmaSDL = {};

  // Initialization data.
  double eMinHigh, deltaEHigh, eMaxHigh;

  // Store proton mass.
  double mp;

  // Buffer recently used input and related output.
  int id1SDL = {}, id2SDL = {}, mixSDL = {};
  double eCM12SDL = {}, sigSDL[10] = {};

};

//==========================================================================

} // end namespace Pythia8

#endif // Pythia8_SigmaLowEnergy_H
