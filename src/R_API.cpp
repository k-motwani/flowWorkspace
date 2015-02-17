/*
 *
 *
 * some C++ routines to be invoked by R for the faster R APIs
 *
 *  Created on: Aug 18, 2014
 *      Author: wjiang2
 */
#include "include/R_GatingSet.hpp"//need to be manually added to RcppExports.CPP as well

//[[Rcpp::plugins(temp)]]

//' grab vectors of pop counts and the parent counts along with their paths and FCS filenames
//'
//' This speeds up the process of getPopStats by putting the loop in c++ and avoiding copying while constructing vectors
//'
//' @param gsPtr external pointer that points to the C data structure of GatingSet
//' @param sampleNames sample names vector
//' @param subpopulation population vector that specify the subset of pops to query
//' @param flowJo logical flag to specify whether flowCore or flowJo counts to return
//' @param isFullPath logical flag to specify whether return the full path or partial path of populations
//[[Rcpp::export(".getPopCounts")]]
Rcpp::List getPopCounts(Rcpp::XPtr<GatingSet> gsPtr, StringVec sampleNames, StringVec subpopulation, bool flowJo, bool isFullPath){

	bool isFlowCore = !flowJo;

	unsigned nPop = subpopulation.size();
	unsigned nSample = sampleNames.size();
	unsigned nVec = nPop * nSample;
	Rcpp::CharacterVector sampleVec(nVec);
	Rcpp::CharacterVector popVec(nVec);
	Rcpp::CharacterVector parentVec(nVec);
	Rcpp::IntegerVector countVec(nVec);
	Rcpp::IntegerVector parentCountVec(nVec);

	StringVec allNodes = gsPtr->getGatingHierarchy(sampleNames.at(0))->getPopPaths(REGULAR, isFullPath, true);

	unsigned counter = 0;
	for(unsigned i = 0; i < nSample; i++){
		std::string sn = sampleNames.at(i);
		GatingHierarchy * gh = gsPtr->getGatingHierarchy(sn);
		for(unsigned j = 0; j < nPop; j++){
			 	std::string pop = subpopulation.at(j);
				sampleVec(counter) = sn;
				popVec(counter) = pop;

//				get count of this pop
				VertexID u = gh->getNodeID(pop);
				countVec(counter) = gh->getNodeProperty(u).getStats(isFlowCore)["count"];

//				 get parent name
				VertexID pid = gh->getParent(u);
				parentVec(counter) = allNodes.at(pid);

//				get parent count
				parentCountVec(counter) = gh->getNodeProperty(pid).getStats(isFlowCore)["count"];

				//increment counter
				counter++;

		}
	}



	return Rcpp::List::create(Rcpp::Named("name", sampleVec)
							, Rcpp::Named("Population", popVec)
							, Rcpp::Named("Parent", parentVec)
							, Rcpp::Named("Count", countVec)
							, Rcpp::Named("ParentCount", parentCountVec)
							);
}


//' construct the biexpTrans c++ object on the fly
//'
//' It returns the spline coefficients vectors to R.
//'
//' It is used to extract the spline coefficient vectors from the calibration table
//' which is computed by biexpTrans class and then return to R for constructing flowJo transformation function within R.
//' Mainly used for openCyto autoGating process where no xml workspace is needed to create flowJo transformation.
//[[Rcpp::export(".getSplineCoefs")]]
Rcpp::List getSplineCoefs(int channelRange=4096, double maxValue=262144, double pos = 4.5, double neg = 0, double widthBasis = -10){

	biexpTrans curTran;
	curTran.channelRange = channelRange;
	curTran.maxValue = maxValue;
	curTran.pos = pos;
	curTran.neg = neg;
	curTran.widthBasis = widthBasis;


	curTran.computCalTbl();
	calibrationTable cal = curTran.getCalTbl();

	cal.interpolate();
	Spline_Coefs obj=cal.getSplineCoefs();

	return Rcpp::List::create(Named("z",obj.coefs)
								, Named("method",obj.method)
								, Named("type", "biexp")
								, Named("channelRange", channelRange)
								, Named("maxValue", maxValue)
								, Named("neg", neg)
								, Named("pos", pos)
								, Named("widthBasis", widthBasis)
								);


}

//' store the transformation functions created from R into GatingSet
//'
//' @param gsPtr external pointer that points to the C data structure of GatingSet
//' @param transformList a transformList that constains a list of transformation functions.
//'         Each of these functions carries the attributes to be used to convert to c++ transformation
//[[Rcpp::export(".addTrans")]]
void addTrans(Rcpp::XPtr<GatingSet> gsPtr, Rcpp::S4 transformList){

	trans_map tm;
	/*
	 * parse the transformList
	 */
	Rcpp::List funs = transformList.slot("transforms");
	for(Rcpp::List::iterator it = funs.begin(); it != funs.end(); it++){
		Rcpp::S4 transMp = *it;
		std::string ch = transMp.slot("input");
		Rcpp::Function transFunc = transMp.slot("f");

		Rcpp::RObject type = transFunc.attr("type");
		if(type.isNULL())
			Rcpp::stop("transformation function must have 'type' attribute!");
		else{
			std::string trans_type = Rcpp::as<std::string>(type.get__());
			if(trans_type == "biexp")
			{
				Rcpp::List param = transFunc.attr("parameters");
				/*
				 * create biexpTrans based on the parameters stored as function attribute
				 */
				biexpTrans * trans =  new biexpTrans();
				trans->channelRange = Rcpp::as<int>(param["channelRange"]);
				trans->maxValue = Rcpp::as<int>(param["maxValue"]);
				trans->neg = Rcpp::as<double>(param["neg"]);
				trans->pos = Rcpp::as<double>(param["pos"]);
				trans->widthBasis = Rcpp::as<double>(param["widthBasis"]);
				//compute the calibration table
				trans->computCalTbl();
				trans->interpolate();

				//push into the trans map
				tm[ch] = trans;

			}else
				Rcpp::stop("add the unsupported transformation function!" + trans_type);
		}
	}

	/*
	 * add the new trans map to the GatingSet
	 */
	gsPtr->addTransMap("autoGating", tm);
	/*
	 * propagate to each sample
	 */
	StringVec sn = gsPtr->getSamples();
	for(StringVec::iterator it = sn.begin(); it != sn.end(); it++){
		GatingHierarchy * gh = gsPtr->getGatingHierarchy(*it);
		gh->addTransMap(tm);
	}

}
