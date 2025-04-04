// VinciaMergingHooks.h is a part of the PYTHIA event generator.
// Copyright (C) 2025 Torbjorn Sjostrand, Peter Skands.
// PYTHIA is licenced under the GNU GPL v2 or later, see COPYING for details.
// Please respect the MCnet Guidelines, see GUIDELINES for details.

// This file is written by Helen Brooks, Christian T Preuss.

#include "Pythia8/VinciaMergingHooks.h"
#include "Pythia8/VinciaHistory.h"

namespace Pythia8 {

using namespace VinciaConstants;

//==========================================================================

// The HardProcessParticle class methods.

//-------------------------------------------------------------------------

void HardProcessParticle::print() const {
  cout << name();
  if (!isIntermediate()) return;
  cout << "( -->";
  // Loop over decay daughters.
  for(auto dit=daughters.begin(); dit != daughters.end(); ++dit) {
    const HardProcessParticle* p = listPtr->getPart(*dit);
    if (p != nullptr) cout << "  " << p->name();
  }
  cout << " )";
}

//==========================================================================

// The HardProcessParticleList class methods.

//-------------------------------------------------------------------------

void HardProcessParticleList::list() const {

  cout << "\n *--------  VINCIA Hard Process Summary ----------------------"
       <<"------------------------*\n\n"
       << "  Hard Process:\n\n  ";
  // Loop over levels.
  for (auto it = particles.begin(); it != particles.end(); ++it) {
    if (it->first > 0) cout << " -->";
    // Loop over particles at this level and print.
    for (auto pit = it->second.begin(); pit != it->second.end(); ++pit) {
      cout << " "; pit->print();}
  }
  cout << endl << endl
       << " *---------------------------------------------------------------"
       <<"---------------------*\n";

}

//-------------------------------------------------------------------------

// Add multiparticle to list.

ParticleLocator HardProcessParticleList::add(int level, string nameIn,
  const MultiParticle* multiPtr, vector<ParticleLocator>& mothersIn) {
  ParticleLocator loc = getNextLoc(level);
  particles[level].push_back(
    HardProcessParticle(nameIn, multiPtr, loc, this, mothersIn));
  return loc;
}

//-------------------------------------------------------------------------

// Add particle to list from data.

ParticleLocator HardProcessParticleList::add(int level, int idIn,
  ParticleDataEntryPtr pdata,vector<ParticleLocator>& mothersIn) {
  ParticleLocator loc = getNextLoc(level);
  particles[level].push_back(
    HardProcessParticle(idIn, pdata, loc, this, mothersIn));
  return loc;
}

//-------------------------------------------------------------------------

// If a particle decays, set the list of children it decays
// into (using ParticleLocators).

void HardProcessParticleList::setDaughters(ParticleLocator& mother,
  vector<ParticleLocator>& daughters) {
  if (particles.find(mother.level) != particles.end() &&
      (int)particles[mother.level].size() > mother.pos)
    particles[mother.level].at(mother.pos).daughters = daughters;
}

//==========================================================================

// The VinciaHardProcess class methods.

//-------------------------------------------------------------------------

// Initialise process.

void VinciaHardProcess::initOnProcess(string process,
  ParticleData* particleData) {

  initLookup(particleData);
  if (verbose >= VinciaConstants::DEBUG)
    printOut(__METHOD_NAME__, "Processing raw string " + process);

  // 1) Preprocessing. Remove leading "{".
  auto splitPos =  min(process.find_first_of("{"),process.length()-1);
  process = process.substr(splitPos+1);
  // Remove trailing "}".
  splitPos =  min(process.find_last_of("}"),process.length());
  process = process.substr(0, splitPos);

  // 2) Split into incoming and outgoing.
  vector<string> inWords, outWords;
  if (!splitProcess(process, inWords, outWords)) {
    loggerPtr->ERROR_MSG("failed to split process");
    return;
  }
  if (!getParticles(particleData, inWords, outWords)) {
    loggerPtr->ERROR_MSG("failed to save hard process");
    return;
  }
  if (verbose >= Logger::NORMAL) list();
  isInit = true;

}

//-------------------------------------------------------------------------

// Hard process lookup.

void VinciaHardProcess::listLookup() const {
  cout << "\n *--------  VINCIA Hard Process Lookup ----------------------"
       << "-------------------------*\n\n Particle IDs directory:\n\n";
  for (auto it = lookupIDfromName.begin(); it != lookupIDfromName.end(); ++it)
    cout << "  " << setw(14) << std::left
         << it->first << "    (" << it->second << ")\n";
  cout << "\n *-----------------------------------------------------------"
       << "-------------------------*\n";
}

//-------------------------------------------------------------------------

// Obtain the colour structure of the hard process.

void VinciaHardProcess::getColourStructure(ColourStructure& colStructNow) {

  vector<HardProcessParticle*> colRes, uncolRes;
  vector<int> leptons(multiparticles["LEPTONS"].pidList),
    partons(multiparticles["j"].pidList);

  // Set beams.
  auto beams = parts.getBeams();
  colStructNow.beamA = beams.first;
  colStructNow.beamB = beams.second;

  // Loop over level one particles and save.
  vector<HardProcessParticle>* partsVec = parts.getLevel(1);
  if (partsVec != nullptr)
    for (auto it = partsVec->begin(); it != partsVec->end(); ++it) {
      // Is this a resonance? (or e.g. a photon with specified decay).
      if (!it->isFinal() || it->isRes()) {
        // Is it coloured or not?
        if (it->isCol()) colRes.push_back(&(*it));
        else uncolRes.push_back(&(*it));
      } else {
        // Fetch ID.
        int pid = 0;
        if (it->isMulti()) {
          const MultiParticle* multi = it->getMulti();
          // Just get representative id.
          pid = multi->pidList.at(0);
        } else pid = it->id();

        // Is it a lepton?
        if (find(leptons.begin(),leptons.end(),pid)!=leptons.end())
          colStructNow.leptons.push_back(&(*it));
        // Is it a parton?
        if (find(partons.begin(),partons.end(),pid)!=partons.end() &&
            it->isCol())
          colStructNow.coloured.push_back(&(*it));
      }
    }

  if (verbose >= VinciaConstants::DEBUG) {
    stringstream ss;
    ss << "Found "<< colRes.size() << " coloured resonances";
    printOut(__METHOD_NAME__, ss.str());
  }

  // Recursively replace coloured resonances until empty.
  while (!colRes.empty()) {

    // Copy next element from back.
    HardProcessParticle* resPtrNow = colRes.back();
    // Delete from back.
    colRes.pop_back();
    // Get decay products. Did we specify the decay?
    if (!resPtrNow->isFinal()) {
      vector<ParticleLocator> daughterLocs = resPtrNow->getDaughters();
      for (auto loc = daughterLocs.begin(); loc!=daughterLocs.end(); ++loc) {
        HardProcessParticle * daughter = parts.getPart(*loc);

        // Is this a resonance?
        if (daughter->isRes()) {
          // Is it coloured or not?
          if (daughter->isCol()) colRes.push_back(daughter);
          else uncolRes.push_back(daughter);
        // Fetch ID.
        } else if (daughter->isFinal()) {
          int pid = daughter->id();
          if (daughter->isMulti()) {
            const MultiParticle* multi = daughter->getMulti();
            // Just get representative id.
            pid = multi->pidList.at(0);
          }

          // Is it a lepton?
          if (find(leptons.begin(), leptons.end(), pid) != leptons.end())
            colStructNow.leptons.push_back(daughter);
          // Is it a parton?
          if (find(partons.begin(),partons.end(),pid)!=partons.end() &&
            daughter->isCol())
            colStructNow.coloured.push_back(daughter);
        // Just print a warning.
        } else
          loggerPtr->ERROR_MSG("cannot handle particle: " + daughter->name());
      }
    // Just pretend it is stable.
    } else colStructNow.coloured.push_back(resPtrNow);
  }

  if (verbose >= VinciaConstants::DEBUG) {
    stringstream ss;
    ss << "Found " << uncolRes.size() << " uncoloured resonances";
    printOut(__METHOD_NAME__, ss.str());
  }

  // Loop over colour-singlet resonances, and save those which decay
  // hadronically.
  for (int iRes = 0; iRes != (int)uncolRes.size(); ++iRes) {

    HardProcessParticle* resPtr = uncolRes.at(iRes);
    bool isHadronic(false), isLeptonic(false), isUndecayed(false);

    // Did we specify the decay?
    if (!resPtr->isFinal()) {
      vector<ParticleLocator> daughterLocs = resPtr->getDaughters();
      for (auto loc = daughterLocs.begin(); loc!=daughterLocs.end(); ++loc) {
        HardProcessParticle* daughter = parts.getPart(*loc);
        // Is it coloured or not?
        if (daughter->isCol()) {
          isHadronic = true; break;
        // Is this another uncoloured resonance?
        } else if (daughter->isRes()) {
          uncolRes.push_back(daughter);
        } else if (daughter->isFinal()) {
          // Fetch ID.
          int pid = daughter->id();
          if (daughter->isMulti()) {
            const MultiParticle * multi = daughter->getMulti();
            // Just get representative id.
            pid = multi->pidList.at(0);
          }

          // Is it a lepton?
          if (find(leptons.begin(),leptons.end(),pid)!=leptons.end()) {
            isLeptonic = true;
            colStructNow.leptons.push_back(daughter);
          }
        }
      }
    } else isUndecayed = true;

    if (isHadronic || isLeptonic) {
      int charge(resPtr->chargeType()), id(0);
      bool isFCN = false;
      if (resPtr->isMulti()) {
        isFCN = (resPtr->getMulti())->isFCN;
        id = (resPtr->getMulti())->pidList.at(0);
      } else {
        id = resPtr->id();
        if (isFCNres.find(id)!=isFCNres.end()) isFCN = isFCNres[id];
      }

      // Neutral.
      if (charge == 0) {
        // Flavour-changing.
        if (isFCN) {
          if (isHadronic) colStructNow.resNeutralFCHad.push_back(id);
          else if (isLeptonic) colStructNow.resNeutralFCLep.push_back(id);
        // Flavour-neutral.
        } else {
          if (isHadronic) colStructNow.resNeutralFNHad.push_back(id);
          else if (isLeptonic) colStructNow.resNeutralFNLep.push_back(id);
        }
      // Positive.
      } else if (charge > 0) {
        if (isHadronic) colStructNow.resPlusHad.push_back(id);
        else if (isLeptonic) colStructNow.resPlusLep.push_back(id);
      // Negative.
      } else {
        if (isHadronic) colStructNow.resMinusHad.push_back(id);
        else if (isLeptonic) colStructNow.resMinusLep.push_back(id);
      }
    // Done with leptonically and hadronically decays.
    } else if (isUndecayed) {
      int charge = resPtr->chargeType();
      int id = resPtr->isMulti() ?
        resPtr->getMulti()->pidList.at(0) : resPtr->id();

      // Positive.
      if (charge > 0.) colStructNow.resPlusUndecayed.push_back(id);
      // Negative.
      else if (charge < 0.) colStructNow.resMinusUndecayed.push_back(id);
      // Neutral.
      else colStructNow.resNeutralUndecayed.push_back(id);
    }
  }

  // Check beams and coloured particles: how many explicit qqbar
  // pairs, how many coloured?
  int nQ(0), nQbar(0), nG(0);
  for (int i = 0; i < int(colStructNow.coloured.size()); ++i) {
    int colType=(colStructNow.coloured.at(i))->colType();
    if (colType == 1) nQ++;
    else if (colType == -1) nQbar++;
    else if (colType == 2) nG++;
  }
  int colA =  colStructNow.beamA->colType();
  if (colA == 1) nQbar++;
  else if (colA == -1) nQ++;
  else if (colA == 2) nG++;
  int colB = colStructNow.beamB->colType();
  if (colB == 1) nQbar++;
  else if (colB == -1) nQ++;
  else if (colB == 2) nG++;

  colStructNow.nQQbarPairs = 0;
  if (nQ == nQbar) colStructNow.nQQbarPairs = nQ;
  else loggerPtr->WARNING_MSG("inconsistent number of quarks");
  colStructNow.nColoured = nQ + nQbar + nG;

  // Set minimum number of quark pairs.
  if (colStructNow.nQQbarPairs == 0) {
    // If lepton-lepton > jets, need at least 1 qqbar pair
    // (even if user did not specify).
    if (colA == 0 && colB == 0 && colStructNow.nColoured > 0)
      colStructNow.nQQbarPairs++;
    // If lepton-quark scattering, need at least 1 qqbar pair.
    else if ((colA != 0 && colB == 0) || (colB != 0 && colA == 0))
      colStructNow.nQQbarPairs++;
    // If p p > colour singlet, need at least 1 qqbar pair (except for HEFT).
    else if (colA != 0 && colB!= 0 && uncolRes.size() > 0
      && (colStructNow.nColoured - 2) == 0) {
      // If HEFT turned on, no minimum number of quark pairs required.
      if (!doHEFT) colStructNow.nQQbarPairs++;
    // If p p > colour singlet + 2 jets, check for VBF and HEFT.
    } else if (colA != 0 && colB!= 0 && uncolRes.size() > 0
      && (colStructNow.nColoured - 4) == 0) {
      // If VBF process, need two quark pairs.
      if (doVBF) colStructNow.nQQbarPairs = 2;
      // If HEFT turned on, no minimum number of quark pairs required.
      else if (!doHEFT) colStructNow.nQQbarPairs++;
    }
  }

  // What are the max and minimum number of chains associated with
  // the "beam" scattering (i.e. not from singlet decays)?
  if (colStructNow.nQQbarPairs > 0)
    colStructNow.nMinBeamChains = colStructNow.nQQbarPairs;
  else if (nG > 0) colStructNow.nMinBeamChains = 1;
  else colStructNow.nMinBeamChains = 0;
  colStructNow.nMaxBeamChains = colStructNow.nColoured / 2;
  colStructNow.nMaxBeamChains = min(2, colStructNow.nMaxBeamChains);

}

//--------------------------------------------------------------------------

// Initialised multiparticle definitions.

void VinciaHardProcess::defineMultiparticles() {

  // Create the multiparticle.
  MultiParticle mpart;
  mpart.isRes = false;
  mpart.isFCN = false;

  // Define p, pbar, n, nbar, and j.
  mpart.pidList = {1, 2 ,3, 4, 5, -1, -2, -3, -4, -5, 21};
  mpart.coltypes = {2, 1, -1};
  mpart.id = 2212;
  mpart.charge = 1;
  multiparticles["p"] = multiparticles["p+"] = mpart;
  mpart.id = -2212;
  mpart.charge = -1;
  multiparticles["pbar"] = multiparticles["p-"] = mpart;
  mpart.id = 2112;
  mpart.charge = 0;
  multiparticles["n"] = mpart;
  mpart.id = -2112;
  multiparticles["nbar"] = mpart;
  mpart.id = 0;
  mpart.charge = 999;
  multiparticles["j"] = mpart;

  // Define quarks and anti-quarks.
  mpart.pidList = {1, 2, 3, 4, 5};
  mpart.coltypes = {1};
  multiparticles["q"] = multiparticles["Q"] = multiparticles["QUARK"] = mpart;
  mpart.pidList = {-1, -2, -3, -4, -5};
  mpart.coltypes = {-1};
  multiparticles["qbar"] = multiparticles["QBAR"] =
    multiparticles["ANTIQUARK"] = mpart;

  // Define charged leptons.
  mpart.pidList = {11, -11, 13, -13, 15, -15};
  mpart.coltypes = {0};
  multiparticles["LEPTONS"] = mpart;
  mpart.pidList = {-11, -13, -15};
  mpart.charge = 1;
  multiparticles["l+"] = mpart;
  mpart.pidList = {11, 13, 15};
  mpart.charge = -1;
  multiparticles["l-"] = mpart;

  // Define neutrinos.
  mpart.charge = 0;
  mpart.pidList = {12, -12, 14, -14, 16, -16};
  multiparticles["NEUTRINOS"] = mpart;
  mpart.pidList = {12, 14, 16};
  multiparticles["nu"] = mpart;
  mpart.pidList = {-12, -14, -16};
  multiparticles["nubar"] = mpart;

  // Define gamma*/Z.
  mpart.pidList = {22, 23};
  mpart.isRes = true;
  multiparticles["gammaZ"] = mpart;

}

//-------------------------------------------------------------------------

// Check if ID may be a beam particle.

bool VinciaHardProcess::isBeamID(int id) {
  id = abs(id);
  return id == 2212 || id == 2112 || id == 11 || id == 13 ||
    id == 22 || id == 990;}

//-------------------------------------------------------------------------

// Initialise map of names to IDs.

void VinciaHardProcess::initLookup(ParticleData* particleData) {

  int next(1);
  while (next > 0) {

    // Don't include (r)hadrons or heavy ions or beam remnant.
    if (!particleData->isHadron(next) && (next < 1000500 || next > 2000000) &&
        (next < 1000 || next > 6000) && !particleData->isOnium(next) &&
        next < 9900000 ) {

      // Add this ID to lookup. Save name.
      string nameNow = particleData->name(next);
      lookupIDfromName[nameNow] = next;

      // Save antiname.
      nameNow = particleData->name(-next);
      if (nameNow != "void" && nameNow != "" && nameNow.find_first_not_of(" ")
          < nameNow.length()) lookupIDfromName[nameNow]=-next;

      // Is it a  neutral resonance?
      if (particleData->isResonance(next) &&
          particleData->chargeType(next) == 0) {

        // Is it flavour-changing (and decaying to quarks)?
        ParticleDataEntryPtr p = particleData->findParticle(next);
        int nChannels = p->sizeChannels();
        bool isFC = false;

        // Loop over all channels.
        for (int i = 0; i < nChannels; ++i) {
          DecayChannel dec = p->channel(i);
          if (dec.currentBR() > 0.) {
            if (dec.contains(1,-3)) isFC = true;
            else if (dec.contains(1,-5)) isFC = true;
            else if (dec.contains(3,-1)) isFC = true;
            else if (dec.contains(3,-5)) isFC = true;
            else if (dec.contains(5,-1)) isFC = true;
            else if (dec.contains(5,-3)) isFC = true;
            else if (dec.contains(2,-4)) isFC = true;
            else if (dec.contains(2,-6)) isFC = true;
            else if (dec.contains(4,-2)) isFC = true;
            else if (dec.contains(4,-6)) isFC = true;
            else if (dec.contains(6,-2)) isFC = true;
            else if (dec.contains(6,-4)) isFC = true;
          }
          if (isFC) break;
        }
        // Save.
        isFCNres[next] = isFC;
      }
    }
    // Get next ID.
    next=particleData->nextId(next);
  }
  if (verbose >= Logger::REPORT) listLookup();

}

//-------------------------------------------------------------------------

// Split process string into incoming, outgoing using ">".

bool VinciaHardProcess::splitProcess(string process, vector<string>& inWords,
  vector<string>& outWords) {

  if (process.empty() || process=="void") return false;
  inWords.clear();
  outWords.clear();

  // Remove leading and trailing whitespace.
  auto splitPos = min(process.find_first_not_of(" "), process.length());
  process = process.substr(splitPos);
  splitPos = min(process.find_last_not_of(" "), process.length());
  process = process.substr(0, splitPos + 1);
  if (process.empty()) return false;

  // 2) Split string into incoming and outgoing using ">" as a delimiter.
  splitPos = min(process.find_first_of(">"),process.length());
  string inString =  process.substr(0,splitPos);
  string outString = process.substr(min(splitPos+1,process.length()));

  // 3) Split incoming into beam, using whitespace as delimiter.
  splitbyWhitespace(inString, inWords);

  // 4) Split outgoing by brackets.
  vector<string> outWordsFront, outWordsBack;
  while (!outString.empty()) {
    size_t start(0), stop(outString.length()), length(outString.length());

    // Are there any brackets?
    if (outString.find("(") != string::npos ||
        outString.find(")") != string::npos ) {
      start = outString.find_first_of("(");
      stop = outString.find_last_of(")");
      if (start > outString.length() - 1 || stop == outString.length()
        || start>stop) {
        loggerPtr->ERROR_MSG("unclosed bracket");
        return false;
      }

      // Extract all characters preceding the first "(" if any.
      string word = outString.substr(0,start);
      if (!word.empty()) splitbyWhitespace(word,outWordsFront);
      // Extract all characters after the last ")" if any.
      word = outString.substr(min(stop+1,outString.length()));
      if (!word.empty()) splitbyWhitespace(word,outWordsBack,true);

      // Trim string and update start.
      outString = outString.substr(0,stop+1);
      outString = outString.substr(start);
      start = outString.find_first_of("(")+1;

      // Find innermost ")" bracket.
      auto firstClose = outString.find_first_of(")");
      // Find the preceding "(" bracket.
      auto lastOpen = outString.find_last_of("(",firstClose);
      // Count open nested brackets (at least 1).
      int nOpen = 1;
      auto open = outString.find_first_of("(");
      while (open < lastOpen) {
        open = outString.find_first_of("(",open+1);
        nOpen++;
      }
      // Now find outermost closing bracket.
      int nClose = 1;
      stop = firstClose;
      while (nClose < nOpen && stop < outString.length()) {
        stop = outString.find_first_of(")",stop+1);
        nClose++;
      }

      // Did we reach end of string before we closed all brackets?
      if (nClose != nOpen) {
        loggerPtr->ERROR_MSG("unclosed bracket");
        return false;
      }
      length = stop-start;
      stop = stop+1;
    }// No more brackets.

    // Save.
    string word=outString.substr(start,length);
    // If further structure, wait until later to resolve.
    if (word.find(">") != string::npos) outWordsFront.push_back(word);
    else splitbyWhitespace(word,outWordsFront);
    outString=outString.substr(min(stop, outString.length()));
  }
  outWords = outWordsFront;
  outWords.insert(outWords.end(),outWordsBack.begin(),outWordsBack.end());
  return true;

}

//-------------------------------------------------------------------------

// Split string into words by spaces, and append.

void VinciaHardProcess::splitbyWhitespace(string wordIn,
  vector<string>& wordsOut, bool atFront) {

  // Remove trailing whitespace.
  auto splitPos = min(wordIn.find_last_not_of(" "), wordIn.length());
  wordIn = wordIn.substr(0, splitPos + 1);

  // Remove leading whitespace.
  splitPos = min(wordIn.find_first_not_of(" "), wordIn.length());
  wordIn = wordIn.substr(splitPos);

  // Split each word using whitespace.
  while (!wordIn.empty()) {
    // Extract all characters preceding the first whitespace.
    splitPos = min(wordIn.find_first_of(" "), wordIn.length());
    string word = wordIn.substr(0,splitPos);
    if (atFront) wordsOut.insert(wordsOut.begin(), word);
    else wordsOut.push_back(word);

    // Remove word from string.
    wordIn = wordIn.substr(splitPos);

    // Remove leading whitespace.
    splitPos =  min(wordIn.find_first_not_of(" "), wordIn.length());
    wordIn = wordIn.substr(splitPos);
  }

}

//-------------------------------------------------------------------------

// Set the list of particles in the hard process.

bool VinciaHardProcess::getParticles(ParticleData* particleDataPtr,
  vector<string> inWords, vector<string> outWords) {
  vector<ParticleLocator> empty, empty2;
  return getParticles(particleDataPtr, inWords, outWords, 0, empty, empty2);
}

//-------------------------------------------------------------------------

// Recursive version (if decays are found).

// Having split the process into an initial set of incoming and
// outoing words, this function will set up the particles in the map
// parts.  If resonance decays are specified using ">", it will call
// itself recursively (incrementing the level of the map) until all
// decays have been resolved.  If a particle name cannot be
// interpreted it will return false, otherwise it will return true.

bool VinciaHardProcess::getParticles(ParticleData* particleDataPtr,
  vector<string> inWords, vector<string> outWords, int levelNow,
  vector<ParticleLocator>& mothersIn, vector<ParticleLocator>& mothersNow) {
  if (levelNow == 0 && inWords.size() != 2) {
    loggerPtr->ERROR_MSG("expect exactly two beam particles");
    return false;
  } else if (levelNow > 0 && inWords.size() != 1) {
    loggerPtr->ERROR_MSG("please specify resonances one at a time");
    return false;
  }

  // Store incoming.
  mothersNow.clear();
  vector<ParticleLocator> daughters;
  ParticleLocator latestLoc;
  for (unsigned int iWord = 0; iWord < inWords.size(); ++iWord) {
    if (!addParticle(particleDataPtr, levelNow, true,
        inWords.at(iWord), mothersIn, latestLoc)) return false;
    // These mothers are the daughters of the previous level.
    mothersNow.push_back(latestLoc);
  }

  // Check if we should resolve decays.
  int levelNext = levelNow + 1;
  if (levelNext > 1 && !resolveDecays) {
    loggerPtr->WARNING_MSG("ignoring resonance decay information");
    // Exit early.
    return true;
  }
  for (int iWord = 0; iWord < (int)outWords.size(); ++iWord) {
    string wordNow = outWords.at(iWord);
    // Does this word contain ">" ?
    if (wordNow.find(">") == string::npos) {
      if (!addParticle(particleDataPtr, levelNext, false,
          wordNow, mothersNow, latestLoc)) return false;
      daughters.push_back(latestLoc);
    } else {
      // There is a resonance decay here, so need to split up further.
      vector<string> resWords, decWords;
      if (!splitProcess(wordNow,resWords,decWords)) return false;

      // Now recurse.
      vector<ParticleLocator> decayMothers;
      if (!getParticles(particleDataPtr, resWords, decWords,
          levelNext, mothersNow, decayMothers)) return false;
      daughters.insert(daughters.end(), decayMothers.begin(),
        decayMothers.end());
    }
  }

  // If this is a resonance decay, check how many daughters we found.
  if (levelNext > 1 && daughters.size() != 2 ) {
    loggerPtr->ERROR_MSG("resonances should decay to exactly two particles");
    return false;
  }

  // Now just need to set the daughters of the current level mothers.
  for (unsigned int imother=0; imother<mothersNow.size(); ++imother)
    setDaughters(mothersNow.at(imother), daughters);
  // Done!
  return true;

}

//-------------------------------------------------------------------------

// Add a particle to list, and save location in loc if successful.

bool VinciaHardProcess::addParticle(ParticleData* particleDataPtr, int level,
  bool isIncoming, string name,vector<ParticleLocator>& mothersIn,
  ParticleLocator& loc) {

  bool isMulti(false), isRes(false);
  MultiParticle* multiPtr(nullptr);
  ParticleDataEntryPtr pData(nullptr);
  int pid(0);
  if (multiparticles.find(name) != multiparticles.end()) {
    isMulti = true;
    multiPtr = &(multiparticles[name]);
    pid = multiPtr->id;
    isRes = multiPtr->isRes;
  // Lookup in data.
  } else if (lookupIDfromName.find(name) != lookupIDfromName.end()) {
    pid = lookupIDfromName[name];
    pData = particleDataPtr->findParticle(pid);
    if (pData == nullptr) {
      loggerPtr->ERROR_MSG("mismatch between particle database and "
        "VinciaHardProcess database");
      return false;
    }
    isRes = pData->isResonance();
  } else {
    loggerPtr->ERROR_MSG("particle '" + name
        + "' not found in database");
    return false;
  }

  if (isIncoming) {
    // Check if incoming is a beam particle.
    if (level == 0 && !isBeamID(pid)) {
      loggerPtr->ERROR_MSG("particle '" + name
        + "' is not an allowed beam particle");
      return false;
    // Otherwise it should be a resonance.
    } else if (level > 0 && !isRes) {
      loggerPtr->ERROR_MSG("particle '" + name
        + "' is not a known resonance");
      return false;
    }
  }

  // Checks complete, now save.
  if (isMulti) loc = parts.add(level, name, multiPtr, mothersIn);
  else loc = parts.add(level, pid, pData, mothersIn);
  return true;

}

//==========================================================================

// The VinciaMergingHooks class.

//-------------------------------------------------------------------------

// Initialise.

void VinciaMergingHooks::init() {

  // Safety check.
  if (settingsPtr->mode("PartonShowers:model") != 2) {
    loggerPtr->WARNING_MSG(
      "do not use VinciaMergingHooks without setting PartonShowers:model = 2");
    return;
  }

  // Extract settings.
  verbose = mode("Vincia:verbose");

  // Showers on/off.
  bool doFSR = settingsPtr->flag("PartonLevel:FSR");
  bool doISR = settingsPtr->flag("PartonLevel:ISR");
  doFF = doFSR && settingsPtr->flag("Vincia:doFF");
  doII = doISR && settingsPtr->flag("Vincia:doII");
  doIF = doISR && settingsPtr->flag("Vincia:doIF");
  doRF = doFSR && settingsPtr->flag("Vincia:doRF");

  // Merging settings.
  processSave           = word("Merging:Process");
  nJetMaxSave           = mode("Merging:nJetMax");
  nQuarksMergeSave      = mode("Merging:nQuarksMerge");
  includeWGTinXSECSave  = flag("Merging:includeWeightInXsection");

  // Save merging scale definition and associated parameters.
  doDynamicMergingSave  = flag("Merging:doDynamicMerging");
  doCutBasedMergingSave = flag("Merging:doCutBasedMerging");
  doPTLundMergingSave   = flag("Merging:doMerging");
  doKTMergingSave       = flag("Merging:doKTMerging");
  doMGMergingSave       = flag("Merging:doMGMerging");
  if (doCutBasedMergingSave)
    tmsListSave = {parm("Merging:dRijMS"),
      parm("Merging:pTiMS"),
      parm("Merging:QijMS")};
  else tmsValueSave     = parm("Merging:TMS");
  if (doKTMergingSave || doMGMergingSave) {
    DparameterSave      = parm("Merging:Dparameter");
    ktTypeSave          = mode("Merging:ktType");
  }
  if (doDynamicMergingSave) SparameterSave = parm("Merging:Sparameter");

  // Special settings for merging in resonance systems.
  doMergeRes            = flag("Vincia:MergeInResSystems");
  nJetMaxResSave        = doMergeRes ? mode("Vincia:MergeNJetMaxRes") : 0;
  nMergeResSys          = doMergeRes ? mode("Vincia:MergeNResSys") : 0;

  // Special settings for merging in pure VBF or HEFT events.
  doHEFT                = flag("Vincia:MergeHEFT");
  doVBF                 = flag("Vincia:MergeVBF");

  // Settings for UMESS merging (CKKW-L by default).
  doUMEPSTreeSave       = flag("Merging:doUMEPSTree");
  doUMEPSSubtSave       = flag("Merging:doUMEPSSubt");
  if (doUMEPSTreeSave && doUMEPSSubtSave) {
    loggerPtr->ERROR_MSG("conflicting UMEPS settings");
    return;
  }

  // Sanity checks.
  if (nJetMaxSave == 0 && ( nJetMaxResSave == 0 || nMergeResSys == 0)) {
    loggerPtr->ERROR_MSG(
      "no additional jets were requested, set Merging:nJetMax or "
      "Vincia:MergeNJetMaxRes with Vincia:MergeNResSys = on");
    return;
  }
  if (processSave == "void" || processSave == "") {
    loggerPtr->ERROR_MSG("process string not set");
    return;
  }

  // TODO: for now FF splitter map = 2 is not implemented.
  if (mode("Vincia:kineMapFFsplit") != 1) {
    loggerPtr->WARNING_MSG("forcing kineMapFFsplit = 1; "
      "others not yet supported by merging");
    settingsPtr->readString("Vincia:kineMapFFsplit = 1");
  }

  // TODO: for now can't do merging for polarised.
  bool helicityShower = flag("Vincia:helicityShower");
  if (helicityShower) {
    loggerPtr->ERROR_MSG("currently merging is not available for "
      "helicity showers","set Vincia:helicityShower = off to do merging");
    return;
  }

  // Information on renormalisation scale variations.
  // TODO: not yet implemented in Vincia merging.
  doVariations = false;
  nWgts = 1;
  if (doVariations) {
    muRVarFactors =
      infoPtr->weightContainerPtr->weightsMerging.getMuRVarFactors();
    doVariations = muRVarFactors.size() != 0;
    nWgts += muRVarFactors.size();
  }
  if (doVariations) {
    loggerPtr->WARNING_MSG("automated renormalisation scale variations"
      " are not yet supported by Vincia");
    muRVarFactors.clear();
    nWgts = 1;
  }
  // Initialise CKKWL weight.
  weightCKKWLSave = vector<double>(nWgts, 1.);
  weightFIRSTSave = vector<double>(nWgts, 0.);
  nMinMPISave = 100;
  muMISave = -1.;

  // Initialize merging weights in weight container.
  vector<string> weightNames = {"MUR1.0_MUF1.0"};
  for (double fact: muRVarFactors)
    weightNames.push_back("MUR"+ to_string(fact) + "_MUF1.0");
  infoPtr->weightContainerPtr->weightsMerging.bookVectors(
      weightCKKWLSave, weightFIRSTSave, weightNames);

  // Other members to be initialised.
  tmsValueNow           = tmsValueSave;
  nJetMaxLocal          = nJetMaxSave;
  hasJetMaxLocal        = false;
  doVetoNotInResSav     = false;
  // Do not allow events to be vetoed by parton level.
  doIgnoreStepSave      = true;
  // Do not allow  emissions to be removed by merging hooks.
  doIgnoreEmissionsSave = true;
  doRemoveDecayProducts = !doMergeRes;

  // Initialise hard process.
  vinHardProcessPtr = new VinciaHardProcess(loggerPtr, verbose, doMergeRes,
    doHEFT, doVBF);
  hardProcess  = vinHardProcessPtr;
  hardProcess->initOnProcess(processSave,particleDataPtr);

  // Extract the colour structure of the hard process
  // - the main thing we actually care about!
  if (!setColourStructure()) {
    loggerPtr->ERROR_MSG("colour structure of hard process could not "
      "be initialised");
    return;
  }

  // Initialise input event.
  inputEvent.init("(hard process)", particleDataPtr);

  // Successfully completed initialisation.
  isInit = true;

}

//-------------------------------------------------------------------------

// Set starting scales.

bool VinciaMergingHooks::setShowerStartingScales(bool isTrial, bool,
  double& pTscaleIn, const Event& event, double& pTmaxFSRIn, bool&,
  double& pTmaxISRIn, bool&, double& pTmaxMPIIn, bool&) {
  if (isTrial) {
    double scale = event.scale();
    pTscaleIn    = scale;
    pTmaxFSRIn   = scale;
    pTmaxISRIn   = scale;
    pTmaxMPIIn   = scale;
  }
  return true;
}

//-------------------------------------------------------------------------

// Calculate merging scale according to scale definition.

double VinciaMergingHooks::tmsNow(const Event& event) {
  // Merging according to a cut in kT.
  if (doKTMergingSave || doMGMergingSave) return kTmin(event);
  // In the other cases, merging scale is in terms of evolution variable.
  return pTlast(event);
}

//-------------------------------------------------------------------------

// Check if event should be vetoed due to branching above merging scale.

bool VinciaMergingHooks::doVetoStep(const Event&, const Event& event, bool) {
  // Check whether we could in principle veto the event due to this branching.
  // If so, veto if the event is above the merging scale.
  bool doVeto = doIgnoreStepSave ? false : isAboveMS(event);

  if (verbose >= VinciaConstants::DEBUG) {
    stringstream ss;
    ss << "event " << (doVeto ? "vetoed" : "not vetoed")
       << (doIgnoreStepSave ? " (ignored step)" : "");
    printOut(__METHOD_NAME__, ss.str());
  }

  // If we veto the event, update weights.
  if (doVeto) {
    if (!includeWGTinXSEC()) setWeightCKKWL(vector<double>(nWgts, 0.));
    else infoPtr->weightContainerPtr->setWeightNominal(0.);
  }
  return doVeto;
}

//-------------------------------------------------------------------------

// Check if branching should be vetoed because it is above the merging scale.

bool VinciaMergingHooks::doVetoEmission(const Event& event) {
  // Check whether we should veto this branching.
  // If so, veto if the branching is above the merging scale.
  bool doVeto = doIgnoreEmissionsSave ? false : isAboveMS(event);
  if (verbose >= DEBUG) {
    stringstream ss;
    ss << "branching " << (doVeto ? "vetoed" : "not vetoed")
       << (doIgnoreEmissionsSave ? " (ignored emission)" : "");
    printOut(__METHOD_NAME__, ss.str());
  }

  return doVeto;
}

//-------------------------------------------------------------------------

// Check if an event is above the merging scale.

bool VinciaMergingHooks::isAboveMS(const Event& event) {
  // Merging according to cuts.
  if (doCutBasedMergingSave) {
    // Fetch cuts in event. Order is pT, DeltaRjj, Qjj.
    vector<double> cutsEvt = cutsMin(event);

    // Fetch minimal values.
    double pTmin       = pTiMS();
    double QjjMin      = QijMS();
    double deltaRjjMin = dRijMS();

    // Check cuts.
    double pTevt       = cutsEvt.at(0);
    // If we have only one jet, we disregard the two-particle cuts.
    if (cutsEvt.size() == 1) return (pTevt > pTmin);
    double QjjEvt      = cutsEvt.at(1);
    double deltaRjjEvt = cutsEvt.at(2);
    return (QjjEvt > QjjMin && deltaRjjEvt > deltaRjjMin);
  }

  // Otherwise fetch current scale and merging scale.
  double tNow = tmsNow(event);
  double tMS  = tmsCut();
  if (verbose >= VinciaConstants::DEBUG) {
    stringstream ss;
    ss << "tNow = " << tNow << " and tMS = " << tMS;
    printOut(__METHOD_NAME__,ss.str());
  }

  // Check against merging scale.
  return tNow>tMS;

}

//-------------------------------------------------------------------------

// Find Vincia pT of the last shower emission.
// Note: this assumes that the event already contains an emission
// generated by the shower. This is sufficient, as this should only be
// called in the veto step of the main shower.

double VinciaMergingHooks::pTlast(const Event& event) {
  // Assume FSR.
  int ii = event.size() - 3;
  int ij = event.size() - 2;
  int ik = event.size() - 1;
  // Test if ISR.
  if ( (event[ik].status() != 51 && event[ik].status() != 52) ||
    event[ij].status() != 51 || event[ii].status() != 51) {
    ii = -1;
    ij = -1;
    ik = -1;
    for (int i(event.size()-1); i > 0; --i) {
      if      (ii == -1 && event[i].status() == -41) ii = i;
      else if (ij == -1 && event[i].status() ==  43) ij = i;
      else if (ik == -1
        && (event[i].status() == -41 || event[i].status() == 44)) ik = i;
      if (ii != -1 && ij != -1 && ik != -1) break;
    }
  }
  // Check if we found an emission.
  if (ii < 0 || ij < 0 || ik < 0) {
    loggerPtr->ERROR_MSG("no branching found");
    return -1.;
  }
  // Otherwise return Vincia pT evolution scale.
  return pTvincia(event, ii, ij, ik);
}

//-------------------------------------------------------------------------

// Calculate Vincia's pT evolution scale. Based on PowhegHooks method.

double VinciaMergingHooks::pTvincia(const Event& event,
  int i1, int i3, int i2) {

  // Sanity check.
  if (i1 < 0 || i2 < 0 || i3 < 0) {
    loggerPtr->ERROR_MSG("invalid event indices");
    return -1.;
  }

  // Shorthands.
  Vec4 p1 = event[i1].p();
  Vec4 p3 = event[i3].p();
  Vec4 p2 = event[i2].p();

  // Fetch mothers of 1 and 2.
  int iMoth1 = event[i1].mother1();
  int iMoth2 = event[i2].mother1();
  if (iMoth1 == 0 || iMoth2 == 0) {
    loggerPtr->ERROR_MSG("mothers of particles not found");
    return 0.;
  }

  // Invariants defined as in Eq. (5) in arXiv:2008.09468.
  double mMoth1Sq = event[iMoth1].m2();
  double mMoth2Sq = event[iMoth2].m2();
  double sgn1 = event[i1].isFinal() ? 1. : -1.;
  double sgn2 = event[i2].isFinal() ? 1. : -1.;
  double qSq13 = sgn1*(m2(sgn1*p1+p3) - mMoth1Sq);
  double qSq23 = sgn2*(m2(sgn2*p2+p3) - mMoth2Sq);

  // Normalisation as in Eq. (6) in arXiv:2008.09468.
  double sMax = -1.;
  if (event[i1].isFinal() && event[i2].isFinal()) {
    // FF.
    sMax = m2(p1+p2+p3) - mMoth1Sq - mMoth2Sq;
  } else if ((event[i1].isResonance() && event[i2].isFinal())
    || (!event[i1].isFinal() && event[i2].isFinal())) {
    // RF or IF.
    sMax = 2.*p1*p3 + 2.*p1*p2;
  } else if ((event[i1].isFinal() && event[i2].isResonance())
    || (event[i1].isFinal() && !event[i2].isFinal())) {
    // FR or FI.
    sMax = 2.*p2*p3 + 2.*p1*p2;
  } else if (!event[i1].isFinal() || !event[i2].isFinal()) {
    // II.
    sMax = 2.*p1*p2;
  }

  // Calculate pT2 as in Eq. (5) in arXiv:2008.09468.
  double pT2now = qSq13*qSq23/sMax;

  // Sanity check.
  if (pT2now < 0.) {
    loggerPtr->ERROR_MSG("negative pT");
    return 0.;
  }

  // Return pT.
  return sqrt(pT2now);
}

//-------------------------------------------------------------------------

// Minimal kT in (hard) event.

double VinciaMergingHooks::kTmin(const Event& event) {

  // Fetch indices of jets in event.
  vector<int> jets = getJetsInEvent(event);

  // From here mainly a copy of MergingHooks::kTms().
  int kTtype = (event[3].colType() == 0 && event[4].colType() == 0) ?
    -1 : ktTypeSave;
  // Find minimal kT.
  double kT = event[0].e();
  for (int iJet1 = 0; iJet1 < (int)jets.size(); ++iJet1) {
    double kt12 = kT;
    // For hadronic collisions, compute separation to the beam axis.
    if (kTtype == 1 || kTtype == 2)
      kt12 = min(kt12, event[jets.at(iJet1)].pT());
    // Compute separation to other jets.
    for (int iJet2 = iJet1 + 1; iJet2 < (int)jets.size(); ++iJet2)
      kt12 = min(kt12, kTdurham(event[jets[iJet1]], event[jets[iJet2]],
        kTtype, DparameterSave));
    // Keep the minimal Durham separation.
    kT = min(kT, kt12);
  }
  return kT;

}

//-------------------------------------------------------------------------

// Minimal cuts in (hard) event.

vector<double> VinciaMergingHooks::cutsMin(const Event& event) {
  // Fetch indices of jets in event.
  vector<int> jets = getJetsInEvent(event);

  // Initialise.
  vector<double> cuts;
  double pTmin       = event[0].e();
  double deltaRjjMin = 10.;
  double QjjMin      = event[0].e();

  // If we have only one jet, disregard two-particle cuts.
  if (jets.size() == 1) {
    cuts.push_back(event[jets[0]].pT());
    return cuts;
  }

  // Mainly copy of MergingHooks::cutbasedms().
  for (int iJet1 = 0; iJet1 < (int)jets.size(); ++iJet1) {
    // Save pT.
    pTmin = min(pTmin, event[jets[iJet1]].pT());

    // Check two-parton cuts.
    for (int iJet2 = iJet1; iJet2 < (int)jets.size(); ++iJet2) {
      // Save deltaRjj.
      deltaRjjMin = min(deltaRjjMin,
        deltaRij(event[jets[iJet1]].p(), event[jets[iJet2]].p()));
      // Save mass.
      QjjMin = min(QjjMin,
        (event[jets[iJet1]].p() + event[jets[iJet2]].p()).mCalc());
    }
  }
  cuts.push_back(pTmin);
  cuts.push_back(deltaRjjMin);
  cuts.push_back(QjjMin);
  return cuts;

}

//-------------------------------------------------------------------------

// Get maximal number of clustering steps (assuming no incomplete history).

int VinciaMergingHooks::getNumberOfClusteringSteps(const Event& state, bool) {
  int nStepsMax(0);
  for (const Particle& ptcl : state)
    if (ptcl.isQuark() || ptcl.isGluon()) nStepsMax += 1;
  nStepsMax -= getNPartons();
  return nStepsMax;
}

//-------------------------------------------------------------------------

// Fetch the colour structure of the hard process.

ColourStructure VinciaMergingHooks::getColourStructure() {
  if (hasColStruct) return colStructSav;
  else if (vinHardProcessPtr != nullptr) {
    vinHardProcessPtr->getColourStructure(colStructSav);
    hasColStruct = true;
    return colStructSav;
  } else {
    loggerPtr->ERROR_MSG("hard process pointer is null");
    return ColourStructure();
  }
}

//-------------------------------------------------------------------------

// Find the colour structure of the hard process.

bool VinciaMergingHooks::setColourStructure() {
  hasColStruct = false;
  if (hardProcess != nullptr) {
    if (!vinHardProcessPtr->initSuccess()) return false;
    vinHardProcessPtr->getColourStructure(colStructSav);
    if (getNResHad() != nMergeResSys) {
      loggerPtr->ERROR_MSG("mismatch in settings Vincia:MergeNJetMaxRes "
        "and Merging:Process");
      return false;
    }
    if (getNResHad() == 0 && getNChainsMax() == 0) {
      loggerPtr->ERROR_MSG("no colour in specified Merging:Process");
      return false;
    }
    hasColStruct = true;
    if (verbose >= Logger::NORMAL) printColStruct();
  } else loggerPtr->ERROR_MSG("hard process pointer is null");
  return hasColStruct;
}

//-------------------------------------------------------------------------

// Print the colour structure.

void VinciaMergingHooks::printColStruct() {

  cout << "\n *--------  Colour Structure Summary -------------------------"
       <<"------------------------*\n"
       <<"   Number of colour chains from beam scattering: "
       << colStructSav.nMinBeamChains
       <<" <= n <= "<< colStructSav.nMaxBeamChains << "\n"
       <<"   Number of colour chains from neutral (FN) uncoloured resonances: "
       << colStructSav.resNeutralFNHad.size() << "\n"
       <<"   Number of colour chains from neutral (FC) uncoloured resonances: "
       << colStructSav.resNeutralFNHad.size() << "\n"
       <<"   Number of colour chains from positive uncoloured resonances: "
       << colStructSav.resPlusHad.size() << "\n"
       <<"   Number of colour chains from negative uncoloured resonances: "
       << colStructSav.resMinusHad.size() << "\n\n"
       << " *---------------------------------------------------------------"
       <<"---------------------*\n";
}

//-------------------------------------------------------------------------

// Check whether a particle is a resonance decay product.

bool VinciaMergingHooks::isResDecayProd(int iPtcl, const Event& event) {

  // Fetch mothers.
  int iMother1(event[iPtcl].mother1()), iMother2(event[iPtcl].mother2());

  // Check if one of them is a resonance.
  bool motherIsRes = false;
  if (iMother1 != 0) motherIsRes = event[iMother1].isResonance();
  if (!motherIsRes && iMother2 != 0)
    motherIsRes = event[iMother2].isResonance();
  return motherIsRes;

}

//-------------------------------------------------------------------------

// Get indices of all jets in event, according to a specific jet definition.

vector<int> VinciaMergingHooks::getJetsInEvent(const Event& event) {

  // Loop through event record.
  vector<int> jets;
  for (int iPtcl = 3; iPtcl < event.size(); ++iPtcl) {
    // Only consider final-state particles.
    if (!event[iPtcl].isFinal() || event[iPtcl].colType() == 0) continue;
    // Only consider particles in hard system.
    if (!isInHard(iPtcl, event)) continue;
    // Only consider gluons and light quarks (defined by nQuarksMergeSave).
    if (!checkAgainstCut(event[iPtcl])) continue;
    // Only consider resonance decay products if we merge in resonance systems.
    if (!doMergeRes && isResDecayProd(iPtcl, event)) continue;
    // Save particle as jet.
    jets.push_back(iPtcl);
  }
  return jets;

}

//==========================================================================

} // end namespace Pythia8
