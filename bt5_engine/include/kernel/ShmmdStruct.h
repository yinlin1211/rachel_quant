#ifndef SHM_MD_STRUCT_H
#define SHM_MD_STRUCT_H

#include "sync.h"
//#include <unordered_map>
#include <string>
#include <string.h>
#include <vector>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <float.h>
#include <time.h>

template <class DataField, class Lock = NullSync>
struct ShmStruct {
    ShmStruct() : pshm(0), pindex(0), db(0) { }

    /////////////////////////////////////////////
    /// for writer side
    bool init(const std::string& shmPath, bool clearShm, size_t reserveSize) {
        int ret = -1;
        int flag;
        mode_t mod;
        if(clearShm) {
            ret = shm_unlink(shmPath.c_str());
            if(ret < 0 && errno != 2) {
                perror("shm_unlik");
                return false;
            }

            flag = O_RDWR | O_CREAT;
            mod = 00666;
        } else {
            flag = O_RDWR;
            mod = S_IRUSR;
        }

        int fd = shm_open(shmPath.c_str(), flag, mod);
        if(fd < 0) {
            std::cerr << "failed to open shm:" << shmPath << " " << fd << std::endl;
            perror("shm_open");
            return false;
        }
        size_t filesize = reserveSize  * (sizeof(DataField) + sizeof(struct timeval));

        if(clearShm) {
            ret = ftruncate(fd, filesize);
            if(ret < 0) {
                perror("ftruncate");
                std::cerr << "reserveSize:" << reserveSize << std::endl;
                std::cerr << "filesize:" << filesize << std::endl;
                return false;
            }
        }

        pshm = (char*)mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if(pshm == 0) {
            perror("mmap");
            return false;
        }

        plock = (pthread_spinlock_t*) pshm;
        pindex = (int*)(pshm + sizeof(pthread_spinlock_t));
        int* pcapacity = pindex + 1;
        db = (DataField*)(pindex + 2);
        ptv = (struct timeval*)(db + reserveSize);

        if(clearShm) {
            pthread_spin_init(plock, PTHREAD_PROCESS_SHARED);
            *(pindex) = -1;
            *pcapacity = reserveSize;
        }

        return true;
    }

    void pushData(std::vector<DataField*> dataVec, bool doCompare = false) {
        //std::cout << "vec pushing, compare:" << doCompare << std::endl;
        int updated = 0;
        int updateIndex = -1;
        int nextIndex;
        {
            Lock lock(plock);
            nextIndex = *pindex + 1;
            //for(std::vector<DataField*>::iterator it = dataVec.begin(); it != dataVec.end(); ++it) {
            for(auto it = dataVec.begin(); it != dataVec.end(); ++it) {
                DataField* data = *it;
/*                if(doCompare) {
                    int lastSnap = getLatestIndex(data->InstrumentID);
                    if(lastSnap != -1 && !isNewer(db + lastSnap, data)) {
                        continue;
                    }
                }*/
                updateIndex = nextIndex + updated;
                DataField* dest = db + updateIndex;
                *dest = *data;
                ++updated;
            }
            if(updated > 0) {
                __sync_add_and_fetch(pindex, updated); // sync for reader
            }
        }

        if(updated > 0) {
            gettimeofday(ptv + nextIndex, NULL);
            for(int i = 1; i < updated; ++i) {
                ptv[nextIndex + i] = ptv[nextIndex];
            }
        }
    }


    void pushData(DataField* data, bool doCompare = false) {
        //std::cout << "pushing, compare:" << doCompare << std::endl;
        int updateIndex = -1;
        {
            Lock lock(plock);
        /*    if(doCompare) {
                int lastSnap = getLatestIndex(data->InstrumentID);
                if(lastSnap != -1 && !isNewer(db + lastSnap, data)) {
                    return;
                }
            }*/

            updateIndex = *pindex + 1;
            DataField* dest = db + updateIndex;
            *dest = *data;
            __sync_add_and_fetch(pindex, 1); // sync for reader
            //latestMap[data->InstrumentID] = *pindex;
        }

        //clock_gettime(CLOCK_MONOTONIC, ptv + updateIndex);
        gettimeofday(ptv + updateIndex, NULL);
    }

    //////////////////////////////////////////////////
    /// for reader side
    bool open(const std::string& shmPath) {
        int fd = shm_open(shmPath.c_str(), O_RDONLY, S_IRUSR);
        if(fd < 0) {
            std::cerr << "failed to open shm:" << shmPath << std::endl;
            perror("shm_open");
            return false;
        }
        struct stat stat;
        fstat(fd, &stat);

        pshm = (char*)mmap(NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if(pshm == 0) {
            perror("mmap");
            return false;
        }
        plock = (pthread_spinlock_t*) pshm;
        pindex = (int*)(pshm + sizeof(pthread_spinlock_t));
        int capacity = *(pindex + 1);
        db = (DataField*)(pindex + 2);

        ptv = (struct timeval*)(db + capacity);

        return true;
    }

    int getLatestIndex(const char* inst) {
        const int backTrace = 500;
        int index = *pindex;
        bool equal;
        for(int i = index; i >= std::max(0, index - backTrace + 1); --i) {
            equal = strcmp(inst, db[i].InstrumentID) == 0;

            //if(strcmp(inst, db[i].InstrumentID) == 0) {
            if(equal) {
                return i;
            }
        }

        return -1;
    }

    DataField* getDb() const {
        return db;
    }
    int getIndex() const {
        return *pindex;
    }
    struct timeval* getTimeValBase() const {
        return ptv;
    }
  private:
/*    bool isNewer(DataField* orig, DataField* cur) {
        if(orig->UpdateTime[0] == 0) {
            return true; // orig lack UpdateTime
        }

        if(cur->UpdateTime[0] == '0' && orig->UpdateTime[0] == '2') {
            return true; // 24 pm passed
        }

        int r = strcmp(cur->UpdateTime, orig->UpdateTime);
        if(r > 0 ||
                (r == 0 && cur->UpdateMillisec > orig->UpdateMillisec)) {
            return true;
        } else {
            return false;
        }
    }
*/
    char* pshm;
    pthread_spinlock_t* volatile plock;
    int* volatile pindex;
    DataField* db;
    struct timeval* ptv;
};
#endif
