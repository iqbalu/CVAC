/******************************************************************************
 * CVAC Software Disclaimer
 * 
 * This software was developed at the Naval Postgraduate School, Monterey, CA,
 * by employees of the Federal Government in the course of their official duties.
 * Pursuant to title 17 Section 105 of the United States Code this software
 * is not subject to copyright protection and is in the public domain. It is 
 * an experimental system.  The Naval Postgraduate School assumes no
 * responsibility whatsoever for its use by other parties, and makes
 * no guarantees, expressed or implied, about its quality, reliability, 
 * or any other characteristic.
 * We would appreciate acknowledgement and a brief notification if the software
 * is used.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above notice,
 *       this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Naval Postgraduate School, nor the name of
 *       the U.S. Government, nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE NAVAL POSTGRADUATE SCHOOL (NPS) AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL NPS OR THE U.S. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/
#include <iostream>
#include <vector>

#include <Ice/Communicator.h>
#include <Ice/Initialize.h>
#include <Ice/ObjectAdapter.h>
#include <util/processRunSet.h>
#include <util/FileUtils.h>
#include <util/ServiceMan.h>
#include <util/processLabels.h>
#include <util/DetectorDataArchive.h>

#include "opencv2/core/core.hpp"
#include "opencv2/core/internal.hpp"
#include "cv.h"
#include "cascadeclassifier.h"
#include "cvhaartraining.h"

#include "CascadeTrainI.h"

using namespace std;
using namespace cvac;


///////////////////////////////////////////////////////////////////////////////
// This is called by IceBox to get the service to communicate with.
extern "C"
{
  //
  // ServiceManager handles all the icebox interactions so we construct
  // it and set a pointer to our detector.
  //
  ICE_DECLSPEC_EXPORT IceBox::Service* create(Ice::CommunicatorPtr communicator)
  {
    ServiceManager *sMan = new ServiceManager();
    CascadeTrainI *cascade = new CascadeTrainI(sMan);
    sMan->setService(cascade, cascade->getName());
    return (::IceBox::Service *) sMan->getIceService();

  }
}

CascadeTrainI::CascadeTrainI(ServiceManager *serv)
  : fInitialized(false)
{
  mServiceMan = serv;	
}

CascadeTrainI::~CascadeTrainI()
{
  mAdapter->deactivate();  
}

void CascadeTrainI::initialize(::Ice::Int verbosity,const ::Ice::Current& current)
{
  // Obtain CVAC verbosity
  Ice::PropertiesPtr props = current.adapter->getCommunicator()->getProperties();
  string verbStr = props->getProperty("CVAC.ServicesVerbosity");
  if (!verbStr.empty())
  {
    vLogger.setLocalVerbosityLevel( verbStr );
  }
  // Create TrainerPropertiesI class to allow the user to modify training 
  // parameters
  Ice::ObjectAdapterPtr mAdapter = current.adapter->getCommunicator()->createObjectAdapter("");
  Ice::Identity ident;
  ident.name = IceUtil::generateUUID();
  ident.category = "";
  mTrainProps = new TrainerPropertiesI();
  TrainerPropertiesPtr trainPropPtr = mTrainProps;
  mAdapter->add(trainPropPtr, ident);
  mAdapter->activate();
  //current.con->addAdapter(mAdapter);    

  // Fill in the trainProps with default values;
  mTrainProps->numStages = 20;
  //mTrainProps->featureType = CvFeatureParams::HAAR; // HAAR, LBP, HOG;
  mTrainProps->featureType = CvFeatureParams::LBP; // HAAR, LBP, HOG;
  mTrainProps->boost_type = CvBoost::GENTLE;  // CvBoost::DISCRETE, REAL, LOGIT
  mTrainProps->minHitRate = 0.995F;
  mTrainProps->maxFalseAlarm = 0.5F;
  mTrainProps->weight_trim_rate = 0.95F; // From opencv/modules/ml/src/boost.cpp
  mTrainProps->max_depth = 1;
  mTrainProps->weak_count = 100;
  mTrainProps->width = 25;
  mTrainProps->height = 25;
  
  fInitialized = true;
}

bool CascadeTrainI::isInitialized(const ::Ice::Current& current)
{
  return fInitialized;
}

void CascadeTrainI::destroy(const ::Ice::Current& current)
{
  fInitialized = false;
}
std::string CascadeTrainI::getName(const ::Ice::Current& current)
{
  return "OpenCVCascadeTrainer";
}
std::string CascadeTrainI::getDescription(const ::Ice::Current& current)
{
  return "OpenCVCascadeTrainer: OpenCV Cascade trainer";
}

void CascadeTrainI::setVerbosity(::Ice::Int verbosity, const ::Ice::Current& current)
{
}

::TrainerPropertiesPrx CascadeTrainI::getTrainerProperties(const ::Ice::Current &current)
{
  return (IceProxy::cvac::TrainerProperties *)mTrainProps;
}

/**
 * Prepend the CVAC_DataDir to the runset's filepath directories.  
 * @param The runset to modify
 * @param The directory path to prepend.
 **/
