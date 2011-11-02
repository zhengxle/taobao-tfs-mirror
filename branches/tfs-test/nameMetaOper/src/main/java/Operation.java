package com.taobao.common.tfs.nameMetaOper;

import java.util.concurrent.Executors; 

import java.util.ArrayList;
import java.util.Random;
import java.util.zip.CRC32;

import java.io.FileReader;
import java.io.BufferedReader;
import java.io.FileInputStream;

import org.apache.log4j.Logger;
import org.apache.log4j.Level;

import com.taobao.common.tfs.DefaultTfsManager;

class Operation implements Runnable {
  protected String operType;
  protected String inputFile;
  protected String outputFile;
  protected ArrayList<String> inputList = new ArrayList<String>();
  protected ArrayList<String> outputList = new ArrayList<String>();
  protected StatInfo statInfo = new StatInfo();
  protected static Logger log = Logger.getLogger("nameMetaOper");
  protected Random random = new Random();
  protected String dirNamePrefix = "dir_";
  protected int nameIndex = 0;
  protected String beginDirPath = "/dir_" + nameIndex;
  protected String currDirPath = beginDirPath;
  protected String currFilePath;
  protected long userId = 381;
  protected DefaultTfsManager tfsManager;

  long startTime = 0;
  long operTime = 0;

  void execute() {
  }

  Operation(long userId, DefaultTfsManager tfsManager) {
    if (userId != 0) {
      this.userId = userId;
    }
    this.tfsManager = tfsManager;
  }

  @Override
  public void run() {
    log.setLevel(Level.DEBUG);
    execute();
  }

  public static void readFromInputFile(String inputFile, ArrayList<String> inputList) {
    BufferedReader buffReader = null;
    try {
      buffReader = new BufferedReader(new FileReader(inputFile));
      String line = null;
      while ((line = buffReader.readLine()) != null) {
        inputList.add(line);
      }
    } catch (Exception e) {
        e.printStackTrace();
    } finally {
      try {
        buffReader.close();
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  }

  void addStatInfo(StatInfo statInfo, boolean ret, long operTime) {
    log.debug("operTime: " + operTime);
    operTime /= 1000000; // ms
    if (ret) {
      statInfo.succCount++;
      statInfo.succTime += operTime;
    }
    else {
      statInfo.failCount++;
      statInfo.failTime += operTime;
    }
    statInfo.totalCount++;
    statInfo.totalTime += operTime;
  }

  void doStat(StatInfo statInfo, StatInfo opStatInfo) {
    // Op statInfo
    opStatInfo.totalCount += statInfo.totalCount;
    opStatInfo.succCount += statInfo.succCount;
    opStatInfo.failCount += statInfo.failCount;
    opStatInfo.succTime += statInfo.succTime;
    opStatInfo.failTime += statInfo.failTime;
    opStatInfo.totalTime += statInfo.totalTime;
    statInfo.clear();

    if (opStatInfo.totalCount != 0) {
      opStatInfo.succRate = ((float)opStatInfo.succCount/(float)opStatInfo.totalCount) * 100;
    }
    if (opStatInfo.succTime != 0) {
      opStatInfo.succTPS = ((float)opStatInfo.succCount/((float)opStatInfo.succTime/1000));
    }
    if (opStatInfo.failTime != 0) {
      opStatInfo.failTPS = ((float)opStatInfo.failCount/((float)opStatInfo.failTime/1000));
    }
    if (opStatInfo.totalTime != 0) {
      opStatInfo.totalTPS = ((float)opStatInfo.totalCount/((float)opStatInfo.totalTime/1000));
    }
    log.debug("@ " + this.operType + " stat info, succCount: " + opStatInfo.succCount + ", failCount: " + opStatInfo.failCount
      + ", totalCount: " + opStatInfo.totalCount + ", succRate: " + opStatInfo.succRate + "%" + ", succTPS: " + opStatInfo.succTPS
      + ", failTPS: " + opStatInfo.failTPS + ", totalTPS: " + opStatInfo.totalTPS);
  }

  public void genNextDirPath() {
    nameIndex++;
    int randomVal = random.nextInt(100);
    // if path is too long, back to the top level
    if (currDirPath.length() > 240) {
      currDirPath = Path.join("/", dirNamePrefix + nameIndex);
      // still too long, change the dirNamePrefix
      if (currDirPath.length() > 240) {
        dirNamePrefix += "e_";
        nameIndex = 0;
        currDirPath = dirNamePrefix + nameIndex;
      }
    }
    else if (randomVal <= 50) { // width expand
      String parentDir;
      parentDir = Path.getParentDir(currDirPath);
      if (parentDir == null) {
        parentDir = "/";
      }
      for (int i = 0; i < randomVal % 5; i++) {
        parentDir = Path.getParentDir(parentDir);
        if (parentDir == null) {
          parentDir = "/";
        }
      }
      currDirPath = Path.join(parentDir, dirNamePrefix + nameIndex);
    }
    else {  // depth expand
      currDirPath = Path.join(currDirPath, dirNamePrefix + nameIndex); 
    }
  }

  protected int getCrc(String fileName) {                                                                                                                            
    try {
      FileInputStream input = new FileInputStream(fileName);        
      int readLength;
      byte[] data = new byte[102400];
      CRC32 crc = new CRC32();
      crc.reset();
      while ((readLength = input.read(data, 0, 102400)) > 0) {
        crc.update(data, 0, readLength);
      }
      input.close();

      return (int)crc.getValue();
    } 
    catch (Exception e) {
      e.printStackTrace();
    }
    return 0;
  }
}
