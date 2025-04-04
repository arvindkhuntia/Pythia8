// Weights.cc is a part of the PYTHIA event generator.
// Copyright (C) 2025 Torbjorn Sjostrand.
// PYTHIA is licenced under the GNU GPL v2 or later, see COPYING for details.
// Please respect the MCnet Guidelines, see GUIDELINES for details.

// Function definitions (not found in the header) for the Weight classes.

#include "Pythia8/FragmentationFlavZpT.h"
#include "Pythia8/Info.h"
#include "Pythia8/Settings.h"
#include "Pythia8/Weights.h"

namespace Pythia8 {

//==========================================================================

// WeightsBase class.

//--------------------------------------------------------------------------

// Function to return processed weights to weight container, e.g. if
// weights should be combined before proceeding.

void WeightsBase::collectWeightValues(vector<double>& outputWeights,
  double norm) {
  for (int iWgt = 1; iWgt < getWeightsSize(); ++iWgt)
    outputWeights.push_back(norm*getWeightsValue(iWgt));
}

//--------------------------------------------------------------------------

// Similar function to return processed weight names.

void WeightsBase::collectWeightNames(vector<string>& outputNames) {
  for (int iWgt = 1; iWgt < getWeightsSize(); ++iWgt)
    outputNames.push_back(getWeightsName(iWgt));
}

//--------------------------------------------------------------------------

// Parse a WVec of variations into a weight group map.

void WeightsBase::parse(string wvecKey,
  map<string, map<string, double> > &dct) {
  for (string &line : infoPtr->settingsPtr->wvec(wvecKey)) {

    // Skip blank lines and remove leading spaces.
    line = toLower(line);
    if (line.empty()) continue;

    // Separate line by spaces, and read key/value pairs.
    string group, word;
    stringstream sline(line);
    while (getline(sline, word, ' ')) {
      if (group.empty()) {group = toLower(word); continue;}
      size_t token = word.find("=");
      if (token == string::npos) continue;
      string val = toLower(word.substr(token + 1));
      dct[group][toLower(word.substr(0, token))] = stod(val);
    }
  }
}

//==========================================================================

// WeightsSimpleShower class.

//--------------------------------------------------------------------------

// Initialize shower weights.

void WeightsSimpleShower::init(bool doMerging ) {

  // Call base initialization.
  WeightsBase::init();
  mergingVarNames.resize(0);

  // Force shower variations if needed by merging but not requested by user
  if (!infoPtr->settingsPtr->flag("UncertaintyBands:doVariations") &&
      infoPtr->weightContainerPtr->weightsMerging.getMuRVarFactors().size()
      && doMerging ) {
    infoPtr->settingsPtr->flag("UncertaintyBands:doVariations", true);
    // In this case, also empty requested variations, to not do default ones
    infoPtr->settingsPtr->wvec("UncertaintyBands:List",vector<string>(0));
  }

  // Assemble shower variation strings needed for merging
  if (doMerging)
    for (double fac: infoPtr->weightContainerPtr->weightsMerging.
        getMuRVarFactors()) {
          string stringfsr = "fsr:murfac=" + to_string(fac);
          string stringisr = "isr:murfac=" + to_string(fac);
          mergingVarNames.push_back({stringfsr,stringisr});
    }

}

//--------------------------------------------------------------------------

// Uncertainty variations initialization.

void WeightsSimpleShower::initWeightGroups(bool isISR) {
  vector<string> variationListIn = infoPtr->settingsPtr->
    wvec("UncertaintyBands:List");
  size_t vNames = weightNames.size();
  externalVariations.clear();
  externalVarNames.clear();
  externalGroupNames.clear();
  externalMap.clear();
  initialNameSave.clear();
  externalVariations.push_back("Baseline");
  initialNameSave.push_back("Baseline");
  for(vector<string>::const_iterator v=variationListIn.begin();
      v != variationListIn.end(); ++v) {
    string line = *v;
    // Remove initial blank spaces
    while (line.find(" ") == 0) line.erase( 0, 1);
    size_t pos=0;
    // Search for pdf:family keyword for SpaceShower
    if( isISR && ((pos = line.find("isr:pdf:family")) != string::npos) ) {
      size_t posEnd = line.find(" ",pos);
      if( posEnd == string::npos ) posEnd = line.size();
      for(size_t i=0; i < vNames; ++i) {
        string local = weightNames[i];
        if( local.find("isr:pdf:member") != string::npos ) {
          size_t iEqual = local.find("=")+1;
          string nMember = local.substr(iEqual,local.size());
          nMember.append(" ");
          string tmpLine = line;
          tmpLine.replace(pos,posEnd-pos,local);
          size_t iBlank = line.find_first_of(" ");
          tmpLine.replace(iBlank,1,nMember);
          externalVariations.push_back(tmpLine);
          initialNameSave.push_back(line.substr(0,line.find_first_of(" ")));
        }
      }
    } else {
      externalVariations.push_back(line);
      initialNameSave.push_back(line.substr(0,line.find_first_of(" ")));
    }
  }
  externalVariationsSize = externalVariations.size();
  size_t nNames = externalVariationsSize;
  externalVarNames.resize(nNames);
  externalVarNames[0].push_back("Baseline");
  externalGroupNames.resize(nNames);
  externalGroupNames[0]="Baseline";
  for(size_t iWeight = 0; iWeight < nNames; ++iWeight) {
    string uVarString = toLower(externalVariations[iWeight]);
    size_t firstBlank  = uVarString.find_first_of(" ");
    size_t endLine = uVarString.size();
    if( firstBlank > endLine) continue;
    externalGroupNames[iWeight] = uVarString.substr(0,firstBlank);
    uVarString  = uVarString.substr(firstBlank+1,endLine);
    size_t pos = 0;
    while ((pos = uVarString.find(" ")) != string::npos) {
      string token = uVarString.substr(0, pos);
      externalVarNames[iWeight].push_back(token);
      uVarString.erase(0, pos + 1);
    }
    if (uVarString == "" || uVarString == " ") continue;
    externalVarNames[iWeight].push_back(uVarString);
  }
  externalMap.resize(nNames);
  for(size_t iWeight = 0; iWeight < vNames; ++iWeight) {
    for(size_t iV = 0; iV < nNames; ++iV) {
      for(size_t iW = 0; iW < externalVarNames[iV].size(); ++iW) {
        if( externalVarNames[iV][iW] == weightNames[iWeight] ) {
          externalMap[iV].push_back(iWeight);
        } else if ( isISR && externalVarNames[iV][iW].find("isr:pdf:family")
        != string::npos && weightNames[iWeight].find("isr:pdf:member")
        != string::npos ) {
          externalMap[iV].push_back(iWeight);
        }
      }
    }
  }
}

//--------------------------------------------------------------------------

// Return weight group name.

string WeightsSimpleShower::getGroupName(int iGN) const {
  if (iGN < 0 || iGN >= externalVariationsSize) return "Null";
  return externalGroupNames[iGN];
}

//--------------------------------------------------------------------------

// Return weight group value.

double WeightsSimpleShower::getGroupWeight(int iGW) const {
  double tempWeight(1.0);
  if( iGW < 0 || iGW >= externalVariationsSize )
    return tempWeight;
  for( vector<int>::const_iterator cit = externalMap[iGW].
      begin(); cit < externalMap[iGW].end(); ++cit )
    tempWeight *= getWeightsValue(*cit);
  return tempWeight;
}

//--------------------------------------------------------------------------

// Initialise list of atomic uncertainty variations to be done by shower
// from list specified by uVars.

bool WeightsSimpleShower::initUniqueShowerVars() {

  // Reset saved list of uncertainty variations.
  uniqueShowerVars.clear();

  // Parse each string in uVars list to look for recognized keywords.
  vector<string> uVarsListIn = infoPtr->settingsPtr->
    wvec("UncertaintyBands:List");
  size_t size = uVarsListIn.size();
  for (size_t iWeight = 0; iWeight < size; ++iWeight) {
    // Convert to lowercase (to be case-insensitive). Also remove
    // extra spaces, so everything is mapped to "key=value"
    string uVarString = toLower(uVarsListIn[iWeight]);
    while (uVarString.find(" ") == 0) uVarString.erase( 0, 1);
    int iEnd = uVarString.find(" ", 0);
    uVarString.erase(0,iEnd+1);
    while (uVarString.find("=") != string::npos) {
      iEnd = uVarString.find_first_of(" ", 0);
      if( iEnd<0 ) iEnd = uVarString.length();
      string insertString = uVarString.substr(0,iEnd);
      if( uniqueShowerVars.size() == 0 ) {
        uniqueShowerVars.push_back(insertString);
      } else if ( find(uniqueShowerVars.begin(), uniqueShowerVars.end(),
          insertString) == uniqueShowerVars.end() ) {
        uniqueShowerVars.push_back(insertString);
      }
      uVarString.erase(0,iEnd+1);
    }
  }

  // Also attach weights needed for merging
  for (vector<string> mergingVariation: mergingVarNames) {
    for (string varString: mergingVariation) {
      uniqueShowerVars.push_back(varString);
    }
  }

  if (uniqueShowerVars.size() == 0) return false;
  else return true;
}

//--------------------------------------------------------------------------

// Extract those variations that match a specified list of keys.

vector<string> WeightsSimpleShower::getUniqueShowerVars(vector<string> keys) {

  // Define return vector and return immediately if nothing to do.
  vector<string> uniqueVarsNow;
  size_t sizeAll = uniqueShowerVars.size();
  if (keys.size() == 0 || sizeAll == 0) return uniqueVarsNow;

  // Parse each string in uniqueShowerVars to look for recognized keywords.
  for (string uVarString: uniqueShowerVars) {
    int firstEqual = uVarString.find_first_of("=");
    string testString = uVarString.substr(0, firstEqual);
    // does the key match an fsr one?
    if( find(keys.begin(), keys.end(), testString) != keys.end() ) {
      if( uniqueVarsNow.size() == 0 ) {
        uniqueVarsNow.push_back(uVarString);
      } else if ( find(uniqueVarsNow.begin(), uniqueVarsNow.end(), uVarString)
      == uniqueVarsNow.end() ) {
        uniqueVarsNow.push_back(uVarString);
      }
    }
  }
  return uniqueVarsNow;
}

//--------------------------------------------------------------------------

// Initialise list of enhancement factors to be applied to shower trials.

bool WeightsSimpleShower::initEnhanceFactors() {

  // Get uncertainty variations from Settings (as list of strings to parse).
  vector<string> listIn = infoPtr->settingsPtr->
    wvec("EnhancedSplittings:List");
  if (listIn.size() == 0) return false;

  string delim = "=";
  // Parse each string in list to look for recognized keywords.
  for ( auto &word : listIn ) {
    auto end = word.find(delim);
    string name = word.substr(0,end);
    string value = word.substr(end+1);
    name.erase(remove_if(name.begin(),name.end(),isspace),name.end());
    value.erase(remove_if(value.begin(),value.end(),isspace),value.end());
    double rvalue;
    istringstream iss(value);
    iss >> rvalue;
    pair<string,double> t(name,rvalue);
    enhanceFactors.insert(t);
  }
  return true;
}

//--------------------------------------------------------------------------

// Get vector of muR weight combinations (isr & fsr) needed by merging
vector<double> WeightsSimpleShower::getMuRWeightVector() {
  int nVarCombs = mergingVarNames.size();
  vector<double> ret( nVarCombs, 1. );
  for (int iVarComb = 0; iVarComb < nVarCombs; ++iVarComb) {
    int nVars = mergingVarNames[iVarComb].size();
    for (int iVar = 0; iVar < nVars; ++iVar) {
      int index = findIndexOfName(mergingVarNames[iVarComb][iVar]);
      if (index != -1) ret[iVarComb] *= getWeightsValue(index);
    }
  }
  // Insert central shower weight
  ret.insert(ret.begin(),getWeightsValue(0));
  return ret;
}

//--------------------------------------------------------------------------

// Collect shower weight names.

void WeightsSimpleShower::collectWeightNames(vector<string>& outputNames) {
  for (int iwt=1; iwt < getWeightsSize(); ++iwt)
    outputNames.push_back("AUX_" + getWeightsName(iwt));
  for (int iwtGrp = 1; iwtGrp < nWeightGroups(); ++iwtGrp)
    outputNames.push_back("AUX_" + getGroupName(iwtGrp));
}

//--------------------------------------------------------------------------

// Collect shower weight values.

void WeightsSimpleShower::collectWeightValues(vector<double>& outputWeights,
  double norm) {
  for (int iwt=1; iwt < getWeightsSize(); ++iwt)
    outputWeights.push_back(getWeightsValue(iwt)*norm);
  for (int iwtGrp = 1; iwtGrp < nWeightGroups(); ++iwtGrp)
    outputWeights.push_back(getGroupWeight(iwtGrp)*norm);
}

//==========================================================================

// WeightsLHEF class.

//--------------------------------------------------------------------------

// Reset all internal values.

void WeightsLHEF::clear() {weightValues.resize(0); weightNames.resize(0);}

//--------------------------------------------------------------------------

// Store the current event information.

void WeightsLHEF::bookVectors(vector<double> weights, vector<string> names) {
  weightValues = weights;
  // Normalize values relative to eventWeightLHEF.
  double norm = 1./infoPtr->eventWeightLHEF;
  for (double& value: weightValues) value *= norm;
  weightNames = convertNames(names);
}

//--------------------------------------------------------------------------

// Function to return processed weights to weight container.

void WeightsLHEF::collectWeightValues(vector<double>& ret, double norm) {

  // Attach the LHEF weights, starting with well-defined MUF and MUR
  // variations, and then followed by any other LHEF weight.
  for (int iwt=0; iwt < getWeightsSize(); ++iwt) {
    double value = getWeightsValue(iwt);
    string name  = getWeightsName(iwt);
    if (name.find("MUR") == string::npos || name.find("MUF") == string::npos)
      continue;
    ret.push_back(value*norm);
  }
  for (int iwt=0; iwt < getWeightsSize(); ++iwt) {
    double value = getWeightsValue(iwt);
    string name  = getWeightsName(iwt);
    if (name.find("MUR") != string::npos || name.find("MUF") != string::npos)
      continue;
    ret.push_back(value*norm);
  }

  // Done.
  return;
}

//--------------------------------------------------------------------------

// Function to return processed weight names to weight container.

void WeightsLHEF::collectWeightNames(vector<string>& ret) {

  // Attach the LHEF weights, starting with well-defined MUF and MUR
  // variations, and then followed by any other LHEF weight.
  for (int iwt = 0; iwt < getWeightsSize(); ++iwt) {
    string name = getWeightsName(iwt);
    if (name.find("MUR") == string::npos || name.find("MUF") == string::npos)
      continue;
    ret.push_back("AUX_"+name);
  }
  for (int iwt=0; iwt < getWeightsSize(); ++iwt) {
    string name  = getWeightsName(iwt);
    if (name.find("MUR") != string::npos || name.find("MUF") != string::npos)
      continue;
    ret.push_back("AUX_"+name);
  }
}


//--------------------------------------------------------------------------

// Convert weight names in MadGraph5 convention to the convention outlined
// in https://arxiv.org/pdf/1405.1067.pdf, page 162ff.

vector<string> WeightsLHEF::convertNames(vector<string> names) {
  vector<string> ret;
  for (size_t i = 0; i < names.size(); ++i) {
    string name = names[i];
    if (name=="1001") name="MUR1.0_MUF1.0";
    if (name=="1002") name="MUR1.0_MUF2.0";
    if (name=="1003") name="MUR1.0_MUF0.5";
    if (name=="1004") name="MUR2.0_MUF1.0";
    if (name=="1005") name="MUR2.0_MUF2.0";
    if (name=="1006") name="MUR2.0_MUF0.5";
    if (name=="1007") name="MUR0.5_MUF1.0";
    if (name=="1008") name="MUR0.5_MUF2.0";
    if (name=="1009") name="MUR0.5_MUF0.5";
    ret.push_back(name);
  }
  return ret;
}

//--------------------------------------------------------------------------

// Identify muR (and muF) variations in LHEF weights. This mapping is needed
// to later combine with the respective shower and merging weights.

void WeightsLHEF::identifyVariationsFromLHAinit(map<string, LHAweight>*
  weights) {
  muRvars.clear();
  int index = 0;
  for (map<string,LHAweight>::const_iterator it =
        weights->begin(); it != weights->end(); ++it) {
    string cont = it->second.contents;
    double muR = 0, muF = 0;
    // Go through all tags of one weight
    while (true) {
      // Erase leading blanks, skip irrelevant tags
      while (cont.substr(0,4) != "muR=" && cont.substr(0,4) != "muF=") {
        if (cont.find_first_of(" ") != string::npos) {
          cont = cont.substr(cont.find_first_of(" ") + 1);
        } else break;
      }
      // Parse muF and muR
      if (cont.substr(0,4) == "muR=") {
        muR = stof(cont.substr(4,cont.find_first_of(" ")));
        cont = cont.substr(cont.find_first_of(" ") + 1);
      }
      if (cont.substr(0,4) == "muF=") {
        muF = stof(cont.substr(4,cont.find_first_of(" ")));
        cont = cont.substr(cont.find_first_of(" ") + 1);
      }
      // Stop if both muF and muR set
      if (muR && muF) break;
      // Also stop if end of contents reached
      if (cont.find_first_of(" ") == string::npos) break;
      }
    // For now, only save muR values for corresponding muF=1.
    if (muF == 1.) muRvars[index] = muR;
    ++index;
  }
}

//==========================================================================

// WeightsMerging class.

//--------------------------------------------------------------------------

// Reset all internal values.

void WeightsMerging::clear() {
  for (size_t i=0; i < weightValues.size(); ++i) {
    weightValues[i] = 1.;
    weightValuesFirst[i] = 0.;
  }
  for (size_t i=0; i < weightValuesP.size(); ++i) {
    weightValuesP[i] = 1.;
    weightValuesFirstP[i] = 0.;
    weightValuesPC[i] = 1.;
    weightValuesFirstPC[i] = 0.;
  }
}

//--------------------------------------------------------------------------

// Initialize merging weights.

void WeightsMerging::init() {

  // Reset weight vectors
  weightValues.resize(0);
  weightNames.resize(0);
  weightValuesFirst.resize(0);
  weightValuesP.resize(0);
  weightValuesPC.resize(0);
  weightValuesFirstP.resize(0);
  weightValuesFirstPC.resize(0);

  // Initialization of all required variation weights done in MergingHooks.cc
  bookWeight("MUR1.0_MUF1.0", 1., 0.);
  isNLO = (infoPtr->settingsPtr->flag("Merging:doUNLOPSLoop") ||
           infoPtr->settingsPtr->flag("Merging:doUNLOPSSubtNLO") ||
           infoPtr->settingsPtr->flag("Merging:doNL3LOOP") );
}

//--------------------------------------------------------------------------

// Function to create a new, synchronized, pair of weight name and value.

void WeightsMerging::bookWeight(string name, double value, double valueFirst) {
  weightNames.push_back(name);
  weightValues.push_back(value);
  weightValuesFirst.push_back(valueFirst);
}

//--------------------------------------------------------------------------

// Store the current event information.

void WeightsMerging::bookVectors(vector<double> weights, vector<string> names){
  weightValues.resize(0);
  weightNames.resize(0);
  weightValuesFirst.resize(0);
  weightValuesP.resize(0);
  weightValuesPC.resize(0);
  weightValuesFirstP.resize(0);
  weightValuesFirstPC.resize(0);
  for (size_t i=0; i < weights.size(); ++i)
    bookWeight(names[i], weights[i], 0.);
}

//--------------------------------------------------------------------------

// Store the current event information, including first order weights for
// NLO merging.

void WeightsMerging::bookVectors(vector<double> weights,
                    vector<double> weightsFirst, vector<string> names) {
  weightValues.resize(0);
  weightNames.resize(0);
  weightValuesFirst.resize(0);
  weightValuesP.resize(0);
  weightValuesPC.resize(0);
  weightValuesFirstP.resize(0);
  weightValuesFirstPC.resize(0);
  for (size_t i=0; i < weights.size(); ++i)
    bookWeight(names[i], weights[i], weightsFirst[i]);
}

//--------------------------------------------------------------------------

// Functions to set values of weights. Does not apply to first order weights or
// scheme variation weights.

void WeightsMerging::reweightValueByIndex(int iPos, double val) {
  weightValues[iPos] *= val;
}

//--------------------------------------------------------------------------

// Reweigth merging weights by name. Does not apply to first order weights or
// scheme variation weights.

void WeightsMerging::reweightValueByName(string name, double val) {
  reweightValueByIndex(findIndexOfName(name), val);}

//--------------------------------------------------------------------------

// Functions to set values of first order weights. Does not apply to scheme
// variation weights.

void WeightsMerging::setValueFirstByIndex(int iPos, double val) {
  weightValuesFirst[iPos] = val;}

//--------------------------------------------------------------------------

// Set values of first order weights by name. Does not apply to scheme
// variation weights.

void WeightsMerging::setValueFirstByName(string name, double val) {
  setValueFirstByIndex(findIndexOfName(name), val);}

//--------------------------------------------------------------------------

// Set values as whole vector.

void WeightsMerging::setValueVector(vector<double> valueVector) {
  weightValues = valueVector;}

//--------------------------------------------------------------------------

// Set first order weight values as vector.

void WeightsMerging::setValueFirstVector(vector<double> valueFirstVector) {
  weightValuesFirst = valueFirstVector;}

//--------------------------------------------------------------------------

// Function telling merging which muR variations to perform, read from
// Merging:muRfactors setting.

vector<double> WeightsMerging::getMuRVarFactors() {
  return infoPtr->settingsPtr->pvec("Merging:muRfactors");}

//--------------------------------------------------------------------------

// Function to return processed weights to weight container.

void WeightsMerging::collectWeightValues(vector<double>& ret, double norm) {
  vector<double> showerWeights =
    infoPtr->weightContainerPtr->weightsSimpleShower.getMuRWeightVector();
  for (int iwt=1; iwt < getWeightsSize(); ++iwt) {
    double value = getWeightsValue(iwt)*norm;
    if (getWeightsValue(0) != 0 ) value /= getWeightsValue(0);
    // Combine with corresponding LHE variation
    if (isNLO) {
      // Check if corresponding muR variation is available
      if (muRVarLHEFindex.find(iwt) == muRVarLHEFindex.end()) {
        string errormsg = "Requested muR variation ";
        errormsg += std::to_string(getMuRVarFactors()[iwt-1]) +
        " not found in LHE file.";
        infoPtr->loggerPtr->ERROR_MSG(errormsg);
      } else
        value *= infoPtr->weightContainerPtr->weightsLHEF.
          getWeightsValue(muRVarLHEFindex[iwt]);
    }
    // Combine with corresponding shower weight
    value *= showerWeights[iwt];
    ret.push_back(value);
  }

  // Include scheme variation weights if present
  if (weightValuesP.size()) {
    for (int iwt=0; iwt < getWeightsSize(); ++iwt) {
      // Normalize with UNLOPS-1 central merging weight, since that is
      // collected into the nominal weight.
      double valueP = getWeightsValueP(iwt)*norm;
      double valuePC = getWeightsValuePC(iwt)*norm;
      if (getWeightsValue(0) != 0 ) {
        valueP /= getWeightsValue(0);
        valuePC /= getWeightsValue(0);
      }

      // Combine with corresponding LHE variation
      if (isNLO) {
        // Check if corresponding muR variation is available
        if (muRVarLHEFindex.find(iwt) != muRVarLHEFindex.end()) {
          double fact = infoPtr->weightContainerPtr->weightsLHEF.
            getWeightsValue(muRVarLHEFindex[iwt]);
          valueP *= fact;
          valuePC *= fact;
        }
      }
      // Combine with corresponding shower weight
      if (iwt != 0) {
        valueP *= showerWeights[iwt-1];
        valuePC *= showerWeights[iwt-1];
      }
      ret.push_back(valueP);
      ret.push_back(valuePC);
    }
  }

  // Done.
  return;
}

//--------------------------------------------------------------------------

// Similar function to return processed weight names.

void WeightsMerging::collectWeightNames(vector<string>& outputNames) {
  for (int iwt=1; iwt < getWeightsSize(); ++iwt) {
    string name  = getWeightsName(iwt);
    outputNames.push_back(name);
  }

  // Include scheme variation names if present
  if (weightValuesP.size()) {
    for (int iwt=0; iwt < getWeightsSize(); ++iwt) {
      string nameP  = getWeightsName(iwt)+"_SCHEMEP";
      string namePC  = getWeightsName(iwt)+"_SCHEMEPC";
      outputNames.push_back(nameP);
      outputNames.push_back(namePC);
    }
  }
  return;
}

//--------------------------------------------------------------------------

// Set up mapping between LHEF variations and weights.

void WeightsMerging::setLHEFvariationMapping() {
  if (!isNLO) return;
  map<int,double> muRvarsLHEF = infoPtr->weightContainerPtr->weightsLHEF.
    muRvars;
  vector<double> muRvarsMerging = getMuRVarFactors();
  for (unsigned int iMerVar = 0; iMerVar < muRvarsMerging.size(); ++iMerVar) {
    for (pair<int,double> muRvarLHEF: muRvarsLHEF) {
      if (abs(muRvarLHEF.second - muRvarsMerging[iMerVar]) < 1e-10) {
        muRVarLHEFindex[iMerVar+1] = muRvarLHEF.first;
        continue;
      }
    }
  }
}

//==========================================================================

// WeightsFragmentation class.

//--------------------------------------------------------------------------

// Fragmentation variations initialization.

void WeightsFragmentation::init() {
  WeightsBase::init();
  weightParms.clear();
  externalGroupNames.clear();
  externalMap.clear();
  flavBreaks = vector<int>(13, 0);

  // Read weight groups into a dictionary.
  map<string, map<string, double> > weightGroups;
  parse("VariationFrag:list", weightGroups);

  // Define the ordering of the parameters and key mapping.
  weightParms.resize(keyOrder.size());

  // Initialize the flavor selector.
  StringFlav flavSel;
  flavSel.initInfoPtr(*infoPtr);
  flavSel.init();

  // Create a map from variation keys to standard keys.
  vector<map<string, string> > keyMap(keyOrder.size());
  for (int iFac = 0; iFac < (int)keyOrder.size(); ++iFac)
    for (auto &key : keyOrder[iFac]) keyMap[iFac][key.first] = key.second;

  // Store the baseline parameters.
  Settings *settingsPtr = infoPtr->settingsPtr;
  vector<map<string, double> > baseParms(keyOrder.size());
  flavBase.resize(0);
  for (auto &idx : flavIdxs)
    flavBase.push_back(flavSel.getFlavourSpinRatios(0, idx));
  for (int iFac = 0; iFac < (int)keyOrder.size(); ++iFac) {
    for(auto &key : keyOrder[iFac]) {
      baseParms[iFac][key.first] = settingsPtr->parm(key.second);
      if (iFac == Flav) flavBase.push_back(baseParms[iFac][key.first]);
    }
  }

  // Loop over the groups and determine the parameter variations.
  for (auto &group : weightGroups) {

    // Set the parameter variations for a group.
    externalGroupNames.push_back(group.first);
    vector<map<string, double> > varParms = baseParms;
    for (auto &var : group.second) {
      for (int iFac = 0; iFac < (int)varParms.size(); ++iFac) {
        auto itr = varParms[iFac].find(var.first);
        if (itr != varParms[iFac].end()) {itr->second = var.second; break;}
      }
    }

    // Map the group parameters to factorized parameters.
    externalMap.push_back({});
    for (int iFac = 0; iFac < (int)keyMap.size(); ++iFac) {
      if (varParms[iFac] == baseParms[iFac]) continue;
      string name;
      vector<double> key;

      // Transform flavor parameters if needed.
      if (iFac == Flav) {
        for (auto &var : varParms[iFac])
          settingsPtr->parm(keyMap[Flav][var.first], var.second, true);
        flavSel.init();
        for (auto &idx : flavIdxs)
          key.push_back(flavSel.getFlavourSpinRatios(0, idx));
        for (auto &var : baseParms[iFac])
          settingsPtr->parm(keyMap[Flav][var.first], var.second, true);
      }

      // Set the standard parameters.
      for (auto &var : keyOrder[iFac]) {
        double val = varParms[iFac][var.first];
        key.push_back(val);
        name += var.first + "=" + toString(val) + "+";
      }

      // Check if this is a new factorized variation.
      auto parm = weightParms[iFac].insert({key, weightValues.size()});
      externalMap.back().push_back(parm.first->second);
      if (!parm.second) continue;
      weightValues.push_back(1.);
      weightNames.push_back(name.substr(0, name.size() - 1));
    }
  }

}

//--------------------------------------------------------------------------

// Collect fragmentation weight names.

void WeightsFragmentation::collectWeightNames(vector<string>& outputNames) {
  for (int iWgt = 1; iWgt < getWeightsSize(); ++iWgt)
    outputNames.push_back("AUX_" + getWeightsName(iWgt));
  for (int iWG = 0; iWG < nWeightGroups(); ++iWG)
    outputNames.push_back("AUX_" + getGroupName(iWG));
}

//--------------------------------------------------------------------------

// Collect fragmentation weight values.

void WeightsFragmentation::collectWeightValues(vector<double>& outputWeights,
  double norm) {
  for (int iWgt = 1; iWgt < getWeightsSize(); ++iWgt)
    outputWeights.push_back(getWeightsValue(iWgt)*norm);
  for (int iWG = 0; iWG < nWeightGroups(); ++iWG)
    outputWeights.push_back(getGroupWeight(iWG)*norm);
}

//--------------------------------------------------------------------------

// Calculate the derived flavor parameters.

vector<double> WeightsFragmentation::flavParms(
  double xi, double rho, double x, double y) {

  // Set the primary parameters.
  int offset = flavIdxs.size();
  vector<double> parms(offset + 4, 0);
  parms[offset + 0] = xi;
  parms[offset + 1] = rho;
  parms[offset + 2] = x;
  parms[offset + 3] = y;

  // Pass the primary parameters to settings.
  Settings* settingsPtr = infoPtr->settingsPtr;
  for (int idx = 0; idx < (int)keyOrder[Flav].size(); ++idx)
    settingsPtr->parm(keyOrder[Flav][idx].second,
      parms[offset + idx], true);

  // Initialize a flavor selector.
  StringFlav flavSel;
  flavSel.initInfoPtr(*infoPtr);
  flavSel.init();

  // Set the derived parameters.
  for (int idx = 0; idx < (int)flavIdxs.size(); ++idx)
    parms[idx] = flavSel.getFlavourSpinRatios(0, flavIdxs[idx]);

  // Reset the settings.
  for (int idx = 0; idx < (int)keyOrder[Flav].size(); ++idx)
    settingsPtr->parm(keyOrder[Flav][idx].second,
      flavBase[offset + idx], true);
  return parms;

}

//--------------------------------------------------------------------------

// Calculate a flavor weight, given the derived parameters and breaks.

double WeightsFragmentation::flavWeight(const vector<double>& parms,
  const vector<int>& breaks) {

  // Loop over parameter ratios (avoids floating point exceptions).
  double wgt = 1;
  static const vector<int> iParms  = {6, 5, 0, 1, 2, 3, 4};
  static const vector<int> iBreaks = {2, 3, 4, 6, 8, 9, 12};
  for (int i = 0; i < (int)iParms.size(); ++i) {
    // Weight unchanged if negative for 5th parameter.
    if (iParms[i] == 5 && parms[iParms[i]] <= 0) continue;
    // Weight unchanged if no breaks.
    if (breaks[iBreaks[i]] == 0) continue;
    // Infinite weight if parameter is infinite.
    if (isinf(parms[iParms[i]])) return numeric_limits<double>::infinity();
    wgt *= pow(parms[iParms[i]] / flavBase[iParms[i]], breaks[iBreaks[i]]);
  }

  // Include remaining weight terms and return.
  return wgt * pow((1. + flavBase[5]) / (1. + parms[5]), breaks[0])
    * pow((2. + flavBase[6]) / (2. + parms[6]), breaks[1])
    * pow((2. + flavBase[0]) / (2. + parms[0]), breaks[3])
    * pow((2. + flavBase[1]) / (2. + parms[1]), breaks[5])
    * pow((2. + flavBase[2]) / (2. + parms[2]), breaks[7])
    * pow((1. - parms[3]) / (1. - flavBase[3]), breaks[10])
    * pow((1. + flavBase[4]) / (1. + parms[4]), breaks[11]);

}

//--------------------------------------------------------------------------

// Count the flavor breaks for variations.

void WeightsFragmentation::flavCount(int idIn, bool early, bool noChoice) {

  // Determine the break properties and store.
  int idPop = idIn / 1000;
  int idVtx = (idIn - idPop*1000) / 100;
  int spin  = idIn - idPop*1000 - idVtx*100;

  // No diquark break from a diquark mother.
  if (!noChoice) ++flavBreaks[0];
  // If early is true, this is a quark-pair.
  if (early) {
    ++flavBreaks[1];
    // Include additional factor for s quark.
    if (idIn == 3) ++flavBreaks[2];
    return;
  }
  // Additional pieces for diquark probabilities.
  ++flavBreaks[3];
  if (idPop > 2) ++flavBreaks[4];
  if (idPop < 3) {
    ++flavBreaks[5];
    if (idVtx > 2) ++flavBreaks[6];
  } else {
    ++flavBreaks[7];
    if (idVtx > 2) ++flavBreaks[8];
  }
  if (idPop < 3 && idVtx < 3) {
    if (idPop == idVtx) ++flavBreaks[9];
    else ++flavBreaks[10];
  }
  if (idPop != idVtx) {
    ++flavBreaks[11];
    if (spin > 1) ++flavBreaks[12];
  }

}

//==========================================================================

// The WeightContainer class.

//--------------------------------------------------------------------------

// Set nominal weight.

void WeightContainer::setWeightNominal(double weightNow) {
  weightNominal = weightNow;}

//--------------------------------------------------------------------------

// Assemble nominal weight, including nominal parton shower and merging weight.
// PS and Merging weight variations are stored relative to this return value.

double WeightContainer::collectWeightNominal() {
  return weightNominal * weightsShowerPtr->getWeightsValue(0)
                       * weightsMerging.getWeightsValue(0)
                       * weightsFragmentation.getWeightsValue(0);
}


//--------------------------------------------------------------------------

// Functions to retrieve the stored information.

int WeightContainer::numberOfWeights() {
  return weightValueVector().size();
}

double WeightContainer::weightValueByIndex(int key) {
  return weightValueVector()[key];
}

string WeightContainer::weightNameByIndex(int key) {
  return weightNameVector()[key];
}

//--------------------------------------------------------------------------

// Function to return the vector of weight values, combining all weights from
// all subcategories.

vector<double> WeightContainer::weightValueVector() {
  vector<double> ret;

  // The very first entry in the vector should always be the nominal weight.
  double collWgtNom = collectWeightNominal();
  ret.push_back(collWgtNom);

  // Let all weights attach the relative weight values to the return vector.
  // Second argument allows for normalization.
  if (!doSuppressAUXweights) {
    weightsLHEF.collectWeightValues(ret,collWgtNom);
    weightsShowerPtr->collectWeightValues(ret,collWgtNom);
    weightsFragmentation.collectWeightValues(ret,collWgtNom);
  }
  weightsUserHooks.collectWeightValues(ret,collWgtNom);
  weightsMerging.collectWeightValues(ret,collWgtNom);

  // Done.
  return ret;

}

//--------------------------------------------------------------------------

// Function to return the vector of weight names, combining all names from
// all subcategories, cf. weightValueVector function.

vector<string> WeightContainer::weightNameVector() {
  vector<string> ret;

   // The very first entry in the vector should always be the nominal weight.
  ret.push_back("Weight");

  // Let all weights attach the weight names to the return vector.
  if (!doSuppressAUXweights) {
    weightsLHEF.collectWeightNames(ret);
    weightsShowerPtr->collectWeightNames(ret);
    weightsFragmentation.collectWeightNames(ret);
  }
  weightsUserHooks.collectWeightNames(ret);
  weightsMerging.collectWeightNames(ret);

  // Done.
  return ret;

}

//--------------------------------------------------------------------------

// Reset all members to default status.

void WeightContainer::clear() {
  weightNominal = 1.;
  weightsLHEF.clear();
  if (weightsShowerPtr != nullptr) weightsShowerPtr->clear();
  weightsFragmentation.clear();
  weightsUserHooks.clear();
  weightsMerging.clear();
}

//--------------------------------------------------------------------------

// Reset total cross section estimate.

void WeightContainer::clearTotal() {
  if (sigmaTotal.size()) {
    sigmaTotal = vector<double>(sigmaTotal.size(),0.);
    errorTotal = vector<double>(errorTotal.size(),0.);
  }
}

//--------------------------------------------------------------------------

// Function to set Pointers in weight classes.

void WeightContainer::initPtrs(Info* infoPtrIn) {
  infoPtr = infoPtrIn;
  // Default pointer to shower weights = weightsSimpleShower.
  weightsShowerPtr = &weightsSimpleShower;
  weightsLHEF.setPtrs(infoPtrIn);
  weightsShowerPtr->setPtrs(infoPtrIn);
  weightsFragmentation.setPtrs(infoPtrIn);
  weightsUserHooks.setPtrs(infoPtrIn);
  weightsMerging.setPtrs(infoPtrIn);
}

//--------------------------------------------------------------------------

// Function to initialize weight classes.

void WeightContainer::init( bool doMerging ) {
  weightsShowerPtr->init(doMerging);
  weightsFragmentation.init();
  weightsUserHooks.init();
  weightsMerging.init();
  doSuppressAUXweights = infoPtr->settingsPtr->
    flag("Weights:suppressAUX");
  if (xsecIsInit) {
    sigmaSample = vector<double>(sigmaSample.size(),0.);
    errorSample = vector<double>(errorSample.size(),0.);
  }
}

//--------------------------------------------------------------------------

// Initialize accumulation of cross section.

void WeightContainer::initXsecVec() {
  if (!xsecIsInit) {
    sigmaTotal = vector<double>(weightNameVector().size(),0.);
    sigmaSample = vector<double>(weightNameVector().size(),0.);
    errorTotal = vector<double>(weightNameVector().size(),0.);
    errorSample = vector<double>(weightNameVector().size(),0.);
    xsecIsInit = true;
  }
}

//--------------------------------------------------------------------------

// Return cross section for current sample.

vector<double> WeightContainer::getSampleXsec() {return sigmaSample;}

//--------------------------------------------------------------------------

// Return cross section error for current sample.

vector<double> WeightContainer::getSampleXsecErr() {
  vector<double> ret;
  for (double error: errorSample) ret.push_back(sqrt(error));
  return ret;
}

//--------------------------------------------------------------------------

// Return cross section estimate for total run.

vector<double> WeightContainer::getTotalXsec() {return sigmaTotal;}

//--------------------------------------------------------------------------

// Return cross section error estimate for total run.
vector<double> WeightContainer::getTotalXsecErr() {
  vector<double> ret;
  for (double error: errorTotal) ret.push_back(sqrt(error));
  return ret;
}
//--------------------------------------------------------------------------

// Accumulate cross section for all weights. Provide cross section estimate
// for whole sample if lhaStrategy != 4 or -4.

void WeightContainer::accumulateXsec(double norm) {
  if (!xsecIsInit) initXsecVec();
  vector<double> weights = weightValueVector();
  for (unsigned int iWgt = 0; iWgt < weights.size(); ++iWgt) {
    sigmaTotal[iWgt] += weights[iWgt]*norm;
    sigmaSample[iWgt] += weights[iWgt]*norm;
    errorTotal[iWgt] += pow2(weights[iWgt]*norm);
    errorSample[iWgt] += pow2(weights[iWgt]*norm);
  }
}

//==========================================================================

} // end namespace Pythia8