void CascadeTrainI::addDataPath(RunSet runset, const std::string &CVAC_DataDir)
{
    unsigned int i;
    for (i = 0; i < runset.purposedLists.size(); i++)
    {
        cvac::PurposedLabelableSeq* lab = static_cast<cvac::PurposedLabelableSeq*>(runset.purposedLists[i].get());
        // expand the file names
        LabelableList artifacts = lab->labeledArtifacts;
        std::vector<LabelablePtr>::iterator it;
        for (it = artifacts.begin(); it < artifacts.end(); it++)
        {
            LabelablePtr lptr = (*it);
            Substrate sub = lptr->sub;
            FilePath  filePath = sub.path;
            std::string fname;
            fname = cvac::getFSPath(filePath, CVAC_DataDir);
            filePath.directory.relativePath = getFileDirectory(fname);
            filePath.filename = getFileName(fname);
            sub.path = filePath;
            lptr->sub = sub;
        }
    }
}


void CascadeTrainI::writeBgFile( const RunSetWrapper& rsw, 
                                const string& bgFilename, int* pNumNeg, string CVAC_DataDir )
{
  // TODO: iterate over NEGATIVE purposes only, count numNeg, write 
  // file names to bgFilename as we did for the old OpenCV Performance training;
  // something like:
  // Constraints con; // has a bunch of defaults
  // con.compatiblePurpose = NEGATIVE;
  // con.substrateType = IMAGES;
  // con.mimeTypes = { "jpg", "png", "bmp" };
  // con.spacesInFilenamesPermitted = false;
  // LabelableIterator it = rsw.iterator( con );
  // for labelable in it ...
  // Save stored data from RunSet to OpenCv negative samples file

  unsigned int i;
  std::vector<RectangleLabels> negRectlabels;
  int imgCnt = 0;
  const cvac::RunSet &runset = rsw.runset;
  for (i = 0; i < runset.purposedLists.size(); i++)
  {
    // Only look for positive purposes
    if (NEGATIVE == runset.purposedLists[i]->pur.ptype)
    {
        // Store training-input data to vectors
        cvac::PurposedLabelableSeq* lab = static_cast<cvac::PurposedLabelableSeq*>(runset.purposedLists[i].get());
        LabelableList artifacts = lab->labeledArtifacts;
        imgCnt += cvac::processLabelArtifactsToRects(&artifacts, 
                                    NULL, &negRectlabels, true);
     }
  }
  ofstream backgroundFile;
  backgroundFile.open(bgFilename.c_str());
  int cnt = 0;
  std::vector<cvac::RectangleLabels>::iterator it;
  for (it = negRectlabels.begin(); it < negRectlabels.end(); it++)
  {
    // fileName, # of objects, x, y, width, height
    cvac::RectangleLabels recLabel = *it;
    backgroundFile << recLabel.filename;
    cnt++;
    // NO EXTRA BLANK LINE after the last sample, or cvhaartraining.cpp can fail on: "CV_Assert(elements_read == 1);"
    if ((cnt) < imgCnt)
        backgroundFile << endl;
  }
  backgroundFile.flush();
  backgroundFile.close();
  // Clean up any memory 
  cvac::cleanupRectangleLabels(&negRectlabels);
  *pNumNeg = cnt;
}

