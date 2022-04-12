#include<exception>
#include<fstream>
#include<iostream>
#include<memory>
#include<queue>
#include<string>
#include<utility>
#include<vector>
#include<fcntl.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>

using namespace std;
//--------------------------------------- Task.h ---------------------------------------
#ifndef _ABSTRACT_TASK_
#define _ABSTRACT_TASK_

class Task{
public: 
    Task() = default;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    virtual ~Task() = default;
    virtual void run() = 0;
};

#endif

//--------------------------------------- ThreadPool.h ---------------------------------------
#ifndef _THREAD_POOL_
#define _THREAD_POOL_

class ThreadPool{
public: 
    ThreadPool(const unsigned &pool_sz);
    ThreadPool(const ThreadPool&) = delete;
    ~ThreadPool();
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& addTask(unique_ptr<Task> task);
    void shutDown();
    size_t waitingQueueSize()const;
protected:
    static void* workThread(void* which_pool);
    void worker();
private:
    queue<unique_ptr<Task>> task_queue;
    bool shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

ThreadPool::ThreadPool(const unsigned &pool_sz):shutdown(false),mutex(PTHREAD_MUTEX_INITIALIZER),cond(PTHREAD_COND_INITIALIZER){
    //set detach
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED))
        throw runtime_error("set detach failed");
    if(pthread_attr_setscope(&attr,PTHREAD_SCOPE_SYSTEM)) 
        throw runtime_error("set scope failed");
    //create work threads
    for(unsigned i=0;i!=pool_sz;++i){
        pthread_t tid;
        if(pthread_create(&tid,&attr,&ThreadPool::workThread,(void*)this))
            throw runtime_error("create pthread failed");
    }
}

ThreadPool::~ThreadPool(){
    shutdown=true;
}

void* ThreadPool::workThread(void* which_pool){
    auto pool=static_cast<ThreadPool*>(which_pool);
    pool->worker();
    return nullptr;
}

void ThreadPool::worker(){
    while(true){
        //----------------- critical region begin ----------------------------
        pthread_mutex_lock(&mutex);
        while(task_queue.empty()){
            if(shutdown){
                pthread_mutex_unlock(&mutex);
                return;
            }
            pthread_cond_wait(&cond,&mutex);
        }
        unique_ptr<Task> task=std::move(task_queue.front());
        task_queue.pop();
        pthread_mutex_unlock(&mutex);
        //----------------- critical region end ------------------------------
        task->run();
    }
}

ThreadPool& ThreadPool::addTask(unique_ptr<Task> task){
    //----------------- critical region begin ----------------------------
    pthread_mutex_lock(&mutex);
    task_queue.push(std::move(task));
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    //----------------- critical region end ------------------------------
    return *this;
}

void ThreadPool::shutDown(){
    shutdown=true;
}

size_t ThreadPool::waitingQueueSize()const{
    return task_queue.size();
}

#endif

//--------------------------------------- main.cpp ---------------------------------------
constexpr unsigned long CHUNKS_SZ=4294967296; // 4 GB
constexpr unsigned CHUNK_SZ=4096; // 4 KB
constexpr unsigned CHUNK_MAX_NUM=1048576; // 4GB/4KB=1024*1024
//constexpr unsigned WAITING_QUEUE_MAX_SZ=1048576;

pthread_mutex_t flags_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t flags_cond=PTHREAD_COND_INITIALIZER;

bool flags[CHUNK_MAX_NUM];
pair<char*,unsigned> collector[CHUNK_MAX_NUM];
//unsigned waiting_queue_size=0;
char *chunks=nullptr;

class Encoder:public Task{
public:
    Encoder(char* _src,
            const unsigned &_chunk_id):src(_src),dst(chunks+(unsigned long)_chunk_id*2*CHUNK_SZ),chunk_id(_chunk_id){
        return;
    }
    void run() override {
        collector[chunk_id].first=dst;
        unsigned cnt=1;
        char pre_ch=src[0];
        int i=0,j=0;
        while(++i<collector[chunk_id].second){
            if(src[i]==pre_ch) ++cnt;
            else{
                if(pre_ch==EOF||cnt==EOF) throw runtime_error("EOF comes before the end of file");
                dst[j++]=pre_ch;
                dst[j++]=static_cast<unsigned char>(cnt);
                pre_ch=src[i];
                cnt=1;
            }
        }
        if(pre_ch!=EOF&&cnt!=0){
            dst[j++]=pre_ch;
            dst[j++]=static_cast<unsigned char>(cnt);
        }
        collector[chunk_id].second=j;
        //----------------- critical region begin ----------------------------
        pthread_mutex_lock(&flags_mutex);
        flags[chunk_id]=true;
        //--waiting_queue_size;
        pthread_cond_signal(&flags_cond);
        pthread_mutex_unlock(&flags_mutex);
        //----------------- critical region end ------------------------------
    }
private:
    char *src,*dst;
    unsigned chunk_id;
};


