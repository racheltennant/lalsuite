"""
  * Copyright (C) 2004, 2005 Cristina V. Torres
  *
  *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with with program; see the file COPYING. If not, write to the
  *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
  *  MA  02111-1307  USA
"""

"""
Classes and methods for the tracksearch pipeline
"""

__author__ = 'Cristina Torres <cristina@phys.utb.edu>'
__date__ = '$Date$'
__version__ = ''

import os
import sys
import re
import time
import string
import math
import exceptions
import ConfigParser
from tracksearchutils import determineDataPadding
from glue import pipeline


#Method to Build directory which is not present
def buildDir(dirpath):
    pathArray=[]
    stopVar=0
    #Check for trailing slash
    oldDir=str(os.path.abspath(dirpath)).__add__('/')
    while stopVar == 0:
        splitContainer=os.path.split(oldDir)
        newSec=splitContainer[0]
        pathArray.append(newSec)
        oldDir=newSec
        if ((splitContainer[1] == '') and (splitContainer[0] == '/')):
            stopVar=1
    #Now reverse this list and recurse creating the pathing as needed
    pathArray.reverse()
    for pathBlock in pathArray:
        if os.path.isfile(pathBlock):
            sys.stderr.write('We have a problem the path requested may clobber an existing file!\n')
            sys.stderr.write('File :'+str(pathBlock)+'\n')
            os.abort()
        if not os.path.isdir(pathBlock):
           #sys.stdout.write('Making path:'+str(pathBlock)+'\n')
           try:
               os.mkdir(pathBlock)
           except: pass
    
    #Now double check that directory exists else flag error
    if not os.path.isdir(dirpath):
        os.stderr.write('\n WARNING: Directory still not ready contain(s) '+str(pathArray.__len__())+' branches!\n')
        os.stderr.write(str(dirpath))
        os.stderr.write(str(pathArray))
#End def

def isPowOf2(input):
    inNum=int(input)
    ans=inNum&(inNum-1)
    if ans== 0:
        return 1
    else:
        return 0
#End def

def powOf2Floor(input):
    ans=0
    inNum=int(input)
    if inNum < 0:
        sys.stderr.write("ERROR Unexpected negative value found!\n")
    newPow2=1
    expPow=1
    if not isPowOf2(inNum):
        while newPow2 < inNum:
            newPow2=math.pow(2,expPow)
            expPow+=1
        expPow+=-1
        ans=math.pow(2,expPow)
    else:
        ans=inNum
    return ans

def determineLayerPath(cp,blockID,layerID,channel=''):
    """
    Build the correct layer path for each layer of a multi-resolution search.
    """
    layerPath=os.path.normpath(str('%s/%s/'%(determineBlockPath(cp,blockID,channel),layerID)))
    return str(layerPath)
#End def

def determineBlockPath(cp,blockID,channel=''):
    """
    Build the correct block path for each data segment to be searched.
    """
    blockPath=os.path.normpath(str('%s/%s/%s/'%(cp.get('filelayout','workpath'),blockID,channel)))
    return str(blockPath)
#End def

class tracksearchCheckIniFile:
    """
    This class will check the ini file for appropriate arguements
    where possible.  The errors will be listed upon request
    Mainly we check for files which the job will need to run
    executables and other aux files.
    """

    def __init__(self,cp):
        self.iniOpts=cp
        self.errList=[]
    #End init

    def checkOpts(self):
        #Check for existance of [condor] section files
        condorOptList=self.iniOpts.options('condor')
        fileNotFound=False
        homedirectory=os.path.abspath(str(os.getenv('HOME')))+'/'
        for entry in condorOptList:
            optValue=self.iniOpts.get('condor',entry)
            if optValue.startswith('~/'):
                newValue=os.path.normpath(optValue.replace('~/',homedirectory))
                self.iniOpts.set('condor',entry,newValue)
                optValue=newValue
            if str(optValue).__contains__('/'):
                if not os.path.exists(str(optValue)):
                    self.errList.append('Can not find :'+str(entry)+':'+str(optValue))
                    fileNotFound=True
        if fileNotFound:
            LALpath=os.getenv('PATH')
            pathsToSearch=[]
            self.errList.append('Try looking for [condor] program paths in:')
            for entry in LALpath.split(':'):
                if (entry.__contains__('lal') and entry.__contains__('bin')):
                    self.errList.append(entry)
                
        #Check [tracksearchbase] section
        lambdaH=0
        lambdaL=1
        lambdaH=self.iniOpts.get('tracksearchbase','start_threshold')
        lambdaL=self.iniOpts.get('tracksearchbase','member_threshold')
        if lambdaL > lambdaH :
            self.errList.append('Error invalid choice for lambda parameters')
            self.errList.append('Lh:'+str(lambdaH)+' Ll:'+str(lambdaL))
        CL=float(self.iniOpts.get('tracksearchbase','length_threshold'))
        if CL < 3:
            self.errList.append('Error invalid choice for curve length threshold')
        IP=float(self.iniOpts.get('tracksearchbase','power_threshold'))
        if IP < 0:
            self.errList.append('Error invalid choice for integrated power threshold')
        #Check [tracksearchtime] section
        WS=1
        NOFB=0
        WS=int(self.iniOpts.get('tracksearchtime','window_size'))
        NOFB=int(self.iniOpts.get('tracksearchtime','number_of_freq_bins'))
        if WS > NOFB:
            self.errList.append('Error window length inconsistent!')
        #Check [multichannel] section if present
        if self.iniOpts.has_section('multichannel'):
            for channel in self.iniOpts.options('multichannel'):
                channelEntry=self.iniOpts.get('multichannel',channel).strip()
                if (channelEntry==''):
                    sys.stdout.write("Found blank channel line for,"+str(channel))
                    sys.stdout.write("Deleting that key from installation.")
                    self.iniOpts.remove_option('multichannel',channel)
        #Check [layerconfig] section
        LTBS=self.iniOpts.get('layerconfig','layerTopBlockSize')
        layerOpts=self.iniOpts.options('layerconfig')
        layerOpts.sort()
        layerTimes=[]
        layerTimesOrig=[]
        layerTimesKey=[]
        layerSetSizeKey=[]
        topLayerBlock=LTBS
        for entry in layerOpts:
            optValue=float(self.iniOpts.get('layerconfig',entry))
            if str(entry).lower().__contains__('timescale'):
                layerTimes.append(optValue)
                layerTimesOrig.append(optValue)
                layerTimesKey.append([int(str(entry).lower().strip("layer").strip("timescale")),optValue])
            if str(entry).lower().__contains__('setsize'):
                layerSetSizeKey.append([int(str(entry).lower().strip("layer").strip("setsize")),optValue])
        layerTimes.sort()
        if layerTimesOrig != layerTimes:
            self.errList.append('Error inconsistent layer time scales!')
        #Check that the amount of work given is less than equal to available data in segment block
        # know as the top layer block
        layerSetSizeKey.sort()
        layerTimesKey.sort()
        for index in range(0,layerTimesKey.__len__()):
            if float(layerSetSizeKey[index][1]) * float(layerTimesKey[index][1]) > topLayerBlock:
                self.errList.append('Error inconsistent assigned workload for layerSetSize and layerTimeScale options. Level: '+str(index+1))
        #Check [pylibraryfiles] section
        condorOptList=self.iniOpts.options('pylibraryfiles')
        for entry in condorOptList:
            optValue=self.iniOpts.get('pylibraryfiles',entry)
            if optValue.startswith('~/'):
                newValue=os.path.normpath(optValue.replace('~/',homedirectory))
                self.iniOpts.set('condor',entry,newValue)
                optValue=newValue
            if str(optValue).__contains__('/'):
                if not os.path.exists(str(optValue)):
                    self.errList.append('Can not find python library file:'+str(entry)+':'+str(optValue))

    #end checkOpts def

    def numberErrors(self):
        return self.errList.__len__()
    #end numberErrors def

    def printErrorList(self):
        sys.stderr.write(str(self.numberErrors())+' INI file Errors found!\n')
        for error in self.errList:
            sys.stderr.write(error+'\n')
    #end printErrorList

    def hasInjectSec(self):
        injectSecFound=False
        injectSecFound=self.iniOpts.has_section('tracksearchinjection')
        #Check the [injectionsection] section if present searching for injection file.
        if injectSecFound:
            injectFile=self.iniOpts.get('tracksearchinjection','inject_file')
            injectCount=int(self.iniOpts.get('tracksearchinjection','inject_count'))
            injectLayerSize=int(self.iniOpts.get('layerconfig','layer1SetSize'))
            injectLayerTimeScale=float(self.iniOpts.get('layerconfig','layer1TimeScale'))
            injectTopBlockSize=float(self.iniOpts.get('layerconfig','layerTopBlockSize'))
            totalJobNodes=injectTopBlockSize/(injectLayerTimeScale*injectLayerSize)
            totalInjects=totalJobNodes*injectCount
            sys.stdout.write("ESTIMATING INJECTION PROPERTIES!\n")
            sys.stdout.write("Time Nodes with Injection:"+str(totalJobNodes)+"\n")
            sys.stdout.write("Total Injections to do   :"+str(totalInjects)+"\n")
            sys.stdout.write("Injects per Time Node    :"+str(injectCount)+"\n")
            if not os.path.exists(injectFile):
                self.errList.append('Can not find text file to inject :'+str(injectFile))
                os.abort()
        return injectSecFound
    #end injectSecTest method
