#!/bin/bash

CVS2GIT=${PWD}/../../shttpd/cvs2svn/cvs2git
CVSFOLDER=${PWD}/$1
OPTIONS=${PWD}/cvs2git_pcc.options
GITREPO=${PWD}/pcc_repo
GITTEMP=${PWD}/cvs2git-tmp
REMOTEGIT=https://gitee.com/eaglexmw/pcc.git
GITCMD=git

echo "修改options文件..."
sed -i "580c\ \ \ \ r'$CVSFOLDER/'," ${OPTIONS}

echo "清理目录..."
rm -rf ${GITTEMP}
rm -rf ${GITREPO}

echo "转换CVS文件.."
mkdir ${CVSFOLDER}/CVSROOT
python2 ${CVS2GIT} --options=${OPTIONS}

echo "生成Git仓库文件..."
mkdir ${GITREPO} && cd ${GITREPO} && ${GITCMD} init .
cat ${GITTEMP}/git-{blob,dump}.dat | ${GITCMD} fast-import 

echo "清理目录..."
rm -rf ${GITTEMP}

echo "拷贝脚本、说明文件等..."
${GITCMD} checkout
mkdir cvt_script
cp ${PWD}/../$0    ./cvt_script/
cp ${OPTIONS}   ./cvt_script/
cp ../README.md ./
${GITCMD} add .
${GITCMD} commit -m"add convert script/Readme.md."
${GITCMD} remote add origin ${REMOTEGIT}
${GITCMD} push --all origin -f

exit 0
