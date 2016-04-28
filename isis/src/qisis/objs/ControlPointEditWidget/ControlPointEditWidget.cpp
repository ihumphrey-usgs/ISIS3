#include "IsisDebug.h"

#include "ControlPointEditWidget.h"

#include <sstream>
#include <vector>
#include <iomanip>

#include <QtWidgets>
#include <QMessageBox>

#include "Application.h"
#include "Camera.h"
#include "ControlMeasureEditWidget.h"
#include "ControlMeasure.h"
#include "ControlMeasureLogData.h"
#include "ControlNet.h"
#include "ControlPoint.h"
#include "FileName.h"
#include "IException.h"
#include "Latitude.h"
#include "Longitude.h"
#include "MainWindow.h"
#include "MdiCubeViewport.h"
#include "Pvl.h"
#include "PvlEditDialog.h"
#include "SerialNumber.h"
#include "SerialNumberList.h"
#include "SpecialPixel.h"
#include "ToolPad.h"
#include "ViewportMainWindow.h"
#include "Workspace.h"

using namespace std;

namespace Isis {

  const int VIEWSIZE = 301;
  const int CHIPVIEWPORT_WIDTH = 310;


  /**
   * Consructs the ControlPointEditWidget widget
   *
   * @param parent Pointer to the parent widget for the ControlPointEditWidget
   *
   */
  ControlPointEditWidget::ControlPointEditWidget (QWidget *parent,
                                                  bool addMeasures) : QWidget(parent) {

    m_addMeasuresButton = addMeasures;
    m_netChanged = false;
    m_templateModified = false;

    m_parent = parent;

    createPointEditor(parent, m_addMeasuresButton);

    connect(this, SIGNAL(newControlNetwork(ControlNet *)),
            m_measureEditor, SIGNAL(newControlNetwork(ControlNet *)));
  }


  ControlPointEditWidget::~ControlPointEditWidget () {
    // TODO: Don't write settings in destructor, must do this earlier in close event
//  writeSettings();

  }



  /**
   * create the widget for editing control points
   *
   * @param parent Pointer to parent QWidget
   * @internal
   *   @history 2008-11-24  Jeannie Walldren - Added "Goodness of Fit" to right
   *                           and left measure info.
   *   @history 2008-11-26  Jeannie Walldren - Added "Number of Measures" to
   *                           QnetTool point information. Moved setWindowTitle()
   *                           command to updateNet() method. Added connection
   *                           between Ignore checkbox toggle() slot and
   *                           ignoreChanged() signal
   *   @history 2008-12-29 Jeannie Walldren - Disabled ground point check box and
   *                          commented out connection between check box and
   *                          setGroundPoint() method.
   *   @history 2008-12-30 Jeannie Walldren - Added connections to toggle
   *                          measures' Ignore check boxes if ignoreLeftChanged()
   *                          and ignoreRightChanged() are emitted. Replaced
   *                          reference to ignoreChanged() with
   *                          ignorePointChanged().
   *   @history 2010-06-03 Jeannie Walldren - Removed "std::" since "using
   *                          namespace std"
   */
  void ControlPointEditWidget::createPointEditor(QWidget *parent, bool addMeasures) {

    setWindowTitle("Control Point Editor");
    setObjectName("ControlPointEditWidget");
    connect(this, SIGNAL(destroyed(QObject *)), this, SLOT(clearEditPoint()));

    createActions();

    // create m_measureEditor first since we need to get its templateFileName
    // later
    m_measureEditor = new ControlMeasureEditWidget(parent, true, true);

    //  TODO Does this need to be moved to ControlNetEditMainWindow???  
    connect(this, SIGNAL(newControlNetwork(ControlNet *)),
        m_measureEditor, SIGNAL(newControlNetwork(ControlNet *)));


    connect(this, SIGNAL(stretchChipViewport(Stretch *, CubeViewport *)),
            m_measureEditor, SIGNAL(stretchChipViewport(Stretch *, CubeViewport *)));
    connect(m_measureEditor, SIGNAL(measureSaved()), this, SLOT(measureSaved()));
    connect(this, SIGNAL(measureChanged()), m_measureEditor, SLOT(colorizeSavePointButton()));
    connect(this, SIGNAL(netChanged()), this, SLOT(colorizeSaveNetButton()));

    QPushButton *addMeasure = NULL;
    if (m_addMeasuresButton) {
      addMeasure = new QPushButton("Add Measure(s) to Point");
      addMeasure->setToolTip("Add a new measure to the edit control point.");
      addMeasure->setWhatsThis("This allows a new control measure to be added "
         "to the currently edited control point.  A selection "
         "box with all cubes from the input list will be "
         "displayed with those that intersect with the "
         "control point highlighted.");
      connect(addMeasure, SIGNAL(clicked()), this, SLOT(addMeasure()));
    }

    m_savePoint = new QPushButton ("Save Point");
    m_savePoint->setToolTip("Save the edit control point to the control "
                            "network.");
    m_savePoint->setWhatsThis("Save the edit control point to the control "
                    "network which is loaded into memory in its entirety. "
                    "When a control point is selected for editing, "
                    "a copy of the point is made so that the original control "
                    "point remains in the network.");
    m_saveDefaultPalette = m_savePoint->palette();
    connect (m_savePoint, SIGNAL(clicked()), this, SLOT(savePoint()));

    m_saveNet = new QPushButton ("Save Control Net");
    m_saveNet->setToolTip("Save the control network.");
    m_savePoint->setWhatsThis("Save the control network.");
//    m_saveDefaultPalette = m_savePoint->palette();
//  This slot is needed because we cannot directly emit a signal with a ControlNet
//  argument after the "Save Net" push button is selected since the parameter list must match.
//  The saveNet slot will simply emit a signal with the ControlNet as the argument.
    connect (m_saveNet, SIGNAL(clicked()), this, SLOT(saveNet()));

    QHBoxLayout * saveMeasureLayout = new QHBoxLayout;
    if (m_addMeasuresButton) {
      saveMeasureLayout->addWidget(addMeasure);
    }
    else {
      saveMeasureLayout->addStretch();
    }
    saveMeasureLayout->addWidget(m_savePoint);
    saveMeasureLayout->addWidget(m_saveNet);

    m_cnetFileNameLabel = new QLabel("Control Network: " + m_cnetFileName);
    m_cnetFileNameLabel->setToolTip("Name of opened control network file.");
    m_cnetFileNameLabel->setWhatsThis("Name of opened control network file.");

    m_templateFileNameLabel = new QLabel("Template File: " +
        m_measureEditor->templateFileName());
    m_templateFileNameLabel->setToolTip("Sub-pixel registration template File.");
//  QString patternMatchDoc =
//          FileName("$ISISROOT/doc/documents/PatternMatch/PatternMatch.html").fileName();
//    m_templateFileNameLabel->setOpenExternalLinks(true);
    m_templateFileNameLabel->setWhatsThis("FileName of the sub-pixel "
                  "registration template.  Refer to $ISISROOT/doc/documents/"
                  "PatternMatch/PatternMatch.html for a description of the "
                  "contents of this file.");

    QVBoxLayout * centralLayout = new QVBoxLayout;

    centralLayout->addWidget(m_cnetFileNameLabel);
    centralLayout->addWidget(m_templateFileNameLabel);
    centralLayout->addWidget(createTopSplitter());
    centralLayout->addStretch();
    centralLayout->addWidget(m_measureEditor);
    centralLayout->addLayout(saveMeasureLayout);

    QWidget *centralWidget = new QWidget;
    centralWidget->setLayout(centralLayout);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setObjectName("ControlPointEditWidgetScroll");
    scrollArea->setWidget(centralWidget);
    scrollArea->setWidgetResizable(true);
    centralWidget->adjustSize();

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(scrollArea);
    setLayout(mainLayout);

//  connect(this, SIGNAL(controlPointChanged()), this, SLOT(paintAllViewports()));

//  readSettings();
  }


  //! creates everything above the ControlPointEdit
  QSplitter * ControlPointEditWidget::createTopSplitter() {

    QHBoxLayout * measureLayout = new QHBoxLayout;
    measureLayout->addWidget(createLeftMeasureGroupBox());
    measureLayout->addWidget(createRightMeasureGroupBox());

    QVBoxLayout * groupBoxesLayout = new QVBoxLayout;
    groupBoxesLayout->addWidget(createControlPointGroupBox());
    groupBoxesLayout->addStretch();
    groupBoxesLayout->addLayout(measureLayout);

    QWidget * groupBoxesWidget = new QWidget;
    groupBoxesWidget->setLayout(groupBoxesLayout);

    createTemplateEditorWidget();

    QSplitter * topSplitter = new QSplitter;
    topSplitter->addWidget(groupBoxesWidget);
    topSplitter->addWidget(m_templateEditorWidget);
    topSplitter->setStretchFactor(0, 4);
    topSplitter->setStretchFactor(1, 3);

    m_templateEditorWidget->hide();

    return topSplitter;
  }