#end Class

            
class tracksearchConvertSegList:
    """
    This class declaration is responsible for opening some segment
    file and reparsing it into block sizes of exactly n seconds each
    from data in the original segments file.  It is this file from where we
    construct an analysis DAG for each line item.
    """
    def __init__(self,segmentFileName,newDuration,configOpts):
        self.origSegObject=pipeline.ScienceData()
        self.newSegObject=pipeline.ScienceData()
        self.newSegFilename=segmentFileName+'.revised'
        self.segFilename=segmentFileName
        self.duration=newDuration
        self.cp=configOpts
    #End __init__()
    
    def writeSegList(self):
        #Read segents from file
        try:
            self.origSegObject.read(self.segFilename,self.duration)
        except IndexError:
            sys.stderr.write("Does not appear to SegWizard formatted file.\n")
            sys.stderr.write("Trying to reparse it assuming two column GPS list.\n")
            sys.stderr.write("No guarantee this will work.\n")
            index=0
            input_fp=open(self.segFilename,'r')
            tmpFilename=self.segFilename+'_TMP'
            tmp_fp=open(tmpFilename,'w')
            for line in input_fp.readlines():
                if (line[0] == '#'):
                    tmp_fp.write(line)
                else:
                    fields=line.split(" ")
                    tmp_fp.write(str(index)+" "+str(int(fields[0]))+" "+str(int(fields[1]))+" "+str(int(fields[1])-int(fields[0]))+"\n")
                index=index+1
            input_fp.close()
            tmp_fp.close()
            self.segFilename=tmpFilename
            sys.stderr.write("Reparsed it.  Trying to load reparsed TMP file.\n")
            self.origSegObject.read(self.segFilename,self.duration)
        #
        #Read the file header
        input_fp=open(self.segFilename,'r')
        newHeading=[]
        for headline in input_fp.readlines():
            if str(headline)[0] == '#':
                 newHeading.append(headline)
        newHeading.append('# This file is a reprocessed list at new intervals.\n')
        newHeading.append('# We drop sections were there is not enough of original data\n')
        newHeading.append('# This file drawn from '+self.segFilename+'\n')
        newHeading.append('# This file should be associated with appropriate INI file\n')
        newHeading.append('# This revised segment list has the burned data removed\n')
        if determineDataPadding(self.cp) > 0:
            newHeading.append('#This file is to be used with a config file asking for data padding of '+str(determineDataPadding(self.cp))+'\n')
        output_fp=open(self.newSegFilename,'w')
        for newHeadline in newHeading:
            output_fp.write(newHeadline)
        #Write redivided list to file
        index=0
        #TO DO
        #Integrate a data burning step to avoid starting any
        #analysis at the exact start/end of science mode segment
        #??? Check why the dataFind end time is off by bin_buffer seconds?
        if self.cp.has_option('layerconfig','burndata'):
            burnOption=int(self.cp.get('layerconfig','burndata'))
        else:
            burnOption=0
        dataToBurn=max([determineDataPadding(self.cp),burnOption])
        for bigChunks in self.origSegObject:
            #Burn off data from bigChunks
            bigChunks.set_start(bigChunks.start()+dataToBurn)
            bigChunks.set_end(bigChunks.end()-dataToBurn)
            bigChunks.make_chunks(self.duration)
            for newChunk in bigChunks:
                index+=1
                label="%d %d %d %d\n"%(index,newChunk.start(),newChunk.end(),newChunk.end()-newChunk.start())
                output_fp.write(label)
        output_fp.close

    def getSegmentName(self):
        #Return a string containing the name of the revised segment list
        return(self.newSegFilename)

class tracksearchHousekeeperJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    This class is responsible for cleaning out files that are not needed after an anaylsis run completes.
    Clearing out these file wills save disk space.  Only files in the RESULTS directory which are
    final data products will remain.  The MAP files and .candidates files in each level of the run
    will be removed.  The candidate files will be tar.gzed and the MAP files will be scraped.
    We will also remove the .diag files if they are present as well.  This is an umbrella feature.  It works on
    clearing files from [filelayout],workpath and its subdirectories.  This is all blockIDs for the DAG.
    """
    def __init__(self,cp,block_id,dagDir,channel=''):
        self.dagDirectory=dagDir
        self.cp=cp
        self.block_id=block_id
        self.__executable = cp.get('condor','housekeeper')
        self.__universe = cp.get('condor','housekeeper_universe')
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        for sec in ['housekeeper']:
            #Check the ini file and warn that we are enabling the rescue of RESULTS directory!
            ignorePathString=cp.get(sec,'ignore-path')
            if not(ignorePathString.__contains__('RESULTS')):
                cp.set(sec,'ignore-path',ignorePathString+',RESULTS')
                sys.stderr.write("I noticed you were not omitting RESULTS directory from cleaning.\n")
                sys.stderr.write("You would delete the RESULTS files, thereby wasting CPU time.\n")
                sys.stderr.write("I'll assume you just forgot and we will keep those files.\n")
            self.add_ini_opts(cp,sec)
        workpath=cp.get('filelayout','workpath')
        self.add_opt('parent-dir',workpath)
        blockPath=determineBlockPath(cp,self.block_id,channel)
        workpath=blockPath
        workpath_logs=workpath+'/logs'
        buildDir(workpath_logs)
        self.set_stdout_file(os.path.normpath(workpath_logs+'/tracksearchHousekeeper-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(workpath_logs+'/tracksearchHousekeeper-$(cluster)-$(process).err'))
        filename="/tracksearchHousekeeper--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
        #End init
    #End class tracksearchHousekeeperJob
    
class tracksearchTimeJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    The class running a TIME instance of the tracksearch code.  This
    code must be invoked specifying the type of job to create.  The argument
    is a string either -> normal  -> injection
    Then the appropriate ini options will be used to create the job.
    """
    def __init__(self,cp,block_id,layer_id,dagDir,jobType,channel=''):
        self.injectFile=""
        self.jobType=jobType
        self.dagDirectory=dagDir
        #ConfigParser object -> cp
        self.__executable = cp.get('condor','tracksearch')
        self.__universe = cp.get('condor','tracksearch_universe');
        self.validJobTypes=['normal','injection']
        #If invalid type is requested display warning and
        #assume a normal injection was requested
        if not self.validJobTypes.__contains__(str(self.jobType).lower()):
            sys.stderr.write("Warning: You requested invalid tracksearchTimeJob type!\n")
            sys.stderr.write("Assuming you meant -> normal <- job type.\n")
            self.jobType='normal'
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        blockID=block_id
        layerID=layer_id
        layerPath=determineLayerPath(cp,blockID,layerID,channel)
        blockPath=determineBlockPath(cp,blockID,channel)
        #THERE IS A PROBLEM WITH GPS START ARGUEMENT TO THE CODE
        #Parse all the options from BASE and Timeseries Config sections
        for sec in ['tracksearchbase']:
            self.add_ini_opts(cp,sec)
        #Readjust the channel name listed in tracksearch time
        #to the current one from from multichannel if
        #self.currenchannel!=''
        if (channel!=''):
            cp.set('tracksearchtime','channel_name',channel)
        for sec in ['tracksearchtime']:
            self.add_ini_opts(cp,sec)
        #Check the type of job this is and add in the injection options
        #if needed!
        if (self.jobType == self.validJobTypes[1]):
            for sec in ['tracksearchinjection']:
                self.add_ini_opts(cp,sec)
            self.injectFile=cp.get('tracksearchinjection','inject_file')
            self.add_opt('inject_file',os.path.basename(self.injectFile))
            self.add_condor_cmd('transfer_input_files',self.injectFile)
        self.add_condor_cmd('should_transfer_files','yes')
        self.add_condor_cmd('when_to_transfer_output','on_exit')
        #Read expected job sampling rate
        sampleRate=float(cp.get('tracksearchtime','sample_rate'))
        #Read expected TF overlapping percentage
        overlapPercentage=float(cp.get('layerconfig','layerOverlapPercent'))
        #Set each trials total_time_point
        setSize=int(cp.get('layerconfig','layer1SetSize'))
        timeScale=float(cp.get('layerconfig','layer1TimeScale'))
        totalTimePoints=int(sampleRate*setSize*timeScale)
        self.add_opt('total_time_points',str(totalTimePoints))
        #Set segment size for each TF map
        segmentTimePoints=int(timeScale*sampleRate)
        self.add_opt('segment_time_points',str(segmentTimePoints))
        #Set overlap size for each TF map
        overlap=int(math.floor(segmentTimePoints*overlapPercentage))
        self.add_opt('overlap',str(overlap))

        #Check for needed directories if not present make them!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs')

        #Set the condor submit initial directory option this
        #should point to the block,layer location for output files
        self.add_condor_cmd('initialdir',layerPath)
        #Set log
        channelName=string.strip(cp.get('tracksearchtime','channel_name'))
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchTime-'+channelName+'-$(macrogpsstartseconds)_$(cluster)_$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchTime-'+channelName+'-$(macrogpsstartseconds)_$(cluster)_$(process).err'))
        filename="/tracksearchTime--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
    #End init
#End Class

class tracksearchMapJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    The class running a MAP instance of the tracksearch code
    """
    def __init__(self,cp,block_id,layer_id,dagDir):
        self.dagDirectory=dagDir
        #ConfigParser object -> cp
        self.__executable = cp.get('condor','tracksearch')
        self.__universe = cp.get('condor','tracksearch_universe');
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        blockID=block_id
        layerID=layer_id
        ### WE NEED A CONDOR DAG VARIABLE TO THE INITIAL DIR ARGUMENT
        layerPath=determineLayerPath(cp,blockID,layerID)
        blockPath=determineBlockPath(cp,blockID)
        #Parse all the options from BASE and Timeseries Config sections
        for sec in ['tracksearchbase']:
            self.add_ini_opts(cp,sec)
        for sec in ['tracksearchmap']:
            self.add_ini_opts(cp,sec)
        #Check for needed directories if not present make them!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs/')
        #Set the condor submit initial directory option this
        #should point to the block,layer location for output files
        self.add_condor_cmd('initialdir','$(macroStartDir)')
        #Set log
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchMap-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchMap-$(cluster)-$(process).err'))
        filename="/tracksearchMap--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
    #End init
#End Class

class tracksearchAveragerJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    The class that defines how we will perform an averager job.  The averager
    jobs, are tasks that merge two TF maps into one.  This is performed on a
    set of maps.  The set is then a new image of reduced dimesions specified
    via input arguments
    """
    def __init__(self,cp,block_id,layer_id,dagDir,channel=''):
        self.dagDirectory=dagDir
    #Setup job options to take a cache of caches and build new maps
            #ConfigParser object -> cp
        self.__executable = cp.get('condor','averager')
        self.__universe = cp.get('condor','averager_universe');
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        blockID=block_id
        layerID=layer_id
        layerPath=determineLayerPath(cp,blockID,layerID,channel)
        blockPath=determineBlockPath(cp,blockID,channel)
        #Parse all the options from BASE and Timeseries Config sections
        for sec in ['averagerbase']:
            self.add_ini_opts(cp,sec)
        for sec in ['tracksearchmap']:
            self.add_ini_opts(cp,sec)
        #Check for needed directories if not present make them!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs/')
        #Set the condor submit initial directory option this
        #should point to the block,layer location for output files
        self.add_condor_cmd('initialdir','$(macroStartDir)')
        #Set log
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchAverager-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchAverager-$(cluster)-$(process).err'))
        filename="/tracksearchAverager--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
    #End init
#End Class

class tracksearchMapCacheBuildJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    This is the class definition which will drive the compiled python file name
    parsing code to create the layers of map building caches required
    to complete the pipeline
    """
    def __init__(self,cp,block_id,layer_id,dagDir,channel=''):
        self.dagDirectory=dagDir
    #Setup job options to take a cache of caches and build new maps
            #ConfigParser object -> cp
        self.__executable = cp.get('condor','cachebuilder')
        self.__universe = cp.get('condor','cachebuilder_universe');
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        blockID=block_id
        layerID=layer_id
        #WE NEED A DAG VARIABLE THE LAYERid FOR PROPER INITIAL DIR
        layerPath=determineLayerPath(cp,blockID,layerID,channel)
        blockPath=determineBlockPath(cp,blockID,channel)
        #From ini sections [averagerbase] and [layerconfig] we will determine
        # the proper arguments to give the mapCacheBuildJob
        layerconfigOpts=[]
        section='layerconfig'
        for opt in cp.options(section):
            arg=string.strip(cp.get(section,opt))
            if str(opt).__contains__(str('TimeScale').lower()):
                layerconfigOpts.append(arg)
        if layerconfigOpts.__len__() <= 1:
            sys.stderr.write("Error with section Layerconfig!\n")
        if layerconfigOpts.__len__() < layerID:
            sys.stderr.write("Error invoking mapBuilderObject: layerID problem!"+str(layerconfigOpts,layerID)+"\n")
        layerconfigOpts.sort()
        #Error condition layerID >= len(layerList)
        #Determine overlap conditions from [tracksearchbase] as percentage
        percentOverlap=float(string.strip(cp.get('layerconfig','layerOverlapPercent')))
        #Get the overlap of new cache files.
        layerSizeLabel='layer%sSetSize'%layerID
        layerTimeLabel='layer%sTimeScale'%layerID
        self.jobSetSize=cp.get('layerconfig',layerSizeLabel)
        self.expectedTotalDuration=cp.get('layerconfig','layerTopBlockSize')
        self.overlapTime=float(string.strip(cp.get('layerconfig',layerTimeLabel)))*percentOverlap
        self.mapTime=float(string.strip(cp.get('layerconfig',layerTimeLabel)))
#         #Work on all files listed
#         #OK THIS IS INCORRECT SETTING OF MACROS
#         #self.add_opt('file','$macroFile')
#         #self.add_opt('start_time','$macroStartTime')
#         #self.add_opt('map_set_duration','$macroMapSetDuration')
#         #self.add_opt('new_map_duration','$macroNewMapDuration')
#         #self.add_opt('overlap_maps','$macroOverlapMaps')
        #Parse all the options from BASE and Timeseries Config sections
        for sec in ['cachebuilder']:
            self.add_ini_opts(cp,sec)
        #Check for needed directories if not present make them!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs/')
        #Adjust initial dir so output ends up in proper place!
        self.add_condor_cmd('initialdir','$(macroStartDir)')
        #Add a condor macro variable for specialized value passing
        #Set log
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchCacheBuild-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchCacheBuild-$(cluster)-$(process).err'))
        filename="/tracksearchCacheBuild--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
    #End init

    def getJobTSAList(self):
        #Generate the theoretical list of TSA cache names to process based
        #on the configuration options given to the job.  This is then input
        #into the tracksearchMAP code to batch process new maps.
        outputList=[]
        A=float(self.expectedTotalDuration)
        B=float(self.mapTime)
        C=float(self.overlapTime)
        F=int(self.jobSetSize)
        jobCacheNumEstimate=int(math.ceil(((A-C)/(B-C))/F))
        if (jobCacheNumEstimate == 0):
            sys.stderr.write("Error check object initialization.")
            return outList
        for num in range(1,jobCacheNumEstimate+1):
            outputList.append('JobSet_'+str(num)+'.cacheTSA')
        return outputList
    #End getJobsetList

    def getJobsetList(self):
        #Generate the theoretical list of Jobset names to process based
        #on the configuration options given to the job
        outputList=[]
        A=float(self.expectedTotalDuration)
        B=float(self.mapTime)
        C=float(self.overlapTime)
        F=int(self.jobSetSize)
        jobCacheNumEstimate=int(math.ceil(((A-C)/(B-C))/F))
        if (jobCacheNumEstimate == 0):
            sys.stderr.write("Error check object initialization.\n")
            return outList
        for num in range(1,jobCacheNumEstimate+1):
            outputList.append('JobSet_'+str(num)+'.jobCache')
        return outputList
    #End getJobTSAList
#End Class

class tracksearchClusterJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    This calls is responsible for creating a generic cluster job.  The
    cluster jobs will concatenate all results file into a single list.
    The single lists will be saved in a special directory.
    """
    def __init__(self,cp,block_id,dagDir,channel=''):
        self.dagDirectory=dagDir
        self.__executable = cp.get('condor','clustertool')
        #HACK SETB pool the job runs on the scheduler
        self.__universe = cp.get('condor','clustertool_universe')
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        self.block_id=blockID=block_id
        layerID='RESULTS_'+str(blockID)
        #Do not use channel information for initial dir.
        #Set initial to RESULTS directory above CHANNEL dirs
        self.initialDir=layerPath=determineLayerPath(cp,blockID,layerID)
        blockPath=determineBlockPath(cp,blockID,channel)
        #Setup needed directories for this job to write in!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs')
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchCluster-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchCluster-$(cluster)-$(process).err'))
        filename="/tracksearchCluster--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
        #Load in the cluster configuration sections!
        #Add the candidateUtils.py equivalent library to dag for proper
        #execution!
        self.candUtil=str(cp.get('pylibraryfiles','pyutilfile'))
        self.add_condor_cmd('should_transfer_files','yes')
        self.add_condor_cmd('when_to_transfer_output','on_exit')
        self.add_condor_cmd('initialdir',self.initialDir)
        #If the job is to run in the scheduler universe we set the proper ENV
        #variables otherwise the submit file will transfer the py script.
        if self.__universe == 'scheduler':
            self.add_condor_cmd('environment','PYTHONPATH=$PYTHONPATH:'+os.path.abspath(os.path.dirname(self.candUtil)))
        else:
            self.add_condor_cmd('transfer_input_files',self.candUtil)
        if cp.has_section('clusterconfig'):
            for sec in ['clusterconfig']:
                self.add_ini_opts(cp,sec)
        #End __init__ method
#End class tracksearchClusterJob

class tracksearchThresholdJob(pipeline.CondorDAGJob, pipeline.AnalysisJob):
    """
    This class acts as a thresholding job.  Threshold can be done in the lalapps_tracksearch code
    but we put it here to allow arbitrary rethresholding after a run.  The user may wish to play
    with the trigger statistics.  This keeps the pipeline reruns to a minimum.
    """
    def __init__(self,cp,block_id,dagDir,channel=''):
        self.dagDirectory=dagDir
        self.__executable = cp.get('condor','clustertool')
        self.__universe= cp .get('condor','clustertool_universe')
        pipeline.CondorDAGJob.__init__(self,self.__universe,self.__executable)
        pipeline.AnalysisJob.__init__(self,cp)
        self.block_id=blockID=block_id
        layerID='RESULTS_'+str(blockID)
        #Do not set channel name information here.  This puts all
        #threshold output files into same place
        self.initialDir=layerPath=determineLayerPath(cp,blockID,layerID)
        blockPath=determineBlockPath(cp,blockID,channel)
        #Setup needed directories for this job to write in!
        buildDir(blockPath)
        buildDir(layerPath)
        buildDir(blockPath+'/logs')
        self.set_stdout_file(os.path.normpath(blockPath+'/logs/tracksearchThreshold-$(cluster)-$(process).out'))
        self.set_stderr_file(os.path.normpath(blockPath+'/logs/tracksearchThreshold-$(cluster)-$(process).err'))
        filename="/tracksearchThreshold--"+str(channel)+".sub"
        self.set_sub_file(os.path.normpath(self.dagDirectory+filename))
        #Load in the cluster configuration sections!
        #Add the candidateUtils.py equivalent library to dag for proper
        #execution!
        self.candUtil=str(cp.get('pylibraryfiles','pyutilfile'))
        self.add_condor_cmd('should_transfer_files','yes')
        self.add_condor_cmd('when_to_transfer_output','on_exit')
        self.add_condor_cmd('transfer_input_files',self.candUtil)
        self.add_condor_cmd('initialdir',self.initialDir)
        #Setp escaping possible quotes in threshold string!
        optionText=str('expression-threshold')
        if cp.has_option('candidatethreshold',optionText):
            newVal=val=cp.get('candidatethreshold',optionText)
            #Introduce proper shell escapes for submit file to work...
            if newVal.__contains__('"'):
                newVal=str(val).replace('"','\\"')
                cp.set('candidatethreshold',optionText,newVal)
        for sec in ['candidatethreshold']:
                self.add_ini_opts(cp,sec)
   #End __init__ method
#End tracksearchThresholdJob class

