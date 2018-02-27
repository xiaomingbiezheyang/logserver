/*
*Copyright (C) 2017 The open  Project
*开机启动logserver，抓取内核和logcat消息，保存在/sdcard/qhlog/下
*/

#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <cutils/properties.h>
#include <cutils/log.h>

#define TAG "qhlogserver"  
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG,__VA_ARGS__)

#define DEBUG false
#define INDEX_FILE  "sdcard/qhlog/index"
#define BUF_SIZE 1024
#define DEFUALT_LOG_FILE_SIZE 5*1024*1024
#define LOOP_TIMES 10

bool logserver_started=false;
char log_file[256]={0};
char cur_log_path[256]={0};
char kernel_log_file[256]={0};
char cpu_log_file[256]={0};
int  LOG_FILE_SIZE=5*1024*1024;

bool logpthread_exit=false;

int logcat_pid=-1;
int busybox_smp_pid=-1;
int top_pid=-1;

/*文件是否存在*/
static int is_file_exist(const char *name)
{
    FILE*fp=NULL;
    char file[256];

    if(name==NULL)
    {
        return 0;
    }

    fp=fopen(name,"r");
    if(NULL==fp)
    {
        return 0;
    }
    fclose(fp);
    fp=NULL;
    
    return 1;
}

/*读取返回编号*/
int readindexfile(const char *path)
{
    FILE *stream;
    char list[5];
    int i, numread;
    int num;
    // 以文本方式打开文件
    if( (stream = fopen(path, "r" )) != NULL )  // 如果不存在，创建
    {
        numread = fread( list, 1, sizeof(list), stream );
        if(numread>0)
        {
            num=atoi(list);
        }
        else
        {
            num=0;
        }
        if(DEBUG)
            LOGD( "num of items read = %d\n", num );
        fclose( stream );
        return num;
    }

    return 0;

}

/*更新编号*/
int writeindexfile(const char *path)
{
    FILE *stream;
    char list[5];
    int numwritten;
    int num;

    num=readindexfile(path);
    num++;
    if(num>LOOP_TIMES)
    {
        num=0;
    }
    // 以文本方式打开文件
    if( (stream = fopen(path, "w+" )) != NULL )  // 如果不存在，创建
    {
        sprintf(list, "%d", num);
        numwritten = fwrite( list, 1, strlen(list), stream );
        if(DEBUG)
            LOGD( "Wrote %d items\n", num );
        fclose( stream );
        return num;
    }

    return -1;

}


//根据名字找进程PID
int getPidByName(const char* task_name)
{
    DIR *dir;
    struct dirent *ptr;
    FILE *fp;
    int flag=0;
    int pid=-1;
    char filepath[256];//cmdline文件的路径可
    char cur_task_name[256];//识别的命令行文本
    char buf[BUF_SIZE];
    dir = opendir("/proc"); //打开路径
    if (NULL != dir)
    {
        while ((ptr = readdir(dir)) != NULL) //循环读取路径下的每一个文件/文件夹
        {
            //如果读取到的是"."或者".."则跳过，读取到的不是文件夹名字也跳过
            if ((strcmp(ptr->d_name, ".") == 0) || (strcmp(ptr->d_name, "..") == 0))
                continue;
            if (DT_DIR != ptr->d_type)
                continue;

            sprintf(filepath, "/proc/%s/status", ptr->d_name);//生成要读取的文件的路径
            fp = fopen(filepath, "r");//打开文件
            if (NULL != fp)
            {
                if(fgets(buf,BUF_SIZE-1,fp)== NULL)
                {
                    fclose(fp);
                    continue;
                }
                sscanf(buf,"%*s %s",cur_task_name);

                //如果文件内容满足要求则打印路径的名字（即进程的PID）
                if (!strcmp(task_name, cur_task_name))
                {
                    if(DEBUG)
                        LOGD("PID:%s\n",ptr->d_name);
                    flag=1;
                    pid= atoi(ptr->d_name);
                }
                fclose(fp);
                if(flag==1)
                {
                    break;
                }
            }

        }
        closedir(dir);//关闭路径

        if(flag==1)
        {
            return pid;
        }
        else
        {
            return -1;
        }
    }

    return -1;
}

//获取文件大小
unsigned long get_file_size(const char *path)    
{    
    unsigned long filesize = -1;        
    struct stat statbuff;    
    if(stat(path, &statbuff) < 0){    
        return filesize;    
    }else{    
        filesize = statbuff.st_size;    
    }    
    return filesize;    
}