  //! @returns The groupbox labeled "Control Point"
  QGroupBox * ControlPointEditWidget::createControlPointGroupBox() {

    // create left vertical layout
    m_ptIdValue = new QLabel;

// 2014-07-22 TLS cnetsuite TODO Handle ground control points SOON
    m_pointType = new QComboBox;
    for (int i=0; i<ControlPoint::PointTypeCount; i++) {
      m_pointType->insertItem(i, ControlPoint::PointTypeToString(
            (ControlPoint::PointType) i));
    }
    QHBoxLayout *pointTypeLayout = new QHBoxLayout;
    QLabel *pointTypeLabel = new QLabel("PointType:");
    pointTypeLayout->addWidget(pointTypeLabel);
    pointTypeLayout->addWidget(m_pointType);
    connect(m_pointType, SIGNAL(activated(int)),
      this, SLOT(setPointType(int)));
    m_numMeasures = new QLabel;
    m_pointAprioriLatitude = new QLabel;
    m_pointAprioriLongitude = new QLabel;
    m_pointAprioriRadius = new QLabel;
    m_pointAprioriLatitudeSigma = new QLabel;
    m_pointAprioriLongitudeSigma = new QLabel;
    m_pointAprioriRadiusSigma = new QLabel;
    QVBoxLayout * leftLayout = new QVBoxLayout;
    leftLayout->addWidget(m_ptIdValue);
    leftLayout->addLayout(pointTypeLayout);
    leftLayout->addWidget(m_pointAprioriLatitude);
    leftLayout->addWidget(m_pointAprioriLongitude);
    leftLayout->addWidget(m_pointAprioriRadius);
    leftLayout->addWidget(m_pointAprioriLatitudeSigma);
    leftLayout->addWidget(m_pointAprioriLongitudeSigma);
    leftLayout->addWidget(m_pointAprioriRadiusSigma);

    // create right vertical layout's top layout
    m_lockPoint = new QCheckBox("Edit Lock Point");
    connect(m_lockPoint, SIGNAL(clicked(bool)), this, SLOT(setLockPoint(bool)));
    m_ignorePoint = new QCheckBox("Ignore Point");
    connect(m_ignorePoint, SIGNAL(clicked(bool)),
      this, SLOT(setIgnorePoint(bool)));
    connect(this, SIGNAL(ignorePointChanged()), m_ignorePoint, SLOT(toggle()));
    m_pointLatitude = new QLabel;
    m_pointLongitude = new QLabel;
    m_pointRadius = new QLabel;

    QVBoxLayout * rightLayout = new QVBoxLayout;
    rightLayout->addWidget(m_numMeasures);
    rightLayout->addWidget(m_lockPoint);
    rightLayout->addWidget(m_ignorePoint);
    rightLayout->addWidget(m_pointLatitude);
    rightLayout->addWidget(m_pointLongitude);
    rightLayout->addWidget(m_pointRadius);


    QHBoxLayout * mainLayout = new QHBoxLayout;
    mainLayout->addLayout(leftLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(rightLayout);

    // create the groupbox
    QGroupBox * groupBox = new QGroupBox("Control Point");
    groupBox->setLayout(mainLayout);

    return groupBox;
  }


  //! @returns The groupbox labeled "Left Measure"
  QGroupBox * ControlPointEditWidget::createLeftMeasureGroupBox() {

    m_leftCombo = new QComboBox;
    m_leftCombo->view()->installEventFilter(this);
    m_leftCombo->setToolTip("Choose left control measure");
    m_leftCombo->setWhatsThis("Choose left control measure identified by "
                              "cube filename.");
    connect(m_leftCombo, SIGNAL(activated(int)),
            this, SLOT(selectLeftMeasure(int)));
    m_lockLeftMeasure = new QCheckBox("Edit Lock Measure");
    connect(m_lockLeftMeasure, SIGNAL(clicked(bool)),
            this, SLOT(setLockLeftMeasure(bool)));
    m_ignoreLeftMeasure = new QCheckBox("Ignore Measure");
    connect(m_ignoreLeftMeasure, SIGNAL(clicked(bool)),
            this, SLOT(setIgnoreLeftMeasure(bool)));
    connect(this, SIGNAL(ignoreLeftChanged()),
            m_ignoreLeftMeasure, SLOT(toggle()));
    m_leftReference = new QLabel();
    m_leftMeasureType = new QLabel();
    m_leftSampError = new QLabel();
    m_leftSampError->setToolTip("<strong>Jigsaw</strong> sample residual.");
    m_leftSampError->setWhatsThis("This is the sample residual for the left "
        "measure calculated by the application, "
        "<strong>jigsaw</strong>.");
    m_leftLineError = new QLabel();
    m_leftLineError->setToolTip("<strong>Jigsaw</strong> line residual.");
    m_leftLineError->setWhatsThis("This is the line residual for the left "
        "measure calculated by the application, "
        "<strong>jigsaw</strong>.");
    m_leftSampShift = new QLabel();
    m_leftSampShift->setToolTip("Sample shift between apriori and current");
    m_leftSampShift->setWhatsThis("The shift between the apriori sample and "
         "the current sample.  The apriori sample is set "
         "when creating a new measure.");
    m_leftLineShift = new QLabel();
    m_leftLineShift->setToolTip("Line shift between apriori and current");
    m_leftLineShift->setWhatsThis("The shift between the apriori line and "
         "the current line.  The apriori line is set "
         "when creating a new measure.");
    m_leftGoodness = new QLabel();
    m_leftGoodness->setToolTip("Goodness of Fit result from sub-pixel "
             "registration.");
    m_leftGoodness->setWhatsThis("Resulting Goodness of Fit from sub-pixel "
         "registration.");
    QVBoxLayout * leftLayout = new QVBoxLayout;
    leftLayout->addWidget(m_leftCombo);
    leftLayout->addWidget(m_lockLeftMeasure);
    leftLayout->addWidget(m_ignoreLeftMeasure);
    leftLayout->addWidget(m_leftReference);
    leftLayout->addWidget(m_leftMeasureType);
    leftLayout->addWidget(m_leftSampError);
    leftLayout->addWidget(m_leftLineError);
    leftLayout->addWidget(m_leftSampShift);
    leftLayout->addWidget(m_leftLineShift);
    leftLayout->addWidget(m_leftGoodness);

    QGroupBox * leftGroupBox = new QGroupBox("Left Measure");
    leftGroupBox->setLayout(leftLayout);

    return leftGroupBox;
  }


  //! @returns The groupbox labeled "Right Measure"
  QGroupBox * ControlPointEditWidget::createRightMeasureGroupBox() {

    // create widgets for the right groupbox
    m_rightCombo = new QComboBox;
    m_rightCombo->view()->installEventFilter(this);
    m_rightCombo->setToolTip("Choose right control measure");
    m_rightCombo->setWhatsThis("Choose right control measure identified by "
                               "cube filename.");
    connect(m_rightCombo, SIGNAL(activated(int)),
            this, SLOT(selectRightMeasure(int)));
    m_lockRightMeasure = new QCheckBox("Edit Lock Measure");
    connect(m_lockRightMeasure, SIGNAL(clicked(bool)),
            this, SLOT(setLockRightMeasure(bool)));
    m_ignoreRightMeasure = new QCheckBox("Ignore Measure");
    connect(m_ignoreRightMeasure, SIGNAL(clicked(bool)),
            this, SLOT(setIgnoreRightMeasure(bool)));
    connect(this, SIGNAL(ignoreRightChanged()),
            m_ignoreRightMeasure, SLOT(toggle()));
    m_rightReference = new QLabel();
    m_rightMeasureType = new QLabel();
    m_rightSampError = new QLabel();
    m_rightSampError->setToolTip("<strong>Jigsaw</strong> sample residual.");
    m_rightSampError->setWhatsThis("This is the sample residual for the right "
        "measure which was calculated by the application, "
        "<strong>jigsaw</strong>.");
    m_rightLineError = new QLabel();
    m_rightLineError->setToolTip("<strong>Jigsaw</strong> line residual.");
    m_rightLineError->setWhatsThis("This is the line residual for the right "
        "measure which was calculated by the application, "
        "<strong>jigsaw</strong>.");
    m_rightSampShift = new QLabel();
    m_rightSampShift->setToolTip(m_leftSampShift->toolTip());
    m_rightSampShift->setWhatsThis(m_leftSampShift->whatsThis());
    m_rightLineShift = new QLabel();
    m_rightLineShift->setToolTip(m_leftLineShift->toolTip());
    m_rightLineShift->setWhatsThis(m_leftLineShift->whatsThis());
    m_rightGoodness = new QLabel();
    m_rightGoodness->setToolTip(m_leftGoodness->toolTip());
    m_rightGoodness->setWhatsThis(m_leftGoodness->whatsThis());

    // create right groupbox
    QVBoxLayout * rightLayout = new QVBoxLayout;
    rightLayout->addWidget(m_rightCombo);
    rightLayout->addWidget(m_lockRightMeasure);
    rightLayout->addWidget(m_ignoreRightMeasure);
    rightLayout->addWidget(m_rightReference);
    rightLayout->addWidget(m_rightMeasureType);
    rightLayout->addWidget(m_rightSampError);
    rightLayout->addWidget(m_rightLineError);
    rightLayout->addWidget(m_rightSampShift);
    rightLayout->addWidget(m_rightLineShift);
    rightLayout->addWidget(m_rightGoodness);

    QGroupBox * rightGroupBox = new QGroupBox("Right Measure");
    rightGroupBox->setLayout(rightLayout);

    return rightGroupBox;
  }


  //! Creates the Widget which contains the template editor and its toolbar
  void ControlPointEditWidget::createTemplateEditorWidget() {

    QToolBar *toolBar = new QToolBar("Template Editor ToolBar");

    toolBar->addAction(m_openTemplateFile);
    toolBar->addSeparator();
    toolBar->addAction(m_saveTemplateFile);
    toolBar->addAction(m_saveTemplateFileAs);

    m_templateEditor = new QTextEdit;
    connect(m_templateEditor, SIGNAL(textChanged()), this,
        SLOT(setTemplateModified()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(toolBar);
    mainLayout->addWidget(m_templateEditor);

    m_templateEditorWidget = new QWidget;
    m_templateEditorWidget->setLayout(mainLayout);
  }



  void ControlPointEditWidget::createActions() {

    m_closePointEditor = new QAction(QIcon(FileName("base/icons/fileclose.png").expanded()),
                                   "&Close", this);
    m_closePointEditor->setToolTip("Close this window");
    m_closePointEditor->setStatusTip("Close this window");
    m_closePointEditor->setShortcut(Qt::ALT + Qt::Key_F4);
    QString whatsThis = "<b>Function:</b> Closes the Match Tool window for this point "
        "<p><b>Shortcut:</b> Alt+F4 </p>";
    m_closePointEditor->setWhatsThis(whatsThis);
    connect(m_closePointEditor, SIGNAL(activated()), this, SLOT(close()));

    m_showHideTemplateEditor = new QAction(QIcon(FileName("base/icons/view_text.png").expanded()),
                                           "&View/edit registration template", this);
    m_showHideTemplateEditor->setCheckable(true);
    m_showHideTemplateEditor->setToolTip("View and/or edit the registration template");
    m_showHideTemplateEditor->setStatusTip("View and/or edit the registration template");
    whatsThis = "<b>Function:</b> Displays the curent registration template.  "
       "The user may edit and save changes under a chosen filename.";
    m_showHideTemplateEditor->setWhatsThis(whatsThis);
    connect(m_showHideTemplateEditor, SIGNAL(activated()), this,
        SLOT(showHideTemplateEditor()));

    m_saveChips = new QAction(QIcon(FileName("base/icons/window_new.png").expanded()),
                              "Save registration chips", this);
    m_saveChips->setToolTip("Save registration chips");
    m_saveChips->setStatusTip("Save registration chips");
    whatsThis = "<b>Function:</b> Save registration chips to file.  "
       "Each chip: pattern, search, fit will be saved to a separate file.";
    m_saveChips->setWhatsThis(whatsThis);
    connect(m_saveChips, SIGNAL(activated()), this, SLOT(saveChips()));

    m_openTemplateFile = new QAction(QIcon(FileName("base/icons/fileopen.png").expanded()),
                                     "&Open registration template", this);
    m_openTemplateFile->setToolTip("Set registration template");
    m_openTemplateFile->setStatusTip("Set registration template");
    whatsThis = "<b>Function:</b> Allows user to select a new file to set as "
        "the registration template";
    m_openTemplateFile->setWhatsThis(whatsThis);
    connect(m_openTemplateFile, SIGNAL(activated()), this, SLOT(openTemplateFile()));

    m_saveTemplateFile = new QAction(QIcon(FileName("base/icons/mActionFileSave.png").expanded()),
                                     "&Save template file", this);
    m_saveTemplateFile->setToolTip("Save the template file");
    m_saveTemplateFile->setStatusTip("Save the template file");
    m_saveTemplateFile->setWhatsThis("Save the registration template file");
    connect(m_saveTemplateFile, SIGNAL(triggered()), this,
        SLOT(saveTemplateFile()));

    m_saveTemplateFileAs = new QAction(QIcon(FileName("base/icons/mActionFileSaveAs.png").expanded()),
                                       "&Save template as...", this);
    m_saveTemplateFileAs->setToolTip("Save the template file as");
    m_saveTemplateFileAs->setStatusTip("Save the template file as");
    m_saveTemplateFileAs->setWhatsThis("Save the registration template file as");
    connect(m_saveTemplateFileAs, SIGNAL(triggered()), this,
        SLOT(saveTemplateFileAs()));
  }



  void ControlPointEditWidget::setSerialNumberList(SerialNumberList *snList) {

    // TODO   If network & snList already exists do some error checking
    m_serialNumberList = snList;
  }



  /**
   * New control network being edited
   *
   * @param cnet ControlNet * 
   * @param filename Qstring,  Need filename to write to widget label.  ControlNet doesn't 
   *                       contain a filename. 
   * @internal
  */  
  void ControlPointEditWidget::setControlNet(ControlNet *cnet, QString cnetFilename) {
    //qDebug()<<"ControlPointEditWidget::setControlNet cnet = "<<cnet<<"   filename = "<<cnetFilename;
    //  TODO  more error checking
    m_controlNet = cnet;
    m_cnetFileName = cnetFilename;
//  setWindowTitle("Control Point Editor- Control Network File: " + cnetFilename);

    //qDebug()<<"ControlPointEditWidget::setControlNet  cnetFilename = "<<cnetFilename<<"  cnet = "<<cnet;
    emit newControlNetwork(cnet);
  }



  void ControlPointEditWidget::setEditPoint(ControlPoint *controlPoint) {
    //qDebug()<<"ControlPointEditWidget::setEditPoint incoming ptId = "<<controlPoint->GetId();

    if (m_editPoint != NULL && m_editPoint->Parent() == NULL) {
      delete m_editPoint;
      m_editPoint = NULL;
    }
    m_editPoint = new ControlPoint;
    *m_editPoint = *controlPoint;
    //qDebug()<<"ControlPointEditWidget::setEditPoint m_editPoint Id = "<<m_editPoint->GetId();

    loadPoint();
    this->setVisible(true);
    this->raise();
    loadTemplateFile(m_measureEditor->templateFileName());

    // New point loaded, make sure Save Measure Button text is default
    m_savePoint->setPalette(m_saveDefaultPalette);
  }



  /**
   * This method is connected with the measureSaved() signal from ControlMeasureEditWidget.
   *
   * @internal
   *   @history 2008-11-26 Jeannie Walldren - Added message box to warn the user
   *                          that they are saving an "Ignore" point and ask
   *                          whether they would like to set Ignore=false. This
   *                          emits an ignoreChanged() signal so the "Ignore" box
   *                          in the window is unchecked.
   *   @history 2008-12-30 Jeannie Walldren - Modified to set measures in
   *                          viewports to Ignore=False if when saving, the user
   *                          chooses to set a point's Ignore=False. Replaced
   *                          reference to ignoreChanged() with
   *                          ignorePointChanged().
   *   @history 2008-12-31 Jeannie Walldren - Added question box to ask user
   *                          whether the current reference measure should be
   *                          replaced with the measure in the left viewport.
   *   @history 2010-01-27 Jeannie Walldren - Added question box to warn the user
   *                          that they are saving an "Ignore" measure and ask
   *                          whether they would like to set Ignore=False. This
   *                          emits an ignoreRightChanged() signal so the "Ignore"
   *                          box in the window is unchecked. Modified Ignore
   *                          Point message for clarity.
   *   @history 2010-11-19 Tracie Sucharski - Renamed from pointSaved.
   *   @history 2011-03-03 Tracie Sucharski - Do not save left measure unless
   *                          the ignore flag was changed, that is the only
   *                          change allowed on the left measure.
   *   @history 2011-04-20 Tracie Sucharski - If left measure equals right
   *                          measure, copy right into left.  Also if EditLock
   *                          true and user does not want to change, then
   *                          do not save measure.  Remove signals
   *                          EditPointChanged and netChanged, since these
   *                          should only happen when the point is saved.
   *   @history 2011-07-01 Tracie Sucharski - Fixed bug where the edit measure
   *                          EditLocked=True, but the original measure was
   *                          False, and we woouldn't allow the measure to be
   *                          saved.
   *   @history 2011-07-25 Tracie Sucharski - Removed editPointChanged signal
   *                          since the editPoint is not changed.  This helped
   *                          with match windows blinking due to refresh.
   *   @history 2011-09-22 Tracie Sucharski - When checking ignore status
   *                          on right measure, check both original and edit
   *                          measure.
   *   @history 2012-04-09 Tracie Sucharski - When checking if left measure
   *                          editLock has changed, use measure->IsEditLocked()
   *                          instead of this classes IsMeasureLocked() because
   *                          it checks the m_editPoint measure instead of
   *                          the measure loaded into the point editor.
   *   @history 2012-04-26 Tracie Sucharski - cleaned up, moved reference checking
   *                          and updating ground surface point to new methods.
   *   @history 2012-05-07 Tracie Sucharski - Removed code to re-load left measure if
   *                          left and right are the same, this is already handled in
   *                          ControlPointEdit::saveMeasure.
   *   @history 2012-06-12 Tracie Sucharski - Change made on 2012-04-26 caused a bug where
   *                          if no ground is loaded the checkReference was not being called and
   *                          reference measure could not be changed and there was no warning
   *                          printed.
   */
  void ControlPointEditWidget::measureSaved() {

    // Read original measures from the network for comparison with measures
    // that have been edited
    ControlMeasure *origLeftMeasure =
                m_editPoint->GetMeasure(m_leftMeasure->GetCubeSerialNumber());
    ControlMeasure *origRightMeasure =
                m_editPoint->GetMeasure(m_rightMeasure->GetCubeSerialNumber());
    // Neither measure has changed, return
    if (*origLeftMeasure == *m_leftMeasure && *origRightMeasure == *m_rightMeasure) {
      return;
    }

    if (m_editPoint->IsIgnored()) {
      QString message = "You are saving changes to a measure on an ignored ";
      message += "point.  Do you want to set Ignore = False on the point and ";
      message += "both measures?";
      switch (QMessageBox::question(this, "Match Tool Save Measure",
                                    message, "&Yes", "&No", 0, 0)) {
        // Yes:  set Ignore=false for the point and measures and save point
        case 0:
          m_editPoint->SetIgnored(false);
          emit ignorePointChanged();
          if (m_leftMeasure->IsIgnored()) {
            m_leftMeasure->SetIgnored(false);
            emit ignoreLeftChanged();
          }
          if (m_rightMeasure->IsIgnored()) {
            m_rightMeasure->SetIgnored(false);
            emit ignoreRightChanged();
          }
        // No: keep Ignore=true and save measure
        case 1:
          break;
      }
    }

    bool savedAMeasure = false;
    //  Error check both measures for edit lock, ignore status and reference
    bool leftChangeOk = validateMeasureChange(m_leftMeasure);
    if (leftChangeOk) {
      m_leftMeasure->SetChooserName(Application::UserName());
      *origLeftMeasure = *m_leftMeasure;
      savedAMeasure = true;
    }
    bool rightChangeOk = validateMeasureChange(m_rightMeasure);
    if (rightChangeOk) {
      m_rightMeasure->SetChooserName(Application::UserName());
      *origRightMeasure = *m_rightMeasure;
      savedAMeasure = true;
    }

    // If left measure == right measure, update left
    if (m_leftMeasure->GetCubeSerialNumber() == m_rightMeasure->GetCubeSerialNumber()) {
      *m_leftMeasure = *m_rightMeasure;
      //  Update left measure of measureEditor
      m_measureEditor->setLeftMeasure (m_leftMeasure, m_leftCube.data(),
                                     m_editPoint->GetId());
    }

    //  Change Save Point button text to red
    if (savedAMeasure) {
      colorizeSavePointButton();
    }

    // TODO  7-31-14 Commented out.  We renamed editPointChanged to controlPointChanged.  However
    //                  do we still need a private signal, editPointChanged?
//  emit editPointChanged(m_editPoint->GetId());

    // Update measure info
    updateLeftMeasureInfo();
    updateRightMeasureInfo();
    loadMeasureTable();

  }



  bool ControlPointEditWidget::validateMeasureChange(ControlMeasure *m) {


    // Read original measures from the network for comparison with measures
    // that have been edited
    ControlMeasure *origMeasure =
                m_editPoint->GetMeasure(m->GetCubeSerialNumber());

    //  If measure hasn't changed, return false, to keep original

    if (*m == *origMeasure) return false;

    // Is measure on Left or Right?  This is needed to print correct information
    // to users in identifying the measure and for updating information widgets.
    QString side = "right";
    if (m->GetCubeSerialNumber() == m_leftMeasure->GetCubeSerialNumber()) {
      side = "left";
    }

    //  Only print error if both original measure in network and the current
    //  edit measure are both editLocked and measure has changed.  If only the edit measure is
    //  locked, then user just locked and it needs to be saved.
    //  Do not use this classes IsMeasureLocked since we actually want to
    //  check the original againsted the edit measure and we don't care
    //  if this is a reference measure.  The check for moving a reference is
    //  done below.
    if (origMeasure->IsEditLocked() && m->IsEditLocked()) {
      QString message = "The " + side + " measure is editLocked ";
      message += "for editing.  Do you want to set EditLock = False for this ";
      message += "measure?";
      int response = QMessageBox::question(this, "Match Tool Save Measure",
                                    message, QMessageBox::Yes | QMessageBox::No);
      // Yes:  set EditLock=false for the right measure
      if (response == QMessageBox::Yes) {
        m->SetEditLock(false);
        if (side == "left") {
          m_lockLeftMeasure->setChecked(false);
        }
        else {
          m_lockRightMeasure->setChecked(false);
        }
      }
      // No:  keep EditLock=true and do NOT save measure
      else {
        return false;
      }
    }

    if (origMeasure->IsIgnored() && m->IsIgnored()) {
      QString message = "The " + side + "measure is ignored.  ";
      message += "Do you want to set Ignore = False on the measure?";
      switch(QMessageBox::question(this, "Match Tool Save Measure",
                                   message, "&Yes", "&No", 0, 0)){
        // Yes:  set Ignore=false for the right measure and save point
        case 0:
            m->SetIgnored(false);
            if (side == "left") {
              emit ignoreLeftChanged();
            }
            else {
              emit ignoreRightChanged();
            }
        // No:  keep Ignore=true and save point
        case 1:
          break;;
      }
    }

    //  If measure is explicit reference and it has moved,warn user
    ControlMeasure *refMeasure = m_editPoint->GetRefMeasure();
    if (m_editPoint->IsReferenceExplicit()) {
      if (refMeasure->GetCubeSerialNumber() == m->GetCubeSerialNumber()) {
        if (m->GetSample() != origMeasure->GetSample() || m->GetLine() != origMeasure->GetLine()) {
          QString message = "You are making a change to the reference measure.  You ";
          message += "may need to move all of the other measures to match the new ";
          message += " coordinate of the reference measure.  Do you really want to ";
          message += " change the reference measure's location? ";
          switch(QMessageBox::question(this, "Match Tool Save Measure",
                                       message, "&Yes", "&No", 0, 0)){
            // Yes:  Save measure
            case 0:
              break;
            // No:  keep original reference, return without saving
            case 1:
              loadPoint();
              return false;
          }
        }
      }
      //  New reference measure
      else if (side == "left" && (refMeasure->GetCubeSerialNumber() != m->GetCubeSerialNumber())) {
        QString message = "This point already contains a reference measure.  ";
        message += "Would you like to replace it with the measure on the left?";
        int  response = QMessageBox::question(this,
                                  "Match Tool Save Measure", message,
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::Yes);
        // Replace reference measure
        if (response == QMessageBox::Yes) {
          //  Update measure file combo boxes:  old reference normal font,
          //    new reference bold font
          QString file = m_serialNumberList->fileName(m_leftMeasure->GetCubeSerialNumber());
          QString fname = FileName(file).name();
          int iref = m_leftCombo->findText(fname);

          //  Save normal font from new reference measure
          QVariant font = m_leftCombo->itemData(iref,Qt::FontRole);
          m_leftCombo->setItemData(iref,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);
          iref = m_rightCombo->findText(fname);
          m_rightCombo->setItemData(iref,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);

          file = m_serialNumberList->fileName(refMeasure->GetCubeSerialNumber());
          fname = FileName(file).name();
          iref = m_leftCombo->findText(fname);
          m_leftCombo->setItemData(iref,font,Qt::FontRole);
          iref = m_rightCombo->findText(fname);
          m_rightCombo->setItemData(iref,font,Qt::FontRole);

          m_editPoint->SetRefMeasure(m->GetCubeSerialNumber());
        }
      }
    }
    else {
      //  No explicit reference, If left, set explicit reference 
      if (side == "left") {
        m_editPoint->SetRefMeasure(m->GetCubeSerialNumber());
      }
    }

    //  All test pass, return true (ok to change measure)
    return true;


  }



  /*
  * Change which measure is the reference.
  *  
  * @author 2012-04-26 Tracie Sucharski - moved funcitonality from measureSaved
  *
  * @internal
  *   @history 2012-06-12 Tracie Sucharski - Moved check for ground loaded on left from the
  *                          measureSaved method.
  */
  void ControlPointEditWidget::checkReference() {

    // Check if ControlPoint has reference measure, if reference Measure is
    // not the same measure that is on the left chip viewport, set left
    // measure as reference.
    ControlMeasure *refMeasure = m_editPoint->GetRefMeasure();
    if (refMeasure->GetCubeSerialNumber() != m_leftMeasure->GetCubeSerialNumber()) {
      QString message = "This point already contains a reference measure.  ";
      message += "Would you like to replace it with the measure on the left?";
      int  response = QMessageBox::question(this,
                                "Match Tool Save Measure", message,
                                QMessageBox::Yes | QMessageBox::No,
                                QMessageBox::Yes);
      // Replace reference measure
      if (response == QMessageBox::Yes) {
        //  Update measure file combo boxes:  old reference normal font,
        //    new reference bold font
        QString file = m_serialNumberList->fileName(m_leftMeasure->GetCubeSerialNumber());
        QString fname = FileName(file).name();
        int iref = m_leftCombo->findText(fname);

        //  Save normal font from new reference measure
        QVariant font = m_leftCombo->itemData(iref,Qt::FontRole);
        m_leftCombo->setItemData(iref,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);
        iref = m_rightCombo->findText(fname);
        m_rightCombo->setItemData(iref,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);

        file = m_serialNumberList->fileName(refMeasure->GetCubeSerialNumber());
        fname = FileName(file).name();
        iref = m_leftCombo->findText(fname);
        m_leftCombo->setItemData(iref,font,Qt::FontRole);
        iref = m_rightCombo->findText(fname);
        m_rightCombo->setItemData(iref,font,Qt::FontRole);

        m_editPoint->SetRefMeasure(m_leftMeasure->GetCubeSerialNumber());
      }

          // ??? Need to set rest of measures to Candiate and add more warning. ???//
    }


  }



  /**
   * Save edit point to the Control Network.  Up to this point the point is
   * simply a copy and does not exist in the network.
   *
   * @author 2010-11-19 Tracie Sucharski
   *
   * @internal
   * @history 2011-04-20 Tracie Sucharski - If EditLock set, prompt for changing
   *                        and do not save point if editLock not changed.
   * @history 2011-07-05 Tracie Sucharski - Move point EditLock error checking
   *                        to individual point parameter setting methods, ie.
   *                        SetPointType, SetIgnorePoint.
   *
   */
  void ControlPointEditWidget::savePoint() {

    if (m_editPoint->HasAprioriCoordinates()) {
      vector<Distance> targetRadii = m_controlNet->GetTargetRadii();

      ControlMeasure *refMeasure = m_editPoint->GetRefMeasure();

      refMeasure->Camera()->SetImage(refMeasure->GetSample(), refMeasure->GetLine());

      double lat = refMeasure->Camera()->UniversalLatitude();
      double lon = refMeasure->Camera()->UniversalLongitude();
      double radius = refMeasure->Camera()->LocalRadius().meters();

      SurfacePoint aprioriPt = m_editPoint->GetAprioriSurfacePoint();
      aprioriPt.SetRadii(Distance(targetRadii[0]),
                         Distance(targetRadii[1]),
                         Distance(targetRadii[2]));
      Distance latSigma = aprioriPt.GetLatSigmaDistance();
      Distance lonSigma = aprioriPt.GetLonSigmaDistance();
      Distance radiusSigma = aprioriPt.GetLocalRadiusSigma();
      aprioriPt.SetSphericalCoordinates(Latitude(lat, Angle::Degrees),
                                        Longitude(lon, Angle::Degrees),
                                        Distance(radius, Distance::Meters));
      aprioriPt.SetSphericalSigmasDistance(latSigma, lonSigma, radiusSigma);
      m_editPoint->SetAprioriSurfacePoint(aprioriPt);

      updateSurfacePointInfo ();
    }

    //  Make a copy of edit point for updating the control net since the edit
    //  point is still loaded in the point editor.
    ControlPoint *updatePoint = new ControlPoint;
    *updatePoint = *m_editPoint;

    //  If edit point exists in the network, save the updated point.  If it
    //  does not exist, add it.
    if (m_controlNet->ContainsPoint(updatePoint->GetId())) {
      ControlPoint *p;
      p = m_controlNet->GetPoint(QString(updatePoint->GetId()));
      *p = *updatePoint;
      delete updatePoint;
      updatePoint = NULL;
      //qDebug()<<"ControlPOintEditWidget::savePoint before point Changed signal";
      emit controlPointChanged(m_editPoint->GetId());
    }
    else {
      m_controlNet->AddPoint(updatePoint);
      //qDebug()<<"ControlPOintEditWidget::savePoint before point added signal";
      emit controlPointAdded(m_editPoint->GetId());
    }

    //  Change Save Measure button text back to default
    m_savePoint->setPalette(m_saveDefaultPalette);

    // At exit, or when opening new net, use for prompting user for a save
    m_netChanged = true;
    emit netChanged();
    //   Refresh chipViewports to show new positions of controlPoints
    m_measureEditor->refreshChips();
  }



  /**
   * Set the point type
   * @param pointType int Index from point type combo box
   *
   * @author 2011-07-05 Tracie Sucharski
   *
   * @internal 
   *   @history 2013-12-06 Tracie Sucharski - If changing point type to constrained or fixed make
   *                           sure reference measure is not ignored.
   */
#if 0  // 2014-07-22 TLS cnetsuite TODO Handle ground control points SOON
  void ControlPointEditWidget::setPointType (int pointType) {
    if (m_editPoint == NULL) return;

    //  If pointType is equal to current type, nothing to do
    if (m_editPoint->GetType() == pointType) return;

    if (pointType != ControlPoint::Free && m_leftMeasure->IsIgnored()) {
      m_pointType->setCurrentIndex((int) m_editPoint->GetType());
      QString message = "The reference measure is Ignored.  Unset the Ignore flag on the ";
      message += "reference measure before setting the point type to Constrained or Fixed.";
      QMessageBox::warning(m_parent, "Ignored Reference Measure", message);
      return;
    }

// 07-07-2014 TLS cnetsuite TODO  This needs to be uncommented & handled correctly
    bool unloadGround = false;
//    if (m_editPoint->GetType() != ControlPoint::Free && pointType == ControlPoint::Free)
//      unloadGround = true;

    ControlPoint::Status status = m_editPoint->SetType((ControlPoint::PointType) pointType);
    if (status == ControlPoint::PointLocked) {
      m_pointType->setCurrentIndex((int) m_editPoint->GetType());
      QString message = "This control point is edit locked.  The point type cannot be changed.  You ";
      message += "must first unlock the point by clicking the check box above labeled ";
      message += "\"Edit Lock Point\".";
      QMessageBox::warning(m_parent, "Point Locked", message);
      return;
    }

// 07-07-2014 TLS cnetsuite TODO  This needs to be uncommented & handled correctly
    //  If ground loaded, read temporary ground measure to the point
    if (pointType != ControlPoint::Free && m_groundOpen) {
//    loadGroundMeasure();
//    m_measureEditor->colorizeSaveButton();
    }
    //  If going from constrained or fixed to free, unload the ground measure.
    else if (unloadGround) {
      // Find in point and delete, it will be re-created with current
      // ground source if this is a fixed point
      if (m_editPoint->HasSerialNumber(m_groundSN)) {
    m_editPoint->Delete(m_groundSN);
      }

      loadPoint();
      m_measureEditor->colorizeSaveButton();
    }

  }
#endif


  /**
   * Set point's "EditLock" keyword to the value of the input parameter.
   * @param ignore Boolean value that determines the EditLock value for this
   *               point.
   *
   * @author 2011-03-07 Tracie Sucharski
   *
   * @internal
   */
  void ControlPointEditWidget::setLockPoint (bool lock) {
    if (m_editPoint == NULL) return;

    m_editPoint->SetEditLock(lock);
    colorizeSavePointButton();
  }



  /**
   * Set point's "Ignore" keyword to the value of the input
   * parameter.
   * @param ignore Boolean value that determines the Ignore value for this point.
   *
   * @internal
   * @history 2010-12-15 Tracie Sucharski - Remove netChanged, the point is
   *                        not changed in the net unless "Save Point" is
   *                        selected.
   */
  void ControlPointEditWidget::setIgnorePoint (bool ignore) {
    if (m_editPoint == NULL) return;

    ControlPoint::Status status = m_editPoint->SetIgnored(ignore);
    if (status == ControlPoint::PointLocked) {
      m_ignorePoint->setChecked(m_editPoint->IsIgnored());
      QString message = "Unable to change Ignored on point.  Set EditLock ";
      message += " to False.";
      QMessageBox::critical(this, "Error", message);
      return;
    }
    colorizeSavePointButton();
  }




  /**
   * Set the "EditLock" keyword of the measure shown in the left viewport to the
   * value of the input parameter.
   *
   * @param ignore Boolean value that determines the EditLock value for the left
   *               measure.
   *
   * @author 2011-03-07 Tracie Sucharski
   *
   * @internal
   * @history 2011-06-27 Tracie Sucharski - emit signal indicating a measure
   *                        parameter has changed.
   * @history 2012-04-16 Tracie Sucharski - When attempting to un-lock a measure
   *                        print error if point is locked.
   */
  void ControlPointEditWidget::setLockLeftMeasure (bool lock) {

    if (m_editPoint->IsEditLocked()) {
      m_lockLeftMeasure->setChecked(m_leftMeasure->IsEditLocked());
      QMessageBox::warning(this, "Point Locked","Point is Edit Locked.  You must un-lock point"
        " before changing a measure.");
      m_lockLeftMeasure->setChecked(m_leftMeasure->IsEditLocked());
      return;
    }

    if (m_leftMeasure != NULL) m_leftMeasure->SetEditLock(lock);

    //  If the right chip is the same as the left chip , update the right editLock
    //  box.
    if (m_rightMeasure != NULL) {
      if (m_rightMeasure->GetCubeSerialNumber() == m_leftMeasure->GetCubeSerialNumber()) {
        m_rightMeasure->SetEditLock(lock);
        m_lockRightMeasure->setChecked(lock);
      }
    }
    emit measureChanged();
  }


  /**
   * Set the "Ignore" keyword of the measure shown in the left
   * viewport to the value of the input parameter.
   *
   * @param ignore Boolean value that determines the Ignore value for the left
   *               measure.
   * @internal
   * @history 2010-01-27 Jeannie Walldren - Fixed bug that resulted in segfault.
   *                          Moved the check whether m_rightMeasure is null
   *                          before the check whether m_rightMeasure equals
   *                          m_leftMeasure.
   * @history 2010-12-15 Tracie Sucharski - Remove netChanged, the point is
   *                        not changed in the net unless "Save Point" is
   *                        selected.
   * @history 2011-06-27 Tracie Sucharski - emit signal indicating a measure
   *                        parameter has changed.
   */
  void ControlPointEditWidget::setIgnoreLeftMeasure (bool ignore) {
    if (m_leftMeasure != NULL) m_leftMeasure->SetIgnored(ignore);

    //  If the right chip is the same as the left chip , update the right
    //  ignore box.
    if (m_rightMeasure != NULL) {
      if (m_rightMeasure->GetCubeSerialNumber() == m_leftMeasure->GetCubeSerialNumber()) {
        m_rightMeasure->SetIgnored(ignore);
        m_ignoreRightMeasure->setChecked(ignore);
      }
    }
    emit measureChanged();
  }


  /**
   * Set the "EditLock" keyword of the measure shown in the right viewport to
   * the value of the input parameter.
   *
   * @param ignore Boolean value that determines the EditLock value for the
   *               right measure.
   *
   * @author 2011-03-07 Tracie Sucharski
   *
   * @internal
   * @history 2011-06-27 Tracie Sucharski - emit signal indicating a measure
   *                        parameter has changed.
   * @history 2012-04-16 Tracie Sucharski - When attempting to un-lock a measure
   *                        print error if point is locked.
   */
  void ControlPointEditWidget::setLockRightMeasure (bool lock) {

    if (m_editPoint->IsEditLocked()) {
      m_lockRightMeasure->setChecked(m_rightMeasure->IsEditLocked());
      QMessageBox::warning(this, "Point Locked","Point is Edit Locked.  You must un-lock point"
        " before changing a measure.");
      m_lockRightMeasure->setChecked(m_rightMeasure->IsEditLocked());
      return;
    }

    if (m_rightMeasure != NULL) m_rightMeasure->SetEditLock(lock);

    //  If the left chip is the same as the right chip , update the left editLock box.
    if (m_leftMeasure != NULL) {
      if (m_leftMeasure->GetCubeSerialNumber() == m_rightMeasure->GetCubeSerialNumber()) {
        m_leftMeasure->SetEditLock(lock);
        m_lockLeftMeasure->setChecked(lock);
      }
    }
    emit measureChanged();
  }


  /**
   * Set the "Ignore" keyword of the measure shown in the right
   * viewport to the value of the input parameter.
   *
   * @param ignore Boolean value that determines the Ignore value for the right
   *               measure.
   * @internal
   * @history 2010-01-27 Jeannie Walldren - Fixed bug that resulted in segfault.
   *                          Moved the check whether m_leftMeasure is null before
   *                          the check whether m_rightMeasure equals
   *                          m_leftMeasure.
   * @history 2010-12-15 Tracie Sucharski - Remove netChanged, the point is
   *                        not changed in the net unless "Save Point" is
   *                        selected.
   * @history 2011-06-27 Tracie Sucharski - emit signal indicating a measure
   *                        parameter has changed.
   */
  void ControlPointEditWidget::setIgnoreRightMeasure (bool ignore) {
    if (m_rightMeasure != NULL) m_rightMeasure->SetIgnored(ignore);

    //  If the right chip is the same as the left chip , update the right
    //  ignore blox.
    if (m_leftMeasure != NULL) {
      if (m_rightMeasure->GetCubeSerialNumber() == m_leftMeasure->GetCubeSerialNumber()) {
        m_leftMeasure->SetIgnored(ignore);
        m_ignoreLeftMeasure->setChecked(ignore);
      }
    }
    emit measureChanged();
  }



  /**
   * @brief Load point into ControlPointEditWidget.
   * @internal
   *   @history 2008-11-26  Jeannie Walldren - Added "Number of Measures" to
   *                           ControlPointEditWidget point information.
   *   @history 2010-06-03 Jeannie Walldren - Removed "std::" since "using
   *                          namespace std"
   *   @history 2010-10-29 Tracie Sucharski - Changed pointfiles to QStringList
   *   @history 2011-04-20 Tracie Sucharski - Was not setting EditLock check box
   *   @history 2011-07-18 Tracie Sucharski - Fixed bug with loading
   *                          ground measure-use AprioriSurface point, not lat,lon
   *                          of reference measure unless there is no apriori
   *                          surface point.
   *   @history 2012-05-08 Tracie Sucharski - m_leftFile changed from QString to QString.
   *   @history 2012-10-02 Tracie Sucharski - When creating a new point, load the cube the user
   *                          clicked on first on the left side, use m_leftFile.
   */
  void ControlPointEditWidget::loadPoint () {

    //  Write pointId
    QString CPId = m_editPoint->GetId();
    QString ptId("Point ID:  ");
    ptId += (QString) CPId;
    m_ptIdValue->setText(ptId);

    //  Write number of measures
    QString ptsize = "Number of Measures:  " +
                   QString::number(m_editPoint->GetNumMeasures());
    m_numMeasures->setText(ptsize);

    //  Set EditLock box correctly
    m_lockPoint->setChecked(m_editPoint->IsEditLocked());

    //  Set ignore box correctly
    m_ignorePoint->setChecked(m_editPoint->IsIgnored());

    // Clear combo boxes
    m_leftCombo->clear();
    m_rightCombo->clear();
    m_pointFiles.clear();

    //  Need all files for this point
    for (int i=0; i<m_editPoint->GetNumMeasures(); i++) {
      ControlMeasure &m = *(*m_editPoint)[i];
      QString file = m_serialNumberList->fileName(m.GetCubeSerialNumber());
      m_pointFiles<<file;
      QString tempFileName = FileName(file).name();
      m_leftCombo->addItem(tempFileName);
      m_rightCombo->addItem(tempFileName);
      if (m_editPoint->IsReferenceExplicit() &&
          (QString)m.GetCubeSerialNumber() == m_editPoint->GetReferenceSN()) {
        m_leftCombo->setItemData(i,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);
        m_rightCombo->setItemData(i,QFont("DejaVu Sans", 12, QFont::Bold), Qt::FontRole);
      }
    }

    //  TODO:  WHAT HAPPENS IF THERE IS ONLY ONE MEASURE IN THIS CONTROLPOINT??
    // Assuming combo loaded in same order as measures in the control point-is
    // this a safe assumption???
    //
    //  Find the file from the cubeViewport that was originally used to select
    //  the point, this will be displayed on the left ChipViewport, unless the
    //  point was selected on the ground source image.  In this case, simply
    //  load the first measure on the left.
    int leftIndex = 0;
    int rightIndex = 0;
    //  Check for reference
    if (m_editPoint->IsReferenceExplicit()) {
      leftIndex = m_editPoint->IndexOfRefMeasure();
    }
    else {
      if (!m_leftFile.isEmpty()) {
        leftIndex = m_leftCombo->findText(FileName(m_leftFile).name());
        //  Sanity check
        if (leftIndex < 0 ) leftIndex = 0;
        m_leftFile.clear();
      }
    }

    if (leftIndex == 0) {
      rightIndex = 1;
    }
    else {
      rightIndex = 0;
    }

    //  Handle pts with a single measure, for now simply put measure on left/right
    //  Evenutally put on left with black on right??
    if (rightIndex > m_editPoint->GetNumMeasures()-1) rightIndex = 0;
    m_rightCombo->setCurrentIndex(rightIndex);
    m_leftCombo->setCurrentIndex(leftIndex);
    //  Initialize pointEditor with measures
    selectLeftMeasure(leftIndex);
    selectRightMeasure(rightIndex);

    updateSurfacePointInfo();

//     loadMeasureTable();
  }



  /**
   * @brief Load measure information into the measure table
   * @internal
   *   @history 2011-12-05 Tracie Sucharski - Turn off sorting until table
   *                          is loaded.
   *
   */
  void ControlPointEditWidget::loadMeasureTable () {
    if (m_measureWindow == NULL) {
      m_measureWindow = new QMainWindow(m_parent);
      m_measureTable = new QTableWidget();
      m_measureTable->setMinimumWidth(1600);
      m_measureTable->setAlternatingRowColors(true);
      m_measureWindow->setCentralWidget(m_measureTable);
    }
    else {
      m_measureTable->clear();
      m_measureTable->setSortingEnabled(false);
    }
    m_measureTable->setRowCount(m_editPoint->GetNumMeasures());
    m_measureTable->setColumnCount(NUMCOLUMNS);

    QStringList labels;
    for (int i=0; i<NUMCOLUMNS; i++) {
      labels<<measureColumnToString((MeasureColumns)i);
    }
    m_measureTable->setHorizontalHeaderLabels(labels);

    //  Fill in values
    for (int row=0; row<m_editPoint->GetNumMeasures(); row++) {
      int column = 0;
      ControlMeasure &m = *(*m_editPoint)[row];

      QString file = m_serialNumberList->fileName(m.GetCubeSerialNumber());
      QTableWidgetItem *tableItem = new QTableWidgetItem(QString(file));
      m_measureTable->setItem(row,column++,tableItem);

      tableItem = new QTableWidgetItem(QString(m.GetCubeSerialNumber()));
      m_measureTable->setItem(row,column++,tableItem);

      tableItem = new QTableWidgetItem();
      tableItem->setData(0,m.GetSample());
      m_measureTable->setItem(row,column++,tableItem);

      tableItem = new QTableWidgetItem();
      tableItem->setData(0,m.GetLine());
      m_measureTable->setItem(row,column++,tableItem);

      if (m.GetAprioriSample() == Null) {
        tableItem = new QTableWidgetItem("Null");
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,m.GetAprioriSample());
      }
      m_measureTable->setItem(row,column++,tableItem);

      if (m.GetAprioriLine() == Null) {
        tableItem = new QTableWidgetItem("Null");
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,m.GetAprioriLine());
      }
      m_measureTable->setItem(row,column++,tableItem);

      if (m.GetSampleResidual() == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,m.GetSampleResidual());
      }
      m_measureTable->setItem(row,column++,tableItem);

      if (m.GetLineResidual() == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,m.GetLineResidual());
      }
      m_measureTable->setItem(row,column++,tableItem);

      if (m.GetResidualMagnitude() == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,m.GetResidualMagnitude());
      }
      m_measureTable->setItem(row,column++,tableItem);

