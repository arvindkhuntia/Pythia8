// main203.cc is a part of the PYTHIA event generator.
// Copyright (C) 2025 Torbjorn Sjostrand.
// PYTHIA is licenced under the GNU GPL v2 or later, see COPYING for details.
// Please respect the MCnet Guidelines, see GUIDELINES for details.

// Authors: Juan Rojo <authors@pythia.org>

// Keywords: parton distribution; LHAPDF

// This program compares the internal and LHAPDF implementations of
// some NNPDF 2.3 QCD+QED sets, for results and for timing.
// Warning: this example is constructed to work for LHAPDF5,
// but current choice is LHAPDF6, which gives some differences.

#include "Pythia8/Pythia.h"
#include "Pythia8/Plugins.h"
using namespace Pythia8;

//==========================================================================

int main() {

  cout<<"\n NNPDF3.1 QED LO phenomenology \n "<<endl;
  cout<<"\n Check access to NNPDF3.1 NNLO QED sets \n "<<endl;

  // Generator.
  Pythia pythia;

  // Access the PDFs.
  int idBeamIn = 2212;
  string pdfPath = pythia.settings.word("xmlPath") + "../pdfdata";
  Logger logger;

  // Grid of studied points.
  string xpdf[] = {"x*g","x*d","x*u","x*s"};
  double xlha[] = {1e-5, 1e-1};
  double Q2[] = { 2.0, 10000.0 };
  string setName;
  string setName_lha;

  // For timing checks.
  int const nq = 500;
  int const nx = 500;
  int const iqMax = sizeof( xlha )/sizeof( xlha[0] );

  // Loop over two internal PDF sets in Pythia8
  // and compare with their LHAPDF correspondents.
  for (int iFitIn = 3; iFitIn < 5; iFitIn++) {

    // Constructor for LHAPDF.
    if (iFitIn == 3) setName = "NNPDF31_nlo_as_0118_luxqed";
    if (iFitIn == 4) setName = "NNPDF31_nnlo_as_0118_luxqed";
    PDFPtr pdfs_nnpdf_lha =
      make_plugin<PDF>("libpythia8lhapdf6.so", "LHAPDF6");
    if (pdfs_nnpdf_lha == nullptr) return -1;
    pdfs_nnpdf_lha->init(idBeamIn, setName, 0, &logger);
    cout << "\n PDF set = " << setName << " \n" << endl;

    // Constructor for internal PDFs.
    LHAGrid1 pdfs_nnpdf(
      idBeamIn, setName + "_0000.dat", pdfPath, &logger);

    // Check quarks and gluons.
    cout << setprecision(6);
    for (int f = 0; f < 4; f++) {
      for (int iq = 0; iq < iqMax; iq++) {
        cout << "  " << xpdf[f] << ", Q2 = " << Q2[iq] << endl;
        cout << "   x \t     Pythia8\t   LHAPDF\t   diff(%) " << endl;
        for (int ix = 0; ix < 2; ix++) {
          double a = pdfs_nnpdf.xf( f, xlha[ix], Q2[iq]);
          double b = pdfs_nnpdf_lha->xf( f, xlha[ix], Q2[iq]);
          double diff = b != 0 ? 1e2 * abs((a-b)/b) :
            std::numeric_limits<double>::infinity();
          cout << scientific << xlha[ix] << " " << a << " " << b
               << " " << diff << endl;
        }
      }
    }

    // Check photon.
    cout << "\n Now checking the photon PDF \n" << endl;
    for (int iq = 0; iq < iqMax; iq++) {
      cout << "  " << "x*gamma" << ", Q2 = " << Q2[iq] << endl;
      cout << "   x \t     Pythia8\t   LHAPDF\t   diff(%) " << endl;
      for (int ix = 0; ix < 2; ix++) {
        double a = pdfs_nnpdf.xf( 22, xlha[ix], Q2[iq]);
        double b = pdfs_nnpdf_lha->xf( 22, xlha[ix], Q2[iq]);
        double diff = b != 0 ? 1e2 * abs((a-b)/b) :
          std::numeric_limits<double>::infinity();
        cout << scientific << xlha[ix] << " " << a << " " << b
             << " " << diff << endl;
      }
    }

    // Now check the timings for evolution.
    cout << "\n Checking timings " << endl;

    // Internal timing.
    clock_t tBegin = clock();
    for (int f = -4; f < 4; f++) {
      for (int iq = 0; iq < nq; iq++) {
        double qq2 = 2.0 * pow( 1e6 / 2.0, double(iq)/nq);
        for (int ix = 0; ix < nx; ix++) {
          double xx = 1e-6 * pow( 9e-1 / 1e-6, double(ix)/nx);
          pdfs_nnpdf.xf(f,xx,qq2);
        }
      }
    }
    clock_t tEnd = clock();
    double tUsed = double(tEnd - tBegin) / double(CLOCKS_PER_SEC);
    cout << " NNPDF internal timing = " << tUsed << endl;

    // External timing.
    tBegin = clock();
    for (int f = -4; f < 4; f++) {
      for (int iq = 0; iq < nq; iq++) {
        double qq2 = 2.0 * pow(1e6 / 2.0, double(iq)/nq);
        for (int ix = 0; ix < nx; ix++) {
          double xx = 1e-6 * pow( 9e-1 / 1e-6, double(ix)/nx);
          pdfs_nnpdf_lha->xf(f,xx,qq2);
        }
      }
    }
    tEnd = clock();
    tUsed = double(tEnd - tBegin) / double(CLOCKS_PER_SEC);
    cout << " NNPDF LHAPDF   timing = " << tUsed << endl;

  } // End loop over NNPDF internal sets

  // Done.
  cout << "\n Compared LHAPDF and internal Pythia8 results.\n" << endl;

  return 0;
}
