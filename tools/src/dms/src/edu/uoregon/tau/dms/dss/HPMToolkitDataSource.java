/*
 * Name: HPMToolkitDataSource.java 
 * Author: Kevin Huck, Alan Morris 
 * Description: Parse an HPMToolkit data file
 */


package edu.uoregon.tau.dms.dss;

import java.io.*;
import java.util.*;

public class HPMToolkitDataSource extends DataSource {

    public HPMToolkitDataSource(Object initializeObject) {
        super();
        this.setMetrics(new Vector());
        this.initializeObject = initializeObject;
    }

    private Object initializeObject;

    public void cancelLoad() {
        return;
    }

    public int getProgress() {
        return 0;
    }

    public void load() throws FileNotFoundException, IOException {

        boolean firstFile = true;
        v = (Vector) initializeObject;
        for (Enumeration e = v.elements(); e.hasMoreElements();) {
            File files[] = (File[]) e.nextElement();
            for (int i = 0; i < files.length; i++) {
                //System.out.println("Processing data file, please wait ......");
                long time = System.currentTimeMillis();

                FileInputStream fileIn = new FileInputStream(files[i]);
                InputStreamReader inReader = new InputStreamReader(fileIn);
                br = new BufferedReader(inReader);

                // increment the node counter - there's a file for each node.
                nodeID++;

                // clear the event names
                eventNames = new Hashtable();

                //####################################
                //First Line
                //####################################
                //This line is not required. Check to make sure that it is there however.
                inputString = br.readLine();
                if (inputString == null)
                    return;
                //####################################
                //End - First Line
                //####################################

                // find the end of the resource statistics
                while ((inputString = br.readLine()) != null) {
                    if (inputString.trim().startsWith("Instrumented section:")) {
                        // System.out.println("Found beginning of data: ");
                        // exit this while loop
                        break;
                    }
                }

                boolean first = true; // we are currently defining the
                // boundaries between instrumented sections to be the line "Instrumented section:"
                // so we copy inclusive -> exclusive at this time, but we don't
                // want to do it the first time.
                boolean exclusiveSet = false;
                // find the callsite data
                while (inputString != null) {
                    if (inputString.length() == 0) {
                        // do nothing
                    } else if (inputString.trim().startsWith("Instrumented section:")) {

                        if (!first && !exclusiveSet) {
                            // there was no exclusive time for this instrumented section, this means
                            // that the exclusive time = inclusive time (it did not call any
                            // instrumented functionProfiles.
                            // copy inclusive over to exclusive
                            functionProfile.setExclusive(timeMetric, functionProfile.getInclusive(timeMetric));
                            if (functionProfile.getExclusive(timeMetric) > function.getMaxExclusive(timeMetric)) {
                                function.setMaxExclusive(timeMetric, functionProfile.getExclusive(timeMetric));
                            }
                        }
                        first = false;
                        exclusiveSet = false;
                        processHeaderLine1(inputString);
                    } else if (inputString.trim().startsWith("file:")) {
                        processHeaderLine2(inputString);
                    } else if (inputString.trim().startsWith("Count:")) {
                        processHeaderLine3(inputString);
                    } else if (inputString.trim().startsWith("Wall Clock Time:")) {
                        //processHardwareCounter(inputString);
                        processTime(inputString, true);
                    } else if (inputString.trim().startsWith("Average duration:")) {
                        // because I can't figure out how to get the metrics
                        // allocated correctly, just toss this value out. I don't have
                        // this metric for every measurement, so the metric indexing
                        // gets all hosed up.
                        // - kevin, Nov 15, 2004
                        // processHardwareCounter(inputString);
                        processHeaderLine4(inputString, 1);
                    } else if (inputString.trim().startsWith("Standard deviation:")) {
                        // because I can't figure out how to get the metrics
                        // allocated correctly, just toss this value out. I don't have
                        // this metric for every measurement, so the metric indexing
                        // gets all hosed up.
                        // processHardwareCounter(inputString);
                        processHeaderLine4(inputString, 2);
                    } else if (inputString.trim().startsWith("Exclusive duration:")) {
                        // because I can't figure out how to get the metrics
                        // allocated correctly, just toss this value out. I don't have
                        // this metric for every measurement, so the metric indexing
                        // gets all hosed up.
                        // processHardwareCounter(inputString);
                        //processHeaderLine4(inputString, 3);
                        processTime(inputString, false);
                        exclusiveSet = true;
                    } else if (inputString.trim().startsWith("Total time in user mode:")) {
                        processHardwareCounter(inputString);
                    } else {
                        processHardwareCounter(inputString);
                    }
                    inputString = br.readLine();
                }

                // we also have to do this here to get the last instrumented
                // section
                if (!first && !exclusiveSet) {
                    // there was no exclusive time for this instrumented
                    // section, this means that the exclusive time = inclusive time (it did not
                    // call any instrumented functionProfiles.
                    // copy inclusive over to exclusive
                    functionProfile.setExclusive(timeMetric, functionProfile.getInclusive(timeMetric));
                    if (functionProfile.getExclusive(timeMetric) > function.getMaxExclusive(timeMetric)) {
                        function.setMaxExclusive(timeMetric, functionProfile.getExclusive(timeMetric));

                    }
                }

                //Close the file.
                br.close();

                //Set firstRead to false.
                this.setFirstMetric(false);

                time = (System.currentTimeMillis()) - time;
                //System.out.println("Time to process file (in milliseconds): " + time);
            }
        }
        //Generate derived data.
        this.generateDerivedData();
    }

