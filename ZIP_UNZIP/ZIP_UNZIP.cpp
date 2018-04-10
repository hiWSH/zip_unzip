// ZIP_UNZIP.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "ZIP_UNZIP.h"
#include <string>
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <winnt.h>
#include <winnls.h>
#include <winbase.h>
#include "cJSON.h"
#include "zip.h"
#include "unzip.h"
#include <Strsafe.h> 
#include <map>
using namespace std;
#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 唯一的应用程序对象

CWinApp theApp;

using namespace std;

#define  ERROR   "0100000004"
#define  SUCCESS  "0000000000"
#define  INVALID_FILE "0000000001"
typedef  unsigned long DWORD;
#define  LEN_1024  1024
#define  ASSERT_PTR(jason,ret) \
	strcpy((ret),ERROR);\
	if((jason) == nullptr)\
	{\
	return (ret);\
	}\

/*
aim:便于以后可以根据分割符扩展压缩文件数量
author:wsh
time:20180403
input:
	string strTime:需要分割的字符串
output:
	vector<string>:分割后保存的vector容器
*/
static std::vector<WCHAR*> split(WCHAR* strTime)  
{  
	std::vector<WCHAR*> result;   
	int pos = 0;//字符串结束标记，方便将最后一个单词入vector  
	size_t i;
	for( i = 0; i < wcslen(strTime); i++)  
	{  
		if(strTime[i] == '|')  
		{  
			WCHAR* temp = new WCHAR[i-pos+1];
			memset(temp,0x00,sizeof(WCHAR));
			wcsncpy(temp,strTime+pos,i-pos);
			temp[i-pos] = 0x00;
			result.push_back(temp); 
			pos=i+1;
		}  
	}  
	//判断最后一个
	if (pos < i)
	{
		WCHAR* temp = new WCHAR[i-pos+1];
		memset(temp,0x00,sizeof(WCHAR));
		wcsncpy(temp,strTime+pos,i-pos);
		temp[i-pos] = 0x00;
		result.push_back(temp);
	}
	return result;  
} 

static errno_t getFullName(WCHAR* path,WCHAR* fullName)
{
	//得到文件名
	WCHAR path_buffer[_MAX_PATH];
	WCHAR drive[_MAX_DRIVE];
	WCHAR dir[_MAX_DIR];
	WCHAR fname[_MAX_FNAME];
	WCHAR ext[_MAX_EXT];
	memset(fullName,0x00,sizeof(fullName));
	errno_t ret = _wsplitpath_s(path,drive,dir,fname,ext);
	wsprintf(fullName,L"%s%s",fname,ext);
	return ret;
}
static int getLastFolderName(WCHAR* path,WCHAR* folderName)
{
	int lenPath = wcslen(path);
	for (;lenPath;lenPath--)
	{
		if (path[lenPath-1] == '\\')
		{
			break;
		}
	}
	wsprintf(folderName,L"%s",path+lenPath);
	return 0;
}

