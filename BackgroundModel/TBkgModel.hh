// Fitter class of the background model

#ifndef __TBkgModel__
#define __TBkgModel__
#include "TObject.h"
#include "TBkgModelSource.hh"
#include "TBkgModelParameter.hh"
#include "TFile.h"
#include "TH1.h"
#include "TH2C.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TF1.h"
#include "TChain.h"
#include "TCut.h"
#include "TGraphErrors.h"
#include "TMinuit.h"
#include "TPad.h"
#include "TString.h"
#include "TRandom3.h"
#include "TDatime.h"
#include "TMatrixT.h"
#include "TMatrixDEigen.h"
#include "TGaxis.h"
#include <TMinuitMinimizer.h>
#include "TROOT.h"
#include "TStyle.h"

#include "Math/Minimizer.h"
#include "Math/Factory.h"
#include "Math/Functor.h"
#include <vector>
#include <map>
#include <iostream>
#include <fstream>

class TBkgModel : public TBkgModelSource {

public:
	TBkgModel();

	TBkgModel(double fFitMin, double fFitMax, int fBinBase, int fDataset, bool fSave):TBkgModelSource(fFitMin, fFitMax, fBinBase, fDataSet)
	{
		bSave = fSave;
  		dNParam = 44; // number of fitting parameters
  		dNumCalls = 0;
  		minuit = new TMinuit(dNParam);
  		nLoop = 0;
	}

	virtual ~TBkgModel();

	TH1D* CalculateResidualsAdaptive(TH1D *h1, TH1D *h2, TH1D *hResid, int binMin, int binMax, int dMult);

	void CorrectForEfficiency();

	bool DoTheFitAdaptive(double f2nuValue, double fVariableValue);

	double GetChiSquareAdaptive();

	void GenerateParameters();

	void PrintParameters();

	void PrintParActivity();

	void ResetParameters();

	void SanityCheck();

	void SetParameters(int index, double value);

	void SetParEfficiency();
	
	int ShowNParameters();

	void UpdateModelAdaptive();


  	TBkgModelParameter *BkgParM1[100];
  	TBkgModelParameter *BkgParM2[100];

	double 	dChiSquare;

	std::map<std::string, int> dParMap;

private:

	TMinuit			*minuit;

	TH1D 			*hChiSquaredProgressM1;
	TH1D 			*hChiSquaredProgressM2;
	TH1D 			*hChiSquaredProgressM2Sum;

	TDatime 		*tTime;

	// Cut Efficiency
	// TF1 			*fEfficiency;
	TH1D 			*hEfficiency;
	TH1D 			*hEfficiencyM2;
	// TH1 			*hEfficiencyM1;

	int 			nLoop;

	TFile *fSaveResult;
	std::string 	dSaveDir;


	// Error Matrix
	// TMatrixT<double> 	*mCorrMatrix;

	bool			bFixedRes;
	bool			bAdaptiveBinning;
	bool 			bSave;

	int 			dNumCalls;
	int 			dMult;

	// Parameters

	int 				dNParam;
	int 				dNumFreeParameters;
	int 				dNDF;

	double				fParameters[139];
	double				fParError[139];
	double 				fParActivity[139]; // Integral of all events
	double 				fParActivityErr[139];
	double 				fParMass[139]; // Mass of all elements
	double 				fParSurfaceArea[139]; // Surface area of all elements
	double				fResolution[52];
	double 				fParEfficiencyM1[139]; // Efficiency of the parameters 
	double				dSecToYears;
	double				fMCEff[62];

 ClassDef(TBkgModel,1) // 
 };

#endif // __TBkgModel__