#!/usr/bin/env python

# This file is part of VoltDB.
# Copyright (C) 2008-2012 VoltDB Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

import sys
import os.path
import shutil
import fnmatch
import subprocess
import time
import filecmp
import socket
from optparse import OptionParser
from subprocess import call # invoke unix/linux cmds
from xml.etree import ElementTree
from xml.etree.ElementTree import Element, SubElement
# add the path to the volt python client, just based on knowing
# where we are now
sys.path.append('../../src/py_client')
try:
    from voltdbclient import *
except ImportError:
    sys.path.append('./src/py_client')
    from voltdbclient import *
from Query import VoltQueryClient
from XMLUtils import prettify # To create a human readable xml file

hostname = socket.gethostname()
pkgName = {'comm': 'LINUX-voltdb', 'pro': 'LINUX-voltdb-ent'}
tail = "tar.gz"
sepLineD = "========================================================"
sepLineS = "--------------------------------------------"
# http://volt0/kits/candidate/LINUX-voltdb-2.8.1.tar.gz
# http://volt0/kits/candidate/LINUX-voltdb-ent-2.8.1.tar.gz
root = 'http://volt0/kits/candidate/'
destDir = "/tmp/"
elem2Test = {'helloworld':'run.sh', 'voltcache':'run.sh', 'voltkv':'run.sh', 'voter':'run.sh'}
defaultHost = "localhost"
defaultPort = 21212

# To parse the output of './examples/voter/run.sh client' and get a specific portion
# of the output. A sample value would be like the one below:
'''
 Voting Results
 --------------------------------------------------------------------------------

 A total of 8166781 votes were received...
  - 7,816,923 Accepted
   -    79,031 Rejected (Invalid Contestant)
     -        12 Rejected (Maximum Vote Count Reached)
     -         0 Failed (Transaction Error)

    Contestant Name     Votes Received
    Edwina Burnam            2,156,993
    Jessie Eichman           1,652,654
    Alana Bregman            1,189,909
    Kelly Clauss             1,084,995
    Jessie Alloway           1,060,892
    Tabatha Gehling            939,604

    The Winner is: Edwina Burnam
'''
def findSectionInFile(srce, start, end):
    flag = 0
    status = False
    ins = open(srce, "r" )
    str = None
    msg = "The Winner is NOT Edwina Burnam"
    for line in ins:
        if(line.find(start) > -1):
            flag = 1
        if(flag == 1):
            if(str == None):
                str = line
            else:
                str += line
        if(line.find(end) > -1):
            flag = 0
            status = True
            msg = "The Winner is Edwina Burnam"
    return (status, msg)

# To read the first line of a srouce file 'srce'
def readFirstLine(srce):
    firstline = None
    if(os.path.getsize(srce) > 0):
        with open(srce) as f:
            content = f.readlines()
        firstline = content[0].rstrip()
    return firstline

# The release number can be optionally passed in from cmdline with -r switch
# If it's ommitted at cmdline, then this function is called to get the release
# number from 'version.txt'
def getReleaseNum():
    cwd = os.path.realpath(__file__)
    path = os.path.dirname(os.path.abspath(__file__))
    root = path.replace("tests/scripts", "")
    verFile = root + "version.txt"
    ver = readFirstLine(verFile)
    return ver

# Always create a fresh new subdir
def createAFreshDir(dir):
    ret = 0
    if os.path.exists(dir):
        shutil.rmtree(dir)
    if not os.path.exists(dir):
        os.makedirs(dir)
    if not os.path.exists(dir):
        ret = -1
    return ret

# To get a VoltDB tar ball file and untar it in a designated place
def installVoltDB(pkg, release):
    info = {}
    info["ok"] = False
    if(pkg not in pkgName):
        info["err"] = "Invalid pkg name: '%s'!" % pkg
        info["err"] += "\nThe valid pkg names are:\n"
        for k in pkgName:
            info["err"] += k + ", "
        info["err"] = info["err"].strip() # Trim the leading/trailing spaces
        info["err"] = info["err"][:-1] # Trim the last char ','
        return info

    pkgname = pkgName[pkg] + '-' + release + "." + tail
    srce = root + pkgname
    dest = destDir + pkgname
    cmd = "wget " + srce + " -O " + dest + " 2>/dev/null"

    ret = call(cmd, shell=True)
    if ret != 0 or not os.path.exists(dest):
        info["err"] = "Cannot download '%s'" % srce
        return info

    fsize = os.path.getsize(dest)
    if fsize == 0:
        info["err"] = "The pkg '%s' is blank!" % dest
        return info

    ret = createAFreshDir(workDir)
    if ret != 0:
        info["err"] = "Cannot create the working directory: '%s'" % workDir
        return info

    cmd = "tar zxf " + dest + " -C " + workDir + " 2>/dev/null"
