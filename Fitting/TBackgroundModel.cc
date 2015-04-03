#include "TMinuit.h"
#include "TLine.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TLegend.h"
#include "TBackgroundModel.hh"
#include "TApplication.h"
#include "TRandom3.h"
#include "TAxis.h"
#include "TLine.h"
#include "TPaveText.h"
#include "TPave.h"
#include "TMath.h"
#include <cmath>
#include <string>
#include <vector>


using namespace std;
using std::cout;
using std::endl;
using std::map;
using std::vector;

ClassImp(TBackgroundModel)
  
//first set up a global function that calls your classes method that calculates the quantity to minimize
void myExternal_FCNAdap(int &n, double *grad, double &fval, double x[], int code)
{
  // Required External Wrapper for function to be minimized by Minuit 
 
  // This gets called for each value of the parameters minuit tries
  // here the x array contains the parameters you are trying to fit
  
  // here myClass should inherit from TObject
  TBackgroundModel* Obj = (TBackgroundModel*)gMinuit->GetObjectFit(); 

  // implement a method in your class for setting the parameters and thus update the parameters of your fitter class 
  for(int i = 0; i < 29; i++ )
  {
    Obj->SetParameters(i, x[i]);
  }
  // Implement a method in your class that calculates the quantity you want to minimize, here I call it GetChiSquare. set its output equal to fval. minuit tries to minimize fval
    Obj->UpdateModelAdaptive();
    fval = Obj->GetChiSquareAdaptive();
}

TBackgroundModel::TBackgroundModel()
{

}

TBackgroundModel::TBackgroundModel(double fFitMin, double fFitMax, int fBinBase, int fDataSet, bool fSave)
{
  bSave = fSave;
  bToyData = false;

  tTime = new TDatime();
  dNParam = 29; // number of fitting parameters
  dNumCalls = 0;
  dSecToYears = 1./(60*60*24*365);


  // Data directories depending on QCC/local
  // dDataDir =  "/Users/brian/macros/Simulations/Production/";
  dDataDir =  "/Users/brian/macros/CUOREZ/Bkg";
   // dDataDir = "/cuore/user/zhubrian/CUORE0/scratch/v2.30";
  dMCDir = "/Users/brian/macros/Simulations/Production";
   // dMCDir = "/cuore/user/zhubrian/MC/Bkg";
  dSaveDir = "/Users/brian/Dropbox/code/Fitting";
   // dSaveDir = "/cuore/user/zhubrian/code/Fitting";
  dDataIntegral = 0;

  // Bin size (keV) -- base binning is 1 keV
  dBinSize = 1; 
  // Histogram range - from 0 to 10 MeV
  dMinEnergy = 0.;
  dMaxEnergy = 10000.;
  dBinBase = fBinBase;
  dDataSet = fDataSet;

  dChiSquare = 0;

  if(fFitMin >= fFitMax)
  {
    cout << "Fit Min >= Fit Max!" << endl;
  }

  minuit = new TMinuit(dNParam);

  // Fitting range
  dFitMin = fFitMin;
  dFitMax = fFitMax;

  dNBins = (dMaxEnergy - dMinEnergy)/ dBinSize;

  // Data Histograms
  fDataHistoTot     = new TH1D("fDataHistoTot",  "", dNBins, dMinEnergy, dMaxEnergy);
  fDataHistoM1      = new TH1D("fDataHistoM1",   "", dNBins, dMinEnergy, dMaxEnergy);
  fDataHistoM2      = new TH1D("fDataHistoM2",   "", dNBins, dMinEnergy, dMaxEnergy);
  fDataHistoM2Sum   = new TH1D("fDataHistoM2Sum",   "", dNBins, dMinEnergy, dMaxEnergy);

  // Data cuts 
  qtree = new TChain("qtree");
  // base_cut = base_cut && "(TimeUntilSignalEvent_SameChannel > 4.0 || TimeUntilSignalEvent_SameChannel < 0)";
  // base_cut = base_cut && "(TimeSinceSignalEvent_SameChannel > 3.1 || TimeSinceSignalEvent_SameChannel < 0)";
  base_cut = base_cut && "Channel != 1 && Channel != 10"; 

  // Old PSA cuts
  // base_cut = base_cut && "abs(BaselineSlope)<0.1";
  // base_cut = base_cut && "OF_TVR < 1.75 && OF_TVL < 2.05";

  // New PSA cuts
  // base_cut = base_cut && "NormBaselineSlope < 4.8 && NormBaselineSlope > -4";
  // base_cut = base_cut && "NormRiseTime < 4.8 && NormRiseTime > -4";
  // base_cut = base_cut && "NormDecayTime < 4.8 && NormDecayTime > -4";
  // base_cut = base_cut && "NormDelay < 4.8 && NormDelay > -4";
  // base_cut = base_cut && "NormTVL < 5.3 && NormTVL > -6";
  // base_cut = base_cut && "NormTVR < 5.3 && NormTVR > -6";

  LoadData();

  TF1 *fEff = new TF1("fEff", "[0]+[1]/(1+[2]*exp(-[3]*x)) + [4]/(1+[5]*exp(-[6]*x))", dMinEnergy, dMaxEnergy);
  fEff->SetParameters(-4.71e-2, 1.12e-1, 2.29, -8.81e-5, 9.68e-1, 2.09, 1.58e-2);

  hEfficiency = new TH1D("hEfficiency", "", dNBins, dMinEnergy, dMaxEnergy);

  for(int i = 1; i <= hEfficiency->GetNbinsX(); i++)
  {
    hEfficiency->SetBinContent(i, fEff->Eval(hEfficiency->GetBinCenter(i)));
  }

  fDataHistoM1->Divide( hEfficiency );
  fDataHistoM2->Divide( hEfficiency );
  fDataHistoM2Sum->Divide( hEfficiency );


  // Total model histograms M1
  // M2Sum histograms created but not used (in case I need them someday...)
  fModelTotM1      = new TH1D("fModelTotM1",      "Frame",        dNBins, dMinEnergy, dMaxEnergy);  
  fModelTotthM1    = new TH1D("fModelTotthM1",    "Total th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotuM1     = new TH1D("fModelTotuM1",     "Total u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotkM1     = new TH1D("fModelTotkM1",     "Total k40",    dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcoM1    = new TH1D("fModelTotvoM1",    "Total co60",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotmnM1    = new TH1D("fModelTotmnM1",    "Total mn54",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotNDBDM1  = new TH1D("fModelTotNDBDM1",  "Total NDBD",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTot2NDBDM1 = new TH1D("fModelTot2NDBDM1", "Total 2NDBD",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbiM1    = new TH1D("fModelTotbiM1",    "Total bi207",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbi2M1   = new TH1D("fModelTotbi2M1",   "Total bi210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotptM1    = new TH1D("fModelTotptM1",    "Total pt190",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotpbM1    = new TH1D("fModelTotpbM1",    "Total pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcsM1    = new TH1D("fModelTotcsM1",    "Total cs137",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotco2M1   = new TH1D("fModelTotco2M1",   "Total co58",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotteo2M1  = new TH1D("fModelTotteo2M1",  "Total TeO2",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotSthM1   = new TH1D("fModelTotSthM1",   "Total S th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSuM1    = new TH1D("fModelTotSuM1",    "Total S u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpbM1   = new TH1D("fModelTotSpbM1",   "Total S pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpoM1   = new TH1D("fModelTotSpoM1",   "Total S po210",  dNBins, dMinEnergy, dMaxEnergy);

  fModelTotExtM1   = new TH1D("fModelTotExtM1",   "Total External",  dNBins, dMinEnergy, dMaxEnergy);

  // Total model histograms M2
  fModelTotM2      = new TH1D("fModelTotM2",      "Frame",        dNBins, dMinEnergy, dMaxEnergy);  
  fModelTotthM2    = new TH1D("fModelTotthM2",    "Total th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotuM2     = new TH1D("fModelTotuM2",     "Total u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotkM2     = new TH1D("fModelTotkM2",     "Total k40",    dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcoM2    = new TH1D("fModelTotcoM2",    "Total co60",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotmnM2    = new TH1D("fModelTotmnM2",    "Total mn54",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotNDBDM2  = new TH1D("fModelTotNDBDM2",  "Total NDBD",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTot2NDBDM2 = new TH1D("fModelTot2NDBDM2", "Total 2NDBD",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbiM2    = new TH1D("fModelTotbiM2",    "Total bi207",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbi2M2   = new TH1D("fModelTotbi2M2",   "Total bi210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotptM2    = new TH1D("fModelTotptM2",    "Total pt190",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotpbM2    = new TH1D("fModelTotpbM2",    "Total pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcsM2    = new TH1D("fModelTotcsM2",    "Total cs137",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotco2M2   = new TH1D("fModelTotco2M2",   "Total co58",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotteo2M2  = new TH1D("fModelTotteo2M2",  "Total TeO2",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotSthM2   = new TH1D("fModelTotSthM2",   "Total S th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSuM2    = new TH1D("fModelTotSuM2",    "Total S u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpbM2   = new TH1D("fModelTotSpbM2",   "Total S pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpoM2   = new TH1D("fModelTotSpoM2",   "Total S po210",  dNBins, dMinEnergy, dMaxEnergy);

  fModelTotExtM2   = new TH1D("fModelTotExtM2",   "Total External",  dNBins, dMinEnergy, dMaxEnergy);


  // Total model histograms M2Sum
  fModelTotM2Sum      = new TH1D("fModelTotM2Sum",      "Frame",        dNBins, dMinEnergy, dMaxEnergy);  
  fModelTotthM2Sum    = new TH1D("fModelTotthM2Sum",    "Total th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotuM2Sum     = new TH1D("fModelTotuM2Sum",     "Total u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotkM2Sum     = new TH1D("fModelTotkM2Sum",     "Total k40",    dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcoM2Sum    = new TH1D("fModelTotcoM2Sum",    "Total co60",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotmnM2Sum    = new TH1D("fModelTotmnM2Sum",    "Total mn54",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotNDBDM2Sum  = new TH1D("fModelTotNDBDM2Sum",  "Total NDBD",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTot2NDBDM2Sum = new TH1D("fModelTot2NDBDM2Sum", "Total 2NDBD",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbiM2Sum    = new TH1D("fModelTotbiM2Sum",    "Total bi207",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotbi2M2Sum   = new TH1D("fModelTotbi2M2Sum",   "Total bi210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotptM2Sum    = new TH1D("fModelTotptM2Sum",    "Total pt190",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotpbM2Sum    = new TH1D("fModelTotpbM2Sum",    "Total pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotcsM2Sum    = new TH1D("fModelTotcsM2Sum",    "Total cs137",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotco2M2Sum   = new TH1D("fModelTotco2M2Sum",   "Total co58",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotteo2M2Sum  = new TH1D("fModelTotteo2M2Sum",  "Total TeO2",   dNBins, dMinEnergy, dMaxEnergy);

  fModelTotSthM2Sum   = new TH1D("fModelTotSthM2Sum",   "Total S th232",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSuM2Sum    = new TH1D("fModelTotSuM2Sum",    "Total S u238",   dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpbM2Sum   = new TH1D("fModelTotSpbM2Sum",   "Total S pb210",  dNBins, dMinEnergy, dMaxEnergy);
  fModelTotSpoM2Sum   = new TH1D("fModelTotSpoM2Sum",   "Total S po210",  dNBins, dMinEnergy, dMaxEnergy);

  fModelTotExtM2Sum   = new TH1D("fModelTotExtM2Sum",   "Total External",  dNBins, dMinEnergy, dMaxEnergy);

/////////////////// Model histograms
//////////// Crystal M1 and M2
  hTeO20nuM1       = new TH1D("hTeO20nuM1",    "hTeO20nuM1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO22nuM1       = new TH1D("hTeO22nuM1",    "hTeO22nuM1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2co60M1      = new TH1D("hTeO2co60M1",   "hTeO2co60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2k40M1       = new TH1D("hTeO2k40M1",    "hTeO2k40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2pb210M1     = new TH1D("hTeO2pb210M1",  "hTeO2pb210M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2po210M1     = new TH1D("hTeO2po210M1",  "hTeO2po210M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2te125M1     = new TH1D("hTeO2te125M1",  "hTeO2te125M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th232M1     = new TH1D("hTeO2th232M1",  "hTeO2th232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hTeO2th228M1     = new TH1D("hTeO2th228M1",  "hTeO2th228M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra226M1     = new TH1D("hTeO2ra226M1",  "hTeO2ra226M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2rn222M1     = new TH1D("hTeO2rn222M1",  "hTeO2rn222M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u238M1      = new TH1D("hTeO2u238M1",   "hTeO2u238M1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230M1     = new TH1D("hTeO2th230M1",  "hTeO2th230M1",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u234M1      = new TH1D("hTeO2u234M1",   "hTeO2u234M1",   dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Spb210M1_01      = new TH1D("hTeO2Spb210M1_01",   "hTeO2Spb210M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M1_001     = new TH1D("hTeO2Spo210M1_001",  "hTeO2Spo210M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M1_01      = new TH1D("hTeO2Spo210M1_01",   "hTeO2Spo210M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sth232M1_01      = new TH1D("hTeO2Sth232M1_01",   "hTeO2Sth232M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Su238M1_01       = new TH1D("hTeO2Su238M1_01",    "hTeO2Su238M1_01",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_001    = new TH1D("hTeO2Sxpb210M1_001", "hTeO2Sxpb210M1_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_01     = new TH1D("hTeO2Sxpb210M1_01",  "hTeO2Sxpb210M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_1      = new TH1D("hTeO2Sxpb210M1_1",   "hTeO2Sxpb210M1_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_10     = new TH1D("hTeO2Sxpb210M1_10",  "hTeO2Sxpb210M1_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M1_001    = new TH1D("hTeO2Sxpo210M1_001", "hTeO2Sxpo210M1_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M1_01     = new TH1D("hTeO2Sxpo210M1_01",  "hTeO2Sxpo210M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M1_1      = new TH1D("hTeO2Sxpo210M1_1",   "hTeO2Sxpo210M1_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M1_001    = new TH1D("hTeO2Sxth232M1_001", "hTeO2Sxth232M1_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M1_01     = new TH1D("hTeO2Sxth232M1_01",  "hTeO2Sxth232M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M1_1      = new TH1D("hTeO2Sxth232M1_1",   "hTeO2Sxth232M1_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M1_10     = new TH1D("hTeO2Sxth232M1_10",  "hTeO2Sxth232M1_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M1_001     = new TH1D("hTeO2Sxu238M1_001",  "hTeO2Sxu238M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M1_01      = new TH1D("hTeO2Sxu238M1_01",   "hTeO2Sxu238M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M1_1       = new TH1D("hTeO2Sxu238M1_1",    "hTeO2Sxu238M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M1_10      = new TH1D("hTeO2Sxu238M1_10",   "hTeO2Sxu238M1_10",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M1_100      = new TH1D("hTeO2Sxu238M1_100",   "hTeO2Sxu238M1_100",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M1_100     = new TH1D("hTeO2Sxth232M1_100",  "hTeO2Sxth232M1_100",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_100     = new TH1D("hTeO2Sxpb210M1_100",  "hTeO2Sxpb210M1_100",  dNBins, dMinEnergy, dMaxEnergy);


  hTeO2th232onlyM1      = new TH1D("hTeO2th232onlyM1", "hTeO2th232onlyM1",     dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra228pb208M1     = new TH1D("hTeO2ra228pb208M1", "hTeO2ra228pb208M1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230onlyM1      = new TH1D("hTeO2th230onlyM1", "hTeO2th230onlyM1",     dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM1_001  = new TH1D("hTeO2Sxth232onlyM1_001", "hTeO2Sxth232onlyM1_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M1_001 = new TH1D("hTeO2Sxra228pb208M1_001", "hTeO2Sxra228pb208M1_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M1_001  = new TH1D("hTeO2Sxu238th230M1_001", "hTeO2Sxu238th230M1_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM1_001  = new TH1D("hTeO2Sxth230onlyM1_001", "hTeO2Sxth230onlyM1_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M1_001 = new TH1D("hTeO2Sxra226pb210M1_001", "hTeO2Sxra226pb210M1_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M1_0001     = new TH1D("hTeO2Sxpb210M1_0001", "hTeO2Sxpb210M1_0001",         dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM1_01  = new TH1D("hTeO2Sxth232onlyM1_01", "hTeO2Sxth232onlyM1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M1_01 = new TH1D("hTeO2Sxra228pb208M1_01", "hTeO2Sxra228pb208M1_01", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M1_01  = new TH1D("hTeO2Sxu238th230M1_01", "hTeO2Sxu238th230M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM1_01  = new TH1D("hTeO2Sxth230onlyM1_01", "hTeO2Sxth230onlyM1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M1_01 = new TH1D("hTeO2Sxra226pb210M1_01", "hTeO2Sxra226pb210M1_01", dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM1_0001  = new TH1D("hTeO2Sxth232onlyM1_0001", "hTeO2Sxth232onlyM1_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M1_0001 = new TH1D("hTeO2Sxra228pb208M1_0001", "hTeO2Sxra228pb208M1_0001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M1_0001  = new TH1D("hTeO2Sxu238th230M1_0001", "hTeO2Sxu238th230M1_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM1_0001  = new TH1D("hTeO2Sxth230onlyM1_0001", "hTeO2Sxth230onlyM1_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M1_0001 = new TH1D("hTeO2Sxra226pb210M1_0001", "hTeO2Sxra226pb210M1_0001", dNBins, dMinEnergy, dMaxEnergy);

  hTeO20nuM2       = new TH1D("hTeO20nuM2",    "hTeO20nuM2",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO22nuM2       = new TH1D("hTeO22nuM2",    "hTeO22nuM2",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2co60M2      = new TH1D("hTeO2co60M2",   "hTeO2co60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2k40M2       = new TH1D("hTeO2k40M2",    "hTeO2k40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2pb210M2     = new TH1D("hTeO2pb210M2",  "hTeO2pb210M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2po210M2     = new TH1D("hTeO2po210M2",  "hTeO2po210M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2te125M2     = new TH1D("hTeO2te125M2",  "hTeO2te125M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th232M2     = new TH1D("hTeO2th232M2",  "hTeO2th232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hTeO2th228M2     = new TH1D("hTeO2th228M2",  "hTeO2th228M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra226M2     = new TH1D("hTeO2ra226M2",  "hTeO2ra226M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2rn222M2     = new TH1D("hTeO2rn222M2",  "hTeO2rn222M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u238M2      = new TH1D("hTeO2u238M2",   "hTeO2u238M2",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230M2     = new TH1D("hTeO2th230M2",  "hTeO2th230M2",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u234M2      = new TH1D("hTeO2u234M2",   "hTeO2u234M2",   dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Spb210M2_01      = new TH1D("hTeO2Spb210M2_01",   "hTeO2Spb210M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M2_001     = new TH1D("hTeO2Spo210M2_001",  "hTeO2Spo210M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M2_01      = new TH1D("hTeO2Spo210M2_01",   "hTeO2Spo210M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sth232M2_01      = new TH1D("hTeO2Sth232M2_01",   "hTeO2Sth232M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Su238M2_01       = new TH1D("hTeO2Su238M2_01",    "hTeO2Su238M2_01",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_001    = new TH1D("hTeO2Sxpb210M2_001", "hTeO2Sxpb210M2_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_01     = new TH1D("hTeO2Sxpb210M2_01",  "hTeO2Sxpb210M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_1      = new TH1D("hTeO2Sxpb210M2_1",   "hTeO2Sxpb210M2_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_10     = new TH1D("hTeO2Sxpb210M2_10",  "hTeO2Sxpb210M2_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2_001    = new TH1D("hTeO2Sxpo210M2_001", "hTeO2Sxpo210M2_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2_01     = new TH1D("hTeO2Sxpo210M2_01",  "hTeO2Sxpo210M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2_1      = new TH1D("hTeO2Sxpo210M2_1",   "hTeO2Sxpo210M2_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2_001    = new TH1D("hTeO2Sxth232M2_001", "hTeO2Sxth232M2_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2_01     = new TH1D("hTeO2Sxth232M2_01",  "hTeO2Sxth232M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2_1      = new TH1D("hTeO2Sxth232M2_1",   "hTeO2Sxth232M2_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2_10     = new TH1D("hTeO2Sxth232M2_10",  "hTeO2Sxth232M2_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2_001     = new TH1D("hTeO2Sxu238M2_001",  "hTeO2Sxu238M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2_01      = new TH1D("hTeO2Sxu238M2_01",   "hTeO2Sxu238M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2_1       = new TH1D("hTeO2Sxu238M2_1",    "hTeO2Sxu238M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2_10      = new TH1D("hTeO2Sxu238M2_10",   "hTeO2Sxu238M2_10",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2_100      = new TH1D("hTeO2Sxu238M2_100",   "hTeO2Sxu238M2_100",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2_100     = new TH1D("hTeO2Sxth232M2_100",  "hTeO2Sxth232M2_100",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_100     = new TH1D("hTeO2Sxpb210M2_100",  "hTeO2Sxpb210M2_100",  dNBins, dMinEnergy, dMaxEnergy);

  hTeO2th232onlyM2      = new TH1D("hTeO2th232onlyM2", "hTeO2th232onlyM2",     dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra228pb208M2     = new TH1D("hTeO2ra228pb208M2", "hTeO2ra228pb208M2",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230onlyM2      = new TH1D("hTeO2th230onlyM2", "hTeO2th230onlyM2",     dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2_001  = new TH1D("hTeO2Sxth232onlyM2_001", "hTeO2Sxth232onlyM2_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2_001 = new TH1D("hTeO2Sxra228pb208M2_001", "hTeO2Sxra228pb208M2_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2_001  = new TH1D("hTeO2Sxu238th230M2_001", "hTeO2Sxu238th230M2_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2_001  = new TH1D("hTeO2Sxth230onlyM2_001", "hTeO2Sxth230onlyM2_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2_001 = new TH1D("hTeO2Sxra226pb210M2_001", "hTeO2Sxra226pb210M2_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2_0001     = new TH1D("hTeO2Sxpb210M2_0001", "hTeO2Sxpb210M2_0001",         dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2_01  = new TH1D("hTeO2Sxth232onlyM2_01", "hTeO2Sxth232onlyM2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2_01 = new TH1D("hTeO2Sxra228pb208M2_01", "hTeO2Sxra228pb208M2_01", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2_01  = new TH1D("hTeO2Sxu238th230M2_01", "hTeO2Sxu238th230M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2_01  = new TH1D("hTeO2Sxth230onlyM2_01", "hTeO2Sxth230onlyM2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2_01 = new TH1D("hTeO2Sxra226pb210M2_01", "hTeO2Sxra226pb210M2_01", dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2_0001  = new TH1D("hTeO2Sxth232onlyM2_0001", "hTeO2Sxth232onlyM2_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2_0001 = new TH1D("hTeO2Sxra228pb208M2_0001", "hTeO2Sxra228pb208M2_0001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2_0001  = new TH1D("hTeO2Sxu238th230M2_0001", "hTeO2Sxu238th230M2_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2_0001  = new TH1D("hTeO2Sxth230onlyM2_0001", "hTeO2Sxth230onlyM2_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2_0001 = new TH1D("hTeO2Sxra226pb210M2_0001", "hTeO2Sxra226pb210M2_0001", dNBins, dMinEnergy, dMaxEnergy);

  hTeO20nuM2Sum       = new TH1D("hTeO20nuM2Sum",    "hTeO20nuM2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO22nuM2Sum       = new TH1D("hTeO22nuM2Sum",    "hTeO22nuM2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2co60M2Sum      = new TH1D("hTeO2co60M2Sum",   "hTeO2co60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2k40M2Sum       = new TH1D("hTeO2k40M2Sum",    "hTeO2k40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2pb210M2Sum     = new TH1D("hTeO2pb210M2Sum",  "hTeO2pb210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2po210M2Sum     = new TH1D("hTeO2po210M2Sum",  "hTeO2po210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2te125M2Sum     = new TH1D("hTeO2te125M2Sum",  "hTeO2te125M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th232M2Sum     = new TH1D("hTeO2th232M2Sum",  "hTeO2th232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hTeO2th228M2Sum     = new TH1D("hTeO2th228M2Sum",  "hTeO2th228M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra226M2Sum     = new TH1D("hTeO2ra226M2Sum",  "hTeO2ra226M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2rn222M2Sum     = new TH1D("hTeO2rn222M2Sum",  "hTeO2rn222M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u238M2Sum      = new TH1D("hTeO2u238M2Sum",   "hTeO2u238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230M2Sum     = new TH1D("hTeO2th230M2Sum",  "hTeO2th230M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2u234M2Sum      = new TH1D("hTeO2u234M2Sum",   "hTeO2u234M2Sum",   dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Spb210M2Sum_01      = new TH1D("hTeO2Spb210M2Sum_01",   "hTeO2Spb210M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M2Sum_001     = new TH1D("hTeO2Spo210M2Sum_001",  "hTeO2Spo210M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Spo210M2Sum_01      = new TH1D("hTeO2Spo210M2Sum_01",   "hTeO2Spo210M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sth232M2Sum_01      = new TH1D("hTeO2Sth232M2Sum_01",   "hTeO2Sth232M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Su238M2Sum_01       = new TH1D("hTeO2Su238M2Sum_01",    "hTeO2Su238M2Sum_01",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_001    = new TH1D("hTeO2Sxpb210M2Sum_001", "hTeO2Sxpb210M2Sum_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_01     = new TH1D("hTeO2Sxpb210M2Sum_01",  "hTeO2Sxpb210M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_1      = new TH1D("hTeO2Sxpb210M2Sum_1",   "hTeO2Sxpb210M2Sum_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_10     = new TH1D("hTeO2Sxpb210M2Sum_10",  "hTeO2Sxpb210M2Sum_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2Sum_001    = new TH1D("hTeO2Sxpo210M2Sum_001", "hTeO2Sxpo210M2Sum_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2Sum_01     = new TH1D("hTeO2Sxpo210M2Sum_01",  "hTeO2Sxpo210M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpo210M2Sum_1      = new TH1D("hTeO2Sxpo210M2Sum_1",   "hTeO2Sxpo210M2Sum_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2Sum_001    = new TH1D("hTeO2Sxth232M2Sum_001", "hTeO2Sxth232M2Sum_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2Sum_01     = new TH1D("hTeO2Sxth232M2Sum_01",  "hTeO2Sxth232M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2Sum_1      = new TH1D("hTeO2Sxth232M2Sum_1",   "hTeO2Sxth232M2Sum_1",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2Sum_10     = new TH1D("hTeO2Sxth232M2Sum_10",  "hTeO2Sxth232M2Sum_10",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2Sum_001     = new TH1D("hTeO2Sxu238M2Sum_001",  "hTeO2Sxu238M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2Sum_01      = new TH1D("hTeO2Sxu238M2Sum_01",   "hTeO2Sxu238M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2Sum_1       = new TH1D("hTeO2Sxu238M2Sum_1",    "hTeO2Sxu238M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2Sum_10      = new TH1D("hTeO2Sxu238M2Sum_10",   "hTeO2Sxu238M2Sum_10",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238M2Sum_100      = new TH1D("hTeO2Sxu238M2Sum_100",   "hTeO2Sxu238M2Sum_100",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth232M2Sum_100     = new TH1D("hTeO2Sxth232M2Sum_100",  "hTeO2Sxth232M2Sum_100",  dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_100     = new TH1D("hTeO2Sxpb210M2Sum_100",  "hTeO2Sxpb210M2Sum_100",  dNBins, dMinEnergy, dMaxEnergy);

  hTeO2th232onlyM2Sum      = new TH1D("hTeO2th232onlyM2Sum", "hTeO2th232onlyM2Sum",     dNBins, dMinEnergy, dMaxEnergy);
  hTeO2ra228pb208M2Sum     = new TH1D("hTeO2ra228pb208M2Sum", "hTeO2ra228pb208M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2th230onlyM2Sum      = new TH1D("hTeO2th230onlyM2Sum", "hTeO2th230onlyM2Sum",     dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2Sum_001  = new TH1D("hTeO2Sxth232onlyM2Sum_001", "hTeO2Sxth232onlyM2Sum_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2Sum_001 = new TH1D("hTeO2Sxra228pb208M2Sum_001", "hTeO2Sxra228pb208M2Sum_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2Sum_001  = new TH1D("hTeO2Sxu238th230M2Sum_001", "hTeO2Sxu238th230M2Sum_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2Sum_001  = new TH1D("hTeO2Sxth230onlyM2Sum_001", "hTeO2Sxth230onlyM2Sum_001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2Sum_001 = new TH1D("hTeO2Sxra226pb210M2Sum_001", "hTeO2Sxra226pb210M2Sum_001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxpb210M2Sum_0001     = new TH1D("hTeO2Sxpb210M2Sum_0001", "hTeO2Sxpb210M2Sum_0001",         dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2Sum_01  = new TH1D("hTeO2Sxth232onlyM2Sum_01", "hTeO2Sxth232onlyM2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2Sum_01 = new TH1D("hTeO2Sxra228pb208M2Sum_01", "hTeO2Sxra228pb208M2Sum_01", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2Sum_01  = new TH1D("hTeO2Sxu238th230M2Sum_01", "hTeO2Sxu238th230M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2Sum_01  = new TH1D("hTeO2Sxth230onlyM2Sum_01", "hTeO2Sxth230onlyM2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2Sum_01 = new TH1D("hTeO2Sxra226pb210M2Sum_01", "hTeO2Sxra226pb210M2Sum_01", dNBins, dMinEnergy, dMaxEnergy);

  hTeO2Sxth232onlyM2Sum_0001  = new TH1D("hTeO2Sxth232onlyM2Sum_0001", "hTeO2Sxth232onlyM2Sum_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra228pb208M2Sum_0001 = new TH1D("hTeO2Sxra228pb208M2Sum_0001", "hTeO2Sxra228pb208M2Sum_0001", dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxu238th230M2Sum_0001  = new TH1D("hTeO2Sxu238th230M2Sum_0001", "hTeO2Sxu238th230M2Sum_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxth230onlyM2Sum_0001  = new TH1D("hTeO2Sxth230onlyM2Sum_0001", "hTeO2Sxth230onlyM2Sum_0001",   dNBins, dMinEnergy, dMaxEnergy);
  hTeO2Sxra226pb210M2Sum_0001 = new TH1D("hTeO2Sxra226pb210M2Sum_0001", "hTeO2Sxra226pb210M2Sum_0001", dNBins, dMinEnergy, dMaxEnergy);

////////////// Frame M1 and M2
  hCuFrameco58M1      = new TH1D("hCuFrameco58M1",   "hCuFrameco58M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameco60M1      = new TH1D("hCuFrameco60M1",   "hCuFrameco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramecs137M1     = new TH1D("hCuFramecs137M1",  "hCuFramecs137M1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFramek40M1       = new TH1D("hCuFramek40M1",    "hCuFramek40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFramemn54M1      = new TH1D("hCuFramemn54M1",   "hCuFramemn54M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramepb210M1     = new TH1D("hCuFramepb210M1",  "hCuFramepb210M1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameth232M1     = new TH1D("hCuFrameth232M1",  "hCuFrameth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuFrameu238M1      = new TH1D("hCuFrameu238M1",   "hCuFrameu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hCuFrameSth232M1_1     = new TH1D("hCuFrameSth232M1_1",    "hCuFrameSth232M1_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSu238M1_1      = new TH1D("hCuFrameSu238M1_1",     "hCuFrameSu238M1_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M1_001  = new TH1D("hCuFrameSxpb210M1_001", "hCuFrameSxpb210M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M1_01   = new TH1D("hCuFrameSxpb210M1_01",  "hCuFrameSxpb210M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M1_1    = new TH1D("hCuFrameSxpb210M1_1",   "hCuFrameSxpb210M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M1_10   = new TH1D("hCuFrameSxpb210M1_10",  "hCuFrameSxpb210M1_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M1_001  = new TH1D("hCuFrameSxth232M1_001", "hCuFrameSxth232M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M1_01   = new TH1D("hCuFrameSxth232M1_01",  "hCuFrameSxth232M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M1_1    = new TH1D("hCuFrameSxth232M1_1",   "hCuFrameSxth232M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M1_10   = new TH1D("hCuFrameSxth232M1_10",  "hCuFrameSxth232M1_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M1_001   = new TH1D("hCuFrameSxu238M1_001",  "hCuFrameSxu238M1_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M1_01    = new TH1D("hCuFrameSxu238M1_01",   "hCuFrameSxu238M1_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M1_1     = new TH1D("hCuFrameSxu238M1_1",    "hCuFrameSxu238M1_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M1_10    = new TH1D("hCuFrameSxu238M1_10",   "hCuFrameSxu238M1_10",    dNBins, dMinEnergy, dMaxEnergy);


  hCuFrameco58M2      = new TH1D("hCuFrameco58M2",   "hCuFrameco58M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameco60M2      = new TH1D("hCuFrameco60M2",   "hCuFrameco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramecs137M2     = new TH1D("hCuFramecs137M2",  "hCuFramecs137M2",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFramek40M2       = new TH1D("hCuFramek40M2",    "hCuFramek40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFramemn54M2      = new TH1D("hCuFramemn54M2",   "hCuFramemn54M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramepb210M2     = new TH1D("hCuFramepb210M2",  "hCuFramepb210M2",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameth232M2     = new TH1D("hCuFrameth232M2",  "hCuFrameth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuFrameu238M2      = new TH1D("hCuFrameu238M2",   "hCuFrameu238M2",   dNBins, dMinEnergy, dMaxEnergy);

  hCuFrameSth232M2_1     = new TH1D("hCuFrameSth232M2_1",    "hCuFrameSth232M2_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSu238M2_1      = new TH1D("hCuFrameSu238M2_1",     "hCuFrameSu238M2_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2_001  = new TH1D("hCuFrameSxpb210M2_001", "hCuFrameSxpb210M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2_01   = new TH1D("hCuFrameSxpb210M2_01",  "hCuFrameSxpb210M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2_1    = new TH1D("hCuFrameSxpb210M2_1",   "hCuFrameSxpb210M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2_10   = new TH1D("hCuFrameSxpb210M2_10",  "hCuFrameSxpb210M2_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2_001  = new TH1D("hCuFrameSxth232M2_001", "hCuFrameSxth232M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2_01   = new TH1D("hCuFrameSxth232M2_01",  "hCuFrameSxth232M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2_1    = new TH1D("hCuFrameSxth232M2_1",   "hCuFrameSxth232M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2_10   = new TH1D("hCuFrameSxth232M2_10",  "hCuFrameSxth232M2_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2_001   = new TH1D("hCuFrameSxu238M2_001",  "hCuFrameSxu238M2_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2_01    = new TH1D("hCuFrameSxu238M2_01",   "hCuFrameSxu238M2_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2_1     = new TH1D("hCuFrameSxu238M2_1",    "hCuFrameSxu238M2_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2_10    = new TH1D("hCuFrameSxu238M2_10",   "hCuFrameSxu238M2_10",    dNBins, dMinEnergy, dMaxEnergy);

  hCuFrameco58M2Sum      = new TH1D("hCuFrameco58M2Sum",   "hCuFrameco58M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameco60M2Sum      = new TH1D("hCuFrameco60M2Sum",   "hCuFrameco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramecs137M2Sum     = new TH1D("hCuFramecs137M2Sum",  "hCuFramecs137M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFramek40M2Sum       = new TH1D("hCuFramek40M2Sum",    "hCuFramek40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFramemn54M2Sum      = new TH1D("hCuFramemn54M2Sum",   "hCuFramemn54M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFramepb210M2Sum     = new TH1D("hCuFramepb210M2Sum",  "hCuFramepb210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameth232M2Sum     = new TH1D("hCuFrameth232M2Sum",  "hCuFrameth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuFrameu238M2Sum      = new TH1D("hCuFrameu238M2Sum",   "hCuFrameu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);

  hCuFrameSth232M2Sum_1     = new TH1D("hCuFrameSth232M2Sum_1",    "hCuFrameSth232M2Sum_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSu238M2Sum_1      = new TH1D("hCuFrameSu238M2Sum_1",     "hCuFrameSu238M2Sum_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2Sum_001  = new TH1D("hCuFrameSxpb210M2Sum_001", "hCuFrameSxpb210M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2Sum_01   = new TH1D("hCuFrameSxpb210M2Sum_01",  "hCuFrameSxpb210M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2Sum_1    = new TH1D("hCuFrameSxpb210M2Sum_1",   "hCuFrameSxpb210M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxpb210M2Sum_10   = new TH1D("hCuFrameSxpb210M2Sum_10",  "hCuFrameSxpb210M2Sum_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2Sum_001  = new TH1D("hCuFrameSxth232M2Sum_001", "hCuFrameSxth232M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2Sum_01   = new TH1D("hCuFrameSxth232M2Sum_01",  "hCuFrameSxth232M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2Sum_1    = new TH1D("hCuFrameSxth232M2Sum_1",   "hCuFrameSxth232M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxth232M2Sum_10   = new TH1D("hCuFrameSxth232M2Sum_10",  "hCuFrameSxth232M2Sum_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2Sum_001   = new TH1D("hCuFrameSxu238M2Sum_001",  "hCuFrameSxu238M2Sum_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2Sum_01    = new TH1D("hCuFrameSxu238M2Sum_01",   "hCuFrameSxu238M2Sum_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2Sum_1     = new TH1D("hCuFrameSxu238M2Sum_1",    "hCuFrameSxu238M2Sum_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuFrameSxu238M2Sum_10    = new TH1D("hCuFrameSxu238M2Sum_10",   "hCuFrameSxu238M2Sum_10",    dNBins, dMinEnergy, dMaxEnergy);

/////////// CuBox (TShield) M1 and M2
  hCuBoxco58M1      = new TH1D("hCuBoxco58M1",   "hCuBoxco58M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxco60M1      = new TH1D("hCuBoxco60M1",   "hCuBoxco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxcs137M1     = new TH1D("hCuBoxcs137M1",  "hCuBoxcs137M1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxk40M1       = new TH1D("hCuBoxk40M1",    "hCuBoxk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxmn54M1      = new TH1D("hCuBoxmn54M1",   "hCuBoxmn54M1",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxpb210M1     = new TH1D("hCuBoxpb210M1",  "hCuBoxpb210M1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxth232M1     = new TH1D("hCuBoxth232M1",  "hCuBoxth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuBoxu238M1      = new TH1D("hCuBoxu238M1",   "hCuBoxu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hCuBoxSth232M1_1     = new TH1D("hCuBoxSth232M1_1",    "hCuBoxSth232M1_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSu238M1_1      = new TH1D("hCuBoxSu238M1_1",     "hCuBoxSu238M1_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M1_001  = new TH1D("hCuBoxSxpb210M1_001", "hCuBoxSxpb210M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M1_01   = new TH1D("hCuBoxSxpb210M1_01",  "hCuBoxSxpb210M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M1_1    = new TH1D("hCuBoxSxpb210M1_1",   "hCuBoxSxpb210M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M1_10   = new TH1D("hCuBoxSxpb210M1_10",  "hCuBoxSxpb210M1_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M1_001  = new TH1D("hCuBoxSxth232M1_001", "hCuBoxSxth232M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M1_01   = new TH1D("hCuBoxSxth232M1_01",  "hCuBoxSxth232M1_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M1_1    = new TH1D("hCuBoxSxth232M1_1",   "hCuBoxSxth232M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M1_10   = new TH1D("hCuBoxSxth232M1_10",  "hCuBoxSxth232M1_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M1_001   = new TH1D("hCuBoxSxu238M1_001",  "hCuBoxSxu238M1_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M1_01    = new TH1D("hCuBoxSxu238M1_01",   "hCuBoxSxu238M1_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M1_1     = new TH1D("hCuBoxSxu238M1_1",    "hCuBoxSxu238M1_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M1_10    = new TH1D("hCuBoxSxu238M1_10",   "hCuBoxSxu238M1_10",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBoxco58M2      = new TH1D("hCuBoxco58M2",   "hCuBoxco58M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxco60M2      = new TH1D("hCuBoxco60M2",   "hCuBoxco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxcs137M2     = new TH1D("hCuBoxcs137M2",  "hCuBoxcs137M2",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxk40M2       = new TH1D("hCuBoxk40M2",    "hCuBoxk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxmn54M2      = new TH1D("hCuBoxmn54M2",   "hCuBoxmn54M2",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxpb210M2     = new TH1D("hCuBoxpb210M2",  "hCuBoxpb210M2",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxth232M2     = new TH1D("hCuBoxth232M2",  "hCuBoxth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuBoxu238M2      = new TH1D("hCuBoxu238M2",   "hCuBoxu238M2",   dNBins, dMinEnergy, dMaxEnergy);

  hCuBoxSth232M2_1     = new TH1D("hCuBoxSth232M2_1",    "hCuBoxSth232M2_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSu238M2_1      = new TH1D("hCuBoxSu238M2_1",     "hCuBoxSu238M2_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2_001  = new TH1D("hCuBoxSxpb210M2_001", "hCuBoxSxpb210M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2_01   = new TH1D("hCuBoxSxpb210M2_01",  "hCuBoxSxpb210M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2_1    = new TH1D("hCuBoxSxpb210M2_1",   "hCuBoxSxpb210M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2_10   = new TH1D("hCuBoxSxpb210M2_10",  "hCuBoxSxpb210M2_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2_001  = new TH1D("hCuBoxSxth232M2_001", "hCuBoxSxth232M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2_01   = new TH1D("hCuBoxSxth232M2_01",  "hCuBoxSxth232M2_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2_1    = new TH1D("hCuBoxSxth232M2_1",   "hCuBoxSxth232M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2_10   = new TH1D("hCuBoxSxth232M2_10",  "hCuBoxSxth232M2_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2_001   = new TH1D("hCuBoxSxu238M2_001",  "hCuBoxSxu238M2_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2_01    = new TH1D("hCuBoxSxu238M2_01",   "hCuBoxSxu238M2_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2_1     = new TH1D("hCuBoxSxu238M2_1",    "hCuBoxSxu238M2_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2_10    = new TH1D("hCuBoxSxu238M2_10",   "hCuBoxSxu238M2_10",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBoxco58M2Sum      = new TH1D("hCuBoxco58M2Sum",   "hCuBoxco58M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxco60M2Sum      = new TH1D("hCuBoxco60M2Sum",   "hCuBoxco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxcs137M2Sum     = new TH1D("hCuBoxcs137M2Sum",  "hCuBoxcs137M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxk40M2Sum       = new TH1D("hCuBoxk40M2Sum",    "hCuBoxk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxmn54M2Sum      = new TH1D("hCuBoxmn54M2Sum",   "hCuBoxmn54M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxpb210M2Sum     = new TH1D("hCuBoxpb210M2Sum",  "hCuBoxpb210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxth232M2Sum     = new TH1D("hCuBoxth232M2Sum",  "hCuBoxth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hCuBoxu238M2Sum      = new TH1D("hCuBoxu238M2Sum",   "hCuBoxu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);

  hCuBoxSth232M2Sum_1     = new TH1D("hCuBoxSth232M2Sum_1",    "hCuBoxSth232M2Sum_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSu238M2Sum_1      = new TH1D("hCuBoxSu238M2Sum_1",     "hCuBoxSu238M2Sum_1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2Sum_001  = new TH1D("hCuBoxSxpb210M2Sum_001", "hCuBoxSxpb210M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2Sum_01   = new TH1D("hCuBoxSxpb210M2Sum_01",  "hCuBoxSxpb210M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2Sum_1    = new TH1D("hCuBoxSxpb210M2Sum_1",   "hCuBoxSxpb210M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxpb210M2Sum_10   = new TH1D("hCuBoxSxpb210M2Sum_10",  "hCuBoxSxpb210M2Sum_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2Sum_001  = new TH1D("hCuBoxSxth232M2Sum_001", "hCuBoxSxth232M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2Sum_01   = new TH1D("hCuBoxSxth232M2Sum_01",  "hCuBoxSxth232M2Sum_01",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2Sum_1    = new TH1D("hCuBoxSxth232M2Sum_1",   "hCuBoxSxth232M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxth232M2Sum_10   = new TH1D("hCuBoxSxth232M2Sum_10",  "hCuBoxSxth232M2Sum_10",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2Sum_001   = new TH1D("hCuBoxSxu238M2Sum_001",  "hCuBoxSxu238M2Sum_001",   dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2Sum_01    = new TH1D("hCuBoxSxu238M2Sum_01",   "hCuBoxSxu238M2Sum_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2Sum_1     = new TH1D("hCuBoxSxu238M2Sum_1",    "hCuBoxSxu238M2Sum_1",     dNBins, dMinEnergy, dMaxEnergy);
  hCuBoxSxu238M2Sum_10    = new TH1D("hCuBoxSxu238M2Sum_10",   "hCuBoxSxu238M2Sum_10",    dNBins, dMinEnergy, dMaxEnergy);

/////////// CuBox + CuFrame M1 and M2
  hCuBox_CuFrameco60M1      = new TH1D("hCuBox_CuFrameco60M1", "hCuBox_CuFrameco60M1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramek40M1       = new TH1D("hCuBox_CuFramek40M1", "hCuBox_CuFramek40M1",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M1     = new TH1D("hCuBox_CuFrameth232M1", "hCuBox_CuFrameth232M1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M1      = new TH1D("hCuBox_CuFrameu238M1", "hCuBox_CuFrameu238M1",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M1_10  = new TH1D("hCuBox_CuFrameth232M1_10", "hCuBox_CuFrameth232M1_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M1_10   = new TH1D("hCuBox_CuFrameu238M1_10", "hCuBox_CuFrameu238M1_10",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M1_10  = new TH1D("hCuBox_CuFramepb210M1_10", "hCuBox_CuFramepb210M1_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M1_1   = new TH1D("hCuBox_CuFramepb210M1_1", "hCuBox_CuFramepb210M1_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M1_01  = new TH1D("hCuBox_CuFramepb210M1_01", "hCuBox_CuFramepb210M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M1_001 = new TH1D("hCuBox_CuFramepb210M1_001", "hCuBox_CuFramepb210M1_001",  dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M1_1  = new TH1D("hCuBox_CuFrameth232M1_1", "hCuBox_CuFrameth232M1_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M1_1   = new TH1D("hCuBox_CuFrameu238M1_1", "hCuBox_CuFrameu238M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M1_01  = new TH1D("hCuBox_CuFrameth232M1_01", "hCuBox_CuFrameth232M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M1_01   = new TH1D("hCuBox_CuFrameu238M1_01", "hCuBox_CuFrameu238M1_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M1_001  = new TH1D("hCuBox_CuFrameth232M1_001", "hCuBox_CuFrameth232M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M1_001   = new TH1D("hCuBox_CuFrameu238M1_001", "hCuBox_CuFrameu238M1_001",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameco60M2      = new TH1D("hCuBox_CuFrameco60M2", "hCuBox_CuFrameco60M2",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramek40M2       = new TH1D("hCuBox_CuFramek40M2", "hCuBox_CuFramek40M2",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2     = new TH1D("hCuBox_CuFrameth232M2", "hCuBox_CuFrameth232M2",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2      = new TH1D("hCuBox_CuFrameu238M2", "hCuBox_CuFrameu238M2",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M2_10  = new TH1D("hCuBox_CuFrameth232M2_10", "hCuBox_CuFrameth232M2_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2_10   = new TH1D("hCuBox_CuFrameu238M2_10", "hCuBox_CuFrameu238M2_10",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2_10  = new TH1D("hCuBox_CuFramepb210M2_10", "hCuBox_CuFramepb210M2_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2_1   = new TH1D("hCuBox_CuFramepb210M2_1", "hCuBox_CuFramepb210M2_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2_01  = new TH1D("hCuBox_CuFramepb210M2_01", "hCuBox_CuFramepb210M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2_001 = new TH1D("hCuBox_CuFramepb210M2_001", "hCuBox_CuFramepb210M2_001",  dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M2_1  = new TH1D("hCuBox_CuFrameth232M2_1", "hCuBox_CuFrameth232M2_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2_1   = new TH1D("hCuBox_CuFrameu238M2_1", "hCuBox_CuFrameu238M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2_01  = new TH1D("hCuBox_CuFrameth232M2_01", "hCuBox_CuFrameth232M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2_01   = new TH1D("hCuBox_CuFrameu238M2_01", "hCuBox_CuFrameu238M2_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2_001  = new TH1D("hCuBox_CuFrameth232M2_001", "hCuBox_CuFrameth232M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2_001   = new TH1D("hCuBox_CuFrameu238M2_001", "hCuBox_CuFrameu238M2_001",    dNBins, dMinEnergy, dMaxEnergy);


  hCuBox_CuFrameco60M2Sum   = new TH1D("hCuBox_CuFrameco60M2Sum", "hCuBox_CuFrameco60M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramek40M2Sum    = new TH1D("hCuBox_CuFramek40M2Sum", "hCuBox_CuFramek40M2Sum",      dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2Sum  = new TH1D("hCuBox_CuFrameth232M2Sum", "hCuBox_CuFrameth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2Sum   = new TH1D("hCuBox_CuFrameu238M2Sum", "hCuBox_CuFrameu238M2Sum",    dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M2Sum_10   = new TH1D("hCuBox_CuFrameth232M2Sum_10", "hCuBox_CuFrameth232M2Sum_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2Sum_10    = new TH1D("hCuBox_CuFrameu238M2Sum_10", "hCuBox_CuFrameu238M2Sum_10",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2Sum_10   = new TH1D("hCuBox_CuFramepb210M2Sum_10", "hCuBox_CuFramepb210M2Sum_10",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2Sum_1   = new TH1D("hCuBox_CuFramepb210M2Sum_1", "hCuBox_CuFramepb210M2Sum_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2Sum_01  = new TH1D("hCuBox_CuFramepb210M2Sum_01", "hCuBox_CuFramepb210M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFramepb210M2Sum_001 = new TH1D("hCuBox_CuFramepb210M2Sum_001", "hCuBox_CuFramepb210M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);

  hCuBox_CuFrameth232M2Sum_1  = new TH1D("hCuBox_CuFrameth232M2Sum_1", "hCuBox_CuFrameth232M2Sum_1",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2Sum_1   = new TH1D("hCuBox_CuFrameu238M2Sum_1", "hCuBox_CuFrameu238M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2Sum_01  = new TH1D("hCuBox_CuFrameth232M2Sum_01", "hCuBox_CuFrameth232M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2Sum_01   = new TH1D("hCuBox_CuFrameu238M2Sum_01", "hCuBox_CuFrameu238M2Sum_01",    dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameth232M2Sum_001  = new TH1D("hCuBox_CuFrameth232M2Sum_001", "hCuBox_CuFrameth232M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hCuBox_CuFrameu238M2Sum_001   = new TH1D("hCuBox_CuFrameu238M2Sum_001", "hCuBox_CuFrameu238M2Sum_001",    dNBins, dMinEnergy, dMaxEnergy);

/////////// 50mK M1 and M2
  h50mKco58M1      = new TH1D("h50mKco58M1",   "h50mKco58M1",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKco60M1      = new TH1D("h50mKco60M1",   "h50mKco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKcs137M1     = new TH1D("h50mKcs137M1",  "h50mKcs137M1",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKk40M1       = new TH1D("h50mKk40M1",    "h50mKk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  h50mKmn54M1      = new TH1D("h50mKmn54M1",   "h50mKmn54M1",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKpb210M1     = new TH1D("h50mKpb210M1",  "h50mKpb210M1",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKth232M1     = new TH1D("h50mKth232M1",  "h50mKth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  h50mKu238M1      = new TH1D("h50mKu238M1",   "h50mKu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  h50mKco58M2      = new TH1D("h50mKco58M2",   "h50mKco58M2",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKco60M2      = new TH1D("h50mKco60M2",   "h50mKco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKcs137M2     = new TH1D("h50mKcs137M2",  "h50mKcs137M2",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKk40M2       = new TH1D("h50mKk40M2",    "h50mKk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  h50mKmn54M2      = new TH1D("h50mKmn54M2",   "h50mKmn54M2",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKpb210M2     = new TH1D("h50mKpb210M2",  "h50mKpb210M2",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKth232M2     = new TH1D("h50mKth232M2",  "h50mKth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  h50mKu238M2      = new TH1D("h50mKu238M2",   "h50mKu238M2",   dNBins, dMinEnergy, dMaxEnergy);

  h50mKco58M2Sum      = new TH1D("h50mKco58M2Sum",   "h50mKco58M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKco60M2Sum      = new TH1D("h50mKco60M2Sum",   "h50mKco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKcs137M2Sum     = new TH1D("h50mKcs137M2Sum",  "h50mKcs137M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKk40M2Sum       = new TH1D("h50mKk40M2Sum",    "h50mKk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  h50mKmn54M2Sum      = new TH1D("h50mKmn54M2Sum",   "h50mKmn54M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  h50mKpb210M2Sum     = new TH1D("h50mKpb210M2Sum",  "h50mKpb210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  h50mKth232M2Sum     = new TH1D("h50mKth232M2Sum",  "h50mKth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  h50mKu238M2Sum      = new TH1D("h50mKu238M2Sum",   "h50mKu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);

/////////// 600mK M1 and M2
  h600mKco60M1      = new TH1D("h600mKco60M1",   "h600mKco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  h600mKk40M1       = new TH1D("h600mKk40M1",    "h600mKk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  h600mKth232M1     = new TH1D("h600mKth232M1",  "h600mKth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  h600mKu238M1      = new TH1D("h600mKu238M1",   "h600mKu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  h600mKco60M2      = new TH1D("h600mKco60M2",   "h600mKco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  h600mKk40M2       = new TH1D("h600mKk40M2",    "h600mKk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  h600mKth232M2     = new TH1D("h600mKth232M2",  "h600mKth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  h600mKu238M2      = new TH1D("h600mKu238M2",   "h600mKu238M2",   dNBins, dMinEnergy, dMaxEnergy);  

  h600mKco60M2Sum      = new TH1D("h600mKco60M2Sum",   "h600mKco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  h600mKk40M2Sum       = new TH1D("h600mKk40M2Sum",    "h600mKk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  h600mKth232M2Sum     = new TH1D("h600mKth232M2Sum",  "h600mKth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  h600mKu238M2Sum      = new TH1D("h600mKu238M2Sum",   "h600mKu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy); 

/////////// Internal Shields M1 and M2
  hInternalco60M1   = new TH1D("hInternalco60M1", "hInternalco60M1",    dNBins, dMinEnergy, dMaxEnergy);
  hInternalk40M1    = new TH1D("hInternalk40M1", "hInternalk40M1",      dNBins, dMinEnergy, dMaxEnergy);
  hInternalth232M1  = new TH1D("hInternalth232M1", "hInternalth232M1",  dNBins, dMinEnergy, dMaxEnergy);
  hInternalu238M1   = new TH1D("hInternalu238M1", "hInternalu238M1",    dNBins, dMinEnergy, dMaxEnergy);

  hInternalco60M2   = new TH1D("hInternalco60M2", "hInternalco60M2",    dNBins, dMinEnergy, dMaxEnergy);
  hInternalk40M2    = new TH1D("hInternalk40M2", "hInternalk40M2",      dNBins, dMinEnergy, dMaxEnergy);
  hInternalth232M2  = new TH1D("hInternalth232M2", "hInternalth232M2",  dNBins, dMinEnergy, dMaxEnergy);
  hInternalu238M2   = new TH1D("hInternalu238M2", "hInternalu238M2",    dNBins, dMinEnergy, dMaxEnergy);

  hInternalco60M2Sum  = new TH1D("hInternalco60M2Sum", "hInternalco60M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hInternalk40M2Sum   = new TH1D("hInternalk40M2Sum", "hInternalk40M2Sum",      dNBins, dMinEnergy, dMaxEnergy);
  hInternalth232M2Sum = new TH1D("hInternalth232M2Sum", "hInternalth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);
  hInternalu238M2Sum  = new TH1D("hInternalu238M2Sum", "hInternalu238M2Sum",    dNBins, dMinEnergy, dMaxEnergy);


////////// Roman Lead M1 and M2
  hPbRomco60M1      = new TH1D("hPbRomco60M1",   "hPbRomco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hPbRomcs137M1     = new TH1D("hPbRomcs137M1",  "hPbRomcs137M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomk40M1       = new TH1D("hPbRomk40M1",    "hPbRomk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hPbRompb210M1     = new TH1D("hPbRompb210M1",  "hPbRompb210M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomth232M1     = new TH1D("hPbRomth232M1",  "hPbRomth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomu238M1      = new TH1D("hPbRomu238M1",   "hPbRomu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hPbRomco60M2      = new TH1D("hPbRomco60M2",   "hPbRomco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hPbRomcs137M2     = new TH1D("hPbRomcs137M2",  "hPbRomcs137M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomk40M2       = new TH1D("hPbRomk40M2",    "hPbRomk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hPbRompb210M2     = new TH1D("hPbRompb210M2",  "hPbRompb210M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomth232M2     = new TH1D("hPbRomth232M2",  "hPbRomth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomu238M2      = new TH1D("hPbRomu238M2",   "hPbRomu238M2",   dNBins, dMinEnergy, dMaxEnergy);

  hPbRomco60M2Sum      = new TH1D("hPbRomco60M2Sum",   "hPbRomco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hPbRomcs137M2Sum     = new TH1D("hPbRomcs137M2Sum",  "hPbRomcs137M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomk40M2Sum       = new TH1D("hPbRomk40M2Sum",    "hPbRomk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hPbRompb210M2Sum     = new TH1D("hPbRompb210M2Sum",  "hPbRompb210M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomth232M2Sum     = new TH1D("hPbRomth232M2Sum",  "hPbRomth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hPbRomu238M2Sum      = new TH1D("hPbRomu238M2Sum",   "hPbRomu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);

///////////// Main bath M1 and M2
  hMBco60M1      = new TH1D("hMBco60M1",   "hMBco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hMBk40M1       = new TH1D("hMBk40M1",    "hMBk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hMBth232M1     = new TH1D("hMBth232M1",  "hMBth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hMBu238M1      = new TH1D("hMBu238M1",   "hMBu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hMBco60M2      = new TH1D("hMBco60M2",   "hMBco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hMBk40M2       = new TH1D("hMBk40M2",    "hMBk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hMBth232M2     = new TH1D("hMBth232M2",  "hMBth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hMBu238M2      = new TH1D("hMBu238M2",   "hMBu238M2",   dNBins, dMinEnergy, dMaxEnergy);  

  hMBco60M2Sum      = new TH1D("hMBco60M2Sum",   "hMBco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hMBk40M2Sum       = new TH1D("hMBk40M2Sum",    "hMBk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hMBth232M2Sum     = new TH1D("hMBth232M2Sum",  "hMBth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hMBu238M2Sum      = new TH1D("hMBu238M2Sum",   "hMBu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);  

///////// Super Insulation M1 and M2
  hSIk40M1       = new TH1D("hSIk40M1",    "hSIk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hSIth232M1     = new TH1D("hSIth232M1",  "hSIth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hSIu238M1      = new TH1D("hSIu238M1",   "hSIu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hSIk40M2       = new TH1D("hSIk40M2",    "hSIk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hSIth232M2     = new TH1D("hSIth232M2",  "hSIth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hSIu238M2      = new TH1D("hSIu238M2",   "hSIu238M2",   dNBins, dMinEnergy, dMaxEnergy);


///////// IVC M1 and M2
  hIVCco60M1      = new TH1D("hIVCco60M1",   "hIVCco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hIVCk40M1       = new TH1D("hIVCk40M1",    "hIVCk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hIVCth232M1     = new TH1D("hIVCth232M1",  "hIVCth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hIVCu238M1      = new TH1D("hIVCu238M1",   "hIVCu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hIVCco60M2Sum      = new TH1D("hIVCco60M2Sum",   "hIVCco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hIVCk40M2Sum       = new TH1D("hIVCk40M2Sum",    "hIVCk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hIVCth232M2Sum     = new TH1D("hIVCth232M2Sum",  "hIVCth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hIVCu238M2Sum      = new TH1D("hIVCu238M2Sum",   "hIVCu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);  

///////// OVC M1 and M2
  hOVCco60M1      = new TH1D("hOVCco60M1",   "hOVCco60M1",   dNBins, dMinEnergy, dMaxEnergy);
  hOVCk40M1       = new TH1D("hOVCk40M1",    "hOVCk40M1",    dNBins, dMinEnergy, dMaxEnergy);
  hOVCth232M1     = new TH1D("hOVCth232M1",  "hOVCth232M1",  dNBins, dMinEnergy, dMaxEnergy);  
  hOVCu238M1      = new TH1D("hOVCu238M1",   "hOVCu238M1",   dNBins, dMinEnergy, dMaxEnergy);

  hOVCco60M2      = new TH1D("hOVCco60M2",   "hOVCco60M2",   dNBins, dMinEnergy, dMaxEnergy);
  hOVCk40M2       = new TH1D("hOVCk40M2",    "hOVCk40M2",    dNBins, dMinEnergy, dMaxEnergy);
  hOVCth232M2     = new TH1D("hOVCth232M2",  "hOVCth232M2",  dNBins, dMinEnergy, dMaxEnergy);  
  hOVCu238M2      = new TH1D("hOVCu238M2",   "hOVCu238M2",   dNBins, dMinEnergy, dMaxEnergy);  

  hOVCco60M2Sum      = new TH1D("hOVCco60M2Sum",   "hOVCco60M2Sum",   dNBins, dMinEnergy, dMaxEnergy);
  hOVCk40M2Sum       = new TH1D("hOVCk40M2Sum",    "hOVCk40M2Sum",    dNBins, dMinEnergy, dMaxEnergy);
  hOVCth232M2Sum     = new TH1D("hOVCth232M2Sum",  "hOVCth232M2Sum",  dNBins, dMinEnergy, dMaxEnergy);  
  hOVCu238M2Sum      = new TH1D("hOVCu238M2Sum",   "hOVCu238M2Sum",   dNBins, dMinEnergy, dMaxEnergy);  


////////// External Shields
  hExtPbbi210M1 = new TH1D("hExtPbbi210M1", "hExtPbbi210M1", dNBins, dMinEnergy, dMaxEnergy);

  hExtPbbi210M2 = new TH1D("hExtPbbi210M2", "hExtPbbi210M2", dNBins, dMinEnergy, dMaxEnergy);
  
  hExtPbbi210M2Sum = new TH1D("hExtPbbi210M2Sum", "hExtPbbi210M2Sum", dNBins, dMinEnergy, dMaxEnergy);


/////////// Fudge Factors
  hFudge661M1 = new TH1D("hFudge661M1", "hFudge661M1", dNBins, dMinEnergy, dMaxEnergy);
  hFudge803M1 = new TH1D("hFudge803M1", "hFudge803M1", dNBins, dMinEnergy, dMaxEnergy);
  hFudge1063M1 = new TH1D("hFudge1063M1", "hFudge1063M1", dNBins, dMinEnergy, dMaxEnergy);

  hFudge661M2 = new TH1D("hFudge661M2", "hFudge661M2", dNBins, dMinEnergy, dMaxEnergy);
  hFudge803M2 = new TH1D("hFudge803M2", "hFudge803M2", dNBins, dMinEnergy, dMaxEnergy);
  hFudge1063M2 = new TH1D("hFudge1063M2", "hFudge1063M2", dNBins, dMinEnergy, dMaxEnergy);


// Mess with rebinning here 
  // fDataHistoM1->Rebin(10);
  // fDataHistoM2->Rebin(10);
  // fDataHistoM2Sum->Rebin(10);
  dBaseBinSize = dBinSize*1;

/////// Adaptive binning
 // Calculates adaptive binning vectors
  dAdaptiveVectorM1 = AdaptiveBinning(fDataHistoM1, dBinBase);
  dAdaptiveBinsM1 = dAdaptiveVectorM1.size() - 1;
  dAdaptiveArrayM1 = &dAdaptiveVectorM1[0];
  dAdaptiveVectorM2 = AdaptiveBinning(fDataHistoM2, dBinBase);
  dAdaptiveBinsM2 = dAdaptiveVectorM2.size() - 1;
  dAdaptiveArrayM2 = &dAdaptiveVectorM2[0];
  dAdaptiveVectorM2Sum = AdaptiveBinning(fDataHistoM2Sum, dBinBase);
  dAdaptiveBinsM2Sum = dAdaptiveVectorM2Sum.size() - 1;
  dAdaptiveArrayM2Sum = &dAdaptiveVectorM2Sum[0];

 
  // Adaptive binning data
  fAdapDataHistoM1   = new TH1D("fAdapDataHistoM1",   "", dAdaptiveBinsM1, dAdaptiveArrayM1);
  fAdapDataHistoM2   = new TH1D("fAdapDataHistoM2",   "", dAdaptiveBinsM2, dAdaptiveArrayM2);
  fAdapDataHistoM2Sum   = new TH1D("fAdapDataHistoM2Sum",   "", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  
  hnewM1 = fDataHistoM1->Rebin(dAdaptiveBinsM1, "hnewM1", dAdaptiveArrayM1);
  hnewM2 = fDataHistoM2->Rebin(dAdaptiveBinsM2, "hnewM2", dAdaptiveArrayM2);
  hnewM2Sum = fDataHistoM2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewM2Sum", dAdaptiveArrayM2Sum);

  for(int i = 1; i <= dAdaptiveBinsM1; i++)
  {
    fAdapDataHistoM1->SetBinContent(i, hnewM1->GetBinContent(i)/hnewM1->GetBinWidth(i));
    fAdapDataHistoM1->SetBinError(i, TMath::Sqrt(hnewM1->GetBinContent(i))/hnewM1->GetBinWidth(i));
    // fAdapDataHistoM1->SetBinError(i, 0); // If I don't want errors for some reason
  }

  for(int i = 1; i <= dAdaptiveBinsM2; i++)
  {
    fAdapDataHistoM2->SetBinContent(i, hnewM2->GetBinContent(i)/hnewM2->GetBinWidth(i));
    fAdapDataHistoM2->SetBinError(i, TMath::Sqrt(hnewM2->GetBinContent(i))/hnewM2->GetBinWidth(i));
    // fAdapDataHistoM2->SetBinError(i, 0); // If I don't want errors for some reason
  }

  for(int i = 1; i <= dAdaptiveBinsM2Sum; i++)
  {
    fAdapDataHistoM2Sum->SetBinContent(i, hnewM2Sum->GetBinContent(i)/hnewM2Sum->GetBinWidth(i));
    fAdapDataHistoM2Sum->SetBinError(i, TMath::Sqrt(hnewM2Sum->GetBinContent(i))/hnewM2Sum->GetBinWidth(i));
    // fAdapDataHistoM2Sum->SetBinError(i, 0); // If I don't want errors for some reason
  }

  // dDataIntegral = fDataHistoM1->Integral(1, dNBins);
  // dDataIntegralM1 = fDataHistoM1->Integral(1, 10000/dBinSize);
  // dDataIntegralM2 = fDataHistoM2->Integral(1, 10000/dBinSize);
  // dDataIntegralM2Sum = fDataHistoM2Sum->Integral(1, 10000/dBinSize);
  dDataIntegralTot = qtree->GetEntries();

  dDataIntegralM1 = fAdapDataHistoM1->Integral("width");
  dDataIntegralM2 = fAdapDataHistoM2->Integral("width");
  dDataIntegralM2Sum = fAdapDataHistoM2Sum->Integral("width");

  // dDataIntegralM1 = dDataIntegralM1;
  // dDataIntegralM2 = dDataIntegralM2;

  dFitMinBinM1 = fAdapDataHistoM1->FindBin(dFitMin);
  dFitMinBinM2 = fAdapDataHistoM2->FindBin(dFitMin);
  dFitMinBinM2Sum = fAdapDataHistoM2Sum->FindBin(dFitMin);
  dFitMaxBinM1 = fAdapDataHistoM1->FindBin(dFitMax);
  dFitMaxBinM2 = fAdapDataHistoM2->FindBin(dFitMax);
  dFitMaxBinM2Sum = fAdapDataHistoM2Sum->FindBin(dFitMax);

  // Outputs on screen
  cout << "Fit M1 from bin: " << dFitMinBinM1 << " to " << dFitMaxBinM1 << endl;
  cout << "Fit M2 from bin: " << dFitMinBinM2 << " to " << dFitMaxBinM2 << endl;
  cout << "Fit M2Sum from bin: " << dFitMinBinM2Sum << " to " << dFitMaxBinM2Sum << endl;

  cout << "Total Events in background spectrum: " << dDataIntegralTot << endl; 
  cout << "Events in background spectrum (M1): " << dDataIntegralM1 << endl;
  cout << "Events in background spectrum (M2): " << dDataIntegralM2 << endl;
  cout << "Events in background spectrum (M2Sum): " << dDataIntegralM2Sum << endl;
  cout << "Livetime of background: " << dLivetimeYr << endl;


//////////////// Adaptive binned histograms
////////// Total Adaptive binning histograms M1
  fModelTotAdapM1      = new TH1D("fModelTotAdapM1",      "Total PDF M1", dAdaptiveBinsM1, dAdaptiveArrayM1);  
  fModelTotAdapthM1    = new TH1D("fModelTotAdapthM1",    "Total Bulk th232",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapuM1     = new TH1D("fModelTotAdapuM1",     "Total Bulk u238",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapkM1     = new TH1D("fModelTotAdapkM1",     "Total Bulk k40",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapcoM1    = new TH1D("fModelTotAdapcoM1",    "Total Bulk co60",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapmnM1    = new TH1D("fModelTotAdapmnM1",    "Total Bulk mn54",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  fModelTotAdapNDBDM1  = new TH1D("fModelTotAdapNDBDM1",  "Total Bulk NDBD",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdap2NDBDM1 = new TH1D("fModelTotAdap2NDBDM1", "Total Bulk 2NDBD",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapbiM1    = new TH1D("fModelTotAdapbiM1",    "Total Bulk bi207",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapbi2M1   = new TH1D("fModelTotAdapbi2M1",   "Total Bulk bi210",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapptM1    = new TH1D("fModelTotAdapptM1",    "Total Bulk pt190",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdappbM1    = new TH1D("fModelTotAdappbM1",    "Total Bulk pb210",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapcsM1    = new TH1D("fModelTotAdapcsM1",    "Total Bulk cs137",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapco2M1   = new TH1D("fModelTotAdapco2M1",   "Total Bulk co58",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapteo2M1  = new TH1D("fModelTotAdapteo2M1",  "Total Bulk TeO2",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  fModelTotAdapSthM1   = new TH1D("fModelTotAdapSthM1",   "Total Surface th232",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapSuM1    = new TH1D("fModelTotAdapSuM1",    "Total Surface u238",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapSpbM1   = new TH1D("fModelTotAdapSpbM1",   "Total Surface pb210",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  fModelTotAdapSpoM1   = new TH1D("fModelTotAdapSpoM1",   "Total Surface po210",  dAdaptiveBinsM1, dAdaptiveArrayM1);

  fModelTotAdapExtM1   = new TH1D("fModelTotAdapExtM1", "Total External", dAdaptiveBinsM1, dAdaptiveArrayM1);

  fModelTotAdapFudgeM1 = new TH1D("fModelTotAdapFudgeM1", "Total Fudge Factors", dAdaptiveBinsM1, dAdaptiveArrayM1);

/////////// Total Adaptive binning histograms M2
  fModelTotAdapM2      = new TH1D("fModelTotAdapM2",      "Total PDF M2", dAdaptiveBinsM2, dAdaptiveArrayM2);  
  fModelTotAdapthM2    = new TH1D("fModelTotAdapthM2",    "Total Bulk th232",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapuM2     = new TH1D("fModelTotAdapuM2",     "Total Bulk u238",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapkM2     = new TH1D("fModelTotAdapkM2",     "Total Bulk k40",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapcoM2    = new TH1D("fModelTotAdapcoM2",    "Total Bulk co60",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapmnM2    = new TH1D("fModelTotAdapmnM2",    "Total Bulk mn54",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  fModelTotAdapNDBDM2  = new TH1D("fModelTotAdapNDBDM2",  "Total Bulk NDBD",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdap2NDBDM2 = new TH1D("fModelTotAdap2NDBDM2", "Total Bulk 2NDBD",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapbiM2    = new TH1D("fModelTotAdapbiM2",    "Total Bulk bi207",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapbi2M2   = new TH1D("fModelTotAdapbi2M2",   "Total Bulk bi210",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapptM2    = new TH1D("fModelTotAdapotM2",    "Total Bulk pt190",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdappbM2    = new TH1D("fModelTotAdappbM2",    "Total Bulk pb210",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapcsM2    = new TH1D("fModelTotAdapcsM2",    "Total Bulk cs137",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapco2M2   = new TH1D("fModelTotAdapco2M2",   "Total Bulk co58",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapteo2M2  = new TH1D("fModelTotAdapteo2M2",  "Total Bulk TeO2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  fModelTotAdapSthM2   = new TH1D("fModelTotAdapSthM2",   "Total Surface th232",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapSuM2    = new TH1D("fModelTotAdapSuM2",    "Total Surface u238",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapSpbM2   = new TH1D("fModelTotAdapSpbM2",   "Total Surface pb210",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  fModelTotAdapSpoM2   = new TH1D("fModelTotAdapSpoM2",   "Total Surface po210",  dAdaptiveBinsM2, dAdaptiveArrayM2);

  fModelTotAdapExtM2   = new TH1D("fModelTotAdapExtM2", "Total External", dAdaptiveBinsM2, dAdaptiveArrayM2);

  fModelTotAdapFudgeM2 = new TH1D("fModelTotAdapFudgeM2", "Total Fudge Factors", dAdaptiveBinsM2, dAdaptiveArrayM2);

/////////// Total Adaptive binning histograms M2Sum

  fModelTotAdapM2Sum      = new TH1D("fModelTotAdapM2Sum",      "Total PDF M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  fModelTotAdapthM2Sum    = new TH1D("fModelTotAdapthM2Sum",    "Total Bulk th232",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapuM2Sum     = new TH1D("fModelTotAdapuM2Sum",     "Total Bulk u238",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapkM2Sum     = new TH1D("fModelTotAdapkM2Sum",     "Total Bulk k40",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapcoM2Sum    = new TH1D("fModelTotAdapcoM2Sum",    "Total Bulk co60",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapmnM2Sum    = new TH1D("fModelTotAdapmnM2Sum",    "Total Bulk mn54",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  fModelTotAdapNDBDM2Sum  = new TH1D("fModelTotAdapNDBDM2Sum",  "Total Bulk NDBD",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdap2NDBDM2Sum = new TH1D("fModelTotAdap2NDBDM2Sum", "Total Bulk 2NDBD",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapbiM2Sum    = new TH1D("fModelTotAdapbiM2Sum",    "Total Bulk bi207",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapbi2M2Sum   = new TH1D("fModelTotAdapbi2M2Sum",   "Total Bulk bi210",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapptM2Sum    = new TH1D("fModelTotAdapotM2Sum",    "Total Bulk pt190",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdappbM2Sum    = new TH1D("fModelTotAdappbM2Sum",    "Total Bulk pb210",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapcsM2Sum    = new TH1D("fModelTotAdapcsM2Sum",    "Total Bulk cs137",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapco2M2Sum   = new TH1D("fModelTotAdapco2M2Sum",   "Total Bulk co58",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapteo2M2Sum  = new TH1D("fModelTotAdapteo2M2Sum",  "Total Bulk TeO2",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  fModelTotAdapSthM2Sum   = new TH1D("fModelTotAdapSthM2Sum",   "Total Surface th232",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapSuM2Sum    = new TH1D("fModelTotAdapSuM2Sum",    "Total Surface u238",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapSpbM2Sum   = new TH1D("fModelTotAdapSpbM2Sum",   "Total Surface pb210",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  fModelTotAdapSpoM2Sum   = new TH1D("fModelTotAdapSpoM2Sum",   "Total Surface po210",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  fModelTotAdapExtM2Sum   = new TH1D("fModelTotAdapExtM2Sum", "Total External", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  fModelTotAdapFudgeM2Sum = new TH1D("fModelTotAdapFudgeM2Sum", "Total Fudge Factors", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);


/////////// Crystal M1 and M2
  hAdapTeO20nuM1       = new TH1D("hAdapTeO20nuM1",    "TeO2 Bulk 0nu M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO22nuM1       = new TH1D("hAdapTeO22nuM1",    "TeO2 Bulk 2nu M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2co60M1      = new TH1D("hAdapTeO2co60M1",   "TeO2 Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2k40M1       = new TH1D("hAdapTeO2k40M1",    "TeO2 Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2pb210M1     = new TH1D("hAdapTeO2pb210M1",  "TeO2 Bulk pb210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2po210M1     = new TH1D("hAdapTeO2po210M1",  "TeO2 Bulk po210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2te125M1     = new TH1D("hAdapTeO2te125M1",  "TeO2 Bulk te125 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2th232M1     = new TH1D("hAdapTeO2th232M1",  "TeO2 Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapTeO2th228M1     = new TH1D("hAdapTeO2th228M1",  "TeO2 Bulk th228 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2ra226M1     = new TH1D("hAdapTeO2ra226M1",  "TeO2 Bulk ra226 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2rn222M1     = new TH1D("hAdapTeO2rn222M1",  "TeO2 Bulk rn222 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2u238M1      = new TH1D("hAdapTeO2u238M1",   "TeO2 Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2th230M1     = new TH1D("hAdapTeO2th230M1",  "TeO2 Bulk th230M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2u234M1      = new TH1D("hAdapTeO2u234M1",   "TeO2 Bulk u234 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO2Spb210M1_01      = new TH1D("hAdapTeO2Spb210M1_01",   "TeO2 S pb210M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Spo210M1_001     = new TH1D("hAdapTeO2Spo210M1_001",  "TeO2 S po210M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Spo210M1_01      = new TH1D("hAdapTeO2Spo210M1_01",   "TeO2 S po210M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sth232M1_01      = new TH1D("hAdapTeO2Sth232M1_01",   "TeO2 S th232M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Su238M1_01       = new TH1D("hAdapTeO2Su238M1_01",    "TeO2 S u238M1 0.1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_001    = new TH1D("hAdapTeO2Sxpb210M1_001", "TeO2 Sx pb210 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_01     = new TH1D("hAdapTeO2Sxpb210M1_01",  "TeO2 Sx pb210 M1 0.1 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_1      = new TH1D("hAdapTeO2Sxpb210M1_1",   "TeO2 Sx pb210 M1 1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_10     = new TH1D("hAdapTeO2Sxpb210M1_10",  "TeO2 Sx pb210 M1 10 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpo210M1_001    = new TH1D("hAdapTeO2Sxpo210M1_001", "TeO2 Sx po210 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpo210M1_01     = new TH1D("hAdapTeO2Sxpo210M1_01",  "TeO2 Sx po210 M1 0.1 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpo210M1_1      = new TH1D("hAdapTeO2Sxpo210M1_1",   "TeO2 Sx po210 M1 1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth232M1_001    = new TH1D("hAdapTeO2Sxth232M1_001", "TeO2 Sx th232 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth232M1_01     = new TH1D("hAdapTeO2Sxth232M1_01",  "TeO2 Sx th232 M1 0.1 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth232M1_1      = new TH1D("hAdapTeO2Sxth232M1_1",   "TeO2 Sx th232 M1 1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth232M1_10     = new TH1D("hAdapTeO2Sxth232M1_10",  "TeO2 Sx th232 M1 10 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238M1_001     = new TH1D("hAdapTeO2Sxu238M1_001",  "TeO2 Sx u238 M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238M1_01      = new TH1D("hAdapTeO2Sxu238M1_01",   "TeO2 Sx u238 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238M1_1       = new TH1D("hAdapTeO2Sxu238M1_1",    "TeO2 Sx u238 M1 1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238M1_10      = new TH1D("hAdapTeO2Sxu238M1_10",   "TeO2 Sx u238 M1 10 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO2Sxu238M1_100      = new TH1D("hAdapTeO2Sxu238M1_100",   "TeO2 Sx u238 M1 100 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth232M1_100     = new TH1D("hAdapTeO2Sxth232M1_100",  "TeO2 Sx th232 M1 100 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_100     = new TH1D("hAdapTeO2Sxpb210M1_100",  "TeO2 Sx pb210 M1 100 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);


  hAdapTeO2th232onlyM1      = new TH1D("hAdapTeO2th232onlyM1",   "TeO2 Bulk th232 only M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2ra228pb208M1     = new TH1D("hAdapTeO2ra228pb208M1",  "TeO2 Bulk ra228-pb208 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2th230onlyM1      = new TH1D("hAdapTeO2th230onlyM1",   "TeO2 Bulk th230 only M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO2Sxth232onlyM1_001  = new TH1D("hAdapTeO2Sxth232onlyM1_001", "TeO2 Sx th232 only M1 0.01 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra228pb208M1_001 = new TH1D("hAdapTeO2Sxra228pb208M1_001", "TeO2 Sx ra228-pb208 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238th230M1_001  = new TH1D("hAdapTeO2Sxu238th230M1_001", "TeO2 Sx u238-th230 M1 0.01 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth230onlyM1_001  = new TH1D("hAdapTeO2Sxth230onlyM1_001", "TeO2 Sx th230-only M1 0.01 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra226pb210M1_001 = new TH1D("hAdapTeO2Sxra226pb210M1_001", "TeO2 Sx ra226-pb210 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxpb210M1_0001     = new TH1D("hAdapTeO2Sxpb210M1_0001", "TeO2 Sx pb210 M1 0.001 #mum",         dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO2Sxth232onlyM1_01  = new TH1D("hAdapTeO2Sxth232onlyM1_01", "TeO2 Sx th232 only M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra228pb208M1_01 = new TH1D("hAdapTeO2Sxra228pb208M1_01", "TeO2 Sx ra228-pb208 M1 0.1 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238th230M1_01  = new TH1D("hAdapTeO2Sxu238th230M1_01", "TeO2 Sx u238-th230 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth230onlyM1_01  = new TH1D("hAdapTeO2Sxth230onlyM1_01", "TeO2 Sx th230-only M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra226pb210M1_01 = new TH1D("hAdapTeO2Sxra226pb210M1_01", "TeO2 Sx ra226-pb210 M1 0.1 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO2Sxth232onlyM1_0001  = new TH1D("hAdapTeO2Sxth232onlyM1_0001", "TeO2 Sx th232 only M1 0.001 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra228pb208M1_0001 = new TH1D("hAdapTeO2Sxra228pb208M1_0001", "TeO2 Sx ra228-pb208 M1 0.001 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxu238th230M1_0001  = new TH1D("hAdapTeO2Sxu238th230M1_0001", "TeO2 Sx u238-th230 M1 0.001 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxth230onlyM1_0001  = new TH1D("hAdapTeO2Sxth230onlyM1_0001", "TeO2 Sx th230-only M1 0.001 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapTeO2Sxra226pb210M1_0001 = new TH1D("hAdapTeO2Sxra226pb210M1_0001", "TeO2 Sx ra226-pb210 M1 0.001 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapTeO20nuM2       = new TH1D("hAdapTeO20nuM2",    "TeO2 Bulk 0nu M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO22nuM2       = new TH1D("hAdapTeO22nuM2",    "TeO2 Bulk 2nu M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2co60M2      = new TH1D("hAdapTeO2co60M2",   "TeO2 Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2k40M2       = new TH1D("hAdapTeO2k40M2",    "TeO2 Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2pb210M2     = new TH1D("hAdapTeO2pb210M2",  "TeO2 Bulk pb210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2po210M2     = new TH1D("hAdapTeO2po210M2",  "TeO2 Bulk po210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2te125M2     = new TH1D("hAdapTeO2te125M2",  "TeO2 Bulk te125 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2th232M2     = new TH1D("hAdapTeO2th232M2",  "TeO2 Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapTeO2th228M2     = new TH1D("hAdapTeO2th228M2",  "TeO2 Bulk th228 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2ra226M2     = new TH1D("hAdapTeO2ra226M2",  "TeO2 Bulk ra226 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2rn222M2     = new TH1D("hAdapTeO2rn222M2",  "TeO2 Bulk rn222 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2u238M2      = new TH1D("hAdapTeO2u238M2",   "TeO2 Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2th230M2     = new TH1D("hAdapTeO2th230M2",  "TeO2 Bulk th230 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2u234M2      = new TH1D("hAdapTeO2u234M2",   "TeO2 Bulk u234 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2Spb210M2_01      = new TH1D("hAdapTeO2Spb210M2_01",   "TeO2 S pb210 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Spo210M2_001     = new TH1D("hAdapTeO2Spo210M2_001",  "TeO2 S po210 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Spo210M2_01      = new TH1D("hAdapTeO2Spo210M2_01",   "TeO2 S po210 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sth232M2_01      = new TH1D("hAdapTeO2Sth232M2_01",   "TeO2 S th232 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Su238M2_01       = new TH1D("hAdapTeO2Su238M2_01",    "TeO2 S u238 M2 0.1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_001    = new TH1D("hAdapTeO2Sxpb210M2_001", "TeO2 Sx pb210 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_01     = new TH1D("hAdapTeO2Sxpb210M2_01",  "TeO2 Sx pb210 M2 0.1 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_1      = new TH1D("hAdapTeO2Sxpb210M2_1",   "TeO2 Sx pb210 M2 1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_10     = new TH1D("hAdapTeO2Sxpb210M2_10",  "TeO2 Sx pb210 M2 10 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpo210M2_001    = new TH1D("hAdapTeO2Sxpo210M2_001", "TeO2 Sx po210 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpo210M2_01     = new TH1D("hAdapTeO2Sxpo210M2_01",  "TeO2 Sx po210 M2 0.1 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpo210M2_1      = new TH1D("hAdapTeO2Sxpo210M2_1",   "TeO2 Sx po210 M2 1",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth232M2_001    = new TH1D("hAdapTeO2Sxth232M2_001", "TeO2 Sx th232 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth232M2_01     = new TH1D("hAdapTeO2Sxth232M2_01",  "TeO2 Sx th232 M2 0.1 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth232M2_1      = new TH1D("hAdapTeO2Sxth232M2_1",   "TeO2 Sx th232 M2 1",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth232M2_10     = new TH1D("hAdapTeO2Sxth232M2_10",  "TeO2 Sx th232 M2 10 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238M2_001     = new TH1D("hAdapTeO2Sxu238M2_001",  "TeO2 Sx u238 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238M2_01      = new TH1D("hAdapTeO2Sxu238M2_01",   "TeO2 Sx u238 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238M2_1       = new TH1D("hAdapTeO2Sxu238M2_1",    "TeO2 Sx u238 M2 1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238M2_10      = new TH1D("hAdapTeO2Sxu238M2_10",   "TeO2 Sx u238 M2 10 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2Sxu238M2_100      = new TH1D("hAdapTeO2Sxu238M2_100",   "TeO2 Sx u238 M2 100 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth232M2_100     = new TH1D("hAdapTeO2Sxth232M2_100",  "TeO2 Sx th232 M2 100 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_100     = new TH1D("hAdapTeO2Sxpb210M2_100",  "TeO2 Sx pb210 M2 100 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2th232onlyM2      = new TH1D("hAdapTeO2th232onlyM2",   "TeO2 Bulk th232 only M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2ra228pb208M2     = new TH1D("hAdapTeO2ra228pb208M2",  "TeO2 Bulk ra228-pb208 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2th230onlyM2      = new TH1D("hAdapTeO2th230onlyM2",   "TeO2 Bulk th230 only M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2Sxth232onlyM2_001  = new TH1D("hAdapTeO2Sxth232onlyM2_001", "TeO2 Sx th232 only M2 0.01 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra228pb208M2_001 = new TH1D("hAdapTeO2Sxra228pb208M2_001", "TeO2 Sx ra228-pb208 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238th230M2_001  = new TH1D("hAdapTeO2Sxu238th230M2_001", "TeO2 Sx u238-th230 M2 0.01 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth230onlyM2_001  = new TH1D("hAdapTeO2Sxth230onlyM2_001", "TeO2 Sx th230 only M2 0.01 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra226pb210M2_001 = new TH1D("hAdapTeO2Sxra226pb210M2_001", "TeO2 Sx ra226-pb210 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxpb210M2_0001     = new TH1D("hAdapTeO2Sxpb210M2_0001", "TeO2 Sx pb210 M2 0.001 #mum",         dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2Sxth232onlyM2_01  = new TH1D("hAdapTeO2Sxth232onlyM2_01", "TeO2 Sx th232 only M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra228pb208M2_01 = new TH1D("hAdapTeO2Sxra228pb208M2_01", "TeO2 Sx ra228-pb208 M2 0.1 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238th230M2_01  = new TH1D("hAdapTeO2Sxu238th230M2_01", "TeO2 Sx u238-th230 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth230onlyM2_01  = new TH1D("hAdapTeO2Sxth230onlyM2_01", "TeO2 Sx th230-only M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra226pb210M2_01 = new TH1D("hAdapTeO2Sxra226pb210M2_01", "TeO2 Sx ra226-pb210 M2 0.1 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapTeO2Sxth232onlyM2_0001  = new TH1D("hAdapTeO2Sxth232onlyM2_0001", "TeO2 Sx th232 only M2 0.001 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra228pb208M2_0001 = new TH1D("hAdapTeO2Sxra228pb208M2_0001", "TeO2 Sx ra228-pb208 M2 0.001 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxu238th230M2_0001  = new TH1D("hAdapTeO2Sxu238th230M2_0001", "TeO2 Sx u238-th230 M2 0.001 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxth230onlyM2_0001  = new TH1D("hAdapTeO2Sxth230onlyM2_0001", "TeO2 Sx th230-only M2 0.001 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapTeO2Sxra226pb210M2_0001 = new TH1D("hAdapTeO2Sxra226pb210M2_0001", "TeO2 Sx ra226-pb210 M2 0.001 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);


  hAdapTeO20nuM2Sum       = new TH1D("hAdapTeO20nuM2Sum",    "TeO2 Bulk 0nu M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO22nuM2Sum       = new TH1D("hAdapTeO22nuM2Sum",    "TeO2 Bulk 2nu M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2co60M2Sum      = new TH1D("hAdapTeO2co60M2Sum",   "TeO2 Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2k40M2Sum       = new TH1D("hAdapTeO2k40M2Sum",    "TeO2 Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2pb210M2Sum     = new TH1D("hAdapTeO2pb210M2Sum",  "TeO2 Bulk pb210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2po210M2Sum     = new TH1D("hAdapTeO2po210M2Sum",  "TeO2 Bulk po210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2te125M2Sum     = new TH1D("hAdapTeO2te125M2Sum",  "TeO2 Bulk te125 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2th232M2Sum     = new TH1D("hAdapTeO2th232M2Sum",  "TeO2 Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapTeO2th228M2Sum     = new TH1D("hAdapTeO2th228M2Sum",  "TeO2 Bulk th228 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2ra226M2Sum     = new TH1D("hAdapTeO2ra226M2Sum",  "TeO2 Bulk ra226 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2rn222M2Sum     = new TH1D("hAdapTeO2rn222M2Sum",  "TeO2 Bulk rn222 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2u238M2Sum      = new TH1D("hAdapTeO2u238M2Sum",   "TeO2 Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2th230M2Sum     = new TH1D("hAdapTeO2th230M2Sum",  "TeO2 Bulk th230 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2u234M2Sum      = new TH1D("hAdapTeO2u234M2Sum",   "TeO2 Bulk u234 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2Spb210M2Sum_01      = new TH1D("hAdapTeO2Spb210M2Sum_01",   "TeO2 S pb210 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Spo210M2Sum_001     = new TH1D("hAdapTeO2Spo210M2Sum_001",  "TeO2 S po210 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Spo210M2Sum_01      = new TH1D("hAdapTeO2Spo210M2Sum_01",   "TeO2 S po210 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sth232M2Sum_01      = new TH1D("hAdapTeO2Sth232M2Sum_01",   "TeO2 S th232 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Su238M2Sum_01       = new TH1D("hAdapTeO2Su238M2Sum_01",    "TeO2 S u238 M2Sum 0.1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_001    = new TH1D("hAdapTeO2Sxpb210M2Sum_001", "TeO2 Sx pb210 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_01     = new TH1D("hAdapTeO2Sxpb210M2Sum_01",  "TeO2 Sx pb210 M2Sum 0.1 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_1      = new TH1D("hAdapTeO2Sxpb210M2Sum_1",   "TeO2 Sx pb210 M2Sum 1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_10     = new TH1D("hAdapTeO2Sxpb210M2Sum_10",  "TeO2 Sx pb210 M2Sum 10 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpo210M2Sum_001    = new TH1D("hAdapTeO2Sxpo210M2Sum_001", "TeO2 Sx po210 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpo210M2Sum_01     = new TH1D("hAdapTeO2Sxpo210M2Sum_01",  "TeO2 Sx po210 M2Sum 0.1 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpo210M2Sum_1      = new TH1D("hAdapTeO2Sxpo210M2Sum_1",   "TeO2 Sx po210 M2Sum 1",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth232M2Sum_001    = new TH1D("hAdapTeO2Sxth232M2Sum_001", "TeO2 Sx th232 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth232M2Sum_01     = new TH1D("hAdapTeO2Sxth232M2Sum_01",  "TeO2 Sx th232 M2Sum 0.1 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth232M2Sum_1      = new TH1D("hAdapTeO2Sxth232M2Sum_1",   "TeO2 Sx th232 M2Sum 1",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth232M2Sum_10     = new TH1D("hAdapTeO2Sxth232M2Sum_10",  "TeO2 Sx th232 M2Sum 10 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238M2Sum_001     = new TH1D("hAdapTeO2Sxu238M2Sum_001",  "TeO2 Sx u238 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238M2Sum_01      = new TH1D("hAdapTeO2Sxu238M2Sum_01",   "TeO2 Sx u238 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238M2Sum_1       = new TH1D("hAdapTeO2Sxu238M2Sum_1",    "TeO2 Sx u238 M2Sum 1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238M2Sum_10      = new TH1D("hAdapTeO2Sxu238M2Sum_10",   "TeO2 Sx u238 M2Sum 10 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2Sxu238M2Sum_100      = new TH1D("hAdapTeO2Sxu238M2Sum_100",   "TeO2 Sx u238 M2Sum 100 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth232M2Sum_100     = new TH1D("hAdapTeO2Sxth232M2Sum_100",  "TeO2 Sx th232 M2Sum 100 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_100     = new TH1D("hAdapTeO2Sxpb210M2Sum_100",  "TeO2 Sx pb210 M2Sum 100 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2th232onlyM2Sum      = new TH1D("hAdapTeO2th232onlyM2Sum",   "TeO2 Bulk th232 only M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2ra228pb208M2Sum     = new TH1D("hAdapTeO2ra228pb208M2Sum",  "TeO2 Bulk ra228-pb208 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2th230onlyM2Sum      = new TH1D("hAdapTeO2th230onlyM2Sum",   "TeO2 Bulk th230 only M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2Sxth232onlyM2Sum_001  = new TH1D("hAdapTeO2Sxth232onlyM2Sum_001", "TeO2 Sx th232 only M2Sum 0.01 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra228pb208M2Sum_001 = new TH1D("hAdapTeO2Sxra228pb208M2Sum_001", "TeO2 Sx ra228-pb208 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238th230M2Sum_001  = new TH1D("hAdapTeO2Sxu238th230M2Sum_001", "TeO2 Sx u238-th230 M2Sum 0.01 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth230onlyM2Sum_001  = new TH1D("hAdapTeO2Sxth230onlyM2Sum_001", "TeO2 Sx th230 only M2Sum 0.01 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra226pb210M2Sum_001 = new TH1D("hAdapTeO2Sxra226pb210M2Sum_001", "TeO2 Sx ra226-pb210 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxpb210M2Sum_0001     = new TH1D("hAdapTeO2Sxpb210M2Sum_0001", "TeO2 Sx pb210 M2Sum 0.001 #mum",         dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2Sxth232onlyM2Sum_01  = new TH1D("hAdapTeO2Sxth232onlyM2Sum_01", "TeO2 Sx th232 only M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra228pb208M2Sum_01 = new TH1D("hAdapTeO2Sxra228pb208M2Sum_01", "TeO2 Sx ra228-pb208 M2Sum 0.1 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238th230M2Sum_01  = new TH1D("hAdapTeO2Sxu238th230M2Sum_01", "TeO2 Sx u238-th230 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth230onlyM2Sum_01  = new TH1D("hAdapTeO2Sxth230onlyM2Sum_01", "TeO2 Sx th230-only M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra226pb210M2Sum_01 = new TH1D("hAdapTeO2Sxra226pb210M2Sum_01", "TeO2 Sx ra226-pb210 M2Sum 0.1 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapTeO2Sxth232onlyM2Sum_0001  = new TH1D("hAdapTeO2Sxth232onlyM2Sum_0001", "TeO2 Sx th232 only M2Sum 0.001 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra228pb208M2Sum_0001 = new TH1D("hAdapTeO2Sxra228pb208M2Sum_0001", "TeO2 Sx ra228-pb208 M2Sum 0.001 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxu238th230M2Sum_0001  = new TH1D("hAdapTeO2Sxu238th230M2Sum_0001", "TeO2 Sx u238-th230 M2Sum 0.001 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxth230onlyM2Sum_0001  = new TH1D("hAdapTeO2Sxth230onlyM2Sum_0001", "TeO2 Sx th230-only M2Sum 0.001 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapTeO2Sxra226pb210M2Sum_0001 = new TH1D("hAdapTeO2Sxra226pb210M2Sum_0001", "TeO2 Sx ra226-pb210 M2Sum 0.001 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);


////////// Frame M1 and M2
  hAdapCuFrameco58M1      = new TH1D("hAdapCuFrameco58M1",   "CuFrame Bulk co58 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameco60M1      = new TH1D("hAdapCuFrameco60M1",   "CuFrame Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFramecs137M1     = new TH1D("hAdapCuFramecs137M1",  "CuFrame Bulk cs137 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFramek40M1       = new TH1D("hAdapCuFramek40M1",    "CuFrame Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFramemn54M1      = new TH1D("hAdapCuFramemn54M1",   "CuFrame Bulk mn54 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFramepb210M1     = new TH1D("hAdapCuFramepb210M1",  "CuFrame Bulk pb210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameth232M1     = new TH1D("hAdapCuFrameth232M1",  "CuFrame Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapCuFrameu238M1      = new TH1D("hAdapCuFrameu238M1",   "CuFrame Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuFrameSth232M1_1     = new TH1D("hAdapCuFrameSth232M1_1",    "CuFrame S th232 M1 1 #mum",     dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSu238M1_1      = new TH1D("hAdapCuFrameSu238M1_1",     "CuFrame S u238 M1 1 #mum",      dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxpb210M1_001  = new TH1D("hAdapCuFrameSxpb210M1_001", "CuFrame Sx pb210 M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxpb210M1_01   = new TH1D("hAdapCuFrameSxpb210M1_01",  "CuFrame Sx pb210 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxpb210M1_1    = new TH1D("hAdapCuFrameSxpb210M1_1",   "CuFrame Sx pb210 M1 1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxpb210M1_10   = new TH1D("hAdapCuFrameSxpb210M1_10",  "CuFrame Sx pb210 M1 10 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxth232M1_001  = new TH1D("hAdapCuFrameSxth232M1_001", "CuFrame Sx th232 M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxth232M1_01   = new TH1D("hAdapCuFrameSxth232M1_01",  "CuFrame Sx th232 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxth232M1_1    = new TH1D("hAdapCuFrameSxth232M1_1",   "CuFrame Sx th232 M1 1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxth232M1_10   = new TH1D("hAdapCuFrameSxth232M1_10",  "CuFrame Sx th232 M1 10 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxu238M1_001   = new TH1D("hAdapCuFrameSxu238M1_001",  "CuFrame Sx u238 M1 0.01 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxu238M1_01    = new TH1D("hAdapCuFrameSxu238M1_01",   "CuFrame Sx u238 M1 0.1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxu238M1_1     = new TH1D("hAdapCuFrameSxu238M1_1",    "CuFrame Sx u238 M1 1 #mum",     dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuFrameSxu238M1_10    = new TH1D("hAdapCuFrameSxu238M1_10",   "CuFrame Sx u238 M1 10 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuFrameco58M2      = new TH1D("hAdapCuFrameco58M2",   "CuFrame Bulk co58 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameco60M2      = new TH1D("hAdapCuFrameco60M2",   "CuFrame Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFramecs137M2     = new TH1D("hAdapCuFramecs137M2",  "CuFrame Bulk cs137 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFramek40M2       = new TH1D("hAdapCuFramek40M2",    "CuFrame Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFramemn54M2      = new TH1D("hAdapCuFramemn54M2",   "CuFrame Bulk mn54 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFramepb210M2     = new TH1D("hAdapCuFramepb210M2",  "CuFrame Bulk pb210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameth232M2     = new TH1D("hAdapCuFrameth232M2",  "CuFrame Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapCuFrameu238M2      = new TH1D("hAdapCuFrameu238M2",   "CuFrame Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuFrameSth232M2_1     = new TH1D("hAdapCuFrameSth232M2_1",    "CuFrame S th232 M2 1 #mum",     dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSu238M2_1      = new TH1D("hAdapCuFrameSu238M2_1",     "CuFrame S u238 M2 1 #mum",      dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxpb210M2_001  = new TH1D("hAdapCuFrameSxpb210M2_001", "CuFrame Sx pb210 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxpb210M2_01   = new TH1D("hAdapCuFrameSxpb210M2_01",  "CuFrame Sx pb210 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxpb210M2_1    = new TH1D("hAdapCuFrameSxpb210M2_1",   "CuFrame Sx pb210 M2 1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxpb210M2_10   = new TH1D("hAdapCuFrameSxpb210M2_10",  "CuFrame Sx pb210 M2 10 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxth232M2_001  = new TH1D("hAdapCuFrameSxth232M2_001", "CuFrame Sx th232 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxth232M2_01   = new TH1D("hAdapCuFrameSxth232M2_01",  "CuFrame Sx th232 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxth232M2_1    = new TH1D("hAdapCuFrameSxth232M2_1",   "CuFrame Sx th232 M2 1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxth232M2_10   = new TH1D("hAdapCuFrameSxth232M2_10",  "CuFrame Sx th232 M2 10 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxu238M2_001   = new TH1D("hAdapCuFrameSxu238M2_001",  "CuFrame Sx u238 M2 0.01 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxu238M2_01    = new TH1D("hAdapCuFrameSxu238M2_01",   "CuFrame Sx u238 M2 0.1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxu238M2_1     = new TH1D("hAdapCuFrameSxu238M2_1",    "CuFrame Sx u238 M2 1 #mum",     dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuFrameSxu238M2_10    = new TH1D("hAdapCuFrameSxu238M2_10",   "CuFrame Sx u238 M2 10 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuFrameco58M2Sum      = new TH1D("hAdapCuFrameco58M2Sum",   "CuFrame Bulk co58 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameco60M2Sum      = new TH1D("hAdapCuFrameco60M2Sum",   "CuFrame Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFramecs137M2Sum     = new TH1D("hAdapCuFramecs137M2Sum",  "CuFrame Bulk cs137 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFramek40M2Sum       = new TH1D("hAdapCuFramek40M2Sum",    "CuFrame Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFramemn54M2Sum      = new TH1D("hAdapCuFramemn54M2Sum",   "CuFrame Bulk mn54 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFramepb210M2Sum     = new TH1D("hAdapCuFramepb210M2Sum",  "CuFrame Bulk pb210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameth232M2Sum     = new TH1D("hAdapCuFrameth232M2Sum",  "CuFrame Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapCuFrameu238M2Sum      = new TH1D("hAdapCuFrameu238M2Sum",   "CuFrame Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapCuFrameSth232M2Sum_1     = new TH1D("hAdapCuFrameSth232M2Sum_1",    "CuFrame S th232 M2Sum 1 #mum",     dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSu238M2Sum_1      = new TH1D("hAdapCuFrameSu238M2Sum_1",     "CuFrame S u238 M2Sum 1 #mum",      dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxpb210M2Sum_001  = new TH1D("hAdapCuFrameSxpb210M2Sum_001", "CuFrame Sx pb210 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxpb210M2Sum_01   = new TH1D("hAdapCuFrameSxpb210M2Sum_01",  "CuFrame Sx pb210 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxpb210M2Sum_1    = new TH1D("hAdapCuFrameSxpb210M2Sum_1",   "CuFrame Sx pb210 M2Sum 1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxpb210M2Sum_10   = new TH1D("hAdapCuFrameSxpb210M2Sum_10",  "CuFrame Sx pb210 M2Sum 10 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxth232M2Sum_001  = new TH1D("hAdapCuFrameSxth232M2Sum_001", "CuFrame Sx th232 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxth232M2Sum_01   = new TH1D("hAdapCuFrameSxth232M2Sum_01",  "CuFrame Sx th232 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxth232M2Sum_1    = new TH1D("hAdapCuFrameSxth232M2Sum_1",   "CuFrame Sx th232 M2Sum 1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxth232M2Sum_10   = new TH1D("hAdapCuFrameSxth232M2Sum_10",  "CuFrame Sx th232 M2Sum 10 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxu238M2Sum_001   = new TH1D("hAdapCuFrameSxu238M2Sum_001",  "CuFrame Sx u238 M2Sum 0.01 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxu238M2Sum_01    = new TH1D("hAdapCuFrameSxu238M2Sum_01",   "CuFrame Sx u238 M2Sum 0.1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxu238M2Sum_1     = new TH1D("hAdapCuFrameSxu238M2Sum_1",    "CuFrame Sx u238 M2Sum 1 #mum",     dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuFrameSxu238M2Sum_10    = new TH1D("hAdapCuFrameSxu238M2Sum_10",   "CuFrame Sx u238 M2Sum 10 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

////////// CuBox (TShield) M1 and M2
  hAdapCuBoxco58M1      = new TH1D("hAdapCuBoxco58M1",   "CuBox Bulk co58 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxco60M1      = new TH1D("hAdapCuBoxco60M1",   "CuBox Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxcs137M1     = new TH1D("hAdapCuBoxcs137M1",  "CuBox Bulk cs137 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxk40M1       = new TH1D("hAdapCuBoxk40M1",    "CuBox Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxmn54M1      = new TH1D("hAdapCuBoxmn54M1",   "CuBox Bulk mn54 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxpb210M1     = new TH1D("hAdapCuBoxpb210M1",  "CuBox Bulk pb210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxth232M1     = new TH1D("hAdapCuBoxth232M1",  "CuBox Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapCuBoxu238M1      = new TH1D("hAdapCuBoxu238M1",   "CuBox Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuBoxSth232M1_1     = new TH1D("hAdapCuBoxSth232M1_1",    "CuBox S th232 M1 1 #mum",     dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSu238M1_1      = new TH1D("hAdapCuBoxSu238M1_1",     "CuBox S u238 M1 1 #mum",      dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxpb210M1_001  = new TH1D("hAdapCuBoxSxpb210M1_001", "CuBox Sx pb210 M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxpb210M1_01   = new TH1D("hAdapCuBoxSxpb210M1_01",  "CuBox Sx pb210 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxpb210M1_1    = new TH1D("hAdapCuBoxSxpb210M1_1",   "CuBox Sx pb210 M1 1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxpb210M1_10   = new TH1D("hAdapCuBoxSxpb210M1_10",  "CuBox Sx pb210 M1 10 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxth232M1_001  = new TH1D("hAdapCuBoxSxth232M1_001", "CuBox Sx th232 M1 0.01 #mum",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxth232M1_01   = new TH1D("hAdapCuBoxSxth232M1_01",  "CuBox Sx th232 M1 0.1 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxth232M1_1    = new TH1D("hAdapCuBoxSxth232M1_1",   "CuBox Sx th232 M1 1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxth232M1_10   = new TH1D("hAdapCuBoxSxth232M1_10",  "CuBox Sx th232 M1 10 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxu238M1_001   = new TH1D("hAdapCuBoxSxu238M1_001",  "CuBox Sx u238 M1 0.01 #mum",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxu238M1_01    = new TH1D("hAdapCuBoxSxu238M1_01",   "CuBox Sx u238 M1 0.1 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxu238M1_1     = new TH1D("hAdapCuBoxSxu238M1_1",    "CuBox Sx u238 M1 1 #mum",     dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBoxSxu238M1_10    = new TH1D("hAdapCuBoxSxu238M1_10",   "CuBox Sx u238 M1 10 #mum",    dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuBoxco58M2      = new TH1D("hAdapCuBoxco58M2",   "CuBox Bulk co58 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxco60M2      = new TH1D("hAdapCuBoxco60M2",   "CuBox Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxcs137M2     = new TH1D("hAdapCuBoxcs137M2",  "CuBox Bulk cs137 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxk40M2       = new TH1D("hAdapCuBoxk40M2",    "CuBox Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxmn54M2      = new TH1D("hAdapCuBoxmn54M2",   "CuBox Bulk mn54 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxpb210M2     = new TH1D("hAdapCuBoxpb210M2",  "CuBox Bulk pb210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxth232M2     = new TH1D("hAdapCuBoxth232M2",  "CuBox Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapCuBoxu238M2      = new TH1D("hAdapCuBoxu238M2",   "CuBox Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuBoxSth232M2_1     = new TH1D("hAdapCuBoxSth232M2_1",    "CuBox S th232 M2 1 #mum",     dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSu238M2_1      = new TH1D("hAdapCuBoxSu238M2_1",     "CuBox S u238 M2 1 #mum",      dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxpb210M2_001  = new TH1D("hAdapCuBoxSxpb210M2_001", "CuBox Sx pb210 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxpb210M2_01   = new TH1D("hAdapCuBoxSxpb210M2_01",  "CuBox Sx pb210 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxpb210M2_1    = new TH1D("hAdapCuBoxSxpb210M2_1",   "CuBox Sx pb210 M2 1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxpb210M2_10   = new TH1D("hAdapCuBoxSxpb210M2_10",  "CuBox Sx pb210 M2 10 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxth232M2_001  = new TH1D("hAdapCuBoxSxth232M2_001", "CuBox Sx th232 M2 0.01 #mum",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxth232M2_01   = new TH1D("hAdapCuBoxSxth232M2_01",  "CuBox Sx th232 M2 0.1 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxth232M2_1    = new TH1D("hAdapCuBoxSxth232M2_1",   "CuBox Sx th232 M2 1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxth232M2_10   = new TH1D("hAdapCuBoxSxth232M2_10",  "CuBox Sx th232 M2 10 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxu238M2_001   = new TH1D("hAdapCuBoxSxu238M2_001",  "CuBox Sx u238 M2 0.01 #mum",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxu238M2_01    = new TH1D("hAdapCuBoxSxu238M2_01",   "CuBox Sx u238 M2 0.1 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxu238M2_1     = new TH1D("hAdapCuBoxSxu238M2_1",    "CuBox Sx u238 M2 1 #mum",     dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBoxSxu238M2_10    = new TH1D("hAdapCuBoxSxu238M2_10",   "CuBox Sx u238 M2 10 #mum",    dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuBoxco58M2Sum      = new TH1D("hAdapCuBoxco58M2Sum",   "CuBox Bulk co58 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxco60M2Sum      = new TH1D("hAdapCuBoxco60M2Sum",   "CuBox Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxcs137M2Sum     = new TH1D("hAdapCuBoxcs137M2Sum",  "CuBox Bulk cs137 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxk40M2Sum       = new TH1D("hAdapCuBoxk40M2Sum",    "CuBox Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxmn54M2Sum      = new TH1D("hAdapCuBoxmn54M2Sum",   "CuBox Bulk mn54 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxpb210M2Sum     = new TH1D("hAdapCuBoxpb210M2Sum",  "CuBox Bulk pb210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxth232M2Sum     = new TH1D("hAdapCuBoxth232M2Sum",  "CuBox Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapCuBoxu238M2Sum      = new TH1D("hAdapCuBoxu238M2Sum",   "CuBox Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapCuBoxSth232M2Sum_1     = new TH1D("hAdapCuBoxSth232M2Sum_1",    "CuBox S th232 M2Sum 1 #mum",     dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSu238M2Sum_1      = new TH1D("hAdapCuBoxSu238M2Sum_1",     "CuBox S u238 M2Sum 1 #mum",      dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxpb210M2Sum_001  = new TH1D("hAdapCuBoxSxpb210M2Sum_001", "CuBox Sx pb210 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxpb210M2Sum_01   = new TH1D("hAdapCuBoxSxpb210M2Sum_01",  "CuBox Sx pb210 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxpb210M2Sum_1    = new TH1D("hAdapCuBoxSxpb210M2Sum_1",   "CuBox Sx pb210 M2Sum 1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxpb210M2Sum_10   = new TH1D("hAdapCuBoxSxpb210M2Sum_10",  "CuBox Sx pb210 M2Sum 10 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxth232M2Sum_001  = new TH1D("hAdapCuBoxSxth232M2Sum_001", "CuBox Sx th232 M2Sum 0.01 #mum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxth232M2Sum_01   = new TH1D("hAdapCuBoxSxth232M2Sum_01",  "CuBox Sx th232 M2Sum 0.1 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxth232M2Sum_1    = new TH1D("hAdapCuBoxSxth232M2Sum_1",   "CuBox Sx th232 M2Sum 1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxth232M2Sum_10   = new TH1D("hAdapCuBoxSxth232M2Sum_10",  "CuBox Sx th232 M2Sum 10 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxu238M2Sum_001   = new TH1D("hAdapCuBoxSxu238M2Sum_001",  "CuBox Sx u238 M2Sum 0.01 #mum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxu238M2Sum_01    = new TH1D("hAdapCuBoxSxu238M2Sum_01",   "CuBox Sx u238 M2Sum 0.1 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxu238M2Sum_1     = new TH1D("hAdapCuBoxSxu238M2Sum_1",    "CuBox Sx u238 M2Sum 1 #mum",     dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBoxSxu238M2Sum_10    = new TH1D("hAdapCuBoxSxu238M2Sum_10",   "CuBox Sx u238 M2Sum 10 #mum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

//////////// CuBox + CuFrame M1 and M2

  hAdapCuBox_CuFrameco60M1 = new TH1D("hAdapCuBox_CuFrameco60M1", "CuBox+CuFrame Bulk co60 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFramek40M1 = new TH1D("hAdapCuBox_CuFramek40M1", "CuBox+CuFrame Bulk k40 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFrameth232M1 = new TH1D("hAdapCuBox_CuFrameth232M1", "CuBox+CuFrame Bulk th232 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFrameu238M1 = new TH1D("hAdapCuBox_CuFrameu238M1", "CuBox+CuFrame Bulk u238 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuBox_CuFrameth232M1_10 = new TH1D("hAdapCuBox_CuFrameth232M1_10", "CuBox+CuFrame Sx th232 M1 10 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFrameu238M1_10 = new TH1D("hAdapCuBox_CuFrameu238M1_10", "CuBox+CuFrame Sx u238 M1 10 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFramepb210M1_10 = new TH1D("hAdapCuBox_CuFramepb210M1_10", "CuBox+CuFrame Sx pb210 M1 10 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFramepb210M1_1 = new TH1D("hAdapCuBox_CuFramepb210M1_1", "CuBox+CuFrame Sx pb210 M1 1 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFramepb210M1_01 = new TH1D("hAdapCuBox_CuFramepb210M1_01", "CuBox+CuFrame Sx pb210 M1 0.1 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapCuBox_CuFramepb210M1_001 = new TH1D("hAdapCuBox_CuFramepb210M1_001", "CuBox+CuFrame Sx pb210 M1 0.01 #mum", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapCuBox_CuFrameth232M1_1  = new TH1D("hAdapCuBox_CuFrameth232M1_1", "hAdapCuBox_CuFrameth232M1_1",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M1_1   = new TH1D("hAdapCuBox_CuFrameu238M1_1", "hAdapCuBox_CuFrameu238M1_1",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M1_01  = new TH1D("hAdapCuBox_CuFrameth232M1_01", "hAdapCuBox_CuFrameth232M1_01",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M1_01   = new TH1D("hAdapCuBox_CuFrameu238M1_01", "hAdapCuBox_CuFrameu238M1_01",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M1_001  = new TH1D("hAdapCuBox_CuFrameth232M1_001", "hAdapCuBox_CuFrameth232M1_001",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M1_001   = new TH1D("hAdapCuBox_CuFrameu238M1_001", "hAdapCuBox_CuFrameu238M1_001",    dNBins, dMinEnergy, dMaxEnergy);


  hAdapCuBox_CuFrameco60M2 = new TH1D("hAdapCuBox_CuFrameco60M2", "CuBox+CuFrame Bulk co60 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFramek40M2 = new TH1D("hAdapCuBox_CuFramek40M2", "CuBox+CuFrame Bulk k40 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFrameth232M2 = new TH1D("hAdapCuBox_CuFrameth232M2", "CuBox+CuFrame Bulk th232 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFrameu238M2 = new TH1D("hAdapCuBox_CuFrameu238M2", "CuBox+CuFrame Bulk u238 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuBox_CuFrameth232M2_10 = new TH1D("hAdapCuBox_CuFrameth232M2_10", "CuBox+CuFrame Sx th232 M2 10 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFrameu238M2_10 = new TH1D("hAdapCuBox_CuFrameu238M2_10", "CuBox+CuFrame Sx u238 M2 10 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFramepb210M2_10 = new TH1D("hAdapCuBox_CuFramepb210M2_10", "CuBox+CuFrame Sx pb210 M2 10 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFramepb210M2_1 = new TH1D("hAdapCuBox_CuFramepb210M2_1", "CuBox+CuFrame Sx pb210 M2 1 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFramepb210M2_01 = new TH1D("hAdapCuBox_CuFramepb210M2_01", "CuBox+CuFrame Sx pb210 M2 0.1 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapCuBox_CuFramepb210M2_001 = new TH1D("hAdapCuBox_CuFramepb210M2_001", "CuBox+CuFrame Sx pb210 M2 0.01 #mum", dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapCuBox_CuFrameth232M2_1  = new TH1D("hAdapCuBox_CuFrameth232M2_1", "hAdapCuBox_CuFrameth232M2_1",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2_1   = new TH1D("hAdapCuBox_CuFrameu238M2_1", "hAdapCuBox_CuFrameu238M2_1",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M2_01  = new TH1D("hAdapCuBox_CuFrameth232M2_01", "hAdapCuBox_CuFrameth232M2_01",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2_01   = new TH1D("hAdapCuBox_CuFrameu238M2_01", "hAdapCuBox_CuFrameu238M2_01",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M2_001  = new TH1D("hAdapCuBox_CuFrameth232M2_001", "hAdapCuBox_CuFrameth232M2_001",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2_001   = new TH1D("hAdapCuBox_CuFrameu238M2_001", "hAdapCuBox_CuFrameu238M2_001",    dNBins, dMinEnergy, dMaxEnergy);


  hAdapCuBox_CuFrameco60M2Sum = new TH1D("hAdapCuBox_CuFrameco60M2Sum", "CuBox+CuFrame Bulk co60 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFramek40M2Sum = new TH1D("hAdapCuBox_CuFramek40M2Sum", "CuBox+CuFrame Bulk k40 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFrameth232M2Sum = new TH1D("hAdapCuBox_CuFrameth232M2Sum", "CuBox+CuFrame Bulk th232 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFrameu238M2Sum = new TH1D("hAdapCuBox_CuFrameu238M2Sum", "CuBox+CuFrame Bulk u238 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapCuBox_CuFrameth232M2Sum_10 = new TH1D("hAdapCuBox_CuFrameth232M2Sum_10", "CuBox+CuFrame Sx th232 M2Sum 10 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFrameu238M2Sum_10 = new TH1D("hAdapCuBox_CuFrameu238M2Sum_10", "CuBox+CuFrame Sx u238 M2Sum 10 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFramepb210M2Sum_10 = new TH1D("hAdapCuBox_CuFramepb210M2Sum_10", "CuBox+CuFrame Sx pb210 M2Sum 10 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFramepb210M2Sum_1 = new TH1D("hAdapCuBox_CuFramepb210M2Sum_1", "CuBox+CuFrame Sx pb210 M2Sum 1 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFramepb210M2Sum_01 = new TH1D("hAdapCuBox_CuFramepb210M2Sum_01", "CuBox+CuFrame Sx pb210 M2Sum 0.1 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapCuBox_CuFramepb210M2Sum_001 = new TH1D("hAdapCuBox_CuFramepb210M2Sum_001", "CuBox+CuFrame Sx pb210 M2Sum 0.01 #mum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hAdapCuBox_CuFrameth232M2Sum_1  = new TH1D("hAdapCuBox_CuFrameth232M2Sum_1", "hAdapCuBox_CuFrameth232M2Sum_1",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2Sum_1   = new TH1D("hAdapCuBox_CuFrameu238M2Sum_1", "hAdapCuBox_CuFrameu238M2Sum_1",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M2Sum_01  = new TH1D("hAdapCuBox_CuFrameth232M2Sum_01", "hAdapCuBox_CuFrameth232M2Sum_01",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2Sum_01   = new TH1D("hAdapCuBox_CuFrameu238M2Sum_01", "hAdapCuBox_CuFrameu238M2Sum_01",    dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameth232M2Sum_001  = new TH1D("hAdapCuBox_CuFrameth232M2Sum_001", "hAdapCuBox_CuFrameth232M2Sum_001",  dNBins, dMinEnergy, dMaxEnergy);
  hAdapCuBox_CuFrameu238M2Sum_001   = new TH1D("hAdapCuBox_CuFrameu238M2Sum_001", "hAdapCuBox_CuFrameu238M2Sum_001",    dNBins, dMinEnergy, dMaxEnergy);

////////// 50mK M1 and M2
  hAdap50mKco58M1      = new TH1D("hAdap50mKco58M1",   "50mK Bulk co58 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKco60M1      = new TH1D("hAdap50mKco60M1",   "50mK Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKcs137M1     = new TH1D("hAdap50mKcs137M1",  "50mK Bulk cs137 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKk40M1       = new TH1D("hAdap50mKk40M1",    "50mK Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKmn54M1      = new TH1D("hAdap50mKmn54M1",   "50mK Bulk mn54 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKpb210M1     = new TH1D("hAdap50mKpb210M1",  "50mK Bulk pb210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap50mKth232M1     = new TH1D("hAdap50mKth232M1",  "50mK Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdap50mKu238M1      = new TH1D("hAdap50mKu238M1",   "50mK Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdap50mKco58M2      = new TH1D("hAdap50mKco58M2",   "50mK Bulk co58 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKco60M2      = new TH1D("hAdap50mKco60M2",   "50mK Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKcs137M2     = new TH1D("hAdap50mKcs137M2",  "50mK Bulk cs137 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKk40M2       = new TH1D("hAdap50mKk40M2",    "50mK Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKmn54M2      = new TH1D("hAdap50mKmn54M2",   "50mK Bulk mn54 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKpb210M2     = new TH1D("hAdap50mKpb210M2",  "50mK Bulk pb210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap50mKth232M2     = new TH1D("hAdap50mKth232M2",  "50mK Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdap50mKu238M2      = new TH1D("hAdap50mKu238M2",   "50mK Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdap50mKco58M2Sum      = new TH1D("hAdap50mKco58M2Sum",   "50mK Bulk co58 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKco60M2Sum      = new TH1D("hAdap50mKco60M2Sum",   "50mK Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKcs137M2Sum     = new TH1D("hAdap50mKcs137M2Sum",  "50mK Bulk cs137 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKk40M2Sum       = new TH1D("hAdap50mKk40M2Sum",    "50mK Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKmn54M2Sum      = new TH1D("hAdap50mKmn54M2Sum",   "50mK Bulk mn54 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKpb210M2Sum     = new TH1D("hAdap50mKpb210M2Sum",  "50mK Bulk pb210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap50mKth232M2Sum     = new TH1D("hAdap50mKth232M2Sum",  "50mK Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdap50mKu238M2Sum      = new TH1D("hAdap50mKu238M2Sum",   "50mK Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

////////// 600mK M1 and M2
  hAdap600mKco60M1      = new TH1D("hAdap600mKco60M1",   "600mK Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap600mKk40M1       = new TH1D("hAdap600mKk40M1",    "600mK Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdap600mKth232M1     = new TH1D("hAdap600mKth232M1",  "600mK Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdap600mKu238M1      = new TH1D("hAdap600mKu238M1",   "600mK Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdap600mKco60M2      = new TH1D("hAdap600mKco60M2",   "600mK Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap600mKk40M2       = new TH1D("hAdap600mKk40M2",    "600mK Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdap600mKth232M2     = new TH1D("hAdap600mKth232M2",  "600mK Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdap600mKu238M2      = new TH1D("hAdap600mKu238M2",   "600mK Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);  

  hAdap600mKco60M2Sum      = new TH1D("hAdap600mKco60M2Sum",   "600mK Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap600mKk40M2Sum       = new TH1D("hAdap600mKk40M2Sum",    "600mK Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdap600mKth232M2Sum     = new TH1D("hAdap600mKth232M2Sum",  "600mK Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdap600mKu238M2Sum      = new TH1D("hAdap600mKu238M2Sum",   "600mK Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

////////// Roman Lead M1 and M2
  hAdapPbRombi207M1     = new TH1D("hAdapPbRombi207M1",  "Roman Lead Bulk bi207 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapPbRomco60M1      = new TH1D("hAdapPbRomco60M1",   "Roman Lead Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapPbRomcs137M1     = new TH1D("hAdapPbRomcs137M1",  "Roman Lead Bulk cs137 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapPbRomk40M1       = new TH1D("hAdapPbRomk40M1",    "Roman Lead Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapPbRompb210M1     = new TH1D("hAdapPbRompb210M1",  "Roman Lead Bulk pb210 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapPbRomth232M1     = new TH1D("hAdapPbRomth232M1",  "Roman Lead Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapPbRomu238M1      = new TH1D("hAdapPbRomu238M1",   "Roman Lead Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapPbRombi207M2     = new TH1D("hAdapPbRombi207M2",  "Roman Lead Bulk bi207 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapPbRomco60M2      = new TH1D("hAdapPbRomco60M2",   "Roman Lead Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapPbRomcs137M2     = new TH1D("hAdapPbRomcs137M2",  "Roman Lead Bulk cs137 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapPbRomk40M2       = new TH1D("hAdapPbRomk40M2",    "Roman Lead Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapPbRompb210M2     = new TH1D("hAdapPbRompb210M2",  "Roman Lead Bulk pb210 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapPbRomth232M2     = new TH1D("hAdapPbRomth232M2",  "Roman Lead Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapPbRomu238M2      = new TH1D("hAdapPbRomu238M2",   "Roman Lead Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapPbRombi207M2Sum     = new TH1D("hAdapPbRombi207M2Sum",  "Roman Lead Bulk bi207 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapPbRomco60M2Sum      = new TH1D("hAdapPbRomco60M2Sum",   "Roman Lead Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapPbRomcs137M2Sum     = new TH1D("hAdapPbRomcs137M2Sum",  "Roman Lead Bulk cs137 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapPbRomk40M2Sum       = new TH1D("hAdapPbRomk40M2Sum",    "Roman Lead Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapPbRompb210M2Sum     = new TH1D("hAdapPbRompb210M2Sum",  "Roman Lead Bulk pb210 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapPbRomth232M2Sum     = new TH1D("hAdapPbRomth232M2Sum",  "Roman Lead Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapPbRomu238M2Sum      = new TH1D("hAdapPbRomu238M2Sum",   "Roman Lead Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);


///////// Internal Shields M1 and M2
  hAdapInternalco60M1 = new TH1D("hAdapInternalco60M1", "Internal Shields Bulk co60 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapInternalk40M1 = new TH1D("hAdapInternalk40M1", "Internal Shields Bulk k40 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapInternalth232M1 = new TH1D("hAdapInternalth232M1", "Internal Shields Bulk th232 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapInternalu238M1 = new TH1D("hAdapInternalu238M1", "Internal Shields Bulk u238 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapInternalco60M2 = new TH1D("hAdapInternalco60M2", "Internal Shields Bulk co60 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapInternalk40M2 = new TH1D("hAdapInternalk40M2", "Internal Shields Bulk k40 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapInternalth232M2 = new TH1D("hAdapInternalth232M2", "Internal Shields Bulk th232 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapInternalu238M2 = new TH1D("hAdapInternalu238M2", "Internal Shields Bulk u238 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapInternalco60M2Sum = new TH1D("hAdapInternalco60M2Sum", "Internal Shields Bulk co60 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapInternalk40M2Sum = new TH1D("hAdapInternalk40M2Sum", "Internal Shields Bulk k40 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapInternalth232M2Sum = new TH1D("hAdapInternalth232M2Sum", "Internal Shields Bulk th232 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapInternalu238M2Sum = new TH1D("hAdapInternalu238M2Sum", "Internal Shields Bulk u238 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  

////////////// Main bath M1 and M2
  hAdapMBco60M1      = new TH1D("hAdapMBco60M1",   "Main Bath Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapMBk40M1       = new TH1D("hAdapMBk40M1",    "Main Bath Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapMBth232M1     = new TH1D("hAdapMBth232M1",  "Main Bath Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapMBu238M1      = new TH1D("hAdapMBu238M1",   "Main Bath Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapMBco60M2      = new TH1D("hAdapMBco60M2",   "Main Bath Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapMBk40M2       = new TH1D("hAdapMBk40M2",    "Main Bath Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapMBth232M2     = new TH1D("hAdapMBth232M2",  "Main Bath Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapMBu238M2      = new TH1D("hAdapMBu238M2",   "Main Bath Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);  

  hAdapMBco60M2Sum      = new TH1D("hAdapMBco60M2Sum",   "Main Bath Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapMBk40M2Sum       = new TH1D("hAdapMBk40M2Sum",    "Main Bath Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapMBth232M2Sum     = new TH1D("hAdapMBth232M2Sum",  "Main Bath Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapMBu238M2Sum      = new TH1D("hAdapMBu238M2Sum",   "Main Bath Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  

/////////// Super Insulation M1 and M2
  hAdapSIk40M1       = new TH1D("hAdapSIk40M1",    "Super Insulation Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1); 
  hAdapSIth232M1     = new TH1D("hAdapSIth232M1",  "Super Insulation Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapSIu238M1      = new TH1D("hAdapSIu238M1",   "Super Insulation Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapSIk40M2       = new TH1D("hAdapSIk40M2",    "Super Insulation Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapSIth232M2     = new TH1D("hAdapSIth232M2",  "Super Insulation Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapSIu238M2      = new TH1D("hAdapSIu238M2",   "Super Insulation Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);

  hAdapSIk40M2Sum       = new TH1D("hAdapSIk40M2Sum",    "Super Insulation Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapSIth232M2Sum     = new TH1D("hAdapSIth232M2Sum",  "Super Insulation Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapSIu238M2Sum      = new TH1D("hAdapSIu238M2Sum",   "Super Insulation Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

//////////// IVC M1 and M2
  hAdapIVCco60M1      = new TH1D("hAdapIVCco60M1",   "IVC Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapIVCk40M1       = new TH1D("hAdapIVCk40M1",    "IVC Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapIVCth232M1     = new TH1D("hAdapIVCth232M1",  "IVC Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapIVCu238M1      = new TH1D("hAdapIVCu238M1",   "IVC Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapIVCco60M2      = new TH1D("hAdapIVCco60M2",   "IVC Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapIVCk40M2       = new TH1D("hAdapIVCk40M2",    "IVC Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapIVCth232M2     = new TH1D("hAdapIVCth232M2",  "IVC Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapIVCu238M2      = new TH1D("hAdapIVCu238M2",   "IVC Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);  

  hAdapIVCco60M2Sum      = new TH1D("hAdapIVCco60M2Sum",   "IVC Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapIVCk40M2Sum       = new TH1D("hAdapIVCk40M2Sum",    "IVC Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapIVCth232M2Sum     = new TH1D("hAdapIVCth232M2Sum",  "IVC Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapIVCu238M2Sum      = new TH1D("hAdapIVCu238M2Sum",   "IVC Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum); 

////////////// OVC M1 and M2
  hAdapOVCco60M1      = new TH1D("hAdapOVCco60M1",   "OVC Bulk co60 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapOVCk40M1       = new TH1D("hAdapOVCk40M1",    "OVC Bulk k40 M1",    dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapOVCth232M1     = new TH1D("hAdapOVCth232M1",  "OVC Bulk th232 M1",  dAdaptiveBinsM1, dAdaptiveArrayM1);  
  hAdapOVCu238M1      = new TH1D("hAdapOVCu238M1",   "OVC Bulk u238 M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapOVCco60M2      = new TH1D("hAdapOVCco60M2",   "OVC Bulk co60 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapOVCk40M2       = new TH1D("hAdapOVCk40M2",    "OVC Bulk k40 M2",    dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapOVCth232M2     = new TH1D("hAdapOVCth232M2",  "OVC Bulk th232 M2",  dAdaptiveBinsM2, dAdaptiveArrayM2);  
  hAdapOVCu238M2      = new TH1D("hAdapOVCu238M2",   "OVC Bulk u238 M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);  

  hAdapOVCco60M2Sum      = new TH1D("hAdapOVCco60M2Sum",   "OVC Bulk co60 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapOVCk40M2Sum       = new TH1D("hAdapOVCk40M2Sum",    "OVC Bulk k40 M2Sum",    dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  hAdapOVCth232M2Sum     = new TH1D("hAdapOVCth232M2Sum",  "OVC Bulk th232 M2Sum",  dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  
  hAdapOVCu238M2Sum      = new TH1D("hAdapOVCu238M2Sum",   "OVC Bulk u238 M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);  

/////////// External Sources M1 and M2
  hAdapExtPbbi210M1 = new TH1D("hAdapExtPbbi210M1", "External Lead Bulk bi210 M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapExtPbbi210M2 = new TH1D("hAdapExtPbbi210M2", "External Lead Bulk bi210 M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapExtPbbi210M2Sum = new TH1D("hAdapExtPbbi210M2Sum", "External Lead Bulk bi210 M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

/////////// Fudge Factors
  hAdapFudge661M1 = new TH1D("hAdapFudge661M1", "Fudge Factor 661M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapFudge803M1 = new TH1D("hAdapFudge803M1", "Fudge Factor 803M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hAdapFudge1063M1 = new TH1D("hAdapFudge1063M1", "Fudge Factor 1063M1", dAdaptiveBinsM1, dAdaptiveArrayM1);

  hAdapFudge661M2 = new TH1D("hAdapFudge661M2", "Fudge Factor 661M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapFudge803M2 = new TH1D("hAdapFudge803M2", "Fudge Factor 803M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hAdapFudge1063M2 = new TH1D("hAdapFudge1063M2", "Fudge Factor 1063M2", dAdaptiveBinsM2, dAdaptiveArrayM2);

  hEnergyScaleDummyM1 = new TH1D("hEnergyScaleDummyM1",   "Energy Scale M1",   dAdaptiveBinsM1, dAdaptiveArrayM1);
  hEnergyScaleDummyM2 = new TH1D("hEnergyScaleDummyM2",   "Energy Scale M2",   dAdaptiveBinsM2, dAdaptiveArrayM2);
  hEnergyScaleDummyM2Sum = new TH1D("hEnergyScaleDummyM2Sum",   "Energy Scale M2Sum",   dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);

  hChiSquaredProgressM1 = new TH1D("hChiSquaredProgressM1", "Chi Squared Progress", dAdaptiveBinsM1, dAdaptiveArrayM1);
  hChiSquaredProgressM2 = new TH1D("hChiSquaredProgressM2", "Chi Squared Progress", dAdaptiveBinsM2, dAdaptiveArrayM2);
  hChiSquaredProgressM2Sum = new TH1D("hChiSquaredProgressM2Sum", "Chi Squared Progress", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);


  // Loads all of the PDFs from file
  Initialize();

/*
  // Add 2nbb events to background
  SanityCheck();
  // Set Error Bars to 0 for MC histograms so I don't go crazy
  for(int i = 1; i <= dAdaptiveBinsM1; i++)
  {
    fAdapDataHistoM1->SetBinError(i, 0);
  }

  for(int i = 1; i <= dAdaptiveBinsM2; i++)
  {
    fAdapDataHistoM2->SetBinError(i, 0);
  }

  for(int i = 1; i <= dAdaptiveBinsM2Sum; i++)
  {
    fAdapDataHistoM2Sum->SetBinError(i, 0);
  }
*/

  dBestChiSq = 0; // Chi-Squared from best fit (for ProfileNLL calculation)
  // Do the fit now if no other tests are needed 
  nLoop = 0;
  DoTheFitAdaptive(0,0);
  // DoTheFitAdaptive(0.0674202742, 0.0263278758);  
  // ProfileNLL(0.0685222152, 3968.95); 
  // ProfileNLL2D(0.0674202742, 0.0000003189, 3754);

  // Number of Toy fits
  if(bToyData)ToyFit(1);

}

// Probably needs updating  
TBackgroundModel::~TBackgroundModel()
{
  delete fDataHistoTot;
  delete fDataHistoM1;
  delete fDataHistoM2;
  delete fDataHistoM2Sum;

  delete fAdapDataHistoM1;
  delete fAdapDataHistoM2;
  delete fAdapDataHistoM2Sum;


  // Updated 01-20-2015
  // Total PDFs M1
  delete fModelTotM1;
  delete fModelTotthM1;
  delete fModelTotuM1;
  delete fModelTotkM1;
  delete fModelTotcoM1;
  delete fModelTotmnM1;
  delete fModelTotNDBDM1;
  delete fModelTot2NDBDM1;
  delete fModelTotbiM1;
  delete fModelTotbi2M1;  
  delete fModelTotptM1;
  delete fModelTotpbM1;
  delete fModelTotcsM1;
  delete fModelTotco2M1;
  delete fModelTotteo2M1;

  delete fModelTotSthM1;
  delete fModelTotSuM1;
  delete fModelTotSpoM1;
  delete fModelTotSpbM1;
  delete fModelTotExtM1;

  delete fModelTotAdapM1;
  delete fModelTotAdapthM1;
  delete fModelTotAdapuM1;
  delete fModelTotAdapkM1;
  delete fModelTotAdapcoM1;
  delete fModelTotAdapmnM1;
  delete fModelTotAdapNDBDM1;
  delete fModelTotAdap2NDBDM1;
  delete fModelTotAdapbiM1;
  delete fModelTotAdapbi2M1;
  delete fModelTotAdapptM1;
  delete fModelTotAdappbM1;
  delete fModelTotAdapcsM1;
  delete fModelTotAdapco2M1;
  delete fModelTotAdapteo2M1;

  delete fModelTotAdapSthM1;
  delete fModelTotAdapSuM1;
  delete fModelTotAdapSpoM1;
  delete fModelTotAdapSpbM1;
  delete fModelTotAdapExtM1;


  delete fModelTotAdapAlphaM1;
  delete fModelTotAdapAlphaHighM1;
  delete fModelTotAdapAlphaLowM1;

  // Total PDFs M2
  delete fModelTotM2;
  delete fModelTotthM2;
  delete fModelTotuM2;
  delete fModelTotkM2;
  delete fModelTotcoM2;
  delete fModelTotmnM2;
  delete fModelTotNDBDM2;
  delete fModelTot2NDBDM2;
  delete fModelTotbiM2;
  delete fModelTotbi2M2;
  delete fModelTotptM2;
  delete fModelTotpbM2;
  delete fModelTotcsM2;
  delete fModelTotco2M2;
  delete fModelTotteo2M2;

  delete fModelTotSthM2;
  delete fModelTotSuM2;
  delete fModelTotSpoM2;
  delete fModelTotSpbM2;
  delete fModelTotExtM2;

  delete fModelTotAdapM2;
  delete fModelTotAdapthM2;
  delete fModelTotAdapuM2;
  delete fModelTotAdapkM2;
  delete fModelTotAdapcoM2;
  delete fModelTotAdapmnM2;
  delete fModelTotAdapNDBDM2;
  delete fModelTotAdap2NDBDM2;
  delete fModelTotAdapbiM2;
  delete fModelTotAdapbi2M2;
  delete fModelTotAdapptM2;
  delete fModelTotAdappbM2;
  delete fModelTotAdapcsM2;
  delete fModelTotAdapco2M2;
  delete fModelTotAdapteo2M2;

  delete fModelTotAdapSthM2;
  delete fModelTotAdapSuM2;
  delete fModelTotAdapSpoM2;
  delete fModelTotAdapSpbM2;
  delete fModelTotAdapExtM2;

  // Total PDFs M2Sum
  delete fModelTotM2Sum;
  delete fModelTotthM2Sum;
  delete fModelTotuM2Sum;
  delete fModelTotkM2Sum;
  delete fModelTotcoM2Sum;
  delete fModelTotmnM2Sum;
  delete fModelTotNDBDM2Sum;
  delete fModelTot2NDBDM2Sum;
  delete fModelTotbiM2Sum;
  delete fModelTotbi2M2Sum;
  delete fModelTotptM2Sum;
  delete fModelTotpbM2Sum;
  delete fModelTotcsM2Sum;
  delete fModelTotco2M2Sum;
  delete fModelTotteo2M2Sum;

  delete fModelTotSthM2Sum;
  delete fModelTotSuM2Sum;
  delete fModelTotSpoM2Sum;
  delete fModelTotSpbM2Sum;
  delete fModelTotExtM2Sum;

  delete fModelTotAdapM2Sum;
  delete fModelTotAdapthM2Sum;
  delete fModelTotAdapuM2Sum;
  delete fModelTotAdapkM2Sum;
  delete fModelTotAdapcoM2Sum;
  delete fModelTotAdapmnM2Sum;
  delete fModelTotAdapNDBDM2Sum;
  delete fModelTotAdap2NDBDM2Sum;
  delete fModelTotAdapbiM2Sum;
  delete fModelTotAdapbi2M2Sum;
  delete fModelTotAdapptM2Sum;
  delete fModelTotAdappbM2Sum;
  delete fModelTotAdapcsM2Sum;
  delete fModelTotAdapco2M2Sum;
  delete fModelTotAdapteo2M2Sum;

  delete fModelTotAdapSthM2Sum;
  delete fModelTotAdapSuM2Sum;
  delete fModelTotAdapSpoM2Sum;
  delete fModelTotAdapSpbM2Sum;
  delete fModelTotAdapExtM2Sum;

  delete hTeO20nuM1;
  delete hTeO22nuM1;
  delete hTeO2co60M1;
  delete hTeO2k40M1;
  delete hTeO2pb210M1;
  delete hTeO2po210M1;
  delete hTeO2te125M1;
  delete hTeO2th232M1;
  delete hTeO2th228M1;
  delete hTeO2ra226M1;
  delete hTeO2rn222M1;
  delete hTeO2u238M1;
  delete hTeO2th230M1;
  delete hTeO2u234M1;

  delete hTeO2th232onlyM1;
  delete hTeO2ra228pb208M1;
  delete hTeO2th230onlyM1;

  delete hTeO2Sxth232onlyM1_001;
  delete hTeO2Sxra228pb208M1_001;
  delete hTeO2Sxu238th230M1_001;
  delete hTeO2Sxth230onlyM1_001;
  delete hTeO2Sxra226pb210M1_001;
  delete hTeO2Sxpb210M1_0001;

  delete hTeO2Spb210M1_01;
  delete hTeO2Spo210M1_001;
  delete hTeO2Spo210M1_01;
  delete hTeO2Sth232M1_01;
  delete hTeO2Su238M1_01;
  delete hTeO2Sxpb210M1_001;
  delete hTeO2Sxpb210M1_01;
  delete hTeO2Sxpb210M1_1;
  delete hTeO2Sxpb210M1_10;
  delete hTeO2Sxpo210M1_001;
  delete hTeO2Sxpo210M1_01;
  delete hTeO2Sxpo210M1_1;
  delete hTeO2Sxth232M1_001;
  delete hTeO2Sxth232M1_01;
  delete hTeO2Sxth232M1_1;
  delete hTeO2Sxth232M1_10;
  delete hTeO2Sxu238M1_001;
  delete hTeO2Sxu238M1_01;
  delete hTeO2Sxu238M1_1;
  delete hTeO2Sxu238M1_10;

  delete hTeO20nuM2;
  delete hTeO22nuM2;
  delete hTeO2co60M2;
  delete hTeO2k40M2;
  delete hTeO2pb210M2;
  delete hTeO2po210M2;
  delete hTeO2te125M2;
  delete hTeO2th232M2;
  delete hTeO2th228M2;
  delete hTeO2ra226M2;
  delete hTeO2rn222M2;
  delete hTeO2u238M2;
  delete hTeO2th230M2;
  delete hTeO2u234M2;

  delete hTeO2th232onlyM2;
  delete hTeO2ra228pb208M2;
  delete hTeO2th230onlyM2;

  delete hTeO2Sxth232onlyM2_001;
  delete hTeO2Sxra228pb208M2_001;
  delete hTeO2Sxu238th230M2_001;
  delete hTeO2Sxth230onlyM2_001;
  delete hTeO2Sxra226pb210M2_001;
  delete hTeO2Sxpb210M2_0001;


  delete hTeO2Spb210M2_01;
  delete hTeO2Spo210M2_001;
  delete hTeO2Spo210M2_01;
  delete hTeO2Sth232M2_01;
  delete hTeO2Su238M2_01;
  delete hTeO2Sxpb210M2_001;
  delete hTeO2Sxpb210M2_01;
  delete hTeO2Sxpb210M2_1;
  delete hTeO2Sxpb210M2_10;
  delete hTeO2Sxpo210M2_001;
  delete hTeO2Sxpo210M2_01;
  delete hTeO2Sxpo210M2_1;
  delete hTeO2Sxth232M2_001;
  delete hTeO2Sxth232M2_01;
  delete hTeO2Sxth232M2_1;
  delete hTeO2Sxth232M2_10;
  delete hTeO2Sxu238M2_001;
  delete hTeO2Sxu238M2_01;
  delete hTeO2Sxu238M2_1;
  delete hTeO2Sxu238M2_10;

  delete hTeO20nuM2Sum;
  delete hTeO22nuM2Sum;
  delete hTeO2co60M2Sum;
  delete hTeO2k40M2Sum;
  delete hTeO2pb210M2Sum;
  delete hTeO2po210M2Sum;
  delete hTeO2te125M2Sum;
  delete hTeO2th232M2Sum;
  delete hTeO2th228M2Sum;
  delete hTeO2ra226M2Sum;
  delete hTeO2rn222M2Sum;
  delete hTeO2u238M2Sum;
  delete hTeO2th230M2Sum;
  delete hTeO2u234M2Sum;

  delete hTeO2th232onlyM2Sum;
  delete hTeO2ra228pb208M2Sum;
  delete hTeO2th230onlyM2Sum;

  delete hTeO2Sxth232onlyM2Sum_001;
  delete hTeO2Sxra228pb208M2Sum_001;
  delete hTeO2Sxu238th230M2Sum_001;
  delete hTeO2Sxth230onlyM2Sum_001;
  delete hTeO2Sxra226pb210M2Sum_001;
  delete hTeO2Sxpb210M2Sum_0001;

  delete hTeO2Spb210M2Sum_01;
  delete hTeO2Spo210M2Sum_001;
  delete hTeO2Spo210M2Sum_01;
  delete hTeO2Sth232M2Sum_01;
  delete hTeO2Su238M2Sum_01;
  delete hTeO2Sxpb210M2Sum_001;
  delete hTeO2Sxpb210M2Sum_01;
  delete hTeO2Sxpb210M2Sum_1;
  delete hTeO2Sxpb210M2Sum_10;
  delete hTeO2Sxpo210M2Sum_001;
  delete hTeO2Sxpo210M2Sum_01;
  delete hTeO2Sxpo210M2Sum_1;
  delete hTeO2Sxth232M2Sum_001;
  delete hTeO2Sxth232M2Sum_01;
  delete hTeO2Sxth232M2Sum_1;
  delete hTeO2Sxth232M2Sum_10;
  delete hTeO2Sxu238M2Sum_001;
  delete hTeO2Sxu238M2Sum_01;
  delete hTeO2Sxu238M2Sum_1;
  delete hTeO2Sxu238M2Sum_10;

  delete hCuFrameco58M1;
  delete hCuFrameco60M1;
  delete hCuFramecs137M1;
  delete hCuFramek40M1;
  delete hCuFramemn54M1;
  delete hCuFramepb210M1;
  delete hCuFrameth232M1;
  delete hCuFrameu238M1;

  delete hCuFrameSth232M1_1;
  delete hCuFrameSu238M1_1;
  delete hCuFrameSxpb210M1_001;
  delete hCuFrameSxpb210M1_01;
  delete hCuFrameSxpb210M1_1;
  delete hCuFrameSxpb210M1_10;
  delete hCuFrameSxth232M1_001;
  delete hCuFrameSxth232M1_01;
  delete hCuFrameSxth232M1_1;
  delete hCuFrameSxth232M1_10;
  delete hCuFrameSxu238M1_001;
  delete hCuFrameSxu238M1_01;
  delete hCuFrameSxu238M1_1;
  delete hCuFrameSxu238M1_10;

  delete hCuFrameco58M2;
  delete hCuFrameco60M2;
  delete hCuFramecs137M2;
  delete hCuFramek40M2;
  delete hCuFramemn54M2;
  delete hCuFramepb210M2;
  delete hCuFrameth232M2;
  delete hCuFrameu238M2;

  delete hCuFrameSth232M2_1;
  delete hCuFrameSu238M2_1;
  delete hCuFrameSxpb210M2_001;
  delete hCuFrameSxpb210M2_01;
  delete hCuFrameSxpb210M2_1;
  delete hCuFrameSxpb210M2_10;
  delete hCuFrameSxth232M2_001;
  delete hCuFrameSxth232M2_01;
  delete hCuFrameSxth232M2_1;
  delete hCuFrameSxth232M2_10;
  delete hCuFrameSxu238M2_001;
  delete hCuFrameSxu238M2_01;
  delete hCuFrameSxu238M2_1;
  delete hCuFrameSxu238M2_10;

  delete hCuFrameco58M2Sum;
  delete hCuFrameco60M2Sum;
  delete hCuFramecs137M2Sum;
  delete hCuFramek40M2Sum;
  delete hCuFramemn54M2Sum;
  delete hCuFramepb210M2Sum;
  delete hCuFrameth232M2Sum;
  delete hCuFrameu238M2Sum;

  delete hCuFrameSth232M2Sum_1;
  delete hCuFrameSu238M2Sum_1;
  delete hCuFrameSxpb210M2Sum_001;
  delete hCuFrameSxpb210M2Sum_01;
  delete hCuFrameSxpb210M2Sum_1;
  delete hCuFrameSxpb210M2Sum_10;
  delete hCuFrameSxth232M2Sum_001;
  delete hCuFrameSxth232M2Sum_01;
  delete hCuFrameSxth232M2Sum_1;
  delete hCuFrameSxth232M2Sum_10;
  delete hCuFrameSxu238M2Sum_001;
  delete hCuFrameSxu238M2Sum_01;
  delete hCuFrameSxu238M2Sum_1;
  delete hCuFrameSxu238M2Sum_10;

  delete hCuBoxco58M1;
  delete hCuBoxco60M1;
  delete hCuBoxcs137M1;
  delete hCuBoxk40M1;
  delete hCuBoxmn54M1;
  delete hCuBoxpb210M1;
  delete hCuBoxth232M1;
  delete hCuBoxu238M1;  

  delete hCuBoxSth232M1_1;
  delete hCuBoxSu238M1_1;
  delete hCuBoxSxpb210M1_001;
  delete hCuBoxSxpb210M1_01;
  delete hCuBoxSxpb210M1_1;
  delete hCuBoxSxpb210M1_10;
  delete hCuBoxSxth232M1_001;
  delete hCuBoxSxth232M1_01;
  delete hCuBoxSxth232M1_1;
  delete hCuBoxSxth232M1_10;
  delete hCuBoxSxu238M1_001;
  delete hCuBoxSxu238M1_01;
  delete hCuBoxSxu238M1_1;
  delete hCuBoxSxu238M1_10;

  delete hCuBoxco58M2;
  delete hCuBoxco60M2;
  delete hCuBoxcs137M2;
  delete hCuBoxk40M2;
  delete hCuBoxmn54M2;
  delete hCuBoxpb210M2;
  delete hCuBoxth232M2;
  delete hCuBoxu238M2;  

  delete hCuBoxSth232M2_1;
  delete hCuBoxSu238M2_1;
  delete hCuBoxSxpb210M2_001;
  delete hCuBoxSxpb210M2_01;
  delete hCuBoxSxpb210M2_1;
  delete hCuBoxSxpb210M2_10;
  delete hCuBoxSxth232M2_001;
  delete hCuBoxSxth232M2_01;
  delete hCuBoxSxth232M2_1;
  delete hCuBoxSxth232M2_10;
  delete hCuBoxSxu238M2_001;
  delete hCuBoxSxu238M2_01;
  delete hCuBoxSxu238M2_1;
  delete hCuBoxSxu238M2_10;

  delete hCuBoxco58M2Sum;
  delete hCuBoxco60M2Sum;
  delete hCuBoxcs137M2Sum;
  delete hCuBoxk40M2Sum;
  delete hCuBoxmn54M2Sum;
  delete hCuBoxpb210M2Sum;
  delete hCuBoxth232M2Sum;
  delete hCuBoxu238M2Sum; 

  delete hCuBoxSth232M2Sum_1;
  delete hCuBoxSu238M2Sum_1;
  delete hCuBoxSxpb210M2Sum_001;
  delete hCuBoxSxpb210M2Sum_01;
  delete hCuBoxSxpb210M2Sum_1;
  delete hCuBoxSxpb210M2Sum_10;
  delete hCuBoxSxth232M2Sum_001;
  delete hCuBoxSxth232M2Sum_01;
  delete hCuBoxSxth232M2Sum_1;
  delete hCuBoxSxth232M2Sum_10;
  delete hCuBoxSxu238M2Sum_001;
  delete hCuBoxSxu238M2Sum_01;
  delete hCuBoxSxu238M2Sum_1;
  delete hCuBoxSxu238M2Sum_10;

  delete hCuBox_CuFrameco60M1;
  delete hCuBox_CuFramek40M1;
  delete hCuBox_CuFrameth232M1;
  delete hCuBox_CuFrameu238M1;

  delete hCuBox_CuFrameth232M1_10;
  delete hCuBox_CuFrameu238M1_10;
  delete hCuBox_CuFramepb210M1_10;
  delete hCuBox_CuFramepb210M1_01;

  delete hCuBox_CuFrameco60M2;
  delete hCuBox_CuFramek40M2;
  delete hCuBox_CuFrameth232M2;
  delete hCuBox_CuFrameu238M2;

  delete hCuBox_CuFrameth232M2_10;
  delete hCuBox_CuFrameu238M2_10;
  delete hCuBox_CuFramepb210M2_10;
  delete hCuBox_CuFramepb210M2_01;

  delete hCuBox_CuFrameco60M2Sum;
  delete hCuBox_CuFramek40M2Sum;
  delete hCuBox_CuFrameth232M2Sum;
  delete hCuBox_CuFrameu238M2Sum;

  delete hCuBox_CuFrameth232M2Sum_10;
  delete hCuBox_CuFrameu238M2Sum_10;
  delete hCuBox_CuFramepb210M2Sum_10;
  delete hCuBox_CuFramepb210M2Sum_01;

  delete h50mKco58M1;
  delete h50mKco60M1;
  delete h50mKcs137M1;
  delete h50mKk40M1;
  delete h50mKmn54M1;
  delete h50mKpb210M1;
  delete h50mKth232M1;
  delete h50mKu238M1;   

  delete h50mKco58M2;
  delete h50mKco60M2;
  delete h50mKcs137M2;
  delete h50mKk40M2;
  delete h50mKmn54M2;
  delete h50mKpb210M2;
  delete h50mKth232M2;
  delete h50mKu238M2; 

  delete h50mKco58M2Sum;
  delete h50mKco60M2Sum;
  delete h50mKcs137M2Sum;
  delete h50mKk40M2Sum;
  delete h50mKmn54M2Sum;
  delete h50mKpb210M2Sum;
  delete h50mKth232M2Sum;
  delete h50mKu238M2Sum;

  delete h600mKco60M1;
  delete h600mKk40M1;
  delete h600mKth232M1;
  delete h600mKu238M1;    

  delete h600mKco60M2;
  delete h600mKk40M2;
  delete h600mKth232M2;
  delete h600mKu238M2;  

  delete h600mKco60M2Sum;
  delete h600mKk40M2Sum;
  delete h600mKth232M2Sum;
  delete h600mKu238M2Sum; 

  delete hInternalco60M1;
  delete hInternalk40M1;
  delete hInternalth232M1;
  delete hInternalu238M1;

  delete hInternalco60M2;
  delete hInternalk40M2;
  delete hInternalth232M2;
  delete hInternalu238M2;

  delete hInternalco60M2Sum;
  delete hInternalk40M2Sum;
  delete hInternalth232M2Sum;
  delete hInternalu238M2Sum;  


  delete hPbRombi207M1;
  delete hPbRomco60M1;
  delete hPbRomcs137M1;
  delete hPbRomk40M1;
  delete hPbRompb210M1;
  delete hPbRomth232M1;
  delete hPbRomu238M1;    

  delete hPbRombi207M2;
  delete hPbRomco60M2;
  delete hPbRomcs137M2;
  delete hPbRomk40M2;
  delete hPbRompb210M2;
  delete hPbRomth232M2;
  delete hPbRomu238M2;    

  delete hPbRombi207M2Sum;
  delete hPbRomco60M2Sum;
  delete hPbRomcs137M2Sum;
  delete hPbRomk40M2Sum;
  delete hPbRompb210M2Sum;
  delete hPbRomth232M2Sum;
  delete hPbRomu238M2Sum; 

  delete hMBco60M1;
  delete hMBk40M1;
  delete hMBth232M1;
  delete hMBu238M1;   

  delete hMBco60M2;
  delete hMBk40M2;
  delete hMBth232M2;
  delete hMBu238M2; 

  delete hMBco60M2Sum;
  delete hMBk40M2Sum;
  delete hMBth232M2Sum;
  delete hMBu238M2Sum;  

  delete hSIk40M1;
  delete hSIth232M1;
  delete hSIu238M1;

  delete hSIk40M2;
  delete hSIth232M2;
  delete hSIu238M2;

  delete hSIk40M2Sum;
  delete hSIth232M2Sum;
  delete hSIu238M2Sum;

  delete hExtPbbi210M1;

  delete hExtPbbi210M2;
  
  delete hExtPbbi210M2Sum;

  delete hIVCco60M1;
  delete hIVCk40M1;
  delete hIVCth232M1;
  delete hIVCu238M1;    

  delete hIVCco60M2;
  delete hIVCk40M2;
  delete hIVCth232M2;
  delete hIVCu238M2;  

  delete hIVCco60M2Sum;
  delete hIVCk40M2Sum;
  delete hIVCth232M2Sum;
  delete hIVCu238M2Sum; 

  delete hOVCco60M1;
  delete hOVCk40M1;
  delete hOVCth232M1;
  delete hOVCu238M1;    

  delete hOVCco60M2;
  delete hOVCk40M2;
  delete hOVCth232M2;
  delete hOVCu238M2;  

  delete hOVCco60M2Sum;
  delete hOVCk40M2Sum;
  delete hOVCth232M2Sum;
  delete hOVCu238M2Sum; 

  delete hAdapTeO20nuM1;
  delete hAdapTeO22nuM1;
  delete hAdapTeO2co60M1;
  delete hAdapTeO2k40M1;
  delete hAdapTeO2pb210M1;
  delete hAdapTeO2po210M1;
  delete hAdapTeO2te125M1;
  delete hAdapTeO2th232M1;
  delete hAdapTeO2th228M1;
  delete hAdapTeO2ra226M1;
  delete hAdapTeO2rn222M1;
  delete hAdapTeO2u238M1;
  delete hAdapTeO2th230M1;
  delete hAdapTeO2u234M1;

  delete hAdapTeO2th232onlyM1;
  delete hAdapTeO2ra228pb208M1;
  delete hAdapTeO2th230onlyM1;

  delete hAdapTeO2Sxth232onlyM1_001;
  delete hAdapTeO2Sxra228pb208M1_001;
  delete hAdapTeO2Sxu238th230M1_001;
  delete hAdapTeO2Sxth230onlyM1_001;
  delete hAdapTeO2Sxra226pb210M1_001;
  delete hAdapTeO2Sxpb210M1_0001;

  delete hAdapTeO2Spb210M1_01;
  delete hAdapTeO2Spo210M1_001;
  delete hAdapTeO2Spo210M1_01;
  delete hAdapTeO2Sth232M1_01;
  delete hAdapTeO2Su238M1_01;
  delete hAdapTeO2Sxpb210M1_001;
  delete hAdapTeO2Sxpb210M1_01;
  delete hAdapTeO2Sxpb210M1_1;
  delete hAdapTeO2Sxpb210M1_10;
  delete hAdapTeO2Sxpo210M1_001;
  delete hAdapTeO2Sxpo210M1_01;
  delete hAdapTeO2Sxpo210M1_1;
  delete hAdapTeO2Sxth232M1_001;
  delete hAdapTeO2Sxth232M1_01;
  delete hAdapTeO2Sxth232M1_1;
  delete hAdapTeO2Sxth232M1_10;
  delete hAdapTeO2Sxu238M1_001;
  delete hAdapTeO2Sxu238M1_01;
  delete hAdapTeO2Sxu238M1_1;
  delete hAdapTeO2Sxu238M1_10;

  delete hAdapTeO20nuM2;
  delete hAdapTeO22nuM2;
  delete hAdapTeO2co60M2;
  delete hAdapTeO2k40M2;
  delete hAdapTeO2pb210M2;
  delete hAdapTeO2po210M2;
  delete hAdapTeO2te125M2;
  delete hAdapTeO2th232M2;
  delete hAdapTeO2th228M2;
  delete hAdapTeO2ra226M2;
  delete hAdapTeO2rn222M2;
  delete hAdapTeO2u238M2;
  delete hAdapTeO2th230M2;
  delete hAdapTeO2u234M2;

  delete hAdapTeO2th232onlyM2;
  delete hAdapTeO2ra228pb208M2;
  delete hAdapTeO2th230onlyM2;

  delete hAdapTeO2Sxth232onlyM2_001;
  delete hAdapTeO2Sxra228pb208M2_001;
  delete hAdapTeO2Sxu238th230M2_001;
  delete hAdapTeO2Sxth230onlyM2_001;
  delete hAdapTeO2Sxra226pb210M2_001;
  delete hAdapTeO2Sxpb210M2_0001;

  delete hAdapTeO2Spb210M2_01;
  delete hAdapTeO2Spo210M2_001;
  delete hAdapTeO2Spo210M2_01;
  delete hAdapTeO2Sth232M2_01;
  delete hAdapTeO2Su238M2_01;
  delete hAdapTeO2Sxpb210M2_001;
  delete hAdapTeO2Sxpb210M2_01;
  delete hAdapTeO2Sxpb210M2_1;
  delete hAdapTeO2Sxpb210M2_10;
  delete hAdapTeO2Sxpo210M2_001;
  delete hAdapTeO2Sxpo210M2_01;
  delete hAdapTeO2Sxpo210M2_1;
  delete hAdapTeO2Sxth232M2_001;
  delete hAdapTeO2Sxth232M2_01;
  delete hAdapTeO2Sxth232M2_1;
  delete hAdapTeO2Sxth232M2_10;
  delete hAdapTeO2Sxu238M2_001;
  delete hAdapTeO2Sxu238M2_01;
  delete hAdapTeO2Sxu238M2_1;
  delete hAdapTeO2Sxu238M2_10;

  delete hAdapTeO20nuM2Sum;
  delete hAdapTeO22nuM2Sum;
  delete hAdapTeO2co60M2Sum;
  delete hAdapTeO2k40M2Sum;
  delete hAdapTeO2pb210M2Sum;
  delete hAdapTeO2po210M2Sum;
  delete hAdapTeO2te125M2Sum;
  delete hAdapTeO2th232M2Sum;
  delete hAdapTeO2th228M2Sum;
  delete hAdapTeO2ra226M2Sum;
  delete hAdapTeO2rn222M2Sum;
  delete hAdapTeO2u238M2Sum;
  delete hAdapTeO2th230M2Sum;
  delete hAdapTeO2u234M2Sum;

  delete hAdapTeO2th232onlyM2Sum;
  delete hAdapTeO2ra228pb208M2Sum;
  delete hAdapTeO2th230onlyM2Sum;

  delete hAdapTeO2Sxth232onlyM2Sum_001;
  delete hAdapTeO2Sxra228pb208M2Sum_001;
  delete hAdapTeO2Sxu238th230M2Sum_001;
  delete hAdapTeO2Sxth230onlyM2Sum_001;
  delete hAdapTeO2Sxra226pb210M2Sum_001;
  delete hAdapTeO2Sxpb210M2Sum_0001;

  delete hAdapTeO2Spb210M2Sum_01;
  delete hAdapTeO2Spo210M2Sum_001;
  delete hAdapTeO2Spo210M2Sum_01;
  delete hAdapTeO2Sth232M2Sum_01;
  delete hAdapTeO2Su238M2Sum_01;
  delete hAdapTeO2Sxpb210M2Sum_001;
  delete hAdapTeO2Sxpb210M2Sum_01;
  delete hAdapTeO2Sxpb210M2Sum_1;
  delete hAdapTeO2Sxpb210M2Sum_10;
  delete hAdapTeO2Sxpo210M2Sum_001;
  delete hAdapTeO2Sxpo210M2Sum_01;
  delete hAdapTeO2Sxpo210M2Sum_1;
  delete hAdapTeO2Sxth232M2Sum_001;
  delete hAdapTeO2Sxth232M2Sum_01;
  delete hAdapTeO2Sxth232M2Sum_1;
  delete hAdapTeO2Sxth232M2Sum_10;
  delete hAdapTeO2Sxu238M2Sum_001;
  delete hAdapTeO2Sxu238M2Sum_01;
  delete hAdapTeO2Sxu238M2Sum_1;
  delete hAdapTeO2Sxu238M2Sum_10;

  delete hAdapCuFrameco58M1;
  delete hAdapCuFrameco60M1;
  delete hAdapCuFramecs137M1;
  delete hAdapCuFramek40M1;
  delete hAdapCuFramemn54M1;
  delete hAdapCuFramepb210M1;
  delete hAdapCuFrameth232M1;
  delete hAdapCuFrameu238M1;

  delete hAdapCuFrameSth232M1_1;
  delete hAdapCuFrameSu238M1_1;
  delete hAdapCuFrameSxpb210M1_001;
  delete hAdapCuFrameSxpb210M1_01;
  delete hAdapCuFrameSxpb210M1_1;
  delete hAdapCuFrameSxpb210M1_10;
  delete hAdapCuFrameSxth232M1_001;
  delete hAdapCuFrameSxth232M1_01;
  delete hAdapCuFrameSxth232M1_1;
  delete hAdapCuFrameSxth232M1_10;
  delete hAdapCuFrameSxu238M1_001;
  delete hAdapCuFrameSxu238M1_01;
  delete hAdapCuFrameSxu238M1_1;
  delete hAdapCuFrameSxu238M1_10;

  delete hAdapCuFrameco58M2;
  delete hAdapCuFrameco60M2;
  delete hAdapCuFramecs137M2;
  delete hAdapCuFramek40M2;
  delete hAdapCuFramemn54M2;
  delete hAdapCuFramepb210M2;
  delete hAdapCuFrameth232M2;
  delete hAdapCuFrameu238M2;

  delete hAdapCuFrameSth232M2_1;
  delete hAdapCuFrameSu238M2_1;
  delete hAdapCuFrameSxpb210M2_001;
  delete hAdapCuFrameSxpb210M2_01;
  delete hAdapCuFrameSxpb210M2_1;
  delete hAdapCuFrameSxpb210M2_10;
  delete hAdapCuFrameSxth232M2_001;
  delete hAdapCuFrameSxth232M2_01;
  delete hAdapCuFrameSxth232M2_1;
  delete hAdapCuFrameSxth232M2_10;
  delete hAdapCuFrameSxu238M2_001;
  delete hAdapCuFrameSxu238M2_01;
  delete hAdapCuFrameSxu238M2_1;
  delete hAdapCuFrameSxu238M2_10;

  delete hAdapCuFrameco58M2Sum;
  delete hAdapCuFrameco60M2Sum;
  delete hAdapCuFramecs137M2Sum;
  delete hAdapCuFramek40M2Sum;
  delete hAdapCuFramemn54M2Sum;
  delete hAdapCuFramepb210M2Sum;
  delete hAdapCuFrameth232M2Sum;
  delete hAdapCuFrameu238M2Sum;

  delete hAdapCuFrameSth232M2Sum_1;
  delete hAdapCuFrameSu238M2Sum_1;
  delete hAdapCuFrameSxpb210M2Sum_001;
  delete hAdapCuFrameSxpb210M2Sum_01;
  delete hAdapCuFrameSxpb210M2Sum_1;
  delete hAdapCuFrameSxpb210M2Sum_10;
  delete hAdapCuFrameSxth232M2Sum_001;
  delete hAdapCuFrameSxth232M2Sum_01;
  delete hAdapCuFrameSxth232M2Sum_1;
  delete hAdapCuFrameSxth232M2Sum_10;
  delete hAdapCuFrameSxu238M2Sum_001;
  delete hAdapCuFrameSxu238M2Sum_01;
  delete hAdapCuFrameSxu238M2Sum_1;
  delete hAdapCuFrameSxu238M2Sum_10;

  delete hAdapCuBoxco58M1;
  delete hAdapCuBoxco60M1;
  delete hAdapCuBoxcs137M1;
  delete hAdapCuBoxk40M1;
  delete hAdapCuBoxmn54M1;
  delete hAdapCuBoxpb210M1;
  delete hAdapCuBoxth232M1;
  delete hAdapCuBoxu238M1;  

  delete hAdapCuBoxSth232M1_1;
  delete hAdapCuBoxSu238M1_1;
  delete hAdapCuBoxSxpb210M1_001;
  delete hAdapCuBoxSxpb210M1_01;
  delete hAdapCuBoxSxpb210M1_1;
  delete hAdapCuBoxSxpb210M1_10;
  delete hAdapCuBoxSxth232M1_001;
  delete hAdapCuBoxSxth232M1_01;
  delete hAdapCuBoxSxth232M1_1;
  delete hAdapCuBoxSxth232M1_10;
  delete hAdapCuBoxSxu238M1_001;
  delete hAdapCuBoxSxu238M1_01;
  delete hAdapCuBoxSxu238M1_1;
  delete hAdapCuBoxSxu238M1_10;

  delete hAdapCuBoxco58M2;
  delete hAdapCuBoxco60M2;
  delete hAdapCuBoxcs137M2;
  delete hAdapCuBoxk40M2;
  delete hAdapCuBoxmn54M2;
  delete hAdapCuBoxpb210M2;
  delete hAdapCuBoxth232M2;
  delete hAdapCuBoxu238M2;  

  delete hAdapCuBoxSth232M2_1;
  delete hAdapCuBoxSu238M2_1;
  delete hAdapCuBoxSxpb210M2_001;
  delete hAdapCuBoxSxpb210M2_01;
  delete hAdapCuBoxSxpb210M2_1;
  delete hAdapCuBoxSxpb210M2_10;
  delete hAdapCuBoxSxth232M2_001;
  delete hAdapCuBoxSxth232M2_01;
  delete hAdapCuBoxSxth232M2_1;
  delete hAdapCuBoxSxth232M2_10;
  delete hAdapCuBoxSxu238M2_001;
  delete hAdapCuBoxSxu238M2_01;
  delete hAdapCuBoxSxu238M2_1;
  delete hAdapCuBoxSxu238M2_10;

  delete hAdapCuBoxco58M2Sum;
  delete hAdapCuBoxco60M2Sum;
  delete hAdapCuBoxcs137M2Sum;
  delete hAdapCuBoxk40M2Sum;
  delete hAdapCuBoxmn54M2Sum;
  delete hAdapCuBoxpb210M2Sum;
  delete hAdapCuBoxth232M2Sum;
  delete hAdapCuBoxu238M2Sum; 

  delete hAdapCuBoxSth232M2Sum_1;
  delete hAdapCuBoxSu238M2Sum_1;
  delete hAdapCuBoxSxpb210M2Sum_001;
  delete hAdapCuBoxSxpb210M2Sum_01;
  delete hAdapCuBoxSxpb210M2Sum_1;
  delete hAdapCuBoxSxpb210M2Sum_10;
  delete hAdapCuBoxSxth232M2Sum_001;
  delete hAdapCuBoxSxth232M2Sum_01;
  delete hAdapCuBoxSxth232M2Sum_1;
  delete hAdapCuBoxSxth232M2Sum_10;
  delete hAdapCuBoxSxu238M2Sum_001;
  delete hAdapCuBoxSxu238M2Sum_01;
  delete hAdapCuBoxSxu238M2Sum_1;
  delete hAdapCuBoxSxu238M2Sum_10;

  delete hAdapCuBox_CuFrameco60M1;
  delete hAdapCuBox_CuFramek40M1;
  delete hAdapCuBox_CuFrameth232M1;
  delete hAdapCuBox_CuFrameu238M1;

  delete hAdapCuBox_CuFrameth232M1_10;
  delete hAdapCuBox_CuFrameu238M1_10;
  delete hAdapCuBox_CuFramepb210M1_10;
  delete hAdapCuBox_CuFramepb210M1_01;

  delete hAdapCuBox_CuFrameco60M2;
  delete hAdapCuBox_CuFramek40M2;
  delete hAdapCuBox_CuFrameth232M2;
  delete hAdapCuBox_CuFrameu238M2;

  delete hAdapCuBox_CuFrameth232M2_10;
  delete hAdapCuBox_CuFrameu238M2_10;
  delete hAdapCuBox_CuFramepb210M2_10;
  delete hAdapCuBox_CuFramepb210M2_01;

  delete hAdapCuBox_CuFrameco60M2Sum;
  delete hAdapCuBox_CuFramek40M2Sum;
  delete hAdapCuBox_CuFrameth232M2Sum;
  delete hAdapCuBox_CuFrameu238M2Sum;

  delete hAdapCuBox_CuFrameth232M2Sum_10;
  delete hAdapCuBox_CuFrameu238M2Sum_10;
  delete hAdapCuBox_CuFramepb210M2Sum_10;
  delete hAdapCuBox_CuFramepb210M2Sum_01;

  delete hAdap50mKco58M1;
  delete hAdap50mKco60M1;
  delete hAdap50mKcs137M1;
  delete hAdap50mKk40M1;
  delete hAdap50mKmn54M1;
  delete hAdap50mKpb210M1;
  delete hAdap50mKth232M1;
  delete hAdap50mKu238M1;   

  delete hAdap50mKco58M2;
  delete hAdap50mKco60M2;
  delete hAdap50mKcs137M2;
  delete hAdap50mKk40M2;
  delete hAdap50mKmn54M2;
  delete hAdap50mKpb210M2;
  delete hAdap50mKth232M2;
  delete hAdap50mKu238M2; 

  delete hAdap50mKco58M2Sum;
  delete hAdap50mKco60M2Sum;
  delete hAdap50mKcs137M2Sum;
  delete hAdap50mKk40M2Sum;
  delete hAdap50mKmn54M2Sum;
  delete hAdap50mKpb210M2Sum;
  delete hAdap50mKth232M2Sum;
  delete hAdap50mKu238M2Sum;  

  delete hAdap600mKco60M1;
  delete hAdap600mKk40M1;
  delete hAdap600mKth232M1;
  delete hAdap600mKu238M1;    

  delete hAdap600mKco60M2;
  delete hAdap600mKk40M2;
  delete hAdap600mKth232M2;
  delete hAdap600mKu238M2;  

  delete hAdap600mKco60M2Sum;
  delete hAdap600mKk40M2Sum;
  delete hAdap600mKth232M2Sum;
  delete hAdap600mKu238M2Sum; 

  delete hAdapPbRombi207M1;
  delete hAdapPbRomco60M1;
  delete hAdapPbRomcs137M1;
  delete hAdapPbRomk40M1;
  delete hAdapPbRompb210M1;
  delete hAdapPbRomth232M1;
  delete hAdapPbRomu238M1;    

  delete hAdapPbRombi207M2;
  delete hAdapPbRomco60M2;
  delete hAdapPbRomcs137M2;
  delete hAdapPbRomk40M2;
  delete hAdapPbRompb210M2;
  delete hAdapPbRomth232M2;
  delete hAdapPbRomu238M2;    

  delete hAdapPbRombi207M2Sum;
  delete hAdapPbRomco60M2Sum;
  delete hAdapPbRomcs137M2Sum;
  delete hAdapPbRomk40M2Sum;
  delete hAdapPbRompb210M2Sum;
  delete hAdapPbRomth232M2Sum;
  delete hAdapPbRomu238M2Sum; 

  delete hAdapMBco60M1;
  delete hAdapMBk40M1;
  delete hAdapMBth232M1;
  delete hAdapMBu238M1;   

  delete hAdapMBco60M2;
  delete hAdapMBk40M2;
  delete hAdapMBth232M2;
  delete hAdapMBu238M2; 

  delete hAdapMBco60M2Sum;
  delete hAdapMBk40M2Sum;
  delete hAdapMBth232M2Sum;
  delete hAdapMBu238M2Sum;  

  delete hAdapSIk40M1;
  delete hAdapSIth232M1;
  delete hAdapSIu238M1;

  delete hAdapSIk40M2;
  delete hAdapSIth232M2;
  delete hAdapSIu238M2;

  delete hAdapSIk40M2Sum;
  delete hAdapSIth232M2Sum;
  delete hAdapSIu238M2Sum;

  delete hAdapInternalco60M1;
  delete hAdapInternalk40M1;
  delete hAdapInternalth232M1;
  delete hAdapInternalu238M1;

  delete hAdapInternalco60M2;
  delete hAdapInternalk40M2;
  delete hAdapInternalth232M2;
  delete hAdapInternalu238M2;

  delete hAdapInternalco60M2Sum;
  delete hAdapInternalk40M2Sum;
  delete hAdapInternalth232M2Sum;
  delete hAdapInternalu238M2Sum;  

  delete hAdapIVCco60M1;
  delete hAdapIVCk40M1;
  delete hAdapIVCth232M1;
  delete hAdapIVCu238M1;    

  delete hAdapIVCco60M2;
  delete hAdapIVCk40M2;
  delete hAdapIVCth232M2;
  delete hAdapIVCu238M2;  

  delete hAdapIVCco60M2Sum;
  delete hAdapIVCk40M2Sum;
  delete hAdapIVCth232M2Sum;
  delete hAdapIVCu238M2Sum; 

  delete hAdapOVCco60M1;
  delete hAdapOVCk40M1;
  delete hAdapOVCth232M1;
  delete hAdapOVCu238M1;    

  delete hAdapOVCco60M2;
  delete hAdapOVCk40M2;
  delete hAdapOVCth232M2;
  delete hAdapOVCu238M2;  

  delete hAdapOVCco60M2Sum;
  delete hAdapOVCk40M2Sum;
  delete hAdapOVCth232M2Sum;
  delete hAdapOVCu238M2Sum;

  delete hAdapExtPbbi210M1;

  delete hAdapExtPbbi210M2;
  
  delete hAdapExtPbbi210M2Sum;
}

// Returns vector of bin low-edge for adaptive binning
vector<double> TBackgroundModel::AdaptiveBinning(TH1D *h1, int dBinBase)
{
  // dBinBase is the minimal number of counts per bin
  vector<double> dBinArrayThing;

  double dDummy = 0;
  double dDummyFill = 0;
  int j = 0;
  // 25 since start at 50 keV with 2 keV bin width
  for(int i = h1->FindBin(0); i < h1->FindBin(50); i++)
  {
    dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(i));
  }
  for(int i = h1->FindBin(50); i < h1->GetNbinsX(); i++)
  {
    // Added per each peak
/*
    // K40
    if(i >= h1->FindBin(1450) && i < h1->FindBin(1470))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(1450)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(1470-1450)/dBaseBinSize;
    }
    if(i >= h1->FindBin(1755) && i < h1->FindBin(1775))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(1755)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(1775-1755)/dBaseBinSize;
    }    
    if(i >= h1->FindBin(2195) && i < h1->FindBin(2215))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(2195)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(2215-2195)/dBaseBinSize;
    }        
    // Tl-208
    if(i >= h1->FindBin(2600) && i < h1->FindBin(2630))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(2600)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(2630-2600)/dBaseBinSize;
    }
*/

    // Pt peak 3150 - 3400
    if(i >= h1->FindBin(3150) && i < h1->FindBin(3400))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(3150)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(3400-3150)/dBaseBinSize;
    }
    // 4050 - 4200
    if(i >= h1->FindBin(4025) && i < h1->FindBin(4170))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(4025)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(4170-4025)/dBaseBinSize;
    }
    // 4200 - 4350
    if(i >= h1->FindBin(4170) && i < h1->FindBin(4350))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(4170)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(4350-4170)/dBaseBinSize;
    }    

    // 4700 - 4850
    if(i >= h1->FindBin(4700) && i < h1->FindBin(4850))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(4700)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(4850-4700)/dBaseBinSize;
    }

    // 4850 - 4950
    if(i >= h1->FindBin(4850) && i < h1->FindBin(5000))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(4850)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(5000-4850)/dBaseBinSize;
    }

    // 5200 - 5400
    if(i >= h1->FindBin(5000) && i < h1->FindBin(5100))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(5000)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(5100-5000)/dBaseBinSize;
    }

    // 5200 - 5400
    if(i >= h1->FindBin(5200) && i < h1->FindBin(5400))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(5200)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(5400-5200)/dBaseBinSize;
    }
    // 5400 - 5650
    if(i >= h1->FindBin(5400) && i < h1->FindBin(5650))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(5400)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(5650-5400)/dBaseBinSize;
    }

    // 5650 - 5800
    if(i >= h1->FindBin(5650) && i < h1->FindBin(5780))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(5650)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(5780-5650)/dBaseBinSize;
    }

    // 5800 - 6050
    if(i >= h1->FindBin(5780) && i < h1->FindBin(6100))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(5780)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(6100-5780)/dBaseBinSize;
    }    

    // 6050 - 6350
    if(i >= h1->FindBin(6100) && i < h1->FindBin(6450))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(6100)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(6450-6100)/dBaseBinSize;
    }    

    // 6700 - 6900
    if(i >= h1->FindBin(6700) && i < h1->FindBin(7000))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(6700)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(7000-6700)/dBaseBinSize;
    }    

    // 7500 - 8000
    if(i >= h1->FindBin(7000) && i < h1->FindBin(8000))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(7000)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(8000-7000)/dBaseBinSize;
    }    

    // 8000 to the end 10000
    if(i >= h1->FindBin(8000) && i < h1->FindBin(10000))
    {
     dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(h1->FindBin(8000)));
     // Reset everything
     j = 0;
     dDummyFill = 0;
     dDummy = 0;
     i = i+(10000-8000)/dBaseBinSize;
    }    


    dDummy = h1->GetBinContent(i);
    dDummyFill += dDummy;


    if(dDummyFill >= dBinBase)
    {
      dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(i-j));
      dDummy = 0;
      dDummyFill = 0;
      j = 0;
    }
    else if(i == h1->GetNbinsX()-1) // for the very end if it doesn't reach 50 events (which it won't)
    {
        dBinArrayThing.push_back(h1->GetXaxis()->GetBinLowEdge(i-j));
    }
    else 
    {
      j++;
    }
  }
return dBinArrayThing;
}

TH1D *TBackgroundModel::CalculateResidualsAdaptive(TH1D *h1, TH1D *h2, TH1D *hResid, int binMin, int binMax, int dMult)
{

  if(binMin >= binMax)
  {
    cout << " Residuals: min bin >= max bin" << endl;
  }

  if(dMult == 1)
  {
  TH1D  *hOut       = new TH1D("hOutResidualM1", "Fit Residuals M1", dAdaptiveBinsM1, dAdaptiveArrayM1);
  }
  if(dMult == 2)
  {
  TH1D  *hOut       = new TH1D("hOutResidualM2", "Fit Residuals M2", dAdaptiveBinsM2, dAdaptiveArrayM2);
  }
  if(dMult == 3)
  {
  TH1D  *hOut       = new TH1D("hOutResidualM2Sum", "Fit Residuals M2Sum", dAdaptiveBinsM2Sum, dAdaptiveArrayM2Sum);
  }

  // Clone histograms for rebinning
  TH1D  *hCloneBkg    = (TH1D*)h1->Clone("hCloneBkg");
  TH1D  *hCloneMC   = (TH1D*)h2->Clone("hCloneMC");

  // Variables used in Residual calculations
  double dResidualX = 0, dResidualY = 0, dResidualXErr = 0, dResidualYErr = 0;

  double datam1_i = 0, modelm1_i = 0;

  // Residual plot and distribution
  for(int j = binMin ; j < binMax ; j++)
  {

    if( hCloneBkg->GetBinCenter(j) >= 3150 && hCloneBkg->GetBinCenter(j) <= 3400)continue;    
    // if( hCloneBkg->GetBinCenter(j) >= 800 && hCloneBkg->GetBinCenter(j) <= 808)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 1060 && hCloneBkg->GetBinCenter(j) <= 1068)continue;

    // if( hCloneBkg->GetBinCenter(j) >= 506 && hCloneBkg->GetBinCenter(j) <= 515)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 579 && hCloneBkg->GetBinCenter(j) <= 589)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 605 && hCloneBkg->GetBinCenter(j) <= 615)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 906 && hCloneBkg->GetBinCenter(j) <= 917)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 1450 && hCloneBkg->GetBinCenter(j) <= 1475)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 1755 && hCloneBkg->GetBinCenter(j) <= 1780)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 2090 && hCloneBkg->GetBinCenter(j) <= 2130)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 2200 && hCloneBkg->GetBinCenter(j) <= 2220)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 2440 && hCloneBkg->GetBinCenter(j) <= 2450)continue;
    // if( hCloneBkg->GetBinCenter(j) >= 2600 && hCloneBkg->GetBinCenter(j) <= 2630)continue;

    dResidualX    = hCloneBkg->GetBinCenter(j);

    datam1_i = h1->GetBinContent(j)*h1->GetBinWidth(j);
    modelm1_i = h2->GetBinContent(j)*h1->GetBinWidth(j);
    
    // Re-multiply bin content by bin width (for # of counts)
    dResidualY = (datam1_i - modelm1_i)/TMath::Sqrt(datam1_i);

    hOut->SetBinContent(j, dResidualY);
    hOut->SetBinError(j, 0.01);
    hResid->Fill(dResidualY);
  }

  return hOut;
}


// Draws background spectrum
void TBackgroundModel::DrawBkg()
{

 	gStyle->SetOptStat(0);
  gStyle->SetOptFit();
 	// gStyle->SetOptTitle(0);	

  TLegend *leg = new TLegend(0.75,0.75,0.97,0.97);
  leg->AddEntry(fDataHistoTot, "Total", "l");
  leg->AddEntry(fDataHistoM1, "M1", "l");
  leg->AddEntry(fDataHistoM2, "M2", "l");
  leg->Draw();

  TCanvas *c1 = new TCanvas("c1", "c1", 1200, 1800);
  c1->Divide(1, 2);
  c1->cd(1);
  gPad->SetLogy();
  fAdapDataHistoM1->GetXaxis()->SetTitle("Energy (keV)");
  fAdapDataHistoM1->GetYaxis()->SetTitle("Counts/bin");  
  fAdapDataHistoM1->Draw("");


  c1->cd(2); 
  gPad->SetLogy();
  fAdapDataHistoM2->GetXaxis()->SetTitle("Energy (keV)");
  fAdapDataHistoM2->GetYaxis()->SetTitle("Counts/bin");   
  fAdapDataHistoM2->Draw("");

}

double TBackgroundModel::GetChiSquareAdaptive()
{
  double chiSquare = 0.;
  double datam1_i, errm1_i;
  double datam2_i, errm2_i;
  double datam2sum_i, errm2sum_i;
  double modelm1_i, modelm2_i, modelm2sum_i;


  for(int i = dFitMinBinM1; i < dFitMaxBinM1; i++)
  {
    // Dividing by base bin size in chi-squared because the weight is width/base bin size when filling
    if( fAdapDataHistoM1->GetBinCenter(i) >= 3150 && fAdapDataHistoM1->GetBinCenter(i) <= 3400)continue;
    // Skipping unknown peaks
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 800 && fAdapDataHistoM1->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1060 && fAdapDataHistoM1->GetBinCenter(i) <= 1068)continue; 

    // if( fAdapDataHistoM1->GetBinCenter(i) >= 506 && fAdapDataHistoM1->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 579 && fAdapDataHistoM1->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 605 && fAdapDataHistoM1->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 906 && fAdapDataHistoM1->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1450 && fAdapDataHistoM1->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1755 && fAdapDataHistoM1->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2090 && fAdapDataHistoM1->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2200 && fAdapDataHistoM1->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2440 && fAdapDataHistoM1->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2600 && fAdapDataHistoM1->GetBinCenter(i) <= 2630)continue;  

    datam1_i = fAdapDataHistoM1->GetBinContent(i)*fAdapDataHistoM1->GetBinWidth(i);
    modelm1_i = fModelTotAdapM1->GetBinContent(i)*fAdapDataHistoM1->GetBinWidth(i);
    
    if(modelm1_i != 0 && datam1_i != 0)
    // if(modelm1_i != 0)
    {
      // Log-likelihood
      chiSquare += 2 * (modelm1_i - datam1_i + datam1_i * TMath::Log(datam1_i/modelm1_i));

      // Pearson chi-squared
      // chiSquare += (datam1_i - modelm1_i)*(datam1_i - modelm1_i)/(datam1_i);
      // Neyman chi-squared
      // chiSquare += (datam1_i - modelm1_i)*(datam1_i - modelm1_i)/modelm1_i;
    }
  }

/*
  for(int i = dFitMinBinM2; i < dFitMaxBinM2; i++)
  {
    if( fAdapDataHistoM2->GetBinCenter(i) >= 3150 && fAdapDataHistoM2->GetBinCenter(i) <= 3400)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 800 && fAdapDataHistoM2->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1060 && fAdapDataHistoM2->GetBinCenter(i) <= 1068)continue;

    // if( fAdapDataHistoM2->GetBinCenter(i) >= 506 && fAdapDataHistoM2->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 579 && fAdapDataHistoM2->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 605 && fAdapDataHistoM2->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 906 && fAdapDataHistoM2->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1450 && fAdapDataHistoM2->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1755 && fAdapDataHistoM2->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2090 && fAdapDataHistoM2->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2200 && fAdapDataHistoM2->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2440 && fAdapDataHistoM2->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2600 && fAdapDataHistoM2->GetBinCenter(i) <= 2630)continue;  

    datam2_i = fAdapDataHistoM2->GetBinContent(i)*fAdapDataHistoM2->GetBinWidth(i);
    modelm2_i = fModelTotAdapM2->GetBinContent(i)*fAdapDataHistoM2->GetBinWidth(i);
    
    if(modelm2_i != 0 && datam2_i != 0)
    // if(modelm2_i != 0)      
    {
      // Log-likelihood
      chiSquare += 2 * (modelm2_i - datam2_i + datam2_i * TMath::Log(datam2_i/modelm2_i));

      // Pearson chi-squared
      // chiSquare += (datam2_i - modelm2_i)*(datam2_i - modelm2_i)/(datam2_i);
      
      // Neyman chi-squared
      // chiSquare += (datam2_i - modelm2_i)*(datam2_i - modelm2_i)/modelm2_i;

    }
  }
*/

/*
  for(int i = dFitMinBinM2Sum; i < dFitMaxBinM2Sum; i++)
  {
    if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 3150 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 3400)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 800 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1060 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1068)continue;

    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 506 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 579 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 605 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 906 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1450 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1755 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2090 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2200 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2440 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2600 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2630)continue;  

    datam2sum_i = fAdapDataHistoM2Sum->GetBinContent(i)*fAdapDataHistoM2Sum->GetBinWidth(i);
    modelm2sum_i = fModelTotAdapM2Sum->GetBinContent(i)*fAdapDataHistoM2Sum->GetBinWidth(i);
    
    if(modelm2sum_i != 0 && datam2sum_i != 0)
    {
      // Log-likelihood
      chiSquare += 2 * (modelm2sum_i - datam2sum_i + datam2sum_i * TMath::Log(datam2sum_i/modelm2sum_i));

      // Pearson chi-squared
      // chiSquare += (datam2sum_i - modelm2sum_i)*(datam2sum_i - modelm2sum_i)/(datam2sum_i);
      
      // Neyman chi-squared
      // chiSquare += (datam2sum_i - modelm2sum_i)*(datam2sum_i - modelm2sum_i)/modelm2sum_i;

    }
  }
*/

  return chiSquare;
}

void TBackgroundModel::Initialize()
{	

  // Loads PDFs from file
  cout << "Loading PDF Histograms from file" << endl;
  cout << "Directory " << Form("%s/", dMCDir.c_str()) << endl;

  fBulkInner = new TFile(Form("%s/OldProd/MCProduction_BulkInner_1keV.root", dMCDir.c_str()));
  fBulkInnerOld = new TFile(Form("%s/OldProd/MCProduction_BulkInner_1keV.root", dMCDir.c_str()));
  fBulkInnerM2Sum = new TFile(Form("%s/OldProd/MCProduction_BulkInnerM2Sum_1keV.root", dMCDir.c_str()));

  fBulkOuter = new TFile(Form("%s/OldProd/MCProduction_BulkOuter_1keV.root", dMCDir.c_str()));
  fBulkOuterOld = new TFile(Form("%s/OldProd/MCProduction_BulkOuter_1keV.root", dMCDir.c_str()));
  fBulkOuterM2Sum = new TFile(Form("%s/OldProd/MCProduction_BulkOuterM2Sum_1keV.root", dMCDir.c_str()));

  fSurfaceCrystal = new TFile(Form("%s/OldProd/MCProduction_SurfaceCrystal_1keV_new.root", dMCDir.c_str()));
  fSurfaceCrystalOld = new TFile(Form("%s/OldProd/MCProduction_SurfaceCrystal_1keV_new.root", dMCDir.c_str()));
  fSurfaceOther = new TFile(Form("%s/OldProd/MCProduction_SurfaceOther_1keV.root", dMCDir.c_str()));
  fSurfaceOtherOld = new TFile(Form("%s/OldProd/MCProduction_SurfaceOther_1keV.root", dMCDir.c_str()));

  fFudge = new TFile(Form("%s/OldProd/MCProduction_FudgeFactor_1keV.root", dMCDir.c_str()));

///////////// Bulk Histograms
/////// Crystal M1 and M2
  hTeO20nuM1     = (TH1D*)fBulkInnerOld->Get("hTeO20nuM1");
  hTeO22nuM1     = (TH1D*)fBulkInnerOld->Get("hTeO22nuM1");
  hTeO2co60M1    = (TH1D*)fBulkInnerOld->Get("hTeO2co60M1");
  hTeO2k40M1     = (TH1D*)fBulkInnerOld->Get("hTeO2k40M1");
  hTeO2pb210M1   = (TH1D*)fBulkInnerOld->Get("hTeO2pb210M1");
  hTeO2po210M1   = (TH1D*)fBulkInnerOld->Get("hTeO2po210M1");
  hTeO2te125M1   = (TH1D*)fBulkInnerOld->Get("hTeO2te125M1");
  hTeO2th232M1   = (TH1D*)fBulkInnerOld->Get("hTeO2th232M1");
  // hTeO2th228M1   = (TH1D*)fBulkInner->Get("hTeO2th228M1");
  // hTeO2ra226M1   = (TH1D*)fBulkInner->Get("hTeO2ra226M1");
  // hTeO2rn222M1   = (TH1D*)fBulkInner->Get("hTeO2rn222M1");
  hTeO2u238M1    = (TH1D*)fBulkInnerOld->Get("hTeO2u238M1");
  // hTeO2th230M1   = (TH1D*)fBulkInner->Get("hTeO2th230M1");
  // hTeO2u234M1    = (TH1D*)fBulkInner->Get("hTeO2u234M1");

  hTeO2th232onlyM1 = (TH1D*)fBulkInnerOld->Get("hTeO2th232onlyM1");
  hTeO2ra228pb208M1 = (TH1D*)fBulkInnerOld->Get("hTeO2ra228pb208M1");
  hTeO2th230onlyM1 = (TH1D*)fBulkInnerOld->Get("hTeO2th230onlyM1");

  hTeO20nuM2     = (TH1D*)fBulkInnerOld->Get("hTeO20nuM2");
  hTeO22nuM2     = (TH1D*)fBulkInnerOld->Get("hTeO22nuM2");
  hTeO2co60M2    = (TH1D*)fBulkInnerOld->Get("hTeO2co60M2");
  hTeO2k40M2     = (TH1D*)fBulkInnerOld->Get("hTeO2k40M2");
  hTeO2pb210M2   = (TH1D*)fBulkInnerOld->Get("hTeO2pb210M2");
  hTeO2po210M2   = (TH1D*)fBulkInnerOld->Get("hTeO2po210M2");
  hTeO2te125M2   = (TH1D*)fBulkInnerOld->Get("hTeO2te125M2");
  hTeO2th232M2   = (TH1D*)fBulkInnerOld->Get("hTeO2th232M2");
  // hTeO2th228M2   = (TH1D*)fBulkInner->Get("hTeO2th228M2");
  // hTeO2ra226M2   = (TH1D*)fBulkInner->Get("hTeO2ra226M2");
  // hTeO2rn222M2   = (TH1D*)fBulkInner->Get("hTeO2rn222M2");
  hTeO2u238M2    = (TH1D*)fBulkInnerOld->Get("hTeO2u238M2");
  // hTeO2th230M2   = (TH1D*)fBulkInner->Get("hTeO2th230M2");
  // hTeO2u234M2    = (TH1D*)fBulkInner->Get("hTeO2u234M2");

  hTeO2th232onlyM2 = (TH1D*)fBulkInnerOld->Get("hTeO2th232onlyM2");
  hTeO2ra228pb208M2 = (TH1D*)fBulkInnerOld->Get("hTeO2ra228pb208M2");
  hTeO2th230onlyM2 = (TH1D*)fBulkInnerOld->Get("hTeO2th230onlyM2");

  hTeO20nuM2Sum     = (TH1D*)fBulkInnerM2Sum->Get("hTeO20nuM2Sum");
  hTeO22nuM2Sum     = (TH1D*)fBulkInnerM2Sum->Get("hTeO22nuM2Sum");
  hTeO2co60M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hTeO2co60M2Sum");
  hTeO2k40M2Sum     = (TH1D*)fBulkInnerM2Sum->Get("hTeO2k40M2Sum");
  hTeO2pb210M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2pb210M2Sum");
  hTeO2po210M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2po210M2Sum");
  hTeO2te125M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2te125M2Sum");
  hTeO2th232M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2th232M2Sum");
  // hTeO2th228M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2th228M2Sum");
  // hTeO2ra226M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2ra226M2Sum");
  // hTeO2rn222M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2rn222M2Sum");
  hTeO2u238M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hTeO2u238M2Sum");
  // hTeO2th230M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hTeO2th230M2Sum");
  // hTeO2u234M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hTeO2u234M2Sum");

  hTeO2th232onlyM2Sum = (TH1D*)fBulkInnerM2Sum->Get("hTeO2th232onlyM2Sum");
  hTeO2ra228pb208M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hTeO2ra228pb208M2Sum");
  hTeO2th230onlyM2Sum = (TH1D*)fBulkInnerM2Sum->Get("hTeO2th230onlyM2Sum");


///////// Frame M1 and M2
  hCuFrameco58M1    = (TH1D*)fBulkInnerOld->Get("hCuFrameco58M1");
  hCuFrameco60M1    = (TH1D*)fBulkInnerOld->Get("hCuFrameco60M1");
  hCuFramecs137M1   = (TH1D*)fBulkInnerOld->Get("hCuFramecs137M1");
  hCuFramek40M1     = (TH1D*)fBulkInnerOld->Get("hCuFramek40M1");
  hCuFramemn54M1    = (TH1D*)fBulkInnerOld->Get("hCuFramemn54M1");
  hCuFramepb210M1   = (TH1D*)fBulkInnerOld->Get("hCuFramepb210M1");
  hCuFrameth232M1   = (TH1D*)fBulkInnerOld->Get("hCuFrameth232M1");
  hCuFrameu238M1    = (TH1D*)fBulkInnerOld->Get("hCuFrameu238M1");
   
  hCuFrameco58M2    = (TH1D*)fBulkInnerOld->Get("hCuFrameco58M2");
  hCuFrameco60M2    = (TH1D*)fBulkInnerOld->Get("hCuFrameco60M2");
  hCuFramecs137M2   = (TH1D*)fBulkInnerOld->Get("hCuFramecs137M2");
  hCuFramek40M2     = (TH1D*)fBulkInnerOld->Get("hCuFramek40M2");
  hCuFramemn54M2    = (TH1D*)fBulkInnerOld->Get("hCuFramemn54M2");
  hCuFramepb210M2   = (TH1D*)fBulkInnerOld->Get("hCuFramepb210M2");
  hCuFrameth232M2   = (TH1D*)fBulkInnerOld->Get("hCuFrameth232M2");
  hCuFrameu238M2    = (TH1D*)fBulkInnerOld->Get("hCuFrameu238M2");

  hCuFrameco58M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuFrameco58M2Sum");
  hCuFrameco60M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuFrameco60M2Sum");
  hCuFramecs137M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuFramecs137M2Sum");
  hCuFramek40M2Sum     = (TH1D*)fBulkInnerM2Sum->Get("hCuFramek40M2Sum");
  hCuFramemn54M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuFramemn54M2Sum");
  hCuFramepb210M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuFramepb210M2Sum");
  hCuFrameth232M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuFrameth232M2Sum");
  hCuFrameu238M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuFrameu238M2Sum");

///////// CuBox (TShield) M1 and M2
  hCuBoxco58M1    = (TH1D*)fBulkInnerOld->Get("hCuBoxco58M1");
  hCuBoxco60M1    = (TH1D*)fBulkInnerOld->Get("hCuBoxco60M1");
  hCuBoxcs137M1   = (TH1D*)fBulkInnerOld->Get("hCuBoxcs137M1");
  hCuBoxk40M1     = (TH1D*)fBulkInnerOld->Get("hCuBoxk40M1");
  hCuBoxmn54M1    = (TH1D*)fBulkInnerOld->Get("hCuBoxmn54M1");
  hCuBoxpb210M1   = (TH1D*)fBulkInnerOld->Get("hCuBoxpb210M1");
  hCuBoxth232M1   = (TH1D*)fBulkInnerOld->Get("hCuBoxth232M1");
  hCuBoxu238M1    = (TH1D*)fBulkInnerOld->Get("hCuBoxu238M1");
   
  hCuBoxco58M2    = (TH1D*)fBulkInnerOld->Get("hCuBoxco58M2");
  hCuBoxco60M2    = (TH1D*)fBulkInnerOld->Get("hCuBoxco60M2");
  hCuBoxcs137M2   = (TH1D*)fBulkInnerOld->Get("hCuBoxcs137M2");
  hCuBoxk40M2     = (TH1D*)fBulkInnerOld->Get("hCuBoxk40M2");
  hCuBoxmn54M2    = (TH1D*)fBulkInnerOld->Get("hCuBoxmn54M2");
  hCuBoxpb210M2   = (TH1D*)fBulkInnerOld->Get("hCuBoxpb210M2");
  hCuBoxth232M2   = (TH1D*)fBulkInnerOld->Get("hCuBoxth232M2");
  hCuBoxu238M2    = (TH1D*)fBulkInnerOld->Get("hCuBoxu238M2");

  hCuBoxco58M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxco58M2Sum");
  hCuBoxco60M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxco60M2Sum");
  hCuBoxcs137M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxcs137M2Sum");
  hCuBoxk40M2Sum     = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxk40M2Sum");
  hCuBoxmn54M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxmn54M2Sum");
  hCuBoxpb210M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxpb210M2Sum");
  hCuBoxth232M2Sum   = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxth232M2Sum");
  hCuBoxu238M2Sum    = (TH1D*)fBulkInnerM2Sum->Get("hCuBoxu238M2Sum");

///////// CuBox + CuFrame M1 and M2
  hCuBox_CuFrameco60M1 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameco60M1");
  hCuBox_CuFramek40M1 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFramek40M1");
  hCuBox_CuFrameth232M1 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameth232M1");
  hCuBox_CuFrameu238M1 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameu238M1");

  hCuBox_CuFrameco60M2 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameco60M2");
  hCuBox_CuFramek40M2 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFramek40M2");
  hCuBox_CuFrameth232M2 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameth232M2");
  hCuBox_CuFrameu238M2 = (TH1D*)fBulkInnerOld->Get("hCuBox_CuFrameu238M2");

  hCuBox_CuFrameco60M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hCuBox_CuFrameco60M2Sum");
  hCuBox_CuFramek40M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hCuBox_CuFramek40M2Sum");
  hCuBox_CuFrameth232M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hCuBox_CuFrameth232M2Sum");
  hCuBox_CuFrameu238M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hCuBox_CuFrameu238M2Sum");

/*
///////// 50mK M1 and M2
  h50mKco58M1    = (TH1D*)fBulkOuter->Get("h50mKco58M1");
  h50mKco60M1    = (TH1D*)fBulkOuter->Get("h50mKco60M1");
  h50mKcs137M1   = (TH1D*)fBulkOuter->Get("h50mKcs137M1");
  h50mKk40M1     = (TH1D*)fBulkOuter->Get("h50mKk40M1");
  h50mKmn54M1    = (TH1D*)fBulkOuter->Get("h50mKmn54M1");
  h50mKpb210M1   = (TH1D*)fBulkOuter->Get("h50mKpb210M1");
  h50mKth232M1   = (TH1D*)fBulkOuter->Get("h50mKth232M1");
  h50mKu238M1    = (TH1D*)fBulkOuter->Get("h50mKu238M1");
   
  h50mKco58M2    = (TH1D*)fBulkOuter->Get("h50mKco58M2");
  h50mKco60M2    = (TH1D*)fBulkOuter->Get("h50mKco60M2");
  h50mKcs137M2   = (TH1D*)fBulkOuter->Get("h50mKcs137M2");
  h50mKk40M2     = (TH1D*)fBulkOuter->Get("h50mKk40M2");
  h50mKmn54M2    = (TH1D*)fBulkOuter->Get("h50mKmn54M2");
  h50mKpb210M2   = (TH1D*)fBulkOuter->Get("h50mKpb210M2");
  h50mKth232M2   = (TH1D*)fBulkOuter->Get("h50mKth232M2");
  h50mKu238M2    = (TH1D*)fBulkOuter->Get("h50mKu238M2");

  h50mKco58M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h50mKco58M2Sum");
  h50mKco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h50mKco60M2Sum");
  h50mKcs137M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("h50mKcs137M2Sum");
  h50mKk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("h50mKk40M2Sum");
  h50mKmn54M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h50mKmn54M2Sum");
  h50mKpb210M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("h50mKpb210M2Sum");
  h50mKth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("h50mKth232M2Sum");
  h50mKu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h50mKu238M2Sum");

///////// 600mK M1 and M2
  h600mKco60M1    = (TH1D*)fBulkOuter->Get("h600mKco60M1");
  h600mKk40M1     = (TH1D*)fBulkOuter->Get("h600mKk40M1");
  h600mKth232M1   = (TH1D*)fBulkOuter->Get("h600mKth232M1");
  h600mKu238M1    = (TH1D*)fBulkOuter->Get("h600mKu238M1");
   
  h600mKco60M2    = (TH1D*)fBulkOuter->Get("h600mKco60M2");
  h600mKk40M2     = (TH1D*)fBulkOuter->Get("h600mKk40M2");
  h600mKth232M2   = (TH1D*)fBulkOuter->Get("h600mKth232M2");
  h600mKu238M2    = (TH1D*)fBulkOuter->Get("h600mKu238M2");

  h600mKco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h600mKco60M2Sum");
  h600mKk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("h600mKk40M2Sum");
  h600mKth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("h600mKth232M2Sum");
  h600mKu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("h600mKu238M2Sum");
*/
//////// Roman Lead M1 and M2
  // hPbRombi207M1   = (TH1D*)fBulkOuter->Get("hPbRombi207M1");
  hPbRomco60M1    = (TH1D*)fBulkOuterOld->Get("hPbRomco60M1");
  hPbRomcs137M1   = (TH1D*)fBulkOuterOld->Get("hPbRomcs137M1");
  hPbRomk40M1     = (TH1D*)fBulkOuterOld->Get("hPbRomk40M1");
  hPbRompb210M1   = (TH1D*)fBulkOuterOld->Get("hPbRompb210M1");
  hPbRomth232M1   = (TH1D*)fBulkOuterOld->Get("hPbRomth232M1");
  hPbRomu238M1    = (TH1D*)fBulkOuterOld->Get("hPbRomu238M1");
   
  // hPbRombi207M2   = (TH1D*)fBulkOuter->Get("hPbRombi207M2");
  hPbRomco60M2    = (TH1D*)fBulkOuterOld->Get("hPbRomco60M2");
  hPbRomcs137M2   = (TH1D*)fBulkOuterOld->Get("hPbRomcs137M2");
  hPbRomk40M2     = (TH1D*)fBulkOuterOld->Get("hPbRomk40M2");
  hPbRompb210M2   = (TH1D*)fBulkOuterOld->Get("hPbRompb210M2");
  hPbRomth232M2   = (TH1D*)fBulkOuterOld->Get("hPbRomth232M2");
  hPbRomu238M2    = (TH1D*)fBulkOuterOld->Get("hPbRomu238M2");

  // hPbRombi207M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hPbRombi207M2Sum");
  hPbRomco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hPbRomco60M2Sum");
  hPbRomcs137M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hPbRomcs137M2Sum");
  hPbRomk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("hPbRomk40M2Sum");
  hPbRompb210M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hPbRompb210M2Sum");
  hPbRomth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hPbRomth232M2Sum");
  hPbRomu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hPbRomu238M2Sum");

/*
/////// Main Bath M1 and M2
  hMBco60M1    = (TH1D*)fBulkOuter->Get("hMBco60M1");
  hMBk40M1     = (TH1D*)fBulkOuter->Get("hMBk40M1");
  hMBth232M1   = (TH1D*)fBulkOuter->Get("hMBth232M1");
  hMBu238M1    = (TH1D*)fBulkOuter->Get("hMBu238M1");
   
  hMBco60M2    = (TH1D*)fBulkOuter->Get("hMBco60M2");
  hMBk40M2     = (TH1D*)fBulkOuter->Get("hMBk40M2");
  hMBth232M2   = (TH1D*)fBulkOuter->Get("hMBth232M2");
  hMBu238M2    = (TH1D*)fBulkOuter->Get("hMBu238M2");

  hMBco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hMBco60M2Sum");
  hMBk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("hMBk40M2Sum");
  hMBth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hMBth232M2Sum");
  hMBu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hMBu238M2Sum");
*/
////// Internal Shields M1 and M2
  // hInternalco60M1 = (TH1D*)fBulkInnerOld->Get("hInternalco60M1");
  // hInternalk40M1 = (TH1D*)fBulkOuter->Get("hInternalk40M1");
  // hInternalth232M1 = (TH1D*)fBulkOuter->Get("hInternalth232M1");
  // hInternalu238M1 = (TH1D*)fBulkInnerOld->Get("hInternalu238M1");

  // hInternalco60M2 = (TH1D*)fBulkInnerOld->Get("hInternalco60M2");
  // hInternalk40M2 = (TH1D*)fBulkOuter->Get("hInternalk40M2");
  // hInternalth232M2 = (TH1D*)fBulkOuter->Get("hInternalth232M2");
  // hInternalu238M2 = (TH1D*)fBulkInnerOld->Get("hInternalu238M2");

  // Old Production
  hInternalco60M1 = (TH1D*)fBulkInnerOld->Get("hInternalco60M1");
  hInternalk40M1 = (TH1D*)fBulkInnerOld->Get("hInternalk40M1");
  hInternalth232M1 = (TH1D*)fBulkInnerOld->Get("hInternalth232M1");
  hInternalu238M1 = (TH1D*)fBulkInnerOld->Get("hInternalu238M1");

  hInternalco60M2 = (TH1D*)fBulkInnerOld->Get("hInternalco60M2");
  hInternalk40M2 = (TH1D*)fBulkInnerOld->Get("hInternalk40M2");
  hInternalth232M2 = (TH1D*)fBulkInnerOld->Get("hInternalth232M2");
  hInternalu238M2 = (TH1D*)fBulkInnerOld->Get("hInternalu238M2");

  hInternalco60M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hInternalco60M2Sum");
  hInternalk40M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hInternalk40M2Sum");
  hInternalth232M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hInternalth232M2Sum");
  hInternalu238M2Sum = (TH1D*)fBulkInnerM2Sum->Get("hInternalu238M2Sum");

/*
/////// Super Insulation M1 and M2
  hSIk40M1     = (TH1D*)fBulkOuter->Get("hSIk40M1");
  hSIth232M1   = (TH1D*)fBulkOuter->Get("hSIth232M1");
  hSIu238M1    = (TH1D*)fBulkOuter->Get("hSIu238M1");  

  hSIk40M2     = (TH1D*)fBulkOuter->Get("hSIk40M2");
  hSIth232M2   = (TH1D*)fBulkOuter->Get("hSIth232M2");
  hSIu238M2    = (TH1D*)fBulkOuter->Get("hSIu238M2"); 

  hSIk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("hSIk40M2Sum");
  hSIth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hSIth232M2Sum");
  hSIu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hSIu238M2Sum"); 

/////// IVC M1 and M2
  hIVCco60M1    = (TH1D*)fBulkOuter->Get("hIVCco60M1");
  hIVCk40M1     = (TH1D*)fBulkOuter->Get("hIVCk40M1");
  hIVCth232M1   = (TH1D*)fBulkOuter->Get("hIVCth232M1");
  hIVCu238M1    = (TH1D*)fBulkOuter->Get("hIVCu238M1");
   
  hIVCco60M2    = (TH1D*)fBulkOuter->Get("hIVCco60M2");
  hIVCk40M2     = (TH1D*)fBulkOuter->Get("hIVCk40M2");
  hIVCth232M2   = (TH1D*)fBulkOuter->Get("hIVCth232M2");
  hIVCu238M2    = (TH1D*)fBulkOuter->Get("hIVCu238M2");

  hIVCco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hIVCco60M2Sum");
  hIVCk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("hIVCk40M2Sum");
  hIVCth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hIVCth232M2Sum");
  hIVCu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hIVCu238M2Sum");
*/
/////// OVC M1 and M2
  hOVCco60M1    = (TH1D*)fBulkOuterOld->Get("hOVCco60M1");
  hOVCk40M1     = (TH1D*)fBulkOuterOld->Get("hOVCk40M1");
  hOVCth232M1   = (TH1D*)fBulkOuterOld->Get("hOVCth232M1");
  hOVCu238M1    = (TH1D*)fBulkOuterOld->Get("hOVCu238M1");
   
  hOVCco60M2    = (TH1D*)fBulkOuterOld->Get("hOVCco60M2");
  hOVCk40M2     = (TH1D*)fBulkOuterOld->Get("hOVCk40M2");
  hOVCth232M2   = (TH1D*)fBulkOuterOld->Get("hOVCth232M2");
  hOVCu238M2    = (TH1D*)fBulkOuterOld->Get("hOVCu238M2");

  hOVCco60M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hOVCco60M2Sum");
  hOVCk40M2Sum     = (TH1D*)fBulkOuterM2Sum->Get("hOVCk40M2Sum");
  hOVCth232M2Sum   = (TH1D*)fBulkOuterM2Sum->Get("hOVCth232M2Sum");
  hOVCu238M2Sum    = (TH1D*)fBulkOuterM2Sum->Get("hOVCu238M2Sum");

/////// External Sources M1 and M2
  hExtPbbi210M1 = (TH1D*)fBulkOuterOld->Get("hExtPbbi210M1");
 
  hExtPbbi210M2 = (TH1D*)fBulkOuterOld->Get("hExtPbbi210M2");

  // hExtPbbi210M2Sum = (TH1D*)fBulkOuterM2Sum->Get("hExtPbbi210M2Sum");

////////// Fudge Factors
  hFudge661M1 = (TH1D*)fFudge->Get("hFudge661M1");
  hFudge803M1 = (TH1D*)fFudge->Get("hFudge803M1");
  hFudge1063M1 = (TH1D*)fFudge->Get("hFudge1063M1");

  hFudge661M2 = (TH1D*)fFudge->Get("hFudge661M2");
  hFudge803M2 = (TH1D*)fFudge->Get("hFudge803M2");
  hFudge1063M2 = (TH1D*)fFudge->Get("hFudge1063M2");

//////////// Surface PDFs
///// Crystal M1 and M2
  // hTeO2Spb210M1_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spb210M1_01");
  // hTeO2Spo210M1_001   = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M1_001");
  // hTeO2Spo210M1_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M1_01");
  // hTeO2Sth232M1_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sth232M1_01");
  // hTeO2Su238M1_01     = (TH1D*)fSurfaceCrystal->Get("hTeO2Su238M1_01");
  hTeO2Sxpb210M1_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M1_001");
  hTeO2Sxpb210M1_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M1_01");
  hTeO2Sxpb210M1_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M1_1");
  hTeO2Sxpb210M1_10   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M1_10");
  hTeO2Sxpo210M1_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M1_001");
  hTeO2Sxpo210M1_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M1_01");
  hTeO2Sxpo210M1_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M1_1");
  hTeO2Sxth232M1_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M1_001");
  hTeO2Sxth232M1_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M1_01");
  hTeO2Sxth232M1_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M1_1");
  hTeO2Sxth232M1_10   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M1_10");
  hTeO2Sxu238M1_001   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M1_001");
  hTeO2Sxu238M1_01    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M1_01");
  hTeO2Sxu238M1_1     = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M1_1");
  hTeO2Sxu238M1_10    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M1_10");

  hTeO2Sxu238M1_100    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M1_100");
  hTeO2Sxth232M1_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M1_100");
  hTeO2Sxpb210M1_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M1_100");

  hTeO2Sxth232onlyM1_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232onlyM1_001");
  hTeO2Sxra228pb208M1_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxra228pb208M1_001");
  hTeO2Sxu238th230M1_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238th230M1_001");
  hTeO2Sxth230onlyM1_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth230onlyM1_001");
  hTeO2Sxra226pb210M1_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxra226pb210M1_001");
  hTeO2Sxpb210M1_0001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M1_0001");

  hTeO2Sxth232onlyM1_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM1_01");
  hTeO2Sxra228pb208M1_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M1_01");
  hTeO2Sxu238th230M1_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M1_01");
  hTeO2Sxth230onlyM1_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM1_01");
  hTeO2Sxra226pb210M1_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M1_01");

  hTeO2Sxth232onlyM1_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM1_0001");
  hTeO2Sxra228pb208M1_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M1_0001");
  hTeO2Sxu238th230M1_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M1_0001");
  hTeO2Sxth230onlyM1_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM1_0001");
  hTeO2Sxra226pb210M1_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M1_0001");

  // hTeO2Spb210M2_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spb210M2_01");
  // hTeO2Spo210M2_001   = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M2_001");
  // hTeO2Spo210M2_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M2_01");
  // hTeO2Sth232M2_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sth232M2_01");
  // hTeO2Su238M2_01     = (TH1D*)fSurfaceCrystal->Get("hTeO2Su238M2_01");
  hTeO2Sxpb210M2_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M2_001");
  hTeO2Sxpb210M2_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M2_01");
  hTeO2Sxpb210M2_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M2_1");
  hTeO2Sxpb210M2_10   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M2_10");
  hTeO2Sxpo210M2_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M2_001");
  hTeO2Sxpo210M2_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M2_01");
  hTeO2Sxpo210M2_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpo210M2_1");
  hTeO2Sxth232M2_001  = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M2_001");
  hTeO2Sxth232M2_01   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M2_01");
  hTeO2Sxth232M2_1    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M2_1");
  hTeO2Sxth232M2_10   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232M2_10");
  hTeO2Sxu238M2_001   = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M2_001");
  hTeO2Sxu238M2_01    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M2_01");
  hTeO2Sxu238M2_1     = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M2_1");
  hTeO2Sxu238M2_10    = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238M2_10");

  hTeO2Sxu238M2_100    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2_100");
  hTeO2Sxth232M2_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2_100");
  hTeO2Sxpb210M2_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2_100");

  hTeO2Sxth232onlyM2_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth232onlyM2_001");
  hTeO2Sxra228pb208M2_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxra228pb208M2_001");
  hTeO2Sxu238th230M2_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxu238th230M2_001");
  hTeO2Sxth230onlyM2_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxth230onlyM2_001");
  hTeO2Sxra226pb210M2_001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxra226pb210M2_001");
  hTeO2Sxpb210M2_0001 = (TH1D*)fSurfaceCrystalOld->Get("hTeO2Sxpb210M2_0001");

  hTeO2Sxth232onlyM2_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM2_01");
  hTeO2Sxra228pb208M2_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M2_01");
  hTeO2Sxu238th230M2_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M2_01");
  hTeO2Sxth230onlyM2_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM2_01");
  hTeO2Sxra226pb210M2_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M2_01");

  hTeO2Sxth232onlyM2_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM2_0001");
  hTeO2Sxra228pb208M2_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M2_0001");
  hTeO2Sxu238th230M2_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M2_0001");
  hTeO2Sxth230onlyM2_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM2_0001");
  hTeO2Sxra226pb210M2_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M2_0001");

  // hTeO2Spb210M2Sum_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spb210M2Sum_01");
  // hTeO2Spo210M2Sum_001   = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M2Sum_001");
  // hTeO2Spo210M2Sum_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Spo210M2Sum_01");
  // hTeO2Sth232M2Sum_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sth232M2Sum_01");
  // hTeO2Su238M2Sum_01     = (TH1D*)fSurfaceCrystal->Get("hTeO2Su238M2Sum_01");
  hTeO2Sxpb210M2Sum_001  = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_001");
  hTeO2Sxpb210M2Sum_01   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_01");
  hTeO2Sxpb210M2Sum_1    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_1");
  hTeO2Sxpb210M2Sum_10   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_10");
  hTeO2Sxpo210M2Sum_001  = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpo210M2Sum_001");
  hTeO2Sxpo210M2Sum_01   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpo210M2Sum_01");
  hTeO2Sxpo210M2Sum_1    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpo210M2Sum_1");
  hTeO2Sxth232M2Sum_001  = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2Sum_001");
  hTeO2Sxth232M2Sum_01   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2Sum_01");
  hTeO2Sxth232M2Sum_1    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2Sum_1");
  hTeO2Sxth232M2Sum_10   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2Sum_10");
  hTeO2Sxu238M2Sum_001   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2Sum_001");
  hTeO2Sxu238M2Sum_01    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2Sum_01");
  hTeO2Sxu238M2Sum_1     = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2Sum_1");
  hTeO2Sxu238M2Sum_10    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2Sum_10");

  hTeO2Sxu238M2Sum_100    = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238M2Sum_100");
  hTeO2Sxth232M2Sum_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232M2Sum_100");
  hTeO2Sxpb210M2Sum_100   = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_100");

  hTeO2Sxth232onlyM2Sum_001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM2Sum_001");
  hTeO2Sxra228pb208M2Sum_001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M2Sum_001");
  hTeO2Sxu238th230M2Sum_001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M2Sum_001");
  hTeO2Sxth230onlyM2Sum_001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM2Sum_001");
  hTeO2Sxra226pb210M2Sum_001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M2Sum_001");
  hTeO2Sxpb210M2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxpb210M2Sum_0001");

  hTeO2Sxth232onlyM2Sum_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM2Sum_01");
  hTeO2Sxra228pb208M2Sum_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M2Sum_01");
  hTeO2Sxu238th230M2Sum_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M2Sum_01");
  hTeO2Sxth230onlyM2Sum_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM2Sum_01");
  hTeO2Sxra226pb210M2Sum_01 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M2Sum_01");

  hTeO2Sxth232onlyM2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth232onlyM2Sum_0001");
  hTeO2Sxra228pb208M2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra228pb208M2Sum_0001");
  hTeO2Sxu238th230M2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxu238th230M2Sum_0001");
  hTeO2Sxth230onlyM2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxth230onlyM2Sum_0001");
  hTeO2Sxra226pb210M2Sum_0001 = (TH1D*)fSurfaceCrystal->Get("hTeO2Sxra226pb210M2Sum_0001");

//////// Frame M1 and M2
  // hCuFrameSth232M1_1    = (TH1D*)fSurfaceOther->Get("hCuFrameSth232M1_1");
  // hCuFrameSu238M1_1     = (TH1D*)fSurfaceOther->Get("hCuFrameSu238M1_1");
  hCuFrameSxpb210M1_001 = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M1_001");
  hCuFrameSxpb210M1_01  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M1_01");
  hCuFrameSxpb210M1_1   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M1_1");
  hCuFrameSxpb210M1_10  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M1_10");
  hCuFrameSxth232M1_001 = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M1_001");
  hCuFrameSxth232M1_01  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M1_01");
  hCuFrameSxth232M1_1   = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M1_1");
  hCuFrameSxth232M1_10  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M1_10");
  hCuFrameSxu238M1_001  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M1_001");
  hCuFrameSxu238M1_01   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M1_01");
  hCuFrameSxu238M1_1    = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M1_1");
  hCuFrameSxu238M1_10   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M1_10");

  // hCuFrameSth232M2_1    = (TH1D*)fSurfaceOther->Get("hCuFrameSth232M2_1");
  // hCuFrameSu238M2_1     = (TH1D*)fSurfaceOther->Get("hCuFrameSu238M2_1");
  hCuFrameSxpb210M2_001 = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M2_001");
  hCuFrameSxpb210M2_01  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M2_01");
  hCuFrameSxpb210M2_1   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M2_1");
  hCuFrameSxpb210M2_10  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxpb210M2_10");
  hCuFrameSxth232M2_001 = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2_001");
  hCuFrameSxth232M2_01  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2_01");
  hCuFrameSxth232M2_1   = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2_1");
  hCuFrameSxth232M2_10  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2_10");
  hCuFrameSxu238M2_001  = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M2_001");
  hCuFrameSxu238M2_01   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M2_01");
  hCuFrameSxu238M2_1    = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M2_1");
  hCuFrameSxu238M2_10   = (TH1D*)fSurfaceOtherOld->Get("hCuFrameSxu238M2_10");

  // hCuFrameSth232M2Sum_1    = (TH1D*)fSurfaceOther->Get("hCuFrameSth232M2Sum_1");
  // hCuFrameSu238M2Sum_1     = (TH1D*)fSurfaceOther->Get("hCuFrameSu238M2Sum_1");
  hCuFrameSxpb210M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuFrameSxpb210M2Sum_001");
  hCuFrameSxpb210M2Sum_01  = (TH1D*)fSurfaceOther->Get("hCuFrameSxpb210M2Sum_01");
  hCuFrameSxpb210M2Sum_1   = (TH1D*)fSurfaceOther->Get("hCuFrameSxpb210M2Sum_1");
  hCuFrameSxpb210M2Sum_10  = (TH1D*)fSurfaceOther->Get("hCuFrameSxpb210M2Sum_10");
  hCuFrameSxth232M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2Sum_001");
  hCuFrameSxth232M2Sum_01  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2Sum_01");
  hCuFrameSxth232M2Sum_1   = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2Sum_1");
  hCuFrameSxth232M2Sum_10  = (TH1D*)fSurfaceOther->Get("hCuFrameSxth232M2Sum_10");
  hCuFrameSxu238M2Sum_001  = (TH1D*)fSurfaceOther->Get("hCuFrameSxu238M2Sum_001");
  hCuFrameSxu238M2Sum_01   = (TH1D*)fSurfaceOther->Get("hCuFrameSxu238M2Sum_01");
  hCuFrameSxu238M2Sum_1    = (TH1D*)fSurfaceOther->Get("hCuFrameSxu238M2Sum_1");
  hCuFrameSxu238M2Sum_10   = (TH1D*)fSurfaceOther->Get("hCuFrameSxu238M2Sum_10");

/////// CuBox M1 and M2
  // hCuBoxSth232M1_1    = (TH1D*)fSurfaceOther->Get("hCuBoxSth232M1_1");
  // hCuBoxSu238M1_1     = (TH1D*)fSurfaceOther->Get("hCuBoxSu238M1_1");
  hCuBoxSxpb210M1_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M1_001");
  hCuBoxSxpb210M1_01  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M1_01");
  hCuBoxSxpb210M1_1   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M1_1");
  hCuBoxSxpb210M1_10  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M1_10");
  hCuBoxSxth232M1_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M1_001");
  hCuBoxSxth232M1_01  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M1_01");
  hCuBoxSxth232M1_1   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M1_1");
  hCuBoxSxth232M1_10  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M1_10");
  hCuBoxSxu238M1_001  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M1_001");
  hCuBoxSxu238M1_01   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M1_01");
  hCuBoxSxu238M1_1    = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M1_1");
  hCuBoxSxu238M1_10   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M1_10");

  // hCuBoxSth232M2_1    = (TH1D*)fSurfaceOther->Get("hCuBoxSth232M2_1");
  // hCuBoxSu238M2_1     = (TH1D*)fSurfaceOther->Get("hCuBoxSu238M2_1");
  hCuBoxSxpb210M2_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M2_001");
  hCuBoxSxpb210M2_01  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M2_01");
  hCuBoxSxpb210M2_1   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M2_1");
  hCuBoxSxpb210M2_10  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxpb210M2_10");
  hCuBoxSxth232M2_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M2_001");
  hCuBoxSxth232M2_01  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M2_01");
  hCuBoxSxth232M2_1   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M2_1");
  hCuBoxSxth232M2_10  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxth232M2_10");
  hCuBoxSxu238M2_001  = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M2_001");
  hCuBoxSxu238M2_01   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M2_01");
  hCuBoxSxu238M2_1    = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M2_1");
  hCuBoxSxu238M2_10   = (TH1D*)fSurfaceOtherOld->Get("hCuBoxSxu238M2_10");

  // hCuBoxSth232M2Sum_1    = (TH1D*)fSurfaceOther->Get("hCuBoxSth232M2Sum_1");
  // hCuBoxSu238M2Sum_1     = (TH1D*)fSurfaceOther->Get("hCuBoxSu238M2Sum_1");
  hCuBoxSxpb210M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuBoxSxpb210M2Sum_001");
  hCuBoxSxpb210M2Sum_01  = (TH1D*)fSurfaceOther->Get("hCuBoxSxpb210M2Sum_01");
  hCuBoxSxpb210M2Sum_1   = (TH1D*)fSurfaceOther->Get("hCuBoxSxpb210M2Sum_1");
  hCuBoxSxpb210M2Sum_10  = (TH1D*)fSurfaceOther->Get("hCuBoxSxpb210M2Sum_10");
  hCuBoxSxth232M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuBoxSxth232M2Sum_001");
  hCuBoxSxth232M2Sum_01  = (TH1D*)fSurfaceOther->Get("hCuBoxSxth232M2Sum_01");
  hCuBoxSxth232M2Sum_1   = (TH1D*)fSurfaceOther->Get("hCuBoxSxth232M2Sum_1");
  hCuBoxSxth232M2Sum_10  = (TH1D*)fSurfaceOther->Get("hCuBoxSxth232M2Sum_10");
  hCuBoxSxu238M2Sum_001  = (TH1D*)fSurfaceOther->Get("hCuBoxSxu238M2Sum_001");
  hCuBoxSxu238M2Sum_01   = (TH1D*)fSurfaceOther->Get("hCuBoxSxu238M2Sum_01");
  hCuBoxSxu238M2Sum_1    = (TH1D*)fSurfaceOther->Get("hCuBoxSxu238M2Sum_1");
  hCuBoxSxu238M2Sum_10   = (TH1D*)fSurfaceOther->Get("hCuBoxSxu238M2Sum_10");

/////// CuBox + CuFrame

  hCuBox_CuFrameth232M1_10 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M1_10");
  hCuBox_CuFrameu238M1_10 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M1_10");
  hCuBox_CuFramepb210M1_10 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M1_10");
  hCuBox_CuFramepb210M1_1 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M1_1");
  hCuBox_CuFramepb210M1_01 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M1_01");
  hCuBox_CuFramepb210M1_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M1_001");

  hCuBox_CuFrameth232M1_1 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M1_1");
  hCuBox_CuFrameu238M1_1 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M1_1");
  hCuBox_CuFrameth232M1_01 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M1_01");
  hCuBox_CuFrameu238M1_01 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M1_01");
  hCuBox_CuFrameth232M1_001 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M1_001");
  hCuBox_CuFrameu238M1_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M1_001");  

  hCuBox_CuFrameth232M2_10 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2_10");
  hCuBox_CuFrameu238M2_10 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2_10");
  hCuBox_CuFramepb210M2_10 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M2_10");
  hCuBox_CuFramepb210M2_1 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M2_1");
  hCuBox_CuFramepb210M2_01 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M2_01");
  hCuBox_CuFramepb210M2_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFramepb210M2_001");

  hCuBox_CuFrameth232M2_1 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2_1");
  hCuBox_CuFrameu238M2_1 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2_1");
  hCuBox_CuFrameth232M2_01 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2_01");
  hCuBox_CuFrameu238M2_01 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2_01");
  hCuBox_CuFrameth232M2_001 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2_001");
  hCuBox_CuFrameu238M2_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2_001"); 

  hCuBox_CuFrameth232M2Sum_10 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2Sum_10");
  hCuBox_CuFrameu238M2Sum_10 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameu238M2Sum_10");
  hCuBox_CuFramepb210M2Sum_10 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFramepb210M2Sum_10");
  hCuBox_CuFramepb210M2Sum_1 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFramepb210M2Sum_1");
  hCuBox_CuFramepb210M2Sum_01 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFramepb210M2Sum_01");
  hCuBox_CuFramepb210M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFramepb210M2Sum_001"); 

  hCuBox_CuFrameth232M2Sum_1 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2Sum_1");
  hCuBox_CuFrameu238M2Sum_1 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2Sum_1");
  hCuBox_CuFrameth232M2Sum_01 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2Sum_01");
  hCuBox_CuFrameu238M2Sum_01 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2Sum_01");
  hCuBox_CuFrameth232M2Sum_001 = (TH1D*)fSurfaceOther->Get("hCuBox_CuFrameth232M2Sum_001");
  hCuBox_CuFrameu238M2Sum_001 = (TH1D*)fSurfaceOtherOld->Get("hCuBox_CuFrameu238M2Sum_001"); 

///////////// Get adaptive binned histograms
//////// Crystal M1 and M2
  hnewTeO20nuM1 = hTeO20nuM1->Rebin(dAdaptiveBinsM1, "hnewTeO20nuM1", dAdaptiveArrayM1);
  hnewTeO22nuM1 = hTeO22nuM1->Rebin(dAdaptiveBinsM1, "hnewTeO22nuM1", dAdaptiveArrayM1);
  hnewTeO2co60M1 = hTeO2co60M1->Rebin(dAdaptiveBinsM1, "hnewTeO2co60M1", dAdaptiveArrayM1);
  hnewTeO2k40M1 = hTeO2k40M1->Rebin(dAdaptiveBinsM1, "hnewTeO2k40M1", dAdaptiveArrayM1);
  hnewTeO2pb210M1 = hTeO2pb210M1->Rebin(dAdaptiveBinsM1, "hnewTeO2pb210M1", dAdaptiveArrayM1);
  hnewTeO2po210M1 = hTeO2po210M1->Rebin(dAdaptiveBinsM1, "hnewTeO2po210M1", dAdaptiveArrayM1);
  hnewTeO2te125M1 = hTeO2te125M1->Rebin(dAdaptiveBinsM1, "hnewTeO2te125M1", dAdaptiveArrayM1);
  hnewTeO2th232M1 = hTeO2th232M1->Rebin(dAdaptiveBinsM1, "hnewTeO2th232M1", dAdaptiveArrayM1);
  hnewTeO2u238M1 = hTeO2u238M1->Rebin(dAdaptiveBinsM1, "hnewTeO2u238M1", dAdaptiveArrayM1);

  // hnewTeO2Spb210M1_01 = hTeO2Spb210M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Spb210M1_01", dAdaptiveArrayM1);
  // hnewTeO2Spo210M1_001 = hTeO2Spo210M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Spo210M1_001", dAdaptiveArrayM1);
  // hnewTeO2Spo210M1_01 = hTeO2Spo210M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Spo210M1_01", dAdaptiveArrayM1);
  // hnewTeO2Sth232M1_01 = hTeO2Sth232M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sth232M1_01", dAdaptiveArrayM1);
  // hnewTeO2Su238M1_01 = hTeO2Su238M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Su238M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_001 = hTeO2Sxpb210M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_01 = hTeO2Sxpb210M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_1 = hTeO2Sxpb210M1_1->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_1", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_10 = hTeO2Sxpb210M1_10->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_10", dAdaptiveArrayM1);
  hnewTeO2Sxpo210M1_001 = hTeO2Sxpo210M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpo210M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxpo210M1_01 = hTeO2Sxpo210M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpo210M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxpo210M1_1 = hTeO2Sxpo210M1_1->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpo210M1_1", dAdaptiveArrayM1);
  hnewTeO2Sxth232M1_001 = hTeO2Sxth232M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxth232M1_01 = hTeO2Sxth232M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxth232M1_1 = hTeO2Sxth232M1_1->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232M1_1", dAdaptiveArrayM1);
  hnewTeO2Sxth232M1_10 = hTeO2Sxth232M1_10->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232M1_10", dAdaptiveArrayM1);
  hnewTeO2Sxu238M1_001 = hTeO2Sxu238M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxu238M1_01 = hTeO2Sxu238M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxu238M1_1  = hTeO2Sxu238M1_1->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238M1_1", dAdaptiveArrayM1);
  hnewTeO2Sxu238M1_10 = hTeO2Sxu238M1_10->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238M1_10", dAdaptiveArrayM1);

  hnewTeO2Sxu238M1_100 = hTeO2Sxu238M1_100->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238M1_100", dAdaptiveArrayM1);
  hnewTeO2Sxth232M1_100 = hTeO2Sxth232M1_100->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232M1_100", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_100 = hTeO2Sxpb210M1_100->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_100", dAdaptiveArrayM1);


  hnewTeO2th232onlyM1 = hTeO2th232onlyM1->Rebin(dAdaptiveBinsM1, "hnewTeO2th232onlyM1", dAdaptiveArrayM1);
  hnewTeO2ra228pb208M1 = hTeO2ra228pb208M1->Rebin(dAdaptiveBinsM1, "hnewTeO2ra228pb208M1", dAdaptiveArrayM1);
  hnewTeO2th230onlyM1 = hTeO2th230onlyM1->Rebin(dAdaptiveBinsM1, "hnewTeO2th230onlyM1", dAdaptiveArrayM1);

  hnewTeO2Sxth232onlyM1_001 = hTeO2Sxth232onlyM1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232onlyM1_001", dAdaptiveArrayM1);
  hnewTeO2Sxra228pb208M1_001 = hTeO2Sxra228pb208M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra228pb208M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxu238th230M1_001 = hTeO2Sxu238th230M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238th230M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxth230onlyM1_001 = hTeO2Sxth230onlyM1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth230onlyM1_001", dAdaptiveArrayM1);
  hnewTeO2Sxra226pb210M1_001 = hTeO2Sxra226pb210M1_001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra226pb210M1_001", dAdaptiveArrayM1);
  hnewTeO2Sxpb210M1_0001 = hTeO2Sxpb210M1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxpb210M1_0001", dAdaptiveArrayM1);

  hnewTeO2Sxth232onlyM1_01 = hTeO2Sxth232onlyM1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232onlyM1_01", dAdaptiveArrayM1);
  hnewTeO2Sxra228pb208M1_01 = hTeO2Sxra228pb208M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra228pb208M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxu238th230M1_01 = hTeO2Sxu238th230M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238th230M1_01", dAdaptiveArrayM1);
  hnewTeO2Sxth230onlyM1_01 = hTeO2Sxth230onlyM1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth230onlyM1_01", dAdaptiveArrayM1);
  hnewTeO2Sxra226pb210M1_01 = hTeO2Sxra226pb210M1_01->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra226pb210M1_01", dAdaptiveArrayM1);

  hnewTeO2Sxth232onlyM1_0001 = hTeO2Sxth232onlyM1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth232onlyM1_0001", dAdaptiveArrayM1);
  hnewTeO2Sxra228pb208M1_0001 = hTeO2Sxra228pb208M1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra228pb208M1_0001", dAdaptiveArrayM1);
  hnewTeO2Sxu238th230M1_0001 = hTeO2Sxu238th230M1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxu238th230M1_0001", dAdaptiveArrayM1);
  hnewTeO2Sxth230onlyM1_0001 = hTeO2Sxth230onlyM1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxth230onlyM1_0001", dAdaptiveArrayM1);
  hnewTeO2Sxra226pb210M1_0001 = hTeO2Sxra226pb210M1_0001->Rebin(dAdaptiveBinsM1, "hnewTeO2Sxra226pb210M1_0001", dAdaptiveArrayM1);  

  hnewTeO20nuM2 = hTeO20nuM2->Rebin(dAdaptiveBinsM2, "hnewTeO20nuM2", dAdaptiveArrayM2);
  hnewTeO22nuM2 = hTeO22nuM2->Rebin(dAdaptiveBinsM2, "hnewTeO22nuM2", dAdaptiveArrayM2);
  hnewTeO2co60M2 = hTeO2co60M2->Rebin(dAdaptiveBinsM2, "hnewTeO2co60M2", dAdaptiveArrayM2);
  hnewTeO2k40M2 = hTeO2k40M2->Rebin(dAdaptiveBinsM2, "hnewTeO2k40M2", dAdaptiveArrayM2);
  hnewTeO2pb210M2 = hTeO2pb210M2->Rebin(dAdaptiveBinsM2, "hnewTeO2pb210M2", dAdaptiveArrayM2);
  hnewTeO2po210M2 = hTeO2po210M2->Rebin(dAdaptiveBinsM2, "hnewTeO2po210M2", dAdaptiveArrayM2);
  hnewTeO2te125M2 = hTeO2te125M2->Rebin(dAdaptiveBinsM2, "hnewTeO2te125M2", dAdaptiveArrayM2);
  hnewTeO2th232M2 = hTeO2th232M2->Rebin(dAdaptiveBinsM2, "hnewTeO2th232M2", dAdaptiveArrayM2);
  hnewTeO2u238M2 = hTeO2u238M2->Rebin(dAdaptiveBinsM2, "hnewTeO2u238M2", dAdaptiveArrayM2);

  // hnewTeO2Spb210M2_01 = hTeO2Spb210M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Spb210M2_01", dAdaptiveArrayM2);
  // hnewTeO2Spo210M2_001 = hTeO2Spo210M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Spo210M2_001", dAdaptiveArrayM2);
  // hnewTeO2Spo210M2_01 = hTeO2Spo210M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Spo210M2_01", dAdaptiveArrayM2);
  // hnewTeO2Sth232M2_01 = hTeO2Sth232M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sth232M2_01", dAdaptiveArrayM2);
  // hnewTeO2Su238M2_01 = hTeO2Su238M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Su238M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_001 = hTeO2Sxpb210M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_01 = hTeO2Sxpb210M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_1 = hTeO2Sxpb210M2_1->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_1", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_10 = hTeO2Sxpb210M2_10->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_10", dAdaptiveArrayM2);
  hnewTeO2Sxpo210M2_001 = hTeO2Sxpo210M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpo210M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxpo210M2_01 = hTeO2Sxpo210M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpo210M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxpo210M2_1 = hTeO2Sxpo210M2_1->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpo210M2_1", dAdaptiveArrayM2);
  hnewTeO2Sxth232M2_001 = hTeO2Sxth232M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxth232M2_01 = hTeO2Sxth232M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxth232M2_1 = hTeO2Sxth232M2_1->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232M2_1", dAdaptiveArrayM2);
  hnewTeO2Sxth232M2_10 = hTeO2Sxth232M2_10->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232M2_10", dAdaptiveArrayM2);
  hnewTeO2Sxu238M2_001 = hTeO2Sxu238M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxu238M2_01 = hTeO2Sxu238M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxu238M2_1 = hTeO2Sxu238M2_1->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238M2_1", dAdaptiveArrayM2);
  hnewTeO2Sxu238M2_10 = hTeO2Sxu238M2_10->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238M2_10", dAdaptiveArrayM2);

  hnewTeO2Sxu238M2_100 = hTeO2Sxu238M2_100->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238M2_100", dAdaptiveArrayM2);
  hnewTeO2Sxth232M2_100 = hTeO2Sxth232M2_100->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232M2_100", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_100 = hTeO2Sxpb210M2_100->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_100", dAdaptiveArrayM2);

  hnewTeO2th232onlyM2 = hTeO2th232onlyM2->Rebin(dAdaptiveBinsM2, "hnewTeO2th232onlyM2", dAdaptiveArrayM2);
  hnewTeO2ra228pb208M2 = hTeO2ra228pb208M2->Rebin(dAdaptiveBinsM2, "hnewTeO2ra228pb208M2", dAdaptiveArrayM2);
  hnewTeO2th230onlyM2 = hTeO2th230onlyM2->Rebin(dAdaptiveBinsM2, "hnewTeO2th230onlyM2", dAdaptiveArrayM2);

  hnewTeO2Sxth232onlyM2_001 = hTeO2Sxth232onlyM2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232onlyM2_001", dAdaptiveArrayM2);
  hnewTeO2Sxra228pb208M2_001 = hTeO2Sxra228pb208M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra228pb208M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxu238th230M2_001 = hTeO2Sxu238th230M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238th230M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxth230onlyM2_001 = hTeO2Sxth230onlyM2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth230onlyM2_001", dAdaptiveArrayM2);
  hnewTeO2Sxra226pb210M2_001 = hTeO2Sxra226pb210M2_001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra226pb210M2_001", dAdaptiveArrayM2);
  hnewTeO2Sxpb210M2_0001 = hTeO2Sxpb210M2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxpb210M2_0001", dAdaptiveArrayM2);

  hnewTeO2Sxth232onlyM2_01 = hTeO2Sxth232onlyM2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232onlyM2_01", dAdaptiveArrayM2);
  hnewTeO2Sxra228pb208M2_01 = hTeO2Sxra228pb208M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra228pb208M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxu238th230M2_01 = hTeO2Sxu238th230M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238th230M2_01", dAdaptiveArrayM2);
  hnewTeO2Sxth230onlyM2_01 = hTeO2Sxth230onlyM2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth230onlyM2_01", dAdaptiveArrayM2);
  hnewTeO2Sxra226pb210M2_01 = hTeO2Sxra226pb210M2_01->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra226pb210M2_01", dAdaptiveArrayM2);

  hnewTeO2Sxth232onlyM2_0001 = hTeO2Sxth232onlyM2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth232onlyM2_0001", dAdaptiveArrayM2);
  hnewTeO2Sxra228pb208M2_0001 = hTeO2Sxra228pb208M2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra228pb208M2_0001", dAdaptiveArrayM2);
  hnewTeO2Sxu238th230M2_0001 = hTeO2Sxu238th230M2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxu238th230M2_0001", dAdaptiveArrayM2);
  hnewTeO2Sxth230onlyM2_0001 = hTeO2Sxth230onlyM2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxth230onlyM2_0001", dAdaptiveArrayM2);
  hnewTeO2Sxra226pb210M2_0001 = hTeO2Sxra226pb210M2_0001->Rebin(dAdaptiveBinsM2, "hnewTeO2Sxra226pb210M2_0001", dAdaptiveArrayM2);  

  hnewTeO20nuM2Sum = hTeO20nuM2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO20nuM2Sum", dAdaptiveArrayM2Sum);
  hnewTeO22nuM2Sum = hTeO22nuM2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO22nuM2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2co60M2Sum = hTeO2co60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2co60M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2k40M2Sum = hTeO2k40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2k40M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2pb210M2Sum = hTeO2pb210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2pb210M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2po210M2Sum = hTeO2po210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2po210M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2te125M2Sum = hTeO2te125M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2te125M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2th232M2Sum = hTeO2th232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2th232M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2u238M2Sum = hTeO2u238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2u238M2Sum", dAdaptiveArrayM2Sum);

  // hnewTeO2Spb210M2Sum_01 = hTeO2Spb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Spb210M2Sum_01", dAdaptiveArrayM2Sum);
  // hnewTeO2Spo210M2Sum_001 = hTeO2Spo210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Spo210M2Sum_001", dAdaptiveArrayM2Sum);
  // hnewTeO2Spo210M2Sum_01 = hTeO2Spo210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Spo210M2Sum_01", dAdaptiveArrayM2Sum);
  // hnewTeO2Sth232M2Sum_01 = hTeO2Sth232M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sth232M2Sum_01", dAdaptiveArrayM2Sum);
  // hnewTeO2Su238M2Sum_01 = hTeO2Su238M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Su238M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_001 = hTeO2Sxpb210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_01 = hTeO2Sxpb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_1 = hTeO2Sxpb210M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_1", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_10 = hTeO2Sxpb210M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_10", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpo210M2Sum_001 = hTeO2Sxpo210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpo210M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpo210M2Sum_01 = hTeO2Sxpo210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpo210M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpo210M2Sum_1 = hTeO2Sxpo210M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpo210M2Sum_1", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth232M2Sum_001 = hTeO2Sxth232M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth232M2Sum_01 = hTeO2Sxth232M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth232M2Sum_1 = hTeO2Sxth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth232M2Sum_10 = hTeO2Sxth232M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232M2Sum_10", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238M2Sum_001 = hTeO2Sxu238M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238M2Sum_01 = hTeO2Sxu238M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238M2Sum_1 = hTeO2Sxu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238M2Sum_10 = hTeO2Sxu238M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238M2Sum_10", dAdaptiveArrayM2Sum);

  hnewTeO2Sxu238M2Sum_100 = hTeO2Sxu238M2Sum_100->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238M2Sum_100", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth232M2Sum_100 = hTeO2Sxth232M2Sum_100->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232M2Sum_100", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_100 = hTeO2Sxpb210M2Sum_100->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_100", dAdaptiveArrayM2Sum);

  hnewTeO2th232onlyM2Sum = hTeO2th232onlyM2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2th232onlyM2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2ra228pb208M2Sum = hTeO2ra228pb208M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2ra228pb208M2Sum", dAdaptiveArrayM2Sum);
  hnewTeO2th230onlyM2Sum = hTeO2th230onlyM2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2th230onlyM2Sum", dAdaptiveArrayM2Sum);

  hnewTeO2Sxth232onlyM2Sum_001 = hTeO2Sxth232onlyM2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232onlyM2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra228pb208M2Sum_001 = hTeO2Sxra228pb208M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra228pb208M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238th230M2Sum_001 = hTeO2Sxu238th230M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238th230M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth230onlyM2Sum_001 = hTeO2Sxth230onlyM2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth230onlyM2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra226pb210M2Sum_001 = hTeO2Sxra226pb210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra226pb210M2Sum_001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxpb210M2Sum_0001 = hTeO2Sxpb210M2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxpb210M2Sum_0001", dAdaptiveArrayM2Sum);

  hnewTeO2Sxth232onlyM2Sum_01 = hTeO2Sxth232onlyM2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232onlyM2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra228pb208M2Sum_01 = hTeO2Sxra228pb208M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra228pb208M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238th230M2Sum_01 = hTeO2Sxu238th230M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238th230M2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth230onlyM2Sum_01 = hTeO2Sxth230onlyM2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth230onlyM2Sum_01", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra226pb210M2Sum_01 = hTeO2Sxra226pb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra226pb210M2Sum_01", dAdaptiveArrayM2Sum);

  hnewTeO2Sxth232onlyM2Sum_0001 = hTeO2Sxth232onlyM2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth232onlyM2Sum_0001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra228pb208M2Sum_0001 = hTeO2Sxra228pb208M2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra228pb208M2Sum_0001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxu238th230M2Sum_0001 = hTeO2Sxu238th230M2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxu238th230M2Sum_0001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxth230onlyM2Sum_0001 = hTeO2Sxth230onlyM2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxth230onlyM2Sum_0001", dAdaptiveArrayM2Sum);
  hnewTeO2Sxra226pb210M2Sum_0001 = hTeO2Sxra226pb210M2Sum_0001->Rebin(dAdaptiveBinsM2Sum, "hnewTeO2Sxra226pb210M2Sum_0001", dAdaptiveArrayM2Sum);  

////////// Frame M1 and M2
  hnewCuFrameco58M1 = hCuFrameco58M1->Rebin(dAdaptiveBinsM1, "hnewCuFrameco58M1", dAdaptiveArrayM1);
  hnewCuFrameco60M1 = hCuFrameco60M1->Rebin(dAdaptiveBinsM1, "hnewCuFrameco60M1", dAdaptiveArrayM1);
  hnewCuFramecs137M1 = hCuFramecs137M1->Rebin(dAdaptiveBinsM1, "hnewCuFramecs137M1", dAdaptiveArrayM1);
  hnewCuFramek40M1 = hCuFramek40M1->Rebin(dAdaptiveBinsM1, "hnewCuFramek40M1", dAdaptiveArrayM1);
  hnewCuFramemn54M1 = hCuFramemn54M1->Rebin(dAdaptiveBinsM1, "hnewCuFramemn54M1", dAdaptiveArrayM1);
  hnewCuFramepb210M1 = hCuFramepb210M1->Rebin(dAdaptiveBinsM1, "hnewCuFramepb210M1", dAdaptiveArrayM1);
  hnewCuFrameth232M1 = hCuFrameth232M1->Rebin(dAdaptiveBinsM1, "hnewCuFrameth232M1", dAdaptiveArrayM1);
  hnewCuFrameu238M1 = hCuFrameu238M1->Rebin(dAdaptiveBinsM1, "hnewCuFrameu238M1", dAdaptiveArrayM1);

  // hnewCuFrameSth232M1_1  = hCuFrameSth232M1_1->Rebin(dAdaptiveBinsM1, "hnewCuFrameSth232M1_1", dAdaptiveArrayM1);
  // hnewCuFrameSu238M1_1 = hCuFrameSu238M1_1->Rebin(dAdaptiveBinsM1, "hnewCuFrameSu238M1_1", dAdaptiveArrayM1);
  hnewCuFrameSxpb210M1_001 = hCuFrameSxpb210M1_001->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxpb210M1_001", dAdaptiveArrayM1);
  hnewCuFrameSxpb210M1_01 = hCuFrameSxpb210M1_01->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxpb210M1_01", dAdaptiveArrayM1);
  hnewCuFrameSxpb210M1_1 = hCuFrameSxpb210M1_1->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxpb210M1_1", dAdaptiveArrayM1);
  hnewCuFrameSxpb210M1_10 = hCuFrameSxpb210M1_10->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxpb210M1_10", dAdaptiveArrayM1);
  hnewCuFrameSxth232M1_001 = hCuFrameSxth232M1_001->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxth232M1_001", dAdaptiveArrayM1);
  hnewCuFrameSxth232M1_01 = hCuFrameSxth232M1_01->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxth232M1_01", dAdaptiveArrayM1);
  hnewCuFrameSxth232M1_1 = hCuFrameSxth232M1_1->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxth232M1_1", dAdaptiveArrayM1);
  hnewCuFrameSxth232M1_10 = hCuFrameSxth232M1_10->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxth232M1_10", dAdaptiveArrayM1);
  hnewCuFrameSxu238M1_001 = hCuFrameSxu238M1_001->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxu238M1_001", dAdaptiveArrayM1);
  hnewCuFrameSxu238M1_01 = hCuFrameSxu238M1_01->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxu238M1_01", dAdaptiveArrayM1);
  hnewCuFrameSxu238M1_1 = hCuFrameSxu238M1_1->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxu238M1_1", dAdaptiveArrayM1);
  hnewCuFrameSxu238M1_10 = hCuFrameSxu238M1_10->Rebin(dAdaptiveBinsM1, "hnewCuFrameSxu238M1_10", dAdaptiveArrayM1);

  hnewCuFrameco58M2 = hCuFrameco58M2->Rebin(dAdaptiveBinsM2, "hnewCuFrameco58M2", dAdaptiveArrayM2);
  hnewCuFrameco60M2 = hCuFrameco60M2->Rebin(dAdaptiveBinsM2, "hnewCuFrameco60M2", dAdaptiveArrayM2);
  hnewCuFramecs137M2 = hCuFramecs137M2->Rebin(dAdaptiveBinsM2, "hnewCuFramecs137M2", dAdaptiveArrayM2);
  hnewCuFramek40M2 = hCuFramek40M2->Rebin(dAdaptiveBinsM2, "hnewCuFramek40M2", dAdaptiveArrayM2);
  hnewCuFramemn54M2 = hCuFramemn54M2->Rebin(dAdaptiveBinsM2, "hnewCuFramemn54M2", dAdaptiveArrayM2);
  hnewCuFramepb210M2 = hCuFramepb210M2->Rebin(dAdaptiveBinsM2, "hnewCuFramepb210M2", dAdaptiveArrayM2);
  hnewCuFrameth232M2 = hCuFrameth232M2->Rebin(dAdaptiveBinsM2, "hnewCuFrameth232M2", dAdaptiveArrayM2);
  hnewCuFrameu238M2 = hCuFrameu238M2->Rebin(dAdaptiveBinsM2, "hnewCuFrameu238M2", dAdaptiveArrayM2);

  // hnewCuFrameSth232M2_1 = hCuFrameSth232M2_1->Rebin(dAdaptiveBinsM2, "hnewCuFrameSth232M2_1", dAdaptiveArrayM2);
  // hnewCuFrameSu238M2_1 = hCuFrameSu238M2_1->Rebin(dAdaptiveBinsM2, "hnewCuFrameSu238M2_1", dAdaptiveArrayM2);
  hnewCuFrameSxpb210M2_001 = hCuFrameSxpb210M2_001->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxpb210M2_001", dAdaptiveArrayM2);
  hnewCuFrameSxpb210M2_01 = hCuFrameSxpb210M2_01->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxpb210M2_01", dAdaptiveArrayM2);
  hnewCuFrameSxpb210M2_1 = hCuFrameSxpb210M2_1->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxpb210M2_1", dAdaptiveArrayM2);
  hnewCuFrameSxpb210M2_10 = hCuFrameSxpb210M2_10->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxpb210M2_10", dAdaptiveArrayM2);
  hnewCuFrameSxth232M2_001 = hCuFrameSxth232M2_001->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxth232M2_001", dAdaptiveArrayM2);
  hnewCuFrameSxth232M2_01 = hCuFrameSxth232M2_01->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxth232M2_01", dAdaptiveArrayM2);
  hnewCuFrameSxth232M2_1 = hCuFrameSxth232M2_1->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxth232M2_1", dAdaptiveArrayM2);
  hnewCuFrameSxth232M2_10 = hCuFrameSxth232M2_10->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxth232M2_10", dAdaptiveArrayM2);
  hnewCuFrameSxu238M2_001 = hCuFrameSxu238M2_001->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxu238M2_001", dAdaptiveArrayM2);
  hnewCuFrameSxu238M2_01 = hCuFrameSxu238M2_01->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxu238M2_01", dAdaptiveArrayM2);
  hnewCuFrameSxu238M2_1 = hCuFrameSxu238M2_1->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxu238M2_1", dAdaptiveArrayM2);
  hnewCuFrameSxu238M2_10 = hCuFrameSxu238M2_10->Rebin(dAdaptiveBinsM2, "hnewCuFrameSxu238M2_10", dAdaptiveArrayM2);

  hnewCuFrameco58M2Sum = hCuFrameco58M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameco58M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFrameco60M2Sum = hCuFrameco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameco60M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFramecs137M2Sum = hCuFramecs137M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFramecs137M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFramek40M2Sum = hCuFramek40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFramek40M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFramemn54M2Sum = hCuFramemn54M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFramemn54M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFramepb210M2Sum = hCuFramepb210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFramepb210M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFrameth232M2Sum = hCuFrameth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameth232M2Sum", dAdaptiveArrayM2Sum);
  hnewCuFrameu238M2Sum = hCuFrameu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameu238M2Sum", dAdaptiveArrayM2Sum);

  hnewCuFrameSth232M2Sum_1 = hCuFrameSth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuFrameSu238M2Sum_1 = hCuFrameSu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuFrameSxpb210M2Sum_001 = hCuFrameSxpb210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxpb210M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuFrameSxpb210M2Sum_01 = hCuFrameSxpb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxpb210M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuFrameSxpb210M2Sum_1 = hCuFrameSxpb210M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxpb210M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuFrameSxpb210M2Sum_10 = hCuFrameSxpb210M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxpb210M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuFrameSxth232M2Sum_001 = hCuFrameSxth232M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxth232M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuFrameSxth232M2Sum_01 = hCuFrameSxth232M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxth232M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuFrameSxth232M2Sum_1 = hCuFrameSxth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuFrameSxth232M2Sum_10 = hCuFrameSxth232M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxth232M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuFrameSxu238M2Sum_001 = hCuFrameSxu238M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxu238M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuFrameSxu238M2Sum_01 = hCuFrameSxu238M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxu238M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuFrameSxu238M2Sum_1 = hCuFrameSxu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuFrameSxu238M2Sum_10 = hCuFrameSxu238M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuFrameSxu238M2Sum_10", dAdaptiveArrayM2Sum);

///////// CuBox (TShield) M1 and M2
  hnewCuBoxco58M1 = hCuBoxco58M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxco58M1", dAdaptiveArrayM1);
  hnewCuBoxco60M1 = hCuBoxco60M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxco60M1", dAdaptiveArrayM1);
  hnewCuBoxcs137M1 = hCuBoxcs137M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxcs137M1", dAdaptiveArrayM1);
  hnewCuBoxk40M1 = hCuBoxk40M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxk40M1", dAdaptiveArrayM1);
  hnewCuBoxmn54M1 = hCuBoxmn54M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxmn54M1", dAdaptiveArrayM1);
  hnewCuBoxpb210M1 = hCuBoxpb210M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxpb210M1", dAdaptiveArrayM1);
  hnewCuBoxth232M1 = hCuBoxth232M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxth232M1", dAdaptiveArrayM1);
  hnewCuBoxu238M1 = hCuBoxu238M1->Rebin(dAdaptiveBinsM1, "hnewCuBoxu238M1", dAdaptiveArrayM1);

  // hnewCuBoxSth232M1_1 = hCuBoxSth232M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBoxSth232M1_1", dAdaptiveArrayM1);
  // hnewCuBoxSu238M1_1 = hCuBoxSu238M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBoxSu238M1_1", dAdaptiveArrayM1);
  hnewCuBoxSxpb210M1_001 = hCuBoxSxpb210M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxpb210M1_001", dAdaptiveArrayM1);
  hnewCuBoxSxpb210M1_01 = hCuBoxSxpb210M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxpb210M1_01", dAdaptiveArrayM1);
  hnewCuBoxSxpb210M1_1 = hCuBoxSxpb210M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxpb210M1_1", dAdaptiveArrayM1);
  hnewCuBoxSxpb210M1_10 = hCuBoxSxpb210M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxpb210M1_10", dAdaptiveArrayM1);
  hnewCuBoxSxth232M1_001 = hCuBoxSxth232M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxth232M1_001", dAdaptiveArrayM1);
  hnewCuBoxSxth232M1_01 = hCuBoxSxth232M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxth232M1_01", dAdaptiveArrayM1);
  hnewCuBoxSxth232M1_1 = hCuBoxSxth232M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxth232M1_1", dAdaptiveArrayM1);
  hnewCuBoxSxth232M1_10 = hCuBoxSxth232M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxth232M1_10", dAdaptiveArrayM1);
  hnewCuBoxSxu238M1_001 = hCuBoxSxu238M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxu238M1_001", dAdaptiveArrayM1);
  hnewCuBoxSxu238M1_01 = hCuBoxSxu238M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxu238M1_01", dAdaptiveArrayM1);
  hnewCuBoxSxu238M1_1 = hCuBoxSxu238M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxu238M1_1", dAdaptiveArrayM1);
  hnewCuBoxSxu238M1_10 = hCuBoxSxu238M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBoxSxu238M1_10", dAdaptiveArrayM1);

  hnewCuBoxco58M2 = hCuBoxco58M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxco58M2", dAdaptiveArrayM2);
  hnewCuBoxco60M2 = hCuBoxco60M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxco60M2", dAdaptiveArrayM2);
  hnewCuBoxcs137M2 = hCuBoxcs137M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxcs137M2", dAdaptiveArrayM2);
  hnewCuBoxk40M2 = hCuBoxk40M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxk40M2", dAdaptiveArrayM2);
  hnewCuBoxmn54M2 = hCuBoxmn54M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxmn54M2", dAdaptiveArrayM2);
  hnewCuBoxpb210M2 = hCuBoxpb210M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxpb210M2", dAdaptiveArrayM2);
  hnewCuBoxth232M2 = hCuBoxth232M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxth232M2", dAdaptiveArrayM2);
  hnewCuBoxu238M2 = hCuBoxu238M2->Rebin(dAdaptiveBinsM2, "hnewCuBoxu238M2", dAdaptiveArrayM2);

  // hnewCuBoxSth232M2_1 = hCuBoxSth232M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBoxSth232M2_1", dAdaptiveArrayM2);
  // hnewCuBoxSu238M2_1 = hCuBoxSu238M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBoxSu238M2_1", dAdaptiveArrayM2);
  hnewCuBoxSxpb210M2_001 = hCuBoxSxpb210M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxpb210M2_001", dAdaptiveArrayM2);
  hnewCuBoxSxpb210M2_01 = hCuBoxSxpb210M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxpb210M2_01", dAdaptiveArrayM2);
  hnewCuBoxSxpb210M2_1 = hCuBoxSxpb210M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxpb210M2_1", dAdaptiveArrayM2);
  hnewCuBoxSxpb210M2_10 = hCuBoxSxpb210M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxpb210M2_10", dAdaptiveArrayM2);
  hnewCuBoxSxth232M2_001 = hCuBoxSxth232M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxth232M2_001", dAdaptiveArrayM2);
  hnewCuBoxSxth232M2_01 = hCuBoxSxth232M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxth232M2_01", dAdaptiveArrayM2);
  hnewCuBoxSxth232M2_1 = hCuBoxSxth232M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxth232M2_1", dAdaptiveArrayM2);
  hnewCuBoxSxth232M2_10 = hCuBoxSxth232M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxth232M2_10", dAdaptiveArrayM2);
  hnewCuBoxSxu238M2_001 = hCuBoxSxu238M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxu238M2_001", dAdaptiveArrayM2);
  hnewCuBoxSxu238M2_01 = hCuBoxSxu238M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxu238M2_01", dAdaptiveArrayM2);
  hnewCuBoxSxu238M2_1 = hCuBoxSxu238M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxu238M2_1", dAdaptiveArrayM2);
  hnewCuBoxSxu238M2_10 = hCuBoxSxu238M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBoxSxu238M2_10", dAdaptiveArrayM2);


  hnewCuBoxco58M2Sum = hCuBoxco58M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxco58M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxco60M2Sum = hCuBoxco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxco60M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxcs137M2Sum = hCuBoxcs137M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxcs137M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxk40M2Sum = hCuBoxk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxk40M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxmn54M2Sum = hCuBoxmn54M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxmn54M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxpb210M2Sum = hCuBoxpb210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxpb210M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxth232M2Sum = hCuBoxth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxth232M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBoxu238M2Sum = hCuBoxu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxu238M2Sum", dAdaptiveArrayM2Sum);

  hnewCuBoxSth232M2Sum_1 = hCuBoxSth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBoxSu238M2Sum_1 = hCuBoxSu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBoxSxpb210M2Sum_001 = hCuBoxSxpb210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxpb210M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuBoxSxpb210M2Sum_01 = hCuBoxSxpb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxpb210M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBoxSxpb210M2Sum_1 = hCuBoxSxpb210M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxpb210M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBoxSxpb210M2Sum_10 = hCuBoxSxpb210M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxpb210M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuBoxSxth232M2Sum_001 = hCuBoxSxth232M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxth232M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuBoxSxth232M2Sum_01 = hCuBoxSxth232M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxth232M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBoxSxth232M2Sum_1 = hCuBoxSxth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBoxSxth232M2Sum_10 = hCuBoxSxth232M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxth232M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuBoxSxu238M2Sum_001 = hCuBoxSxu238M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxu238M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuBoxSxu238M2Sum_01 = hCuBoxSxu238M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxu238M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBoxSxu238M2Sum_1 = hCuBoxSxu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBoxSxu238M2Sum_10 = hCuBoxSxu238M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBoxSxu238M2Sum_10", dAdaptiveArrayM2Sum);


///////// CuBox + CuFrame M1 and M2
  hnewCuBox_CuFrameco60M1 = hCuBox_CuFrameco60M1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameco60M1", dAdaptiveArrayM1);
  hnewCuBox_CuFramek40M1 = hCuBox_CuFramek40M1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFramek40M1", dAdaptiveArrayM1);
  hnewCuBox_CuFrameth232M1 = hCuBox_CuFrameth232M1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameth232M1", dAdaptiveArrayM1);
  hnewCuBox_CuFrameu238M1 = hCuBox_CuFrameu238M1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameu238M1", dAdaptiveArrayM1);

  hnewCuBox_CuFrameth232M1_10 = hCuBox_CuFrameth232M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameth232M1_10", dAdaptiveArrayM1);
  hnewCuBox_CuFrameu238M1_10 = hCuBox_CuFrameu238M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameu238M1_10", dAdaptiveArrayM1);
  hnewCuBox_CuFramepb210M1_10 = hCuBox_CuFramepb210M1_10->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFramepb210M1_10", dAdaptiveArrayM1);
  hnewCuBox_CuFramepb210M1_1 = hCuBox_CuFramepb210M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFramepb210M1_1", dAdaptiveArrayM1);
  hnewCuBox_CuFramepb210M1_01 = hCuBox_CuFramepb210M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFramepb210M1_01", dAdaptiveArrayM1);
  hnewCuBox_CuFramepb210M1_001 = hCuBox_CuFramepb210M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFramepb210M1_001", dAdaptiveArrayM1);

  hnewCuBox_CuFrameth232M1_1 = hCuBox_CuFrameth232M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameth232M1_1", dAdaptiveArrayM1);
  hnewCuBox_CuFrameu238M1_1 = hCuBox_CuFrameu238M1_1->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameu238M1_1", dAdaptiveArrayM1);
  hnewCuBox_CuFrameth232M1_01 = hCuBox_CuFrameth232M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameth232M1_01", dAdaptiveArrayM1);
  hnewCuBox_CuFrameu238M1_01 = hCuBox_CuFrameu238M1_01->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameu238M1_01", dAdaptiveArrayM1);
  hnewCuBox_CuFrameth232M1_001 = hCuBox_CuFrameth232M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameth232M1_001", dAdaptiveArrayM1);
  hnewCuBox_CuFrameu238M1_001 = hCuBox_CuFrameu238M1_001->Rebin(dAdaptiveBinsM1, "hnewCuBox_CuFrameu238M1_001", dAdaptiveArrayM1);    

  hnewCuBox_CuFrameco60M2 = hCuBox_CuFrameco60M2->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameco60M2", dAdaptiveArrayM2);
  hnewCuBox_CuFramek40M2 = hCuBox_CuFramek40M2->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFramek40M2", dAdaptiveArrayM2);
  hnewCuBox_CuFrameth232M2 = hCuBox_CuFrameth232M2->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameth232M2", dAdaptiveArrayM2);
  hnewCuBox_CuFrameu238M2 = hCuBox_CuFrameu238M2->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameu238M2", dAdaptiveArrayM2);

  hnewCuBox_CuFrameth232M2_10 = hCuBox_CuFrameth232M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameth232M2_10", dAdaptiveArrayM2);
  hnewCuBox_CuFrameu238M2_10 = hCuBox_CuFrameu238M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameu238M2_10", dAdaptiveArrayM2);
  hnewCuBox_CuFramepb210M2_10 = hCuBox_CuFramepb210M2_10->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFramepb210M2_10", dAdaptiveArrayM2);
  hnewCuBox_CuFramepb210M2_1 = hCuBox_CuFramepb210M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFramepb210M2_1", dAdaptiveArrayM2);
  hnewCuBox_CuFramepb210M2_01 = hCuBox_CuFramepb210M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFramepb210M2_01", dAdaptiveArrayM2);
  hnewCuBox_CuFramepb210M2_001 = hCuBox_CuFramepb210M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFramepb210M2_001", dAdaptiveArrayM2);

  hnewCuBox_CuFrameth232M2_1 = hCuBox_CuFrameth232M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameth232M2_1", dAdaptiveArrayM2);
  hnewCuBox_CuFrameu238M2_1 = hCuBox_CuFrameu238M2_1->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameu238M2_1", dAdaptiveArrayM2);
  hnewCuBox_CuFrameth232M2_01 = hCuBox_CuFrameth232M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameth232M2_01", dAdaptiveArrayM2);
  hnewCuBox_CuFrameu238M2_01 = hCuBox_CuFrameu238M2_01->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameu238M2_01", dAdaptiveArrayM2);
  hnewCuBox_CuFrameth232M2_001 = hCuBox_CuFrameth232M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameth232M2_001", dAdaptiveArrayM2);
  hnewCuBox_CuFrameu238M2_001 = hCuBox_CuFrameu238M2_001->Rebin(dAdaptiveBinsM2, "hnewCuBox_CuFrameu238M2_001", dAdaptiveArrayM2);  

  hnewCuBox_CuFrameco60M2Sum = hCuBox_CuFrameco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameco60M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFramek40M2Sum = hCuBox_CuFramek40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFramek40M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameth232M2Sum = hCuBox_CuFrameth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameth232M2Sum", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameu238M2Sum = hCuBox_CuFrameu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameu238M2Sum", dAdaptiveArrayM2Sum);

  hnewCuBox_CuFrameth232M2Sum_10 = hCuBox_CuFrameth232M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameth232M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameu238M2Sum_10 = hCuBox_CuFrameu238M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameu238M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFramepb210M2Sum_10 = hCuBox_CuFramepb210M2Sum_10->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFramepb210M2Sum_10", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFramepb210M2Sum_1 = hCuBox_CuFramepb210M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFramepb210M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFramepb210M2Sum_01 = hCuBox_CuFramepb210M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFramepb210M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFramepb210M2Sum_001 = hCuBox_CuFramepb210M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFramepb210M2Sum_001", dAdaptiveArrayM2Sum);

  hnewCuBox_CuFrameth232M2Sum_1 = hCuBox_CuFrameth232M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameth232M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameu238M2Sum_1 = hCuBox_CuFrameu238M2Sum_1->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameu238M2Sum_1", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameth232M2Sum_01 = hCuBox_CuFrameth232M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameth232M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameu238M2Sum_01 = hCuBox_CuFrameu238M2Sum_01->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameu238M2Sum_01", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameth232M2Sum_001 = hCuBox_CuFrameth232M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameth232M2Sum_001", dAdaptiveArrayM2Sum);
  hnewCuBox_CuFrameu238M2Sum_001 = hCuBox_CuFrameu238M2Sum_001->Rebin(dAdaptiveBinsM2Sum, "hnewCuBox_CuFrameu238M2Sum_001", dAdaptiveArrayM2Sum);  

/*
////////// 50mK M1 and M2
  h50mKco58M1->Rebin(dAdaptiveBinsM1, "hnew50mKco58M1", dAdaptiveArrayM1);
  h50mKco60M1->Rebin(dAdaptiveBinsM1, "hnew50mKco60M1", dAdaptiveArrayM1);
  h50mKcs137M1->Rebin(dAdaptiveBinsM1, "hnew50mKcs137M1", dAdaptiveArrayM1);
  h50mKk40M1->Rebin(dAdaptiveBinsM1, "hnew50mKk40M1", dAdaptiveArrayM1);
  h50mKmn54M1->Rebin(dAdaptiveBinsM1, "hnew50mKmn54M1", dAdaptiveArrayM1);
  h50mKpb210M1->Rebin(dAdaptiveBinsM1, "hnew50mKpb210M1", dAdaptiveArrayM1);
  h50mKth232M1->Rebin(dAdaptiveBinsM1, "hnew50mKth232M1", dAdaptiveArrayM1);
  h50mKu238M1->Rebin(dAdaptiveBinsM1, "hnew50mKu238M1", dAdaptiveArrayM1);

  h50mKco58M2->Rebin(dAdaptiveBinsM2, "hnew50mKco58M2", dAdaptiveArrayM2);
  h50mKco60M2->Rebin(dAdaptiveBinsM2, "hnew50mKco60M2", dAdaptiveArrayM2);
  h50mKcs137M2->Rebin(dAdaptiveBinsM2, "hnew50mKcs137M2", dAdaptiveArrayM2);
  h50mKk40M2->Rebin(dAdaptiveBinsM2, "hnew50mKk40M2", dAdaptiveArrayM2);
  h50mKmn54M2->Rebin(dAdaptiveBinsM2, "hnew50mKmn54M2", dAdaptiveArrayM2);
  h50mKpb210M2->Rebin(dAdaptiveBinsM2, "hnew50mKpb210M2", dAdaptiveArrayM2);
  h50mKth232M2->Rebin(dAdaptiveBinsM2, "hnew50mKth232M2", dAdaptiveArrayM2);
  h50mKu238M2->Rebin(dAdaptiveBinsM2, "hnew50mKu238M2", dAdaptiveArrayM2);

  h50mKco58M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKco58M2Sum", dAdaptiveArrayM2Sum);
  h50mKco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKco60M2Sum", dAdaptiveArrayM2Sum);
  h50mKcs137M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKcs137M2Sum", dAdaptiveArrayM2Sum);
  h50mKk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKk40M2Sum", dAdaptiveArrayM2Sum);
  h50mKmn54M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKmn54M2Sum", dAdaptiveArrayM2Sum);
  h50mKpb210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKpb210M2Sum", dAdaptiveArrayM2Sum);
  h50mKth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKth232M2Sum", dAdaptiveArrayM2Sum);
  h50mKu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew50mKu238M2Sum", dAdaptiveArrayM2Sum);
*/
//////// 600mK
  hnew600mKco60M1 = h600mKco60M1->Rebin(dAdaptiveBinsM1, "hnew600mKco60M1", dAdaptiveArrayM1);
  hnew600mKk40M1 = h600mKk40M1->Rebin(dAdaptiveBinsM1, "hnew600mKk40M1", dAdaptiveArrayM1);
  hnew600mKth232M1 = h600mKth232M1->Rebin(dAdaptiveBinsM1, "hnew600mKth232M1", dAdaptiveArrayM1);
  hnew600mKu238M1 = h600mKu238M1->Rebin(dAdaptiveBinsM1, "hnew600mKu238M1", dAdaptiveArrayM1);

  hnew600mKco60M2 = h600mKco60M2->Rebin(dAdaptiveBinsM2, "hnew600mKco60M2", dAdaptiveArrayM2);
  hnew600mKk40M2 = h600mKk40M2->Rebin(dAdaptiveBinsM2, "hnew600mKk40M2", dAdaptiveArrayM2);
  hnew600mKth232M2 = h600mKth232M2->Rebin(dAdaptiveBinsM2, "hnew600mKth232M2", dAdaptiveArrayM2);
  hnew600mKu238M2 = h600mKu238M2->Rebin(dAdaptiveBinsM2, "hnew600mKu238M2", dAdaptiveArrayM2);

  hnew600mKco60M2Sum = h600mKco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew600mKco60M2Sum", dAdaptiveArrayM2Sum);
  hnew600mKk40M2Sum = h600mKk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew600mKk40M2Sum", dAdaptiveArrayM2Sum);
  hnew600mKth232M2Sum = h600mKth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew600mKth232M2Sum", dAdaptiveArrayM2Sum);
  hnew600mKu238M2Sum = h600mKu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnew600mKu238M2Sum", dAdaptiveArrayM2Sum);


///////// Roman Lead M1 and M2
  hnewPbRomco60M1 = hPbRomco60M1->Rebin(dAdaptiveBinsM1, "hnewPbRomco60M1", dAdaptiveArrayM1);
  hnewPbRomcs137M1 = hPbRomcs137M1->Rebin(dAdaptiveBinsM1, "hnewPbRomcs137M1", dAdaptiveArrayM1);
  hnewPbRomk40M1 = hPbRomk40M1->Rebin(dAdaptiveBinsM1, "hnewPbRomk40M1", dAdaptiveArrayM1);
  hnewPbRompb210M1 = hPbRompb210M1->Rebin(dAdaptiveBinsM1, "hnewPbRompb210M1", dAdaptiveArrayM1);
  hnewPbRomth232M1 = hPbRomth232M1->Rebin(dAdaptiveBinsM1, "hnewPbRomth232M1", dAdaptiveArrayM1);
  hnewPbRomu238M1 = hPbRomu238M1->Rebin(dAdaptiveBinsM1, "hnewPbRomu238M1", dAdaptiveArrayM1);

  hnewPbRomco60M2 = hPbRomco60M2->Rebin(dAdaptiveBinsM2, "hnewPbRomco60M2", dAdaptiveArrayM2);
  hnewPbRomcs137M2 = hPbRomcs137M2->Rebin(dAdaptiveBinsM2, "hnewPbRomcs137M2", dAdaptiveArrayM2);
  hnewPbRomk40M2 = hPbRomk40M2->Rebin(dAdaptiveBinsM2, "hnewPbRomk40M2", dAdaptiveArrayM2);
  hnewPbRompb210M2 = hPbRompb210M2->Rebin(dAdaptiveBinsM2, "hnewPbRompb210M2", dAdaptiveArrayM2);
  hnewPbRomth232M2 = hPbRomth232M2->Rebin(dAdaptiveBinsM2, "hnewPbRomth232M2", dAdaptiveArrayM2);
  hnewPbRomu238M2 = hPbRomu238M2->Rebin(dAdaptiveBinsM2, "hnewPbRomu238M2", dAdaptiveArrayM2);

  hnewPbRomco60M2Sum = hPbRomco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRomco60M2Sum", dAdaptiveArrayM2Sum);
  hnewPbRomcs137M2Sum = hPbRomcs137M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRomcs137M2Sum", dAdaptiveArrayM2Sum);
  hnewPbRomk40M2Sum = hPbRomk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRomk40M2Sum", dAdaptiveArrayM2Sum);
  hnewPbRompb210M2Sum = hPbRompb210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRompb210M2Sum", dAdaptiveArrayM2Sum);
  hnewPbRomth232M2Sum = hPbRomth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRomth232M2Sum", dAdaptiveArrayM2Sum);
  hnewPbRomu238M2Sum = hPbRomu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewPbRomu238M2Sum", dAdaptiveArrayM2Sum);

////////// Internal Shields
  hnewInternalco60M1 = hInternalco60M1->Rebin(dAdaptiveBinsM1, "hnewInternalco60M1", dAdaptiveArrayM1);
  hnewInternalk40M1 = hInternalk40M1->Rebin(dAdaptiveBinsM1, "hnewInternalk40M1", dAdaptiveArrayM1);
  hnewInternalth232M1 = hInternalth232M1->Rebin(dAdaptiveBinsM1, "hnewInternalth232M1", dAdaptiveArrayM1);
  hnewInternalu238M1 = hInternalu238M1->Rebin(dAdaptiveBinsM1, "hnewInternalu238M1", dAdaptiveArrayM1);

  hnewInternalco60M2 = hInternalco60M2->Rebin(dAdaptiveBinsM2, "hnewInternalco60M2", dAdaptiveArrayM2);
  hnewInternalk40M2 = hInternalk40M2->Rebin(dAdaptiveBinsM2, "hnewInternalk40M2", dAdaptiveArrayM2);
  hnewInternalth232M2 = hInternalth232M2->Rebin(dAdaptiveBinsM2, "hnewInternalth232M2", dAdaptiveArrayM2);
  hnewInternalu238M2 = hInternalu238M2->Rebin(dAdaptiveBinsM2, "hnewInternalu238M2", dAdaptiveArrayM2);

  hnewInternalco60M2Sum = hInternalco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewInternalco60M2Sum", dAdaptiveArrayM2Sum);
  hnewInternalk40M2Sum = hInternalk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewInternalk40M2Sum", dAdaptiveArrayM2Sum);
  hnewInternalth232M2Sum = hInternalth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewInternalth232M2Sum", dAdaptiveArrayM2Sum);
  hnewInternalu238M2Sum = hInternalu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewInternalu238M2Sum", dAdaptiveArrayM2Sum);

/*
////////// Main Bath M1 and M2
  hMBco60M1->Rebin(dAdaptiveBinsM1, "hnewMBco60M1", dAdaptiveArrayM1);
  hMBk40M1->Rebin(dAdaptiveBinsM1, "hnewMBk40M1", dAdaptiveArrayM1);
  hMBth232M1->Rebin(dAdaptiveBinsM1, "hnewMBth232M1", dAdaptiveArrayM1);
  hMBu238M1->Rebin(dAdaptiveBinsM1, "hnewMBu238M1", dAdaptiveArrayM1);

  hMBco60M2->Rebin(dAdaptiveBinsM2, "hnewMBco60M2", dAdaptiveArrayM2);
  hMBk40M2->Rebin(dAdaptiveBinsM2, "hnewMBk40M2", dAdaptiveArrayM2);
  hMBth232M2->Rebin(dAdaptiveBinsM2, "hnewMBth232M2", dAdaptiveArrayM2);
  hMBu238M2->Rebin(dAdaptiveBinsM2, "hnewMBu238M2", dAdaptiveArrayM2);

  hMBco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewMBco60M2Sum", dAdaptiveArrayM2Sum);
  hMBk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewMBk40M2Sum", dAdaptiveArrayM2Sum);
  hMBth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewMBth232M2Sum", dAdaptiveArrayM2Sum);
  hMBu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewMBu238M2Sum", dAdaptiveArrayM2Sum);

///////// Super Insulation M1 and M2
  hSIk40M1->Rebin(dAdaptiveBinsM1, "hnewSIk40M1", dAdaptiveArrayM1);
  hSIth232M1->Rebin(dAdaptiveBinsM1, "hnewSIth232M1", dAdaptiveArrayM1);
  hSIu238M1->Rebin(dAdaptiveBinsM1, "hnewSIu238M1", dAdaptiveArrayM1);

  hSIk40M2->Rebin(dAdaptiveBinsM2, "hnewSIk40M2", dAdaptiveArrayM2);
  hSIth232M2->Rebin(dAdaptiveBinsM2, "hnewSIth232M2", dAdaptiveArrayM2);
  hSIu238M2->Rebin(dAdaptiveBinsM2, "hnewSIu238M2", dAdaptiveArrayM2);

  hSIk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewSIk40M2Sum", dAdaptiveArrayM2Sum);
  hSIth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewSIth232M2Sum", dAdaptiveArrayM2Sum);
  hSIu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewSIu238M2Sum", dAdaptiveArrayM2Sum);

///////// IVC M1 and M2
  hIVCco60M1->Rebin(dAdaptiveBinsM1, "hnewIVCco60M1", dAdaptiveArrayM1);
  hIVCk40M1->Rebin(dAdaptiveBinsM1, "hnewIVCk40M1", dAdaptiveArrayM1);
  hIVCth232M1->Rebin(dAdaptiveBinsM1, "hnewIVCth232M1", dAdaptiveArrayM1);
  hIVCu238M1->Rebin(dAdaptiveBinsM1, "hnewIVCu238M1", dAdaptiveArrayM1);

  hIVCco60M2->Rebin(dAdaptiveBinsM2, "hnewIVCco60M2", dAdaptiveArrayM2);
  hIVCk40M2->Rebin(dAdaptiveBinsM2, "hnewIVCk40M2", dAdaptiveArrayM2);
  hIVCth232M2->Rebin(dAdaptiveBinsM2, "hnewIVCth232M2", dAdaptiveArrayM2);
  hIVCu238M2->Rebin(dAdaptiveBinsM2, "hnewIVCu238M2", dAdaptiveArrayM2);

  hIVCco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewIVCco60M2Sum", dAdaptiveArrayM2Sum);
  hIVCk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewIVCk40M2Sum", dAdaptiveArrayM2Sum);
  hIVCth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewIVCth232M2Sum", dAdaptiveArrayM2Sum);
  hIVCu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewIVCu238M2Sum", dAdaptiveArrayM2Sum);
*/

////////// OVC M1 and M2
  hnewOVCco60M1 = hOVCco60M1->Rebin(dAdaptiveBinsM1, "hnewOVCco60M1", dAdaptiveArrayM1);
  hnewOVCk40M1 = hOVCk40M1->Rebin(dAdaptiveBinsM1, "hnewOVCk40M1", dAdaptiveArrayM1);
  hnewOVCth232M1 = hOVCth232M1->Rebin(dAdaptiveBinsM1, "hnewOVCth232M1", dAdaptiveArrayM1);
  hnewOVCu238M1 = hOVCu238M1->Rebin(dAdaptiveBinsM1, "hnewOVCu238M1", dAdaptiveArrayM1);

  hnewOVCco60M2 = hOVCco60M2->Rebin(dAdaptiveBinsM2, "hnewOVCco60M2", dAdaptiveArrayM2);
  hnewOVCk40M2 = hOVCk40M2->Rebin(dAdaptiveBinsM2, "hnewOVCk40M2", dAdaptiveArrayM2);
  hnewOVCth232M2 = hOVCth232M2->Rebin(dAdaptiveBinsM2, "hnewOVCth232M2", dAdaptiveArrayM2);
  hnewOVCu238M2 = hOVCu238M2->Rebin(dAdaptiveBinsM2, "hnewOVCu238M2", dAdaptiveArrayM2);

  hnewOVCco60M2Sum = hOVCco60M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewOVCco60M2Sum", dAdaptiveArrayM2Sum);
  hnewOVCk40M2Sum = hOVCk40M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewOVCk40M2Sum", dAdaptiveArrayM2Sum);
  hnewOVCth232M2Sum = hOVCth232M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewOVCth232M2Sum", dAdaptiveArrayM2Sum);
  hnewOVCu238M2Sum = hOVCu238M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewOVCu238M2Sum", dAdaptiveArrayM2Sum);

  hnewExtPbbi210M1 = hExtPbbi210M1->Rebin(dAdaptiveBinsM1, "hnewExtPbbi210M1", dAdaptiveArrayM1);
  hnewExtPbbi210M2 = hExtPbbi210M2->Rebin(dAdaptiveBinsM2, "hnewExtPbbi210M2", dAdaptiveArrayM2);
  hnewExtPbbi210M2Sum = hExtPbbi210M2Sum->Rebin(dAdaptiveBinsM2Sum, "hnewExtPbbi210M2Sum", dAdaptiveArrayM2Sum);



  // Fill adaptive binning histograms
  for(int i = 1; i <= dAdaptiveBinsM1; i++)
  {
    hAdapTeO20nuM1->SetBinContent(i, hnewTeO20nuM1->GetBinContent(i)/hnewTeO20nuM1->GetBinWidth(i));
    hAdapTeO22nuM1->SetBinContent(i, hnewTeO22nuM1->GetBinContent(i)/hnewTeO22nuM1->GetBinWidth(i));
    hAdapTeO2co60M1->SetBinContent(i, hnewTeO2co60M1->GetBinContent(i)/hnewTeO2co60M1->GetBinWidth(i));
    hAdapTeO2k40M1->SetBinContent(i, hnewTeO2k40M1->GetBinContent(i)/hnewTeO2k40M1->GetBinWidth(i));
    hAdapTeO2pb210M1->SetBinContent(i, hnewTeO2pb210M1->GetBinContent(i)/hnewTeO2pb210M1->GetBinWidth(i));
    hAdapTeO2po210M1->SetBinContent(i, hnewTeO2po210M1->GetBinContent(i)/hnewTeO2po210M1->GetBinWidth(i));
    hAdapTeO2te125M1->SetBinContent(i, hnewTeO2te125M1->GetBinContent(i)/hnewTeO2te125M1->GetBinWidth(i));
    hAdapTeO2th232M1->SetBinContent(i, hnewTeO2th232M1->GetBinContent(i)/hnewTeO2th232M1->GetBinWidth(i));
    // hAdapTeO2th228M1->SetBinContent(i, hnewTeO2th228M1->GetBinContent(i)/hnewTeO2th228M1->GetBinWidth(i));
    // hAdapTeO2ra226M1->SetBinContent(i, hnewTeO2ra226M1->GetBinContent(i)/hnewTeO2ra226M1->GetBinWidth(i));
    // hAdapTeO2rn222M1->SetBinContent(i, hnewTeO2rn222M1->GetBinContent(i)/hnewTeO2rn222M1->GetBinWidth(i));
    hAdapTeO2u238M1->SetBinContent(i, hnewTeO2u238M1->GetBinContent(i)/hnewTeO2u238M1->GetBinWidth(i));
    // hAdapTeO2th230M1->SetBinContent(i, hnewTeO2th230M1->GetBinContent(i)/hnewTeO2th230M1->GetBinWidth(i));
    // hAdapTeO2u234M1->SetBinContent(i, hnewTeO2u234M1->GetBinContent(i)/hnewTeO2u234M1->GetBinWidth(i));

    hAdapTeO2th232onlyM1->SetBinContent(i, hnewTeO2th232onlyM1->GetBinContent(i)/hnewTeO2th232onlyM1->GetBinWidth(i));
    hAdapTeO2ra228pb208M1->SetBinContent(i, hnewTeO2ra228pb208M1->GetBinContent(i)/hnewTeO2ra228pb208M1->GetBinWidth(i));
    hAdapTeO2th230onlyM1->SetBinContent(i, hnewTeO2th230onlyM1->GetBinContent(i)/hnewTeO2th230onlyM1->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM1_001->SetBinContent(i, hnewTeO2Sxth232onlyM1_001->GetBinContent(i)/hnewTeO2Sxth232onlyM1_001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M1_001->SetBinContent(i, hnewTeO2Sxra228pb208M1_001->GetBinContent(i)/hnewTeO2Sxra228pb208M1_001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M1_001->SetBinContent(i, hnewTeO2Sxu238th230M1_001->GetBinContent(i)/hnewTeO2Sxu238th230M1_001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM1_001->SetBinContent(i, hnewTeO2Sxth230onlyM1_001->GetBinContent(i)/hnewTeO2Sxth230onlyM1_001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M1_001->SetBinContent(i, hnewTeO2Sxra226pb210M1_001->GetBinContent(i)/hnewTeO2Sxra226pb210M1_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_0001->SetBinContent(i, hnewTeO2Sxpb210M1_0001->GetBinContent(i)/hnewTeO2Sxpb210M1_0001->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM1_01->SetBinContent(i, hnewTeO2Sxth232onlyM1_01->GetBinContent(i)/hnewTeO2Sxth232onlyM1_01->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M1_01->SetBinContent(i, hnewTeO2Sxra228pb208M1_01->GetBinContent(i)/hnewTeO2Sxra228pb208M1_01->GetBinWidth(i));
    hAdapTeO2Sxu238th230M1_01->SetBinContent(i, hnewTeO2Sxu238th230M1_01->GetBinContent(i)/hnewTeO2Sxu238th230M1_01->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM1_01->SetBinContent(i, hnewTeO2Sxth230onlyM1_01->GetBinContent(i)/hnewTeO2Sxth230onlyM1_01->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M1_01->SetBinContent(i, hnewTeO2Sxra226pb210M1_01->GetBinContent(i)/hnewTeO2Sxra226pb210M1_01->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM1_0001->SetBinContent(i, hnewTeO2Sxth232onlyM1_0001->GetBinContent(i)/hnewTeO2Sxth232onlyM1_0001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M1_0001->SetBinContent(i, hnewTeO2Sxra228pb208M1_0001->GetBinContent(i)/hnewTeO2Sxra228pb208M1_0001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M1_0001->SetBinContent(i, hnewTeO2Sxu238th230M1_0001->GetBinContent(i)/hnewTeO2Sxu238th230M1_0001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM1_0001->SetBinContent(i, hnewTeO2Sxth230onlyM1_0001->GetBinContent(i)/hnewTeO2Sxth230onlyM1_0001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M1_0001->SetBinContent(i, hnewTeO2Sxra226pb210M1_0001->GetBinContent(i)/hnewTeO2Sxra226pb210M1_0001->GetBinWidth(i));

    // hAdapTeO2Spb210M1_01->SetBinContent(i, hnewTeO2Spb210M1_01->GetBinContent(i)/hnewTeO2Spb210M1_01->GetBinWidth(i));
    // hAdapTeO2Spo210M1_001->SetBinContent(i, hnewTeO2Spo210M1_001->GetBinContent(i)/hnewTeO2Spo210M1_001->GetBinWidth(i));
    // hAdapTeO2Spo210M1_01->SetBinContent(i, hnewTeO2Spo210M1_01->GetBinContent(i)/hnewTeO2Spo210M1_01->GetBinWidth(i));
    // hAdapTeO2Sth232M1_01->SetBinContent(i, hnewTeO2Sth232M1_01->GetBinContent(i)/hnewTeO2Sth232M1_01->GetBinWidth(i));
    // hAdapTeO2Su238M1_01->SetBinContent(i, hnewTeO2Su238M1_01->GetBinContent(i)/hnewTeO2Su238M1_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_001->SetBinContent(i, hnewTeO2Sxpb210M1_001->GetBinContent(i)/hnewTeO2Sxpb210M1_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_01->SetBinContent(i, hnewTeO2Sxpb210M1_01->GetBinContent(i)/hnewTeO2Sxpb210M1_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_1->SetBinContent(i, hnewTeO2Sxpb210M1_1->GetBinContent(i)/hnewTeO2Sxpb210M1_1->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_10->SetBinContent(i, hnewTeO2Sxpb210M1_10->GetBinContent(i)/hnewTeO2Sxpb210M1_10->GetBinWidth(i));
    hAdapTeO2Sxpo210M1_001->SetBinContent(i, hnewTeO2Sxpo210M1_001->GetBinContent(i)/hnewTeO2Sxpo210M1_001->GetBinWidth(i));
    hAdapTeO2Sxpo210M1_01->SetBinContent(i, hnewTeO2Sxpo210M1_01->GetBinContent(i)/hnewTeO2Sxpo210M1_01->GetBinWidth(i));
    hAdapTeO2Sxpo210M1_1->SetBinContent(i, hnewTeO2Sxpo210M1_1->GetBinContent(i)/hnewTeO2Sxpo210M1_1->GetBinWidth(i));
    hAdapTeO2Sxth232M1_001->SetBinContent(i, hnewTeO2Sxth232M1_001->GetBinContent(i)/hnewTeO2Sxth232M1_001->GetBinWidth(i));
    hAdapTeO2Sxth232M1_01->SetBinContent(i, hnewTeO2Sxth232M1_01->GetBinContent(i)/hnewTeO2Sxth232M1_01->GetBinWidth(i));
    hAdapTeO2Sxth232M1_1->SetBinContent(i, hnewTeO2Sxth232M1_1->GetBinContent(i)/hnewTeO2Sxth232M1_1->GetBinWidth(i));
    hAdapTeO2Sxth232M1_10->SetBinContent(i, hnewTeO2Sxth232M1_10->GetBinContent(i)/hnewTeO2Sxth232M1_10->GetBinWidth(i));
    hAdapTeO2Sxu238M1_001->SetBinContent(i, hnewTeO2Sxu238M1_001->GetBinContent(i)/hnewTeO2Sxu238M1_001->GetBinWidth(i));
    hAdapTeO2Sxu238M1_01->SetBinContent(i, hnewTeO2Sxu238M1_01->GetBinContent(i)/hnewTeO2Sxu238M1_01->GetBinWidth(i));
    hAdapTeO2Sxu238M1_1->SetBinContent(i, hnewTeO2Sxu238M1_1->GetBinContent(i)/hnewTeO2Sxu238M1_1->GetBinWidth(i));
    hAdapTeO2Sxu238M1_10->SetBinContent(i, hnewTeO2Sxu238M1_10->GetBinContent(i)/hnewTeO2Sxu238M1_10->GetBinWidth(i));

    hAdapTeO2Sxu238M1_100->SetBinContent(i, hnewTeO2Sxu238M1_100->GetBinContent(i)/hnewTeO2Sxu238M1_100->GetBinWidth(i));
    hAdapTeO2Sxth232M1_100->SetBinContent(i, hnewTeO2Sxth232M1_100->GetBinContent(i)/hnewTeO2Sxth232M1_100->GetBinWidth(i));
    hAdapTeO2Sxpb210M1_100->SetBinContent(i, hnewTeO2Sxpb210M1_100->GetBinContent(i)/hnewTeO2Sxpb210M1_100->GetBinWidth(i));


    hAdapCuFrameco58M1->SetBinContent(i, hnewCuFrameco58M1->GetBinContent(i)/hnewCuFrameco58M1->GetBinWidth(i));
    hAdapCuFrameco60M1->SetBinContent(i, hnewCuFrameco60M1->GetBinContent(i)/hnewCuFrameco60M1->GetBinWidth(i));
    hAdapCuFramecs137M1->SetBinContent(i, hnewCuFramecs137M1->GetBinContent(i)/hnewCuFramecs137M1->GetBinWidth(i));
    hAdapCuFramek40M1->SetBinContent(i, hnewCuFramek40M1->GetBinContent(i)/hnewCuFramek40M1->GetBinWidth(i));
    hAdapCuFramemn54M1->SetBinContent(i, hnewCuFramemn54M1->GetBinContent(i)/hnewCuFramemn54M1->GetBinWidth(i));
    hAdapCuFramepb210M1->SetBinContent(i, hnewCuFramepb210M1->GetBinContent(i)/hnewCuFramepb210M1->GetBinWidth(i));
    hAdapCuFrameth232M1->SetBinContent(i, hnewCuFrameth232M1->GetBinContent(i)/hnewCuFrameth232M1->GetBinWidth(i));
    hAdapCuFrameu238M1->SetBinContent(i, hnewCuFrameu238M1->GetBinContent(i)/hnewCuFrameu238M1->GetBinWidth(i));

    // hAdapCuFrameSth232M1_1->SetBinContent(i, hnewCuFrameSth232M1_1->GetBinContent(i)/hnewCuFrameSth232M1_1->GetBinWidth(i));
    // hAdapCuFrameSu238M1_1->SetBinContent(i, hnewCuFrameSu238M1_1->GetBinContent(i)/hnewCuFrameSu238M1_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M1_001->SetBinContent(i, hnewCuFrameSxpb210M1_001->GetBinContent(i)/hnewCuFrameSxpb210M1_001->GetBinWidth(i));
    hAdapCuFrameSxpb210M1_01->SetBinContent(i, hnewCuFrameSxpb210M1_01->GetBinContent(i)/hnewCuFrameSxpb210M1_01->GetBinWidth(i));
    hAdapCuFrameSxpb210M1_1->SetBinContent(i, hnewCuFrameSxpb210M1_1->GetBinContent(i)/hnewCuFrameSxpb210M1_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M1_10->SetBinContent(i, hnewCuFrameSxpb210M1_10->GetBinContent(i)/hnewCuFrameSxpb210M1_10->GetBinWidth(i));
    hAdapCuFrameSxth232M1_001->SetBinContent(i, hnewCuFrameSxth232M1_001->GetBinContent(i)/hnewCuFrameSxth232M1_001->GetBinWidth(i));
    hAdapCuFrameSxth232M1_01->SetBinContent(i, hnewCuFrameSxth232M1_01->GetBinContent(i)/hnewCuFrameSxth232M1_01->GetBinWidth(i));
    hAdapCuFrameSxth232M1_1->SetBinContent(i, hnewCuFrameSxth232M1_1->GetBinContent(i)/hnewCuFrameSxth232M1_1->GetBinWidth(i));
    hAdapCuFrameSxth232M1_10->SetBinContent(i, hnewCuFrameSxth232M1_10->GetBinContent(i)/hnewCuFrameSxth232M1_10->GetBinWidth(i));
    hAdapCuFrameSxu238M1_001->SetBinContent(i, hnewCuFrameSxu238M1_001->GetBinContent(i)/hnewCuFrameSxu238M1_001->GetBinWidth(i));
    hAdapCuFrameSxu238M1_01->SetBinContent(i, hnewCuFrameSxu238M1_01->GetBinContent(i)/hnewCuFrameSxu238M1_01->GetBinWidth(i));
    hAdapCuFrameSxu238M1_1->SetBinContent(i, hnewCuFrameSxu238M1_1->GetBinContent(i)/hnewCuFrameSxu238M1_1->GetBinWidth(i));
    hAdapCuFrameSxu238M1_10->SetBinContent(i, hnewCuFrameSxu238M1_10->GetBinContent(i)/hnewCuFrameSxu238M1_10->GetBinWidth(i));

    hAdapCuBoxco58M1->SetBinContent(i, hnewCuBoxco58M1->GetBinContent(i)/hnewCuBoxco58M1->GetBinWidth(i));
    hAdapCuBoxco60M1->SetBinContent(i, hnewCuBoxco60M1->GetBinContent(i)/hnewCuBoxco60M1->GetBinWidth(i));
    hAdapCuBoxcs137M1->SetBinContent(i, hnewCuBoxcs137M1->GetBinContent(i)/hnewCuBoxcs137M1->GetBinWidth(i));
    hAdapCuBoxk40M1->SetBinContent(i, hnewCuBoxk40M1->GetBinContent(i)/hnewCuBoxk40M1->GetBinWidth(i));
    hAdapCuBoxmn54M1->SetBinContent(i, hnewCuBoxmn54M1->GetBinContent(i)/hnewCuBoxmn54M1->GetBinWidth(i));
    hAdapCuBoxpb210M1->SetBinContent(i, hnewCuBoxpb210M1->GetBinContent(i)/hnewCuBoxpb210M1->GetBinWidth(i));
    hAdapCuBoxth232M1->SetBinContent(i, hnewCuBoxth232M1->GetBinContent(i)/hnewCuBoxth232M1->GetBinWidth(i));
    hAdapCuBoxu238M1->SetBinContent(i, hnewCuBoxu238M1->GetBinContent(i)/hnewCuBoxu238M1->GetBinWidth(i));

    // hAdapCuBoxSth232M1_1->SetBinContent(i, hnewCuBoxSth232M1_1->GetBinContent(i)/hnewCuBoxSth232M1_1->GetBinWidth(i));
    // hAdapCuBoxSu238M1_1->SetBinContent(i, hnewCuBoxSu238M1_1->GetBinContent(i)/hnewCuBoxSu238M1_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M1_001->SetBinContent(i, hnewCuBoxSxpb210M1_001->GetBinContent(i)/hnewCuBoxSxpb210M1_001->GetBinWidth(i));
    hAdapCuBoxSxpb210M1_01->SetBinContent(i, hnewCuBoxSxpb210M1_01->GetBinContent(i)/hnewCuBoxSxpb210M1_01->GetBinWidth(i));
    hAdapCuBoxSxpb210M1_1->SetBinContent(i, hnewCuBoxSxpb210M1_1->GetBinContent(i)/hnewCuBoxSxpb210M1_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M1_10->SetBinContent(i, hnewCuBoxSxpb210M1_10->GetBinContent(i)/hnewCuBoxSxpb210M1_10->GetBinWidth(i));
    hAdapCuBoxSxth232M1_001->SetBinContent(i, hnewCuBoxSxth232M1_001->GetBinContent(i)/hnewCuBoxSxth232M1_001->GetBinWidth(i));
    hAdapCuBoxSxth232M1_01->SetBinContent(i, hnewCuBoxSxth232M1_01->GetBinContent(i)/hnewCuBoxSxth232M1_01->GetBinWidth(i));
    hAdapCuBoxSxth232M1_1->SetBinContent(i, hnewCuBoxSxth232M1_1->GetBinContent(i)/hnewCuBoxSxth232M1_1->GetBinWidth(i));
    hAdapCuBoxSxth232M1_10->SetBinContent(i, hnewCuBoxSxth232M1_10->GetBinContent(i)/hnewCuBoxSxth232M1_10->GetBinWidth(i));
    hAdapCuBoxSxu238M1_001->SetBinContent(i, hnewCuBoxSxu238M1_001->GetBinContent(i)/hnewCuBoxSxu238M1_001->GetBinWidth(i));
    hAdapCuBoxSxu238M1_01->SetBinContent(i, hnewCuBoxSxu238M1_01->GetBinContent(i)/hnewCuBoxSxu238M1_01->GetBinWidth(i));
    hAdapCuBoxSxu238M1_1->SetBinContent(i, hnewCuBoxSxu238M1_1->GetBinContent(i)/hnewCuBoxSxu238M1_1->GetBinWidth(i));
    hAdapCuBoxSxu238M1_10->SetBinContent(i, hnewCuBoxSxu238M1_10->GetBinContent(i)/hnewCuBoxSxu238M1_10->GetBinWidth(i));

    hAdapCuBox_CuFrameco60M1->SetBinContent(i, hnewCuBox_CuFrameco60M1->GetBinContent(i)/hnewCuBox_CuFrameco60M1->GetBinWidth(i));
    hAdapCuBox_CuFramek40M1->SetBinContent(i, hnewCuBox_CuFramek40M1->GetBinContent(i)/hnewCuBox_CuFramek40M1->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M1->SetBinContent(i, hnewCuBox_CuFrameth232M1->GetBinContent(i)/hnewCuBox_CuFrameth232M1->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M1->SetBinContent(i, hnewCuBox_CuFrameu238M1->GetBinContent(i)/hnewCuBox_CuFrameu238M1->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M1_10->SetBinContent(i, hnewCuBox_CuFrameth232M1_10->GetBinContent(i)/hnewCuBox_CuFrameth232M1_10->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M1_10->SetBinContent(i, hnewCuBox_CuFrameu238M1_10->GetBinContent(i)/hnewCuBox_CuFrameu238M1_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M1_10->SetBinContent(i, hnewCuBox_CuFramepb210M1_10->GetBinContent(i)/hnewCuBox_CuFramepb210M1_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M1_1->SetBinContent(i, hnewCuBox_CuFramepb210M1_1->GetBinContent(i)/hnewCuBox_CuFramepb210M1_1->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M1_01->SetBinContent(i, hnewCuBox_CuFramepb210M1_01->GetBinContent(i)/hnewCuBox_CuFramepb210M1_01->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M1_001->SetBinContent(i, hnewCuBox_CuFramepb210M1_001->GetBinContent(i)/hnewCuBox_CuFramepb210M1_001->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M1_1->SetBinContent(i, hnewCuBox_CuFrameth232M1_1->GetBinContent(i)/hnewCuBox_CuFrameth232M1_1->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M1_1->SetBinContent(i, hnewCuBox_CuFrameu238M1_1->GetBinContent(i)/hnewCuBox_CuFrameu238M1_1->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M1_01->SetBinContent(i, hnewCuBox_CuFrameth232M1_01->GetBinContent(i)/hnewCuBox_CuFrameth232M1_01->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M1_01->SetBinContent(i, hnewCuBox_CuFrameu238M1_01->GetBinContent(i)/hnewCuBox_CuFrameu238M1_01->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M1_001->SetBinContent(i, hnewCuBox_CuFrameth232M1_001->GetBinContent(i)/hnewCuBox_CuFrameth232M1_001->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M1_001->SetBinContent(i, hnewCuBox_CuFrameu238M1_001->GetBinContent(i)/hnewCuBox_CuFrameu238M1_001->GetBinWidth(i));        

/*
    hAdap50mKco58M1->SetBinContent(i, hnew50mKco58M1->GetBinContent(i)/hnew50mKco58M1->GetBinWidth(i));
    hAdap50mKco60M1->SetBinContent(i, hnew50mKco60M1->GetBinContent(i)/hnew50mKco60M1->GetBinWidth(i));
    hAdap50mKcs137M1->SetBinContent(i, hnew50mKcs137M1->GetBinContent(i)/hnew50mKcs137M1->GetBinWidth(i));
    hAdap50mKk40M1->SetBinContent(i, hnew50mKk40M1->GetBinContent(i)/hnew50mKk40M1->GetBinWidth(i));
    hAdap50mKmn54M1->SetBinContent(i, hnew50mKmn54M1->GetBinContent(i)/hnew50mKmn54M1->GetBinWidth(i));
    hAdap50mKpb210M1->SetBinContent(i, hnew50mKpb210M1->GetBinContent(i)/hnew50mKpb210M1->GetBinWidth(i));
    hAdap50mKth232M1->SetBinContent(i, hnew50mKth232M1->GetBinContent(i)/hnew50mKth232M1->GetBinWidth(i));
    hAdap50mKu238M1->SetBinContent(i, hnew50mKu238M1->GetBinContent(i)/hnew50mKu238M1->GetBinWidth(i));

    hAdap600mKco60M1->SetBinContent(i, hnew600mKco60M1->GetBinContent(i)/hnew600mKco60M1->GetBinWidth(i));
    hAdap600mKk40M1->SetBinContent(i, hnew600mKk40M1->GetBinContent(i)/hnew600mKk40M1->GetBinWidth(i));
    hAdap600mKth232M1->SetBinContent(i, hnew600mKth232M1->GetBinContent(i)/hnew600mKth232M1->GetBinWidth(i));
    hAdap600mKu238M1->SetBinContent(i, hnew600mKu238M1->GetBinContent(i)/hnew600mKu238M1->GetBinWidth(i));
*/

    // hAdapPbRombi207M1->SetBinContent(i, hnewPbRombi207M1->GetBinContent(i)/hnewPbRombi207M1->GetBinWidth(i));
    hAdapPbRomco60M1->SetBinContent(i, hnewPbRomco60M1->GetBinContent(i)/hnewPbRomco60M1->GetBinWidth(i));
    hAdapPbRomcs137M1->SetBinContent(i, hnewPbRomcs137M1->GetBinContent(i)/hnewPbRomcs137M1->GetBinWidth(i));
    hAdapPbRomk40M1->SetBinContent(i, hnewPbRomk40M1->GetBinContent(i)/hnewPbRomk40M1->GetBinWidth(i));
    hAdapPbRompb210M1->SetBinContent(i, hnewPbRompb210M1->GetBinContent(i)/hnewPbRompb210M1->GetBinWidth(i));
    hAdapPbRomth232M1->SetBinContent(i, hnewPbRomth232M1->GetBinContent(i)/hnewPbRomth232M1->GetBinWidth(i));
    hAdapPbRomu238M1->SetBinContent(i, hnewPbRomu238M1->GetBinContent(i)/hnewPbRomu238M1->GetBinWidth(i));

    hAdapInternalco60M1->SetBinContent(i, hnewInternalco60M1->GetBinContent(i)/hnewInternalco60M1->GetBinWidth(i));
    hAdapInternalk40M1->SetBinContent(i, hnewInternalk40M1->GetBinContent(i)/hnewInternalk40M1->GetBinWidth(i));
    hAdapInternalth232M1->SetBinContent(i, hnewInternalth232M1->GetBinContent(i)/hnewInternalth232M1->GetBinWidth(i));
    hAdapInternalu238M1->SetBinContent(i, hnewInternalu238M1->GetBinContent(i)/hnewInternalu238M1->GetBinWidth(i));

/*
    hAdapMBco60M1->SetBinContent(i, hnewMBco60M1->GetBinContent(i)/hnewMBco60M1->GetBinWidth(i));
    hAdapMBk40M1->SetBinContent(i, hnewMBk40M1->GetBinContent(i)/hnewMBk40M1->GetBinWidth(i));
    hAdapMBth232M1->SetBinContent(i, hnewMBth232M1->GetBinContent(i)/hnewMBth232M1->GetBinWidth(i));
    hAdapMBu238M1->SetBinContent(i, hnewMBu238M1->GetBinContent(i)/hnewMBu238M1->GetBinWidth(i));

    hAdapSIk40M1->SetBinContent(i, hnewSIk40M1->GetBinContent(i)/hnewSIk40M1->GetBinWidth(i));
    hAdapSIth232M1->SetBinContent(i, hnewSIth232M1->GetBinContent(i)/hnewSIth232M1->GetBinWidth(i));
    hAdapSIu238M1->SetBinContent(i, hnewSIu238M1->GetBinContent(i)/hnewSIu238M1->GetBinWidth(i));

    hAdapIVCco60M1->SetBinContent(i, hnewIVCco60M1->GetBinContent(i)/hnewIVCco60M1->GetBinWidth(i));
    hAdapIVCk40M1->SetBinContent(i, hnewIVCk40M1->GetBinContent(i)/hnewIVCk40M1->GetBinWidth(i));
    hAdapIVCth232M1->SetBinContent(i, hnewIVCth232M1->GetBinContent(i)/hnewIVCth232M1->GetBinWidth(i));
    hAdapIVCu238M1->SetBinContent(i, hnewIVCu238M1->GetBinContent(i)/hnewIVCu238M1->GetBinWidth(i));
*/
    hAdapOVCco60M1->SetBinContent(i, hnewOVCco60M1->GetBinContent(i)/hnewOVCco60M1->GetBinWidth(i));
    hAdapOVCk40M1->SetBinContent(i, hnewOVCk40M1->GetBinContent(i)/hnewOVCk40M1->GetBinWidth(i));
    hAdapOVCth232M1->SetBinContent(i, hnewOVCth232M1->GetBinContent(i)/hnewOVCth232M1->GetBinWidth(i));
    hAdapOVCu238M1->SetBinContent(i, hnewOVCu238M1->GetBinContent(i)/hnewOVCu238M1->GetBinWidth(i));

    hAdapExtPbbi210M1->SetBinContent(i, hnewExtPbbi210M1->GetBinContent(i)/hnewExtPbbi210M1->GetBinWidth(i));
  }

  for(int i = 1; i <= dAdaptiveBinsM2; i++)
  {
    hAdapTeO20nuM2->SetBinContent(i, hnewTeO20nuM2->GetBinContent(i)/hnewTeO20nuM2->GetBinWidth(i));
    hAdapTeO22nuM2->SetBinContent(i, hnewTeO22nuM2->GetBinContent(i)/hnewTeO22nuM2->GetBinWidth(i));
    hAdapTeO2co60M2->SetBinContent(i, hnewTeO2co60M2->GetBinContent(i)/hnewTeO2co60M2->GetBinWidth(i));
    hAdapTeO2k40M2->SetBinContent(i, hnewTeO2k40M2->GetBinContent(i)/hnewTeO2k40M2->GetBinWidth(i));
    hAdapTeO2pb210M2->SetBinContent(i, hnewTeO2pb210M2->GetBinContent(i)/hnewTeO2pb210M2->GetBinWidth(i));
    hAdapTeO2po210M2->SetBinContent(i, hnewTeO2po210M2->GetBinContent(i)/hnewTeO2po210M2->GetBinWidth(i));
    hAdapTeO2te125M2->SetBinContent(i, hnewTeO2te125M2->GetBinContent(i)/hnewTeO2te125M2->GetBinWidth(i));
    hAdapTeO2th232M2->SetBinContent(i, hnewTeO2th232M2->GetBinContent(i)/hnewTeO2th232M2->GetBinWidth(i));
    // hAdapTeO2th228M2->SetBinContent(i, hnewTeO2th228M2->GetBinContent(i)/hnewTeO2th228M2->GetBinWidth(i));
    // hAdapTeO2ra226M2->SetBinContent(i, hnewTeO2ra226M2->GetBinContent(i)/hnewTeO2ra226M2->GetBinWidth(i));
    // hAdapTeO2rn222M2->SetBinContent(i, hnewTeO2rn222M2->GetBinContent(i)/hnewTeO2rn222M2->GetBinWidth(i));
    hAdapTeO2u238M2->SetBinContent(i, hnewTeO2u238M2->GetBinContent(i)/hnewTeO2u238M2->GetBinWidth(i));
    // hAdapTeO2th230M2->SetBinContent(i, hnewTeO2th230M2->GetBinContent(i)/hnewTeO2th230M2->GetBinWidth(i));
    // hAdapTeO2u234M2->SetBinContent(i, hnewTeO2u234M2->GetBinContent(i)/hnewTeO2u234M2->GetBinWidth(i));

    // hAdapTeO2Spb210M2_01->SetBinContent(i, hnewTeO2Spb210M2_01->GetBinContent(i)/hnewTeO2Spb210M2_01->GetBinWidth(i));
    // hAdapTeO2Spo210M2_001->SetBinContent(i, hnewTeO2Spo210M2_001->GetBinContent(i)/hnewTeO2Spo210M2_001->GetBinWidth(i));
    // hAdapTeO2Spo210M2_01->SetBinContent(i, hnewTeO2Spo210M2_01->GetBinContent(i)/hnewTeO2Spo210M2_01->GetBinWidth(i));
    // hAdapTeO2Sth232M2_01->SetBinContent(i, hnewTeO2Sth232M2_01->GetBinContent(i)/hnewTeO2Sth232M2_01->GetBinWidth(i));
    // hAdapTeO2Su238M2_01->SetBinContent(i, hnewTeO2Su238M2_01->GetBinContent(i)/hnewTeO2Su238M2_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_001->SetBinContent(i, hnewTeO2Sxpb210M2_001->GetBinContent(i)/hnewTeO2Sxpb210M2_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_01->SetBinContent(i, hnewTeO2Sxpb210M2_01->GetBinContent(i)/hnewTeO2Sxpb210M2_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_1->SetBinContent(i, hnewTeO2Sxpb210M2_1->GetBinContent(i)/hnewTeO2Sxpb210M2_1->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_10->SetBinContent(i, hnewTeO2Sxpb210M2_10->GetBinContent(i)/hnewTeO2Sxpb210M2_10->GetBinWidth(i));
    hAdapTeO2Sxpo210M2_001->SetBinContent(i, hnewTeO2Sxpo210M2_001->GetBinContent(i)/hnewTeO2Sxpo210M2_001->GetBinWidth(i));
    hAdapTeO2Sxpo210M2_01->SetBinContent(i, hnewTeO2Sxpo210M2_01->GetBinContent(i)/hnewTeO2Sxpo210M2_01->GetBinWidth(i));
    hAdapTeO2Sxpo210M2_1->SetBinContent(i, hnewTeO2Sxpo210M2_1->GetBinContent(i)/hnewTeO2Sxpo210M2_1->GetBinWidth(i));
    hAdapTeO2Sxth232M2_001->SetBinContent(i, hnewTeO2Sxth232M2_001->GetBinContent(i)/hnewTeO2Sxth232M2_001->GetBinWidth(i));
    hAdapTeO2Sxth232M2_01->SetBinContent(i, hnewTeO2Sxth232M2_01->GetBinContent(i)/hnewTeO2Sxth232M2_01->GetBinWidth(i));
    hAdapTeO2Sxth232M2_1->SetBinContent(i, hnewTeO2Sxth232M2_1->GetBinContent(i)/hnewTeO2Sxth232M2_1->GetBinWidth(i));
    hAdapTeO2Sxth232M2_10->SetBinContent(i, hnewTeO2Sxth232M2_10->GetBinContent(i)/hnewTeO2Sxth232M2_10->GetBinWidth(i));
    hAdapTeO2Sxu238M2_001->SetBinContent(i, hnewTeO2Sxu238M2_001->GetBinContent(i)/hnewTeO2Sxu238M2_001->GetBinWidth(i));
    hAdapTeO2Sxu238M2_01->SetBinContent(i, hnewTeO2Sxu238M2_01->GetBinContent(i)/hnewTeO2Sxu238M2_01->GetBinWidth(i));
    hAdapTeO2Sxu238M2_1->SetBinContent(i, hnewTeO2Sxu238M2_1->GetBinContent(i)/hnewTeO2Sxu238M2_1->GetBinWidth(i));
    hAdapTeO2Sxu238M2_10->SetBinContent(i, hnewTeO2Sxu238M2_10->GetBinContent(i)/hnewTeO2Sxu238M2_10->GetBinWidth(i));

    hAdapTeO2Sxu238M2_100->SetBinContent(i, hnewTeO2Sxu238M2_100->GetBinContent(i)/hnewTeO2Sxu238M2_100->GetBinWidth(i));
    hAdapTeO2Sxth232M2_100->SetBinContent(i, hnewTeO2Sxth232M2_100->GetBinContent(i)/hnewTeO2Sxth232M2_100->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_100->SetBinContent(i, hnewTeO2Sxpb210M2_100->GetBinContent(i)/hnewTeO2Sxpb210M2_100->GetBinWidth(i));

    hAdapTeO2th232onlyM2->SetBinContent(i, hnewTeO2th232onlyM2->GetBinContent(i)/hnewTeO2th232onlyM2->GetBinWidth(i));
    hAdapTeO2ra228pb208M2->SetBinContent(i, hnewTeO2ra228pb208M2->GetBinContent(i)/hnewTeO2ra228pb208M2->GetBinWidth(i));
    hAdapTeO2th230onlyM2->SetBinContent(i, hnewTeO2th230onlyM2->GetBinContent(i)/hnewTeO2th230onlyM2->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2_001->SetBinContent(i, hnewTeO2Sxth232onlyM2_001->GetBinContent(i)/hnewTeO2Sxth232onlyM2_001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2_001->SetBinContent(i, hnewTeO2Sxra228pb208M2_001->GetBinContent(i)/hnewTeO2Sxra228pb208M2_001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2_001->SetBinContent(i, hnewTeO2Sxu238th230M2_001->GetBinContent(i)/hnewTeO2Sxu238th230M2_001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2_001->SetBinContent(i, hnewTeO2Sxth230onlyM2_001->GetBinContent(i)/hnewTeO2Sxth230onlyM2_001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2_001->SetBinContent(i, hnewTeO2Sxra226pb210M2_001->GetBinContent(i)/hnewTeO2Sxra226pb210M2_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M2_0001->SetBinContent(i, hnewTeO2Sxpb210M2_0001->GetBinContent(i)/hnewTeO2Sxpb210M2_0001->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2_01->SetBinContent(i, hnewTeO2Sxth232onlyM2_01->GetBinContent(i)/hnewTeO2Sxth232onlyM2_01->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2_01->SetBinContent(i, hnewTeO2Sxra228pb208M2_01->GetBinContent(i)/hnewTeO2Sxra228pb208M2_01->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2_01->SetBinContent(i, hnewTeO2Sxu238th230M2_01->GetBinContent(i)/hnewTeO2Sxu238th230M2_01->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2_01->SetBinContent(i, hnewTeO2Sxth230onlyM2_01->GetBinContent(i)/hnewTeO2Sxth230onlyM2_01->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2_01->SetBinContent(i, hnewTeO2Sxra226pb210M2_01->GetBinContent(i)/hnewTeO2Sxra226pb210M2_01->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2_0001->SetBinContent(i, hnewTeO2Sxth232onlyM2_0001->GetBinContent(i)/hnewTeO2Sxth232onlyM2_0001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2_0001->SetBinContent(i, hnewTeO2Sxra228pb208M2_0001->GetBinContent(i)/hnewTeO2Sxra228pb208M2_0001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2_0001->SetBinContent(i, hnewTeO2Sxu238th230M2_0001->GetBinContent(i)/hnewTeO2Sxu238th230M2_0001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2_0001->SetBinContent(i, hnewTeO2Sxth230onlyM2_0001->GetBinContent(i)/hnewTeO2Sxth230onlyM2_0001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2_0001->SetBinContent(i, hnewTeO2Sxra226pb210M2_0001->GetBinContent(i)/hnewTeO2Sxra226pb210M2_0001->GetBinWidth(i));

    hAdapCuFrameco58M2->SetBinContent(i, hnewCuFrameco58M2->GetBinContent(i)/hnewCuFrameco58M2->GetBinWidth(i));
    hAdapCuFrameco60M2->SetBinContent(i, hnewCuFrameco60M2->GetBinContent(i)/hnewCuFrameco60M2->GetBinWidth(i));
    hAdapCuFramecs137M2->SetBinContent(i, hnewCuFramecs137M2->GetBinContent(i)/hnewCuFramecs137M2->GetBinWidth(i));
    hAdapCuFramek40M2->SetBinContent(i, hnewCuFramek40M2->GetBinContent(i)/hnewCuFramek40M2->GetBinWidth(i));
    hAdapCuFramemn54M2->SetBinContent(i, hnewCuFramemn54M2->GetBinContent(i)/hnewCuFramemn54M2->GetBinWidth(i));
    hAdapCuFramepb210M2->SetBinContent(i, hnewCuFramepb210M2->GetBinContent(i)/hnewCuFramepb210M2->GetBinWidth(i));
    hAdapCuFrameth232M2->SetBinContent(i, hnewCuFrameth232M2->GetBinContent(i)/hnewCuFrameth232M2->GetBinWidth(i));
    hAdapCuFrameu238M2->SetBinContent(i, hnewCuFrameu238M2->GetBinContent(i)/hnewCuFrameu238M2->GetBinWidth(i));

    // hAdapCuFrameSth232M2_1->SetBinContent(i, hnewCuFrameSth232M2_1->GetBinContent(i)/hnewCuFrameSth232M2_1->GetBinWidth(i));
    // hAdapCuFrameSu238M2_1->SetBinContent(i, hnewCuFrameSu238M2_1->GetBinContent(i)/hnewCuFrameSu238M2_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M2_001->SetBinContent(i, hnewCuFrameSxpb210M2_001->GetBinContent(i)/hnewCuFrameSxpb210M2_001->GetBinWidth(i));
    hAdapCuFrameSxpb210M2_01->SetBinContent(i, hnewCuFrameSxpb210M2_01->GetBinContent(i)/hnewCuFrameSxpb210M2_01->GetBinWidth(i));
    hAdapCuFrameSxpb210M2_1->SetBinContent(i, hnewCuFrameSxpb210M2_1->GetBinContent(i)/hnewCuFrameSxpb210M2_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M2_10->SetBinContent(i, hnewCuFrameSxpb210M2_10->GetBinContent(i)/hnewCuFrameSxpb210M2_10->GetBinWidth(i));
    hAdapCuFrameSxth232M2_001->SetBinContent(i, hnewCuFrameSxth232M2_001->GetBinContent(i)/hnewCuFrameSxth232M2_001->GetBinWidth(i));
    hAdapCuFrameSxth232M2_01->SetBinContent(i, hnewCuFrameSxth232M2_01->GetBinContent(i)/hnewCuFrameSxth232M2_01->GetBinWidth(i));
    hAdapCuFrameSxth232M2_1->SetBinContent(i, hnewCuFrameSxth232M2_1->GetBinContent(i)/hnewCuFrameSxth232M2_1->GetBinWidth(i));
    hAdapCuFrameSxth232M2_10->SetBinContent(i, hnewCuFrameSxth232M2_10->GetBinContent(i)/hnewCuFrameSxth232M2_10->GetBinWidth(i));
    hAdapCuFrameSxu238M2_001->SetBinContent(i, hnewCuFrameSxu238M2_001->GetBinContent(i)/hnewCuFrameSxu238M2_001->GetBinWidth(i));
    hAdapCuFrameSxu238M2_01->SetBinContent(i, hnewCuFrameSxu238M2_01->GetBinContent(i)/hnewCuFrameSxu238M2_01->GetBinWidth(i));
    hAdapCuFrameSxu238M2_1->SetBinContent(i, hnewCuFrameSxu238M2_1->GetBinContent(i)/hnewCuFrameSxu238M2_1->GetBinWidth(i));
    hAdapCuFrameSxu238M2_10->SetBinContent(i, hnewCuFrameSxu238M2_10->GetBinContent(i)/hnewCuFrameSxu238M2_10->GetBinWidth(i));

    hAdapCuBoxco58M2->SetBinContent(i, hnewCuBoxco58M2->GetBinContent(i)/hnewCuBoxco58M2->GetBinWidth(i));
    hAdapCuBoxco60M2->SetBinContent(i, hnewCuBoxco60M2->GetBinContent(i)/hnewCuBoxco60M2->GetBinWidth(i));
    hAdapCuBoxcs137M2->SetBinContent(i, hnewCuBoxcs137M2->GetBinContent(i)/hnewCuBoxcs137M2->GetBinWidth(i));
    hAdapCuBoxk40M2->SetBinContent(i, hnewCuBoxk40M2->GetBinContent(i)/hnewCuBoxk40M2->GetBinWidth(i));
    hAdapCuBoxmn54M2->SetBinContent(i, hnewCuBoxmn54M2->GetBinContent(i)/hnewCuBoxmn54M2->GetBinWidth(i));
    hAdapCuBoxpb210M2->SetBinContent(i, hnewCuBoxpb210M2->GetBinContent(i)/hnewCuBoxpb210M2->GetBinWidth(i));
    hAdapCuBoxth232M2->SetBinContent(i, hnewCuBoxth232M2->GetBinContent(i)/hnewCuBoxth232M2->GetBinWidth(i));
    hAdapCuBoxu238M2->SetBinContent(i, hnewCuBoxu238M2->GetBinContent(i)/hnewCuBoxu238M2->GetBinWidth(i));

    // hAdapCuBoxSth232M2_1->SetBinContent(i, hnewCuBoxSth232M2_1->GetBinContent(i)/hnewCuBoxSth232M2_1->GetBinWidth(i));
    // hAdapCuBoxSu238M2_1->SetBinContent(i, hnewCuBoxSu238M2_1->GetBinContent(i)/hnewCuBoxSu238M2_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M2_001->SetBinContent(i, hnewCuBoxSxpb210M2_001->GetBinContent(i)/hnewCuBoxSxpb210M2_001->GetBinWidth(i));
    hAdapCuBoxSxpb210M2_01->SetBinContent(i, hnewCuBoxSxpb210M2_01->GetBinContent(i)/hnewCuBoxSxpb210M2_01->GetBinWidth(i));
    hAdapCuBoxSxpb210M2_1->SetBinContent(i, hnewCuBoxSxpb210M2_1->GetBinContent(i)/hnewCuBoxSxpb210M2_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M2_10->SetBinContent(i, hnewCuBoxSxpb210M2_10->GetBinContent(i)/hnewCuBoxSxpb210M2_10->GetBinWidth(i));
    hAdapCuBoxSxth232M2_001->SetBinContent(i, hnewCuBoxSxth232M2_001->GetBinContent(i)/hnewCuBoxSxth232M2_001->GetBinWidth(i));
    hAdapCuBoxSxth232M2_01->SetBinContent(i, hnewCuBoxSxth232M2_01->GetBinContent(i)/hnewCuBoxSxth232M2_01->GetBinWidth(i));
    hAdapCuBoxSxth232M2_1->SetBinContent(i, hnewCuBoxSxth232M2_1->GetBinContent(i)/hnewCuBoxSxth232M2_1->GetBinWidth(i));
    hAdapCuBoxSxth232M2_10->SetBinContent(i, hnewCuBoxSxth232M2_10->GetBinContent(i)/hnewCuBoxSxth232M2_10->GetBinWidth(i));
    hAdapCuBoxSxu238M2_001->SetBinContent(i, hnewCuBoxSxu238M2_001->GetBinContent(i)/hnewCuBoxSxu238M2_001->GetBinWidth(i));
    hAdapCuBoxSxu238M2_01->SetBinContent(i, hnewCuBoxSxu238M2_01->GetBinContent(i)/hnewCuBoxSxu238M2_01->GetBinWidth(i));
    hAdapCuBoxSxu238M2_1->SetBinContent(i, hnewCuBoxSxu238M2_1->GetBinContent(i)/hnewCuBoxSxu238M2_1->GetBinWidth(i));
    hAdapCuBoxSxu238M2_10->SetBinContent(i, hnewCuBoxSxu238M2_10->GetBinContent(i)/hnewCuBoxSxu238M2_10->GetBinWidth(i));


    hAdapCuBox_CuFrameco60M2->SetBinContent(i, hnewCuBox_CuFrameco60M2->GetBinContent(i)/hnewCuBox_CuFrameco60M2->GetBinWidth(i));
    hAdapCuBox_CuFramek40M2->SetBinContent(i, hnewCuBox_CuFramek40M2->GetBinContent(i)/hnewCuBox_CuFramek40M2->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2->SetBinContent(i, hnewCuBox_CuFrameth232M2->GetBinContent(i)/hnewCuBox_CuFrameth232M2->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2->SetBinContent(i, hnewCuBox_CuFrameu238M2->GetBinContent(i)/hnewCuBox_CuFrameu238M2->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M2_10->SetBinContent(i, hnewCuBox_CuFrameth232M2_10->GetBinContent(i)/hnewCuBox_CuFrameth232M2_10->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2_10->SetBinContent(i, hnewCuBox_CuFrameu238M2_10->GetBinContent(i)/hnewCuBox_CuFrameu238M2_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2_10->SetBinContent(i, hnewCuBox_CuFramepb210M2_10->GetBinContent(i)/hnewCuBox_CuFramepb210M2_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2_1->SetBinContent(i, hnewCuBox_CuFramepb210M2_1->GetBinContent(i)/hnewCuBox_CuFramepb210M2_1->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2_01->SetBinContent(i, hnewCuBox_CuFramepb210M2_01->GetBinContent(i)/hnewCuBox_CuFramepb210M2_01->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2_001->SetBinContent(i, hnewCuBox_CuFramepb210M2_001->GetBinContent(i)/hnewCuBox_CuFramepb210M2_001->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M2_1->SetBinContent(i, hnewCuBox_CuFrameth232M2_1->GetBinContent(i)/hnewCuBox_CuFrameth232M2_1->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2_1->SetBinContent(i, hnewCuBox_CuFrameu238M2_1->GetBinContent(i)/hnewCuBox_CuFrameu238M2_1->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2_01->SetBinContent(i, hnewCuBox_CuFrameth232M2_01->GetBinContent(i)/hnewCuBox_CuFrameth232M2_01->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2_01->SetBinContent(i, hnewCuBox_CuFrameu238M2_01->GetBinContent(i)/hnewCuBox_CuFrameu238M2_01->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2_001->SetBinContent(i, hnewCuBox_CuFrameth232M2_001->GetBinContent(i)/hnewCuBox_CuFrameth232M2_001->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2_001->SetBinContent(i, hnewCuBox_CuFrameu238M2_001->GetBinContent(i)/hnewCuBox_CuFrameu238M2_001->GetBinWidth(i));  

/*
    hAdap50mKco58M2->SetBinContent(i, hnew50mKco58M2->GetBinContent(i)/hnew50mKco58M2->GetBinWidth(i));
    hAdap50mKco60M2->SetBinContent(i, hnew50mKco60M2->GetBinContent(i)/hnew50mKco60M2->GetBinWidth(i));
    hAdap50mKcs137M2->SetBinContent(i, hnew50mKcs137M2->GetBinContent(i)/hnew50mKcs137M2->GetBinWidth(i));
    hAdap50mKk40M2->SetBinContent(i, hnew50mKk40M2->GetBinContent(i)/hnew50mKk40M2->GetBinWidth(i));
    hAdap50mKmn54M2->SetBinContent(i, hnew50mKmn54M2->GetBinContent(i)/hnew50mKmn54M2->GetBinWidth(i));
    hAdap50mKpb210M2->SetBinContent(i, hnew50mKpb210M2->GetBinContent(i)/hnew50mKpb210M2->GetBinWidth(i));
    hAdap50mKth232M2->SetBinContent(i, hnew50mKth232M2->GetBinContent(i)/hnew50mKth232M2->GetBinWidth(i));
    hAdap50mKu238M2->SetBinContent(i, hnew50mKu238M2->GetBinContent(i)/hnew50mKu238M2->GetBinWidth(i));

    hAdap600mKco60M2->SetBinContent(i, hnew600mKco60M2->GetBinContent(i)/hnew600mKco60M2->GetBinWidth(i));
    hAdap600mKk40M2->SetBinContent(i, hnew600mKk40M2->GetBinContent(i)/hnew600mKk40M2->GetBinWidth(i));
    hAdap600mKth232M2->SetBinContent(i, hnew600mKth232M2->GetBinContent(i)/hnew600mKth232M2->GetBinWidth(i));
    hAdap600mKu238M2->SetBinContent(i, hnew600mKu238M2->GetBinContent(i)/hnew600mKu238M2->GetBinWidth(i));
*/

    // hAdapPbRombi207M2->SetBinContent(i, hnewPbRombi207M2->GetBinContent(i)/hnewPbRombi207M2->GetBinWidth(i));
    hAdapPbRomco60M2->SetBinContent(i, hnewPbRomco60M2->GetBinContent(i)/hnewPbRomco60M2->GetBinWidth(i));
    hAdapPbRomcs137M2->SetBinContent(i, hnewPbRomcs137M2->GetBinContent(i)/hnewPbRomcs137M2->GetBinWidth(i));
    hAdapPbRomk40M2->SetBinContent(i, hnewPbRomk40M2->GetBinContent(i)/hnewPbRomk40M2->GetBinWidth(i));
    hAdapPbRompb210M2->SetBinContent(i, hnewPbRompb210M2->GetBinContent(i)/hnewPbRompb210M2->GetBinWidth(i));
    hAdapPbRomth232M2->SetBinContent(i, hnewPbRomth232M2->GetBinContent(i)/hnewPbRomth232M2->GetBinWidth(i));
    hAdapPbRomu238M2->SetBinContent(i, hnewPbRomu238M2->GetBinContent(i)/hnewPbRomu238M2->GetBinWidth(i));

    hAdapInternalco60M2->SetBinContent(i, hnewInternalco60M2->GetBinContent(i)/hnewInternalco60M2->GetBinWidth(i));
    hAdapInternalk40M2->SetBinContent(i, hnewInternalk40M2->GetBinContent(i)/hnewInternalk40M2->GetBinWidth(i));
    hAdapInternalth232M2->SetBinContent(i, hnewInternalth232M2->GetBinContent(i)/hnewInternalth232M2->GetBinWidth(i));
    hAdapInternalu238M2->SetBinContent(i, hnewInternalu238M2->GetBinContent(i)/hnewInternalu238M2->GetBinWidth(i));

/*
    hAdapMBco60M2->SetBinContent(i, hnewMBco60M2->GetBinContent(i)/hnewMBco60M2->GetBinWidth(i));
    hAdapMBk40M2->SetBinContent(i, hnewMBk40M2->GetBinContent(i)/hnewMBk40M2->GetBinWidth(i));
    hAdapMBth232M2->SetBinContent(i, hnewMBth232M2->GetBinContent(i)/hnewMBth232M2->GetBinWidth(i));
    hAdapMBu238M2->SetBinContent(i, hnewMBu238M2->GetBinContent(i)/hnewMBu238M2->GetBinWidth(i));

    hAdapSIk40M2->SetBinContent(i, hnewSIk40M2->GetBinContent(i)/hnewSIk40M2->GetBinWidth(i));
    hAdapSIth232M2->SetBinContent(i, hnewSIth232M2->GetBinContent(i)/hnewSIth232M2->GetBinWidth(i));
    hAdapSIu238M2->SetBinContent(i, hnewSIu238M2->GetBinContent(i)/hnewSIu238M2->GetBinWidth(i));


    hAdapIVCco60M2->SetBinContent(i, hnewIVCco60M2->GetBinContent(i)/hnewIVCco60M2->GetBinWidth(i));
    hAdapIVCk40M2->SetBinContent(i, hnewIVCk40M2->GetBinContent(i)/hnewIVCk40M2->GetBinWidth(i));
    hAdapIVCth232M2->SetBinContent(i, hnewIVCth232M2->GetBinContent(i)/hnewIVCth232M2->GetBinWidth(i));
    hAdapIVCu238M2->SetBinContent(i, hnewIVCu238M2->GetBinContent(i)/hnewIVCu238M2->GetBinWidth(i));
*/

    hAdapOVCco60M2->SetBinContent(i, hnewOVCco60M2->GetBinContent(i)/hnewOVCco60M2->GetBinWidth(i));
    hAdapOVCk40M2->SetBinContent(i, hnewOVCk40M2->GetBinContent(i)/hnewOVCk40M2->GetBinWidth(i));
    hAdapOVCth232M2->SetBinContent(i, hnewOVCth232M2->GetBinContent(i)/hnewOVCth232M2->GetBinWidth(i));
    hAdapOVCu238M2->SetBinContent(i, hnewOVCu238M2->GetBinContent(i)/hnewOVCu238M2->GetBinWidth(i));

    hAdapExtPbbi210M2->SetBinContent(i, hnewExtPbbi210M2->GetBinContent(i)/hnewExtPbbi210M2->GetBinWidth(i));
  }


  for(int i = 1; i <= dAdaptiveBinsM2Sum; i++)
  {
    hAdapTeO20nuM2Sum->SetBinContent(i, hnewTeO20nuM2Sum->GetBinContent(i)/hnewTeO20nuM2Sum->GetBinWidth(i));
    hAdapTeO22nuM2Sum->SetBinContent(i, hnewTeO22nuM2Sum->GetBinContent(i)/hnewTeO22nuM2Sum->GetBinWidth(i));
    hAdapTeO2co60M2Sum->SetBinContent(i, hnewTeO2co60M2Sum->GetBinContent(i)/hnewTeO2co60M2Sum->GetBinWidth(i));
    hAdapTeO2k40M2Sum->SetBinContent(i, hnewTeO2k40M2Sum->GetBinContent(i)/hnewTeO2k40M2Sum->GetBinWidth(i));
    hAdapTeO2pb210M2Sum->SetBinContent(i, hnewTeO2pb210M2Sum->GetBinContent(i)/hnewTeO2pb210M2Sum->GetBinWidth(i));
    hAdapTeO2po210M2Sum->SetBinContent(i, hnewTeO2po210M2Sum->GetBinContent(i)/hnewTeO2po210M2Sum->GetBinWidth(i));
    hAdapTeO2te125M2Sum->SetBinContent(i, hnewTeO2te125M2Sum->GetBinContent(i)/hnewTeO2te125M2Sum->GetBinWidth(i));
    hAdapTeO2th232M2Sum->SetBinContent(i, hnewTeO2th232M2Sum->GetBinContent(i)/hnewTeO2th232M2Sum->GetBinWidth(i));
    // hAdapTeO2th228M2Sum->SetBinContent(i, hnewTeO2th228M2Sum->GetBinContent(i)/hnewTeO2th228M2Sum->GetBinWidth(i));
    // hAdapTeO2ra226M2Sum->SetBinContent(i, hnewTeO2ra226M2Sum->GetBinContent(i)/hnewTeO2ra226M2Sum->GetBinWidth(i));
    // hAdapTeO2rn222M2Sum->SetBinContent(i, hnewTeO2rn222M2Sum->GetBinContent(i)/hnewTeO2rn222M2Sum->GetBinWidth(i));
    hAdapTeO2u238M2Sum->SetBinContent(i, hnewTeO2u238M2Sum->GetBinContent(i)/hnewTeO2u238M2Sum->GetBinWidth(i));
    // hAdapTeO2th230M2Sum->SetBinContent(i, hnewTeO2th230M2Sum->GetBinContent(i)/hnewTeO2th230M2Sum->GetBinWidth(i));
    // hAdapTeO2u234M2Sum->SetBinContent(i, hnewTeO2u234M2Sum->GetBinContent(i)/hnewTeO2u234M2Sum->GetBinWidth(i));

    // hAdapTeO2Spb210M2Sum_01->SetBinContent(i, hnewTeO2Spb210M2Sum_01->GetBinContent(i)/hnewTeO2Spb210M2Sum_01->GetBinWidth(i));
    // hAdapTeO2Spo210M2Sum_001->SetBinContent(i, hnewTeO2Spo210M2Sum_001->GetBinContent(i)/hnewTeO2Spo210M2Sum_001->GetBinWidth(i));
    // hAdapTeO2Spo210M2Sum_01->SetBinContent(i, hnewTeO2Spo210M2Sum_01->GetBinContent(i)/hnewTeO2Spo210M2Sum_01->GetBinWidth(i));
    // hAdapTeO2Sth232M2Sum_01->SetBinContent(i, hnewTeO2Sth232M2Sum_01->GetBinContent(i)/hnewTeO2Sth232M2Sum_01->GetBinWidth(i));
    // hAdapTeO2Su238M2Sum_01->SetBinContent(i, hnewTeO2Su238M2Sum_01->GetBinContent(i)/hnewTeO2Su238M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_001->SetBinContent(i, hnewTeO2Sxpb210M2Sum_001->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_01->SetBinContent(i, hnewTeO2Sxpb210M2Sum_01->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_1->SetBinContent(i, hnewTeO2Sxpb210M2Sum_1->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_1->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_10->SetBinContent(i, hnewTeO2Sxpb210M2Sum_10->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_10->GetBinWidth(i));
    hAdapTeO2Sxpo210M2Sum_001->SetBinContent(i, hnewTeO2Sxpo210M2Sum_001->GetBinContent(i)/hnewTeO2Sxpo210M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxpo210M2Sum_01->SetBinContent(i, hnewTeO2Sxpo210M2Sum_01->GetBinContent(i)/hnewTeO2Sxpo210M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxpo210M2Sum_1->SetBinContent(i, hnewTeO2Sxpo210M2Sum_1->GetBinContent(i)/hnewTeO2Sxpo210M2Sum_1->GetBinWidth(i));
    hAdapTeO2Sxth232M2Sum_001->SetBinContent(i, hnewTeO2Sxth232M2Sum_001->GetBinContent(i)/hnewTeO2Sxth232M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxth232M2Sum_01->SetBinContent(i, hnewTeO2Sxth232M2Sum_01->GetBinContent(i)/hnewTeO2Sxth232M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxth232M2Sum_1->SetBinContent(i, hnewTeO2Sxth232M2Sum_1->GetBinContent(i)/hnewTeO2Sxth232M2Sum_1->GetBinWidth(i));
    hAdapTeO2Sxth232M2Sum_10->SetBinContent(i, hnewTeO2Sxth232M2Sum_10->GetBinContent(i)/hnewTeO2Sxth232M2Sum_10->GetBinWidth(i));
    hAdapTeO2Sxu238M2Sum_001->SetBinContent(i, hnewTeO2Sxu238M2Sum_001->GetBinContent(i)/hnewTeO2Sxu238M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxu238M2Sum_01->SetBinContent(i, hnewTeO2Sxu238M2Sum_01->GetBinContent(i)/hnewTeO2Sxu238M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxu238M2Sum_1->SetBinContent(i, hnewTeO2Sxu238M2Sum_1->GetBinContent(i)/hnewTeO2Sxu238M2Sum_1->GetBinWidth(i));
    hAdapTeO2Sxu238M2Sum_10->SetBinContent(i, hnewTeO2Sxu238M2Sum_10->GetBinContent(i)/hnewTeO2Sxu238M2Sum_10->GetBinWidth(i));

    hAdapTeO2Sxu238M2Sum_100->SetBinContent(i, hnewTeO2Sxu238M2Sum_100->GetBinContent(i)/hnewTeO2Sxu238M2Sum_100->GetBinWidth(i));
    hAdapTeO2Sxth232M2Sum_100->SetBinContent(i, hnewTeO2Sxth232M2Sum_100->GetBinContent(i)/hnewTeO2Sxth232M2Sum_100->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_100->SetBinContent(i, hnewTeO2Sxpb210M2Sum_100->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_100->GetBinWidth(i));

    hAdapTeO2th232onlyM2Sum->SetBinContent(i, hnewTeO2th232onlyM2Sum->GetBinContent(i)/hnewTeO2th232onlyM2Sum->GetBinWidth(i));
    hAdapTeO2ra228pb208M2Sum->SetBinContent(i, hnewTeO2ra228pb208M2Sum->GetBinContent(i)/hnewTeO2ra228pb208M2Sum->GetBinWidth(i));
    hAdapTeO2th230onlyM2Sum->SetBinContent(i, hnewTeO2th230onlyM2Sum->GetBinContent(i)/hnewTeO2th230onlyM2Sum->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2Sum_001->SetBinContent(i, hnewTeO2Sxth232onlyM2Sum_001->GetBinContent(i)/hnewTeO2Sxth232onlyM2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2Sum_001->SetBinContent(i, hnewTeO2Sxra228pb208M2Sum_001->GetBinContent(i)/hnewTeO2Sxra228pb208M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2Sum_001->SetBinContent(i, hnewTeO2Sxu238th230M2Sum_001->GetBinContent(i)/hnewTeO2Sxu238th230M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2Sum_001->SetBinContent(i, hnewTeO2Sxth230onlyM2Sum_001->GetBinContent(i)/hnewTeO2Sxth230onlyM2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2Sum_001->SetBinContent(i, hnewTeO2Sxra226pb210M2Sum_001->GetBinContent(i)/hnewTeO2Sxra226pb210M2Sum_001->GetBinWidth(i));
    hAdapTeO2Sxpb210M2Sum_0001->SetBinContent(i, hnewTeO2Sxpb210M2Sum_0001->GetBinContent(i)/hnewTeO2Sxpb210M2Sum_0001->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2Sum_01->SetBinContent(i, hnewTeO2Sxth232onlyM2Sum_01->GetBinContent(i)/hnewTeO2Sxth232onlyM2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2Sum_01->SetBinContent(i, hnewTeO2Sxra228pb208M2Sum_01->GetBinContent(i)/hnewTeO2Sxra228pb208M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2Sum_01->SetBinContent(i, hnewTeO2Sxu238th230M2Sum_01->GetBinContent(i)/hnewTeO2Sxu238th230M2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2Sum_01->SetBinContent(i, hnewTeO2Sxth230onlyM2Sum_01->GetBinContent(i)/hnewTeO2Sxth230onlyM2Sum_01->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2Sum_01->SetBinContent(i, hnewTeO2Sxra226pb210M2Sum_01->GetBinContent(i)/hnewTeO2Sxra226pb210M2Sum_01->GetBinWidth(i));

    hAdapTeO2Sxth232onlyM2Sum_0001->SetBinContent(i, hnewTeO2Sxth232onlyM2Sum_0001->GetBinContent(i)/hnewTeO2Sxth232onlyM2Sum_0001->GetBinWidth(i));
    hAdapTeO2Sxra228pb208M2Sum_0001->SetBinContent(i, hnewTeO2Sxra228pb208M2Sum_0001->GetBinContent(i)/hnewTeO2Sxra228pb208M2Sum_0001->GetBinWidth(i));
    hAdapTeO2Sxu238th230M2Sum_0001->SetBinContent(i, hnewTeO2Sxu238th230M2Sum_0001->GetBinContent(i)/hnewTeO2Sxu238th230M2Sum_0001->GetBinWidth(i));
    hAdapTeO2Sxth230onlyM2Sum_0001->SetBinContent(i, hnewTeO2Sxth230onlyM2Sum_0001->GetBinContent(i)/hnewTeO2Sxth230onlyM2Sum_0001->GetBinWidth(i));
    hAdapTeO2Sxra226pb210M2Sum_0001->SetBinContent(i, hnewTeO2Sxra226pb210M2Sum_0001->GetBinContent(i)/hnewTeO2Sxra226pb210M2Sum_0001->GetBinWidth(i));

    hAdapCuFrameco58M2Sum->SetBinContent(i, hnewCuFrameco58M2Sum->GetBinContent(i)/hnewCuFrameco58M2Sum->GetBinWidth(i));
    hAdapCuFrameco60M2Sum->SetBinContent(i, hnewCuFrameco60M2Sum->GetBinContent(i)/hnewCuFrameco60M2Sum->GetBinWidth(i));
    hAdapCuFramecs137M2Sum->SetBinContent(i, hnewCuFramecs137M2Sum->GetBinContent(i)/hnewCuFramecs137M2Sum->GetBinWidth(i));
    hAdapCuFramek40M2Sum->SetBinContent(i, hnewCuFramek40M2Sum->GetBinContent(i)/hnewCuFramek40M2Sum->GetBinWidth(i));
    hAdapCuFramemn54M2Sum->SetBinContent(i, hnewCuFramemn54M2Sum->GetBinContent(i)/hnewCuFramemn54M2Sum->GetBinWidth(i));
    hAdapCuFramepb210M2Sum->SetBinContent(i, hnewCuFramepb210M2Sum->GetBinContent(i)/hnewCuFramepb210M2Sum->GetBinWidth(i));
    hAdapCuFrameth232M2Sum->SetBinContent(i, hnewCuFrameth232M2Sum->GetBinContent(i)/hnewCuFrameth232M2Sum->GetBinWidth(i));
    hAdapCuFrameu238M2Sum->SetBinContent(i, hnewCuFrameu238M2Sum->GetBinContent(i)/hnewCuFrameu238M2Sum->GetBinWidth(i));

    // hAdapCuFrameSth232M2Sum_1->SetBinContent(i, hnewCuFrameSth232M2Sum_1->GetBinContent(i)/hnewCuFrameSth232M2Sum_1->GetBinWidth(i));
    // hAdapCuFrameSu238M2Sum_1->SetBinContent(i, hnewCuFrameSu238M2Sum_1->GetBinContent(i)/hnewCuFrameSu238M2Sum_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M2Sum_001->SetBinContent(i, hnewCuFrameSxpb210M2Sum_001->GetBinContent(i)/hnewCuFrameSxpb210M2Sum_001->GetBinWidth(i));
    hAdapCuFrameSxpb210M2Sum_01->SetBinContent(i, hnewCuFrameSxpb210M2Sum_01->GetBinContent(i)/hnewCuFrameSxpb210M2Sum_01->GetBinWidth(i));
    hAdapCuFrameSxpb210M2Sum_1->SetBinContent(i, hnewCuFrameSxpb210M2Sum_1->GetBinContent(i)/hnewCuFrameSxpb210M2Sum_1->GetBinWidth(i));
    hAdapCuFrameSxpb210M2Sum_10->SetBinContent(i, hnewCuFrameSxpb210M2Sum_10->GetBinContent(i)/hnewCuFrameSxpb210M2Sum_10->GetBinWidth(i));
    hAdapCuFrameSxth232M2Sum_001->SetBinContent(i, hnewCuFrameSxth232M2Sum_001->GetBinContent(i)/hnewCuFrameSxth232M2Sum_001->GetBinWidth(i));
    hAdapCuFrameSxth232M2Sum_01->SetBinContent(i, hnewCuFrameSxth232M2Sum_01->GetBinContent(i)/hnewCuFrameSxth232M2Sum_01->GetBinWidth(i));
    hAdapCuFrameSxth232M2Sum_1->SetBinContent(i, hnewCuFrameSxth232M2Sum_1->GetBinContent(i)/hnewCuFrameSxth232M2Sum_1->GetBinWidth(i));
    hAdapCuFrameSxth232M2Sum_10->SetBinContent(i, hnewCuFrameSxth232M2Sum_10->GetBinContent(i)/hnewCuFrameSxth232M2Sum_10->GetBinWidth(i));
    hAdapCuFrameSxu238M2Sum_001->SetBinContent(i, hnewCuFrameSxu238M2Sum_001->GetBinContent(i)/hnewCuFrameSxu238M2Sum_001->GetBinWidth(i));
    hAdapCuFrameSxu238M2Sum_01->SetBinContent(i, hnewCuFrameSxu238M2Sum_01->GetBinContent(i)/hnewCuFrameSxu238M2Sum_01->GetBinWidth(i));
    hAdapCuFrameSxu238M2Sum_1->SetBinContent(i, hnewCuFrameSxu238M2Sum_1->GetBinContent(i)/hnewCuFrameSxu238M2Sum_1->GetBinWidth(i));
    hAdapCuFrameSxu238M2Sum_10->SetBinContent(i, hnewCuFrameSxu238M2Sum_10->GetBinContent(i)/hnewCuFrameSxu238M2Sum_10->GetBinWidth(i));

    hAdapCuBoxco58M2Sum->SetBinContent(i, hnewCuBoxco58M2Sum->GetBinContent(i)/hnewCuBoxco58M2Sum->GetBinWidth(i));
    hAdapCuBoxco60M2Sum->SetBinContent(i, hnewCuBoxco60M2Sum->GetBinContent(i)/hnewCuBoxco60M2Sum->GetBinWidth(i));
    hAdapCuBoxcs137M2Sum->SetBinContent(i, hnewCuBoxcs137M2Sum->GetBinContent(i)/hnewCuBoxcs137M2Sum->GetBinWidth(i));
    hAdapCuBoxk40M2Sum->SetBinContent(i, hnewCuBoxk40M2Sum->GetBinContent(i)/hnewCuBoxk40M2Sum->GetBinWidth(i));
    hAdapCuBoxmn54M2Sum->SetBinContent(i, hnewCuBoxmn54M2Sum->GetBinContent(i)/hnewCuBoxmn54M2Sum->GetBinWidth(i));
    hAdapCuBoxpb210M2Sum->SetBinContent(i, hnewCuBoxpb210M2Sum->GetBinContent(i)/hnewCuBoxpb210M2Sum->GetBinWidth(i));
    hAdapCuBoxth232M2Sum->SetBinContent(i, hnewCuBoxth232M2Sum->GetBinContent(i)/hnewCuBoxth232M2Sum->GetBinWidth(i));
    hAdapCuBoxu238M2Sum->SetBinContent(i, hnewCuBoxu238M2Sum->GetBinContent(i)/hnewCuBoxu238M2Sum->GetBinWidth(i));

    // hAdapCuBoxSth232M2Sum_1->SetBinContent(i, hnewCuBoxSth232M2Sum_1->GetBinContent(i)/hnewCuBoxSth232M2Sum_1->GetBinWidth(i));
    // hAdapCuBoxSu238M2Sum_1->SetBinContent(i, hnewCuBoxSu238M2Sum_1->GetBinContent(i)/hnewCuBoxSu238M2Sum_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M2Sum_001->SetBinContent(i, hnewCuBoxSxpb210M2Sum_001->GetBinContent(i)/hnewCuBoxSxpb210M2Sum_001->GetBinWidth(i));
    hAdapCuBoxSxpb210M2Sum_01->SetBinContent(i, hnewCuBoxSxpb210M2Sum_01->GetBinContent(i)/hnewCuBoxSxpb210M2Sum_01->GetBinWidth(i));
    hAdapCuBoxSxpb210M2Sum_1->SetBinContent(i, hnewCuBoxSxpb210M2Sum_1->GetBinContent(i)/hnewCuBoxSxpb210M2Sum_1->GetBinWidth(i));
    hAdapCuBoxSxpb210M2Sum_10->SetBinContent(i, hnewCuBoxSxpb210M2Sum_10->GetBinContent(i)/hnewCuBoxSxpb210M2Sum_10->GetBinWidth(i));
    hAdapCuBoxSxth232M2Sum_001->SetBinContent(i, hnewCuBoxSxth232M2Sum_001->GetBinContent(i)/hnewCuBoxSxth232M2Sum_001->GetBinWidth(i));
    hAdapCuBoxSxth232M2Sum_01->SetBinContent(i, hnewCuBoxSxth232M2Sum_01->GetBinContent(i)/hnewCuBoxSxth232M2Sum_01->GetBinWidth(i));
    hAdapCuBoxSxth232M2Sum_1->SetBinContent(i, hnewCuBoxSxth232M2Sum_1->GetBinContent(i)/hnewCuBoxSxth232M2Sum_1->GetBinWidth(i));
    hAdapCuBoxSxth232M2Sum_10->SetBinContent(i, hnewCuBoxSxth232M2Sum_10->GetBinContent(i)/hnewCuBoxSxth232M2Sum_10->GetBinWidth(i));
    hAdapCuBoxSxu238M2Sum_001->SetBinContent(i, hnewCuBoxSxu238M2Sum_001->GetBinContent(i)/hnewCuBoxSxu238M2Sum_001->GetBinWidth(i));
    hAdapCuBoxSxu238M2Sum_01->SetBinContent(i, hnewCuBoxSxu238M2Sum_01->GetBinContent(i)/hnewCuBoxSxu238M2Sum_01->GetBinWidth(i));
    hAdapCuBoxSxu238M2Sum_1->SetBinContent(i, hnewCuBoxSxu238M2Sum_1->GetBinContent(i)/hnewCuBoxSxu238M2Sum_1->GetBinWidth(i));
    hAdapCuBoxSxu238M2Sum_10->SetBinContent(i, hnewCuBoxSxu238M2Sum_10->GetBinContent(i)/hnewCuBoxSxu238M2Sum_10->GetBinWidth(i));


    hAdapCuBox_CuFrameco60M2Sum->SetBinContent(i, hnewCuBox_CuFrameco60M2Sum->GetBinContent(i)/hnewCuBox_CuFrameco60M2Sum->GetBinWidth(i));
    hAdapCuBox_CuFramek40M2Sum->SetBinContent(i, hnewCuBox_CuFramek40M2Sum->GetBinContent(i)/hnewCuBox_CuFramek40M2Sum->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2Sum->SetBinContent(i, hnewCuBox_CuFrameth232M2Sum->GetBinContent(i)/hnewCuBox_CuFrameth232M2Sum->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2Sum->SetBinContent(i, hnewCuBox_CuFrameu238M2Sum->GetBinContent(i)/hnewCuBox_CuFrameu238M2Sum->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M2Sum_10->SetBinContent(i, hnewCuBox_CuFrameth232M2Sum_10->GetBinContent(i)/hnewCuBox_CuFrameth232M2Sum_10->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2Sum_10->SetBinContent(i, hnewCuBox_CuFrameu238M2Sum_10->GetBinContent(i)/hnewCuBox_CuFrameu238M2Sum_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2Sum_10->SetBinContent(i, hnewCuBox_CuFramepb210M2Sum_10->GetBinContent(i)/hnewCuBox_CuFramepb210M2Sum_10->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2Sum_1->SetBinContent(i, hnewCuBox_CuFramepb210M2Sum_1->GetBinContent(i)/hnewCuBox_CuFramepb210M2Sum_1->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2Sum_01->SetBinContent(i, hnewCuBox_CuFramepb210M2Sum_01->GetBinContent(i)/hnewCuBox_CuFramepb210M2Sum_01->GetBinWidth(i));
    hAdapCuBox_CuFramepb210M2Sum_001->SetBinContent(i, hnewCuBox_CuFramepb210M2Sum_001->GetBinContent(i)/hnewCuBox_CuFramepb210M2Sum_001->GetBinWidth(i));

    hAdapCuBox_CuFrameth232M2Sum_1->SetBinContent(i, hnewCuBox_CuFrameth232M2Sum_1->GetBinContent(i)/hnewCuBox_CuFrameth232M2Sum_1->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2Sum_1->SetBinContent(i, hnewCuBox_CuFrameu238M2Sum_1->GetBinContent(i)/hnewCuBox_CuFrameu238M2Sum_1->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2Sum_01->SetBinContent(i, hnewCuBox_CuFrameth232M2Sum_01->GetBinContent(i)/hnewCuBox_CuFrameth232M2Sum_01->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2Sum_01->SetBinContent(i, hnewCuBox_CuFrameu238M2Sum_01->GetBinContent(i)/hnewCuBox_CuFrameu238M2Sum_01->GetBinWidth(i));
    hAdapCuBox_CuFrameth232M2Sum_001->SetBinContent(i, hnewCuBox_CuFrameth232M2Sum_001->GetBinContent(i)/hnewCuBox_CuFrameth232M2Sum_001->GetBinWidth(i));
    hAdapCuBox_CuFrameu238M2Sum_001->SetBinContent(i, hnewCuBox_CuFrameu238M2Sum_001->GetBinContent(i)/hnewCuBox_CuFrameu238M2Sum_001->GetBinWidth(i));  

/*
    hAdap50mKco58M2Sum->SetBinContent(i, hnew50mKco58M2Sum->GetBinContent(i)/hnew50mKco58M2Sum->GetBinWidth(i));
    hAdap50mKco60M2Sum->SetBinContent(i, hnew50mKco60M2Sum->GetBinContent(i)/hnew50mKco60M2Sum->GetBinWidth(i));
    hAdap50mKcs137M2Sum->SetBinContent(i, hnew50mKcs137M2Sum->GetBinContent(i)/hnew50mKcs137M2Sum->GetBinWidth(i));
    hAdap50mKk40M2Sum->SetBinContent(i, hnew50mKk40M2Sum->GetBinContent(i)/hnew50mKk40M2Sum->GetBinWidth(i));
    hAdap50mKmn54M2Sum->SetBinContent(i, hnew50mKmn54M2Sum->GetBinContent(i)/hnew50mKmn54M2Sum->GetBinWidth(i));
    hAdap50mKpb210M2Sum->SetBinContent(i, hnew50mKpb210M2Sum->GetBinContent(i)/hnew50mKpb210M2Sum->GetBinWidth(i));
    hAdap50mKth232M2Sum->SetBinContent(i, hnew50mKth232M2Sum->GetBinContent(i)/hnew50mKth232M2Sum->GetBinWidth(i));
    hAdap50mKu238M2Sum->SetBinContent(i, hnew50mKu238M2Sum->GetBinContent(i)/hnew50mKu238M2Sum->GetBinWidth(i));

    hAdap600mKco60M2Sum->SetBinContent(i, hnew600mKco60M2Sum->GetBinContent(i)/hnew600mKco60M2Sum->GetBinWidth(i));
    hAdap600mKk40M2Sum->SetBinContent(i, hnew600mKk40M2Sum->GetBinContent(i)/hnew600mKk40M2Sum->GetBinWidth(i));
    hAdap600mKth232M2Sum->SetBinContent(i, hnew600mKth232M2Sum->GetBinContent(i)/hnew600mKth232M2Sum->GetBinWidth(i));
    hAdap600mKu238M2Sum->SetBinContent(i, hnew600mKu238M2Sum->GetBinContent(i)/hnew600mKu238M2Sum->GetBinWidth(i));
*/

    // hAdapPbRombi207M2Sum->SetBinContent(i, hnewPbRombi207M2Sum->GetBinContent(i)/hnewPbRombi207M2Sum->GetBinWidth(i));
    hAdapPbRomco60M2Sum->SetBinContent(i, hnewPbRomco60M2Sum->GetBinContent(i)/hnewPbRomco60M2Sum->GetBinWidth(i));
    hAdapPbRomcs137M2Sum->SetBinContent(i, hnewPbRomcs137M2Sum->GetBinContent(i)/hnewPbRomcs137M2Sum->GetBinWidth(i));
    hAdapPbRomk40M2Sum->SetBinContent(i, hnewPbRomk40M2Sum->GetBinContent(i)/hnewPbRomk40M2Sum->GetBinWidth(i));
    hAdapPbRompb210M2Sum->SetBinContent(i, hnewPbRompb210M2Sum->GetBinContent(i)/hnewPbRompb210M2Sum->GetBinWidth(i));
    hAdapPbRomth232M2Sum->SetBinContent(i, hnewPbRomth232M2Sum->GetBinContent(i)/hnewPbRomth232M2Sum->GetBinWidth(i));
    hAdapPbRomu238M2Sum->SetBinContent(i, hnewPbRomu238M2Sum->GetBinContent(i)/hnewPbRomu238M2Sum->GetBinWidth(i));

    hAdapInternalco60M2Sum->SetBinContent(i, hnewInternalco60M2Sum->GetBinContent(i)/hnewInternalco60M2Sum->GetBinWidth(i));
    hAdapInternalk40M2Sum->SetBinContent(i, hnewInternalk40M2Sum->GetBinContent(i)/hnewInternalk40M2Sum->GetBinWidth(i));
    hAdapInternalth232M2Sum->SetBinContent(i, hnewInternalth232M2Sum->GetBinContent(i)/hnewInternalth232M2Sum->GetBinWidth(i));
    hAdapInternalu238M2Sum->SetBinContent(i, hnewInternalu238M2Sum->GetBinContent(i)/hnewInternalu238M2Sum->GetBinWidth(i));

/*
    hAdapMBco60M2Sum->SetBinContent(i, hnewMBco60M2Sum->GetBinContent(i)/hnewMBco60M2Sum->GetBinWidth(i));
    hAdapMBk40M2Sum->SetBinContent(i, hnewMBk40M2Sum->GetBinContent(i)/hnewMBk40M2Sum->GetBinWidth(i));
    hAdapMBth232M2Sum->SetBinContent(i, hnewMBth232M2Sum->GetBinContent(i)/hnewMBth232M2Sum->GetBinWidth(i));
    hAdapMBu238M2Sum->SetBinContent(i, hnewMBu238M2Sum->GetBinContent(i)/hnewMBu238M2Sum->GetBinWidth(i));

    hAdapSIk40M2Sum->SetBinContent(i, hnewSIk40M2Sum->GetBinContent(i)/hnewSIk40M2Sum->GetBinWidth(i));
    hAdapSIth232M2Sum->SetBinContent(i, hnewSIth232M2Sum->GetBinContent(i)/hnewSIth232M2Sum->GetBinWidth(i));
    hAdapSIu238M2Sum->SetBinContent(i, hnewSIu238M2Sum->GetBinContent(i)/hnewSIu238M2Sum->GetBinWidth(i));

    hAdapIVCco60M2Sum->SetBinContent(i, hnewIVCco60M2Sum->GetBinContent(i)/hnewIVCco60M2Sum->GetBinWidth(i));
    hAdapIVCk40M2Sum->SetBinContent(i, hnewIVCk40M2Sum->GetBinContent(i)/hnewIVCk40M2Sum->GetBinWidth(i));
    hAdapIVCth232M2Sum->SetBinContent(i, hnewIVCth232M2Sum->GetBinContent(i)/hnewIVCth232M2Sum->GetBinWidth(i));
    hAdapIVCu238M2Sum->SetBinContent(i, hnewIVCu238M2Sum->GetBinContent(i)/hnewIVCu238M2Sum->GetBinWidth(i));
*/

    hAdapOVCco60M2Sum->SetBinContent(i, hnewOVCco60M2Sum->GetBinContent(i)/hnewOVCco60M2Sum->GetBinWidth(i));
    hAdapOVCk40M2Sum->SetBinContent(i, hnewOVCk40M2Sum->GetBinContent(i)/hnewOVCk40M2Sum->GetBinWidth(i));
    hAdapOVCth232M2Sum->SetBinContent(i, hnewOVCth232M2Sum->GetBinContent(i)/hnewOVCth232M2Sum->GetBinWidth(i));
    hAdapOVCu238M2Sum->SetBinContent(i, hnewOVCu238M2Sum->GetBinContent(i)/hnewOVCu238M2Sum->GetBinWidth(i));

    hAdapExtPbbi210M2Sum->SetBinContent(i, hnewExtPbbi210M2Sum->GetBinContent(i)/hnewExtPbbi210M2Sum->GetBinWidth(i));

  }

  SetParEfficiency();
}

// Loads the background data
void TBackgroundModel::LoadData()
{
  switch(dDataSet)
  { 
  case 1:
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2049.root", dDataDir.c_str()));   
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2061.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2064.root", dDataDir.c_str()));   
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2067.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2070.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2073.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2076.root", dDataDir.c_str())); 
    dLivetime = 6042498; // DR 1 
    cout << "Using Data Release 1" << endl;
  break;

  case 2:
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2079.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2085.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2088.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2091.root", dDataDir.c_str())); 
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2097.root", dDataDir.c_str()));
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2100.root", dDataDir.c_str())); 
    dLivetime = 9387524; // DR 2
    cout << "Using Data Release 2" << endl;
  break;

  case 3:
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2103.root", dDataDir.c_str()));
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2109.root", dDataDir.c_str()));
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2118.root", dDataDir.c_str()));
    qtree->Add(Form("%s/Unblinded/ReducedB-ds2124.root", dDataDir.c_str()));
    dLivetime = 7647908; // DR 3
    cout << "Using Data Release 3" << endl;
  break;

  case -1:
    cout << "Using Toy data" << endl;
    bToyData = true;
    qtree->Add(Form("%s/Unblinded/ReducedB-ds*.root", dDataDir.c_str())); 
    dLivetime = 23077930+1511379+901597; // seconds of livetime (DR1 to DR3)
  break;

  default:   
    qtree->Add(Form("%s/Unblinded/ReducedB-ds*.root", dDataDir.c_str())); 
    dLivetime = 23077930+1511379+901597; // seconds of livetime (DR1 to DR3)
    cout << "Using Total Dataset" << endl;
  }

  qtree->Project("fDataHistoTot", "RescaledEnergy", base_cut);
  qtree->Project("fDataHistoM1",  "RescaledEnergy", base_cut && "Multiplicity_Sync == 1");
  qtree->Project("fDataHistoM2",  "RescaledEnergy", base_cut && "Multiplicity_Sync == 2");
  qtree->Project("fDataHistoM2Sum",  "TotalEnergy_Sync", base_cut && "Multiplicity_Sync == 2");

  dLivetimeYr = dLivetime*dSecToYears;  
}

// Prints parameters, needs update 11-06-2014
void TBackgroundModel::PrintParameters()
{
  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
    cout << i << " " << minuit->fCpnam[i] << ": " << fParameters[i] << " +/- " << fParError[i] << endl;
  }
}

void TBackgroundModel::PrintParActivity()
{ 
  // This only gets the number of counts corrected for detector efficiency
  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
  cout << i << " " << minuit->fCpnam[i] << " activity: " << fParActivity[i] << endl;
  }

}

// Resets all parameters to 0
void TBackgroundModel::ResetParameters()
{
  dNumCalls = 0;
  dChiSquare = 0;
  
  fModelTotM1->Reset();
  fModelTotthM1->Reset();
  fModelTotuM1->Reset();
  fModelTotkM1->Reset();
  fModelTotcoM1->Reset();
  fModelTotmnM1->Reset();
  fModelTotNDBDM1->Reset();
  fModelTot2NDBDM1->Reset();
  fModelTotbiM1->Reset();
  fModelTotbi2M1->Reset();
  fModelTotptM1->Reset();
  fModelTotpbM1->Reset();
  fModelTotcsM1->Reset();
  fModelTotco2M1->Reset();
  fModelTotteo2M1->Reset();

  fModelTotSthM1->Reset();
  fModelTotSuM1->Reset();
  fModelTotSpoM1->Reset();
  fModelTotSpbM1->Reset();
  fModelTotExtM1->Reset();

  fModelTotAdapM1->Reset();
  fModelTotAdapthM1->Reset();
  fModelTotAdapuM1->Reset();
  fModelTotAdapkM1->Reset();
  fModelTotAdapcoM1->Reset();
  fModelTotAdapmnM1->Reset();
  fModelTotAdapNDBDM1->Reset();
  fModelTotAdap2NDBDM1->Reset();
  fModelTotAdapbiM1->Reset();
  fModelTotAdapbi2M1->Reset();
  fModelTotAdapptM1->Reset();
  fModelTotAdappbM1->Reset();
  fModelTotAdapcsM1->Reset();
  fModelTotAdapco2M1->Reset();
  fModelTotAdapteo2M1->Reset();

  fModelTotAdapSthM1->Reset();
  fModelTotAdapSuM1->Reset();
  fModelTotAdapSpoM1->Reset();
  fModelTotAdapSpbM1->Reset();
  fModelTotAdapExtM1->Reset();

  fModelTotAdapFudgeM1->Reset();

  // Total PDFs M2
  fModelTotM2->Reset();
  fModelTotthM2->Reset();
  fModelTotuM2->Reset();
  fModelTotkM2->Reset();
  fModelTotcoM2->Reset();
  fModelTotmnM2->Reset();
  fModelTotNDBDM2->Reset();
  fModelTot2NDBDM2->Reset();
  fModelTotbiM2->Reset();
  fModelTotbi2M2->Reset();
  fModelTotptM2->Reset();
  fModelTotpbM2->Reset();
  fModelTotcsM2->Reset();
  fModelTotco2M2->Reset();
  fModelTotteo2M2->Reset();

  fModelTotSthM2->Reset();
  fModelTotSuM2->Reset();
  fModelTotSpoM2->Reset();
  fModelTotSpbM2->Reset();
  fModelTotExtM2->Reset();

  fModelTotAdapM2->Reset();
  fModelTotAdapthM2->Reset();
  fModelTotAdapuM2->Reset();
  fModelTotAdapkM2->Reset();
  fModelTotAdapcoM2->Reset();
  fModelTotAdapmnM2->Reset();
  fModelTotAdapNDBDM2->Reset();
  fModelTotAdap2NDBDM2->Reset();
  fModelTotAdapbiM2->Reset();
  fModelTotAdapbi2M2->Reset();
  fModelTotAdapptM2->Reset();
  fModelTotAdappbM2->Reset();
  fModelTotAdapcsM2->Reset();
  fModelTotAdapco2M2->Reset();
  fModelTotAdapteo2M2->Reset();

  fModelTotAdapSthM2->Reset();
  fModelTotAdapSuM2->Reset();
  fModelTotAdapSpoM2->Reset();
  fModelTotAdapSpbM2->Reset();
  fModelTotAdapExtM2->Reset();

  fModelTotAdapFudgeM2->Reset();


  // Total PDFs M2Sum
  fModelTotM2Sum->Reset();
  fModelTotthM2Sum->Reset();
  fModelTotuM2Sum->Reset();
  fModelTotkM2Sum->Reset();
  fModelTotcoM2Sum->Reset();
  fModelTotmnM2Sum->Reset();
  fModelTotNDBDM2Sum->Reset();
  fModelTot2NDBDM2Sum->Reset();
  fModelTotbiM2Sum->Reset();
  fModelTotbi2M2Sum->Reset();
  fModelTotptM2Sum->Reset();
  fModelTotpbM2Sum->Reset();
  fModelTotcsM2Sum->Reset();
  fModelTotco2M2Sum->Reset();
  fModelTotteo2M2Sum->Reset();

  fModelTotSthM2Sum->Reset();
  fModelTotSuM2Sum->Reset();
  fModelTotSpoM2Sum->Reset();
  fModelTotSpbM2Sum->Reset();
  fModelTotExtM2Sum->Reset();

  fModelTotAdapM2Sum->Reset();
  fModelTotAdapthM2Sum->Reset();
  fModelTotAdapuM2Sum->Reset();
  fModelTotAdapkM2Sum->Reset();
  fModelTotAdapcoM2Sum->Reset();
  fModelTotAdapmnM2Sum->Reset();
  fModelTotAdapNDBDM2Sum->Reset();
  fModelTotAdap2NDBDM2Sum->Reset();
  fModelTotAdapbiM2Sum->Reset();
  fModelTotAdapbi2M2Sum->Reset();
  fModelTotAdapptM2Sum->Reset();
  fModelTotAdappbM2Sum->Reset();
  fModelTotAdapcsM2Sum->Reset();
  fModelTotAdapco2M2Sum->Reset();
  fModelTotAdapteo2M2Sum->Reset();

  fModelTotAdapSthM2Sum->Reset();
  fModelTotAdapSuM2Sum->Reset();
  fModelTotAdapSpoM2Sum->Reset();
  fModelTotAdapSpbM2Sum->Reset();
  fModelTotAdapExtM2Sum->Reset();

  fModelTotAdapFudgeM2Sum->Reset();

  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
    fParameters[i] = 0;
    fParError[i] = 0;
  }
}

// Set Parameters in Model
void TBackgroundModel::SetParameters(int index, double value)
{
	// Change the index max depending on model
	if(index > dNParam) cout << "Index too large" << endl;
	else fParameters[index] = value;
}

void TBackgroundModel::SetParEfficiency()
{
   // fParEfficiencyM1[0] = 0.8897514; // TeO2 0nu
   fParEfficiencyM1[0] = 0.9292826; // TeO2 2nu
   fParEfficiencyM1[1] = 0.5614488; // CuBox+Frame co60
   fParEfficiencyM1[2] = 0.941975; // TeO2 th232 only
   fParEfficiencyM1[3] = 0.9419295; // TeO2 th230 only
   fParEfficiencyM1[4] = 0.4158284; // TeO2 Sx th232 0.01
   fParEfficiencyM1[5] = 0.3802244; // TeO2 Sx ra228 to pb208 0.01 
   fParEfficiencyM1[6] = 0.7457738; // TeO2 Sx u238 to th230 0.01  
   fParEfficiencyM1[7] = 0.7379278; // TeO2 Sx th230 only 0.01
   fParEfficiencyM1[8] = 0.5154646; // TeO2 Sx ra226 to pb210 0.01
   fParEfficiencyM1[9] = 0.4958653 ; // TeO2 Sx pb210 1 ==> necessary for bin below Po210 peak in M2
   fParEfficiencyM1[10] = 0.4457218; // TeO2 Sx pb210 0.01 ==> completely necessary for M2 spectrum
   fParEfficiencyM1[11] = 0.1013459; // CuBox+CuFrame Sx th232 10
   fParEfficiencyM1[12] = 0.0881936; // CuBox+CuFrame Sx u238 10
   fParEfficiencyM1[13] = 0.0725138; // CuBox+CuFrame Sx pb210 0.1 ==> useful for below Po210 peak in M1 but doesn't seem absolutely necessary
   fParEfficiencyM1[14] = 0.0742276; // CuBox+CuFrame Sx pb210 0.01 => necessary for below Po210 peak in M1
   fParEfficiencyM1[15] = 0.00484976; // PbRom k40
   fParEfficiencyM1[16] = 0.000465822; // OVC th232
   fParEfficiencyM1[17] = 0.000293556; // OVC u238
   fParEfficiencyM1[18] = 0.01430904; // OVC co60
   fParEfficiencyM1[19] = 0.00043995; // OVC k40
   fParEfficiencyM1[20] = 0.000191901; // External Lead bi210 
   fParEfficiencyM1[21] = 0.0510853; // CuBox+Frame th232 
   fParEfficiencyM1[22] = 0.0354736; // CuBox+Frame u238
   fParEfficiencyM1[23] = 0.0237369; // PbRom cs137

   // fParMass[0] = 39000/1000.; // TeO2 0nu
   fParMass[0] = 39000/1000.; // TeO2 2nu
   fParMass[1] = (2610.04+6929.71)/1000.; // CuBox+Frame co60
   fParMass[2] = 39000/1000.; // TeO2 th232 only
   fParMass[3] = 39000/1000.; // TeO2 th230 only
   fParMass[4] = 39000/1000.; // TeO2 Sx th232 0.01
   fParMass[5] = 39000/1000.; // TeO2 Sx ra228 to pb208 0.01 
   fParMass[6] = 39000/1000.; // TeO2 Sx u238 to th230 0.01  
   fParMass[7] = 39000/1000.; // TeO2 Sx th230 only 0.01
   fParMass[8] = 39000/1000.; // TeO2 Sx ra226 to pb210 0.01
   fParMass[9] = 39000/1000. ; // TeO2 Sx pb210 1 ==> necessary for bin below Po210 peak in M2
   fParMass[10] = 39000/1000.; // TeO2 Sx pb210 0.01 ==> completely necessary for M2 spectrum
   fParMass[11] = (2610.04+6929.71)/1000.; // CuBox+CuFrame Sx th232 10
   fParMass[12] = (2610.04+6929.71)/1000.; // CuBox+CuFrame Sx u238 10
   fParMass[13] = (2610.04+6929.71)/1000.; // CuBox+CuFrame Sx pb210 0.1 ==> useful for below Po210 peak in M1 but doesn't seem absolutely necessary
   fParMass[14] = (2610.04+6929.71)/1000.; // CuBox+CuFrame Sx pb210 0.01 => necessary for below Po210 peak in M1
   fParMass[15] = 202294.46/1000.; // PbRom k40
   fParMass[16] = 180704.38/1000.; // OVC th232
   fParMass[17] = 180704.38/1000.; // OVC u238
   fParMass[18] = 180704.38/1000.; // OVC co60
   fParMass[19] = 180704.38/1000.; // OVC k40
   fParMass[20] = 24652026/1000.; // External Lead bi210 
   fParMass[21] = (2610.04+6929.71)/1000.; // CuBox+Frame th232 
   fParMass[22] = (2610.04+6929.71)/1000.; // CuBox+Frame u238
   fParMass[23] = 202294.46/1000.; // PbRom cs137

   // fParSurfaceArea[0] = 7800.; // TeO2 0nu
   fParSurfaceArea[0] = 7800.; // TeO2 2nu
   fParSurfaceArea[1] = 2314.02+9467.18; // CuBox+Frame co60
   fParSurfaceArea[2] = 7800.; // TeO2 th232 only
   fParSurfaceArea[3] = 7800.; // TeO2 th230 only
   fParSurfaceArea[4] = 7800.; // TeO2 Sx th232 0.01
   fParSurfaceArea[5] = 7800.; // TeO2 Sx ra228 to pb208 0.01 
   fParSurfaceArea[6] = 7800.; // TeO2 Sx u238 to th230 0.01  
   fParSurfaceArea[7] = 7800.; // TeO2 Sx th230 only 0.01
   fParSurfaceArea[8] = 7800.; // TeO2 Sx ra226 to pb210 0.01
   fParSurfaceArea[9] = 7800. ; // TeO2 Sx pb210 1 ==> necessary for bin below Po210 peak in M2
   fParSurfaceArea[10] = 7800.; // TeO2 Sx pb210 0.01 ==> completely necessary for M2 spectrum
   fParSurfaceArea[11] = 2314.02+9467.18; // CuBox+CuFrame Sx th232 10
   fParSurfaceArea[12] = 2314.02+9467.18; // CuBox+CuFrame Sx u238 10
   fParSurfaceArea[13] = 2314.02+9467.18; // CuBox+CuFrame Sx pb210 0.1 ==> useful for below Po210 peak in M1 but doesn't seem absolutely necessary
   fParSurfaceArea[14] = 2314.02+9467.18; // CuBox+CuFrame Sx pb210 0.01 => necessary for below Po210 peak in M1
   fParSurfaceArea[15] = 20898.8; // PbRom k40
   fParSurfaceArea[16] = 87370.2; // OVC th232
   fParSurfaceArea[17] = 87370.2; // OVC u238
   fParSurfaceArea[18] = 87370.2; // OVC co60
   fParSurfaceArea[19] = 87370.2; // OVC k40
   fParSurfaceArea[20] = 2.38E+005; // External Lead bi210 
   fParSurfaceArea[21] = 2314.02+9467.18; // CuBox+Frame th232 
   fParSurfaceArea[22] = 2314.02+9467.18; // CuBox+Frame u238
   fParSurfaceArea[23] = 20898.8; // PbRom cs137
}

// Creates/updates the background model
void TBackgroundModel::UpdateModelAdaptive()
{
  if(fModelTotAdapM1 == NULL || fModelTotAdapM2 == NULL || fModelTotAdapM2Sum == NULL) 
  {
    cout << "Model Histogram Not Created" << endl;
    return;
  }

  // Reset all bins in model histograms in every loop
  fModelTotAdapM1->Reset();
  fModelTotAdapM2->Reset();
  fModelTotAdapM2Sum->Reset();

  dNumCalls++;
  if(dNumCalls%1000==0)
  {
    cout << "Call #: "<< dNumCalls << endl;
  }

/////// Create model
/////// M1
  // fModelTotAdapM1->Add( hAdapTeO20nuM1,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM1->Add( hAdapTeO22nuM1,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM1->Add( hAdapCuBox_CuFrameco60M1,             dDataIntegralM1*fParameters[1]);
  fModelTotAdapM1->Add( hAdapTeO2th232onlyM1,        dDataIntegralM1*fParameters[2]);
  fModelTotAdapM1->Add( hAdapTeO2th230onlyM1,        dDataIntegralM1*fParameters[3]);
  fModelTotAdapM1->Add( hAdapTeO2Sxth232onlyM1_001,      dDataIntegralM1*fParameters[4]);
  fModelTotAdapM1->Add( hAdapTeO2Sxra228pb208M1_001, dDataIntegralM1*fParameters[5]);
  fModelTotAdapM1->Add( hAdapTeO2Sxu238th230M1_001,  dDataIntegralM1*fParameters[6]);
  fModelTotAdapM1->Add( hAdapTeO2Sxth230onlyM1_001,  dDataIntegralM1*fParameters[7]);
  fModelTotAdapM1->Add( hAdapTeO2Sxra226pb210M1_001, dDataIntegralM1*fParameters[8]);
  fModelTotAdapM1->Add( hAdapTeO2Sxpb210M1_1,        dDataIntegralM1*fParameters[9]);
  fModelTotAdapM1->Add( hAdapTeO2Sxpb210M1_001,      dDataIntegralM1*fParameters[10]);

  fModelTotAdapM1->Add( hAdapCuBox_CuFrameth232M1_10,   dDataIntegralM1*fParameters[11]);
  fModelTotAdapM1->Add( hAdapCuBox_CuFrameu238M1_10,    dDataIntegralM1*fParameters[12]);
  fModelTotAdapM1->Add( hAdapCuBox_CuFramepb210M1_01,   dDataIntegralM1*fParameters[13]);
  fModelTotAdapM1->Add( hAdapCuBox_CuFramepb210M1_001,  dDataIntegralM1*fParameters[14]);

  fModelTotAdapM1->Add( hAdapPbRomk40M1,       dDataIntegralM1*fParameters[15]);
  fModelTotAdapM1->Add( hAdapOVCth232M1,     dDataIntegralM1*fParameters[16]);
  fModelTotAdapM1->Add( hAdapOVCu238M1,      dDataIntegralM1*fParameters[17]);
  fModelTotAdapM1->Add( hAdapOVCco60M1,      dDataIntegralM1*fParameters[18]);
  fModelTotAdapM1->Add( hAdapOVCk40M1,       dDataIntegralM1*fParameters[19]);
  fModelTotAdapM1->Add( hAdapExtPbbi210M1,   dDataIntegralM1*fParameters[20]);

  fModelTotAdapM1->Add( hAdapCuBox_CuFrameth232M1,     dDataIntegralM1*fParameters[21]);
  fModelTotAdapM1->Add( hAdapCuBox_CuFrameu238M1,      dDataIntegralM1*fParameters[22]);
  fModelTotAdapM1->Add( hAdapPbRomcs137M1,             dDataIntegralM1*fParameters[23]);

  fModelTotAdapM1->Add( hAdapTeO2Sxth232M1_1,    dDataIntegralM1*fParameters[24]);
  fModelTotAdapM1->Add( hAdapTeO2Sxth232M1_10,    dDataIntegralM1*fParameters[25]);
  fModelTotAdapM1->Add( hAdapTeO2Sxu238M1_1,     dDataIntegralM1*fParameters[26]);
  fModelTotAdapM1->Add( hAdapTeO2Sxu238M1_10,     dDataIntegralM1*fParameters[27]);
  fModelTotAdapM1->Add( hAdapTeO2Sxpb210M1_10,     dDataIntegralM1*fParameters[28]);

/////// M2
  // fModelTotAdapM2->Add( hAdapTeO20nuM2,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM2->Add( hAdapTeO22nuM2,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM2->Add( hAdapCuBox_CuFrameco60M2,             dDataIntegralM1*fParameters[1]);
  fModelTotAdapM2->Add( hAdapTeO2th232onlyM2,        dDataIntegralM1*fParameters[2]);
  fModelTotAdapM2->Add( hAdapTeO2th230onlyM2,        dDataIntegralM1*fParameters[3]);
  fModelTotAdapM2->Add( hAdapTeO2Sxth232onlyM2_001,      dDataIntegralM1*fParameters[4]);
  fModelTotAdapM2->Add( hAdapTeO2Sxra228pb208M2_001, dDataIntegralM1*fParameters[5]);
  fModelTotAdapM2->Add( hAdapTeO2Sxu238th230M2_001,  dDataIntegralM1*fParameters[6]);
  fModelTotAdapM2->Add( hAdapTeO2Sxth230onlyM2_001,  dDataIntegralM1*fParameters[7]);
  fModelTotAdapM2->Add( hAdapTeO2Sxra226pb210M2_001, dDataIntegralM1*fParameters[8]);
  fModelTotAdapM2->Add( hAdapTeO2Sxpb210M2_1,        dDataIntegralM1*fParameters[9]);
  fModelTotAdapM2->Add( hAdapTeO2Sxpb210M2_001,      dDataIntegralM1*fParameters[10]);

  fModelTotAdapM2->Add( hAdapCuBox_CuFrameth232M2_10,   dDataIntegralM1*fParameters[11]);
  fModelTotAdapM2->Add( hAdapCuBox_CuFrameu238M2_10,    dDataIntegralM1*fParameters[12]);
  fModelTotAdapM2->Add( hAdapCuBox_CuFramepb210M2_01,   dDataIntegralM1*fParameters[13]);
  fModelTotAdapM2->Add( hAdapCuBox_CuFramepb210M2_001,  dDataIntegralM1*fParameters[14]);


  fModelTotAdapM2->Add( hAdapPbRomk40M2,       dDataIntegralM1*fParameters[15]);
  fModelTotAdapM2->Add( hAdapOVCth232M2,     dDataIntegralM1*fParameters[16]);
  fModelTotAdapM2->Add( hAdapOVCu238M2,      dDataIntegralM1*fParameters[17]);
  fModelTotAdapM2->Add( hAdapOVCco60M2,      dDataIntegralM1*fParameters[18]);
  fModelTotAdapM2->Add( hAdapOVCk40M2,       dDataIntegralM1*fParameters[19]);
  fModelTotAdapM2->Add( hAdapExtPbbi210M2,   dDataIntegralM1*fParameters[20]);

  fModelTotAdapM2->Add( hAdapCuBox_CuFrameth232M2,     dDataIntegralM1*fParameters[21]);
  fModelTotAdapM2->Add( hAdapCuBox_CuFrameu238M2,      dDataIntegralM1*fParameters[22]);
  fModelTotAdapM2->Add( hAdapPbRomcs137M2,             dDataIntegralM1*fParameters[23]);

  fModelTotAdapM2->Add( hAdapTeO2Sxth232M2_1,    dDataIntegralM1*fParameters[24]);
  fModelTotAdapM2->Add( hAdapTeO2Sxth232M2_10,    dDataIntegralM1*fParameters[25]);
  fModelTotAdapM2->Add( hAdapTeO2Sxu238M2_1,     dDataIntegralM1*fParameters[26]);
  fModelTotAdapM2->Add( hAdapTeO2Sxu238M2_10,     dDataIntegralM1*fParameters[27]);
  fModelTotAdapM2->Add( hAdapTeO2Sxpb210M2_10,     dDataIntegralM1*fParameters[28]);

/////// M2Sum
  // fModelTotAdapM2Sum->Add( hAdapTeO20nuM2Sum,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM2Sum->Add( hAdapTeO22nuM2Sum,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFrameco60M2Sum,             dDataIntegralM1*fParameters[1]);
  fModelTotAdapM2Sum->Add( hAdapTeO2th232onlyM2Sum,        dDataIntegralM1*fParameters[2]);
  fModelTotAdapM2Sum->Add( hAdapTeO2th230onlyM2Sum,        dDataIntegralM1*fParameters[3]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxth232onlyM2Sum_001,      dDataIntegralM1*fParameters[4]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxra228pb208M2Sum_001, dDataIntegralM1*fParameters[5]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxu238th230M2Sum_001,  dDataIntegralM1*fParameters[6]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxth230onlyM2Sum_001,  dDataIntegralM1*fParameters[7]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxra226pb210M2Sum_001, dDataIntegralM1*fParameters[8]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxpb210M2Sum_1,        dDataIntegralM1*fParameters[9]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxpb210M2Sum_001,      dDataIntegralM1*fParameters[10]);

  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFrameth232M2Sum_10,   dDataIntegralM1*fParameters[11]);
  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFrameu238M2Sum_10,    dDataIntegralM1*fParameters[12]);
  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFramepb210M2Sum_01,   dDataIntegralM1*fParameters[13]);
  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFramepb210M2Sum_001,  dDataIntegralM1*fParameters[14]);


  fModelTotAdapM2Sum->Add( hAdapPbRomk40M2Sum,       dDataIntegralM1*fParameters[15]);
  fModelTotAdapM2Sum->Add( hAdapOVCth232M2Sum,     dDataIntegralM1*fParameters[16]);
  fModelTotAdapM2Sum->Add( hAdapOVCu238M2Sum,      dDataIntegralM1*fParameters[17]);
  fModelTotAdapM2Sum->Add( hAdapOVCco60M2Sum,      dDataIntegralM1*fParameters[18]);
  fModelTotAdapM2Sum->Add( hAdapOVCk40M2Sum,       dDataIntegralM1*fParameters[19]);
  fModelTotAdapM2Sum->Add( hAdapExtPbbi210M2Sum,   dDataIntegralM1*fParameters[20]);

  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFrameth232M2Sum,     dDataIntegralM1*fParameters[21]);
  fModelTotAdapM2Sum->Add( hAdapCuBox_CuFrameu238M2Sum,      dDataIntegralM1*fParameters[22]);
  fModelTotAdapM2Sum->Add( hAdapPbRomcs137M2Sum,             dDataIntegralM1*fParameters[23]);

  fModelTotAdapM2Sum->Add( hAdapTeO2Sxth232M2Sum_1,    dDataIntegralM1*fParameters[24]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxth232M2Sum_10,    dDataIntegralM1*fParameters[25]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxu238M2Sum_1,     dDataIntegralM1*fParameters[26]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxu238M2Sum_10,     dDataIntegralM1*fParameters[27]);
  fModelTotAdapM2Sum->Add( hAdapTeO2Sxpb210M2Sum_10,     dDataIntegralM1*fParameters[28]);

}
  
// This method sets up minuit and does the fit
bool TBackgroundModel::DoTheFitAdaptive(double f2nuValue, double fVariableValue)
{ 
  // Reset initial parameter/error values
  ResetParameters();

  gStyle->SetOptStat(0);
  // gStyle->SetOptTitle(0);
  gStyle->SetOptFit();

  // Reduce Minuit Output
  minuit->SetPrintLevel(0); // Print level -1 (Quiet), 0 (Normal), 1 (Verbose)
  minuit->Command("SET STRategy 2"); // Sets strategy of fit
  minuit->SetMaxIterations(10000);
  minuit->SetObjectFit(this); //see the external FCN  above


  // Without Initial Values
  // minuit->DefineParameter(0, "TeO2 0nu",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(0, "TeO2 2nu",  f2nuValue, 1E-7, 0, 1.0);
  minuit->DefineParameter(1, "CuBox + CuFrame co60",  0., 1E-7, 0, 1.0);
  // minuit->DefineParameter(3, "CuBox + CuFrame k40",  0, 1E-7, 0, 1.0);
  // minuit->DefineParameter(2, "TeO2 th232 only", 0.000299799, 1E-7, 0, 1.0);
  // minuit->DefineParameter(3, "TeO2 th230 only", 0.000321558, 1E-7, 0, 1.0);
  minuit->DefineParameter(2, "TeO2 th232 only", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(3, "TeO2 th230 only", 0., 1E-7, 0, 1.0);  
  minuit->DefineParameter(4, "TeO2 Sx th232 only 0.01", 0.00160538, 1E-7, 0, 1.0);
  minuit->DefineParameter(5, "TeO2 Sx ra228 to pb208 0.01", 0.00253029, 1E-7, 0, 1.0);
  minuit->DefineParameter(6, "TeO2 Sx u238 to th230 0.01", 0.00175202, 1E-7, 0, 1.0);
  minuit->DefineParameter(7, "TeO2 Sx th230 only 0.01", 0.000779451, 1E-7, 0, 1.0);
  minuit->DefineParameter(8, "TeO2 Sx ra226 to pb210 0.01", 0.00300571, 1E-7, 0, 1.0);
  // minuit->DefineParameter(11, "TeO2 Sx pb210 1", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(9, "TeO2 Sx pb210 1", 0.00536034, 1E-7, 0, 1.0);
  // minuit->DefineParameter(12, "TeO2 Sx pb210 0.01", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(10, "TeO2 Sx pb210 0.01", 0.0420915, 1E-7, 0, 1.0);
  // minuit->DefineParameter(11, "CuBox + CuFrame Sx th232 10", 0.0127220, 1E-7, 0, 1.0);
  minuit->DefineParameter(11, "CuBox + CuFrame Sx th232 10", 0., 1E-7, 0, 1.0);
  // minuit->DefineParameter(12, "CuBox + CuFrame Sx u238 10 ", 0.00682647, 1E-7, 0, 1.0);
  minuit->DefineParameter(12, "CuBox + CuFrame Sx u238 10 ", 0., 1E-7, 0, 1.0);
  // minuit->DefineParameter(13, "CuBox + CuFrame Sx pb210 0.1", 0.00562587, 1E-7, 0, 1.0);
  minuit->DefineParameter(13, "CuBox + CuFrame Sx pb210 0.1", 0, 1E-7, 0, 1.0);
  // minuit->DefineParameter(14, "CuBox + CuFrame Sx pb210 0.01", 0.0171725, 1E-7, 0, 1.0);
  minuit->DefineParameter(14, "CuBox + CuFrame Sx pb210 0.01", 0., 1E-7, 0, 1.0);

  // minuit->DefineParameter(15, "PbRom k40",  4.41512e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(15, "PbRom k40",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(16, "OVC th232",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(17, "OVC u238",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(18, "OVC co60",  0., 1E-7, 0, 1.0);    
  minuit->DefineParameter(19, "OVC k40",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(20, "External Lead bi210", fVariableValue, 1E-7, 0, 1.0);

  // minuit->DefineParameter(21, "CuBox + CuFrame th232",  1.98125E-2, 1E-7, 0, 1.0);
  minuit->DefineParameter(21, "CuBox + CuFrame th232",  0., 1E-7, 0, 1.0);
  // minuit->DefineParameter(22, "CuBox + CuFrame u238",  4.74432E-3, 1E-7, 0, 1.0);
  minuit->DefineParameter(22, "CuBox + CuFrame u238",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(23, "PbRom cs137",  0, 1E-7, 0, 1.0);

  minuit->DefineParameter(24, "TeO2 Sx th232 1", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(25, "TeO2 Sx th232 10", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(26, "TeO2 Sx u238 1", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(27, "TeO2 Sx u238 10", 0., 1E-7, 0, 1.0);
  minuit->DefineParameter(28, "TeO2 Sx pb210 10", 0., 1E-7, 0, 1.0);


/*
  // With Initial Values and 0nu
  minuit->DefineParameter(0, "TeO2 0nu",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(1, "TeO2 2nu",  f2nuValue, 1E-7, 0, 1.0);
  minuit->DefineParameter(2, "CuBox + CuFrame co60",  0., 1E-7, 0, 1.0);
  minuit->DefineParameter(3, "TeO2 th232 only", 3.1092e-04, 1E-7, 0, 1.0);
  minuit->DefineParameter(4, "TeO2 th230 only", 3.5312e-04, 1E-7, 0, 1.0);
  minuit->DefineParameter(5, "TeO2 Sx th232 0.01", 1.5038e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(6, "TeO2 Sx ra228 to pb208 0.01", 2.7271e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(7, "TeO2 Sx u238 to th230 0.01", 1.7670e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(8, "TeO2 Sx th230 only 0.01", 7.2627e-04, 1E-7, 0, 1.0);
  minuit->DefineParameter(9, "TeO2 Sx ra226 to pb210 0.01", 3.0098e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(10, "TeO2 Sx pb210 1", 5.8877e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(11, "TeO2 Sx pb210 0.01", 4.1736e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(12, "CuBox + CuFrame Sx th232 10", 1.3054e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(13, "CuBox + CuFrame Sx u238 10 ", 6.0835e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(14, "CuBox + CuFrame Sx pb210 0.1", 5.4798e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(15, "CuBox + CuFrame Sx pb210 0.01", 1.7325e-02, 1E-7, 0, 1.0);

  minuit->DefineParameter(16, "PbRom k40",  4.1791e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(17, "OVC th232",  8.8577e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(18, "OVC u238",  1.2342e-01, 1E-7, 0, 1.0);
  minuit->DefineParameter(19, "OVC co60",  1.8194e-02, 1E-7, 0, 1.0);    
  minuit->DefineParameter(20, "OVC k40",  6.3955e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(21, "External Lead bi210", fVariableValue, 1E-7, 0, 1.0);

  minuit->DefineParameter(22, "CuBox + CuFrame th232",   1.9420e-02, 1E-7, 0, 1.0);
  minuit->DefineParameter(23, "CuBox + CuFrame u238",  4.0567e-03, 1E-7, 0, 1.0);
  minuit->DefineParameter(24, "PbRom cs137",  2.2591e-03, 1E-7, 0, 1.0);
*/

//////////////////////////////////////

   // Uncommend to fix parameters here
   minuit->FixParameter(0); // TeO2 2nu
   minuit->FixParameter(1); // CuBox+Frame co60
   // minuit->FixParameter(2); // TeO2 th232 only
   // minuit->FixParameter(3); // TeO2 th230 only
   // minuit->FixParameter(4); // TeO2 Sx th232 only 0.01
   // minuit->FixParameter(5); // TeO2 Sx ra228 to pb208 0.01 
   // minuit->FixParameter(6); // TeO2 Sx u238 to th230 0.01  
   // minuit->FixParameter(7); // TeO2 Sx th230 only 0.01
   // minuit->FixParameter(8); // TeO2 Sx ra226 to pb210 0.01
   // minuit->FixParameter(9); // TeO2 Sx pb210 1 ==> necessary for bin below Po210 peak in M2
   // minuit->FixParameter(10); // TeO2 Sx pb210 0.01 ==> completely necessary for M2 spectrum
   // minuit->FixParameter(11); // CuBox+CuFrame Sx th232 10
   // minuit->FixParameter(12); // CuBox+CuFrame Sx u238 10
   // minuit->FixParameter(13); // CuBox+CuFrame Sx pb210 0.1 ==> useful for below Po210 peak in M1 but doesn't seem absolutely necessary
   // minuit->FixParameter(14); // CuBox+CuFrame Sx pb210 0.01 => necessary for below Po210 peak in M1
   minuit->FixParameter(15); // PbRom 
   minuit->FixParameter(16); // OVC th232
   minuit->FixParameter(17); // OVC u238
   minuit->FixParameter(18); // OVC co60
   minuit->FixParameter(19); // OVC k40
   minuit->FixParameter(20); // External Lead bi210 
   minuit->FixParameter(21); // CuBox+Frame th232 
   minuit->FixParameter(22); // CuBox+Frame u238
   minuit->FixParameter(23); // PbRom cs137

   minuit->FixParameter(24); // TeO2 Sx th232 1
   // minuit->FixParameter(25); // TeO2 Sx th232 10 
   // minuit->FixParameter(26); // TeO2 Sx u238 1
   minuit->FixParameter(27); // TeO2 Sx u238 10
   minuit->FixParameter(28); // TeO2 Sx pb210 10


   // Number of free Parameters (for Chi-squared/NDF calculation only)
   dNumFreeParameters = minuit->GetNumPars() - minuit->GetNumFixedPars();
   dNDF = (dFitMaxBinM1-dFitMinBinM1-dNumFreeParameters); // M1 only
   // dNDF = (dFitMaxBinM2-dFitMinBinM2-dNumFreeParameters); // M2 only
   // dNDF = (dFitMaxBinM1+dFitMaxBinM2-dFitMinBinM1-dFitMinBinM2-dNumFreeParameters);
   // dNDF = (dFitMaxBinM1+dFitMaxBinM2+dFitMaxBinM2Sum - dFitMinBinM1-dFitMinBinM2-dFitMinBinM2Sum - dNumFreeParameters);
   // Tell minuit what external function to use 
   minuit->SetFCN(myExternal_FCNAdap);
   int status = minuit->Command("MINImize 500000 0.1"); // Command that actually does the minimization

  // Get final parameters from fit
  for(int i = 0; i < dNParam; i++)
  {
    minuit->GetParameter(i, fParameters[i], fParError[i]);
  }

  // Update model with final parameters
  UpdateModelAdaptive();
  
  dChiSquare = GetChiSquareAdaptive();

  // cout << "Total number of calls = " << dNumCalls << "\t" << "ChiSq/NDF = " << dChiSquare/dNDF << endl; // for M1 and M2
  // cout << "ChiSq = " << dChiSquare << "\t" << "NDF = " << dNDF << endl;
  // cout << "Probability = " << TMath::Prob(dChiSquare, dNDF ) << endl;

// M1
  // fModelTotAdapNDBDM1->Add( hAdapTeO20nuM1,              dDataIntegralM1*fParameters[0]);
  fModelTotAdap2NDBDM1->Add( hAdapTeO22nuM1,              dDataIntegralM1*fParameters[0]);
  fModelTotAdapcoM1->Add( hAdapCuBox_CuFrameco60M1,             dDataIntegralM1*fParameters[1]);
  fModelTotAdapthM1->Add( hAdapTeO2th232onlyM1,        dDataIntegralM1*fParameters[2]);
  fModelTotAdapuM1->Add( hAdapTeO2th230onlyM1,        dDataIntegralM1*fParameters[3]);
  fModelTotAdapthM1->Add( hAdapTeO2Sxth232onlyM1_001,   dDataIntegralM1*fParameters[4]);
  fModelTotAdapthM1->Add( hAdapTeO2Sxra228pb208M1_001, dDataIntegralM1*fParameters[5]);
  fModelTotAdapuM1->Add( hAdapTeO2Sxu238th230M1_001,  dDataIntegralM1*fParameters[6]);
  fModelTotAdapuM1->Add( hAdapTeO2Sxth230onlyM1_001,  dDataIntegralM1*fParameters[7]);
  fModelTotAdapuM1->Add( hAdapTeO2Sxra226pb210M1_001, dDataIntegralM1*fParameters[8]);
  fModelTotAdapSpbM1->Add( hAdapTeO2Sxpb210M1_1,     dDataIntegralM1*fParameters[9]);
  fModelTotAdapSpbM1->Add( hAdapTeO2Sxpb210M1_001,     dDataIntegralM1*fParameters[10]);

  fModelTotAdapthM1->Add( hAdapCuBox_CuFrameth232M1_10,  dDataIntegralM1*fParameters[11]);
  fModelTotAdapuM1->Add( hAdapCuBox_CuFrameu238M1_10,   dDataIntegralM1*fParameters[12]);
  fModelTotAdapSpbM1->Add( hAdapCuBox_CuFramepb210M1_01,  dDataIntegralM1*fParameters[13]);
  fModelTotAdapSpbM1->Add( hAdapCuBox_CuFramepb210M1_001,  dDataIntegralM1*fParameters[14]);


  fModelTotAdapkM1->Add( hAdapPbRomk40M1,      dDataIntegralM1*fParameters[15]);
  fModelTotAdapthM1->Add( hAdapOVCth232M1,    dDataIntegralM1*fParameters[16]);
  fModelTotAdapuM1->Add( hAdapOVCu238M1,     dDataIntegralM1*fParameters[17]);
  fModelTotAdapcoM1->Add( hAdapOVCco60M1,     dDataIntegralM1*fParameters[18]);
  fModelTotAdapkM1->Add( hAdapOVCk40M1,      dDataIntegralM1*fParameters[19]);
  fModelTotAdapbiM1->Add( hAdapExtPbbi210M1,        dDataIntegralM1*fParameters[20]);
  fModelTotAdapthM1->Add( hAdapCuBox_CuFrameth232M1,    dDataIntegralM1*fParameters[21]);
  fModelTotAdapuM1->Add( hAdapCuBox_CuFrameu238M1,     dDataIntegralM1*fParameters[22]);
  fModelTotAdapFudgeM1->Add( hAdapPbRomcs137M1,        dDataIntegralM1*fParameters[23]);

  fModelTotAdapthM1->Add( hAdapTeO2Sxth232M1_1,    dDataIntegralM1*fParameters[24]);
  fModelTotAdapthM1->Add( hAdapTeO2Sxth232M1_10,    dDataIntegralM1*fParameters[25]);
  fModelTotAdapuM1->Add( hAdapTeO2Sxu238M1_1,     dDataIntegralM1*fParameters[26]);
  fModelTotAdapuM1->Add( hAdapTeO2Sxu238M1_10,     dDataIntegralM1*fParameters[27]);
  fModelTotAdapSpbM1->Add( hAdapTeO2Sxpb210M1_10,     dDataIntegralM1*fParameters[28]);

// M2
  // fModelTotAdapNDBDM2->Add( hAdapTeO20nuM2,              dDataIntegralM2*fParameters[0]);
  fModelTotAdap2NDBDM2->Add( hAdapTeO22nuM2,              dDataIntegralM2*fParameters[0]);
  fModelTotAdapcoM2->Add( hAdapCuBox_CuFrameco60M2,             dDataIntegralM2*fParameters[1]);
  fModelTotAdapthM2->Add( hAdapTeO2th232onlyM2,        dDataIntegralM2*fParameters[2]);
  fModelTotAdapuM2->Add( hAdapTeO2th230onlyM2,        dDataIntegralM2*fParameters[3]);
  fModelTotAdapthM2->Add( hAdapTeO2Sxth232onlyM2_001,   dDataIntegralM2*fParameters[4]);
  fModelTotAdapthM2->Add( hAdapTeO2Sxra228pb208M2_001, dDataIntegralM2*fParameters[5]);
  fModelTotAdapuM2->Add( hAdapTeO2Sxu238th230M2_001,  dDataIntegralM2*fParameters[6]);
  fModelTotAdapuM2->Add( hAdapTeO2Sxth230onlyM2_001,  dDataIntegralM2*fParameters[7]);
  fModelTotAdapuM2->Add( hAdapTeO2Sxra226pb210M2_001, dDataIntegralM2*fParameters[8]);
  fModelTotAdapSpbM2->Add( hAdapTeO2Sxpb210M2_1,     dDataIntegralM2*fParameters[9]);
  fModelTotAdapSpbM2->Add( hAdapTeO2Sxpb210M2_001,     dDataIntegralM2*fParameters[10]);

  fModelTotAdapthM2->Add( hAdapCuBox_CuFrameth232M2_10,  dDataIntegralM2*fParameters[11]);
  fModelTotAdapuM2->Add( hAdapCuBox_CuFrameu238M2_10,   dDataIntegralM2*fParameters[12]);
  fModelTotAdapSpbM2->Add( hAdapCuBox_CuFramepb210M2_01,  dDataIntegralM2*fParameters[13]);
  fModelTotAdapSpbM2->Add( hAdapCuBox_CuFramepb210M2_001,  dDataIntegralM2*fParameters[14]);


  fModelTotAdapkM2->Add( hAdapPbRomk40M2,      dDataIntegralM2*fParameters[15]);
  fModelTotAdapthM2->Add( hAdapOVCth232M2,    dDataIntegralM2*fParameters[16]);
  fModelTotAdapuM2->Add( hAdapOVCu238M2,     dDataIntegralM2*fParameters[17]);
  fModelTotAdapcoM2->Add( hAdapOVCco60M2,     dDataIntegralM2*fParameters[18]);
  fModelTotAdapkM2->Add( hAdapOVCk40M2,      dDataIntegralM2*fParameters[19]);
  fModelTotAdapbiM2->Add( hAdapExtPbbi210M2,        dDataIntegralM2*fParameters[20]);
  fModelTotAdapthM2->Add( hAdapCuBox_CuFrameth232M2,    dDataIntegralM2*fParameters[21]);
  fModelTotAdapuM2->Add( hAdapCuBox_CuFrameu238M2,     dDataIntegralM2*fParameters[22]);
  fModelTotAdapFudgeM2->Add( hAdapPbRomcs137M2,        dDataIntegralM2*fParameters[23]);

  fModelTotAdapthM2->Add( hAdapTeO2Sxth232M2_1,    dDataIntegralM2*fParameters[24]);
  fModelTotAdapthM2->Add( hAdapTeO2Sxth232M2_10,    dDataIntegralM2*fParameters[25]);
  fModelTotAdapuM2->Add( hAdapTeO2Sxu238M2_1,     dDataIntegralM2*fParameters[26]);
  fModelTotAdapuM2->Add( hAdapTeO2Sxu238M2_10,     dDataIntegralM2*fParameters[27]);
  fModelTotAdapSpbM2->Add( hAdapTeO2Sxpb210M2_10,     dDataIntegralM2*fParameters[28]);


// M2Sum
  // fModelTotAdapNDBDM2Sum->Add( hAdapTeO20nuM2Sum,              dDataIntegralM2Sum*fParameters[0]);
  fModelTotAdap2NDBDM2Sum->Add( hAdapTeO22nuM2Sum,              dDataIntegralM2Sum*fParameters[0]);
  fModelTotAdapcoM2Sum->Add( hAdapCuBox_CuFrameco60M2Sum,             dDataIntegralM2Sum*fParameters[1]);
  fModelTotAdapthM2Sum->Add( hAdapTeO2th232onlyM2Sum,        dDataIntegralM2Sum*fParameters[2]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2th230onlyM2Sum,        dDataIntegralM2Sum*fParameters[3]);
  fModelTotAdapthM2Sum->Add( hAdapTeO2Sxth232onlyM2Sum_001,   dDataIntegralM2Sum*fParameters[4]);
  fModelTotAdapthM2Sum->Add( hAdapTeO2Sxra228pb208M2Sum_001, dDataIntegralM2Sum*fParameters[5]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2Sxu238th230M2Sum_001,  dDataIntegralM2Sum*fParameters[6]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2Sxth230onlyM2Sum_001,  dDataIntegralM2Sum*fParameters[7]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2Sxra226pb210M2Sum_001, dDataIntegralM2Sum*fParameters[8]);
  fModelTotAdapSpbM2Sum->Add( hAdapTeO2Sxpb210M2Sum_1,     dDataIntegralM2Sum*fParameters[9]);
  fModelTotAdapSpbM2Sum->Add( hAdapTeO2Sxpb210M2Sum_001,     dDataIntegralM2Sum*fParameters[10]);

  fModelTotAdapthM2Sum->Add( hAdapCuBox_CuFrameth232M2Sum_10,  dDataIntegralM2Sum*fParameters[11]);
  fModelTotAdapuM2Sum->Add( hAdapCuBox_CuFrameu238M2Sum_10,   dDataIntegralM2Sum*fParameters[12]);
  fModelTotAdapSpbM2Sum->Add( hAdapCuBox_CuFramepb210M2Sum_01,  dDataIntegralM2Sum*fParameters[13]);
  fModelTotAdapSpbM2Sum->Add( hAdapCuBox_CuFramepb210M2Sum_001,  dDataIntegralM2Sum*fParameters[14]);


  fModelTotAdapkM2Sum->Add( hAdapPbRomk40M2Sum,      dDataIntegralM2Sum*fParameters[15]);
  fModelTotAdapthM2Sum->Add( hAdapOVCth232M2Sum,    dDataIntegralM2Sum*fParameters[16]);
  fModelTotAdapuM2Sum->Add( hAdapOVCu238M2Sum,     dDataIntegralM2Sum*fParameters[17]);
  fModelTotAdapcoM2Sum->Add( hAdapOVCco60M2Sum,     dDataIntegralM2Sum*fParameters[18]);
  fModelTotAdapkM2Sum->Add( hAdapOVCk40M2Sum,      dDataIntegralM2Sum*fParameters[19]);
  fModelTotAdapbiM2Sum->Add( hAdapExtPbbi210M2Sum,        dDataIntegralM2Sum*fParameters[20]);  
  fModelTotAdapthM2Sum->Add( hAdapCuBox_CuFrameth232M2Sum,    dDataIntegralM2Sum*fParameters[21]);
  fModelTotAdapuM2Sum->Add( hAdapCuBox_CuFrameu238M2Sum,     dDataIntegralM2Sum*fParameters[22]);
  fModelTotAdapFudgeM2Sum->Add( hAdapPbRomcs137M2Sum,        dDataIntegralM2Sum*fParameters[23]);

  fModelTotAdapthM2Sum->Add( hAdapTeO2Sxth232M2Sum_1,    dDataIntegralM2Sum*fParameters[24]);
  fModelTotAdapthM2Sum->Add( hAdapTeO2Sxth232M2Sum_10,    dDataIntegralM2Sum*fParameters[25]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2Sxu238M2Sum_1,     dDataIntegralM2Sum*fParameters[26]);
  fModelTotAdapuM2Sum->Add( hAdapTeO2Sxu238M2Sum_10,     dDataIntegralM2Sum*fParameters[27]);
  fModelTotAdapSpbM2Sum->Add( hAdapTeO2Sxpb210M2Sum_10,     dDataIntegralM2Sum*fParameters[28]);

  // ///// Draw Data M1
  fAdapDataHistoM1->SetLineColor(kBlack);
  fAdapDataHistoM1->SetLineWidth(2);
  // fAdapDataHistoM1->SetMaximum(90000);
  // fAdapDataHistoM1->GetXaxis()->SetRange(1, fAdapDataHistoM1->FindBin(3000));


  fModelTotAdapM1->SetLineColor(2);
  fModelTotAdapM1->SetLineWidth(1);
  fModelTotAdapthM1->SetLineColor(3);
  fModelTotAdapthM1->SetLineStyle(2);
  fModelTotAdapuM1->SetLineColor(4);
  fModelTotAdapuM1->SetLineStyle(2);
  fModelTotAdapkM1->SetLineColor(6);
  fModelTotAdapkM1->SetLineStyle(2);
  fModelTotAdapcoM1->SetLineColor(7);
  fModelTotAdapcoM1->SetLineStyle(2);
  fModelTotAdapNDBDM1->SetLineColor(42);
  fModelTotAdapNDBDM1->SetLineStyle(2);
  fModelTotAdap2NDBDM1->SetLineColor(46);
  fModelTotAdap2NDBDM1->SetLineStyle(2);
  fModelTotAdapbiM1->SetLineColor(5);
  fModelTotAdapbiM1->SetLineStyle(2);

  fModelTotAdapSpoM1->SetLineStyle(2);
  fModelTotAdapSpoM1->SetLineColor(44);

  fModelTotAdapSpbM1->SetLineStyle(2);
  fModelTotAdapSpbM1->SetLineColor(8);

  // fModelTotAdapteo2M1->Draw("SAME");
  // fModelTotAdapPbM1->Draw("SAME");
  fModelTotAdapFudgeM1->SetLineStyle(2);
  fModelTotAdapFudgeM1->SetLineColor(kRed+1);


  TLegend *legfit1 = new TLegend(0.7,0.7,0.95,0.92);
  legfit1->SetFillColor(0);
  legfit1->SetTextSize(0.02);
  legfit1->AddEntry(fModelTotAdapM1, "Total PDF", "l");
  legfit1->AddEntry(fModelTotAdapthM1, "Total th-232", "l");
  legfit1->AddEntry(fModelTotAdapuM1, "Total u-238", "l");
  legfit1->AddEntry(fModelTotAdapkM1, "Total k-40", "l");
  legfit1->AddEntry(fModelTotAdapcoM1, "Total co-60", "l");
  // legfit1->AddEntry(fModelTotAdapNDBDM1, "NDBD", "l");
  legfit1->AddEntry(fModelTotAdap2NDBDM1, "2NDBD", "l");
  legfit1->AddEntry(fModelTotAdapSpbM1, "Surface pb-210", "l");
  legfit1->AddEntry(fModelTotAdapbiM1, "Total External bi-210", "l");  
  legfit1->AddEntry(fModelTotAdapFudgeM1, "Total cs-137", "l");

  int nHisto = 2;  
  double width1 = 0.02;
  double width2 = 0.98;
  double canBotMargin = 0.02;
  double canTopMargin = 0.02;
  double padHeight = (1.-canTopMargin-canBotMargin)/nHisto;


  TCanvas *cadap1 = new TCanvas("cadap1", "cadap1", 1200, 800);
  TPad* p1m1 = new TPad("p1m1","p1m1",width1,canBotMargin,width2,canBotMargin+padHeight/1.5,0,0);
  p1m1->SetTopMargin(0.);
  p1m1->SetBottomMargin(0.175);
  p1m1->SetLeftMargin(0.1);
  p1m1->SetRightMargin(0.05);
  p1m1->SetFillColor(0);
  p1m1->SetBorderMode(0);
  p1m1->SetBorderSize(0);
  p1m1->Draw();
  
  // p2 is on top!
  TPad* p2m1 = new TPad("p2m1","p2m1",width1,canBotMargin+padHeight/1.5,width2,canBotMargin+2*padHeight,0,0);
  p2m1->SetBottomMargin(0.);
  p2m1->SetTopMargin(0.08);
  p2m1->SetLeftMargin(0.1);
  p2m1->SetRightMargin(0.05);
  p2m1->SetFillColor(0);
  p2m1->SetBorderMode(0);
  p2m1->SetBorderSize(0);
  p2m1->Draw();


  p1m1->cd();
  TH1D *hRatioM1_1 = (TH1D*)fAdapDataHistoM1->Clone("hRatioM1_1");
  TH1D *hRatioM1_2 = (TH1D*)fAdapDataHistoM1->Clone("hRatioM1_2");
  TH1D *hRatioM1_3 = (TH1D*)fAdapDataHistoM1->Clone("hRatioM1_3");

  // hRatioM1_1->Add(fModelTotAdapM1, -1);
  hRatioM1_1->Divide(fModelTotAdapM1);
  hRatioM1_2->Divide(fModelTotAdapM1);
  hRatioM1_3->Divide(fModelTotAdapM1);
  hRatioM1_3->SetMaximum(2.9);
  hRatioM1_3->SetMinimum(-0.9);
  for(int i = 1; i <= hRatioM1_1->GetNbinsX(); i++)
  {
    hRatioM1_1->SetBinError(i, fAdapDataHistoM1->GetBinError(i)/fModelTotAdapM1->GetBinContent(i) );
    hRatioM1_2->SetBinError(i, 2*fAdapDataHistoM1->GetBinError(i)/fModelTotAdapM1->GetBinContent(i) );
    hRatioM1_3->SetBinError(i, 3*fAdapDataHistoM1->GetBinError(i)/fModelTotAdapM1->GetBinContent(i) );

  }
  TLine *LineM1 = new TLine();
  hRatioM1_3->GetXaxis()->SetRange(fAdapDataHistoM1->FindBin(dFitMin), fAdapDataHistoM1->FindBin(dFitMax-1));
  hRatioM1_3->SetMarkerStyle(6);
  hRatioM1_3->SetTitle("");
  hRatioM1_3->GetXaxis()->SetTitle("Energy (keV)");
  hRatioM1_3->GetYaxis()->SetTitle("Ratio (Data/MC)");
  hRatioM1_3->GetXaxis()->SetLabelSize(0.07);
  hRatioM1_3->GetYaxis()->SetLabelSize(0.07);
  hRatioM1_3->GetXaxis()->SetTitleSize(0.07);
  hRatioM1_3->GetYaxis()->SetTitleSize(0.07);
  hRatioM1_3->GetYaxis()->SetTitleOffset(0.45);
  hRatioM1_1->SetFillColor(kMagenta-9);
  hRatioM1_2->SetFillColor(kGreen-8);
  hRatioM1_3->SetFillColor(kCyan+3);
  hRatioM1_3->Draw("pe2");
  hRatioM1_2->Draw("SAME e2");
  hRatioM1_1->Draw("SAME e2");
  LineM1->DrawLine(dFitMin, 1, dFitMax-1, 1);

  p2m1->cd();
  p2m1->SetLogy();
  fAdapDataHistoM1->GetXaxis()->SetRange(fAdapDataHistoM1->FindBin(dFitMin), fAdapDataHistoM1->FindBin(dFitMax-1));
  fAdapDataHistoM1->SetTitle("Total Model (M1)");
  // fAdapDataHistoM1->SetTitleOffset(1.5);
  // fAdapDataHistoM1->SetTitleSize(0.005);
  fAdapDataHistoM1->GetXaxis()->SetTitle("Energy (keV)");
  fAdapDataHistoM1->GetYaxis()->SetTitle("Counts/Bin");
  fAdapDataHistoM1->Draw("E");
  fModelTotAdapM1->Draw("SAME");
  // fModelTotAdapthM1->Draw("SAME");
  // fModelTotAdapuM1->Draw("SAME");
  // fModelTotAdapkM1->Draw("SAME");
  // fModelTotAdapcoM1->Draw("SAME");
  // fModelTotAdapNDBDM1->Draw("SAME");
  // fModelTotAdap2NDBDM1->Draw("SAME");
  // fModelTotAdapbiM1->Draw("SAME");
  // fModelTotAdapmnM1->Draw("SAME");
  // fModelTotAdapSpbM1->Draw("SAME");
  // fModelTotAdapSpoM1->Draw("SAME");
  // fModelTotAdappbM1->Draw("SAME");
  // fModelTotAdapFudgeM1->Draw("SAME");
  // legfit1->Draw();

  // fDataHistoM1->SetLineColor(17);
  // fDataHistoM1->Draw("SAME");

  ///// Draw Data M2
  fAdapDataHistoM2->SetLineColor(kBlack);
  fAdapDataHistoM2->SetLineWidth(2);
  // fAdapDataHistoM2->SetMaximum(9000);
  // fAdapDataHistoM2->GetXaxis()->SetRange(1, fAdapDataHistoM2->FindBin(3000));
  
  fModelTotAdapM2->SetLineColor(2);
  fModelTotAdapM2->SetLineWidth(1);
  fModelTotAdapthM2->SetLineColor(3);
  fModelTotAdapthM2->SetLineStyle(2);
  fModelTotAdapuM2->SetLineColor(4);
  fModelTotAdapuM2->SetLineStyle(2);
  fModelTotAdapkM2->SetLineColor(6);
  fModelTotAdapkM2->SetLineStyle(2);
  fModelTotAdapcoM2->SetLineColor(7);
  fModelTotAdapcoM2->SetLineStyle(2);
  fModelTotAdapNDBDM2->SetLineColor(42);
  fModelTotAdapNDBDM2->SetLineStyle(2);
  fModelTotAdap2NDBDM2->SetLineColor(46);
  fModelTotAdap2NDBDM2->SetLineStyle(2);
  fModelTotAdapbiM2->SetLineColor(5);
  fModelTotAdapbiM2->SetLineStyle(2);
  fModelTotAdapmnM2->SetLineColor(40);
  fModelTotAdapmnM2->SetLineStyle(2);

  fModelTotAdapSpoM2->SetLineStyle(2);
  fModelTotAdapSpoM2->SetLineColor(44);

  fModelTotAdapSpbM2->SetLineStyle(2);
  fModelTotAdapSpbM2->SetLineColor(8);

  fModelTotAdapFudgeM2->SetLineStyle(2);
  fModelTotAdapFudgeM2->SetLineColor(kRed+1);

  TLegend *legfit2 = new TLegend(0.7,0.7,0.95,0.92);
  legfit2->SetFillColor(0);
  legfit2->SetTextSize(0.02);
  legfit2->AddEntry(fModelTotAdapM2, "Total PDF", "l");
  legfit2->AddEntry(fModelTotAdapthM2, "Total th-232", "l");
  legfit2->AddEntry(fModelTotAdapuM2, "Total u-238", "l");
  legfit2->AddEntry(fModelTotAdapkM2, "Total k-40", "l");
  legfit2->AddEntry(fModelTotAdapcoM2, "Total co-60", "l");
  // legfit2->AddEntry(fModelTotAdapNDBDM2, "NDBD", "l");
  legfit2->AddEntry(fModelTotAdap2NDBDM2, "2NDBD", "l");
  legfit2->AddEntry(fModelTotAdapSpbM2, "Surface pb-210", "l");
  legfit2->AddEntry(fModelTotAdapbiM2, "Total External bi-210", "l");  
  legfit2->AddEntry(fModelTotAdapFudgeM2, "Total cs-137", "l");

  TCanvas *cadap2 = new TCanvas("cadap2", "cadap2", 1200, 800);  
  TPad* p1m2 = new TPad("p1m2","p1m2",width1,canBotMargin,width2,canBotMargin+padHeight/1.5,0,0);
  p1m2->SetTopMargin(0.);
  p1m2->SetBottomMargin(0.175);
  p1m2->SetLeftMargin(0.1);
  p1m2->SetRightMargin(0.05);
  p1m2->SetFillColor(0);
  p1m2->SetBorderMode(0);
  p1m2->SetBorderSize(0);
  p1m2->Draw();
  
  // p2 is on top!
  TPad* p2m2 = new TPad("p2m2","p2m2",width1,canBotMargin+padHeight/1.5,width2,canBotMargin+2*padHeight,0,0);
  p2m2->SetBottomMargin(0.);
  p2m2->SetTopMargin(0.08);
  p2m2->SetLeftMargin(0.1);
  p2m2->SetRightMargin(0.05);
  p2m2->SetFillColor(0);
  p2m2->SetBorderMode(0);
  p2m2->SetBorderSize(0);
  p2m2->Draw();


  p1m2->cd();
  TH1D *hRatioM2_1 = (TH1D*)fAdapDataHistoM2->Clone("hRatioM2_1");
  TH1D *hRatioM2_2 = (TH1D*)fAdapDataHistoM2->Clone("hRatioM2_2");
  TH1D *hRatioM2_3 = (TH1D*)fAdapDataHistoM2->Clone("hRatioM2_3");
  hRatioM2_1->Divide(fModelTotAdapM2);
  hRatioM2_2->Divide(fModelTotAdapM2);
  hRatioM2_3->Divide(fModelTotAdapM2);
  hRatioM2_3->SetMaximum(2.9);
  hRatioM2_3->SetMinimum(-0.9);
  for(int i = 1; i <= hRatioM2_1->GetNbinsX(); i++)
  {
    hRatioM2_1->SetBinError(i, fAdapDataHistoM2->GetBinError(i)/fModelTotAdapM2->GetBinContent(i) );
    hRatioM2_2->SetBinError(i, 2*fAdapDataHistoM2->GetBinError(i)/fModelTotAdapM2->GetBinContent(i) );
    hRatioM2_3->SetBinError(i, 3*fAdapDataHistoM2->GetBinError(i)/fModelTotAdapM2->GetBinContent(i) );
  }
  hRatioM2_3->GetXaxis()->SetRange(fAdapDataHistoM2->FindBin(dFitMin), fAdapDataHistoM2->FindBin(dFitMax-1));
  hRatioM2_3->SetMarkerStyle(6);
  hRatioM2_3->SetTitle("");
  hRatioM2_3->GetXaxis()->SetTitle("Energy (keV)");
  hRatioM2_3->GetYaxis()->SetTitle("Ratio (Data/MC)");  
  hRatioM2_3->GetXaxis()->SetLabelSize(0.07);
  hRatioM2_3->GetYaxis()->SetLabelSize(0.07);
  hRatioM2_3->GetXaxis()->SetTitleSize(0.07);
  hRatioM2_3->GetYaxis()->SetTitleSize(0.07);
  hRatioM2_3->GetYaxis()->SetTitleOffset(0.45);
  hRatioM2_1->SetFillColor(kMagenta-9);
  hRatioM2_2->SetFillColor(kGreen-8);
  hRatioM2_3->SetFillColor(kCyan+3);
  hRatioM2_3->Draw("pE2");
  hRatioM2_2->Draw("SAME e2");
  hRatioM2_1->Draw("SAME e2");
  LineM1->DrawLine(dFitMin, 1, dFitMax-1, 1);



  p2m2->cd();
  p2m2->SetLogy();
  fAdapDataHistoM2->GetXaxis()->SetRange(fAdapDataHistoM2->FindBin(dFitMin), fAdapDataHistoM2->FindBin(dFitMax-1));
  // fAdapDataHistoM2->SetTitleOffset(0.45);
  // fAdapDataHistoM2->SetTitleSize(0.01);
  fAdapDataHistoM2->SetTitle("Total Model (M2)");
  fAdapDataHistoM2->GetXaxis()->SetTitle("Energy (keV)");
  fAdapDataHistoM2->GetYaxis()->SetTitle("Counts/Bin");
  fAdapDataHistoM2->Draw("E");
  fModelTotAdapM2->Draw("SAME");
  // fModelTotAdapthM2->Draw("SAME");
  // fModelTotAdapuM2->Draw("SAME");
  // fModelTotAdapkM2->Draw("SAME");
  // fModelTotAdapcoM2->Draw("SAME");
  // fModelTotAdapNDBDM2->Draw("SAME");
  // fModelTotAdap2NDBDM2->Draw("SAME");
  // fModelTotAdapbiM2->Draw("SAME");
  // fModelTotAdapmnM2->Draw("SAME");
  // fModelTotAdapSpbM2->Draw("SAME");
  // fModelTotAdapSpoM2->Draw("SAME");
  // fModelTotAdappbM2->Draw("SAME");
  // fModelTotAdapFudgeM2->Draw("SAME");
  // legfit2->Draw();

  // fDataHistoM2->SetLineColor(17);
  // fDataHistoM2->Draw("SAME");


  ///// Draw Data M2Sum
  fAdapDataHistoM2Sum->SetLineColor(kBlack);
  fAdapDataHistoM2Sum->SetLineWidth(2);
  // fAdapDataHistoM2Sum->SetMaximum(9000);
  // fAdapDataHistoM2Sum->GetXaxis()->SetRange(1, fAdapDataHistoM2Sum->FindBin(3000));
  
  fModelTotAdapM2Sum->SetLineColor(2);
  fModelTotAdapM2Sum->SetLineWidth(1);
  fModelTotAdapthM2Sum->SetLineColor(3);
  fModelTotAdapthM2Sum->SetLineStyle(2);
  fModelTotAdapuM2Sum->SetLineColor(4);
  fModelTotAdapuM2Sum->SetLineStyle(2);
  fModelTotAdapkM2Sum->SetLineColor(6);
  fModelTotAdapkM2Sum->SetLineStyle(2);
  fModelTotAdapcoM2Sum->SetLineColor(7);
  fModelTotAdapcoM2Sum->SetLineStyle(2);
  fModelTotAdapNDBDM2Sum->SetLineColor(42);
  fModelTotAdapNDBDM2Sum->SetLineStyle(2);
  fModelTotAdap2NDBDM2Sum->SetLineColor(46);
  fModelTotAdap2NDBDM2Sum->SetLineStyle(2);
  fModelTotAdapbiM2Sum->SetLineColor(5);
  fModelTotAdapbiM2Sum->SetLineStyle(2);
  fModelTotAdapmnM2Sum->SetLineColor(40);
  fModelTotAdapmnM2Sum->SetLineStyle(2);

  fModelTotAdapSpoM2Sum->SetLineStyle(2);
  fModelTotAdapSpoM2Sum->SetLineColor(44);

  fModelTotAdapSpbM2Sum->SetLineStyle(2);
  fModelTotAdapSpbM2Sum->SetLineColor(8);

  fModelTotAdapFudgeM2Sum->SetLineStyle(2);
  fModelTotAdapFudgeM2Sum->SetLineColor(kRed+1);

  TCanvas *cadap2sum = new TCanvas("cadap2sum", "cadap2sum", 1200, 800);
  TPad* p1m2sum = new TPad("p1m2sum","p1m2sum",width1,canBotMargin,width2,canBotMargin+padHeight/1.5,0,0);
  p1m2sum->SetTopMargin(0.);
  p1m2sum->SetBottomMargin(0.175);
  p1m2sum->SetLeftMargin(0.1);
  p1m2sum->SetRightMargin(0.05);
  p1m2sum->SetFillColor(0);
  p1m2sum->SetBorderMode(0);
  p1m2sum->SetBorderSize(0);
  p1m2sum->Draw();
  
  // p2 is on top!
  TPad* p2m2sum = new TPad("p2m2sum","p2m2sum",width1,canBotMargin+padHeight/1.5,width2,canBotMargin+2*padHeight,0,0);
  p2m2sum->SetBottomMargin(0.);
  p2m2sum->SetTopMargin(0.08);
  p2m2sum->SetLeftMargin(0.1);
  p2m2sum->SetRightMargin(0.05);
  p2m2sum->SetFillColor(0);
  p2m2sum->SetBorderMode(0);
  p2m2sum->SetBorderSize(0);
  p2m2sum->Draw();

  p1m2sum->cd();
  TH1D *hRatioM2Sum_1 = (TH1D*)fAdapDataHistoM2Sum->Clone("hRatioM2Sum_1");
  TH1D *hRatioM2Sum_2 = (TH1D*)fAdapDataHistoM2Sum->Clone("hRatioM2Sum_2");
  TH1D *hRatioM2Sum_3 = (TH1D*)fAdapDataHistoM2Sum->Clone("hRatioM2Sum_3");
  hRatioM2Sum_1->Divide(fModelTotAdapM2Sum);
  hRatioM2Sum_2->Divide(fModelTotAdapM2Sum);
  hRatioM2Sum_3->Divide(fModelTotAdapM2Sum);
  hRatioM2Sum_3->SetMaximum(2.9);
  hRatioM2Sum_3->SetMinimum(-0.9);
  for(int i = 1; i <= hRatioM2Sum_1->GetNbinsX(); i++)
  {
    hRatioM2Sum_1->SetBinError(i, fAdapDataHistoM2Sum->GetBinError(i)/fModelTotAdapM2Sum->GetBinContent(i) );
    hRatioM2Sum_2->SetBinError(i, 2*fAdapDataHistoM2Sum->GetBinError(i)/fModelTotAdapM2Sum->GetBinContent(i) );
    hRatioM2Sum_3->SetBinError(i, 3*fAdapDataHistoM2Sum->GetBinError(i)/fModelTotAdapM2Sum->GetBinContent(i) );
  }
  hRatioM2Sum_3->GetXaxis()->SetRange(fAdapDataHistoM2Sum->FindBin(dFitMin), fAdapDataHistoM2Sum->FindBin(dFitMax-1));
  hRatioM2Sum_3->SetMarkerStyle(6);
  hRatioM2Sum_3->SetTitle("");
  hRatioM2Sum_3->GetXaxis()->SetTitle("Energy (keV)");
  hRatioM2Sum_3->GetYaxis()->SetTitle("Ratio (Data/MC)");  
  hRatioM2Sum_3->GetXaxis()->SetLabelSize(0.07);
  hRatioM2Sum_3->GetYaxis()->SetLabelSize(0.07);
  hRatioM2Sum_3->GetXaxis()->SetTitleSize(0.07);
  hRatioM2Sum_3->GetYaxis()->SetTitleSize(0.07);
  hRatioM2Sum_3->GetYaxis()->SetTitleOffset(0.45);
  hRatioM2Sum_1->SetFillColor(kMagenta-9);
  hRatioM2Sum_2->SetFillColor(kGreen-8);
  hRatioM2Sum_3->SetFillColor(kCyan+3);
  hRatioM2Sum_3->Draw("pE2");
  hRatioM2Sum_2->Draw("SAME e2");
  hRatioM2Sum_1->Draw("SAME e2");
  LineM1->DrawLine(dFitMin, 1, dFitMax-1, 1);

  p2m2sum->cd();
  p2m2sum->SetLogy();
  fAdapDataHistoM2Sum->GetXaxis()->SetRange(fAdapDataHistoM2Sum->FindBin(dFitMin), fAdapDataHistoM2Sum->FindBin(dFitMax-1));
  // fAdapDataHistoM2Sum->SetTitleOffset(0.45);
  // fAdapDataHistoM2Sum->SetTitleSize(0.01);
  fAdapDataHistoM2Sum->SetTitle("Total Model (M2Sum)");
  fAdapDataHistoM2Sum->GetXaxis()->SetTitle("Energy (keV)");
  fAdapDataHistoM2Sum->GetYaxis()->SetTitle("Counts/Bin");
  fAdapDataHistoM2Sum->Draw("E");
  fModelTotAdapM2Sum->Draw("SAME");
  // fModelTotAdapthM2Sum->Draw("SAME");
  // fModelTotAdapuM2Sum->Draw("SAME");
  // fModelTotAdapkM2Sum->Draw("SAME");
  // fModelTotAdapcoM2Sum->Draw("SAME");
  // fModelTotAdapNDBDM2Sum->Draw("SAME");
  // fModelTotAdap2NDBDM2Sum->Draw("SAME");
  // fModelTotAdapbiM2Sum->Draw("SAME");
  // fModelTotAdapmnM2Sum->Draw("SAME");
  // fModelTotAdapSpbM2Sum->Draw("SAME");
  // fModelTotAdapSpoM2Sum->Draw("SAME");
  // fModelTotAdappbM2Sum->Draw("SAME");
  // fModelTotAdapFudgeM2Sum->Draw("SAME");

  // fDataHistoM2Sum->SetLineColor(17);
  // fDataHistoM2Sum->Draw("SAME");

  // Residuals
  TCanvas *cResidual1 = new TCanvas("cResidual1", "cResidual1", 1200, 800);
  hResidualGausM1 = new TH1D("hResidualGausM1", "M1", 100, -50, 50);
  hResidualDistM1 = CalculateResidualsAdaptive(fAdapDataHistoM1, fModelTotAdapM1, hResidualGausM1, dFitMinBinM1, dFitMaxBinM1, 1);
  hResidualDistM1->SetLineColor(kBlack);
  hResidualDistM1->SetName("Residuals");
  hResidualDistM1->SetTitle("Normalized Residuals (M1)");
  hResidualDistM1->SetMarkerStyle(25);
  hResidualDistM1->GetXaxis()->SetTitle("Energy (keV)");
  // hResidualDistM1->GetXaxis()->SetTitleSize(0.04);
  // hResidualDistM1->GetXaxis()->SetLabelSize(0.05);
  // hResidualDistM1->GetYaxis()->SetLabelSize(0.05);
  // hResidualDistM1->GetYaxis()->SetTitleSize(0.04);
  // hResidualDistM1->GetXaxis()->SetRange(1, fAdapDataHistoM2->FindBin(3000));
  hResidualDistM1->GetYaxis()->SetTitle("Residuals (#sigma)");
  hResidualDistM1->Draw();

  TCanvas *cResidual2 = new TCanvas("cResidual2", "cResidual2", 1200, 800);
  hResidualGausM2 = new TH1D("hResidualGausM2", "M2", 100, -50, 50);  
  hResidualDistM2 = CalculateResidualsAdaptive(fAdapDataHistoM2, fModelTotAdapM2, hResidualGausM2, dFitMinBinM2, dFitMaxBinM2, 2);
  hResidualDistM2->SetLineColor(kBlack);
  hResidualDistM2->SetName("Residuals");
  hResidualDistM2->SetTitle("Normalized Residuals (M2)");
  hResidualDistM2->SetMarkerStyle(25);
  hResidualDistM2->GetXaxis()->SetTitle("Energy (keV)");
  // hResidualDistM2->GetXaxis()->SetTitleSize(0.04);
  // hResidualDistM2->GetXaxis()->SetLabelSize(0.05);
  // hResidualDistM2->GetYaxis()->SetLabelSize(0.05);
  // hResidualDistM2->GetYaxis()->SetTitleSize(0.04); 
  // hResidualDistM2->GetXaxis()->SetRange(1, fAdapDataHistoM2->FindBin(3000));
  hResidualDistM2->GetYaxis()->SetTitle("Residuals (#sigma)");
  hResidualDistM2->Draw();


  TCanvas *cResidual2Sum = new TCanvas("cResidual2Sum", "cResidual2Sum", 1200, 800);
  hResidualGausM2Sum = new TH1D("hResidualGausM2Sum", "M2Sum", 100, -50, 50);  
  hResidualDistM2Sum = CalculateResidualsAdaptive(fAdapDataHistoM2Sum, fModelTotAdapM2Sum, hResidualGausM2Sum, dFitMinBinM2Sum, dFitMaxBinM2Sum, 3);
  hResidualDistM2Sum->SetLineColor(kBlack);
  hResidualDistM2Sum->SetName("Residuals");
  hResidualDistM2Sum->SetTitle("Normalized Residuals (M2Sum)");
  hResidualDistM2Sum->SetMarkerStyle(25);
  hResidualDistM2Sum->GetXaxis()->SetTitle("Energy (keV)");
  // hResidualDistM2Sum->GetXaxis()->SetTitleSize(0.04);
  // hResidualDistM2Sum->GetXaxis()->SetLabelSize(0.05);
  // hResidualDistM2Sum->GetYaxis()->SetLabelSize(0.05);
  // hResidualDistM2Sum->GetYaxis()->SetTitleSize(0.04); 
  // hResidualDistM2Sum->GetXaxis()->SetRange(1, fAdapDataHistoM2Sum->FindBin(3000));
  hResidualDistM2Sum->GetYaxis()->SetTitle("Residuals (#sigma)");
  hResidualDistM2Sum->Draw();

  TCanvas *cres1 = new TCanvas("cres1", "cres1", 1600, 600);
  cres1->Divide(2,1);
  cres1->cd(1);
  hResidualGausM1->Fit("gaus");
  hResidualGausM1->Draw();
  cres1->cd(2);
  hResidualGausM2->Fit("gaus");
  hResidualGausM2->Draw();


  for (int i = dFitMinBinM1; i < dFitMaxBinM1; i++)
  {
    dResidualRMSM1 += hResidualDistM1->GetBinContent(i)*hResidualDistM1->GetBinContent(i);
  }
  for (int i = dFitMinBinM2; i < dFitMaxBinM2; i++)
  {
    dResidualRMSM2 += hResidualDistM2->GetBinContent(i)*hResidualDistM2->GetBinContent(i);
  }
  for (int i = dFitMinBinM2Sum; i < dFitMaxBinM2Sum; i++)
  {
    dResidualRMSM2Sum += hResidualDistM2Sum->GetBinContent(i)*hResidualDistM2Sum->GetBinContent(i);
  }


  dResidualRMSTot = TMath::Sqrt( (dResidualRMSM1 + dResidualRMSM2)/ (dNDF + dNumFreeParameters) );


  dResidualRMSM1 = TMath::Sqrt(dResidualRMSM1/(dFitMaxBinM1-dFitMinBinM1));
  dResidualRMSM2 = TMath::Sqrt(dResidualRMSM2/(dFitMaxBinM2-dFitMinBinM2));
  dResidualRMSM2Sum = TMath::Sqrt(dResidualRMSM2Sum/(dFitMaxBinM2Sum-dFitMinBinM2Sum));


  double dROIRange = fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2570))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2570)) - fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2470)); 
  double d2nbbRange = fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2000))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2000)) - fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(500));
  double d2nbbData = fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr);
  double d2nbbDataErr = TMath::Sqrt(fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr);
  double d2nbbModel = fModelTotAdapM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr);
  double d2nbbModelErr = TMath::Sqrt(fModelTotAdapM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr);
  double dROIData = fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr);
  double dROIDataErr = TMath::Sqrt(fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr);
  double dROIModel = fModelTotAdapM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr);
  double dROIModelErr = TMath::Sqrt(fModelTotAdapM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr);
  // Output integrals of stuff for limits
  cout << "ROI range: " << fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2470)) << " " << fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2570))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2570)) << " keV" << endl; // 2470 to 2572
  cout << "Integral Data in ROI: " << fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt( fAdapDataHistoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total PDF in ROI: " << fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width") << " +/- " << sqrt( fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total Th-232 PDF in ROI: " << fModelTotAdapthM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt( fModelTotAdapthM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total U-238 PDF in ROI: " << fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total Co PDF in ROI: " << fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total Pb-210 PDF in ROI: " << fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << "Integral Total Po-210 PDF in ROI: " << fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;  
  cout << "Integral Total 0NDBD PDF in ROI: " << fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  cout << endl;
  cout << "Integral Data in ROI (counts/keV/y): " << fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt( fAdapDataHistoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Integral Total PDF in ROI (counts/keV/y): " << fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt( fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Integral Total Th-232 PDF in ROI (counts/keV/y): " << fModelTotAdapthM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt( fModelTotAdapthM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Integral Total U-238 PDF in ROI (counts/keV/y): " << fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt(fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Integral Total Co PDF in ROI (counts/keV/y): " << fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt(fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Integral Total Pb-210 PDF in ROI (counts/keV/y): " << fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt(fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  // cout << "Integral Total Po-210 PDF in ROI (counts/keV): " << fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt(fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;  
  cout << "Integral Total 0NDBD PDF in ROI (counts/keV/y): " << fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " +/- " << sqrt(fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << endl;
  cout << "Data in 2nbb region (c/keV/y): " << d2nbbData << " $\\pm$ " << d2nbbDataErr << endl;  
  // cout << "Number of 2nbb: " << fParameters[0]*dDataIntegralM1 << " +/- " << fParError[0]*dDataIntegralM1 << "\t 2nbb half life: " << (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[0]*dDataIntegralM1) << " +/- " << (fParError[0]/fParameters[0]) * (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[0]*dDataIntegralM1) << endl;
  // cout << "Counts in 2nbb (M1 + M2): " << fModelTotAdap2NDBDM1->Integral(1, fAdapDataHistoM1->FindBin(3000), "width") + fModelTotAdap2NDBDM2->Integral(1, fAdapDataHistoM2->FindBin(3000) , "width")/2 << "\t Half-Life " << (0.69314718056)*(4.726e25 * dLivetimeYr)/(fModelTotAdap2NDBDM1->Integral(1, fAdapDataHistoM1->FindBin(2700), "width") + fModelTotAdap2NDBDM2->Integral(1, fAdapDataHistoM2->FindBin(2700) , "width")/2) << endl;
  cout << "Model in 2nbb region (c/keV/y): " << d2nbbModel << " $\\pm$ " << d2nbbModelErr << endl;
  cout << "Data in 0nbb region (c/keV/y): " << dROIData << " $\\pm$ " << dROIDataErr << endl;
  cout << "Model in 0nbb region (c/keV/y): " << dROIModel << " $\\pm$ " << dROIModelErr << endl;
  cout << "Model/Data in 2nbb region: " << d2nbbModel/d2nbbData << endl;
  cout << "Model/Data in 0nbb region: " << dROIModel/dROIData << endl;

  cout << "Residual RMS (Tot): " << dResidualRMSTot << endl;
  cout << "Residual RMS (M1): " << dResidualRMSM1 << "\t" << "Residual RMS (M2): " << dResidualRMSM2 << "\t Residual RMS (M2Sum): "  << dResidualRMSM2Sum << endl;

  dChiSquare = GetChiSquareAdaptive();

  cout << "Total number of calls = " << dNumCalls << "\t" << "ChiSq/NDF = " << dChiSquare/dNDF << endl; // for M1 and M2
  cout << "ChiSq = " << dChiSquare << "\t" << "NDF = " << dNDF << endl;
  cout << "Probability = " << TMath::Prob(dChiSquare, dNDF ) << endl;

/*
    2nbb calculation:
     - TeO2 molar mass: 159.6 g/mol
     - half life is 9.81 * 10^20 years
     - how many in Q0 data so far? 1/rate = half life/ln(2) -> rate = ln(2)/half life = 7.066*10^-22 decays/year (Laura's thesis)
     - Moles = 750g * 49 crystals * 0.3408 abundance/159.6 g/mol = 78.474 mol total
     - N_TeO2 = 78.474 * N_A = 4.726*10^25 nuclei of Te130
     - N_2nbb = N_TeO2 * rate * livetime = 1.551*10^4 events
     - half life = rate * ln(2) = ln(2) * N_TeO2 * livetime / N_2nbb
*/


  // This gets the number of counts corrected for detector efficiency
  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
    fParActivity[i] = fParameters[i]*dDataIntegralM1/fParEfficiencyM1[i];
    fParActivityErr[i] = fParError[i]*dDataIntegralM1/fParEfficiencyM1[i];
  }


  // Correlation Matrix section
  TMatrixT<double> mCorrMatrix;
  TMatrixT<double> mCorrMatrixInverse;
  mCorrMatrix.ResizeTo(TBackgroundModel::dNParam, TBackgroundModel::dNParam);
  mCorrMatrixInverse.ResizeTo(TBackgroundModel::dNParam, TBackgroundModel::dNParam);
  minuit->mnemat(mCorrMatrix.GetMatrixArray(), TBackgroundModel::dNParam);

  for(int i = mCorrMatrix.GetRowLwb(); i <= mCorrMatrix.GetRowUpb(); i++)
    for(int j = mCorrMatrix.GetColLwb(); j <= mCorrMatrix.GetColUpb(); j++)
      mCorrMatrix(i,j) = mCorrMatrix(i,j)/(fParError[i]*fParError[j]);

  for(int i = mCorrMatrix.GetRowLwb(); i <= mCorrMatrix.GetRowUpb(); i++)
    for(int j = mCorrMatrix.GetColLwb(); j <= mCorrMatrix.GetColUpb(); j++)
      mCorrMatrixInverse(i,j) = mCorrMatrix(TBackgroundModel::dNParam-i-1, j); 

  TCanvas *cMatrix = new TCanvas("cMatrix", "cMatrix", 1800, 1000);
  TPad* pM1 = new TPad("pM1","pM1",width1,canBotMargin,width2*0.75,canBotMargin+2*padHeight,0,0);
  pM1->SetTopMargin(0.05);
  pM1->SetBottomMargin(0.05);
  pM1->SetLeftMargin(0.05);
  pM1->SetRightMargin(0.15);
  pM1->SetFillColor(0);
  pM1->SetBorderMode(0);
  pM1->SetBorderSize(0);
  pM1->Draw();

  TPad* pM2 = new TPad("pM2","pM2",width2*0.75,canBotMargin,width2,canBotMargin+2*padHeight,0,0);
  pM2->SetTopMargin(0.05);
  pM2->SetBottomMargin(0.05);
  pM2->SetLeftMargin(0.1);
  pM2->SetRightMargin(0.05);
  pM2->SetFillColor(0);
  pM2->SetBorderMode(0);
  pM2->SetBorderSize(0);
  pM2->Draw();

  pM1->cd();
  pM1->SetGrid();
  pM1->SetFillStyle(4000);
  mCorrMatrix.Draw("colz");

  // If want inverted y-axis for matrix
  // pM1->SetGrid();
  // TH2D *h2Dummy = new TH2D("h2Dummy","", TBackgroundModel::dNParam, 0, TBackgroundModel::dNParam, TBackgroundModel::dNParam, 0, TBackgroundModel::dNParam);
  // h2Dummy->Draw();
  // mCorrMatrix.Draw("colz SAME");

  // TH2C *hgrid = new TH2C("hgrid","",dNParam,0.,dNParam,dNParam,0.,dNParam);
  // hgrid->Draw("SAME");
  // hgrid->GetXaxis()->SetNdivisions(dNParam);
  // hgrid->GetYaxis()->SetNdivisions(dNParam);

  // mCorrMatrixInverse.Draw("colzSAME");
  // Setting axis (inverted axis for matrix)
       // h2Dummy->GetYaxis()->SetLabelOffset(999);
       // // Redraw the new axis
       // gPad->Update();
       // TGaxis *newaxis = new TGaxis(gPad->GetUxmin(),
       //                              gPad->GetUymax(),
       //                              gPad->GetUxmin()-0.0001,
       //                              gPad->GetUymin(),
       //                              h2Dummy->GetYaxis()->GetXmin(),
       //                              h2Dummy->GetYaxis()->GetXmax(),
       //                              505,"+");
       // newaxis->SetLabelOffset(-0.03);
       // newaxis->Draw();
  
       // h2Dummy->GetXaxis()->SetLabelOffset(999);
       // // Redraw the new axis
       // gPad->Update();
       // TGaxis *newaxis2 = new TGaxis(gPad->GetUxmin(),
       //                              gPad->GetUymin(),
       //                              gPad->GetUxmax(),
       //                              gPad->GetUymin(),
       //                              h2Dummy->GetXaxis()->GetXmin(),
       //                              h2Dummy->GetXaxis()->GetXmax(),
       //                              505,"+");
       // newaxis2->SetLabelOffset(0.01);
       // newaxis2->Draw();

  pM2->cd();
  TPaveText *pPave = new TPaveText(0,0,1,1, "NB"); // Text for matrix
  pPave->SetTextSize(0.04);
  pPave->SetFillColor(0);
  pPave->SetBorderSize(0);
  for(int i=0; i < TBackgroundModel::dNParam; i++)
  {
    pPave->AddText(Form("%d: %s", i, minuit->fCpnam[i].Data() ) );
  }
  pPave->Draw();


  double dProgressM1 = 0;
  double dProgressM2 = 0;
  double dProgressM2Sum = 0;
  double dataM1_i = 0, modelM1_i = 0;
  double dataM2_i = 0, modelM2_i = 0;
  double dataM2Sum_i = 0, modelM2Sum_i = 0;

  for(int i = dFitMinBinM1; i < dFitMaxBinM1; i++)
  {
    if( fAdapDataHistoM1->GetBinCenter(i) >= 3150 && fAdapDataHistoM1->GetBinCenter(i) <= 3400)continue;

    // Skipping unknown peaks
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 800 && fAdapDataHistoM1->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1060 && fAdapDataHistoM1->GetBinCenter(i) <= 1068)continue; 

    // if( fAdapDataHistoM1->GetBinCenter(i) >= 506 && fAdapDataHistoM1->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 579 && fAdapDataHistoM1->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 605 && fAdapDataHistoM1->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 906 && fAdapDataHistoM1->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1450 && fAdapDataHistoM1->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 1755 && fAdapDataHistoM1->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2090 && fAdapDataHistoM1->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2200 && fAdapDataHistoM1->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2440 && fAdapDataHistoM1->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM1->GetBinCenter(i) >= 2600 && fAdapDataHistoM1->GetBinCenter(i) <= 2630)continue;  

    dataM1_i = fAdapDataHistoM1->GetBinContent(i)*fAdapDataHistoM1->GetBinWidth(i);
    modelM1_i = fModelTotAdapM1->GetBinContent(i)*fAdapDataHistoM1->GetBinWidth(i);
    dProgressM1 += 2 * (modelM1_i - dataM1_i + dataM1_i * TMath::Log(dataM1_i/modelM1_i));
    hChiSquaredProgressM1->SetBinContent(i, dProgressM1);
  }
  for(int i = dFitMinBinM2; i < dFitMaxBinM2; i++)
  {
    if( fAdapDataHistoM2->GetBinCenter(i) >= 3150 && fAdapDataHistoM2->GetBinCenter(i) <= 3400)continue;

    // Skipping unknown peaks
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 800 && fAdapDataHistoM2->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1060 && fAdapDataHistoM2->GetBinCenter(i) <= 1068)continue; 

    // if( fAdapDataHistoM2->GetBinCenter(i) >= 506 && fAdapDataHistoM2->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 579 && fAdapDataHistoM2->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 605 && fAdapDataHistoM2->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 906 && fAdapDataHistoM2->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1450 && fAdapDataHistoM2->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 1755 && fAdapDataHistoM2->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2090 && fAdapDataHistoM2->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2200 && fAdapDataHistoM2->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2440 && fAdapDataHistoM2->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM2->GetBinCenter(i) >= 2600 && fAdapDataHistoM2->GetBinCenter(i) <= 2630)continue;  

    dataM2_i = fAdapDataHistoM2->GetBinContent(i)*fAdapDataHistoM2->GetBinWidth(i);
    modelM2_i = fModelTotAdapM2->GetBinContent(i)*fAdapDataHistoM2->GetBinWidth(i);
    dProgressM2 += 2 * (modelM2_i - dataM2_i + dataM2_i * TMath::Log(dataM2_i/modelM2_i));
    hChiSquaredProgressM2->SetBinContent(i, dProgressM2);
  }
  for(int i = dFitMinBinM2Sum; i < dFitMaxBinM2Sum; i++)
  {
    if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 3150 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 3400)continue;

    // Skipping unknown peaks
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 800 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 808)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1060 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1068)continue; 

    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 506 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 515)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 579 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 589)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 605 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 615)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 906 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 917)continue;
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1450 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1475)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 1755 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 1780)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2090 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2130)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2200 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2220)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2440 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2450)continue;  
    // if( fAdapDataHistoM2Sum->GetBinCenter(i) >= 2600 && fAdapDataHistoM2Sum->GetBinCenter(i) <= 2630)continue;  

    dataM2Sum_i = fAdapDataHistoM2Sum->GetBinContent(i)*fAdapDataHistoM2Sum->GetBinWidth(i);
    modelM2Sum_i = fModelTotAdapM2Sum->GetBinContent(i)*fAdapDataHistoM2Sum->GetBinWidth(i);
    dProgressM2Sum += 2 * (modelM2Sum_i - dataM2Sum_i + dataM2Sum_i * TMath::Log(dataM2Sum_i/modelM2Sum_i));
    hChiSquaredProgressM2Sum->SetBinContent(i, dProgressM2Sum);
  }

  TCanvas *cProgress = new TCanvas("cProgress", "cProgress", 1600, 1600);
  cProgress->Divide(1, 3);
  cProgress->cd(1);
  hChiSquaredProgressM1->SetTitle("Increase in #chi^{2} (M1)");
  hChiSquaredProgressM1->GetXaxis()->SetTitle("Energy (keV)");
  hChiSquaredProgressM1->GetYaxis()->SetTitle("#chi^{2}");
  hChiSquaredProgressM1->Draw();

  cProgress->cd(2);
  hChiSquaredProgressM2->SetTitle("Increase in #chi^{2} (M2)");  
  hChiSquaredProgressM2->GetXaxis()->SetTitle("Energy (keV)");
  hChiSquaredProgressM2->GetYaxis()->SetTitle("#chi^{2}");  
  hChiSquaredProgressM2->Draw();

  cProgress->cd(3);
  hChiSquaredProgressM2Sum->SetTitle("Increase in #chi^{2} (M2Sum)");  
  hChiSquaredProgressM2Sum->GetXaxis()->SetTitle("Energy (keV)");
  hChiSquaredProgressM2Sum->GetYaxis()->SetTitle("#chi^{2}");  
  hChiSquaredProgressM2Sum->Draw();

  if(bSave)
  {
  // Save matrix to file
  // ofstream OutMatrix;
  // OutMatrix.open(Form("%s/FitResults/Test/OutMatrix_%d_%d.txt", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime()));
  // for(int j = mCorrMatrix.GetColLwb(); j <= mCorrMatrix.GetColUpb(); j++)
  // {
  //   for(int i = mCorrMatrix.GetRowLwb(); i <= mCorrMatrix.GetRowUpb(); i++)
  //   {  
  //     OutMatrix << mCorrMatrix(i,j) << "\t";
  //   }
  //   OutMatrix << endl;
  //   OutMatrix << endl;
  // }
  // OutMatrix.close();

  // // Saving plots
  cadap1->SaveAs(Form("%s/FitResults/Test/FitM1_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  cadap2->SaveAs(Form("%s/FitResults/Test/FitM2_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  cResidual1->SaveAs(Form("%s/FitResults/Test/FitM1Residual_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  cResidual2->SaveAs(Form("%s/FitResults/Test/FitM2Residual_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  cres1->SaveAs(Form("%s/FitResults/Test/FitResidualDist_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  cMatrix->SaveAs(Form("%s/FitResults/Test/FitCovMatrix_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
 
  cProgress->SaveAs(Form("%s/FitResults/Test/ChiSquareProgress_%d_%d_%d.pdf", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));

  // Save histograms to file
  fSaveResult = new TFile(Form("%s/FitResults/Test/FitResult_%d_%d.root", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime()), "RECREATE");
  fSaveResult->Add(fAdapDataHistoM1);
  fSaveResult->Add(fAdapDataHistoM2);
  fSaveResult->Add(fModelTotAdapM1);
  fSaveResult->Add(fModelTotAdapM2);

  // Scale histograms for saving
  // hAdapTeO20nuM1->Scale( dDataIntegralM1*fParameters[0]);
  hAdapTeO22nuM1->Scale( dDataIntegralM1*fParameters[0]);
  hAdapCuBox_CuFrameco60M1->Scale( dDataIntegralM1*fParameters[1]);
  hAdapTeO2th232onlyM1->Scale( dDataIntegralM1*fParameters[2]);
  hAdapTeO2th230onlyM1->Scale( dDataIntegralM1*fParameters[3]);
  hAdapTeO2Sxth232M1_001->Scale( dDataIntegralM1*fParameters[4]);
  hAdapTeO2Sxra228pb208M1_001->Scale( dDataIntegralM1*fParameters[5]);
  hAdapTeO2Sxu238th230M1_001->Scale( dDataIntegralM1*fParameters[6]);
  hAdapTeO2Sxth230onlyM1_001->Scale( dDataIntegralM1*fParameters[7]);
  hAdapTeO2Sxra226pb210M1_001->Scale( dDataIntegralM1*fParameters[8]);
  hAdapTeO2Sxpb210M1_1->Scale( dDataIntegralM1*fParameters[9]);
  hAdapTeO2Sxpb210M1_001->Scale( dDataIntegralM1*fParameters[10]);
  hAdapCuBox_CuFrameth232M1_10->Scale( dDataIntegralM1*fParameters[11]);
  hAdapCuBox_CuFrameu238M1_10->Scale( dDataIntegralM1*fParameters[12]);
  hAdapCuBox_CuFramepb210M1_01->Scale( dDataIntegralM1*fParameters[13]);
  hAdapCuBox_CuFramepb210M1_001->Scale( dDataIntegralM1*fParameters[14]);
  hAdapPbRomk40M1->Scale( dDataIntegralM1*fParameters[15]);
  hAdapOVCth232M1->Scale( dDataIntegralM1*fParameters[16]);
  hAdapOVCu238M1->Scale( dDataIntegralM1*fParameters[17]);
  hAdapOVCco60M1->Scale( dDataIntegralM1*fParameters[18]);
  hAdapOVCk40M1->Scale( dDataIntegralM1*fParameters[19]);
  hAdapExtPbbi210M1->Scale( dDataIntegralM1*fParameters[20]);
  hAdapCuBox_CuFrameth232M1->Scale( dDataIntegralM1*fParameters[21]);
  hAdapCuBox_CuFrameu238M1->Scale( dDataIntegralM1*fParameters[22]);
  hAdapPbRomcs137M1->Scale( dDataIntegralM1*fParameters[23]);

  // hAdapTeO20nuM2->Scale( dDataIntegralM2*fParameters[0]);
  hAdapTeO22nuM2->Scale( dDataIntegralM2*fParameters[0]);
  hAdapCuBox_CuFrameco60M2->Scale( dDataIntegralM2*fParameters[1]);
  hAdapTeO2th232onlyM2->Scale( dDataIntegralM2*fParameters[2]);
  hAdapTeO2th230onlyM2->Scale( dDataIntegralM2*fParameters[3]);
  hAdapTeO2Sxth232M2_001->Scale( dDataIntegralM2*fParameters[4]);
  hAdapTeO2Sxra228pb208M2_001->Scale( dDataIntegralM2*fParameters[5]);
  hAdapTeO2Sxu238th230M2_001->Scale( dDataIntegralM2*fParameters[6]);
  hAdapTeO2Sxth230onlyM2_001->Scale( dDataIntegralM2*fParameters[7]);
  hAdapTeO2Sxra226pb210M2_001->Scale( dDataIntegralM2*fParameters[8]);
  hAdapTeO2Sxpb210M2_1->Scale( dDataIntegralM2*fParameters[9]);
  hAdapTeO2Sxpb210M2_001->Scale( dDataIntegralM2*fParameters[10]);
  hAdapCuBox_CuFrameth232M2_10->Scale( dDataIntegralM2*fParameters[11]);
  hAdapCuBox_CuFrameu238M2_10->Scale( dDataIntegralM2*fParameters[12]);
  hAdapCuBox_CuFramepb210M2_01->Scale( dDataIntegralM2*fParameters[13]);
  hAdapCuBox_CuFramepb210M2_001->Scale( dDataIntegralM2*fParameters[14]);
  hAdapPbRomk40M2->Scale( dDataIntegralM2*fParameters[15]);
  hAdapOVCth232M2->Scale( dDataIntegralM2*fParameters[16]);
  hAdapOVCu238M2->Scale( dDataIntegralM2*fParameters[17]);
  hAdapOVCco60M2->Scale( dDataIntegralM2*fParameters[18]);
  hAdapOVCk40M2->Scale( dDataIntegralM2*fParameters[19]);
  hAdapExtPbbi210M2->Scale( dDataIntegralM2*fParameters[20]);
  hAdapCuBox_CuFrameth232M2->Scale( dDataIntegralM2*fParameters[21]);
  hAdapCuBox_CuFrameu238M2->Scale( dDataIntegralM2*fParameters[22]);
  hAdapPbRomcs137M2->Scale( dDataIntegralM2*fParameters[23]);

  fSaveResult->Add(hAdapTeO20nuM1);
  fSaveResult->Add(hAdapTeO22nuM1);
  // fSaveResult->Add(hAdapTeO2co60M1);
  // fSaveResult->Add(hAdapTeO2k40M1);
  fSaveResult->Add(hAdapCuBox_CuFrameco60M1);
  fSaveResult->Add(hAdapTeO2th232onlyM1);
  fSaveResult->Add(hAdapTeO2th230onlyM1);
  fSaveResult->Add(hAdapTeO2Sxth232M1_001);
  fSaveResult->Add(hAdapTeO2Sxra228pb208M1_001);
  fSaveResult->Add(hAdapTeO2Sxu238th230M1_001);
  fSaveResult->Add(hAdapTeO2Sxth230onlyM1_001);
  fSaveResult->Add(hAdapTeO2Sxra226pb210M1_001);
  fSaveResult->Add(hAdapTeO2Sxpb210M1_1);
  fSaveResult->Add(hAdapTeO2Sxpb210M1_001);
  fSaveResult->Add(hAdapCuBox_CuFrameth232M1_10);
  fSaveResult->Add(hAdapCuBox_CuFrameu238M1_10);
  fSaveResult->Add(hAdapCuBox_CuFramepb210M1_01);
  fSaveResult->Add(hAdapCuBox_CuFramepb210M1_001);
  fSaveResult->Add(hAdapPbRomk40M1);
  fSaveResult->Add(hAdapOVCth232M1);
  fSaveResult->Add(hAdapOVCu238M1);
  fSaveResult->Add(hAdapOVCco60M1);
  fSaveResult->Add(hAdapOVCk40M1);
  fSaveResult->Add(hAdapExtPbbi210M1);
  fSaveResult->Add(hAdapCuBox_CuFrameth232M1);
  fSaveResult->Add(hAdapCuBox_CuFrameu238M1);

  fSaveResult->Add(hAdapTeO20nuM2);
  fSaveResult->Add(hAdapTeO22nuM2);
  // fSaveResult->Add(hAdapTeO2co60M2);
  // fSaveResult->Add(hAdapTeO2k40M2);
  fSaveResult->Add(hAdapCuBox_CuFrameco60M2);  
  fSaveResult->Add(hAdapTeO2th232onlyM2);
  fSaveResult->Add(hAdapTeO2th230onlyM2);
  fSaveResult->Add(hAdapTeO2Sxth232M2_001);
  fSaveResult->Add(hAdapTeO2Sxra228pb208M2_001);
  fSaveResult->Add(hAdapTeO2Sxu238th230M2_001);
  fSaveResult->Add(hAdapTeO2Sxth230onlyM2_001);
  fSaveResult->Add(hAdapTeO2Sxra226pb210M2_001);
  fSaveResult->Add(hAdapTeO2Sxpb210M2_1);
  fSaveResult->Add(hAdapTeO2Sxpb210M2_001);
  fSaveResult->Add(hAdapCuBox_CuFrameth232M2_10);
  fSaveResult->Add(hAdapCuBox_CuFrameu238M2_10);
  fSaveResult->Add(hAdapCuBox_CuFramepb210M2_01);
  fSaveResult->Add(hAdapCuBox_CuFramepb210M2_001);
  fSaveResult->Add(hAdapPbRomk40M2);
  fSaveResult->Add(hAdapOVCth232M2);
  fSaveResult->Add(hAdapOVCu238M2);
  fSaveResult->Add(hAdapOVCco60M2);
  fSaveResult->Add(hAdapOVCk40M2);
  fSaveResult->Add(hAdapExtPbbi210M2);
  fSaveResult->Add(hAdapCuBox_CuFrameth232M2);
  fSaveResult->Add(hAdapCuBox_CuFrameu238M2);

  fSaveResult->Add(&mCorrMatrix);

  fSaveResult->Write(); 

  // cadap1->SaveAs(Form("%s/FitResults/CovMatrix/FitM1_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  // cadap2->SaveAs(Form("%s/FitResults/CovMatrix/FitM2_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  // cResidual1->SaveAs(Form("%s/FitResults/CovMatrix/FitM1Residual_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  // cResidual2->SaveAs(Form("%s/FitResults/CovMatrix/FitM2Residual_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  // cres1->SaveAs(Form("%s/FitResults/CovMatrix/FitResidualDist_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));
  // cMatrix->SaveAs(Form("%s/FitResults/CovMatrix/FitCovMatrix_%d_%d_%d.C", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop));

  LatexResultTable(0);
  } // end bSave

  // Kernal Convolution
  // TH1D *hKernalConvM1 = new TH1D("hKernalConvM1", "", dAdaptiveBinsM1, dAdaptiveArrayM1);
  // Kernal(fModelTotAdapM1, hKernalConvM1);

  // TCanvas *cKernal1 = new TCanvas("cKernal1", "cKernal1", 1200, 1200);
  // hKernalConvM1->Draw();



  return true;
}

// Txt file with useful stuff
void TBackgroundModel::LatexResultTable(double fValue)
{

  OutFile.open(Form("%s/FitResults/Test/FitOutputTable_%d_%d_%d.txt", dSaveDir.c_str(), tTime->GetDate(), tTime->GetTime(), nLoop ));
  OutFile << "Initial Value of 2nbb: " << fValue << endl;
  OutFile << "Fit Range: " << dFitMin << " to " << dFitMax << endl;
  OutFile << "Base binning: " << dBinBase << endl;
  OutFile << "Events in background spectrum (M1): " << dDataIntegralM1 << endl;
  OutFile << "Events in background spectrum (M2): " << dDataIntegralM2 << endl;
  OutFile << "Events in background spectrum (M2Sum): " << dDataIntegralM2Sum << endl;
  OutFile << "Livetime of background: " << dLivetimeYr << endl;
  OutFile << "Total number of calls = " << dNumCalls << "\t" << "ChiSq/NDF = " << dChiSquare/dNDF << endl; // for M1 and M2
  OutFile << "ChiSq = " << dChiSquare << "\t" << "NDF = " << dNDF << endl;
  OutFile << "Probability = " << TMath::Prob(dChiSquare, dNDF ) << endl;

  OutFile << endl;
  OutFile << endl;

  double dROIRange = fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2570))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2570)) - fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2470)); 
  double d2nbbRange = fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2000))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2000)) - fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(500)); 
  // Output integrals of stuff for limits
  OutFile << "ROI range: " << fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2470)) << " " << fAdapDataHistoM1->GetBinLowEdge(fAdapDataHistoM1->FindBin(2570))+fAdapDataHistoM1->GetBinWidth(fAdapDataHistoM1->FindBin(2570)) << " keV" << endl; // 2470 to 2572
  OutFile << "Integral Data in ROI: " << fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt( fAdapDataHistoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total PDF in ROI: " << fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width") << " +/- " << sqrt( fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total Th-232 PDF in ROI: " << fModelTotAdapthM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt( fModelTotAdapthM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total U-238 PDF in ROI: " << fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total Co PDF in ROI: " << fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total Pb-210 PDF in ROI: " << fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total Po-210 PDF in ROI: " << fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;  
  // OutFile << "Integral Total 2NDBD PDF in ROI: " << fModelTotAdap2NDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdap2NDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << "Integral Total 0NDBD PDF in ROI: " << fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ) << " +/- " << sqrt(fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )) << endl;
  OutFile << endl;
  OutFile << "Integral Data in ROI (counts/keV): " << fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt( fAdapDataHistoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total PDF in ROI (counts/keV): " << fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt( fModelTotAdapM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total Th-232 PDF in ROI (counts/keV): " << fModelTotAdapthM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt( fModelTotAdapthM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total U-238 PDF in ROI (counts/keV): " << fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdapuM1->Integral( fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total Co PDF in ROI (counts/keV): " << fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdapcoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total Pb-210 PDF in ROI (counts/keV): " << fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdapSpbM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  // OutFile << "Integral Total Po-210 PDF in ROI (counts/keV): " << fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdapSpoM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;  
  // OutFile << "Integral Total 2NDBD PDF in ROI (counts/keV): " << fModelTotAdap2NDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdap2NDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  OutFile << "Integral Total 0NDBD PDF in ROI (counts/keV): " << fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" )/dROIRange << " +/- " << sqrt(fModelTotAdapNDBDM1->Integral(fAdapDataHistoM1->FindBin(2470),fAdapDataHistoM1->FindBin(2570), "width" ))/dROIRange << endl;
  // OutFile << "Number of 2nbb: " << fParameters[0]*dDataIntegralM1 << " +/- " << fParError[0]*dDataIntegralM1 << "\t 2nbb half life: " << (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[0]*dDataIntegralM1) << " +/- " << fParError[0]/fParameters[0] * (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[0]*dDataIntegralM1) << endl;
  // OutFile << "Counts in 2nbb (M1 + M2): " << fModelTotAdap2NDBDM1->Integral(1, fAdapDataHistoM1->FindBin(3000), "width") + fModelTotAdap2NDBDM2->Integral(1, fAdapDataHistoM2->FindBin(3000) , "width")/2 << "\t Half-Life " << (0.69314718056)*(4.726e25 * dLivetimeYr)/(fModelTotAdap2NDBDM1->Integral(1, fAdapDataHistoM1->FindBin(2700), "width") + fModelTotAdap2NDBDM2->Integral(1, fAdapDataHistoM2->FindBin(2700) , "width")/2) << endl;
  
  OutFile << "Residual RMS (Tot): " << dResidualRMSTot << endl;
  OutFile << "Residual RMS (M1): " << dResidualRMSM1 << "\t" << "Residual RMS (M2): " << dResidualRMSM2 << endl;
  OutFile << endl;
  OutFile << endl;

  // Outputs table of best fit values
  OutFile << "// Name - BestFit - BestFit Err - Integral - Integral Err" << endl;
  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
    OutFile << minuit->fCpnam[i] << Form(" & %.4e$\\pm$%.4e \t\t %.4e$\\pm$%.4e \\\\ ", fParameters[i], fParError[i], dDataIntegralM1*fParameters[i], dDataIntegralM1*fParError[i] ) << endl;
  }

  OutFile << endl;
  OutFile << endl;

  // Outputs table of activities of each cryostat element
  OutFile <<  "// Name - Activity(events) - Activity (Bq/kg) - Activity (Bq/cm2)" << endl;
  for(int i = 0; i < TBackgroundModel::dNParam; i++)
  {
    OutFile << minuit->fCpnam[i] << Form(" & %.4e \\pm %.4e", fParActivity[i], fParActivityErr[i]) << "\t \t" <<  Form("& %.4e \\pm %.4e", fParActivity[i]/fParMass[i]/dLivetime, fParActivityErr[i]/fParMass[i]/dLivetime) << "\t \t" << Form("& %.4e \\pm %.4e \\\\", fParActivity[i]/fParSurfaceArea[i]/dLivetime, fParActivityErr[i]/fParSurfaceArea[i]/dLivetime) << endl;
  }



  OutFile << endl;
  OutFile << endl;
  OutFile << endl;
  
  OutFile << "Contributions to ROI (M1) (counts/keV/y) +/- Err (counts/keV/y)" << endl;
  OutFile << minuit->fCpnam[0] << "& " << hAdapTeO22nuM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO22nuM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[1] << "& " << hAdapCuBox_CuFrameco60M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameco60M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[2] << "& " << hAdapTeO2th232onlyM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2th232onlyM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[3] << "& " << hAdapTeO2th230onlyM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2th230onlyM1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[4] << "& " << hAdapTeO2Sxth232M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxth232M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[5] << "& " << hAdapTeO2Sxra228pb208M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxra228pb208M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[6] << "& " << hAdapTeO2Sxu238th230M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxu238th230M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[7] << "& " << hAdapTeO2Sxth230onlyM1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxth230onlyM1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[8] << "& " << hAdapTeO2Sxra226pb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxra226pb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[9] << "& " << hAdapTeO2Sxpb210M1_1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxpb210M1_1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[10] << "& " << hAdapTeO2Sxpb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxpb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[11] << "& " << hAdapCuBox_CuFrameth232M1_10->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameth232M1_10->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[12] << "& " << hAdapCuBox_CuFrameu238M1_10->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameu238M1_10->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[13] << "& " << hAdapCuBox_CuFramepb210M1_01->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFramepb210M1_01->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[14] << "& " << hAdapCuBox_CuFramepb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFramepb210M1_001->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[15] << "& " << hAdapPbRomk40M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapPbRomk40M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[16] << "& " << hAdapOVCth232M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCth232M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[17] << "& " << hAdapOVCu238M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCu238M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[18] << "& " << hAdapOVCco60M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCco60M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[19] << "& " << hAdapOVCk40M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCk40M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[20] << "& " << hAdapExtPbbi210M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapExtPbbi210M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[21] << "& " << hAdapCuBox_CuFrameth232M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameth232M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[22] << "& " << hAdapCuBox_CuFrameu238M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameu238M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[23] << "& " << hAdapPbRomcs137M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" )/(dROIRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapPbRomcs137M1->Integral( fAdapDataHistoM1->FindBin(2470), fAdapDataHistoM1->FindBin(2570), "width" ))/(dROIRange*dLivetimeYr) << " \\\\" << endl;

  OutFile << endl;
  OutFile << endl;
  OutFile << endl;

  OutFile << "Contributions to 2nbb (M1) (counts/keV/y) +/- Err (counts/keV/y)" << endl;
  OutFile << "Number in 2nbb region" <<fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(fAdapDataHistoM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << endl;
  OutFile << minuit->fCpnam[0] << " & " << hAdapTeO22nuM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO22nuM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[1] << " & " << hAdapCuBox_CuFrameco60M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameco60M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[2] << " & " << hAdapTeO2th232onlyM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2th232onlyM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[3] << " & " << hAdapTeO2th230onlyM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2th230onlyM1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[4] << " & " << hAdapTeO2Sxth232M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxth232M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[5] << " & " << hAdapTeO2Sxra228pb208M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxra228pb208M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[6] << " & " << hAdapTeO2Sxu238th230M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxu238th230M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[7] << " & " << hAdapTeO2Sxth230onlyM1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxth230onlyM1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[8] << " & " << hAdapTeO2Sxra226pb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxra226pb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[9] << " & " << hAdapTeO2Sxpb210M1_1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxpb210M1_1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[10] << " & " << hAdapTeO2Sxpb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapTeO2Sxpb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[11] << " & " << hAdapCuBox_CuFrameth232M1_10->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameth232M1_10->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[12] << " & " << hAdapCuBox_CuFrameu238M1_10->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameu238M1_10->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[13] << " & " << hAdapCuBox_CuFramepb210M1_01->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFramepb210M1_01->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[14] << " & " << hAdapCuBox_CuFramepb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFramepb210M1_001->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[15] << " & " << hAdapPbRomk40M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapPbRomk40M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[16] << " & " << hAdapOVCth232M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCth232M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[17] << " & " << hAdapOVCu238M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCu238M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[18] << " & " << hAdapOVCco60M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCco60M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[19] << " & " << hAdapOVCk40M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapOVCk40M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[20] << " & " << hAdapExtPbbi210M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapExtPbbi210M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[21] << " & " << hAdapCuBox_CuFrameth232M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameth232M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[22] << " & " << hAdapCuBox_CuFrameu238M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapCuBox_CuFrameu238M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;
  OutFile << minuit->fCpnam[23] << " & " << hAdapPbRomcs137M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" )/(d2nbbRange*dLivetimeYr) << " $\\pm$ " << TMath::Sqrt(hAdapPbRomcs137M1->Integral( fAdapDataHistoM1->FindBin(500), fAdapDataHistoM1->FindBin(2000), "width" ))/(d2nbbRange*dLivetimeYr) << " \\\\" << endl;

  OutFile << endl;
  OutFile << endl;
  // for(int i = dFitMinBinM1; i < dFitMaxBinM1; i++)
  // {
  //   OutFile << fAdapDataHistoM1->GetBinContent(i) << "\t" << fAdapDataHistoM1->GetBinWidth(i) << "\t" << fModelTotAdapM1->GetBinContent(i) << "\t" << fModelTotAdapM1->GetBinWidth(i) << endl;
  // }

  OutFile << endl;
  OutFile << endl;
  OutFile << endl;
  OutFile << endl;  

  OutFile.close();
}

// Adds a random percentage of events of 2nbb into spectrum
void TBackgroundModel::SanityCheck()
{
  double dM1 = fDataHistoM1->Integral(1, 10000/dBinSize);
  double dM2 = fDataHistoM2->Integral(1, 10000/dBinSize);
  // Sanity check, adding to background a set amount of 2nbb events, see if code can reconstruct it properly
  fDataHistoM1->Add(hTeO22nuM1, 1.0*dM1);
  fDataHistoM2->Add(hTeO22nuM2, 1.0*dM2);

  fAdapDataHistoM1->Add(hAdapTeO22nuM1, 1.0*dM1);
  fAdapDataHistoM2->Add(hAdapTeO22nuM2, 1.0*dM2);
  // fDataHistoM2Sum->Add(hTeO22nuM2Sum, 0.5*dDataIntegralM2Sum);
}

// Only run in batch mode and make sure to have the 2nbb normalization FIXED
// Probably best to run Minuit in quiet mode as well
void TBackgroundModel::ProfileNLL(double fBestFitInit, double fBestFitChiSq)
{
  dBestChiSq = fBestFitChiSq; // Chi-Squared from best fit (for ProfileNLL calculation)
  // Do the fit now if no other tests are needed 
  nLoop = 0;
  for(int i = -15; i < 15; i++)
  {
    fInitValues.push_back(fBestFitInit + fBestFitInit/100*i);
  }


  OutPNLL.open(Form("%s/FitResults/ProfileNLL/ProfileNLL_%d_DR%d.C", dSaveDir.c_str(), tTime->GetDate(), dDataSet ));
  OutPNLL << "{" << endl;
  OutPNLL << "vector<double> dX;" << endl;
  OutPNLL << "vector<double> dT;" << endl;

  for(std::vector<double>::const_iterator iter = fInitValues.begin(); iter!=fInitValues.end(); iter++)
  {
    // cout << "Loop: " << nLoop << endl;
    DoTheFitAdaptive(*iter, 0);
    // LatexResultTable(*iter);
    cout << "delta ChiSq = " << dChiSquare - dBestChiSq << endl; // Needs to be entered, otherwise just 0
    OutPNLL << Form("dX.push_back(%f); dT.push_back(%f);", dChiSquare-dBestChiSq, (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[0]*dDataIntegralM1) ) << endl;

    nLoop++; // This is purely for file names and to keep track of number of loops
  }

  OutPNLL << "int n = dX.size();" << endl;
  OutPNLL << "double *y = &dX[0];" << endl;
  OutPNLL << "double *x = &dT[0];" << endl;
  OutPNLL << "TCanvas *cNLL = new TCanvas(\"cNLL\", \"cNLL\", 1200, 800);" << endl;
  OutPNLL << "TGraph *g1 = new TGraph(n, x, y);" << endl;
  OutPNLL << "g1->SetLineColor(kBlue);" << endl;
  OutPNLL << "g1->SetLineWidth(2);" << endl;
  OutPNLL << "g1->SetTitle(\"2#nu#beta#beta Profile Negative Log-Likelihood\");" << endl;
  OutPNLL << "g1->GetYaxis()->SetTitle(\"#Delta#chi^{2}\");" << endl;
  OutPNLL << "g1->GetXaxis()->SetTitle(\"t_{1/2} (y)\");" << endl;
  OutPNLL << "g1->Draw(\"AC\");" << endl;
  OutPNLL << "}" << endl;

  OutPNLL.close();
}

void TBackgroundModel::ProfileNLL2D(double fBestFitInit, double fBestFitInit2, double fBestFitChiSq)
{
  dBestChiSq = fBestFitChiSq; // Chi-Squared from best fit (for ProfileNLL calculation)
  // Do the fit now if no other tests are needed 
  nLoop = 0;
  for(int i = -5; i < 5; i++)
  {
    fInitValues.push_back(fBestFitInit + fBestFitInit/100*i);
  }
  for(int j = -5; j < 5; j++)
  {
    fInitValues2.push_back(fBestFitInit2 + fBestFitInit2/100*j);
  }


  OutPNLL.open(Form("%s/FitResults/ProfileNLL/ProfileNLL2D_%d_DR%d.C", dSaveDir.c_str(), tTime->GetDate(), dDataSet ));
  OutPNLL << "{" << endl;
  OutPNLL << "vector<double> dX;" << endl;
  OutPNLL << "vector<double> dY;" << endl;
  OutPNLL << "vector<double> dT;" << endl;

  for(std::vector<double>::const_iterator iter = fInitValues.begin(); iter != fInitValues.end(); iter++)
  {
    for(std::vector<double>::const_iterator iter2 = fInitValues2.begin(); iter2 != fInitValues2.end(); iter2++)
    {
    cout << "Step: " << *iter << " " << *iter2 << endl;    
    DoTheFitAdaptive(*iter, *iter2);
    cout << "delta ChiSq = " << dChiSquare - dBestChiSq << endl; // Needs to be entered, otherwise just 0
    OutPNLL << Form("dX.push_back(%f); dY.push_back(%.10f); dT.push_back(%f);", dChiSquare-dBestChiSq, *iter2, (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[1]*dDataIntegralM1) ) << endl;

    nLoop++; // This is purely for file names and to keep track of number of loops
    }
  }

  OutPNLL << "int n = dX.size();" << endl;
  OutPNLL << "double *z = &dX[0];" << endl;
  OutPNLL << "double *y = &dY[0];" << endl;
  OutPNLL << "double *x = &dT[0];" << endl;
  OutPNLL << "TCanvas *cNLL = new TCanvas(\"cNLL\", \"cNLL\", 1200, 800);" << endl;
  OutPNLL << "TGraph2D *g1 = new TGraph2D(n, x, y, z);" << endl;
  OutPNLL << "g1->SetLineColor(kBlue);" << endl;
  OutPNLL << "g1->SetLineWidth(2);" << endl;
  OutPNLL << "g1->SetTitle(\"2#nu#beta#beta 2D Profile Negative Log-Likelihood\");" << endl;
  OutPNLL << "g1->GetZaxis()->SetTitle(\"#Delta#chi^{2}\");" << endl;
  OutPNLL << "g1->GetYaxis()->SetTitle(\"External Bi-207\");" << endl;
  OutPNLL << "g1->GetXaxis()->SetTitle(\"t_{1/2} (y)\");" << endl;
  // OutPNLL << "g1->Draw(\"surf1\");" << endl;
  OutPNLL << endl;
  OutPNLL << endl;

  OutPNLL << "TH2D *h1;" << endl;
  OutPNLL << "h1 = g1->GetHistogram();" << endl;
  OutPNLL << "double levels[] = {1, 4, 9};" << endl;
  OutPNLL << "h1->SetContour(3, levels);" << endl;
  OutPNLL << "int colors[] = {kGreen, kYellow, kRed};" << endl;
  OutPNLL << "gStyle->SetPalette((sizeof(colors)/sizeof(Int_t)), colors);" << endl;
  OutPNLL << "h1->SetLineWidth(2);" << endl;
  OutPNLL << "h1->Draw(\"cont1\");" << endl;
  OutPNLL << "}" << endl;
  OutPNLL << endl;

  OutPNLL.close();
}


void TBackgroundModel::ToyFit(int fNumFits)
{
    OutToy.open(Form("%s/FitResults/ToyMC/Toy_%d.C", dSaveDir.c_str(), tTime->GetDate() ));

    OutToy << "{" << endl;
    OutToy << endl;

    OutToy << "hPullDist = new TH1D(\"hPullDist\", \"Pull Distribution\", 20, -5, 5);" << endl;
    OutToy << "hToy2nbbRate = new TH1D(\"hToy2nbbRate\", \"Toy Monte Carlo half-life fit values\", 100, 6.7e+20, 6.9e+20);" << endl;
    OutToy << "hToy2nbbError = new TH1D(\"hToy2nbbError\", \"Toy Monte Carlo half-life error values\", 50, 1.e+19, 1.2e+19);" << endl;
    OutToy << "hChiSquared = new TH1D(\"hChiSquared\", \"Distribution of Chi-Squared values\", 60, 0, 20);" << endl;


    cout << "Number of Loops " << fNumFits << endl;
    // Number of toy fits
    for(int i = 1; i <= 10; i++)
    {
      cout << "Toy: " << i << endl;
      fAdapDataHistoM1->Reset();
      fAdapDataHistoM2->Reset();
      
      fToyData = new TFile(Form("%s/Toy/ToyTest_p%d.root", dMCDir.c_str(), i));
      fAdapDataHistoM1 = (TH1D*)fToyData->Get("fAdapDataHistoM1");
      fAdapDataHistoM2 = (TH1D*)fToyData->Get("fAdapDataHistoM2");
    
    // fAdapDataHistoM1->Scale(1.41872984571782959e+05/fAdapDataHistoM1->Integral("width"));
    // fAdapDataHistoM2->Scale(2.66538587693367845e+04/fAdapDataHistoM2->Integral("width"));
      fAdapDataHistoM1->Scale(274875./fAdapDataHistoM1->Integral());
      fAdapDataHistoM2->Scale(97635.6/fAdapDataHistoM2->Integral());

      for(int j = 1; j <= dAdaptiveBinsM1; j++)
      {
        fAdapDataHistoM1->SetBinError(j, TMath::Sqrt(fAdapDataHistoM1->GetBinContent(j))/fAdapDataHistoM1->GetBinWidth(j));
      }
      for(int k = 1; k <= dAdaptiveBinsM2; k++)
      {
        fAdapDataHistoM2->SetBinError(k, TMath::Sqrt(fAdapDataHistoM2->GetBinContent(k))/fAdapDataHistoM2->GetBinWidth(k));
      }

    // M1 - 450231
    // M2 - 135452
    // M1 - Adap 450162
    // M2 - Adap 135379
    // Scale histograms to have same integral as before
    // dDataIntegralM1 = 450162;
    // dDataIntegralM2 = 135379;
    // dDataIntegralM1 = fAdapDataHistoM1->Integral("width");
    // dDataIntegralM2 = fAdapDataHistoM2->Integral("width");
      DoTheFitAdaptive(0,0);
      // Probably need to add a way to input the best-fit half-life
      OutToy << Form("hToy2nbbRate->Fill(%.5e);", (0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[1]*dDataIntegralM1) ) << endl;
      OutToy << Form("hToy2nbbError->Fill(%.5e);", fParError[1]/fParameters[1]*(0.69314718056)*(4.726e25*dLivetimeYr)/(fParameters[1]*dDataIntegralM1) ) << endl;
      OutToy << Form("hPullDist->Fill(%5e);", ((0.69314718056)*(4.726e25 * dLivetimeYr)/(fParameters[1]*dDataIntegralM1) - 6.80668e+20)/(fParError[1]/fParameters[1]*(0.69314718056)*(4.726e25*dLivetimeYr)/(fParameters[1]*dDataIntegralM1))  ) << endl;
      OutToy << Form("hChiSquared->Fill(%f);", dChiSquare) << endl;
    }
    OutToy << endl;
    OutToy << endl;
    OutToy << "TCanvas *c1 = new TCanvas(\"c1\", \"c1\", 1600, 1200);" << endl;
    OutToy << "c1->Divide(2,2);" << endl;
    OutToy << "c1->cd(1);" << endl;
    OutToy << "hPullDist->GetXaxis()->SetTitle(\"#Deltat_{1/2}/#sigma_{t_{1/2}}\");" << endl;
    OutToy << "hPullDist->Draw();" << endl;

    OutToy << "c1->cd(2);" << endl;    
    OutToy << "hToy2nbbRate->GetXaxis()->SetTitle(\"t_{1/2}\");" << endl;
    OutToy << "hToy2nbbRate->Draw();" << endl;

    OutToy << "c1->cd(3);" << endl;    
    OutToy << "hToy2nbbError->GetXaxis()->SetTitle(\"#sigma_{t_{1/2}}\");" << endl;
    OutToy << "hToy2nbbError->Draw();" << endl;

    OutToy << "c1->cd(4);" << endl;    
    OutToy << "hChiSquared->GetXaxis()->SetTitle(\"#chi^{2}\");" << endl;
    OutToy << "hChiSquared->Draw();" << endl;

    OutToy << endl;
    OutToy << endl;
    OutToy << "}" << endl;
    OutToy << endl;
    OutToy.close();
}

TH1D *TBackgroundModel::Kernal(TH1D *hMC, TH1D *hSMC)
{
  hSMC->Reset();
  cout << "Smearing: " << hMC->GetName() << endl;

  double dResolution = 1;

  double dNorm;
  double dSmearedValue;

  gaus = new TF1("gaus","gaus(0)", dMinEnergy, dMaxEnergy);


  for(int i = 1; i <= hMC->GetNbinsX(); i++)
  {
    for(int j = 1; j <= hMC->GetNbinsX(); j++)
    {
      // Area of gaussian
      dNorm = dBaseBinSize*hMC->GetBinContent(j)/(sqrt(2*TMath::Pi())*dResolution);

      // Set parameters of gaussian
      gaus->SetParameters(dNorm, hMC->GetBinCenter(j), dResolution);

      // Smeared contribution from 2nd derivative of gaussian centered at bin j for bin i 
      dSmearedValue = gaus->Derivative2( hSMC->GetBinCenter(i) );

      // Fill bin i with contribution from gaussian centered at bin j
      // cout << "Filling Bin: " << i << " with " << dSmearedValue << endl;
      hSMC->Fill(hSMC->GetBinCenter(i), dSmearedValue);
    }
  }
  
  return hSMC;
}
