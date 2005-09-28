/*
 * Created on Mar 16, 2005
 *
 */
package clustering.r;

import common.*;
import clustering.*;
import org.omegahat.R.Java.REvaluator;

/**
 * This class is used as a list of names and values to describe 
 * a cluster created during some type of clustering operation.
 * 
 * <P>CVS $Id: RDimensionReduction.java,v 1.2 2005/09/28 01:06:58 khuck Exp $</P>
 * @author khuck
 *
 */
public class RDimensionReduction implements DimensionReductionInterface {

	private RawDataInterface inputData = null;
	private REvaluator rEvaluator = null;
	private String method = RMIPerfExplorerModel.NONE;
	private int newDimension = 0;
	
	/**
	 * Default constructor
	 */
	public RDimensionReduction(String method, int newDimension) {
		super();
		this.rEvaluator = RSingletons.getREvaluator();
		this.method = method;
		this.newDimension = newDimension;
	}

	public void reduce() throws ClusterException {
		if (method.equals(RMIPerfExplorerModel.LINEAR_PROJECTION)) {
			System.out.print("Reducing Dimensions...");
			int numReduced = inputData.numVectors() * newDimension;
			rEvaluator.voidEval("reducer <- matrix((runif("+numReduced+",0,1)), nrow=" +
			inputData.numDimensions() + ", ncol="+newDimension+")");
			rEvaluator.voidEval("raw <- crossprod(t(raw), reducer)");
			System.out.println(" Done!");
		}
		return;
	}

	public void setInputData(RawDataInterface inputData) {
		this.inputData = inputData;
		return ;
	}

	public RawDataInterface getOutputData () {
		// do nothing - the data stays in R
		RawDataInterface data = null;
		return data;
	}
}