/**
 * Function called by ProcessLabelArtifactsToRects that returns the
 * size of an image.
 */
bool static getImageWidthHeight(std::string filename, int &width, int &height)
{
   IplImage* img = cvLoadImage(filename.c_str());
   bool res;
   if( !img )
   {
       width = 0;
       height = 0;
       res = false;
   } else
   {
       width = img->width;
       height = img->height;
       res = true;
   }
   cvReleaseImage( &img );
   return res;
}

bool CascadeTrainI::createSamples( const RunSetWrapper& rsw, 
                                   const SamplesParams& params,
                    const string& infoFilename,
                    const string& vecFilename, int* pNumPos, string CVAC_DataDir
                    )
{
 
  bool showsamples = false;

  unsigned int i;
  std::vector<RectangleLabels> posRectlabels;
  int imgCnt = 0;
  const cvac::RunSet &runset = rsw.runset;
  for (i = 0; i < runset.purposedLists.size(); i++)
  {
    // Only look for positive purposes
    if (POSITIVE == runset.purposedLists[i]->pur.ptype)
    {
        // Store training-input data to vectors
        cvac::PurposedLabelableSeq* lab = static_cast<cvac::PurposedLabelableSeq*>(runset.purposedLists[i].get());
        // expand the file names
        // TODO call the utils fixupRunSet function.
        LabelableList artifacts = lab->labeledArtifacts;
        imgCnt += cvac::processLabelArtifactsToRects(&artifacts, getImageWidthHeight, &posRectlabels, true);
     }
  }

  ofstream infoFile;
 
  infoFile.open(infoFilename.c_str());

  // Save stored data from RunSet to OpenCv positive samples .dat file
  int cnt = 0;
  std::vector<cvac::RectangleLabels>::iterator it;
  for (it = posRectlabels.begin(); it < posRectlabels.end(); it++)
  {
    cvac::RectangleLabels recLabel = *it;
    bool skipFile = false;
    if (recLabel.rects.size() <= 0)
    { // No rectangle so use the whole image
        int w, h;
        getImageWidthHeight(recLabel.filename, w, h);
        infoFile << recLabel.filename << " 1 0 0 " << w << " " << h;
    }else 
    {
       int rectCnt = 0;  // Only add labels that are as large as the window size we are using!
       std::vector<cvac::BBoxPtr>::iterator rit;
       // Get count of valid size rectangles.
       for (rit = recLabel.rects.begin(); rit < recLabel.rects.end(); rit++)
       {
           cvac::BBoxPtr rect = *rit;
           if (rect->width < params.width || rect->height < params.height)
               continue;  // dont' count this rect.
           rectCnt++;
       }
       if (rectCnt == 0)
       {
           int w, h;
           getImageWidthHeight(recLabel.filename, w, h);
           if (w >= params.width && h >= params.height)
               infoFile << recLabel.filename << " 1 0 0 " << w << " " << h;
           else
               skipFile = true;
       } else
       {
          // fileName, # of objects, x, y, width, height
          infoFile << recLabel.filename << " " <<
                      rectCnt << " ";

          for (rit = recLabel.rects.begin(); rit < recLabel.rects.end(); rit++)
          {
              cvac::BBoxPtr rect = *rit;
              if (rect->width >= params.width && rect->height >= params.height)
                  infoFile << rect->x << " " << rect->y << " " << rect->width <<
                          " " << rect->height  << " ";
          }
       }
    }
    if (skipFile == false)
        cnt++;
    // NO EXTRA BLANK LINE after the last sample, or cvhaartraining.cpp can fail on: "CV_Assert(elements_read == 1);"
    if ((cnt < imgCnt) && skipFile == false)
        infoFile << endl;
  }
  infoFile.flush();
  infoFile.close();
  // Clean up any memory 
  cvac::cleanupRectangleLabels(&posRectlabels);
  // Save stored data from RunSet to OpenCv negative samples file

  *pNumPos = cvCreateTrainingSamplesFromInfo( infoFilename.c_str(), 
                                              vecFilename.c_str(), 
                                              cnt, showsamples,
                                              params.width, params.height
                                             );
  return true;
}


