package com.taobao.common.tfs.MetaITest_2_2_3;

import org.junit.Test;
import org.junit.Ignore;

import junit.framework.Assert;

import com.taobao.common.tfs.tfsNameBaseCase;
import com.taobao.common.tfs.namemeta.FileMetaInfo;


public class tfsManager_09_lsFile extends tfsNameBaseCase 
{
	@Test
	public void test_01_lsFile_right_filePath()
	{
	   log.info( "test_01_lsFile_right_filePath" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   tfsManager.createFile(appId, userId, "/textlsFile");
	   metaInfo=tfsManager.lsFile(appId, userId, "/textlsFile");
	   Assert.assertNotNull(metaInfo);
	   log.info( "The fileName is"+metaInfo.getFileName());
	   log.info( "The pid is"+metaInfo.getPid());
	   log.info( "The id is"+metaInfo.getId());
	   log.info( "The length is"+metaInfo.getLength());
	   log.info( "*****************************************************" );
	   tfsManager.rmFile(appId, userId, "/textlsFile");
	}
	@Test
	public void test_02_lsFile_null_filePath()
	{
	   log.info( "test_02_lsFile_null_filePath" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   metaInfo=tfsManager.lsFile(appId, userId, null);
	   Assert.assertNull(metaInfo);
	}
	@Test
	public void test_03_lsFile_empty_filePath()
	{
	   log.info( "test_03_lsFile_empty_filePath" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   metaInfo=tfsManager.lsFile(appId, userId, "");
	   Assert.assertNull(metaInfo);
	}
	@Test
	public void test_04_lsFile_wrong_filePath_1()
	{
	   log.info( "test_04_lsFile_wrong_filePath_1" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   metaInfo=tfsManager.lsFile(appId, userId, "/");
	   Assert.assertNull(metaInfo);
	}
	@Test
	public void test_05_lsFile_wrong_filePath_2()
	{
	   log.info( "test_05_lsFile_wrong_filePath_2" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   tfsManager.createFile(appId, userId, "/textlsFile");
	   metaInfo=tfsManager.lsFile(appId, userId, "textlsFile");
	   Assert.assertNull(metaInfo);
	   tfsManager.rmFile(appId, userId, "/textlsFile");
	}
	@Test
	public void test_06_lsFile_wrong_filePath_3()
	{
	   log.info( "test_06_lsFile_wrong_filePath_3" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   tfsManager.createFile(appId, userId, "/textlsFile");
	   metaInfo=tfsManager.lsFile(appId, userId, "///textlsFile///");
	   Assert.assertNotNull(metaInfo);
	   log.info( "The fileName is"+metaInfo.getFileName());
	   log.info( "The pid is"+metaInfo.getPid());
	   log.info( "The id is"+metaInfo.getId());
	   log.info( "The length is"+metaInfo.getLength());
	   log.info( "*****************************************************" );
	   tfsManager.rmFile(appId, userId, "/textlsFile");
	}
	@Test
	public void test_07_lsFile_not_exist_filePath()
	{
	   log.info( "test_07_lsFile_not_exist_filePath" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   tfsManager.createFile(appId, userId, "/textlsFile");
	   metaInfo=tfsManager.lsFile(appId, userId, "/textlsFile/textlsFile");
	   Assert.assertNull(metaInfo);
	   tfsManager.rmFile(appId, userId, "/textlsFile");
	}
	@Test
	public void test_08_lsFile_complex_filePath()
	{
	   log.info( "test_08_lsFile_complex_filePath" );
	   FileMetaInfo metaInfo;
	   metaInfo=null;
	   int Ret;
	   int i;
	   for(i=1;i<=100;i++)
	   {
	       Ret=tfsManager.createFile(appId, userId, "/textlsFile"+i);
	       Assert.assertEquals("Create textlsFile"+i+" should be true", Ret,0);
	   }
	   
	   metaInfo=tfsManager.lsFile(appId, userId, "/textlsFile40");
	   Assert.assertNotNull(metaInfo);
	   log.info( "The fileName is"+metaInfo.getFileName());
	   log.info( "The pid is"+metaInfo.getPid());
	   log.info( "The id is"+metaInfo.getId());
	   log.info( "The length is"+metaInfo.getLength());
	   log.info( "*****************************************************" );
	   
	   for(i=1;i<=100;i++)
	   {
	       Ret=tfsManager.rmFile(appId, userId, "/textlsFile"+i);
	       Assert.assertEquals("Create textlsFile"+i+" should be true", Ret,0);
	   }
	}
}
