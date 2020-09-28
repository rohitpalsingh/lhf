#pragma once

#include <set>
#include <vector>
#include <memory>


// Header file for utils class - see utils.cpp for descriptions
struct simplexNode{
	unsigned index;
	
	std::set<unsigned> simplex;
	double weight = 0;

	simplexNode(){}
	simplexNode(std::set<unsigned> simp, double wt) : simplex(simp), weight(wt) {}
};

typedef std::shared_ptr<simplexNode> simplexNode_P;

struct cmpByWeight{
	bool operator()(simplexNode_P a, simplexNode_P b) const{
		if(a->weight == b->weight){ //If the simplices have the same weight, sort by reverse lexicographic order for fastPersistence
			auto itA = a->simplex.rbegin(), itB = b->simplex.rbegin();
			while(itA != a->simplex.rend()){
				if(*itA != *itB) return *itA > *itB;
				++itA; ++itB;
			}
			return false;
		} else{
			return a->weight < b->weight;
		}
	}
};


struct bettiBoundaryTableEntry{
	unsigned bettiDim;
	double birth;
	double death;
	std::set<unsigned> boundaryPoints;
}; 


class utils {
  private:
	std::string debug = "0";
	std::string outputFile = "console";
  
  public:
	utils();
	utils(std::string, std::string);
	double computeMaxRadius(int, std::vector<std::vector<double>>&, std::vector<std::vector<double>>&, std::vector<unsigned>&);
	double computeAvgRadius(int, std::vector<std::vector<double>>&, std::vector<std::vector<double>>&, std::vector<unsigned>&);
	std::pair<std::vector<std::vector<unsigned>>, std::vector<std::vector<std::vector<double>>>> separatePartitions(int, std::vector<std::vector<double>>, std::vector<unsigned>);
	std::vector<std::vector<std::vector<double>>> separateBoundaryPartitions(std::vector<std::set<unsigned>>, std::vector<std::vector<double>>, std::vector<unsigned>);
	std::pair<std::vector<std::vector<unsigned>>, std::vector<std::vector<std::vector<double>>>> separatePartitions(double, std::vector<std::vector<double>>, std::vector<std::vector<double>>, std::vector<unsigned>);
	// void extractBoundaryPoints(std::vector<bettiBoundaryTableEntry>&);
	std::set<unsigned> extractBoundaryPoints(std::vector<simplexNode_P>);
	std::vector<bettiBoundaryTableEntry> mapPartitionIndexing(std::vector<unsigned>, std::vector<bettiBoundaryTableEntry>);
	void print2DVector(const std::vector<std::vector<unsigned>>&);
	void print1DVector(const std::vector<unsigned>&);
	void print1DVector(const std::set<unsigned>&);
	void print1DVector(const std::vector<double>&);
	std::vector<double> feature_distance(std::vector<double>*, std::vector<double>*);
	double vectors_distance(const double&, const double&);
	double vectors_distance(const std::vector<double>&, const std::vector<double>&);
	void print1DSet(const std::pair<std::set<unsigned>, double>&);	
	std::set<unsigned> setXOR(std::set<unsigned>&, std::set<unsigned>&);
	std::set<unsigned> setIntersect(std::set<unsigned>, std::set<unsigned>, bool isSorted);
	std::vector<unsigned> setIntersect(std::vector<unsigned>, std::vector<unsigned>, bool);
	std::vector<std::set<unsigned>> getSubsets(std::set<unsigned>, int);
	std::vector<unsigned> symmetricDiff(std::vector<unsigned>, std::vector<unsigned>, bool);
	std::set<unsigned> symmetricDiff(std::set<unsigned>, std::set<unsigned>, bool);
	std::vector<unsigned> setUnion(std::vector<unsigned>, std::vector<unsigned>, bool);
	std::set<unsigned> setUnion(std::set<unsigned>, std::set<unsigned>);
	std::pair<std::vector<unsigned>, std::vector<unsigned>> intersect(std::vector<unsigned>, std::vector<unsigned>, bool);
	
	//Utility functions for writing to console/debug file
	void writeLog(std::string module, std::string message);
	void writeDebug(std::string module, std::string message);
	void writeError(std::string module, std::string error){writeLog(module,error);return;};
	void writeFile(std::string fullMessage);
	
	static bool sortBySecond(const std::pair<std::set<unsigned>, double> &, const std::pair<std::set<unsigned>, double> &);
	std::vector<std::set<unsigned>> getSubsets(std::set<unsigned> set);
	std::vector<std::vector<unsigned>> getSubsets(std::vector<unsigned> set);
	
	
	std::vector<double> serialize(std::vector<std::vector<double>>& );
	std::vector<std::vector<double>> deserialize(std::vector<double> , unsigned);
	
	std::vector<double> nearestNeighbors(std::vector<double>&, std::vector<std::vector<double>>&);
};