bool CascadeTrainI::createClassifier( const string& tempDir, 
                       const string& vecFname, const string& bgName,
                       int numPos, int numNeg, 
                       const TrainerPropertiesI *trainProps)
{
  CvCascadeClassifier classifier;
  int precalcValBufSize = 256,
      precalcIdxBufSize = 256;
  bool baseFormatSave = false;
  CvCascadeParams cascadeParams;
  cascadeParams.winSize.width = trainProps->width;
  cascadeParams.winSize.height = trainProps->height;
  CvCascadeBoostParams stageParams;
  Ptr<CvFeatureParams> featureParams[] = 
  { Ptr<CvFeatureParams>(new CvHaarFeatureParams),
    Ptr<CvFeatureParams>(new CvLBPFeatureParams),
    Ptr<CvFeatureParams>(new CvHOGFeatureParams)
  };
  cascadeParams.featureType = trainProps->featureType;
  stageParams.boost_type = trainProps->boost_type;

  bool res = classifier.train( tempDir,
                    vecFname,
                    bgName,
                    numPos, numNeg,
                    precalcValBufSize, precalcIdxBufSize,
                    trainProps->numStages,
                    cascadeParams,
                    *featureParams[cascadeParams.featureType],
                    stageParams,
                    baseFormatSave );  
  return res;
}

