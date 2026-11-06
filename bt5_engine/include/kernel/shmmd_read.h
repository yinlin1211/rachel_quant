#ifndef SHMMD_READER_H
#define SHMMD_READER_H

#include "ShmmdStruct.h"

template <class DataField, class Lock = DefaultSync>
class ShmMdReader {
public:
	ShmMdReader() :shmStruct(new ShmStruct<DataField, Lock>) {}

    bool load(const std::string& shmPath){
        return shmStruct->open(shmPath);
    }

    int getIndex() const{
        return shmStruct->getIndex();
    }

    DataField* getDb(){
        return shmStruct->getDb();
    }

private:
	ShmStruct<DataField, Lock>* shmStruct;
};

#endif

