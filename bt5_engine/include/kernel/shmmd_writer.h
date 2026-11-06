#ifndef SHMMD_WRITER_H
#define SHMMD_WRITER_H

#include "ShmmdStruct.h"

template <class DataField, class Lock = DefaultSync>
class ShmMdWriter {
public:
	ShmMdWriter() :shmStruct(new ShmStruct<DataField, Lock>) {}

	bool init(const std::string& shmPath, bool clearShm, size_t reserveSize = 240 * 10000 * 2) {
	    return shmStruct->init(shmPath, clearShm, reserveSize);
	}

	void pushData(DataField* data, bool doCompare = false) {
		return shmStruct->pushData(data, doCompare);
	}
	void pushData(std::vector<DataField*> dataVec, bool doCompare = false) {
		return shmStruct->pushData(dataVec, doCompare);
	}

private:
	ShmStruct<DataField, Lock>* shmStruct;
};

#endif