void CascadeTrainI::process(const Ice::Identity &client,
                            const ::RunSet& runset,
                            const ::Ice::Current& current)
{	
  TrainerCallbackHandlerPrx callback =
    TrainerCallbackHandlerPrx::uncheckedCast(current.con->createProxy(client)->ice_oneway());		
  // Get the remote client name to use to save cascade file 
  std::string connectName = cvac::getClientConnectionName(current);
  Ice::PropertiesPtr props = (current.adapter->getCommunicator()->getProperties());
  const std::string CVAC_DataDir = props->getProperty("CVAC.DataDir");

  if(runset.purposedLists.size() == 0)
  {
    string _resStr = "Error: no data (runset) for processing\n";
    localAndClientMsg(VLogger::WARN, callback, _resStr.c_str());
    return;
  }
  // Since createSamples fails if there is a space in a file name we will create a temporary runset
  // and provide symbolic links to files that name spaces in there names.
  cvac::RunSet tempRunSet = runset;
  // Add the cvac data dir to the directories in the runset
  addDataPath(tempRunSet, CVAC_DataDir);
  // The symbolic links are created in a tempdir so lets remember it so we can delete it at the end
  std::string tempRSDir = fixupRunSet(tempRunSet, CVAC_DataDir);
  // Iterate over runset, inserting each POSITIVE Labelable into
  // the input file to "createsamples".  Add each NEGATIVE into
  // the bgFile.  Put both created files into a tempdir.
  std::string clientName = mServiceMan->getSandbox()->createClientName(mServiceMan->getIceName(),
                                                             connectName);
  std::string tempDir = mServiceMan->getSandbox()->createTrainingDir(clientName);
  RunSetWrapper rsw( tempRunSet );
  // We can't put the bgName and infoName in the tempdir without
  // changing cvSamples since it assumes that this files location is the root
  // directory for the data.
  //string bgName = tempDir + "/cascade_negatives.txt";
  //string infoName = tempDir + "/cascade_positives.txt";
  string bgName = "cascade_negatives.txt";
  string infoName = "cascade_positives.txt";
  int numNeg = 0;
  writeBgFile( rsw, bgName, &numNeg, CVAC_DataDir );


  // set parameters to createsamples
  SamplesParams samplesParams;
  samplesParams.numSamples = 1000;
  samplesParams.width = mTrainProps->width;
  samplesParams.height = mTrainProps->height;

  // run createsamples
  std::string vecFname = tempDir + "/cascade_positives.vec";
  int numPos = 0;
  createSamples( rsw, samplesParams, infoName, vecFname, &numPos, CVAC_DataDir);
  // invoke the actual training vec file needs extra positive samples
  // so we need to figure out how many to save back.
  // Determine the number of samples extra we need.  We need this
  // since the algorithm 
  // this is (Stages-1)*(1-minHitRate)*numPos + S
  // where S = numpos / minHitRate^Stages - numpos
  int S = int((double)numPos / 
            pow(mTrainProps->minHitRate, mTrainProps->numStages)) - numPos;
  int lessSamples = (int)((double)(mTrainProps->numStages -1) * 
               (1.0 - mTrainProps->minHitRate) * double(numPos)) + S;
  localAndClientMsg(VLogger::INFO, NULL, "Starting with positive count less " +
                     lessSamples);

  // Tell ServiceManager that we will listen for stop
  mServiceMan->setStoppable();

  bool created = createClassifier( tempDir, vecFname, bgName,
                    numPos - lessSamples, numNeg, mTrainProps );

  // Tell ServiceManager that we are done listening for stop
  mServiceMan->clearStop();  
  if (created)
  {
      
      DetectorDataArchive dda;
      std::string clientDir = mServiceMan->getSandbox()->createClientDir(clientName);
      std::string archiveFilename = getDateFilename(clientDir,  "cascade")+ ".zip";
      dda.setArchiveFilename(archiveFilename);
      dda.addFile(XMLID, tempDir + "/cascade.xml");
      dda.createArchive(tempDir);
      mServiceMan->getSandbox()->deleteTrainingDir(clientName);
      DetectorData detectorData;
      detectorData.file.filename = getFileName(archiveFilename);
      detectorData.type = ::cvac::FILE;
      std::string relDir;
      int idx = clientDir.find(CVAC_DataDir.c_str(), 0, CVAC_DataDir.length());
      if (idx == 0)
      {
          relDir = clientDir.substr(CVAC_DataDir.length() + 1);
      }else
      {
          relDir = clientDir;
      }
      detectorData.file.directory.relativePath = relDir; 
      callback->createdDetector(detectorData);

      localAndClientMsg(VLogger::INFO, callback, "Cascade training done.\n");
      
  }else
  {
      localAndClientMsg(VLogger::INFO, callback, "Cascade training failed.\n");
  }
  deleteDirectory(tempRSDir);
}

//----------------------------------------------------------------------------
void TrainerPropertiesI::setWindowSize(const cvac::Size &wsize,
                               const Ice::Current&)
{
  width = wsize.width;
  height = wsize.height;
}
bool TrainerPropertiesI::canSetWindowSize(const ::Ice::Current&)
{
  return true;
}

cvac::Size TrainerPropertiesI::getWindowSize(const ::Ice::Current& )
{
  cvac::Size size;
  size.width = width;
  size.height = height;
  return size;
}

void TrainerPropertiesI::setSensitivity(Ice::Double falseAlarmRate, Ice::Double recall,
                               const ::Ice::Current&)
{
  maxFalseAlarm = falseAlarmRate;
  minHitRate = recall;
}

bool TrainerPropertiesI::canSetSensitivity(const ::Ice::Current& )
{
  return true;
}

void TrainerPropertiesI::getSensitivity(Ice::Double &falseAlarmRate, Ice::Double &recall,
                               const ::Ice::Current& )
{
  falseAlarmRate = maxFalseAlarm;
  recall = minHitRate;
}