//传入要遍历的文件夹路径，并遍历相应文件夹 得到文件树的叶子节点 
void TraverseDirectory(wchar_t Dir[MAX_PATH],std::map<wchar_t*,int> &m)      
{  
	WIN32_FIND_DATA FindFileData;  
	HANDLE hFind=INVALID_HANDLE_VALUE;  
	wchar_t DirSpec[MAX_PATH];                  //定义要遍历的文件夹的目录  
	DWORD dwError;  
	StringCchCopy(DirSpec,MAX_PATH,Dir);  
	StringCchCat(DirSpec,MAX_PATH,TEXT("\\*"));   //定义要遍历的文件夹的完整路径\*  
	bool bIsEmptyFolder = true;
	hFind=FindFirstFile(DirSpec,&FindFileData);          //找到文件夹中的第一个文件  

	if(hFind==INVALID_HANDLE_VALUE)                               //如果hFind句柄创建失败，输出错误信息  
	{  
		FindClose(hFind); 
		wchar_t* path = new wchar_t[MAX_PATH];
		memset(path,0x00,sizeof(path));
		wsprintf(path,L"%s",Dir);
		m.insert(make_pair(path,true)); //文件夹属性
		return;    
	}  
	else   
	{   
		while(FindNextFile(hFind,&FindFileData)!=0)                            //当文件或者文件夹存在时  
		{  
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0&&wcscmp(FindFileData.cFileName,L".")==0||wcscmp(FindFileData.cFileName,L"..")==0)        //判断是文件夹&&表示为"."||表示为"."  
			{  
				continue;  
			}  
			wchar_t DirAdd[MAX_PATH];  
			StringCchCopy(DirAdd,MAX_PATH,Dir);  
			StringCchCat(DirAdd,MAX_PATH,TEXT("\\"));  
			StringCchCat(DirAdd,MAX_PATH,FindFileData.cFileName); 
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0)      //判断如果是文件夹  
			{  
				      //拼接得到此文件夹的完整路径  
				bIsEmptyFolder = false;
				TraverseDirectory(DirAdd,m);                                  //实现递归调用  
			}  
			if((FindFileData.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)==0)    //如果不是文件夹  
			{  
				//wcout<<Dir<<"\\"<<FindFileData.cFileName<<endl;            //输出完整路径  
				bIsEmptyFolder = false;
				wchar_t* path = new wchar_t[MAX_PATH];
				memset(path,0x00,sizeof(path));
				wsprintf(path,L"%s",DirAdd);
				m.insert(make_pair(path,false)); //文件属性
			}  
		}  
		if (bIsEmptyFolder)
		{
			wchar_t* path = new wchar_t[MAX_PATH];
			memset(path,0x00,sizeof(path));
			wsprintf(path,L"%s",Dir);
			m.insert(make_pair(path,TRUE)); //文件夹属性
		}
		FindClose(hFind);  
	}  
} 
const char* zip(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	WCHAR sSrcpath[LEN_1024 * 2] = {};
	WCHAR sDestFile[LEN_1024 * 2] = {};
	char sPassword[LEN_1024] = {};
	if (!root)
	{
		return ret;
	}
	cJSON * item = cJSON_GetObjectItem(root, "srcpath");
	memset(sSrcpath,0x00,sizeof(sSrcpath));
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,sSrcpath,sizeof(sSrcpath)/sizeof(sSrcpath[0]));
	item = cJSON_GetObjectItem(root, "destfile");
	memset(sDestFile,0x00,sizeof(sDestFile));
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,sDestFile,sizeof(sDestFile)/sizeof(sDestFile[0]));
	item = cJSON_GetObjectItem(root, "password");
	sprintf_s(sPassword,"%s",item->valuestring);
    char* pPassword = 0 == strlen(sPassword) || 0 == strcmp(sPassword,"")?NULL:sPassword;
	HZIP hz;
	hz = CreateZip(sDestFile,pPassword);
	bool bExcept = false;
	std::vector<WCHAR*> vSrcCol = split(sSrcpath);
	for (std::vector<WCHAR*>::const_iterator itr=vSrcCol.cbegin();itr!=vSrcCol.cend();itr++)
	{
		DWORD dwFtyp = GetFileAttributes(*itr);
		if (INVALID_FILE_ATTRIBUTES == dwFtyp)
		{
			bExcept = true;
			break;
		}
		else if(dwFtyp & FILE_ATTRIBUTE_DIRECTORY)
		{
			map<wchar_t* ,int> mPath;
			TraverseDirectory(*itr,mPath);
			for (map<wchar_t*,int>::const_iterator mcitr = mPath.cbegin();mcitr!=mPath.cend();mcitr++)
			{
				if (0 == wcscmp(mcitr->first,*itr))continue;
				ZRESULT zr;
				//增加文件
				wchar_t wcAddPath[MAX_PATH] = {};
				memset(wcAddPath,0x00,sizeof(wcAddPath));
				wsprintf(wcAddPath,L"%s",mcitr->first+wcslen(*itr)+1);
				//cout<<*(mcitr->first+wcslen(*itr)+1)<<endl;
				if (mcitr->second == false)  zr= ZipAdd(hz,wcAddPath,mcitr->first);
				//增加文件夹
				if (mcitr->second == true)   zr = ZipAddFolder(hz,wcAddPath);
				if (ZR_OK != zr)
				{
					bExcept = true;
					break;
				}
			}
			mPath.erase(mPath.begin(),mPath.end());
			if (bExcept)break;
		}
		else
		{
			WCHAR fullname[MAX_PATH] = {};
			getFullName(*itr,fullname);
			ZRESULT zr = ZipAdd(hz,fullname,*itr);//hz:zip句柄；第三个参数文本文件位置，第二个参数在压缩包中的相对位置
			if (ZR_OK != zr)
			{
				bExcept = true;
				break;
			}
		}
	}
	if (!bExcept)
	{
		strcpy(ret,SUCCESS);
	}
	CloseZip(hz);
	vSrcCol.erase(vSrcCol.begin(),vSrcCol.end());
	return ret;
}