    //####################################
    //Private Section.
    //####################################

    private void initializeThread() {
        // create the mapping, if necessary
        if (header2.s1 == null)
            function = this.getTrialData().addFunction(header1.s0 + ", " + header2.s0, 1);
        else
            function = this.getTrialData().addFunction(header1.s0 + ", " + header2.s0 + " lines " + header2.s1,
                    1);

        // System.out.println("**** " + header1.s0 + ", " + header2.s0 + " lines
        // " +header2.s1 + " " + threadID + " ****");

        // make sure we start at zero for all counters
        nodeID = (nodeID == -1) ? 0 : nodeID;
        contextID = (contextID == -1) ? 0 : contextID;
        threadID = (threadID == -1) ? 0 : threadID;

        //Get the node,context,thread.
        node = this.getNCT().getNode(nodeID);
        if (node == null)
            node = this.getNCT().addNode(nodeID);
        context = node.getContext(contextID);
        if (context == null)
            context = node.addContext(contextID);
        thread = context.getThread(threadID);
        if (thread == null) {
            thread = context.addThread(threadID);
            thread.setDebug(this.debug());
            thread.initializeFunctionList(this.getTrialData().getNumFunctions());
            thread.initializeUsereventList(this.getTrialData().getNumUserEvents());
        }

        functionProfile = thread.getFunctionProfile(function);
        if (functionProfile == null) {
            functionProfile = new FunctionProfile(function);
            thread.addFunctionProfile(functionProfile, function.getID());
        }

        initialized = true;
    }

    private void processHeaderLine1(String string) {
        // System.out.println("Header line 1");
        StringTokenizer st1 = new StringTokenizer(string, "-");

        // get the first name/value pair
        string = st1.nextToken();
        StringTokenizer st2 = new StringTokenizer(string, ":");
        // ignore the "instrumented section" label
        string = st2.nextToken();
        // get the value
        header1.i0 = Integer.parseInt(st2.nextToken().trim()); // section id

        // get the next name/value pair
        string = st1.nextToken();
        st2 = new StringTokenizer(string, ":");
        // ignore the "Label" label
        string = st2.nextToken();
        // get the value
        header1.s0 = st2.nextToken().trim(); // label value

        // get the next name/value pair
        string = st1.nextToken();
        st2 = new StringTokenizer(string, ":");
        // ignore the "process" label
        string = st2.nextToken();
        // get the value
        header1.i1 = Integer.parseInt(st2.nextToken().trim()); // process id

        // if this label is the same as the previous, increment the thread
        // ID
        // otherwise, reset the thread ID to 0.
        if (eventNames.containsKey(header1.s0)) {
            Integer tmpID = (Integer) eventNames.get(header1.s0);
            threadID = tmpID.intValue();
            threadID++;
            eventNames.put(header1.s0, new Integer(threadID));
        } else {
            threadID = 0;
            eventNames.put(header1.s0, new Integer(threadID));
        }
        initialized = false;
    }