      double sampleShift = m.GetSampleShift();
      if (sampleShift == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,sampleShift);
      }
      m_measureTable->setItem(row,column++,tableItem);

      double lineShift = m.GetLineShift();
      if (lineShift == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,lineShift);
      }
      m_measureTable->setItem(row,column++,tableItem);

      double pixelShift = m.GetPixelShift();
      if (pixelShift == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,pixelShift);
      }
      m_measureTable->setItem(row,column++,tableItem);

      double goodnessOfFit = m.GetLogData(
                      ControlMeasureLogData::GoodnessOfFit).GetNumericalValue();
      if (goodnessOfFit == Null) {
        tableItem = new QTableWidgetItem(QString("Null"));
      }
      else {
        tableItem = new QTableWidgetItem();
        tableItem->setData(0,goodnessOfFit);
      }
      m_measureTable->setItem(row,column++,tableItem);

      if (m.IsIgnored()) tableItem = new QTableWidgetItem("True");
      if (!m.IsIgnored()) tableItem = new QTableWidgetItem("False");
      m_measureTable->setItem(row,column++,tableItem);

      if (IsMeasureLocked(m.GetCubeSerialNumber()))
        tableItem = new QTableWidgetItem("True");
      if (!IsMeasureLocked(m.GetCubeSerialNumber()))
        tableItem = new QTableWidgetItem("False");
      m_measureTable->setItem(row,column++,tableItem);

      tableItem = new QTableWidgetItem(
                  ControlMeasure::MeasureTypeToString(m.GetType()));
      m_measureTable->setItem(row,column,tableItem);

      //  If reference measure set font on this row to bold
      if (m_editPoint->IsReferenceExplicit() &&
          (QString)m.GetCubeSerialNumber() == m_editPoint->GetReferenceSN()) {
        QFont font;
        font.setBold(true);

        for (int col=0; col<m_measureTable->columnCount(); col++)
          m_measureTable->item(row, col)->setFont(font);
      }

    }

    m_measureTable->resizeColumnsToContents();
    m_measureTable->resizeRowsToContents();
    m_measureTable->setSortingEnabled(true);
    m_measureWindow->show();
  }



  QString ControlPointEditWidget::measureColumnToString(ControlPointEditWidget::MeasureColumns column) {
    switch (column) {
      case FILENAME:
        return "FileName";
      case CUBESN:
        return "Serial #";
      case SAMPLE:
        return "Sample";
      case LINE:
        return "Line";
      case SAMPLERESIDUAL:
        return "Sample Residual";
      case LINERESIDUAL:
        return "Line Residual";
      case RESIDUALMAGNITUDE:
        return "Residual Magnitude";
      case SAMPLESHIFT:
        return "Sample Shift";
      case LINESHIFT:
        return "Line Shift";
      case PIXELSHIFT:
        return "Pixel Shift";
      case GOODNESSOFFIT:
        return "Goodness of Fit";
      case IGNORED:
        return "Ignored";
      case EDITLOCK:
        return "Edit Lock";
      case TYPE:
        return "Measure Type";
      case APRIORISAMPLE:
        return "Apriori Sample";
      case APRIORILINE:
        return "Apriori Line";
    }
    throw IException(IException::Programmer,
        "Invalid measure column passed to measureColumnToString", _FILEINFO_);
  }



  /**
   * Select left measure
   *
   * @param index Index of file from the point files vector
   *
   * @internal
   * @history 2010-06-03 Jeannie Walldren - Removed "std::" since "using
   *                          namespace std"
   * @history 2011-07-06 Tracie Sucharski - If point is Locked, and measure is
   *                          reference, lock the measure.
   * @history 2012-10-02 Tracie Sucharski - If measure's cube is not viewed, print error and 
   *                          make sure old measure is retained. 
   */
  void ControlPointEditWidget::selectLeftMeasure(int index) {
    QString file = m_pointFiles[index];

    QString serial;
    try {
      serial = m_serialNumberList->serialNumber(file);
    }
    catch (IException &e) {
      QString message = "Make sure the correct cube is opened.\n\n";
      message += e.toString();
      QMessageBox::critical(this, "Error", message);

      //  Set index of combo back to what it was before user selected new.  Find the index
      //  of current left measure.
      QString file = m_serialNumberList->fileName(m_leftMeasure->GetCubeSerialNumber());
      int i = m_leftCombo->findText(FileName(file).name());
      if (i < 0) i = 0;
      m_leftCombo->setCurrentIndex(i);
      return;
    }

    // Make sure to clear out leftMeasure before making a copy of the selected
    // measure.
    if (m_leftMeasure != NULL) {
      delete m_leftMeasure;
      m_leftMeasure = NULL;
    }
    m_leftMeasure = new ControlMeasure();
    //  Find measure for each file
    *m_leftMeasure = *((*m_editPoint)[serial]);

    //  If m_leftCube is not null, delete before creating new one
    m_leftCube.reset(new Cube(file, "r"));

    //  Update left measure of pointEditor
    m_measureEditor->setLeftMeasure (m_leftMeasure, m_leftCube.data(),
                                     m_editPoint->GetId());
    updateLeftMeasureInfo ();

  }


  /**
   * Select right measure
   *
   * @param index  Index of file from the point files vector
   *
   * @internal
   * @history 2010-06-03 Jeannie Walldren - Removed "std::" since "using
   *                          namespace std"
   * @history 2012-10-02 Tracie Sucharski - If measure's cube is not viewed, print error and 
   *                          make sure old measure is retained. 
   */
  void ControlPointEditWidget::selectRightMeasure(int index) {

    QString file = m_pointFiles[index];

    QString serial;
    try {
      serial = m_serialNumberList->serialNumber(file);
    }
    catch (IException &e) {
      QString message = "Make sure the correct cube is opened.\n\n";
      message += e.toString();
      QMessageBox::critical(this, "Error", message);

      //  Set index of combo back to what it was before user selected new.  Find the index
      //  of current left measure.
      QString file = m_serialNumberList->fileName(m_rightMeasure->GetCubeSerialNumber());
      int i = m_rightCombo->findText(FileName(file).name());
      if (i < 0) i = 0;
      m_rightCombo->setCurrentIndex(i);
      return;
    }

    // Make sure to clear out rightMeasure before making a copy of the selected
    // measure.
    if (m_rightMeasure != NULL) {
      delete m_rightMeasure;
      m_rightMeasure = NULL;
    }
    m_rightMeasure = new ControlMeasure();
    //  Find measure for each file
    *m_rightMeasure = *((*m_editPoint)[serial]);

    //  If m_rightCube is not null, delete before creating new one
    m_rightCube.reset(new Cube(file, "r"));

    //  Update left measure of pointEditor
    m_measureEditor->setRightMeasure (m_rightMeasure,m_rightCube.data(),
                                      m_editPoint->GetId());
    updateRightMeasureInfo ();

  }




  /**
   * @internal
   * @history 2008-11-24  Jeannie Walldren - Added "Goodness of Fit" to left
   *                         measure info.
   * @history 2010-07-22  Tracie Sucharski - Updated new measure types
   *                           associated with implementation of binary
   *                           control networks.
   * @history 2010-12-27  Tracie Sucharski - Write textual Null instead of
   *                           the numeric Null for sample & line residuals.
   * @history 2011-04-20  Tracie Sucharski - Set EditLock check box correctly
   * @history 2011-05-20  Tracie Sucharski - Added Reference output
   * @history 2011-07-19  Tracie Sucharski - Did some re-arranging and added
   *                           sample/line shifts.
   *
   */
  void ControlPointEditWidget::updateLeftMeasureInfo () {

    //  Set editLock measure box correctly
    m_lockLeftMeasure->setChecked(IsMeasureLocked(
                                       m_leftMeasure->GetCubeSerialNumber()));
    //  Set ignore measure box correctly
    m_ignoreLeftMeasure->setChecked(m_leftMeasure->IsIgnored());

    QString s = "Reference: ";
    if (m_editPoint->IsReferenceExplicit() &&
        (QString(m_leftMeasure->GetCubeSerialNumber()) == m_editPoint->GetReferenceSN())) {
      s += "True";
    }
    else {
      s += "False";
    }
    m_leftReference->setText(s);

    s = "Measure Type: ";
    if (m_leftMeasure->GetType() == ControlMeasure::Candidate) s += "Candidate";
    if (m_leftMeasure->GetType() == ControlMeasure::Manual) s += "Manual";
    if (m_leftMeasure->GetType() == ControlMeasure::RegisteredPixel) s += "RegisteredPixel";
    if (m_leftMeasure->GetType() == ControlMeasure::RegisteredSubPixel) s += "RegisteredSubPixel";
    m_leftMeasureType->setText(s);

    if (m_leftMeasure->GetSampleResidual() == Null) {
      s = "Sample Residual: Null";
    }
    else {
      s = "Sample Residual: " + QString::number( m_leftMeasure->GetSampleResidual() );
    }
    m_leftSampError->setText(s);

    if (m_leftMeasure->GetLineResidual() == Null) {
      s = "Line Residual: Null";
    }
    else {
      s = "Line Residual: " + QString::number( m_leftMeasure->GetLineResidual() );
    }
    m_leftLineError->setText(s);

    if (m_leftMeasure->GetSampleShift() == Null) {
      s = "Sample Shift: Null";
    }
    else {
      s = "Sample Shift: " + QString::number( m_leftMeasure->GetSampleShift() );
    }
    m_leftSampShift->setText(s);

    if (m_leftMeasure->GetLineShift() == Null) {
      s = "Line Shift: Null";
    }
    else {
      s = "Line Shift: " + QString::number( m_leftMeasure->GetLineShift() );
    }
    m_leftLineShift->setText(s);

    double goodnessOfFit = m_leftMeasure->GetLogData(
                    ControlMeasureLogData::GoodnessOfFit).GetNumericalValue();
    if (goodnessOfFit == Null) {
      s = "Goodness of Fit: Null";
    }
    else {
      s = "Goodness of Fit: " + QString::number(goodnessOfFit);
    }
    m_leftGoodness->setText(s);

  }



  /**
   * @internal
   * @history 2008-11-24 Jeannie Walldren - Added "Goodness of Fit" to right
   *                         measure info.
   * @history 2010-06-03 Jeannie Walldren - Removed "std::" since "using
   *                         namespace std"
   * @history 2010-07-22 Tracie Sucharski - Updated new measure types
   *                         associated with implementation of binary
   *                         control networks.
   * @history 2010-12-27 Tracie Sucharski - Write textual Null instead of
   *                         the numeric Null for sample & line residuals.
   * @history 2011-04-20 Tracie Sucharski - Set EditLock check box correctly
   * @history 2011-05-20 Tracie Sucharski - Added Reference output
   * @history 2011-07-19 Tracie Sucharski - Did some re-arranging and added
   *                         sample/line shifts.
   *
   */

  void ControlPointEditWidget::updateRightMeasureInfo () {

  //  Set editLock measure box correctly
    m_lockRightMeasure->setChecked(IsMeasureLocked(
                                        m_rightMeasure->GetCubeSerialNumber()));
      //  Set ignore measure box correctly
    m_ignoreRightMeasure->setChecked(m_rightMeasure->IsIgnored());

    QString s = "Reference: ";
    if (m_editPoint->IsReferenceExplicit() &&
        (QString(m_rightMeasure->GetCubeSerialNumber()) == m_editPoint->GetReferenceSN())) {
      s += "True";
    }
    else {
      s += "False";
    }

    m_rightReference->setText(s);

    s = "Measure Type: ";
    if (m_rightMeasure->GetType() == ControlMeasure::Candidate) s+= "Candidate";
    if (m_rightMeasure->GetType() == ControlMeasure::Manual) s+= "Manual";
    if (m_rightMeasure->GetType() == ControlMeasure::RegisteredPixel) s+= "RegisteredPixel";
    if (m_rightMeasure->GetType() == ControlMeasure::RegisteredSubPixel) s+= "RegisteredSubPixel";
    m_rightMeasureType->setText(s);

    if (m_rightMeasure->GetSampleResidual() == Null) {
      s = "Sample Residual: Null";
    }
    else {
      s = "Sample Residual: " + QString::number(m_rightMeasure->GetSampleResidual());
    }
    m_rightSampError->setText(s);

    if (m_rightMeasure->GetLineResidual() == Null) {
      s = "Line Residual: Null";
    }
    else {
      s = "Line Residual: " + QString::number(m_rightMeasure->GetLineResidual());
    }
    m_rightLineError->setText(s);

    if (m_rightMeasure->GetSampleShift() == Null) {
      s = "Sample Shift: Null";
    }
    else {
      s = "Sample Shift: " + QString::number(m_rightMeasure->GetSampleShift());
    }
    m_rightSampShift->setText(s);

    if (m_rightMeasure->GetLineShift() == Null) {
      s = "Line Shift: Null";
    }
    else {
      s = "Line Shift: " + QString::number(m_rightMeasure->GetLineShift());
    }
    m_rightLineShift->setText(s);

    double goodnessOfFit = m_rightMeasure->GetLogData(
                    ControlMeasureLogData::GoodnessOfFit).GetNumericalValue();
    if (goodnessOfFit == Null) {
      s = "Goodness of Fit: Null";
    }
    else {
      s = "Goodness of Fit: " + QString::number(goodnessOfFit);
    }
    m_rightGoodness->setText(s);

  }


    /**
   * Update the Surface Point Information in the QnetTool window
   *
   * @author 2011-03-01 Tracie Sucharski
   *
   * @internal
   * @history 2011-05-12 Tracie Sucharski - Type printing Apriori Values
   * @history 2011-05-24 Tracie Sucharski - Set target radii on apriori
   *                        surface point, so that sigmas can be converted to
   *                        meters.
   */
  void ControlPointEditWidget::updateSurfacePointInfo () {

    QString s;

    SurfacePoint aprioriPoint = m_editPoint->GetAprioriSurfacePoint();
    if (aprioriPoint.GetLatitude().degrees() == Null) {
      s = "AprioriLatitude:  Null";
    }
    else {
      s = "Apriori Latitude:  " +
          QString::number(aprioriPoint.GetLatitude().degrees());
    }
    m_pointAprioriLatitude->setText(s);
    if (aprioriPoint.GetLongitude().degrees() == Null) {
      s = "Apriori Longitude:  Null";
    }
    else {
      s = "Apriori Longitude:  " +
          QString::number(aprioriPoint.GetLongitude().degrees());
    }
    m_pointAprioriLongitude->setText(s);
    if (aprioriPoint.GetLocalRadius().meters() == Null) {
      s = "Apriori Radius:  Null";
    }
    else {
      s = "Apriori Radius:  " +
          QString::number(aprioriPoint.GetLocalRadius().meters(),'f',2) +
          " <meters>";
    }
    m_pointAprioriRadius->setText(s);

    if (aprioriPoint.Valid()) {
      vector<Distance> targRadii = m_controlNet->GetTargetRadii();
      aprioriPoint.SetRadii(targRadii[0],targRadii[1],targRadii[2]);

      if (aprioriPoint.GetLatSigmaDistance().meters() == Null) {
        s = "Apriori Latitude Sigma:  Null";
      }
      else {
        s = "Apriori Latitude Sigma:  " +
            QString::number(aprioriPoint.GetLatSigmaDistance().meters()) +
            " <meters>";
      }
      m_pointAprioriLatitudeSigma->setText(s);
      if (aprioriPoint.GetLonSigmaDistance().meters() == Null) {
        s = "Apriori Longitude Sigma:  Null";
      }
      else {
        s = "Apriori Longitude Sigma:  " +
            QString::number(aprioriPoint.GetLonSigmaDistance().meters()) +
            " <meters>";
      }
      m_pointAprioriLongitudeSigma->setText(s);
      if (aprioriPoint.GetLocalRadiusSigma().meters() == Null) {
        s = "Apriori Radius Sigma:  Null";
      }
      else {
        s = "Apriori Radius Sigma:  " +
            QString::number(aprioriPoint.GetLocalRadiusSigma().meters()) +
            " <meters>";
      }
      m_pointAprioriRadiusSigma->setText(s);
    }
    else {
      s = "Apriori Latitude Sigma:  Null";
      m_pointAprioriLatitudeSigma->setText(s);
      s = "Apriori Longitude Sigma:  Null";
      m_pointAprioriLongitudeSigma->setText(s);
      s = "Apriori Radius Sigma:  Null";
      m_pointAprioriRadiusSigma->setText(s);
    }


    SurfacePoint point = m_editPoint->GetAdjustedSurfacePoint();
    if (point.GetLatitude().degrees() == Null) {
      s = "Adjusted Latitude:  Null";
    }
    else {
      s = "Adjusted Latitude:  " + QString::number(point.GetLatitude().degrees());
    }
    m_pointLatitude->setText(s);
    if (point.GetLongitude().degrees() == Null) {
      s = "Adjusted Longitude:  Null";
    }
    else {
      s = "Adjusted Longitude:  " + QString::number(point.GetLongitude().degrees());
    }
    m_pointLongitude->setText(s);
    if (point.GetLocalRadius().meters() == Null) {
      s = "Adjusted Radius:  Null";
    }
    else {
      s = "Adjusted Radius:  " +
          QString::number(point.GetLocalRadius().meters(),'f',2) + " <meters>";
    }
    m_pointRadius->setText(s);
  }



  /**
   * Event filter for ControlPointEditWidget.  Determines whether to update left or right
   * measure info.
   *
   * @param o Pointer to QObject
   * @param e Pointer to QEvent
   *
   * @return @b bool Indicates whether the event type is "Leave".
   *
   */
  bool ControlPointEditWidget::eventFilter(QObject *o, QEvent *e) {
    if(e->type() != QEvent::Leave) return false;
    if(o == m_leftCombo->view()) {
      updateLeftMeasureInfo();
      m_leftCombo->hidePopup();
    }
    if (o == m_rightCombo->view()) {
      updateRightMeasureInfo ();
      m_rightCombo->hidePopup();
    }
    return true;
  }



  bool ControlPointEditWidget::okToContinue() {

    if (m_templateModified) {
      int r = QMessageBox::warning(this, tr("OK to continue?"),
          tr("The currently opened registration template has been modified.\n"
          "Save changes?"),
          QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
          QMessageBox::Yes);

      if (r == QMessageBox::Yes)
        saveTemplateFileAs();
      else if (r == QMessageBox::Cancel)
        return false;
    }

    return true;
  }



  /**
   * prompt user for a registration template file to open.  Once the file is
   * selected, loadTemplateFile is called to update the current template file
   * being used.
   */
  void ControlPointEditWidget::openTemplateFile() {

    if (!okToContinue())
      return;

    QString filename = QFileDialog::getOpenFileName(this,
        "Select a registration template", ".",
        "Registration template files (*.def *.pvl);;All files (*)");

    if (filename.isEmpty())
      return;

    if (m_measureEditor->setTemplateFile(filename)) {
      loadTemplateFile(filename);
    }
  }



  /**
   * Updates the current template file being used.
   *
   * @param fn The file path of the new template file
   */
  void ControlPointEditWidget::loadTemplateFile(QString fn) {

    QFile file(FileName((QString) fn).expanded());
    if (!file.open(QIODevice::ReadOnly)) {
      QString msg = "Failed to open template file \"" + fn + "\"";
      QMessageBox::warning(this, "IO Error", msg);
      return;
    }

    QTextStream stream(&file);
    m_templateEditor->setText(stream.readAll());
    file.close();

    QScrollBar * sb = m_templateEditor->verticalScrollBar();
    sb->setValue(sb->minimum());

    m_templateModified = false;
    m_saveTemplateFile->setEnabled(false);
    m_templateFileNameLabel->setText("Template File: " + fn);
  }



  //! called when the template file is modified by the template editor
  void ControlPointEditWidget::setTemplateModified() {
    m_templateModified = true;
    m_saveTemplateFile->setEnabled(true);
  }



  //! save the file opened in the template editor
  void ControlPointEditWidget::saveTemplateFile() {

    if (!m_templateModified)
      return;

    QString filename = 
        m_measureEditor->templateFileName();

    writeTemplateFile(filename);
  }



  //! save the contents of template editor to a file chosen by the user
  void ControlPointEditWidget::saveTemplateFileAs() {

    QString filename = QFileDialog::getSaveFileName(this,
        "Save registration template", ".",
        "Registration template files (*.def *.pvl);;All files (*)");

    if (filename.isEmpty())
      return;

    writeTemplateFile(filename);
  }



  /**
   * write the contents of the template editor to the file provided.
   *
   * @param fn The filename to write to
   */
  void ControlPointEditWidget::writeTemplateFile(QString fn) {

    QString contents = m_templateEditor->toPlainText();

    // catch errors in Pvl format when populating pvl object
    stringstream ss;
    ss << contents;
    try {
      Pvl pvl;
      ss >> pvl;
    }
    catch(IException &e) {
      QString message = e.toString();
      QMessageBox::warning(this, "Error", message);
      return;
    }

    QString expandedFileName(
        FileName((QString) fn).expanded());

    QFile file(expandedFileName);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      QString msg = "Failed to save template file to \"" + fn + "\"\nDo you "
          "have permission?";
      QMessageBox::warning(this, "IO Error", msg);
      return;
    }

    // now save contents
    QTextStream stream(&file);
    stream << contents;
    file.close();

    if (m_measureEditor->setTemplateFile(fn)) {
      m_templateModified = false;
      m_saveTemplateFile->setEnabled(false);
      m_templateFileNameLabel->setText("Template File: " + fn);
    }
  }



  /**
   * Allows the user to view the template file that is currently
   * set.
   *
   * @author 2008-12-10 Jeannie Walldren
   * @internal
   *   @history 2008-12-10 Jeannie Walldren - Original Version
   *   @history 2008-12-10 Jeannie Walldren - Added "" namespace to
   *                          PvlEditDialog reference and changed
   *                          registrationDialog from pointer to object
   *   @history 2008-12-15 Jeannie Walldren - Added QMessageBox warning in case
   *                          Template File cannot be read.
   */
  void ControlPointEditWidget::viewTemplateFile() {
    try{
      // Get the template file from the ControlPointEditWidget object
      Pvl templatePvl(m_measureEditor->templateFileName());
      // Create registration dialog window using PvlEditDialog class
      // to view and/or edit the template
      PvlEditDialog registrationDialog(templatePvl);
      registrationDialog.setWindowTitle("View or Edit Template File: "
                                         + templatePvl.fileName());
      registrationDialog.resize(550,360);
      registrationDialog.exec();
    }
    catch (IException &e) {
      QString message = e.toString();
      QMessageBox::information(this, "Error", message);
    }
  }



  /**
   * Slot which calls ControlPointEditWidget slot to save chips
   *
   * @author 2009-03-17 Tracie Sucharski
   */

  void ControlPointEditWidget::saveChips() {
    m_measureEditor->saveChips();
  }



  void ControlPointEditWidget::showHideTemplateEditor() {

    if (!m_templateEditorWidget)
      return;

    m_templateEditorWidget->setVisible(!m_templateEditorWidget->isVisible());
  }



  /**
   * Update the current editPoint information in the Point Editor labels
   *
   * @author 2011-05-05 Tracie Sucharski
   *
   * @TODO  Instead of a single method, should slots be separate for each
   *        updated point parameter, ie. ignore, editLock, apriori, etc.
   *        This is not robust, if other point attributes are changed outside
   *        of ControlPointEditWidget, this method will need to be updated.
   *       *** THIS METHOD SHOULD GO AWAY WHEN CONTROLpOINTEDITOR IS INCLUDED
   *           IN MATCH ***
   *       TODO:  THIS IS ONLY CONNECTED IN qnet.cpp FROM THE NAV TOOL.  REFACTOR WILL
   *                  NEED TO CONNECT CORRECT SIGNALS FROM OTHER WIDGETS TO THIS SLOT.
   */
  void ControlPointEditWidget::updatePointInfo(ControlPoint &updatedPoint) {
    if (m_editPoint == NULL) return;
    if (updatedPoint.GetId() != m_editPoint->GetId()) return;
    //  The edit point has been changed by SetApriori, so m_editPoint needs
    //  to possibly update some values.  Need to retain measures from m_editPoint
    //  because they might have been updated, but not yet saved to the network
    //   ("Save Point").
    m_editPoint->SetEditLock(updatedPoint.IsEditLocked());
    m_editPoint->SetIgnored(updatedPoint.IsIgnored());

    //  Set EditLock box correctly
    m_lockPoint->setChecked(m_editPoint->IsEditLocked());

    //  Set ignore box correctly
    m_ignorePoint->setChecked(m_editPoint->IsIgnored());

  }



  /**
   * Refresh all necessary widgets in ControlPointEditWidget including the PointEditor and
   * CubeViewports.
   *
   * @author 2008-12-09 Tracie Sucharski
   *
   * @internal
   * @history 2010-12-15 Tracie Sucharski - Before setting m_editPoint to NULL,
   *                        release memory.  TODO: Why is the first if statement
   *                        being done???
   * @history 2011-10-20 Tracie Sucharski - If no control points exist in the
   *                        network, emit proper signal and make sure editor
   *                        and measure table are hidden.
   *
   */