const char* unzip(const char* jason)
{
	char* ret = new char[LEN_1024];
	ASSERT_PTR(jason,ret);
	cJSON * root = cJSON_Parse(jason);
	WCHAR sSrcfile[LEN_1024] = {};
	WCHAR sDestpath[LEN_1024] = {};
	char sPassword[LEN_1024] = {};
	char sOverwrite[LEN_1024] = {};
	if (!root)
	{
		return ret;
	}
	cJSON * item = cJSON_GetObjectItem(root, "srcfile");
	memset(sSrcfile,0x00,sizeof(sSrcfile));
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,sSrcfile,sizeof(sSrcfile)/sizeof(sSrcfile[0]));
	item = cJSON_GetObjectItem(root, "destpath");
	memset(sDestpath,0x00,sizeof(sDestpath));
	MultiByteToWideChar(CP_ACP,0,item->valuestring,strlen(item->valuestring)+1,sDestpath,sizeof(sDestpath)/sizeof(sDestpath[0]));
	item = cJSON_GetObjectItem(root, "password");
	sprintf_s(sPassword, "%s", item->valuestring); 
	item = cJSON_GetObjectItem(root, "overwrite");
	sprintf_s(sOverwrite, "%s", item->valuestring); 
	char* pPassword = 0 == strlen(sPassword) || 0 == strcmp(sPassword,"")?NULL:sPassword;
	//如果不覆盖需要判断文件夹是否已存在，存在则返回ERROR
	if (strcmp(sOverwrite,"0") == 0)
	{
		WIN32_FIND_DATA FindFileData;
		HANDLE hFind;
		hFind = FindFirstFile(sDestpath, &FindFileData);
		if (hFind != INVALID_HANDLE_VALUE) 
		{
			CloseHandle(hFind);
			return ret;
		}
	}
	HZIP hz;
	hz = OpenZip(sSrcfile,pPassword);
	SetUnzipBaseDir(hz,sDestpath);
	ZIPENTRY ze; GetZipItem(hz,-1,&ze); int numitems=ze.index;
	for (int zi=0; zi<numitems; zi++)
	{
		GetZipItem(hz,zi,&ze);
		ZRESULT zt = UnzipItem(hz,zi,ze.name);
	}
	CloseZip(hz);
	strcpy(ret,SUCCESS);
	return ret;
}


int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// 初始化 MFC 并在失败时显示错误
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			// TODO: 更改错误代码以符合您的需要
			_tprintf(_T("错误: MFC 初始化失败\n"));
			nRetCode = 1;
		}
		else
		{
			// TODO: 在此处为应用程序的行为编写代码。
			zip("{\"srcpath\":\"D:\\\\job\\\\greatwall\\\\test\\\\gwi\",\"destfile\":\"D:\\\\job\\\\greatwall\\\\test1\\\\gwi4.zip\",\"password\":\"123456\"}");
			unzip("{\"srcfile\":\"D:\\\\job\\\\greatwall\\\\test1\\\\gwi4.zip\",\"destpath\":\"D:\\\\job\\\\greatwall\\\\test2\",\"password\":\"123456\",\"overwrite\":\"\"}");
		}
	}
	else
	{
		// TODO: 更改错误代码以符合您的需要
		_tprintf(_T("错误: GetModuleHandle 失败\n"));
		nRetCode = 1;
	}

	return nRetCode;
}