    private void processHeaderLine2(String string) {
        // System.out.println("Header line 2");
        header2.s0 = null;
        header2.s1 = null;
        StringTokenizer st1 = new StringTokenizer(string, ",");

        // get the first name/value pair
        string = st1.nextToken();
        StringTokenizer st2 = new StringTokenizer(string, ":");
        // ignore the "file" label
        string = st2.nextToken();
        // get the value
        header2.s0 = st2.nextToken().trim(); // section id

        // get the next name/value pair
        string = st1.nextToken();
        st2 = new StringTokenizer(string, ":");
        // ignore the "lines" label
        string = st2.nextToken();
        // get the value
        header2.s1 = st2.nextToken().trim(); // label value
    }

    private void processHeaderLine3(String string) {
        // System.out.println("Header line 3");
        StringTokenizer st1 = new StringTokenizer(string, ":");
        // ignore the "count" label
        string = st1.nextToken();
        // get the value
        header3.i0 = Integer.parseInt(st1.nextToken().trim()); // section id
    }

    private void processHeaderLine4(String string, int variable) {
        // System.out.println("Header line 4");
        StringTokenizer st1 = new StringTokenizer(string, ":");
        // ignore the "Wall Clock Time" label
        string = st1.nextToken();
        // get the value
        string = st1.nextToken();
        StringTokenizer st2 = new StringTokenizer(string, " ");
        if (variable == 0)
            header4.d0 = Double.parseDouble(st2.nextToken()); // section id
        else if (variable == 1)
            header4.d1 = Double.parseDouble(st2.nextToken()); // section id
        else if (variable == 2)
            header4.d2 = Double.parseDouble(st2.nextToken()); // section id
        else if (variable == 3)
            header4.d3 = Double.parseDouble(st2.nextToken()); // section id
    }

    //    private void processHeaderLine5(String string) {
    //        // System.out.println("Header line 5");
    //        try {
    //            StringTokenizer st1 = new StringTokenizer(string, ":");
    //            // ignore the "Total time in user mode" label
    //            string = st1.nextToken();
    //            // get the value
    //            string = st1.nextToken();
    //            StringTokenizer st2 = new StringTokenizer(string, " ");
    //            header5.d0 = Double.parseDouble(st2.nextToken()); // section id
    //        } catch (Exception e) {
    //            System.out.println("An error occurred while parsing the header!");
    //            e.printStackTrace();
    //        }
    //        initializeThread();
    //    }