//  TODO  Is this needed?  
// 
//  void ControlPointEditWidget::refresh() {
//
//    //  Check point being edited, make sure it still exists, if not ???
//    //  Update ignored checkbox??
//    if (m_editPoint != NULL) {
//      try {
//        QString id = m_ptIdValue->text().remove("Point ID:  ");
//        m_controlNet->GetPoint(id);
//      }
//      catch (IException &) {
//        delete m_editPoint;
//        m_editPoint = NULL;
//        emit controlPointChanged();
////      this->setVisible(false);
////      m_measureWindow->setVisible(false);
//      }
//    }
//  }



  /**
   * Turn "Save Point" button text to red
   *
   * @author 2011-06-14 Tracie Sucharski
   */
  void ControlPointEditWidget::colorizeSavePointButton() {

    QColor qc = Qt::red;
    QPalette p = m_savePoint->palette();
    p.setColor(QPalette::ButtonText,qc);
    m_savePoint->setPalette(p);

  }



  /**
   * Turn "Save Net" button text to red
   *  
   * @TODO  Need whoever is actually saving network to emit signal when net has been saved, so 
   * that button can be set back to black. 
   *  
   * @author 2014-07-11 Tracie Sucharski
   */
  void ControlPointEditWidget::colorizeSaveNetButton() {

    QColor qc = Qt::red;
    QPalette p = m_savePoint->palette();
    p.setColor(QPalette::ButtonText,qc);
    m_saveNet->setPalette(p);

  }



  /**
   * Check for implicitly locked measure in m_editPoint.  If point is Locked,
   * and this measure is the reference, it is implicity Locked.
   * Because measure is a copy, the ControlPoint::IsEditLocked() which checks
   * for implicit Lock on Reference measures does not work because there is
   * not a parent point.
   *
   * @param[in] serialNumber (QString)   Serial number of measure to be checked
   *
   *
   * @author 2011-07-06 Tracie Sucharski
   */
  bool ControlPointEditWidget::IsMeasureLocked (QString serialNumber) {

    if (m_editPoint == NULL) return false;

    // Reference implicitly editLocked
    if (m_editPoint->IsEditLocked() && m_editPoint->IsReferenceExplicit() &&
        (m_editPoint->GetReferenceSN() == serialNumber)) {
      return true;
    }
    // Return measures explicit editLocked value
    else {
      return m_editPoint->GetMeasure(serialNumber)->IsEditLocked();
    }

  }


  /**
  *  This slot is needed because we cannot directly emit a signal with a ControlNet
  *  argument after the "Save Net" push button is selected.
  * 
  * @internal
  * @history 2014-07-11 Tracie Sucharski
  */
  void ControlPointEditWidget::saveNet() {

    emit saveControlNet();
  }



  /**
   * This method is called from the constructor so that when the
   * Main window is created, it know's it's size and location.
   *
   */
