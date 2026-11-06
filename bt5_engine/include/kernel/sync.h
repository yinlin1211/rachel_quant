#ifndef __SYNC_H
#define __SYNC_H

#include <pthread.h>
#include <unistd.h>
#include <iostream>

enum SyncBehaveType {
    null,
    normal,
    smart,
};

class NullSync {
  public:
    explicit NullSync(pthread_spinlock_t* plock) {}
    explicit NullSync(pthread_mutex_t* plock) {}
};
class SpinSync {
  public:
    // type: 0-null, 1-normal, 2-try_and_release
    explicit SpinSync(pthread_spinlock_t* plock, SyncBehaveType type = normal, int pshared = PTHREAD_PROCESS_SHARED)
        : _plock(plock), _type(type), _pshared(pshared) {
        if(_type == 1 && pthread_spin_lock(_plock) != 0) {
            std::cerr << "accquire lock failed" << std::endl;
        } else if(_type == 2) {
            int trytimes = 0;
            while(pthread_spin_trylock(_plock) != 0) {
                usleep(1);
                if(++trytimes > 10000) {
                    trytimes = 0;
                    pthread_spin_unlock(_plock);
                    pthread_spin_destroy(_plock);
                    pthread_spin_init(_plock, _pshared);
                }
            }
        } else {
            // NULL
        }
    }

    ~SpinSync() {
        if((_type == 1 || _type == 2) && pthread_spin_unlock(_plock) != 0) {
            std::cerr << "release lock failed" << std::endl;
        }
    }
  private:
    pthread_spinlock_t* _plock;
    SyncBehaveType _type;
    int _pshared;
};

class MutexSync {
  public:
    // type: 0-null, 1-normal, 2-try_and_release
    explicit MutexSync(pthread_mutex_t* plock, SyncBehaveType type = normal, int pshared = PTHREAD_PROCESS_SHARED)
        : _plock(plock), _type(type) {
        if(_type == 1 && pthread_mutex_lock(_plock) != 0) {
            std::cerr << "accquire lock failed" << std::endl;
        } else if(_type == 2) {
            int trytimes = 0;
            while(pthread_mutex_trylock(_plock) != 0) {
                usleep(100);
                if(++trytimes > 10000) {
                    trytimes = 0;
                    pthread_mutex_unlock(_plock);
                    pthread_mutex_destroy(_plock);

                    pthread_mutexattr_t mattr;
                    pthread_mutexattr_init(&mattr);
                    pthread_mutexattr_setpshared(&mattr, _pshared);

                    pthread_mutex_init(_plock, &mattr);
                }
            }
        } else {
            // NULL
        }
    }

    ~MutexSync() {
        if(pthread_mutex_unlock(_plock) != 0) {
            std::cerr << "release lock failed" << std::endl;
        }
    }
  private:
    pthread_mutex_t* _plock;
    SyncBehaveType _type;
    int _pshared;
};

typedef SpinSync DefaultSync;

#endif