    private void processHardwareCounter(String string) {
        if (!initialized) {
            if (header2.s0 == null)
                header2.s0 = new String("Entire Program");
            if (header3.i0 == 0)
                header3.i0 = 1;
            initializeThread();
        } else {
            // thread.incrementStorage();
            function.incrementStorage();
            functionProfile.incrementStorage();
        }
        // System.out.println("Hardwoare counter");
        StringTokenizer st1 = new StringTokenizer(string, ":");
        String metricName = st1.nextToken().trim(); // hardware counter name
        String tmpStr = st1.nextToken().trim();
        int metricCount = 0, newMetricCount = 0;
        // need to clean stuff out of the value, like % and M and whatnot
        st1 = new StringTokenizer(tmpStr, " ");
        double dEventValue = 0.0;
        int iEventValue = 0;
        tmpStr = st1.nextToken().trim();
        boolean typeDouble = false;
        //		if (tmpStr.indexOf(".") > -1) {

        try {
            dEventValue = Double.parseDouble(tmpStr); // callsite index
        } catch (NumberFormatException e) {
            dEventValue = 0;
        }

        typeDouble = true;
        //		} else {
        //		iEventValue = Integer.parseInt(tmpStr); // callsite index
        //		}
        if (st1.hasMoreTokens())
            metricName += " (" + st1.nextToken() + ")";
        // System.out.println(metricName + " = " + dEventValue);

        metricCount = this.getNumberOfMetrics();
        //Set the metric name.
        Metric newMetric = this.addMetric(metricName);
        metric = newMetric.getID();
        newMetricCount = this.getNumberOfMetrics();

        // System.out.println("" + metricCount + ", " + newMetricCount + ",
        // " + (metric+1));
        while (metricCount < newMetricCount) {
            //Need to call increaseVectorStorage() on all objects that
            // require it.
            this.getTrialData().increaseVectorStorage();
            metricCount++;
        }
        while (thread.getNumberOfMetrics() < newMetricCount) {
            thread.incrementStorage();
        }
        while (function.getStorageSize() < newMetricCount) {
            function.incrementStorage();
        }
        while (functionProfile.getStorageSize() < newMetricCount) {
            functionProfile.incrementStorage();
        }
        // new code
        if (typeDouble) {
            // System.out.println("\t" + metricName + " " + metric);
            functionProfile.setExclusive(metric, dEventValue);
            functionProfile.setInclusive(metric, dEventValue);
            double tmpValue = dEventValue / ((double) (header3.i0));
            functionProfile.setInclusivePerCall(metric, tmpValue);
            if ((function.getMaxExclusive(metric)) < dEventValue) {
                function.setMaxExclusive(metric, dEventValue);
                function.setMaxInclusive(metric, dEventValue);
            }
            if (function.getMaxInclusivePerCall(metric) < (dEventValue / header3.i0))
                function.setMaxInclusivePerCall(metric, (dEventValue / header3.i0));
        } else {
            functionProfile.setExclusive(metric, iEventValue);
            functionProfile.setInclusive(metric, iEventValue);
            double tmpValue = iEventValue / ((double) (header3.i0));
            functionProfile.setInclusivePerCall(metric, tmpValue);
            if ((function.getMaxExclusive(metric)) < iEventValue) {
                function.setMaxExclusive(metric, iEventValue);
                function.setMaxInclusive(metric, iEventValue);
            }
            if (function.getMaxInclusivePerCall(metric) < (iEventValue / header3.i0))
                function.setMaxInclusivePerCall(metric, (iEventValue / header3.i0));
        }
        functionProfile.setExclusivePercent(metric, 0);
        functionProfile.setInclusivePercent(metric, 0);
        functionProfile.setNumCalls(header3.i0);
        functionProfile.setNumSubr(0);
        function.setMaxExclusivePercent(metric, 0.0);
        function.setMaxInclusivePercent(metric, 0.0);
        if (function.getMaxNumCalls() < header3.i0)
            function.setMaxNumCalls(header3.i0);
        function.setMaxNumSubr(0);

    }

    // Exclusive Duration lines occurr when the instrumented section has a child
    // instrumented section.

