#pragma once

// Header file for parallelPersistence class - see parallelPersistence.cpp for descriptions
#include <map>
#include <vector>
#include "basePipe.hpp"
#include "simplexBase.hpp"
#include "utils.hpp"

class parallelPersistence : public basePipe {
  private:
	double maxEpsilon;
  public:
	int dim, threads;
    parallelPersistence();
    pipePacket runPipe(pipePacket inData);
    bool configPipe(std::map<std::string, std::string> configMap);
	void outputData(pipePacket);
};