#    print "cmd = '%s'" % cmd
    ret = call(cmd, shell=True)
    if ret == 0:
#        info["dest"] = dest
        info["srce"] = srce
        info["pkgname"] = pkgname
        info["workDir"] = workDir
        info["ok"] = True
    else:
        info["err"] = "VoltDB pkg '%s' installation FAILED at location '%s'" \
            % (dest, workDir) 
    return info
# end of installVoltDB(pkg, release):

# Test Suite can be optionally passed in from cmdline with the swtich '-s'
# Sample key/val pairs for testSuiteList are:
# key: voltcache,   val: /tmp/exp_test/voltdb-2.8.1/examples/voltcache
# key: voter,       val: /tmp/exp_test/voltdb-2.8.1/examples/voter
# key: voltkv,      val: /tmp/exp_test/voltdb-2.8.1/examples/voltkv
# key: helloworld', val: /tmp/exp_test/voltdb-2.8.1/doc/tutorials/helloworld
def setTestSuite(dname, suite):
    testSuiteList = {}
    for dirname, dirnames, filenames in os.walk(dname):
        for subdirname in dirnames:
            if subdirname in elem2Test.keys():
                path = os.path.join(dirname, subdirname)
                run_sh = path + "/" + elem2Test[subdirname]
                if(os.access(run_sh, os.X_OK)):
                    if(suite != "all"):
                        if(path.find(suite) > -1):
                            testSuiteList[suite] = path
                    else:
                        if(path.find(subdirname) > -1):
                            testSuiteList[subdirname] = path
    return testSuiteList

# Not used yet.
# It would be necessary if we wanted to run certain queries before 
def stopPS(ps):
    print "Going to kill this process: '%d'" % ps.id
    killer = subprocess.Popen("kill -9 %d" % (ps.pid), shell = True)
    killer.communicate()
    if killer.returncode != 0:
#        print >> sys.stderr, "Failed to kill the server process %d" % (server.pid)
        print "Failed to kill the server process %d" % (ps.pid)

# To return a voltDB client
def getClient():
    host = defaultHost
    port = defaultPort
    client = None
    for i in xrange(10):
        try:
            client = VoltQueryClient(host, port)
            client.set_quiet(True)
            client.set_timeout(5.0) # 5 seconds
            break
        except socket.error:
            time.sleep(1)

    if client == None:
        print >> sys.stderr, "Unable to connect/create client"
        sys.stderr.flush()
        exit(1)

    return client

# Not used yet. 
# Currently, both startService() and stopService() are implemented in 
# execThisService(). However, if we wanted to run certain queries before 
# shutdown VoltDB, we have to separate startService() and stopService(),
# so that we can add more implementations in between.
def startService(service, logS, logC):
    service_ps = subprocess.Popen(service + " > " + logS + " 2>&1", shell=True)
    time.sleep(2)
    client = getClient()
    ret = call(service + " client > " + logC + " 2>&1", shell=True)
#    print "returning results from service execution: '%s'" % ret
    time.sleep(1)
    return (service_ps, client)

# Not used yet. Refer to the comments for startService()
def stopService(ps, serviceHandle):
    serviceHandle.onecmd("shutdown")
    ps.communicate()

# To execute 'run.sh' and save the output in logS
# To execute 'run.sh client' and save the output in logC
def execThisService(service, logS, logC):
    service_ps = subprocess.Popen(service + " > " + logS + " 2>&1", shell=True)
    time.sleep(2)
    client = getClient()
    ret = call(service + " client > " + logC + " 2>&1", shell=True)
#    print "returning results from service execution: '%s'" % ret
    client.onecmd("shutdown")
    service_ps.communicate()

# Further assertion is required
# We want to make sure that logFileC contains several key strings
# which is defined in 'staticKeyStr'
def assertVotekv_Votecache(mod, logC):
    staticKeyStr = {
"Command iiiiLine Configuration":1,
"Setup & Initialization":1,
"Starting Benchmark":1,
"KV Store Results":1,
"Client Workload Statistics":1,
"System Server Statistics":1,
    }

    dynamicKeyStr = {}
    with open(logC) as f:
        content = f.readlines()

    cnt = 0
    for line in content:
        x = line.strip()
        dynamicKeyStr[x] = 1
        if x in staticKeyStr.keys():
            cnt += 1

    result = False
    msg = None
    keys = {}
    if(cnt == len(staticKeyStr)):
#        msg = "The client output has all the expected key words:\n" + sepLineS + "\n"
        msg = "The client output has all the expected key words:"
#        for key in staticKeyStr:
#            msg += key + "\n"
        keys = staticKeyStr
        result = True
    else:
        msg = "The client output does not have all the expected key words:\n"