    // inclusive = false means we are processing an 'exclusive' line
    private void processTime(String string, boolean inclusive) {
        if (!initialized) {
            if (header2.s0 == null)
                header2.s0 = new String("Entire Program");
            if (header3.i0 == 0)
                header3.i0 = 1;
            initializeThread();
        } else {
            // thread.incrementStorage();
            function.incrementStorage();
            functionProfile.incrementStorage();
        }
        StringTokenizer st1 = new StringTokenizer(string, ":");
        String metricName = st1.nextToken().trim(); // hardware counter name

        metricName = "Time";

        String tmpStr = st1.nextToken().trim();
        int metricCount = 0, newMetricCount = 0;
        // need to clean stuff out of the value, like % and M and whatnot
        st1 = new StringTokenizer(tmpStr, " ");
        double dEventValue = 0.0;
        int iEventValue = 0;
        tmpStr = st1.nextToken().trim();

        try {
            dEventValue = Double.parseDouble(tmpStr); // callsite index
        } catch (NumberFormatException e) {
            dEventValue = 0;
        }

        //if (st1.hasMoreTokens())
        //metricName += " (" + st1.nextToken() + ")";
        // System.out.println(metricName + " = " + dEventValue);

        metricCount = this.getNumberOfMetrics();
        //Set the metric name.
        Metric newMetric = this.addMetric(metricName);
        metric = newMetric.getID();
        timeMetric = metric;
        newMetricCount = this.getNumberOfMetrics();

        // System.out.println("" + metricCount + ", " + newMetricCount + ",
        // " + (metric+1));
        while (metricCount < newMetricCount) {
            //Need to call increaseVectorStorage() on all objects that
            // require it.
            this.getTrialData().increaseVectorStorage();
            metricCount++;
        }
        while (thread.getNumberOfMetrics() < newMetricCount) {
            thread.incrementStorage();
        }
        while (function.getStorageSize() < newMetricCount) {
            function.incrementStorage();
        }
        while (functionProfile.getStorageSize() < newMetricCount) {
            functionProfile.incrementStorage();
        }

        dEventValue = dEventValue * 1000 * 1000;

        if (inclusive) {
            functionProfile.setInclusive(metric, dEventValue);
            //functionProfile.setExclusiveValue(metric,
            // dEventValue);
        } else {
            functionProfile.setExclusive(metric, dEventValue);
        }

        double tmpValue = dEventValue / ((double) (header3.i0));
        functionProfile.setInclusivePerCall(metric, tmpValue);

        if (inclusive) {
            if ((function.getMaxInclusive(metric)) < dEventValue) {
                function.setMaxInclusive(metric, dEventValue);
            }
        } else {
            if ((function.getMaxExclusive(metric)) < dEventValue) {
                function.setMaxExclusive(metric, dEventValue);
            }
        }

        if (function.getMaxInclusivePerCall(metric) < (dEventValue / header3.i0))
            function.setMaxInclusivePerCall(metric, (dEventValue / header3.i0));

        functionProfile.setExclusivePercent(metric, 0);
        functionProfile.setInclusivePercent(metric, 0);
        functionProfile.setNumCalls(header3.i0);
        functionProfile.setNumSubr(0);
        function.setMaxExclusivePercent(metric, 0.0);
        function.setMaxInclusivePercent(metric, 0.0);
        if (function.getMaxNumCalls() < header3.i0)
            function.setMaxNumCalls(header3.i0);
        function.setMaxNumSubr(0);

    }

    private String getMetricName(String inString) {
        String tmpString = null;
        int tmpInt = inString.indexOf("_MULTI_");

        if (tmpInt > 0) {
            //We are reading data from a multiple counter run.
            //Grab the counter name.
            tmpString = inString.substring(tmpInt + 7);
            return tmpString;
        }
        //We are not reading data from a multiple counter run.
        return tmpString;
    }

    //####################################
    //End - Private Section.
    //####################################

    //Frequently used items.
    private int metric = 0;
    private Function function = null;
    private FunctionProfile functionProfile = null;
    private Node node = null;
    private Context context = null;
    private edu.uoregon.tau.dms.dss.Thread thread = null;
    private int nodeID = -1;
    private int contextID = -1;
    private int threadID = -1;
    private int timeMetric = -1;
    private String inputString = null;
    private String s1 = null;
    private String s2 = null;
    private String tokenString;
    private String groupNamesString = null;
    private StringTokenizer genericTokenizer;
    private Vector v = null;
    private BufferedReader br = null;
    boolean initialized = false;
    Hashtable eventNames = new Hashtable();

    //Instance data.
    private LineData header1 = new LineData();
    private LineData header2 = new LineData();
    private LineData header3 = new LineData();
    private LineData header4 = new LineData();
    private LineData header5 = new LineData();
}