//开始抓log
void start_catch_log()
{
    char cmd[256]={0};
    int index=0;
    char index_file[256]={0};

    //log文件名
    sprintf(index_file, "%s/index",cur_log_path);
    index=readindexfile(index_file);
    LOGD("index_file=%s\n",index_file);
    
    sprintf(log_file, "%s/log_%d.log",cur_log_path,index);
    sprintf(kernel_log_file, "%s/kmsg_%d.log",cur_log_path,index);
    sprintf(cpu_log_file, "%s/cpu_%d.log",cur_log_path,index);

    //删除旧log
    if(DEBUG)
        LOGD("log_file=%s\n",log_file);
    sprintf(cmd, "rm %s", log_file);
    system(cmd);

    //开始抓logcat
    sprintf(cmd, "logqihancat -v time >%s &", log_file);
    system(cmd);
    logcat_pid=getPidByName("logqihancat");
   //删除旧log
   if(DEBUG)
        LOGD("kernel_log_file=%s\n",kernel_log_file);
    sprintf(cmd, "rm %s", kernel_log_file);
    system(cmd);

    //开始抓内核消息
    sprintf(cmd, "busybox-smp cat /proc/kmsg >%s &", kernel_log_file);
    system(cmd);
    busybox_smp_pid=getPidByName("busybox-smp");

    sprintf(cmd, "rm %s", cpu_log_file);
    system(cmd);
    sprintf(cmd, "top -m 10 -d 10 >%s &", cpu_log_file);
    system(cmd);
    top_pid=getPidByName("top");

    logserver_started=true;

    writeindexfile(index_file);
    
    if(DEBUG)
        LOGD ("start_catch_log++++!\n");
}

//停止log
void stop_catch_log()
{
    system("logqihancat -c");
    system("busybox-smp killall logqihancat");
    system("busybox-smp killall busybox-smp");
    system("busybox-smp killall top");

    logserver_started=false;
    if(DEBUG)
        LOGD ("stop_catch_log++++!\n");

}

//创建文件夹
void initdata()
{
    char cmd[256]={0};
    int  index=0;

    index=readindexfile(INDEX_FILE);
    system("mkdir /sdcard/qhlog");
    sprintf(cur_log_path,"/sdcard/qhlog/log_%d",index);

    if(DEBUG)
        LOGD("cur_log_path=%s\n",cur_log_path);

    sprintf(cmd,"rm -rf %s",cur_log_path);
    system(cmd);

    sprintf(cmd,"mkdir %s",cur_log_path);
    system(cmd);
    writeindexfile(INDEX_FILE);
    
    char name[PROPERTY_VALUE_MAX];
    property_get("persist.sys.logserver.size", name, "0");
    int size=atoi(name);
    if(size>0){
        LOG_FILE_SIZE=size; 
    }
    LOGD("cur_log_size=%d\n",LOG_FILE_SIZE);
}

void* catch_Log_thread(void *ptr)
{
    long file_size=0;
    long kmesg_file_size=0;
    long cpu_file_size=0;


    if(DEBUG)
        LOGD("catch_Log_thread start\n");

    start_catch_log();

    while(!logpthread_exit)
    {
       sleep(5);
       file_size=get_file_size(log_file);
       kmesg_file_size=get_file_size(kernel_log_file);
       cpu_file_size=get_file_size(cpu_log_file);
       if(DEBUG)
           LOGD("logfilesize=%ld kmesg_file_size=%ld\n",file_size,kmesg_file_size);

       if(file_size>=LOG_FILE_SIZE
           ||kmesg_file_size>=LOG_FILE_SIZE
           ||cpu_file_size>=LOG_FILE_SIZE)
       {
           stop_catch_log();
           sleep(5);
           start_catch_log();
       }
    }
    return NULL;
}

//获取退出标志
int isNeedlogServerExit(){
    char name[PROPERTY_VALUE_MAX];
    int flag=0;
    if(is_file_exist("/sdcard/qhlogserver.exit")){
        return 1;
    }
    property_get("persist.sys.logserver.run", name, "0");
    flag=atoi(name);
    if(flag==1){
        return 0; 
    }
    else{
        return 1; 
    }
}

//设置标志
int SetlogServerRun(int flag){
    char command[10]={0};
    snprintf(command, sizeof(command), "%d",flag);
    property_set("persist.sys.logserver.run", command);


    return 0;
}

int main()
{
    long file_size=0;
    long kmesg_file_size=0;
    
    LOGD("qhlogserver start");
    logpthread_exit=false;
    /*if(isNeedlogServerExit()){
        LOGD("qhlogserver exit by exit flag");
        return 1;
    }*/
    if(is_file_exist("/sdcard/qhlogserver.start")){
        SetlogServerRun(1);
    }
    
    initdata();

    //start_catch_log();

    //创建线程去抓log
    pthread_t id;
    int i,ret;
    ret=pthread_create(&id,NULL,catch_Log_thread,NULL);
    if(ret!=0){
        LOGD ("Create pthread error!\n");
    }

   //主线程一直循环，不退出
    for(;;)
    {
       sleep(2);
       system("sync");
       if(isNeedlogServerExit()==1)
       {
           LOGD ("chekout exit flag,goto exit\r\n");
           stop_catch_log();
           logpthread_exit=true;
           break;
       }

    }
    //stop_catch_log();
    //等待子线程退出
    pthread_join(id,NULL);
    LOGD("qhlogserver exit");
    return 0;
}