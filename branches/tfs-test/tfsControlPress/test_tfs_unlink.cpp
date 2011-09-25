/*
 * (C) 2007-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Version: $Id: test_tfs_unlink.cpp 155 2011-07-26 14:33:27Z mingyan.zc@taobao.com $
 *
 * Authors:
 *   mingyan.zc@taobao.com
 *      - initial release
 *
 */
#include "func.h"
#include "test_tfs_unlink.h"
#include "test_gfactory.h"

using namespace tbsys;

//////////////////////////TestTfsUnlink///////////////////////////////////

int TestTfsUnlink::_have_display_statis = 0;
int TestTfsUnlink::_count = 0;
bool TestTfsUnlink::_hasReadFilelist = false;
int TestTfsUnlink::_partSize = 0;
tbsys::CThreadMutex TestTfsUnlink::_lock;
VUINT32 TestTfsUnlink::_crcSet;
VSTRING TestTfsUnlink::_filenameSet;

int TestTfsUnlink::setUp()
{
  srand(time(NULL) + pthread_self());
  TestTfsCase::setUp();
  _currentFile = 0;
  char *filelist = (char *)CConfig::getCConfig().getString("tfsunlink", "filelist_name");
  _largeFlag  = CConfig::getCConfig().getInt("tfsunlink", "largeFlag");

  _loopFlag = CConfig::getCConfig().getInt("tfsunlink","loop_flag");
  TBSYS_LOG(DEBUG,"filelist = %s", filelist);
  if(filelist == NULL)
  {
    filelist = "./tfsseed_file_list.txt";
  }
  TestTfsUnlink::_lock.lock();
  /* get self's threadNo from global factory */
  _threadNo = TestGFactory::_threadNo++;
  //_session.init(( CConfig::getCConfig().getString("public","tfs_config_file")));
  if (TestTfsUnlink::_hasReadFilelist == false)
  {
    TestCommonUtils::readFilelist(filelist, _crcSet, _filenameSet);
    TestTfsUnlink::_hasReadFilelist = true;
  }
  TestTfsUnlink::_partSize = _crcSet.size() / TestGFactory::_threadCount;
  _lockFlag = 0;
  TestTfsUnlink::_lock.unlock();
  TestCommonUtils::getFilelist(_threadNo, _partSize, _crcSet, _crcSetPerThread,
    _filenameSet, _filenameSetPerThread);

  return 0;
}

int TestTfsUnlink::testUnlink()
{
  int ret = 0;
  TfsFileStat info;

  memset((char *)&info, 0x00, sizeof(TfsFileStat));
  _totalSize = 0;

  if(_filenameSetPerThread.size() == 0){
    TBSYS_LOG(INFO,"File name list is empty");
    return 0;
  }
  
  file_name_ =  const_cast<char*>(_filenameSetPerThread.at(_currentFile).c_str());
  const char *postfix = strlen(file_name_) > 18 ? (char *)(file_name_ + 18) : NULL;
  int64_t fileSize = 0;
#if defined(VER_132)
  ret = _tfsFile->unlink(file_name_, postfix);
#elif defined(VER_140)
  ret = _tfsFile->unlink(file_name_, postfix, fileSize);
#elif defined(VER_141)
  ret = _tfsFile->unlink(fileSize, file_name_, postfix);
#endif

  if (ret != 0)
  {
    TBSYS_LOG(ERROR, "Unlink failed:%d, fileSize:%d", ret, fileSize);
    return ret;
  }
#if 0
  int fd = 0;

  if (_largeFlag)
  {
    ret = _tfsFile->open((char *)file_name,(char *)postfix, T_STAT | T_LARGE);                                                                     
  } else {
    ret = _tfsFile->open((char *)file_name,(char *)postfix, T_STAT);
  }

  if (ret <= 0)
  {
    TBSYS_LOG(ERROR,"Open failed:%d",ret);
    return -1;
  } else {
    fd = ret;
  }

  sleep(2);
  ret = _tfsFile->fstat( fd, &info, FORCE_STAT );
  if(ret != 0)
  {
    TBSYS_LOG(ERROR,"Fstat failed:%d",ret);
    _tfsFile->close( fd );
    return ret;
  }
  _tfsFile->close( fd );

  if (info.flag_ == 0)
  {
    TBSYS_LOG(ERROR,"Unlink faild: delete falg = %d", info.flag_);
    return -1;
  }
#endif
  return ret;
}

int TestTfsUnlink::run()
{
  int iRet = 0;
  int64_t end_time = 0;
  int64_t start_time = 0;
  int fileNo = (int)_filenameSetPerThread.size();
 
  if ((int)_currentFile < fileNo)
  {
    iRet = testUnlink();
    if (iRet == 0)
    {
      TBSYS_LOG( INFO, "Unlink file SUCCESS." );
    }
    else
    {
      TBSYS_LOG( ERROR, "Unlink file ERROR!!!" );
    }
    end_time = CTimeUtil::getTime();
    TestGFactory::_statisForUnlink.addStatis(start_time, end_time, iRet, "unlinkFile");

    if (0 == iRet)
    {
      /* Edit the filename and crc */
      std::string record = file_name_;
      record += " 1";
      TBSYS_LOG(ERROR, "record: %s", record.c_str());
      /* Save it */
      _recordSet.insert(record);
    }
    ++_currentFile;
    if (_loopFlag && _currentFile == fileNo)
    {
      _currentFile = 0;
    }
  } 
  else
  {
   if ((_lockFlag == 0) && TestTfsUnlink::_lock.trylock())
   {
      _count ++;
      _lockFlag = 1;
      TestTfsUnlink::_lock.unlock(); 
   }
  }

  if(_count == TestGFactory::_threadCount)
  {
    TBSYS_LOG(INFO,"stop");
    TestGFactory::_tfsTest.stop();
  }

  return 0;
}

int TestTfsUnlink::saveFilename()
{
  FILE *fp = NULL;
  char *filelist = (char *)CConfig::getCConfig().getString("tfsunlink",
         "unlinkedlist_name");

  if (filelist == NULL)
  {
    filelist = "./tfsunlinked_file_list.txt";                                                                                                                
  }

  if((fp = fopen(filelist, "a")) == NULL)
  {
      TBSYS_LOG(ERROR, "open unlinked_file_list failed.");
      return -1;
  }
  std::set<std::string>::iterator it=_recordSet.begin();
  for(; it != _recordSet.end(); it++)
  {
    fprintf(fp, "%s\n", it->c_str());
  }

  fflush(fp);
  fclose(fp);

  return 0;
}

void TestTfsUnlink::tearDown()
{
  TBSYS_LOG(DEBUG,"tearDown");
  bool need_display = false;
 
  TestTfsUnlink::_lock.lock();
  if(TestTfsUnlink::_have_display_statis == 0){
    TestTfsUnlink::_have_display_statis += 1;
    need_display = true;
  }

  /* Save the record to file */
  int iRet = saveFilename();  
  if (iRet != 0)
  {
    TBSYS_LOG(ERROR,"It's failed to save file name list !!!");
  }
  TestTfsUnlink::_lock.unlock();
  
  if(need_display){
    TBSYS_LOG(INFO, "-------------------------------unlink statis-------------------------------------");
    TestGFactory::_statisForUnlink.displayStatis("unlink statis");
    TBSYS_LOG(INFO, "-------------------------------END----------------------------------------------");
  }
  TestTfsCase::tearDown();
}

