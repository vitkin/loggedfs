#!/bin/bash -x

#
# Generated - do not edit!
#

# Macros
TOP=`pwd`
CND_PLATFORM=OracleSolarisStudio_12.2_linuxCompatGNU-Linux-x86
CND_CONF=Debug
CND_DISTDIR=dist
CND_BUILDDIR=build
NBTMPDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}/tmp-packaging
TMPDIRNAME=tmp-packaging
OUTPUT_PATH=${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/loggedfs-csv
OUTPUT_BASENAME=loggedfs-csv
PACKAGE_TOP_DIR=loggedfs-csv/

# Functions
function checkReturnCode
{
    rc=$?
    if [ $rc != 0 ]
    then
        exit $rc
    fi
}
function makeDirectory
# $1 directory path
# $2 permission (optional)
{
    mkdir -p "$1"
    checkReturnCode
    if [ "$2" != "" ]
    then
      chmod $2 "$1"
      checkReturnCode
    fi
}
function copyFileToTmpDir
# $1 from-file path
# $2 to-file path
# $3 permission
{
    cp "$1" "$2"
    checkReturnCode
    if [ "$3" != "" ]
    then
        chmod $3 "$2"
        checkReturnCode
    fi
}

# Setup
cd "${TOP}"
mkdir -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/package
rm -rf ${NBTMPDIR}
mkdir -p ${NBTMPDIR}

# Copy files and create directories and links
cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "LICENSE" "${NBTMPDIR}/${PACKAGE_TOP_DIR}LICENSE" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "loggedfs.1" "${NBTMPDIR}/${PACKAGE_TOP_DIR}loggedfs.1" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "loggedfs.xml" "${NBTMPDIR}/${PACKAGE_TOP_DIR}loggedfs.xml" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "Makefile" "${NBTMPDIR}/${PACKAGE_TOP_DIR}Makefile" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "README" "${NBTMPDIR}/${PACKAGE_TOP_DIR}README" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/configurations.xml" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/configurations.xml" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Makefile-Debug.mk" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Makefile-Debug.mk" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Makefile-impl.mk" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Makefile-impl.mk" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Makefile-Release.mk" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Makefile-Release.mk" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Makefile-variables.mk" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Makefile-variables.mk" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Package-Debug.bash" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Package-Debug.bash" 0755

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/Package-Release.bash" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/Package-Release.bash" 0755

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/project.properties" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/project.properties" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/nbproject"
copyFileToTmpDir "nbproject/project.xml" "${NBTMPDIR}/${PACKAGE_TOP_DIR}nbproject/project.xml" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/src"
copyFileToTmpDir "src/Config.cpp" "${NBTMPDIR}/${PACKAGE_TOP_DIR}src/Config.cpp" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/src"
copyFileToTmpDir "src/Config.h" "${NBTMPDIR}/${PACKAGE_TOP_DIR}src/Config.h" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/src"
copyFileToTmpDir "src/Filter.cpp" "${NBTMPDIR}/${PACKAGE_TOP_DIR}src/Filter.cpp" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/src"
copyFileToTmpDir "src/Filter.h" "${NBTMPDIR}/${PACKAGE_TOP_DIR}src/Filter.h" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv/src"
copyFileToTmpDir "src/loggedfs.cpp" "${NBTMPDIR}/${PACKAGE_TOP_DIR}src/loggedfs.cpp" 0644

cd "${TOP}"
makeDirectory "${NBTMPDIR}/loggedfs-csv"
copyFileToTmpDir "src/loggedfs.h" "${NBTMPDIR}/${PACKAGE_TOP_DIR}loggedfs.h" 0644


# Generate tar file
cd "${TOP}"
rm -f ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/package/loggedfs-csv.tar
cd ${NBTMPDIR}
tar -vcf ../../../../${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/package/loggedfs-csv.tar *
checkReturnCode

# Cleanup
cd "${TOP}"
rm -rf ${NBTMPDIR}