// TODO: this is the old main function; here only for reference.  remove 
// once no longer needed
int nomain( int argc, char* argv[] )
{
    CvCascadeClassifier classifier;
    String cascadeDirName, vecName, bgName;
    int numPos    = 2000;
    int numNeg    = 1000;
    int numStages = 20;
    int precalcValBufSize = 256,
        precalcIdxBufSize = 256;
    bool baseFormatSave = false;

    CvCascadeParams cascadeParams;
    CvCascadeBoostParams stageParams;
    Ptr<CvFeatureParams> featureParams[] = { Ptr<CvFeatureParams>(new CvHaarFeatureParams),
                                             Ptr<CvFeatureParams>(new CvLBPFeatureParams),
                                             Ptr<CvFeatureParams>(new CvHOGFeatureParams)
                                           };
    int fc = sizeof(featureParams)/sizeof(featureParams[0]);
    if( argc == 1 )
    {
        cout << "Usage: " << argv[0] << endl;
        cout << "  -data <cascade_dir_name>" << endl;
        cout << "  -vec <vec_file_name>" << endl;
        cout << "  -bg <background_file_name>" << endl;
        cout << "  [-numPos <number_of_positive_samples = " << numPos << ">]" << endl;
        cout << "  [-numNeg <number_of_negative_samples = " << numNeg << ">]" << endl;
        cout << "  [-numStages <number_of_stages = " << numStages << ">]" << endl;
        cout << "  [-precalcValBufSize <precalculated_vals_buffer_size_in_Mb = " << precalcValBufSize << ">]" << endl;
        cout << "  [-precalcIdxBufSize <precalculated_idxs_buffer_size_in_Mb = " << precalcIdxBufSize << ">]" << endl;
        cout << "  [-baseFormatSave]" << endl;
        cascadeParams.printDefaults();
        stageParams.printDefaults();
        for( int fi = 0; fi < fc; fi++ )
            featureParams[fi]->printDefaults();
        return 0;
    }

    for( int i = 1; i < argc; i++ )
    {
        bool set = false;
        if( !strcmp( argv[i], "-data" ) )
        {
            cascadeDirName = argv[++i];
        }
        else if( !strcmp( argv[i], "-vec" ) )
        {
            vecName = argv[++i];
        }
        else if( !strcmp( argv[i], "-bg" ) )
        {
            bgName = argv[++i];
        }
        else if( !strcmp( argv[i], "-numPos" ) )
        {
            numPos = atoi( argv[++i] );
        }
        else if( !strcmp( argv[i], "-numNeg" ) )
        {
            numNeg = atoi( argv[++i] );
        }
        else if( !strcmp( argv[i], "-numStages" ) )
        {
            numStages = atoi( argv[++i] );
        }
        else if( !strcmp( argv[i], "-precalcValBufSize" ) )
        {
            precalcValBufSize = atoi( argv[++i] );
        }
        else if( !strcmp( argv[i], "-precalcIdxBufSize" ) )
        {
            precalcIdxBufSize = atoi( argv[++i] );
        }
        else if( !strcmp( argv[i], "-baseFormatSave" ) )
        {
            baseFormatSave = true;
        }
        else if ( cascadeParams.scanAttr( argv[i], argv[i+1] ) ) { i++; }
        else if ( stageParams.scanAttr( argv[i], argv[i+1] ) ) { i++; }
        else if ( !set )
        {
            for( int fi = 0; fi < fc; fi++ )
            {
                set = featureParams[fi]->scanAttr(argv[i], argv[i+1]);
                if ( !set )
                {
                    i++;
                    break;
                }
            }
        }
    }

    classifier.train( cascadeDirName,
                      vecName,
                      bgName,
                      numPos, numNeg,
                      precalcValBufSize, precalcIdxBufSize,
                      numStages,
                      cascadeParams,
                      *featureParams[cascadeParams.featureType],
                      stageParams,
                      baseFormatSave );
    return 0;
}
