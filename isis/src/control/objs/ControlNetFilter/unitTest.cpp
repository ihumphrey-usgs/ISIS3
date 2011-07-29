#include "Isis.h"
#include "IsisDebug.h"
#include "Application.h"
#include "ControlNet.h"
#include "ControlPoint.h"
#include "ControlMeasure.h"
#include "ControlNetFilter.h"
#include "iException.h"
#include "Preference.h"
#include "PvlGroup.h"
#include "PvlKeyword.h"
#include "SpecialPixel.h"
#include "TextFile.h"

#include <string>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

using namespace Isis;
using namespace std;

void PrintControlNetInfo(ControlNet &pCNet);

void IsisMain() {
  Preference::Preferences(true);
  cout << "UnitTest for ControlNetFilter ...." << endl << endl;

  UserInterface &ui = Application::GetUserInterface();

  ControlNet cnetOrig(ui.GetFilename("CNET"));
  ControlNet *cnet = new ControlNet(ui.GetFilename("CNET"));

  //test filters
  std::string sSerialFile = ui.GetFilename("FROMLIST");
  ControlNetFilter *cnetFilter = new ControlNetFilter(cnet, sSerialFile);

  // Point ResidualMagnitude Filter
  cout << "****************** Point_ResidualMagnitude Filter ******************" << endl;  
  PvlGroup filterGrp("Point_ResidualMagnitude");
  PvlKeyword keyword("LessThan", 1);
  filterGrp += keyword;
  cnetFilter->PointResMagnitudeFilter(filterGrp, false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";
  
  // PointEditLock Filter
  cout << "****************** Point_EditLock Filter ******************" << endl; 
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_EditLock");
  keyword= PvlKeyword("EditLock", true);
  filterGrp += keyword;
  cnetFilter->PointEditLockFilter(filterGrp, false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";  
  
  // PointMeasureEditLock Filter
  cout << "****************** Point_NumMeasuresEditLock Filter ******************" << endl;  
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_NumMeasuresEditLock");
  keyword = PvlKeyword("LessThan", 1);
  filterGrp += keyword;
  cnetFilter->PointNumMeasuresEditLockFilter(filterGrp, false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";
  
  // Pixel Shift
  cout << "****************** Point_PixelShift Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_PixelShift");
  keyword = PvlKeyword("LessThan", 10);
  filterGrp += keyword;
  keyword = PvlKeyword("GreaterThan", 1);
  filterGrp += keyword;
  cnetFilter->PointPixelShiftFilter(filterGrp, false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";
  
  // PointID Filter
  cout << "******************  Point_ID Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_IdExpression");
  keyword = PvlKeyword("Expression", "P01*");
  filterGrp += keyword;
  cnetFilter->PointIDFilter(filterGrp, false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // PointNumMeasures Filter
  cout << "****************** Point_NumMeasures Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_NumMeasures");  
  keyword = PvlKeyword("GreaterThan", 2);
  filterGrp += keyword;
  keyword = PvlKeyword("LessThan", 2);
  filterGrp += keyword;
  cnetFilter->PointMeasuresFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // PointsProperties Filter
  cout << "****************** Points_Properties Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_Properties");
  keyword = PvlKeyword("Ignore", false);
  filterGrp += keyword;
  keyword = PvlKeyword("PointType", "constraineD");
  filterGrp += keyword;
  cnetFilter->PointPropertiesFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Point_LatLon  Filter
  cout << "****************** Point_LatLon Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_LatLon");
  PvlKeyword keyword1("MinLat", -100);
  filterGrp += keyword1;

  PvlKeyword keyword2("MaxLat", 100);
  filterGrp += keyword2;

  PvlKeyword keyword3("MinLon", 0);
  filterGrp += keyword3;

  PvlKeyword keyword4("MaxLon", 238);
  filterGrp += keyword4;

  cnetFilter->PointLatLonFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Point_Distance Filter
  cout << "****************** Point_Distance Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_Distance");
  keyword = PvlKeyword("MaxDistance", 50000);
  filterGrp += keyword;
  keyword = PvlKeyword("Units", "meters");
  filterGrp += keyword;
  cnetFilter->PointDistanceFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Points_MeasureProperties Filter
  cout << "****************** Points_MeasureProperties Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_MeasureProperties");
  keyword = PvlKeyword("MeasureType", "Candidate");
  filterGrp += keyword;
  cnetFilter->PointMeasurePropertiesFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Filter Points by Goodness of Fit
   
  //cnet = new ControlNet(ui.GetFilename("CNET"));
  //cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  //filterGrp = PvlGroup("Point_GoodnessOfFit");
  //keyword = PvlKeyword("LessThan", 5);
 // filterGrp += keyword;
 // cnetFilter->PointGoodnessOfFitFilter(filterGrp,  false);
 // filterGrp.Clear()                                      ;
 // delete (cnet);
 // delete (cnetFilter);
 // cnetFilter = NULL;
 // cnet = NULL;
  

  // Point_CubeNames Filter
  cout << "****************** Point_CubeNames Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Point_CubeNames");
  //keyword = PvlKeyword("Cube1", "Clementine1/UVVIS/1994-04-05T12:17:21.337");
  //filterGrp += keyword;
  //keyword = PvlKeyword("Cube2", "Clementine1/UVVIS/1994-03-08T20:03:40.056");
  //filterGrp += keyword;
  keyword = PvlKeyword("Cube3", "Clementine1/UVVIS/1994-03-08T20:04:59.856");
  filterGrp += keyword;
  keyword = PvlKeyword("Cube4", "Clementine1/UVVIS/1994-04-05T12:18:07.957");
  filterGrp += keyword;
  cnetFilter->PointCubeNamesFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Cube Filters
  // Cube_NameExpression Filter
  cout << "****************** Cube_NameExpression Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Cube_NameExpression");
  keyword = PvlKeyword("Expression", "Clementine1/UVVIS/1994-04*");
  filterGrp += keyword;
  cnetFilter->CubeNameExpressionFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Cube_NumPoints Filter
  cout << "****************** Cube_NumPoints Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Cube_NumPoints");
  keyword = PvlKeyword("GreaterThan", 3);
  filterGrp += keyword;
  keyword = PvlKeyword("LessThan", 3);
  filterGrp += keyword;
  cnetFilter->CubeNumPointsFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";

  // Cube_Distance Filter
  cout << "****************** Cube_Distance Filter ******************" << endl;
  cnet = new ControlNet(ui.GetFilename("CNET"));
  cnetFilter = new ControlNetFilter(cnet, sSerialFile);
  filterGrp = PvlGroup("Cube_Distance");
  keyword = PvlKeyword("MaxDistance", 100000);
  filterGrp += keyword;
  keyword = PvlKeyword("Units=", "meters");
  filterGrp += keyword;
  cnetFilter->CubeDistanceFilter(filterGrp,  false);
  filterGrp.Clear();
  PrintControlNetInfo(*cnet);
  delete (cnet);
  delete (cnetFilter);
  cnetFilter = NULL;
  cnet = NULL;
  cout << "************************************************************************\n\n";
}

/**
 * Print Control Net info like Point ID and Measure SerialNum
 *
 * @author Sharmila Prasad (12/28/2010)
 *
 * @param pCNet - Control Net
 */
void PrintControlNetInfo(ControlNet &pCNet) {
  QList<QString> pointIds(pCNet.GetPointIds());
  qSort(pointIds);

  for (int i = 0; i < pointIds.size(); i++) {
    const QString &pointId = pointIds[i];
    ControlPoint *controlPoint = pCNet[pointId];

    cerr << "Control Point ID  " << pointId.toStdString() << endl;
    QList<QString> serialNums(controlPoint->getCubeSerialNumbers());
    qSort(serialNums);
    for (int j = 0; j < serialNums.size(); j++) {
      const QString &serialNum = serialNums.at(j);
      cerr << "   Measure SerialNum "
          << serialNum.toStdString() << endl;
    }
    cerr << endl;
  }
}
