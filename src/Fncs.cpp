#include <RcppEigen.h>
#include "Fncs.h"
using namespace Rcpp;

/**
 * sampleInt
 * @brief Method to sample integer using probabilities p
 * @param probs vector of probabilities
 * @param totP sum of p
 * @returns integer from 0 to length of p minus 1
 */
int sampleInt(const std::vector<double> &probs, double totP = 1) {
  double u    = R::runif(0, totP);
  double sum  = probs[0];
  
  int i = 0;
  while (sum < u) {
    ++i;
    sum += probs[i];
  }

  return(i);
}

/**
 * sampleInt
 * @brief Method to sample integer using probabilities p
 * @param probs vector of probabilities 
 * @returns integer from 0 to length of p minus 1
 */
int sampleInt(const Eigen::VectorXd &probs){
  double totP = probs.sum();
  double u    = R::runif(0, totP);
  double sum  = probs(0);

  int i = 0;
  while (sum < u) {
    ++i;
    sum += probs(i);
  }
  return(i);
}

/**
 * logPSplit
 * @brief log probability of tree split at `depth`: p_split(eta)=alpha/(1+d_eta)^beta
 * @param alpha parameter in range (0, 1)
 * @param beta parameter > 0
 * @param depth depth of split (begins at zero)
 * @param terminal if true returns log(1-p)
 * @returns log probability
 */
double logPSplit(double alpha, double beta, int depth, bool terminal){
  double p = alpha * pow(1.0 + (double)depth, -beta);
  if (terminal) {
    return(log1p(-p));
  } else {
    return(log(p));
  }
}

/**
 * @brief 
 * 
 * @param timeProbs 
 * @param tmin 
 * @param tmax 
 * @param term 
 * @returns double 
 */
double logZIPSplit(Eigen::VectorXd timeProbs, int tmin, int tmax, int nTrees, bool term) {
  double timeprob = 1.0 / (1.0 + exp(-timeProbs.segment(tmin - 1, tmax - tmin + 1).mean()));
  // Rcout << "\n\n-----" << tmin << " - " << tmax << "  Psplit=" << timeprob;
  if (term){
    return log1p(-timeprob);
  } else {
    return log(timeprob);
  }
}

/**
 * logDirichletDensity
 * @brief log probability of Dirichlet with values x and parameters alpha
 * @param x vector of values
 * @param alpha vector of parameters
 * @returns log probability
 */
double logDirichletDensity(const Eigen::VectorXd &x, const Eigen::VectorXd &alpha){
  if (x.size() != alpha.size()){ // ! incorrect sizes
    stop("logDirichletDensity incorrect size");
  }

  double out = lgamma(alpha.sum());

  for (int i = 0; i < alpha.size(); ++i){
    out += ((alpha(i) - 1) * log(x(i))) - lgamma(alpha(i));
  }

  return(out);
}

/**
 * rDirichlet
 * @brief random draw from Dirichlet distribution with parameters alpha
 * @param alpha parameters
 * @returns vector containing draw from Dirichlet
 */
Eigen::VectorXd rDirichlet(const Eigen::VectorXd &alpha) {
  Eigen::VectorXd out(alpha.size());
  double norm = 0;
  for (int i = 0; i < alpha.size(); i++) {
    out(i) = R::rgamma(alpha(i), 1);
    norm += out(i);
  }
  out /= norm;
  return(out);
}

/**
 * @brief draw C^+(0, 1) full conditional using hierarchy: x^2|y~IG(1/2,1/y), y~IG(1/2,1). Full conditional: y|-~IG(1,x^2/(x^2+1)), x^2|-~IG((a+1)/2,1/(b/2+y))
 * 
 * @param x2 pointer to current value of parameter x^2
 * @param a additional IG component for x^2 full conditional
 * @param b additional IG component for x^2 full conditional
 * @param yInv pointer to update 1/y
 * @returns double x^2 draw from full conditional
 */