class tracksearchHousekeeperNode(pipeline.CondorDAGNode,pipeline.AnalysisNode):
    """
    The class acting as a generic template to do carry out the housekeeping job.
    NOTE: This should be in local universe in most cases please check
    the code in the corresponding job class above.
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
    #End init
#End Class
                                   
class tracksearchTimeNode(pipeline.CondorDAGNode,pipeline.AnalysisNode):
    """
    The class acting as a generic template to do a Time search
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
        pipeline.CondorDAGNode.set_retry(self,3)
    #End init
#End Class

class tracksearchMapNode(pipeline.CondorDAGNode,
                         pipeline.AnalysisNode):
    """
    The class acting as a generic template to do a MAP search
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
    #End init
#End Class

class tracksearchAveragerNode(pipeline.CondorDAGNode,
                         pipeline.AnalysisNode):
    """
    The class acting as a generic template to do a MAP merging from specified
    input map cache file to process
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
    #End init
#End Class      

class tracksearchMapCacheBuildNode(pipeline.CondorDAGNode,
                         pipeline.AnalysisNode):
    """
    The class acting as a generic template to do a MAP cache build.
    It managaes a compiled python script to process a dir list of files.
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
    #End init
#End Class

class tracksearchClusterNode(pipeline.CondorDAGNode,pipeline.AnalysisNode):
    """
    The class acting as a generic template to allow building a cluster job.
    It gives the needed features to setup a condor node to cluster/clobber
    the ascii results files from tracksearch C code.
    These jobs will always return success.  We will use /bin/true as a
    post script to ensure this.
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
        #pipeline.CondorDAGNode.set_post_script(self,'/bin/true')
        pipeline.CondorDAGNode.set_retry(self,3)
    #End init
#End Class

class tracksearchThresholdNode(pipeline.CondorDAGNode,pipeline.AnalysisNode):
    """
    The class acting as a generic template to allow building a threshold job.
    It gives the needed features to setup a condor node to cluster/clobber
    the ascii results files from tracksearch C code.
    """
    def __init__(self,job):
        #Expects to run CondorDAGJob object for tracksearch
        pipeline.CondorDAGNode.__init__(self,job)
        pipeline.AnalysisNode.__init__(self)
    #End init
#End Class
        