void parse_cmd(int argc, char* argv[],unsigned &worker_num,char** &ptr_infile) {
	for(int i=1;i<argc;++i){
        if(strcmp(argv[i],"-j")==0){
            worker_num=atoi(argv[i+1]);
            ptr_infile=argv+i+2;
            return;
        }
    }
    ptr_infile=argv+1;
}


int main(int argc, char* argv[]){
    try{
        // initialize
        if(!(chunks=(char*)malloc(CHUNKS_SZ))) 
            throw runtime_error("malloc chunks failed");
        unsigned worker_num=1;
        char** ptr_infile=nullptr;
        parse_cmd(argc,argv,worker_num,ptr_infile);    
        vector<int> fd_in_lst;
        vector<pair<char*,unsigned long>> mmap_src_lst;
        unsigned chunk_num=0;

        // dispatch works
        ThreadPool pool(worker_num);
        while(*ptr_infile!=nullptr){
            char* infile=*ptr_infile;
            int fd_in=open(infile,O_RDONLY);
            if(fd_in==-1) throw runtime_error("open infile failed");
            fd_in_lst.push_back(fd_in);
            
            struct stat stat_infile;
            stat(infile,&stat_infile);
            unsigned long file_sz=stat_infile.st_size;

            char* src_bg=nullptr;
            if(!(src_bg=(char*)mmap(nullptr,file_sz,PROT_READ,MAP_PRIVATE,fd_in,0))) 
                throw runtime_error("mmap input-file failed");
            mmap_src_lst.push_back({src_bg,file_sz});

            for(unsigned long start=0;start<file_sz;start+=CHUNK_SZ){
                flags[chunk_num]=false;
                unsigned len=(start+CHUNK_SZ>file_sz)?file_sz-start:CHUNK_SZ;
                collector[chunk_num]={nullptr,len};
                //----------------- critical region begin ----------------------------
                //pthread_mutex_lock(&flags_mutex);
                //while(waiting_queue_size>=WAITING_QUEUE_MAX_SZ) pthread_cond_wait(&flags_cond, &flags_mutex);
                //++waiting_queue_size;
                //pthread_mutex_unlock(&flags_mutex);
                //----------------- critical region end ------------------------------
                unique_ptr<Encoder> encoder(new Encoder(src_bg+start,chunk_num++));
                pool.addTask(std::move(encoder));
            }
            ++ptr_infile;
        }
        pool.shutDown();

        // merge chunks
        if(chunk_num!=0){
            //----------------- critical region begin ----------------------------
            pthread_mutex_lock(&flags_mutex);
            while(flags[0]==false) pthread_cond_wait(&flags_cond, &flags_mutex);
            flags[0]=false;
            pthread_mutex_unlock(&flags_mutex);
            //----------------- critical region end ------------------------------
            if(chunk_num==1){
                fwrite(collector[0].first,1,collector[0].second,stdout);
            }
            else{
                char* chunk=collector[0].first;
                unsigned len=collector[0].second;
                char pre_c=chunk[len-2];
                unsigned char pre_cnt=chunk[len-1];
                fwrite(chunk,1,len-2,stdout);
                for(unsigned i=1;i<chunk_num;++i){
                    //----------------- critical region begin ----------------------------
                    pthread_mutex_lock(&flags_mutex);
                    while(flags[i]==false) pthread_cond_wait(&flags_cond, &flags_mutex);
                    flags[i]=false;
                    pthread_mutex_unlock(&flags_mutex);
                    //----------------- critical region end ------------------------------
                    chunk=collector[i].first;
                    len=collector[i].second;
                    if(pre_c==chunk[0]){
                        unsigned char cnt=static_cast<unsigned>(pre_cnt)+static_cast<unsigned>(chunk[1]);
                        if(len>=4){
                            fwrite(&pre_c,1,1,stdout);
                            fwrite(&cnt,1,1,stdout);
                            fwrite(chunk+2,1,len-4,stdout);
                            pre_c=chunk[len-2];
                            pre_cnt=chunk[len-1];
                        }
                        else pre_cnt=cnt;// len==2
                    }
                    else{
                        fwrite(&pre_c,1,1,stdout);
                        fwrite(&pre_cnt,1,1,stdout);
                        if(len>2) fwrite(chunk,1,len-2,stdout);
                        pre_c=chunk[len-2];
                        pre_cnt=chunk[len-1];
                    }
                }
                fwrite(&pre_c,1,1,stdout);
                fwrite(&pre_cnt,1,1,stdout);
            }
        }
        for(auto &src_sz:mmap_src_lst) munmap(src_sz.first,src_sz.second);
        for(auto &fd_in:fd_in_lst) close(fd_in);
        free(chunks);
    }catch(runtime_error& err){
        cerr<<err.what()<<endl;
    }
}