#if 0
  void ControlPointEditWidget::readSettings() {
    FileName config("$HOME/.Isis/qview/ControlPointEditWidget.config");
    QSettings settings(config.expanded(),
                       QSettings::NativeFormat);
    QPoint pos = settings.value("pos", QPoint(300, 100)).toPoint();
    QSize size = settings.value("size", QSize(900, 500)).toSize();
    this->resize(size);
    this->move(pos);
  }


  /**
   * This method is called when the Main window is closed or
   * hidden to write the size and location settings to a config
   * file in the user's home directory.
   *
   */
  void ControlPointEditWidget::writeSettings() const {
    /*We do not want to write the settings unless the window is
      visible at the time of closing the application*/
    if (!this->isVisible()) return;
    FileName config("$HOME/.Isis/qview/ControlPointEditWidget.config");
    QSettings settings(config.expanded(),
                       QSettings::NativeFormat);
    settings.setValue("pos", this->pos());
    settings.setValue("size", this->size());
  }
#endif


  void ControlPointEditWidget::clearEditPoint() {
    m_editPoint = NULL;
  }


  // 2014-07-21 TLS  Cnetsuite  This needs to be changed to return the help information or
  //              widget?? to the calling program, so that it can be added to a menu or toolbar.
#if 0
  void ControlPointEditWidget::showHelp() {

    QDialog *helpDialog = new QDialog(this);
    helpDialog->setWindowTitle("Match Tool Help");

    QVBoxLayout *mainLayout = new QVBoxLayout;
    helpDialog->setLayout(mainLayout);

    QLabel *matchTitle = new QLabel("<h2>Match Tool</h2>");
    mainLayout->addWidget(matchTitle);

    QLabel *matchSubtitle = new QLabel("A tool for interactively measuring and editing sample/line "
                                  "registration points between cubes.  These "
                                  "points contain sample, line postions only, no latitude or "
                                  "longitude values are used or recorded.");
    matchSubtitle->setWordWrap(true);
    mainLayout->addWidget(matchSubtitle);

    QTabWidget *tabArea = new QTabWidget;
    tabArea->setDocumentMode(true);
    mainLayout->addWidget(tabArea);

    //  TAB 1 - Overview
    QScrollArea *overviewTab = new QScrollArea;
    overviewTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    overviewTab->setWidgetResizable(true);
    QWidget *overviewContainer = new QWidget;
    QVBoxLayout *overviewLayout = new QVBoxLayout;
    overviewContainer->setLayout(overviewLayout);

    QLabel *purposeTitle = new QLabel("<h2>Purpose</h2>");
    overviewLayout->addWidget(purposeTitle);

    QLabel *purposeText = new QLabel("<p>This tool is for recording and editing registration "
        "points measured between cubes displayed in the <i>qview</i> main window.</p> <p>The "
        "recorded registration points are sample and line pixel coordinates only.  Therefore, this "
        "tool can be used on any images including ones that do not contain a camera model "
        "(i.e, The existence of the Isis Instrument Group on the image labels is not required). "
        "This also means that the tool differs from the <i>qnet</i> control point network "
        "application in that no latitude or longitude values are ever used or recorded "
        "(regardless if the image has a camera model in Isis).</p>"
        "<p>The output control point network that this tool generates is primarily used 1) as "
        "input for an image-wide sample/line translation to register one image to another by "
        "'moving' pixel locations - refer to the documentation for applications such as "
        "<i>translate</i> and <i>warp</i>, or 2) to export the file and use the recorded "
        "measurements in other spreadsheet or plotting packages to visualize magnitude "
        "and direction of varying translations of the images relative to one another.</p> "
        "<p>An automated version of this match tool is the <i>coreg</i> application.  This tool "
        "can be used to visually evaluate and edit the control point network created by "
        "<i>coreg</i>.</p> "
        "<p>The format of the output point network file is binary. This tool uses the Isis control "
        " network framework to create, co-register and save all control points and pixel "
        "measurements.  The application <i>cnetbin2pvl</i> can be used to convert from binary to "
        "a readable PVL format."
        "<p>The Mouse Button functions are: (same as <i>qnet</i>)<ul><li>Modify Point=Left</li> "
        "<li>Delete Point=Middle</li><li>Create New Point=Right</li></ul></p>"
        "<p>Control Points are drawn on the associated displayed cubes with the following colors:  "
        "Green=Valid registration point; Yellow=Ignored point; Red=Active point being edited");
    purposeText->setWordWrap(true);
    overviewLayout->addWidget(purposeText);

    overviewTab->setWidget(overviewContainer);

    //  TAB 2 - Quick Start
    QScrollArea *quickTab = new QScrollArea;
    quickTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    quickTab->setWidgetResizable(true);
    QWidget *quickContainer = new QWidget;
    QVBoxLayout *quickLayout = new QVBoxLayout;
    quickContainer->setLayout(quickLayout);

    QLabel *quickTitle = new QLabel("<h2>Quick Start</h2>");
    quickLayout->addWidget(quickTitle);

    QLabel *quickSubTitle = new QLabel("<h3>Preparation:</h3>");
    quickLayout->addWidget(quickSubTitle);

    QString toolIconDir = FileName("$base/icons").expanded();

    QLabel *quickPrep = new QLabel("<p><ul>"
        "<li>Open the cubes with overlapping areas for choosing control points</li>"
        "<li>Choose the match tool <img src=\"" + toolIconDir +
        "/stock_draw-connector-with-arrows.png\" width=22 height=22> "
        "from the toolpad on the right side of the <i>qview</i> main window</li>");
    quickPrep->setWordWrap(true);
    quickLayout->addWidget(quickPrep);

    QLabel *morePrep = new QLabel("<p>Once the Match tool is activated the tool bar at the top "
        "of the main window contains file action buttons and a help button:");
    morePrep->setWordWrap(true);
    quickLayout->addWidget(morePrep);

    QLabel *fileButtons = new QLabel("<p><ul>"
        "<li><img src=\"" + toolIconDir + "/fileopen.png\" width=22 height=22>  Open an existing "
        "control network  <b>Note:</b> If you do not open an existing network, a new one will "
        "be created</li>"
        "<li><img src=\"" + toolIconDir + "/mActionFileSaveAs.png\" width=22 height=22> Save "
        "control network as ...</li>"
        "<li><img src=\"" + toolIconDir + "/mActionFileSave.png\" width=22 height=22> Save "
        "control network to current file</li>"
        "<li><img src=\"" + toolIconDir + "/help-contents.png\" width=22 height=22> Show Help "
        "</li></ul></p>");
    fileButtons->setWordWrap(true);
    quickLayout->addWidget(fileButtons);

    QLabel *quickFunctionTitle = new QLabel("<h3>Cube Viewport Functions:</h3>");
    quickLayout->addWidget(quickFunctionTitle);

    QLabel *quickFunction = new QLabel(
        "The match tool window will be shown once "
        "you click in a cube viewport window using one of the following "
        "mouse functions.  <b>Note:</b>  Existing control points are drawn on the cube viewports");
    quickFunction->setWordWrap(true);
    quickLayout->addWidget(quickFunction);

    QLabel *quickDesc = new QLabel("<p><ul>"
      "<li>Left Click - Modify the control point closest to the click  <b>Note:</b>  "
      "All cubes in the control point must be displayed before loading the point</li>"
      "<li>Middle Click - Delete the control point closest to the click</li>"
      "<li>Right Click - Create a new control point at the click location</li></ul></p>");
    quickDesc->setWordWrap(true);
    quickDesc->setOpenExternalLinks(true);
    quickLayout->addWidget(quickDesc);

    quickTab->setWidget(quickContainer);

    //  TAB 3 - Control Point Editing
    QScrollArea *controlPointTab = new QScrollArea;
    controlPointTab->setWidgetResizable(true);
    controlPointTab->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *controlPointContainer = new QWidget;
    QVBoxLayout *controlPointLayout = new QVBoxLayout;
    controlPointContainer->setLayout(controlPointLayout);

    QLabel *controlPointTitle = new QLabel("<h2>Control Point Editing</h2>");
    controlPointLayout->addWidget(controlPointTitle);

    QLabel *mouseLabel = new QLabel("<p><h3>When the \"Match\" tool "
      "is activated, the mouse buttons have the following function in the "
      "cube viewports of the main qview window:</h3>");
    mouseLabel->setWordWrap(true);
    mouseLabel->setScaledContents(true);
    controlPointLayout->addWidget(mouseLabel);

    QLabel *controlPointDesc = new QLabel("<ul>"
      "<li>Left click   - Edit the closest control point   <b>Note:</b>  "
      "All cubes in the control point must be displayed before loading the point</li>"
      "<li>Middle click - Delete the closest control point</li>"
      "<li>Right click  - Create new control point at cursor location.  This will bring up a new "
      "point dialog which allows you to enter a point id and will list all cube viewports, "
      "highlighting cubes where the point has been chosen by clicking on the cube's viewport.  "
      "When the desired cubes have been chosen, select the \"Done\" button which will load the "
      "control point into the control point editor window which will allow the control measure "
      "positions to be refined.</li>");
    controlPointDesc->setWordWrap(true);
    controlPointLayout->addWidget(controlPointDesc);

    QLabel *controlPointEditing = new QLabel(
      "<h4>Changing Control Measure Locations</h4>"
        "<p>Both the left and right control measure positions can be adjusted by:"
      "<ul>"
      "<li>Move the cursor location under the crosshair by clicking the left mouse "
            "button</li>"
      "<li>Move 1 pixel at a time by using arrow keys on the keyboard</li>"
      "<li>Move 1 pixel at a time by using arrow buttons above the right and left views</li>"
      "</ul></p>"
      "<h4>Other Point Editor Functions</h4>"
        "<p>Along the right border of the window:</p>"
        "<ul>"
          "<li><strong>Link Zoom</strong>   This will link the two small viewports together when "
              "zooming (ie.  If this is checked, if the left view is zoomed, the right view will "
              "match the left view's zoom factor.  "
              "<b>Note:</b>   Zooming is controlled from the left view.</li>"
         "<li><strong>No Rotate:</strong>  Turn off the rotation and bring right view back to "
          "its original orientation</li>"
          "<li><strong>Rotate:</strong>   Rotate the right view using either the dial "
              "or entering degrees </li>"
          "<li><strong>Show control points:</strong>  Draw crosshairs at all control "
               "point locations visible within the view</li>"
          "<li><strong>Show crosshair:</strong>  Show a red crosshair across the entire "
              "view</li>"
          "<li><strong>Circle:</strong>  Draw circle which may help center measure "
              "on a crater</li></ul"
        "<p>Below the left view:</p>"
          "<ul><li><strong>Blink controls:</strong>  Blink the left and right view in the "
          "left view window using the \"Blink Start\" button <img src=\"" + toolIconDir +
          "/blinkStart.png\" width=22 height=22> and \"Blink Stop\" button <img src=\"" +
          toolIconDir + "/blinkStop.png\" width=22 height=22>.  The arrow keys above the left "
          "and right views and the keyboard arrow keys may be used to move the both views while "
          "blinking.</li>"
        "<li><strong>Register:</strong>  Sub-pixel register the right view to "
              "the left view. A default registration template is used for setting parameters "
              "passed to the sub-pixel registration tool.  The user may load in a predefined "
              "template or edit the current loaded template to influence successful "
              "co-registration results.  For more information regarding the pattern matching "
              "functionlity or how to create a parameter template, refer to the Isis PatternMatch "
              "document and the <i>autoregtemplate</i> application.</li>"
        "<li><strong>Save Measures:</strong>  Save the two control measures using the sample, "
              "line positions under the crosshairs.</li>"
        "<li><strong>Save Point:</strong>  Save the control point to the control network.</li>"
        "</ul>");
    controlPointEditing->setWordWrap(true);
    controlPointLayout->addWidget(controlPointEditing);

    controlPointTab->setWidget(controlPointContainer);

    tabArea->addTab(overviewTab, "&Overview");
    tabArea->addTab(quickTab, "&Quick Start");
    tabArea->addTab(controlPointTab, "&Control Point Editing");

    QHBoxLayout *buttonsLayout = new QHBoxLayout;
    // Flush the buttons to the right
    buttonsLayout->addStretch();

    QPushButton *closeButton = new QPushButton("&Close");
    closeButton->setIcon(QIcon(FileName("$base/icons/guiStop.png").expanded()));
    closeButton->setDefault(true);
    connect(closeButton, SIGNAL(clicked()),
            helpDialog, SLOT(close()));
    buttonsLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonsLayout);

    helpDialog->show();
  }
#endif
}

