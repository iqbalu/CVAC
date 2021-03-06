#
# stopServices.sh  Script to stop CVAC services for binary distribution
#
# Please set INSTALLDIR to install directory
export INSTALLDIR=
if [ "${INSTALLDIR}" == "" ]
then
    echo "INSTALLDIR needs to be defined! Please set INSTALLDIR to binary distribution install directory"
    exit
fi
export PYTHONEXE=/usr/bin/python2.6
export PATH=$PATH:${INSTALLDIR}/bin
export ICEDIR=${INSTALLDIR}/3rdparty/ICE
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:${INSTALLDIR}/lib"
cd ${INSTALLDIR}

# C/C++ and Java services, via icebox admin
${ICEDIR}/bin/iceboxadmin --Ice.Config=config.admin shutdown
${ICEDIR}/bin/iceboxadmin --Ice.Config=config.java_admin shutdown

# Python services that are listed in python.config
if [ "${PYTHONEXE}" != "" ] && [ -f "${INSTALLDIR}/python.config" ]
then
    grep -v -e ^# ${INSTALLDIR}/python.config | while read LINE
    do
        if [ "`which pkill`"  != "" ];
        then
            # pkill seems to work better than killall
            # echo pkill -xf "${PYTHONEXE} $LINE"
            pkill -xf "/usr/bin/python $LINE"
        else
            # echo killall $LINE
            killall $LINE
        fi
    done
fi

echo CVAC services stopped
exit
