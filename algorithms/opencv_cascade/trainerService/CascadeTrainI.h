#ifndef _CascadeTrainI_H__
/*****************************************************************************
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
 **************************************************************************/
#define _CascadeTrainI_H__

#include <Data.h>
#include <Services.h>

#include <Ice/Ice.h>
#include <IceBox/IceBox.h>
#include <IceUtil/UUID.h>
#include <util/processRunSet.h>
#include <util/ServiceMan.h>

class SamplesParams
{
 public:
  int numSamples;
  int width;
  int height;
};

// TODO: change to K's class name
class RunSetWrapper {
public:
  RunSetWrapper(const cvac::RunSet& rs ): runset(rs) { }

public:
  const cvac::RunSet &runset;
};

class TrainerPropertiesI : public cvac::TrainerProperties
{
 public:
   /**
    * Client access fuctions
    */
   virtual void setWindowSize(const cvac::Size &wsize,
                               const Ice::Current& = ::Ice::Current() );
   virtual bool canSetWindowSize(
                               const ::Ice::Current& = ::Ice::Current() );
   virtual cvac::Size getWindowSize(
                               const ::Ice::Current& = ::Ice::Current() );
   virtual void setSensitivity(Ice::Double falseAlarmRate, Ice::Double recall,
                               const ::Ice::Current& = ::Ice::Current() );
   virtual bool canSetSensitivity(
                               const ::Ice::Current& = ::Ice::Current() );
   virtual void getSensitivity(Ice::Double &falseAlarmRate, Ice::Double &recall,
                               const ::Ice::Current& = ::Ice::Current() );
 public:
  int numStages;
  int featureType; // CvFeatureParams::HAAR, LBP, or HOG
  int boost_type;
  double minHitRate;
  double maxFalseAlarm;
  float weight_trim_rate;
  int max_depth;
  int weak_count;
  int width;
  int height;
};

class CascadeTrainI : public cvac::DetectorTrainer
{
 public:
  CascadeTrainI(cvac::ServiceManager *serv);
  ~CascadeTrainI();
	

 public:
  virtual void initialize(::Ice::Int, const ::Ice::Current& = ::Ice::Current() );
  virtual void process(const Ice::Identity &client, const ::cvac::RunSet&,
                       const ::Ice::Current& = ::Ice::Current() );
  virtual bool isInitialized(const ::Ice::Current& = ::Ice::Current() );
  virtual void destroy(const ::Ice::Current& = ::Ice::Current() );
  virtual ::std::string getName(const ::Ice::Current& = ::Ice::Current() );
  virtual ::std::string getDescription(const ::Ice::Current& = ::Ice::Current() );
  virtual void setVerbosity(::Ice::Int, const ::Ice::Current& = ::Ice::Current() );
  virtual ::cvac::TrainerPropertiesPrx getTrainerProperties(const ::Ice::Current& = ::Ice::Current());
  void writeBgFile( const RunSetWrapper& rsw, const std::string& bgFilename, 
                    int* pNumNeg, std::string datadir );

  bool createSamples( const RunSetWrapper& rsw, const SamplesParams& params,
                    const std::string& infoFilename,
                    const std::string& vecFilename, int* pNumPos, std::string datadir);
  bool createClassifier( const std::string& tempDir, 
                         const std::string& vecFname, 
                         const std::string& bgName,
                         int numPos, int numNeg, 
                         const TrainerPropertiesI *trainProps );
  void addDataPath(cvac::RunSet runset, const std::string &CVAC_DataDir);
 private:
  bool  fInitialized;    	
  int   m_cvacVerbosity;
  cvac::ServiceManager *mServiceMan;
  Ice::ObjectAdapterPtr mAdapter;
  TrainerPropertiesI *mTrainProps;
};

#endif //_CascadeTrainI_H__
