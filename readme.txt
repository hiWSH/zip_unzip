zip打包	zip	srcpath:要打包的文件或目录；
destfile: 打包后的zip文件路径，包括文件名；
password:压缩密码，为空时表示不用密码
{‘srcpath’:’’,’destfile’:’’,’password’:’’}	
成功时则为0000000000
失败时返回0100000004


zip解包	unzip	srcfile:要解包的zip文件；
destpath: 解压目录；
password:解压密码，为空时表示不用密码；

overwrite:是否覆盖标志,0不覆盖，其他覆盖；
{‘srcfile’:’’,’destpath’:’’,’password’:’’,’overwrite’:’’}
成功时则为0000000000
失败时返回0100000004
