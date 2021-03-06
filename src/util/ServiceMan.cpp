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
#include <util/ServiceMan.h>
#include <util/Timing.h>
#include <util/FileUtils.h>
#include <Services.h>
#include <Ice/Ice.h>
#include <IceBox/IceBox.h>

using namespace cvac;


class cvac::ServiceManagerIceService : public ::IceBox::Service
{
public:
    ServiceManagerIceService(cvac::ServiceManager *man, cvac::CVAlgorithmService* serv)
    {
        mManager = man;
        mAdapter = NULL;
        mService = serv;

    }
    /**
     * The start function called by IceBox to start this service.
     */
    virtual void start(const ::std::string& name,
                  const ::Ice::CommunicatorPtr& communicator,
                  const ::Ice::StringSeq&)
    {
        localAndClientMsg(VLogger::INFO, NULL, "%s: starting\n", mManager->getServiceName().c_str());
	    mAdapter = communicator->createObjectAdapter(name);
	    mManager->clearStop();
        mManager->setIceName(name);
	    mAdapter->add(mService, communicator->stringToIdentity(mManager->getServiceName()));
	    mAdapter->activate();
        mManager->createSandbox();
	    localAndClientMsg(VLogger::INFO, NULL, "Service started: %s\n", 
                    mManager->getServiceName().c_str());
    }
    /**
     * The stop function called by IceBox to stop this service.
     */
    virtual void stop()
    {
        localAndClientMsg(VLogger::INFO, NULL, "Stopping Service: %s\n", 
                     mManager->getServiceName().c_str());
        mAdapter->deactivate();
        mManager->waitForStopService();
        localAndClientMsg(VLogger::INFO, NULL, "Service stopped: %s\n",
                     mManager->getServiceName().c_str());
    }
    
    ::Ice::ObjectAdapterPtr  getAdapter() { return mAdapter; }



private:
    ::Ice::ObjectAdapterPtr         mAdapter;
    cvac::CVAlgorithmService*       mService;
    cvac::ServiceManager*           mManager;
};