#        msg += "The missing keys are:\n" + sepLineS + "\n"
        for key in staticKeyStr:
            if key not in dynamicKeyStr.keys():
                keys[key] = key
#                msg += key + "\n"

    return (result, msg, keys)

# We want to make sure that logFileC contains this KEY string:
# The Winner is: Edwina Burnam
def assertVoter(mod, logC):
    result = False
    aStr = "Voting Results"
    expected = "The Winner is: Edwina Burnam"
    (result, section) = findSectionInFile(logC, aStr, expected)
    return (result, section)

# To make sure that we see the key string 'Hola, Mundo!'
def assertHelloWorld(modulename, logC):
    expected = "iHola, Mundo!"
    actual = readFirstLine(logC)
    if(expected == actual):
        msg = expected
        result = True
    else:
        msg = "Expected '%s' for module '%s'. Actually returned: '%s'" % (expected, modulename, actual)
        result = False
    return (result, msg)

# To make sure the content of logC which is the output of 'run.sh client'
# is identical to the static baseline file.
# If True, the test is PASSED
# If False, then we need to parse the LogC more carefully before we declare 
# this test is FAILED
def assertClient(e, logC):
    baselineD = origDir + "/plannertester/baseline/"
    baselineF = baselineD + e + "/client_output.txt"
#    print "baselineD = '%s', baselineF = '%s'" % (baselineD, baselineF)
    ret = False
    msg = None
    if(os.path.exists(baselineF)):
        ret = filecmp.cmp(baselineF, logC)
        if(ret == True):
            msg = "The client output matches the baseline:"
        else:
            msg = "Warning!! The client output does NOT match the baseline:"
        msg += "\nBaseline: %s" % baselineF
        msg += "\nThe client output: %s" % logC
    else:
        msg = "Warning!! Cannot find the baseline file:\n%s" % baselineF
    return (ret, msg)

def startTest(testSuiteList):
    statusBySuite = {}
    msgBySuite = {}
    keyWordsBySuite = {}
    for e in testSuiteList:
        os.chdir(testSuiteList[e])
        currDir = os.getcwd()
        service = elem2Test[e]
        print "===--->>> Start to test this suite: %s" % e
#        logFileS = workDir + "/" + e + "_server"
#        logFileC = workDir + "/" + e + "_client"
        logFileS = "/tmp/" + e + "_server"
        logFileC = "/tmp/" + e + "_client"
        msg1 = msg2 = None
        print "logFileS = '%s', logFileC = '%s'" % (logFileS, logFileC)
#        execThisService(service, logFileS, logFileC)
        if(e == "helloworld"):
            (result, msg1) = assertHelloWorld(e, logFileC)
#            statusBySuite[e]["status1"] = result
#            statusBySuite[e]["msg"] = msg1
            statusBySuite[e] = result
            msgBySuite[e] = msg1
            keyWordsBySuite[e] = None
            print "1 cyan e = '%s', result = '%s'" % (e, result)
        elif(e == "voter"):
            (result, msg1) = assertVoter(e, logFileC)
#            statusBySuite[e]["msg"] = msg1
#            statusBySuite[e]["status2"] = result
            statusBySuite[e] = result
            msgBySuite[e] = msg1
            keyWordsBySuite[e] = None
            print "2 cyan e = '%s', result = '%s'" % (e, result)
        elif(e == "voltkv" or e == "voltcache"):
            (result, msg1, keys) = assertVotekv_Votecache(e, logFileC)
#            statusBySuite[e]["msg"] = msg1
#            statusBySuite[e]["status3"] = result
            statusBySuite[e] = result
            msgBySuite[e] = msg1
            keyWordsBySuite[e] = keys
            print "3 cyan e = '%s', result = '%s'" \
                % (e, result)
        else:
            print "e = '%s' ==-->> to be implemented..." % e
            statusBySuite[e] = False
            msgBySuite[e] = "Unknown Suite:'%s'. To be implemented..." % e
            keyWordsBySuite[e] = None

        os.chdir(origDir)
    # end of for e in testSuiteList:
    for xx in statusBySuite:
        print "===--->>>xx = '%s', vaL: '%s'" % (xx, statusBySuite[xx])
    return (statusBySuite, msgBySuite, keyWordsBySuite)
# end of startTest(testSuiteList):

