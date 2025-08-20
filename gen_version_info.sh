#!/bin/sh


SHDIR=$(dirname `readlink -f $0`)

name="mm_"
gitversion=$(git rev-parse --short HEAD)
version=$(sed -n "/static const std::string LINUX_COMPATIBLE = /p" ${SHDIR}/common/global.h | awk -F '[\"]' '{print $2}')

finalname=${name}"v"${version}

if [ ${#gitversion} -eq 0 ]
then
    echo "there is no git in your shell"
    exit
else
    finalname=${finalname}"_"${gitversion}
fi;

finalversion=${finalname}
echo  "${finalversion}"

if [ -f $2/bin/mm ]
then
    mv $2/bin/mm $2/bin/${finalversion}
else
    echo "mm not exist"
fi;
 
#sed -i "s/build_commit_hash.*;/build_commit_hash = \"${gitversion}\";/g" ./ca/global.cpp