///////////////////////////////////////////////////////////////////////////////
cvac::ServiceManager::ServiceManager()
{
 
    mIceService = NULL;
    mStopState = None;
    mSandbox = NULL;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::setService(cvac::CVAlgorithmService *serv,
                      std::string serviceName)
{
    mServiceName = serviceName;
    mIceService = new ServiceManagerIceService(this, serv);
}

///////////////////////////////////////////////////////////////////////////////
//void cvac::ServiceManager::start(const ::std::string& name,const 
//                  ::Ice::CommunicatorPtr& communicator,const 
//                  ::Ice::StringSeq&)
//{
    
//}

///////////////////////////////////////////////////////////////////////////////
//void cvac::ServiceManager::stop()
//{
  
//}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::ServiceManager::getDataDir()
{
	Ice::PropertiesPtr props = (mIceService->getAdapter()->getCommunicator()->getProperties());
	vLogger.setLocalVerbosityLevel(props->getProperty("CVAC.ServicesVerbosity"));

	// Load the CVAC property: 'CVAC.DataDir'.  Used for the xml filename path, and to provide a prefix to Runset paths
	std::string dataDir = props->getProperty("CVAC.DataDir");
	if(dataDir.empty()) 
	{
		localAndClientMsg(VLogger::WARN, NULL, "Unable to locate CVAC Data directory, specified: 'CVAC.DataDir = path/to/dataDir' in </CVAC_Services/config.service>\n");
	}
	localAndClientMsg(VLogger::DEBUG, NULL, "CVAC Data directory configured as: %s \n", dataDir.c_str());
    return dataDir;
}
///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::createSandbox()
{
    mSandbox = new SandboxManager(getDataDir());
}
///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::stopService()
{
    if (mStopState == Running)
        mStopState = Stopping;
    else
        mStopState = Stopped;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::waitForStopService()
{
    stopService();
    while (isStopCompleted() == false)
    {
        cvac::sleep(100);
    }
}

///////////////////////////////////////////////////////////////////////////////
bool cvac::ServiceManager::stopRequested()
{
    if (mStopState == Stopping)
        return true;
    else 
        return false;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::clearStop()
{
    mStopState = None;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::stopCompleted()
{
    mStopState = Stopped;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::setStoppable()
{
    mStopState = Running;
}

///////////////////////////////////////////////////////////////////////////////
bool cvac::ServiceManager::isStopCompleted()
{
    if (mStopState != Stopping)
        return true;
    else
        return false;
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::ServiceManager::getServiceName()
{
    return mServiceName;
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::ServiceManager::getIceName()
{
    return mIceName;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ServiceManager::setIceName(std::string name)
{
    mIceName = name;
}


///////////////////////////////////////////////////////////////////////////////
// SandboxManager classes
///////////////////////////////////////////////////////////////////////////////

cvac::ClientSandbox::ClientSandbox(const std::string &clientName, 
                                  const std::string &CVAC_DataDir )
{
    _clientName = clientName;
    _cvacDataDir = CVAC_DataDir;
}
///////////////////////////////////////////////////////////////////////////////
std::string cvac::ClientSandbox::createTrainingDir()
{
    if (!_trainDir.empty())
    {// Get rid of old training directory 
        deleteDirectory(_trainDir);
    }
    if (_clientDir.empty())
    {
        getClientDir();
    }
    _trainDir = getTempFilename(_clientDir , "train_");   
    if (directoryExists(_trainDir))
    { // This should never happen
        deleteDirectory(_trainDir);
    } 
    makeDirectories(_trainDir);   
    return _trainDir;
    
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::ClientSandbox::getClientDir()
{
    if (_clientDir.empty())
    {
        _clientDir = _cvacDataDir + "/" + SANDBOX + "/" + _clientName;
        if (!directoryExists(_clientDir))
        {
            makeDirectories(_clientDir);
        }
    }
    return _clientDir;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::ClientSandbox::deleteTrainingDir()
{
    if (!_trainDir.empty())
    {
        deleteDirectory(_trainDir);
        _trainDir.erase();
    }
}

///////////////////////////////////////////////////////////////////////////////
cvac::SandboxManager::SandboxManager(const std::string & CVAC_DataDir)
{
    mCVAC_DataDir = CVAC_DataDir;
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::SandboxManager::createClientName(const std::string &serviceName,
                                             const std::string &connectionName)
{
    std::string clientName = serviceName + "_" + connectionName;
    std::vector<ClientSandbox>::iterator it;
    for (it = mSandboxes.begin(); it < mSandboxes.end(); ++it)
    {
        ClientSandbox sbox = (*it);
        if (clientName.compare(sbox.getClientName()) == 0)
        {
            return clientName;
        }
    }
    // We don't have that client name so add a new sandbox
    ClientSandbox *sandbox = new ClientSandbox(clientName, mCVAC_DataDir);
    mSandboxes.push_back(*sandbox);
    return clientName;
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::SandboxManager::createTrainingDir(const std::string &clientName)
{
    std::vector<ClientSandbox>::iterator it;
    for (it = mSandboxes.begin(); it < mSandboxes.end(); ++it)
    {
        if (clientName.compare((*it).getClientName()) == 0)
        {
           return (*it).createTrainingDir();
        }
    }
    std::string empty;
    return empty;
}

///////////////////////////////////////////////////////////////////////////////
void cvac::SandboxManager::deleteTrainingDir(const std::string &clientName)
{
    std::vector<ClientSandbox>::iterator it;
    for (it = mSandboxes.begin(); it < mSandboxes.end(); ++it)
    {
        if (clientName.compare((*it).getClientName()) == 0)
        {
           (*it).deleteTrainingDir();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::SandboxManager::getTrainingDir(const std::string &clientName)
{
    std::vector<ClientSandbox>::iterator it;
    for (it = mSandboxes.begin(); it < mSandboxes.end(); ++it)
    {
        if (clientName.compare((*it).getClientName()) == 0)
        {
           return (*it).getTrainingDir();
        }
    }
    std::string empty;
    return empty;
}

///////////////////////////////////////////////////////////////////////////////
std::string cvac::SandboxManager::createClientDir(const std::string &clientName)
{
    std::vector<ClientSandbox>::iterator it;
    for (it = mSandboxes.begin(); it < mSandboxes.end(); ++it)
    {
        if (clientName.compare((*it).getClientName()) == 0)
        {
           return (*it).getClientDir();
        }
    }
    std::string empty;
    return empty;
}
