/* 
  CallPathUtilFuncs.java

  Title:      ParaProf
  Author:     Robert Bell
  Description:  
*/

package paraprof;

import java.util.*;

public class CallPathUtilFuncs{
    public CallPathUtilFuncs(){}

    public static boolean isAvailable(ListIterator l){
	GlobalMappingElement gme = null;
	String s = null;
	boolean result = false;
	while(l.hasNext()){
	    gme = (GlobalMappingElement) l.next();
	    s = gme.getMappingName();
	    if(s!=null){
		if(s.indexOf("=>")>0){
		    gme.setCallPathObject(true);
		    result = true;
		}
	    }
	}
	return result;
    }

    public static void buildRelations(GlobalMapping gm){
	try{
	    GlobalMappingElement gme1 = null;
	    GlobalMappingElement gme2 = null;
	    GlobalMappingElement gme3 = null;
	    String s = null;
	    String parent = null;
	    String child = null;
	    int location = -1;
	    ParaProfIterator l = null;
	    
	    l = (ParaProfIterator)gm.getMappingIterator(0);
	    while(l.hasNext()){
		gme1 = (GlobalMappingElement) l.next();
		s = gme1.getMappingName();
		location = s.lastIndexOf("=>");
		if(location>0){
		    child = s.substring(location+3,(s.length()-2));
		    s = s.substring(0,location-1);
		    location = s.lastIndexOf("=>");
		    if(location>0){
			parent = s.substring(location+3); 
		    }
		    else
			parent = s;
		    //Update parent/child relationships.
		    gme2 = gm.getGlobalMappingElement(parent,0); 
		    gme3 = gm.getGlobalMappingElement(child,0);
		    gme2.addChild(gme3.getGlobalID(),gme1.getGlobalID());
		    gme3.addParent(gme2.getGlobalID(),gme1.getGlobalID());
		}
	    }
	}
	catch(Exception e){
	    UtilFncs.systemError(e, null, "CPUF01");
	}
    }

    public static void trimCallPathData(GlobalMapping gm, Thread thread){
	ListIterator l1 = null;
	ListIterator l2 = null;
	ListIterator l3 = null;
	GlobalMappingElement gme = null;
	Integer listValue = null;
	Vector functionList = null;
	GlobalThreadDataElement gtde = null;

	try{
	    //Create a pruned list from the global list.
	    //Want to grab a reference to the global list as
	    //this list contains null references for mappings
	    //which do not exist. Makes lookup much faster.
	    functionList = thread.getFunctionList();
	    
	    //Check to make sure that we have not trimmed before.
	    if(thread.trimmed())
		return;
	    
	    l1 = gm.getMappingIterator(0);
	    while(l1.hasNext()){
		gme = (GlobalMappingElement) l1.next();
		if((gme.getGlobalID())<(functionList.size())){ 
		    gtde = (GlobalThreadDataElement) functionList.elementAt(gme.getGlobalID());
		    if((!(gme.isCallPathObject())) && (gtde!=null)){
			l2 = gme.getParentsIterator();
			while(l2.hasNext()){
			    listValue = (Integer)l2.next();
			    if(((listValue.intValue())<(functionList.size()))&&(functionList.elementAt(listValue.intValue())!=null)){
				int location = gtde.addParent(listValue.intValue());
				l3 = gme.getCallPathIDParents(listValue.intValue());
				while(l3.hasNext()){
				    int pathID = ((Integer)l3.next()).intValue();
				    if((pathID<functionList.size())&&(functionList.elementAt(pathID)!=null))
					gtde.addParentCallPathID(location, pathID);
				}
			    }
			}
			l2 = gme.getChildrenIterator();
			while(l2.hasNext()){
			    listValue = (Integer)l2.next();
			    if(((listValue.intValue())<(functionList.size()))&&(functionList.elementAt(listValue.intValue())!=null)){
				int location = gtde.addChild(listValue.intValue());
				l3 = gme.getCallPathIDChildren(listValue.intValue());
				while(l3.hasNext()){
				    int pathID = ((Integer)l3.next()).intValue();
				    if((pathID<functionList.size())&&(functionList.elementAt(pathID)!=null))
					gtde.addChildCallPathID(location, pathID);
				}
			    }
			}
		    }
		}
		
	    }
	    
	    //Set this thread to indicate that it has been trimmed.
	    thread.setTrimmed(true);
	}
	catch(Exception e){
	    //Print out the current state of this function.
	    /*
	    System.out.println("######");
	    System.out.println("gme:");
	    if(gme!=null){
		System.out.println("name:" + gme.getMappingName());
		System.out.println("id:" + gme.getGlobalID());
	    }
	    else
		System.out.println("gme is null");
	    System.out.println("gtde:");
	    if(gtde!=null){
		System.out.println("name:" + gtde.getMappingName());
		System.out.println("id:" + gtde.getMappingID());
	    }
	    else
		System.out.println("gtde is null");
	    System.out.println("listValue:");
	    if(listValue!=null)
		System.out.println("value:" + listValue.intValue());
	    else
		System.out.println("listValue is null");

	    e.printStackTrace();
	    */
	    UtilFncs.systemError(e, null, "CPUF02");
	}
    }
}
