package clustering.r;

import org.omegahat.R.Java.REvaluator;
import org.omegahat.R.Java.ROmegahatInterpreter;

/**
 *
 * <P>CVS $Id: RSingletons.java,v 1.2 2005/09/28 01:06:58 khuck Exp $</P>
 * @author  Kevin Huck
 * @version 0.1
 * @since   0.1
 */
public class RSingletons {
	private static ROmegahatInterpreter rInterpreter = null;
	private static REvaluator rEvaluator = null;

	/**
	 * The RInterpreter is a singleton object, so if the object has not yet
	 * been instantiated, create one.  Then return the instance.
	 * 
	 * @return
	 */
	public static ROmegahatInterpreter getRInterpreter () {
		if (rInterpreter == null) {
			String myArgs[] = new String[2];
			myArgs[0] = new String("--silent");
			myArgs[1] = new String("--vanilla");
			rInterpreter = 
				new ROmegahatInterpreter(ROmegahatInterpreter.fixArgs(myArgs), false);
		}
		return rInterpreter;
	}

	/**
	 * The REvaluator is a singleton object, so if the object has not yet
	 * been instantiated, create one.  Then return the instance.
	 * 
	 * @return
	 */
	public static REvaluator getREvaluator () {
		// make suer the interpreter is not null
		getRInterpreter();
		if (rEvaluator == null) {
			rEvaluator = new REvaluator();
		}
		return rEvaluator;
	}
	
	/**
	 * End the R Session.  Currently only called on RAnalysisFactory.closeFactory().
	 */
	public static void endRSession() {
		try {
			// call the method, to make sure the evaluator exists
			getREvaluator().call("quit");
		} catch (Exception e) { 
			System.out.println(e);
			e.printStackTrace();
		}
		rInterpreter = null;
		rEvaluator = null;
		System.out.println("R session over.");
	}
}