#<?xml version="1.0" encoding="UTF-8" ?>
#<testsuites>
#  <testsuite errors="0" failures="0" hostname="ip-10-143-131-25" id="0" name="TestHSQLDB" package="org.hsqldb_voltpatches" tests="1" time="0.455" timestamp="2012-09-10T17:55:18">
#<testcase classname="Linux_community_build" name="helloworld"/>
#<testcase classname="Linux_community_build" name="voter"/>
#<testcase classname="Linux_community_build" name="voltkv">
#<testcase classname="Linux_community_build" name="voltcache"/>
#<failure nessage="This test experienced some sort of difficulties."> 
#</failure>
#</testcase>
#  </testsuite>
#</testsuites>
#
#    cluster = SubElement(deployment, 'cluster',
#            {'kfactor':kfactor,'sitesperhost':sitesperhost,'hostcount':hostcount})
def create_rpt(info, status, msg, keys):
    proj = Element('project')
    testsuites = SubElement(proj, 'testsuites',
            {'package':info["pkgname"],'URL':info["srce"],
             'hostname':hostname})
    print "testname = '%s'" % testname
    for i in status:
        failureCnt = "0"
        errCnt = "0"
        if(status[i] == False):
            failureCnt = "1"
        else:
            failureCnt = "0"

        print "==-->>suite name: '%s', failureCnt: '%s', status = '%s'" \
            % (i, failureCnt, status[i])
        if(info["ok"] == False):
            errCnt = "1"
        else:
            errCnt = "0"
        testsuite = SubElement(testsuites, 'testsuite',
            {'errors':errCnt,'failures':failureCnt, 'name':testname})
        testcase = SubElement(testsuite, 'testcase', {'name':i})

        if(failureCnt == "1"):
            failure = SubElement(testcase, 'failure',
                    {'Message':msg[i]})
            misStr = None
            if(keys[i] != None):
                for j in keys[i]:
                    if(misStr == None):
                        misStr = j
                    else:
                        misStr += ", " + j
                missing = SubElement(failure, 'Missing',
                        {'MissingString':misStr})
        else:
            failure = SubElement(testcase, 'info',
                    {'Message':msg[i]})
        if(errCnt == "1"):
            error = SubElement(testcase, 'error',
                    {'Error':info["err"]})

    rptf = "/tmp/exp_rpt.xml"
    fo = open(rptf, "wb")
    fo.write(prettify(proj))
    fo.close()
    if not os.path.exists(rptf):
        rptf = None
    return rptf

if __name__ == "__main__":
    usage = "Usage: %prog [options]"
#    parser = OptionParser()
    parser = OptionParser(usage="%prog [-r <release #>] [-p <comm|pro> <-s helloworld|voter|voltkv|voltcache>]", version="%prog 1.0")
    parser.add_option("-r", "--release", dest="release",
                      help="VoltDB release no. If ommitted, it will find it from version.txt.")
    parser.add_option("-p", "--pkg", dest="pkg",
                      help="VoltDB package type: Community or Pro. Defalut is Community.")
    parser.add_option("-s", "--suite", dest="suite",
                      help="Test suite name, if not set, then take all suites")

    parser.set_defaults(pkg="comm")
    parser.set_defaults(suite="all")
    (options, args) = parser.parse_args()
    testname = os.path.basename(os.path.abspath(__file__)).replace(".py", "")
    workDir = destDir + testname
    # e.g. workDir = '/tmp/exp_test'

    suite = options.suite
    if suite not in elem2Test.keys() and suite != "all":
        print "Warning: unknown suite name - '%s'" % suite
        print "Info: So we're going to cover all test suites in this run"

    origDir = os.getcwd()

    releaseNum = options.release
    if(releaseNum == None):
        releaseNum = getReleaseNum()

    print "############################################"
    print "Tested Version in this RUN: %s" % releaseNum
    print "############################################"

    ret = installVoltDB(options.pkg, releaseNum)
    if not ret["ok"]:
        print "Error!! %s" % ret["err"]
#        print >> sys.stderr, "Error!! %s" % ret["err"]
        exit(1)
    for k1 in ret:
        print "key: '%s', Val: '%s'" % (k1, ret[k1])

    testSuiteList = setTestSuite(ret["workDir"], suite)
#    for i in testSuiteList:
#        print "i = '%s', exec = '%s'" % (i, testSuiteList[i])
#    exit(1)

    (tf, msg, keys) = startTest(testSuiteList)
    status = True
    reportXML = create_rpt(ret, tf, msg, keys)
    print "===--->>>reportXML = '%s'" % reportXML
#    for module in success:
#        print "cyan Module: '%s'. Val: '%s'" \
#            % (module, success[module])
#        if not success[module]["status"]:
#            print >> sys.stderr, "\n%s\n%s\nTest '%s' FAILED!!\n%s" \
#                % (success[module]["msg"], sepLineS, module, sepLineD)
#            print "\n%s\n%s\nTest '%s' FAILED!!\n%s" \
#                % (success[module]["msg"], sepLineS, module, sepLineD)
#            status = False
#        else:
#            print "\n%s\n%s\nTest '%s' PASSED!!\n%s" \
#                % (success[module]["msg"], sepLineS, module, sepLineD)
    if(status == False):
        print "\nAt lease one test suite is Failed!!\n"
        exit(1)
    exit(0)
