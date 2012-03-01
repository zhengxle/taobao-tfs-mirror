package com.taobao.common.tfs.itest;

import java.io.IOException;

import junit.framework.Assert;

import org.junit.Test;
import org.junit.Ignore;

import com.taobao.common.tfs.tfsNameBaseCase;

public class tfsManager_13_saveFile_data extends tfsNameBaseCase 
{
	@Test
    public  void  test_01_saveFile_byte() throws IOException
	{
		log.info( "test_01_saveFile_byte" );

        String tfsFile = "/test01SFB";
		boolean bRet = false;

		byte data[] = null;    
		data = getByte(resourcesPath+"100k.jpg");

		bRet = tfsManager.saveFile(appId, userId, data, tfsFile);
		Assert.assertTrue(bRet);
        
        bRet = tfsManager.rmFile(appId, userId, tfsFile);
        Assert.assertTrue(bRet);
	}

    @Test
    public  void  test_02_saveFile_byte_with_empty_data() throws IOException
    {
        log.info( "test_02_saveFile_byte_with_empty_data" );
        
        String fileName = "/test02SFBWED";
        boolean bRet = false;

        byte data[] = null;
        String buf = "";
        data = buf.getBytes();

        bRet = tfsManager.saveFile(appId, userId, data, fileName);
        Assert.assertTrue(bRet);
        
        bRet = tfsManager.rmFile(appId, userId, fileName);
        Assert.assertTrue(bRet);
    }

    @Test
    public  void  test_03_saveFile_byte_with_null_data() throws IOException
    {
        log.info( "test_03_saveFile_byte_with_null_data" );

        boolean bRet = false;

        byte data[] = null;

        bRet = tfsManager.saveFile(appId, userId, data, "/test03SFBWND");
        Assert.assertFalse(bRet);
    }

    @Test
    public  void  test_04_saveFile_byte_with_file_exist() throws IOException
    {
        log.info( "test_04_saveFile_byte_with_file_exist" );

        String tfsFile = "/test04SFBWFE";
        boolean bRet = false;

        byte data[] = null;
        data = getByte(resourcesPath+"100k.jpg");

        bRet = tfsManager.saveFile(appId, userId, data, tfsFile);
        Assert.assertTrue(bRet);

        bRet = tfsManager.saveFile(appId, userId, data, tfsFile);
        Assert.assertFalse(bRet);

        bRet = tfsManager.rmFile(appId, userId, tfsFile);
        Assert.assertTrue(bRet);
    }	

	@Test
    public  void  test_05_saveFile_byte_with_wrong_fileName_1() throws IOException
	{
		log.info( "test_05_saveFile_byte_with_wrong_fileName_1" );

		boolean bRet = false;

		byte data[] = null;
		data = getByte(resourcesPath+"100k.jpg");

		bRet = tfsManager.saveFile(appId, userId, data, "test05SFBWWTN");
		Assert.assertFalse(bRet);
	}
	
	@Test
    public  void  test_06_saveFile_byte_with_wrong_fileName_2() throws IOException
	{
		log.info( "test_06_saveFile_byte_with_wrong_fileName_2" );

		boolean bRet = false;

		byte data[] = null;
		data = getByte(resourcesPath+"100m.jpg");

		bRet = tfsManager.saveFile(appId, userId, data, "/");
		Assert.assertFalse(bRet);
	}
	
	@Test
    public  void  test_07_saveFile_byte_with_empty_fileName() throws IOException
	{
		log.info( "test_07_saveFile_byte_with_empty_fileName" );

		boolean bRet = false;

		byte data[] = null;
		data = getByte(resourcesPath+"100k.jpg");

		bRet = tfsManager.saveFile(appId, userId, data, "");
		Assert.assertFalse(bRet);
	}
    
    @Test
    public  void  test_08_saveFile_byte_with_null_fileName() throws IOException
    {
        log.info( "test_08_saveFile_byte_with_empty_fileName" );

        boolean bRet = false;

        byte data[] = null;
        data = getByte(resourcesPath+"100k.jpg");
        
        bRet = tfsManager.saveFile(appId, userId, data, null);
        Assert.assertFalse(bRet);
    }
}