void rHalfCauchyFC(double* x2, double a, double b, double* yInv){
  double yi = R::rgamma(1.0, *x2 / (*x2 + 1.0));
  if (yInv != 0){
    *yInv = yi;
  }

  *x2 = 1.0 / R::rgamma(0.5 * (a + 1.0), 2.0 / (b + 2.0 * yi));
}

/**
 * intersectAndDiff
 * @brief Function to simultaneously calculate intersection and difference 
 of origVec to newVec
 * @param origVec starting vector of sorted integers
 * @param newVec vector of unsorted integers to be compared to origVec
 * @returns std vector with 2 elements: intersection and difference
 */
// std::vector<std::vector<int> > 
std::pair<std::vector<int>, std::vector<int> >
  intersectAndDiff(const std::vector<int> &origVec, const std::vector<int> &newVec){

  // Assume origVec and newVec are sorted
  // std::sort(newVec.begin(), newVec.end()); // not needed! ~20% speedup
  std::vector<int> intVec;
  std::vector<int> diffVec;
  
  if (origVec.size() == 0){
    return(std::make_pair(origVec, origVec));
  }
    
  if (newVec.size() == 0){
    return(std::make_pair(newVec, origVec));
  }
    
  intVec.reserve(newVec.size());
  diffVec.reserve(origVec.size());

  std::size_t i = 0;
  std::size_t j = 0;
  // iterate over origVec and newVec
  do {
      if (origVec[i] < newVec[j]) { // difference
        diffVec.push_back(origVec[i]);
        ++i;
        continue;

      } else if (origVec[i] == newVec[j]) { // intersection
        intVec.push_back(origVec[i]);
        ++i; ++j;

      } else { // origVec[i] > newVec[j]
        ++j;
      }

      if (j == newVec.size()) {
        diffVec.insert(diffVec.end(), origVec.begin() + i, origVec.end());
        break;
      }

    } while (i < origVec.size());

  return(std::make_pair(intVec, diffVec));
}


//' fast set intersection tool assumes sorted vectors A and B
//'
//' @param A sorted integer vector A
//' @param B sorted integer vector B
//' @returns vector of resulting intersection
//' @export
// [[Rcpp::export]]
std::vector<int> cppIntersection(const IntegerVector& A, const IntegerVector& B) {
  std::vector<int> output;
  std::set_intersection(A.begin(), A.end(), B.begin(), B.end(),
                        std::back_inserter(output));
  return output;
}


/**
 * selectInd
 * @brief Subset a vector only with given indices
 * 
 * @param original A vector to be subset
 * @param indices A vector containing wanted indices
 * @returns A vector with values of given indices
 */

Eigen::VectorXd selectInd(Eigen::VectorXd original, std::vector<int> indices) {
  int m = indices.size(); // Get the total number of index

  Eigen::VectorXd subset; // define a subset
  subset.resize(m);

  // For loop to collect values with a corresponding index
  for(int i = 0; i < m; i++){
    int index   = indices[i]; // Get an index from indices vector
    double val  = original(index); // Find the value corresponding to the index
    subset(i)   = val; // Save the value
  }

  return subset;
}


/**
 * selectIndM
 * @brief Subset a matrix only with given indices (rows)
 * 
 * @param original A vector to be subset
 * @param indices A vector containing wanted indices of row
 * @returns A vector with values of given indices
 */

Eigen::MatrixXd selectIndM(Eigen::MatrixXd original, std::vector<int> indices) {
  int rownum = indices.size(); // row# of submat
  int colnum = original.cols(); // col# of submat

  Eigen::MatrixXd submat; // define a submat
  submat.resize(rownum, colnum);

  // Match indices
  for(int i = 0; i < rownum; i++){
    int index = indices[i]; // Get an index from indices vector
    for(int j = 0; j < colnum; j++){
      double val    = original(index, j); // Find the value corresponding to the index
      submat(i, j)  = val;                // Save the value
    }
  }

  return submat;
}