class tracksearch:
    """
    The class which wraps all X sized second blocks into a uniq DAG
    we expect an input of a Science Segment of exactly n seconds
    specified via the ini file
    """
    #TO HANDLE FUTURE COINCIDENT ANALYSIS WE NEED TO ADJUST DAGDIR FOR
    #EACH IFO THEN MAKE A CLUSTERING DIR OR SOMETHING LIKE THAT
    def __init__(self,cparser,injectFlag,logfile,scienceSegment=''):
        #For large DAG of may possible data Blocks track each call
        #to createJobs() which is called once per data Block for analysis
        self.sciSeg=''
        self.runStartTime=''
        self.blockID=''
        self.lastBlockID=''
        self.dagName=''
        self.cp=cparser
        self.resultPath=self.cp.get('filelayout','workpath')
        self.globalDagName='tracksearchGlobal'
        self.dagDirectory=''
        if (scienceSegment==''):
            self.dataSegmentCount=0
            self.dagName=self.globalDagName
            #Uses legacy getDagDirectory were blockID has not been assigned yet!
            self.dagDirectory=os.path.normpath(self.getDagDirectory()+'/DAGS/');
            self.dag = pipeline.CondorDAG(os.path.normpath(self.dagDirectory+'/'+logfile))
        else:
            self.dataSegmentCount=-1
            self.__initializeBlock__(scienceSegment)
            self.dag = pipeline.CondorDAG(os.path.normpath(self.dagDirectory+'/'+logfile))
            self.dagName='tracksearchDAG_'+str(self.sciSeg.start())+'_duration_'+str(self.sciSeg.dur())
        buildDir(self.dagDirectory)
        self.dagFilename=self.dagDirectory+'/'+self.dagName
        self.dag.set_dag_file(self.dagFilename)
        self.injectFlag=False
        self.injectFlag=injectFlag
        self.jobType=""
        #Injection relevant config
        if self.injectFlag == True:
            self.jobType='injection'
        else:
            self.jobType='normal'
        #Check to see if Config object contains a multichannel
        #section if so set the multichannel boolean to true and adjust
        #dag construction accordingly
        self.currentchannel=str('')
        self.channellist=list()
        self.have_multichannel=False
        self.sec_multichannel=sec_multichannel="multichannel"
        if self.cp.has_section(sec_multichannel):
            self.have_multichannel=True
            for channel in self.cp.options(sec_multichannel):
                self.channellist.append(str(self.cp.get(sec_multichannel,channel)).strip())
        else:
            sys.stdout.write("Setting up standard single channel tracksearch pipe.\n")
        #Variables that are common to all search layers
        self.percentOverlap=float(string.strip(self.cp.get('layerconfig','layerOverlapPercent')))
        self.layerTimeScaleInfo=[]
        tempLib=[]
        for opt in self.cp.options('layerconfig'):
            section='layerconfig'
            arg=string.strip(self.cp.get(section,opt))
            if str(opt).__contains__(str('TimeScale').lower()):
                entry=[]
                entry.append(str(opt))
                entry.append(str(arg))
                tempLib.append(entry)
        tempLib.sort()
        self.layerTimeScaleInfo=tempLib
        tempLib=[]
        self.layerSetSizeInfo=[]
        for opt in self.cp.options('layerconfig'):
            section='layerconfig'
            arg=string.strip(self.cp.get(section,opt))
            if str(opt).__contains__(str('SetSize').lower()):
                entry=[]
                entry.append(str(arg))
                tempLib.append(entry)
        tempLib.sort()
        self.layerSetSizeInfo=tempLib
        if self.layerTimeScaleInfo.__len__()!=self.layerSetSizeInfo.__len__():
            sys.stderr.write("ERROR with section [layerconfig]\n")
        self.layerCount=self.layerTimeScaleInfo.__len__()
        # Initialize the list of dataFind jobs df_job
        # Initialize the list of dataFind nodes also
        # Form in memory df_jobList=
        # list(
        #      tuple(dataFindInitialDir,
        #            dataFindLogPath,
        #            df_job,
        #            list(tuple(start,stop,df_node))))
        #
        self.dataFindJobList=list()
    #End Init

    def __initializeBlock__(self,scienceSegment):
        """
        Initialized the DAG in preparation for incoming block of data to analyze.
        This method should be called by only to other methods __init__().  When the DAG
        will contain multiple smaller dags.  Or the more effective LARGE dag spanning all
        data blocks feed into the method self.createjobs(dataBlock)
        """
        #Expects a fixed size PsuedoScience Segment
        #The dag for this run will be then run from this data
        self.dataSegmentCount=self.dataSegmentCount+1
        self.sciSeg=scienceSegment
        self.runStartTime=self.sciSeg.start()
        self.blockID=str(self.sciSeg.start())+':'+str(self.sciSeg.dur())
        self.lastBlockID='NONE'
        #This is Legacy Variable name.  It is the directory where each blocks submit files should go!
        self.dagDirectory=self.getDagDirectory()
    #End __initializeBlock__()
    
    def __buildSubCacheFile__(self,masterCacheFile,subCachePath,startTime,endTime):
        """
        This method streamlines the masterCacheFile into smaller cache files to
        streamline the access to data via a small cache files.  The small cache file
        will be named via times and written to subCachePath directory.  The file will
        contain all the gwfs which span the time interval requested.
        """
        fileContents=file(masterCacheFile).readlines()
        fileContents.sort()
        subCacheFile=os.path.normpath(subCachePath+'/Opt_'+str(startTime)+'_'+str(endTime)+'.cache')
        startIndex=0
        endIndex=fileContents.__len__()-1
        newList=[]
        for index in range(0,fileContents.__len__()):
            timeStamp=int(fileContents[index].split(' ')[2])
            timeOffset=int(fileContents[index].split(' ')[3])
            frameStart=timeStamp
            frameStop=timeStamp+timeOffset
            if ((frameStart >= startTime) and (frameStop <= endTime)):
                newList.append(fileContents[index])
            elif ((frameStart < startTime) and (frameStop > startTime)):
                newList.append(fileContents[index])
            elif ((frameStart < endTime) and (frameStop > endTime)):
                newList.append(fileContents[index])
        if newList.__len__() == 0:
            sys.stderr.write("ERROR! Inappropriate cache file conversion for "+str(startTime)+" to "+str(endTime)+"\n")
            sys.stderr.write("Are you using correct dummy cache file?\n")
            sys.stderr.write(str(masterCacheFile)+"\n")
            os.abort()
        newList.sort()
        listStart=int(newList[0].split(' ')[2])
        listEnd=int(newList[newList.__len__()-1].split(' ')[2])
        listEndOffset=int(newList[newList.__len__()-1].split(' ')[3])
        listEndAll=listEnd+listEndOffset
        outFile=file(subCacheFile,'w')
        outFile.writelines(newList)
        outFile.close()
        return subCacheFile
    # End __buildSubCacheFile__


    def getDagDirectory(self):
        """
        This function must set the dag directory variable.  It is controlled via the INI file.
        See the entry [filelayout],dagpath.  Ideally this should be a nonNFS directory.  Condor may flag
        error and refuse to run the dag otherwise.
        """
        try:
            dagDirectory=str(self.cp.get('filelayout','dagpath')+'/'+self.blockID+'/')
        except:
            sys.stdout.write("Didn't find the dagpath option in [filelayout], defaulting to old behaviour.\n")
            dagDirectory=str(self.resultPath+'/DAG_files/'+self.blockID+'/')
        dagDirectory=os.path.normpath(dagDirectory)
        return dagDirectory
    #end def
    def __getAssociatedDataFindNode__(self,dataFindInitialDir,dataFindLogPath,nodeStart,nodeEnd):
        """
        Method fetches pre-existing data find node to limit calls in
        multichannel pipeline if the datafindnode does not exists it
        creates it first and keeps a record of the datafindjob to
        create future data find nodes as needed.  It returns a
        datafind node to the calling method
        """
        #1 Check to see if we have a df_job related to input arguments
        #  If NOT create the df_job else select the existing df_job
        #2 Check to see if we have an existing df_node for this df_job
        #  If NOT use selected df_job and create df_node return this
        #  node
        #  IF YES select the df_node and return it to calling function
        # Do we have a dataFind job matching dataFindInitialDir,dataFindLogPath?
        haveDataFindJob=False
        indexJ=0
        indexN=0
        index=0
        haveDataFindNode=False
        newDataFindJobRecord=tuple()
        newDataFindNodeRecord=tuple()
        df_id=df_lp=df_job=df_jobNodeList=''
        for entry in self.dataFindJobList:
            df_id,df_lp,df_job,df_jobNodeList=entry
            if (dataFindInitialDir == df_id) and (dataFindLogPath == df_lp):
                haveDataFindJob=True
                currentDataFindJob=df_job
                currentDataFindNodeList=df_jobNodeList
                indexJ=index
            index=index+1
        #If not dataFindJob found
        if not(haveDataFindJob):
            indexJ=index
            #Create the needed dataFind job
            buildDir(dataFindLogPath)
            buildDir(dataFindInitialDir)
            dataFindSubFileDir=os.path.normpath(self.dagDirectory+'/dataFindJobs/')
            buildDir(dataFindSubFileDir)
            currentDataFindJob=pipeline.LSCDataFindJob(dataFindInitialDir,dataFindLogPath,self.cp)
            filename="/datafind--"+str(self.blockID)+".sub"
            currentDataFindJob.set_sub_file(os.path.normpath(dataFindSubFileDir+filename))
            currentDataFindJob.add_condor_cmd('initialdir',str(dataFindInitialDir))
            currentDataFindNodeList=list()
            #Save record of created df_job
            newDataFindJobRecord=dataFindInitialDir,dataFindLogPath,currentDataFindJob,currentDataFindNodeList
            self.dataFindJobList.append(newDataFindJobRecord)
            haveDataFindJob=True
        index=0
        #Check the list from the datafind job for associated datafind nodes
        currentDataFindJobRecord=self.dataFindJobList[indexJ]
        #If the currentDataFindJob has not associated nodes
        nodeFound=False
        dataFindNodeList=list(currentDataFindJobRecord[3])
        for entry in dataFindNodeList:
            if (nodeStart == entry.get_start()) and (nodeEnd == entry.get_end()):
                nodeFound=True
                associatedDataFindNode=entry
        if ((dataFindNodeList.__len__() == 0) or not(nodeFound)):
            #Create a new associated dataFindNode
            currentDataFindJob=currentDataFindJobRecord[2]
            newDataFindNode=pipeline.LSCDataFindNode(currentDataFindJob)
            newDataFindNode.set_start(nodeStart)
            newDataFindNode.set_end(nodeEnd)
            ifo=str(self.cp.get('datafind','observatory'))
            newDataFindNode.set_observatory(ifo)
            self.dataFindJobList[indexJ][3].append(newDataFindNode)
            associatedDataFindNode=newDataFindNode
            # Add node to the DAG
            self.dag.add_node(associatedDataFindNode)
        return associatedDataFindNode
    #End __getAssociatedDataFindNode__()
    
    def __startingSearchLayer__(self,layerID):
        """
        This layer is responsible for initial data find jobs and the
        tracksearch time jobs in a single layer search this would be
        the only layer to actually be used
        RETURNS the last node of this layer to satisify the
        parent child relationship to next execution layer
        layerID is expected to be a 1 Duh, first layer!
        print 'Preparing layer ',layerID,' of block ',self.blockID
        We need to make chunks of data from this sciSeg which are
        layer1SetSize long in time
        """
        mapTime=float(self.cp.get('layerconfig','layer1TimeScale'))
        overlapTime=mapTime*self.percentOverlap
        setSize=float(self.cp.get('layerconfig','layer1SetSize'))
        #If [tracksearchtime] has bin_buffer section.  Prepare blocks
        #with proper padding data
        padsize=determineDataPadding(self.cp)
        #Create the chunk list that we will makeing data find jobs for
        #If padding is requested implicity assume the data is available
        self.sciSeg.make_chunks((setSize*mapTime))
        if ((self.sciSeg.dur()>=self.sciSeg.unused()) and (self.sciSeg.__len__() <= 1)):
            sys.stderr.write("\n")
            sys.stderr.write("WARNING: Set Size and Map Time(s) seem inconsistent!\n")
            sys.stderr.write("Set Size :"+str(setSize)+"\n")
            sys.stderr.write("Map Time :"+str(mapTime)+"\n")
            sys.stderr.write("Pad Size :"+str(padsize)+"\n")
            sys.stderr.write("Pipeline may be incorrectly constructed! Check your inputs.\n")
            sys.stderr.write("\n")
        #Setup time marker for entire run
        runStartTime=self.runStartTime
        #What is name of dagDir location of dags and sub files
        dagDir=self.dagDirectory
        #Create dataFindJob
        #This path is relative to the jobs initialDir arguments!
        dataFindInitialDir=determineLayerPath(self.cp,self.blockID,"dataFind_Caches")
        dataFindLogPath=determineLayerPath(self.cp,self.blockID,"dataFind_Logs")

        #Setup the jobs after queries to LSCdataFind as equal children
        block_id=self.blockID
        layer_id=layerID
        #
        tracksearchTime_job=tracksearchTimeJob(self.cp,
                                               block_id,
                                               layer_id,
                                               dagDir,
                                               self.jobType,
                                               self.currentchannel)
        channel=self.currentchannel
        prevLayerJobList=[]        
        prev_df = None
        if not str(self.cp.get('condor','datafind')).lower().__contains__(str('LSCdataFind').lower()):
            sys.stdout.write("Assuming we do not need standard data find job! Hardwiring in cache file to pipeline!\n")
            sys.stdout.write("Looking for ini option: condor,datafind_fixcache\n")
            # Pop first chunk off list to keep from repeating jobs!
            # We do this because the for look has a repeat issue the first job
            # seems to be a repeat of 2nd analysis chunk.  The for loop catches these types
            # AnalysisSegment,
            #<ScienceSegment: id 1, start 731488412, end 731554412, dur 66000, unused 0>
            #<AnalysisChunk: start 731488412, end 731554412>
            #<AnalysisChunk: start 731488412, end 731488742>
            # Notice first analysis chunk has bounds of all data then second chunk bounds same
            # start but the proper length.  The quick hack is to pop(0) the list of chunks
            # as done above removing the first entry.  This must be unique to the way we create
            # evenly spaced jobs in terms of samples and start times.  This saves 1 job per pipeline.
            # Only works on datafind_fixcache jobs!!!! (Temp fix)
            self.sciSeg._ScienceSegment__chunks.pop(0)
        else:
            self.sciSeg._ScienceSegment__chunks.pop(0)
        if (self.sciSeg._ScienceSegment__chunks.__len__() < 1):
            sys.stdout.write("WARNING: Data to be analyzed not properly divided or absent!\n")
            sys.stdout.write("The input options must be WRONG!\n")
        for chunk in self.sciSeg:
            #Method that creates a dataFind job if needed and returns
            #appropriate new node entry if it doesn't exists for that
            #time or returns the node that is used for finding that
            #time interval
            nodeStart=chunk.start()-padsize
            nodeEnd=chunk.end()+padsize

            #Set the node job time markers from chunk! This adjustment must
            #reflect any buffering that might be specified on the command line.
            tracksearchTime_node=tracksearchTimeNode(tracksearchTime_job)
            #If executable name is anything but LSCdataFind we
            #assume that the cache file should be hard wired!
            if not str(self.cp.get('condor','datafind')).lower().__contains__(str('LSCdataFind').lower()):
                fixCache=self.cp.get('condor','datafind_fixcache')
                subCacheFile=self.__buildSubCacheFile__(fixCache,dataFindInitialDir,chunk.start(),chunk.end())
                tracksearchTime_node.add_var_opt('cachefile',subCacheFile)
            else:
                #Setup a traditional pipe with a real data finding job
                df_node=self.__getAssociatedDataFindNode__(dataFindInitialDir,dataFindLogPath,nodeStart,nodeEnd)
                tracksearchTime_node.add_parent(df_node)
                outputFileList=df_node.get_output_files()
                tracksearchTime_node.add_var_opt('cachefile',outputFileList[0])
            #Consider using Condor file transfer mechanism to
            #transfer the frame cache file also.
            #Originally the following was uncommented.
            tracksearchTime_node.add_var_opt('gpsstart_seconds',chunk.start())
            self.dag.add_node(tracksearchTime_node)
            prevLayerJobList.append(tracksearchTime_node)
        return prevLayerJobList

    def __finalSearchLayer__(self,layerID,nodeLinks):
        """
        This layer will setup last nodes of dag.  These nodes
        should perform a final map search.  No additional cache
        building needs to be done here
        RETURNS no node linkage
        Setup file globbing for each layer
        """
        channel=self.currentchannel
        tracksearchCluster_job=tracksearchClusterJob(self.cp,self.blockID,self.dagDirectory,channel)
        #Loop through done search layers to glob
        prevJobList=[]
        globformat="Glob::%s::%s::%s.candidates"
        clobformat="Clob::%s::%s::%s_%s.candidates"
        for i in range(1,layerID):
            tracksearchCluster_node=tracksearchClusterNode(tracksearchCluster_job)
            layer2work=determineLayerPath(self.cp,self.blockID,i,channel)+"/*.candidates"
            #globFilename="Glob::"+str(channel)+"::"+str(self.blockID)+"::"+str(i)+".candidates"
            globFilename=globformat%(str(channel),str(self.blockID),str(i))
            tracksearchCluster_node.add_var_opt('file',layer2work)
            tracksearchCluster_node.add_var_opt('outfile',globFilename)
            tracksearchCluster_node.add_var_arg("--glob")
            #Setup the parents
            for parents in nodeLinks:
                tracksearchCluster_node.add_parent(parents)
            self.dag.add_node(tracksearchCluster_node)
            #Record job information these jobs are parent of clobbers
            prevJobList.append(tracksearchCluster_node)
        #Setup the appropriate globbed list clobbering jobs
        tracksearchCluster_job2=tracksearchClusterJob(self.cp,self.blockID,self.dagDirectory,self.currentchannel)
        filename="/tracksearchClobber--"+str(channel)+".sub"
        tracksearchCluster_job2.set_sub_file(os.path.normpath(self.dagDirectory+filename))
        tracksearchCluster_job.add_condor_cmd('initialdir',tracksearchCluster_job.initialDir)
        nextJobList=[]
        for i in range(1,layerID-1):
            tracksearchCluster_node2=tracksearchClusterNode(tracksearchCluster_job2)
            DLP=tracksearchCluster_job2.initialDir
            file2clobber=globformat%(str(channel),str(self.blockID),str(i))
            clobberWith=globformat%(str(channel),str(self.blockID),str(i+1))
            clobFilename=clobformat%(str(channel),str(self.blockID),str(i),str(i+1))
            tracksearchCluster_node2.add_var_opt('file',file2clobber)
            tracksearchCluster_node2.add_var_opt('outfile',clobFilename)
            tracksearchCluster_node2.add_var_opt('clobber',clobberWith)
            for parents in prevJobList:
                tracksearchCluster_node2.add_parent(parents)
            self.dag.add_node(tracksearchCluster_node2)
            nextJobList.append(tracksearchCluster_node2)
        #Step that sets up the tracksearchHousekeeperJobs
        #If housekeeper section has action = NO don't setup the jobs
        if (self.cp.get('housekeeper','action').lower() == 'yes'):
            tracksearchHousekeeper_job=tracksearchHousekeeperJob(self.cp,self.blockID,self.dagDirectory,self.currentchannel)
            tracksearchHousekeeper_node=tracksearchHousekeeperNode(tracksearchHousekeeper_job)
            for parents in prevJobList:
                tracksearchHousekeeper_node.add_parent(parents)
            self.dag.add_node(tracksearchHousekeeper_node)
        #Only do setup the threshold jobs if the ini file has a threshold section!
        if self.cp.has_section('candidatethreshold'):
            tracksearchThreshold_job=tracksearchThresholdJob(self.cp,self.blockID,self.dagDirectory)
            DLP=tracksearchThreshold_job.initialDir
            tracksearchThreshold_node=tracksearchThresholdNode(tracksearchThreshold_job)
            tracksearchThreshold_node.add_var_opt('file',os.path.normpath(str(DLP+'/*.candidates')))
            if nextJobList!=[]:
                for parents in nextJobList:
                    tracksearchThreshold_node.add_parent(parents)
            else:
                for parents in prevJobList:
                    tracksearchThreshold_node.add_parent(parents)
            self.dag.add_node(tracksearchThreshold_node)
    #end def finalsearchlayer method

    def __intermediateSearchLayers__(self,layerID,nodeLinks):
        """
        This layer performs the required map cache building from
        previous ith layer.  It is these caches which are joined into
        new maps and placed as part of the i+1 layer and analyzed
        RETURNS node linkage to possibly perform another search layer
        """
        layerNum=layerID
        layerNumPrevious=layerID-1
        layer_id = layerNum 
        block_id=self.blockID
        dagDir=self.dagDirectory
        if (layerNumPrevious < 1):
            sys.stderr.write("Error calling the intermediate search layer method!\n")
        prevLayerJobList=nodeLinks
        # Setup each additonal individual layer
        # The cache build node list clear after first pass through loop
        if (layerNum < 2):
            prevLayerJobList=[]
        cacheBuild_job=tracksearchMapCacheBuildJob(self.cp,block_id,layerNum,dagDir,self.channel)
        cacheBuild_node=tracksearchMapCacheBuildNode(cacheBuild_job)
        #Add directory to process code expects file but behavior will
        #adjust to a directory listing if that is the case
        #Specify directory contain map files to setup
        #Should be previous layer already processed!
        cacheBuildMapDir=determineLayerPath(self.cp,block_id,layerNumPrevious,self.channel)
        cacheBuildWorkDir=determineLayerPath(self.cp,block_id,layerNum,self.channel)
        cacheBuild_node.add_macro('macroStartDir',cacheBuildWorkDir)
        cacheBuild_node.add_var_opt('file',cacheBuildMapDir)            
        #Set the time of this run to start preping caches for
        cacheBuild_node.add_var_opt('start_time',self.runStartTime)
        #Lookup the proper layer options
        layerMapNewDur=float(self.cp.get('layerconfig','layer'+str(layerNum)+'TimeScale'))
        layerMapSetSize=int(self.cp.get('layerconfig','layer'+str(layerNum)+'SetSize'))
        layerMapOverlap=float(self.percentOverlap*layerMapNewDur)
        cacheBuild_node.add_var_opt('job_set_size',layerMapSetSize)
        cacheBuild_node.add_var_opt('new_map_duration',layerMapNewDur)
        cacheBuild_node.add_var_opt('overlap_maps',layerMapOverlap)
        #Make this process the child of all frame analysis jobs
        for parentJob in prevLayerJobList:
            cacheBuild_node.add_parent(parentJob)
        self.dag.add_node(cacheBuild_node)
        
        # The merge map
        tracksearchAverager_job=tracksearchAveragerJob(self.cp,block_id,layerNum,dagDir,self.channel)
        jobSetList=cacheBuild_job.getJobsetList()
        jobTSAList=cacheBuild_job.getJobTSAList()
        #Var to store copies of these objects to get right parent relation
        averagerJobListing=[]
        #Loop over all theoretical jobsets for map making
        averagerWorkDir=determineLayerPath(self.cp,block_id,layerNum,self.channel)
        averagerTimeLayerBinCount=int(self.cp.get('tracksearchtime','number_of_time_bins'))
        for cacheSet in jobSetList:
            tracksearchAverager_node=tracksearchAveragerNode(tracksearchAverager_job)
            tracksearchAverager_node.add_var_opt('multi_cache',cacheSet)
            tracksearchAverager_node.add_macro('macroStartDir',averagerWorkDir)
            tracksearchAverager_node.add_var_opt('new_t_bins',averagerTimeLayerBinCount)
            tracksearchAverager_node.add_parent(cacheBuild_node)
            self.dag.add_node(tracksearchAverager_node)
            averagerJobListing.append(tracksearchAverager_node)
        #Run this new layer map analysis
        nextLayerJobList=[]
        #The results of this analysis should be stored in layerNum layer
        tracksearchMap_job=tracksearchMapJob(self.cp,block_id,layerNum,dagDir)
        #The entries in the JobSet file will be the input cache sets
        #for this layer of the Analysis
        for cacheSet in jobTSAList:
            tracksearchMap_node=tracksearchMapNode(tracksearchMap_job)
            tracksearchMap_node.add_var_opt('inject_map_cache',cacheSet)
            tracksearchMap_node.add_macro('macroStartDir',determineLayerPath(self.cp,block_id,layerNum,self.channel))
            for parent in averagerJobListing:
                tracksearchMap_node.add_parent(parent)
            self.dag.add_node(tracksearchMap_node)
            nextLayerJobList.append(tracksearchMap_node)
        prevLayerJobList=nextLayerJobList
        return nextLayerJobList
    #end def

    def __createSingleJob__(self,channelname=''):
        """
        Wrapper which should be called once per channel in the
        multichannel configuration section or just once if the only
        channel mentioned is in the tracksearchtime section.  This
        method should never be called without a specified channel name.
        """
        if ((channelname=='') and (self.have_multichannel)):
            sys.stderr.write("Improper function call to __createSingleJob__() method")
            os.abort()
        self.currentchannel=channelname
        layerID=1
        nodeLinkage=self.__startingSearchLayer__(layerID)
        layerCount=self.layerCount
        #set of numbers [i,j)
        for layerNum in range(2,layerCount+1):
            nodeLinkage=self.__intermediateSearchLayers__(layerNum,nodeLinkage)

        self.__finalSearchLayer__(layerCount+1,nodeLinkage)
    #End __createSingleJob___()
    
    def createJobs(self,scienceSegment=''):
        """
        This method will call all the needed jobs/node objects to create
        a coherent job dag
        CHECK TO MAKE SURE LAYERCONFIG IS VALID HAS ENTRIES
        We need a consistency check between above and [tracksearchbase]
        Need a new housekeeping method which will go to each step
        deleting the MAP:*.dat files to save disk space
        """
        stype=type('')
        if ((type(scienceSegment) != stype) and (self.dataSegmentCount<0)):
            sys.stderr.write("Error Inconsistent calling of tracksearch pipe methods!\n")
            return
        if ((self.dataSegmentCount>-1) and (type(scienceSegment) != stype)):
            self.__initializeBlock__(scienceSegment)
        #
        if self.have_multichannel:
            for currentchannel in self.channellist:
                #sys.stdout.write("Installing sub-pipe for channel:"+str(currentchannel)+"\n")
                self.__createSingleJob__(currentchannel)
        else:
            self.__createSingleJob__()
    #End createJobs

    def writePipelineDAG(self,dagname=''):
        """
        Method that writes out the completed DAG and job files inside
        the proper filespace hierarchy for running the tracksearch
        pipeline
        """
        self.dag.write_sub_files()
        self.dag.write_dag()
        #Read in the resulting text file and prepend the DOT
        #information writing the DAG back to disk.
        input_fp=open(self.dag.get_dag_file(),'r')
        contents=input_fp.readlines()
        #Determine dot file name as dagfilename.DOT
        [dotfilename,extension]=os.path.splitext(os.path.abspath(self.dag.get_dag_file()))
        dotfilename=dotfilename+'.dot'
        contents.insert(0,'DOT '+dotfilename+' UPDATE\n');
        input_fp.close()
        output_fp=open(self.dag.get_dag_file(),'w')
        output_fp.writelines(contents)
        output_fp.close()
    #End writeTracksearchTopBlockPipe
#End Class

 
