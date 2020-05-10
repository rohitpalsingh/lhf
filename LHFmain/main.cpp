	#include "mpi.h"
	#include <iostream>
	#include <vector>
	#include <algorithm>
	#include <typeinfo>
	#include <thread>
	#include "readInput.hpp"
	#include "argParser.hpp"
	#include "basePipe.hpp"
	#include "writeOutput.hpp"
	#include "pipePacket.hpp"
	#include "preprocessor.hpp"
	#include "utils.hpp"

	using namespace std;

	int nprocs,id;
	 
	void runPipeline(std::map<std::string, std::string> args, pipePacket* wD){
		auto *ws = new writeOutput();

		// Begin processing parts of the pipeline
		// DataInput -> A -> B -> ... -> DataOutput
		// Parsed by "." -> i.e. A.B.C.D
		auto pipe = args.find("pipeline");
		if(pipe != args.end()){
			auto pipeFuncts = std::string(args["pipeline"]);
			auto lim = count(pipeFuncts.begin(), pipeFuncts.end(), '.') + 1;
			
			//For each '.' separated pipeline function (count of '.' + 1 -> lim)
			for(unsigned i = 0; i < lim; i++){
				auto curFunct = pipeFuncts.substr(0,pipeFuncts.find('.'));
				pipeFuncts = pipeFuncts.substr(pipeFuncts.find('.') + 1);
				
				//Build the pipe component, configure and run
				auto *bp = new basePipe();
				auto *cp = bp->newPipe(curFunct, args["complexType"]);
				
				delete bp;
				
				//Check if the pipe was created and configure
				if(cp != 0 && cp->configPipe(args)){
					//Run the pipe function (wrapper)
					*wD = cp->runPipeWrapper(*wD);
				} else {
					std::cout << cp << std::endl;
					cout << "LHF runPipeline: Failed to configure pipeline: " << args["pipeline"] << endl;
				}
				
				delete cp;
			}
		}
		//If the pipeline was undefined...
		else {
			cout << "LHF runPipeline: Failed to find a suitable pipeline, exiting..." << endl;
			return;
		}
		
		//Output the data using writeOutput library
		pipe = args.find("outputFile");
		if(pipe != args.end()){
			if (args["outputFile"] == "console"){
				//ws->writeConsole(wD);
			} else {
				ws->writeStats(wD->stats, args["outputFile"]);
				ws->writeBarcodes(wD->bettiTable, args["outputFile"]);
				
			}
		}
		
		return;
	}



	void processDataWrapper(std::map<std::string, std::string> args, pipePacket* wD){
		
		//Start with the preprocessing function, if enabled
		auto pre = args["preprocessor"];
		if(pre != ""){
			auto *preprocess = new preprocessor();
			auto *prePipe = preprocess->newPreprocessor(pre);
			
			if(prePipe != 0 && prePipe->configPreprocessor(args)){
				*wD = prePipe->runPreprocessorWrapper(*wD);
			} else {
				cout << "LHF processData: Failed to configure pipeline: " << args["pipeline"] << endl;
			}
		}
		
		runPipeline(args, wD);
			
		return;
	}	

	void processReducedWrapper(std::map<std::string, std::string> args, pipePacket* wD){
		
		auto *ws = new writeOutput();
		std::vector<bettiBoundaryTableEntry> mergedBettiTable;
		
		//Start with the preprocessing function, if enabled
		auto pre = args["preprocessor"];
		if(pre != ""){
			auto *preprocess = new preprocessor();
			auto *prePipe = preprocess->newPreprocessor(pre);
			
			if(prePipe != 0 && prePipe->configPreprocessor(args)){
				*wD = prePipe->runPreprocessorWrapper(*wD);
			} else {
				cout << "LHF processReduced: Failed to configure pipeline: " << args["pipeline"] << endl;
			}
		}
		
		utils ut;
		
		//Separate our partitions for distribution
		auto maxRadius = ut.computeMaxRadius(std::atoi(args["clusters"].c_str()), wD->originalData, wD->fullData, wD->originalLabels);
		auto avgRadius = ut.computeAvgRadius(std::atoi(args["clusters"].c_str()), wD->originalData, wD->fullData, wD->originalLabels);
			
		std::cout << "Using maxRadius: " << maxRadius << "\tavgRadius: " << avgRadius<< std::endl;
		std:vector<unsigned> binCounts;
		for(unsigned a = 0; a < std::atoi(args["clusters"].c_str()); a++){
			binCounts.push_back(std::count(wD->originalLabels.begin(), wD->originalLabels.end(), a));
		}
		std::cout << "Bin Counts: ";
		ut.print1DVector(binCounts);
		
		auto centroids = wD->originalData;
		
		auto partitionedData = ut.separatePartitions(avgRadius, wD->originalData, wD->fullData, wD->originalLabels);
		
		std::cout << "Partitions: " << partitionedData.second.size() << std::endl << "Counts: ";
		
		
		std::vector<unsigned> partitionsize;
		//Get the partition sizes
		for(auto a: partitionedData.second)
			 partitionsize.push_back(a.size());

		int partitionsize_size = partitionsize.size();
		ut.print1DVector(partitionsize);
		
		for(unsigned z = 0; z < partitionedData.second.size(); z++){
			if(partitionedData.second[z].size() > 0){
				std::cout << "Running Pipeline with : " << partitionedData.second[z].size() << " vectors" << std::endl;
				wD->originalData = partitionedData.second[z];
				
				
				runPipeline(args, wD);
				
				wD->complex->clear();
				
				//Map partitions back to original point indexing
				//ut.mapPartitionIndexing(partitionedData.first[z], wD->bettiTable);
				
				for(auto betEntry : wD->bettiTable){
					if(betEntry.boundaryPoints.size() > 0 && *(betEntry.boundaryPoints.begin()) < binCounts[z])
						mergedBettiTable.push_back(betEntry);
					
				}
				
			} else 
				std::cout << "skipping" << std::endl;
		}
		
		std::cout << "Full Data: " << centroids.size() << std::endl;
		if(centroids.size() > 0){
			std::cout << "Running Pipeline with : " << centroids.size() << " vectors" << std::endl;
			wD->originalData = centroids;
			runPipeline(args, wD);
			
			wD->complex->clear();
		} else 
			std::cout << "skipping" << std::endl;
		
		std::cout << std::endl << "_______BETTIS_______" << std::endl;
		
		for(auto a : wD->bettiTable){
			std::cout << a.bettiDim << ",\t" << a.birth << ",\t" << a.death << ",\t";
			ut.print1DVector(a.boundaryPoints);
		}
    
		//Output the data using writeOutput library
		auto pipe = args.find("outputFile");
		if(pipe != args.end()){
			if (args["outputFile"] == "console"){
				//ws->writeConsole(wD);
			} else {
				ws->writeStats(wD->stats, args["outputFile"]);
				ws->writeBarcodes(wD->bettiTable, args["outputFile"]);
				
			}
		}
		return;
	}	





	void processUpscaleWrapper(std::map<std::string, std::string> args, pipePacket* wD){
		
		//Start with the preprocessing function, if enabled
		auto *ws = new writeOutput();
		std::vector<bettiBoundaryTableEntry> mergedBettiTable;
		utils ut;
		
		// check if 1 process only
		  if(nprocs==1)
		  {
			 //Read our input data
			auto *rs = new readInput();
			wD->originalData = rs->readCSV(args["inputFile"]);
			wD->fullData = wD->originalData;
			
			processReducedWrapper(args,wD);
			 
			return ;
			
			  }
		//Check if we are the master process for upscaling   
		if(id == 0){
			std::vector<bettiBoundaryTableEntry> mergedMasterBettiTable;
			
			//Read our input data
			auto *rs = new readInput();
			wD->originalData = rs->readCSV(args["inputFile"]);
			wD->fullData = wD->originalData;

			//Partition the data with the configured preprocessor
			auto pre = args["preprocessor"];
			if(pre != ""){
				auto *preprocess = new preprocessor();
				auto *prePipe = preprocess->newPreprocessor(pre);
        
				if(prePipe != 0 && prePipe->configPreprocessor(args)){
					*wD = prePipe->runPreprocessorWrapper(*wD);
				} else {
					cout << "LHF : Failed to configure pipeline: " << args["pipeline"] << endl;
				}
			}
			
			//Separate our partitions for distribution
			auto maxRadius = ut.computeMaxRadius(std::atoi(args["clusters"].c_str()), wD->originalData, wD->fullData, wD->originalLabels);
			auto avgRadius = ut.computeAvgRadius(std::atoi(args["clusters"].c_str()), wD->originalData, wD->fullData, wD->originalLabels);
			auto centroids = wD->originalData;
			auto partitionedData = ut.separatePartitions(2*avgRadius, wD->originalData, wD->fullData, wD->originalLabels);
			
			std::cout << "Using maxRadius: " << maxRadius << "\tavgRadius: " << avgRadius<< std::endl;
			//	Each node/slave will process at least 1 partition
			//		NOTE: the partition may contain points outside partition that are within 2*Rmax
			int minPartitions = partitionedData.second.size() / (nprocs-1);
			int firstk = partitionedData.second.size() - (minPartitions*(nprocs-1));
					
			int dimension = partitionedData.second[0][0].size();
			std::vector<unsigned> partitionsize;
			
			//Get the partition sizes
			for(auto a: partitionedData.second)
				 partitionsize.push_back(a.size());

			int partitionsize_size = partitionsize.size();
			ut.print1DVector(partitionsize);
			
			
			MPI_Request req;
			int part_size;
			//Sending the data dimensions to slaves	; Asynchronous, send all data at once
			for(int i=1;i<nprocs;i++){			
				MPI_Isend(&dimension,1,MPI_INT,i,1,MPI_COMM_WORLD,&req);				//Data dimension
				MPI_Isend(&partitionsize_size,1,MPI_INT,i,1,MPI_COMM_WORLD,&req);		//Partition size array size
				MPI_Isend(&partitionsize[0],partitionsize_size,MPI_UNSIGNED,i,1,MPI_COMM_WORLD,&req); //Partition size array
				
				//For first k partitions, use minPartitions + 1 partitions
				if(i<=firstk){
					part_size = minPartitions +1;
					MPI_Isend(&part_size,1,MPI_INT,i,1,MPI_COMM_WORLD,&req);			
				}
				//For remaining n-k partitions, use minPartitions
				else
					MPI_Isend(&minPartitions,1,MPI_INT,i,1,MPI_COMM_WORLD,&req);			
			}
			std::vector<std::vector<std::vector<double>>> part(nprocs-1);
			
			//Sending Partitions and labels to slaves
			for(int k =1; k < nprocs; k++){
				if(k<=firstk)
     				part[k-1].resize(minPartitions+1);
				else
					part[k-1].resize(minPartitions);
				int p=0;
				for(int i=k-1; i<partitionsize_size; i += (nprocs-1)){
					part[k-1][p].resize(partitionedData.second[i].size()*dimension);
					part[k-1][p] = ut.serialize(partitionedData.second[i]);
					MPI_Isend(&part[k-1][p][0], part[k-1][p].size(), MPI_DOUBLE, k, 1, MPI_COMM_WORLD,&req);		//Partition
					MPI_Isend(&partitionedData.first[i][0],partitionedData.first[i].size(),MPI_UNSIGNED,k,1,MPI_COMM_WORLD,&req);	//Labels
	        		p++;					
     			}
			}
			
			//Run the centroid replaced data set through master while other processes execute on partitions
			std::cout << "Full Data: " << centroids.size() << std::endl;
			if(centroids.size() > 0){
				std::cout << "Running Pipeline with : " << centroids.size() << " vectors" << std::endl;
				wD->originalData = centroids;
				runPipeline(args, wD);
			
				wD->complex->clear();
			} else 
				std::cout << "skipping" << std::endl;
			
			//Retrieve entries into the merged betti table
			//	Note: Need to remove entries where the lowest index point is greater than partition size
			//			(This also requires sorting of the partitions by [ inPartition, outOfPartition ] )
			
			std::vector<std::vector<unsigned>> betti_dim(nprocs-1);
			std::vector<std::vector<double>> betti_birth(nprocs-1);
			std::vector<std::vector<double>> betti_death(nprocs-1);
			std::vector<std::vector<unsigned>> betti_boundarysize(nprocs-1);
			std::vector<std::vector<unsigned>> betti_boundaries(nprocs-1);
			
			std::vector<int> sizeOfBettiTable(nprocs-1);
			std::vector<int> bound_size(nprocs-1);
		
			MPI_Request reqr1[nprocs-1];
			MPI_Request reqr6[nprocs-1];
			
			std::vector<unsigned> ck1(nprocs-1);
			for(unsigned i=0;i<nprocs-1;i++)
				ck1[i]=i;
			
			std::vector<unsigned> ck2(nprocs-1);
			for(unsigned i=0;i<nprocs-1;i++)
				ck2[i]=i;
				
			
			for(int k=1; k < nprocs; k++){
				MPI_Irecv(&sizeOfBettiTable[k-1],1,MPI_INT,k,0,MPI_COMM_WORLD,&reqr1[k-1]);
				MPI_Irecv(&bound_size[k-1],1,MPI_INT,k,5,MPI_COMM_WORLD,&reqr6[k-1]);
			}
	
		MPI_Request reqr2[nprocs-1];
		MPI_Request reqr3[nprocs-1];
		MPI_Request reqr4[nprocs-1];
		MPI_Request reqr5[nprocs-1];
		MPI_Request reqr7[nprocs-1];
		
		for(int k=1; k < nprocs; k++){
            int flag=0;
			int received_slave;
			while(ck1.size()>0){
				for(auto c : ck1){
					MPI_Test(&reqr1[c],&flag,MPI_SUCCESS);
					if(flag!=0){
						received_slave = c;
						ck1.erase(std::remove(ck1.begin(), ck1.end(), c), ck1.end());
						break;
					}					
			    }
				if(flag!=0)
					break;
			}
		std::cout << "Receiving data from slaves..."<<received_slave+1<< std::endl;
    		
			betti_dim[received_slave].resize(sizeOfBettiTable[received_slave]);
			betti_birth[received_slave].resize(sizeOfBettiTable[received_slave]);
			betti_death[received_slave].resize(sizeOfBettiTable[received_slave]);
			betti_boundarysize[received_slave].resize(sizeOfBettiTable[received_slave]);
			MPI_Irecv(&betti_dim[received_slave][0],sizeOfBettiTable[received_slave],MPI_UNSIGNED,received_slave+1,1,MPI_COMM_WORLD,&reqr2[received_slave]);
			MPI_Irecv(&betti_birth[received_slave][0],sizeOfBettiTable[received_slave],MPI_DOUBLE,received_slave+1,2,MPI_COMM_WORLD,&reqr3[received_slave]);
			MPI_Irecv(&betti_death[received_slave][0],sizeOfBettiTable[received_slave],MPI_DOUBLE,received_slave+1,3,MPI_COMM_WORLD,&reqr4[received_slave]);
			MPI_Irecv(&betti_boundarysize[received_slave][0],sizeOfBettiTable[received_slave],MPI_INT,received_slave+1,4,MPI_COMM_WORLD,&reqr5[received_slave]);
	    	int flag1=0;
			int received_slave1;
			
			while(ck2.size()>0){
				for(auto c : ck2){
					MPI_Test(&reqr6[c],&flag1,MPI_SUCCESS);
					if(flag1!=0){
						ck2.erase(std::remove(ck2.begin(), ck2.end(), c), ck2.end());
						received_slave1 = c;
						break;
					}					
			    }
				if(flag1!=0)
					break;
			}
			betti_boundaries[received_slave1].resize(bound_size[received_slave1]);
			MPI_Irecv(&betti_boundaries[received_slave1][0],bound_size[received_slave1],MPI_UNSIGNED,received_slave1+1,6,MPI_COMM_WORLD,&reqr7[received_slave1]);
			
	    }	
		for(int i=0;i<nprocs-1;i++){
			MPI_Wait(&reqr1[i],MPI_SUCCESS);
			MPI_Wait(&reqr2[i],MPI_SUCCESS);
			MPI_Wait(&reqr3[i],MPI_SUCCESS);
			MPI_Wait(&reqr4[i],MPI_SUCCESS);
			MPI_Wait(&reqr5[i],MPI_SUCCESS);		
			MPI_Wait(&reqr6[i],MPI_SUCCESS);
			MPI_Wait(&reqr7[i],MPI_SUCCESS);
		}
		
		for(int p =0;p<nprocs-1;p++){
				int beg = 0;
				for(int i=0;i<sizeOfBettiTable[p];i++){
					bettiBoundaryTableEntry bettiEntry;
					bettiEntry.bettiDim = betti_dim[p][i];
					bettiEntry.birth = betti_birth[p][i];
					bettiEntry.death = betti_death[p][i];
					for(int bi = beg;bi<(beg+betti_boundarysize[p][i]);bi++)
							bettiEntry.boundaryPoints.insert(betti_boundaries[p][bi]);
					
						beg +=betti_boundarysize[p][i];
						mergedMasterBettiTable.push_back(bettiEntry);
					}
				}
				
			
			std::cout << std::endl << "_______BETTIS_______" << std::endl;
		
			std::cout << std::endl << "mergedMasterBettiTable size is :: " << mergedMasterBettiTable.size() << std::endl;
				
			//Output the data using writeOutput library
			auto pipe = args.find("outputFile");
			if(pipe != args.end()){
				if (args["outputFile"] == "console"){
					//ws->writeConsole(wD);
				} else {
					ws->writeStats(wD->stats, args["outputFile"]);
					ws->writeBarcodes(mergedMasterBettiTable, args["outputFile"]);
					
				}
			}

		//SLAVE PROCESS:

		} else {
			int dim;
			//NOTE: need to have dynamic partition size; whether that means serializing and sending
			//	the partition table and size or dynamically allocating the partsize vector here (push_back)
			std::vector<unsigned> partsize;
			int partsize_size;
			MPI_Request req;
			//MPI_RECV( &Data, Size, 
			//Get the dimension of data coming in
			MPI_Request req1;
			MPI_Irecv(&dim,1,MPI_INT,0,1,MPI_COMM_WORLD,&req1);
			
			//Get the total number of partitions
			MPI_Request req2;
			MPI_Irecv(&partsize_size,1,MPI_INT,0,1,MPI_COMM_WORLD,&req2);
			MPI_Wait(&req2,MPI_SUCCESS);
			partsize.resize(partsize_size);
			
			//Get the partition sizes
			MPI_Request req3;
			MPI_Irecv(&partsize[0],partsize_size,MPI_UNSIGNED,0,1,MPI_COMM_WORLD,&req3);
			
			int no_of_partition;
			MPI_Request req4;
			MPI_Irecv(&no_of_partition,1,MPI_INT,0,1,MPI_COMM_WORLD,&req4);
			
			std::vector<std::vector<double>> flatPartitions(no_of_partition);
			std::vector<std::vector<unsigned>> labels(no_of_partition);	
		
			//std::vector<bettiBoundaryTableEntry> mergedBettiTableSlave;
			std::vector<unsigned> betti_dim;
			std::vector<double> betti_birth;
			std::vector<double> betti_death;
			std::vector<unsigned> betti_boundarysize;
			std::vector<unsigned> betti_boundaries;
		
			MPI_Wait(&req1,MPI_SUCCESS);
			MPI_Wait(&req3,MPI_SUCCESS);
			MPI_Wait(&req4,MPI_SUCCESS);
			MPI_Request req5[no_of_partition];	
			MPI_Request req6[no_of_partition];			
			for(unsigned z = 0; z < no_of_partition; z++){
				int rsize = partsize[z*(nprocs-1)+(id-1)]*dim;
				flatPartitions[z].resize(rsize);
				MPI_Irecv(&flatPartitions[z][0],rsize,MPI_DOUBLE,0,1,MPI_COMM_WORLD,&req5[z]);
				
				int lsize = partsize[z*(nprocs-1)+(id-1)];
				labels[z].resize(lsize);
				MPI_Irecv(&labels[z][0],lsize,MPI_UNSIGNED,0,1,MPI_COMM_WORLD,&req6[z]);
			}
			
			std::vector<unsigned> ck(no_of_partition);
			for(unsigned i=0;i<no_of_partition;i++)
				ck[i]=i;
			
			for(unsigned z = 0; z < no_of_partition; z++){
				int flag =0;
				int received_partition;
				//Look for partition to recieve
				while(ck.size()>0){
					for(auto c : ck){
						MPI_Test(&req5[c],&flag,MPI_SUCCESS);
						if(flag!=0){
							MPI_Wait(&req6[c],MPI_SUCCESS);
							received_partition = c;
							ck.erase(std::remove(ck.begin(), ck.end(), c), ck.end());
							break;
						}					
					}
					if(flag!=0)
						break;
				}
			auto partitionedData = ut.deserialize(flatPartitions[received_partition],dim);
			if(partitionedData.size() > 0){
				std::cout << "Running Pipeline with : " << partitionedData.size() << " vectors" << " id :: "<<id<<std::endl;
				wD->originalData = partitionedData;
				runPipeline(args, wD);
				wD->complex->clear();
				//Map partitions back to original point indexing
				ut.mapPartitionIndexing(labels[received_partition], wD->bettiTable);
				for(auto bet : wD->bettiTable){
					//Check if the current betti entry's minimum index is within the partition (or outside, discard)
					
					//mergedBettiTableSlave.push_back(bet);
					betti_dim.push_back(bet.bettiDim);
					betti_birth.push_back(bet.birth);
					betti_death.push_back(bet.death);
					betti_boundarysize.push_back(bet.boundaryPoints.size());
					betti_boundaries.insert(betti_boundaries.end(),bet.boundaryPoints.begin(),bet.boundaryPoints.end());
				}
			} else 
				std::cout << "skipping" << std::endl;
			}
			
				//Sending of merged betti results back to master (but this should be done after all partitions are finished)
				int bettiTableSize = betti_dim.size();
			    MPI_Request reqs1;
				MPI_Isend(&bettiTableSize,1,MPI_INT,0,0,MPI_COMM_WORLD,&reqs1);
				
				MPI_Request reqs2;
				MPI_Isend(&betti_dim[0],bettiTableSize,MPI_UNSIGNED,0,1,MPI_COMM_WORLD,&reqs2);
				
				MPI_Request reqs3;
				MPI_Isend(&betti_birth[0],bettiTableSize,MPI_DOUBLE,0,2,MPI_COMM_WORLD,&reqs3);
				
				MPI_Request reqs4;
				MPI_Isend(&betti_death[0],bettiTableSize,MPI_DOUBLE,0,3,MPI_COMM_WORLD,&reqs4);
				
				MPI_Request reqs5;
				MPI_Isend(&betti_boundarysize[0],bettiTableSize,MPI_UNSIGNED,0,4,MPI_COMM_WORLD,&reqs5);
				
				int bound_size = 0;
				for(auto e :betti_boundarysize)
					bound_size += e;
				
				MPI_Request reqs6;
				MPI_Isend(&bound_size,1,MPI_INT,0,5,MPI_COMM_WORLD,&reqs6);
				
				MPI_Request reqs7;
				MPI_Isend(&betti_boundaries[0],bound_size,MPI_UNSIGNED,0,6,MPI_COMM_WORLD,&reqs7);
		
				MPI_Wait(&reqs1,MPI_SUCCESS);	
				MPI_Wait(&reqs2,MPI_SUCCESS);
				MPI_Wait(&reqs3,MPI_SUCCESS);
				MPI_Wait(&reqs4,MPI_SUCCESS);
				MPI_Wait(&reqs5,MPI_SUCCESS);
				MPI_Wait(&reqs6,MPI_SUCCESS);
				MPI_Wait(&reqs7,MPI_SUCCESS);
					
		}
	}	

	int main(int argc, char* argv[]){
		//	Steps to compute PH with preprocessing:
		//
		//		1.  Preprocess the input data
		//			a. Store the original dataset, reduced dataset, cluster indices
		//
		//		2.	Build a 2-D neighborhood graph
		//			a. Weight each edge less than epsilon
		//			b. Order edges (min to max)
		//
		//		3.  Rips filtration
		//			a. Build higher-level simplices (d > 2)
		//				- Retain the largest of edges as weight (when simplex forms)
		//			b. Build list of **relevant epsilon values
		//
		//		4.	Betti calculation
		//			a. For each relevant epsilon value
		//				i.	Create Boundary Matrix
		//					- if pn[i,j] < n, set to 1
		//					- else set to 0
		//				ii.	Compute RREF
		//				iii.Store constituent boundary points
		//
		//		5.	Upscaling
		//			a. Upscale around boundary points
		//			b. For each upscaled boundary, repeat steps 2-4
		//
		//
		//	**relevant refers to edge weights, i.e. where a simplex of any
		//			dimension is created (or merged into another simplex)
		
		
		//Define external classes used for reading input, parsing arguments, writing output
		MPI_Init(&argc,&argv);
		MPI_Comm_size(MPI_COMM_WORLD,&nprocs);
		MPI_Comm_rank(MPI_COMM_WORLD,&id);
		
		auto *rs = new readInput();
		auto *ap = new argParser();
		
		//Parse the command-line arguments
		auto args = ap->parse(argc, argv);
		
		//Determine what pipe we will be running
		ap->setPipeline(args);
		
	 //   for(auto z : args)
		//	std::cout << z.first << "\t" << z.second << std::endl;
		
		//Create a pipePacket (datatype) to store the complex and pass between engines
		auto *wD = new pipePacket(args, args["complexType"]);	//wD (workingData)
		
		if(args["pipeline"] != "slidingwindow" && args["pipeline"] != "naivewindow" && args["mode"] != "mpi"){
			//Read data from inputFile CSV
			wD->originalData = rs->readCSV(args["inputFile"]);
			wD->fullData = wD->originalData;
		}
		
		//If data was found in the inputFile
		if(wD->originalData.size() > 0 || args["pipeline"] == "slidingwindow" || args["pipeline"] == "naivewindow" || args["mode"] == "mpi"){

			//Add data to our pipePacket
			wD->originalData = wD->originalData;
			
			if(args["upscale"] == "true"){
				processUpscaleWrapper(args, wD);
			} else if (args["mode"] == "reduced"){
				processReducedWrapper(args,wD);			
			} else {
				processDataWrapper(args, wD);
			}
		} else {
			ap->printUsage();
		}
		
		
		 MPI_Finalize();
		return 0;
	}
