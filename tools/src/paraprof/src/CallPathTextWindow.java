/* 
   CallPathTextWindow.java

   Title:      ParaProf
   Author:     Robert Bell
   Description:  
*/

package paraprof;

import java.util.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;

public class CallPathTextWindow extends JFrame implements ActionListener, MenuListener, Observer{

    public CallPathTextWindow(){
	try{
	    setLocation(new java.awt.Point(0, 0));
	    setSize(new java.awt.Dimension(800, 600));
      
	    //Set the title indicating that there was a problem.
	    this.setTitle("Wrong constructor used!");
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW01");
	}
    }
  
    public CallPathTextWindow(Trial inTrial,
			      int inServerNumber,
			      int inContextNumber,
			      int inThreadNumber,
			      StaticMainWindowData inSMWData,
			      boolean global){
	try{
	    trial = inTrial;
	    sMWData = inSMWData;
	    this.global = global;
      
	    setLocation(new java.awt.Point(0, 0));
	    setSize(new java.awt.Dimension(800, 600));
      
	    //Now set the title.
	    this.setTitle("Call Path Data " + "n,c,t, " + inServerNumber + "," + inContextNumber + "," + inThreadNumber + " - " + trial.getProfilePathName());
      
	    server = inServerNumber;
	    context = inContextNumber;
	    thread = inThreadNumber;
      
	    //Add some window listener code
	    addWindowListener(new java.awt.event.WindowAdapter() {
		    public void windowClosing(java.awt.event.WindowEvent evt) {
			thisWindowClosing(evt);
		    }
		});
        
	    //Set the help window text if required.
	    if(ParaProf.helpWindow.isVisible())
		{
		    ParaProf.helpWindow.clearText();
		    //Since the data must have been loaded.  Tell them someting about
		    //where they are.
		    ParaProf.helpWindow.writeText("This is the total function statistics window.");
		    ParaProf.helpWindow.writeText("");
		    ParaProf.helpWindow.writeText("This window shows you the total statistics for all functions on this thread.");
		    ParaProf.helpWindow.writeText("");
		    ParaProf.helpWindow.writeText("Use the options menu to select different ways of displaying the data.");
		    ParaProf.helpWindow.writeText("");
		    ParaProf.helpWindow.writeText("Right click on any function within this window to bring up a popup");
		    ParaProf.helpWindow.writeText("menu. In this menu you can change or reset the default colour");
		    ParaProf.helpWindow.writeText("for the function, or to show more details about the function.");
		    ParaProf.helpWindow.writeText("You can also left click any function to hightlight it in the system.");
		}
      
      
	    //******************************
	    //Code to generate the menus.
	    //******************************
      
      
	    JMenuBar mainMenu = new JMenuBar();
      
	    //******************************
	    //File menu.
	    //******************************
	    JMenu fileMenu = new JMenu("File");
      
	    //Add a menu item.
	    JMenuItem closeItem = new JMenuItem("Close This Window");
	    closeItem.addActionListener(this);
	    fileMenu.add(closeItem);
      
	    //Add a menu item.
	    JMenuItem exitItem = new JMenuItem("Exit ParaProf!");
	    exitItem.addActionListener(this);
	    fileMenu.add(exitItem);
	    //******************************
	    //End - File menu.
	    //******************************
      
	    //******************************
	    //Window menu.
	    //******************************
	    JMenu windowsMenu = new JMenu("Windows");
	    windowsMenu.addMenuListener(this);
      
	    //Add a submenu.
	    JMenuItem mappingLedgerItem = new JMenuItem("Show Function Ledger");
	    mappingLedgerItem.addActionListener(this);
	    windowsMenu.add(mappingLedgerItem);
      
	    //Add a submenu.
	    mappingGroupLedgerItem = new JMenuItem("Show Group Ledger");
	    mappingGroupLedgerItem.addActionListener(this);
	    windowsMenu.add(mappingGroupLedgerItem);
      
	    //Add a submenu.
	    userEventLedgerItem = new JMenuItem("Show User Event Ledger");
	    userEventLedgerItem.addActionListener(this);
	    windowsMenu.add(userEventLedgerItem);
      
	    //Add a submenu.
	    JMenuItem closeAllSubwindowsItem = new JMenuItem("Close All Sub-Windows");
	    closeAllSubwindowsItem.addActionListener(this);
	    windowsMenu.add(closeAllSubwindowsItem);
	    //******************************
	    //End - Window menu.
	    //******************************
      
      
	    //******************************
	    //Help menu.
	    //******************************
	    JMenu helpMenu = new JMenu("Help");
      
	    //Add a menu item.
	    JMenuItem aboutItem = new JMenuItem("About ParaProf");
	    aboutItem.addActionListener(this);
	    helpMenu.add(aboutItem);
      
	    //Add a menu item.
	    JMenuItem showHelpWindowItem = new JMenuItem("Show Help Window");
	    showHelpWindowItem.addActionListener(this);
	    helpMenu.add(showHelpWindowItem);
	    //******************************
	    //End - Help menu.
	    //******************************
      
      
	    //Now, add all the menus to the main menu.
	    mainMenu.add(fileMenu);
	    mainMenu.add(windowsMenu);
	    mainMenu.add(helpMenu);
      
	    setJMenuBar(mainMenu);
      
	    //******************************
	    //End - Code to generate the menus.
	    //******************************
      
      
	    //******************************
	    //Create and add the componants.
	    //******************************
	    //Setting up the layout system for the main window.
	    Container contentPane = getContentPane();
	    GridBagLayout gbl = new GridBagLayout();
	    contentPane.setLayout(gbl);
	    GridBagConstraints gbc = new GridBagConstraints();
	    gbc.insets = new Insets(5, 5, 5, 5);
      
	    //Create some borders.
	    Border mainloweredbev = BorderFactory.createLoweredBevelBorder();
	    Border mainraisedbev = BorderFactory.createRaisedBevelBorder();
	    Border mainempty = BorderFactory.createEmptyBorder();
      
	    //**********
	    //Panel and ScrollPane definition.
	    //**********
	    panelRef = new CallPathTextWindowPanel(trial,
						   inServerNumber,
						   inContextNumber,
						   inThreadNumber, this, global);
      
	    //The scroll panes into which the list shall be placed.
	    JScrollPane sp = new JScrollPane(panelRef);
	    JScrollBar vScollBar = sp.getVerticalScrollBar();
	    vScollBar.setUnitIncrement(35);
	    sp.setBorder(mainloweredbev);
	    sp.setPreferredSize(new Dimension(500, 450));
	    //**********
	    //End - Panel and ScrollPane definition.
	    //**********
      
	    //Now add the componants to the main screen.
	    gbc.fill = GridBagConstraints.BOTH;
	    gbc.anchor = GridBagConstraints.CENTER;
	    gbc.weightx = 1;
	    gbc.weighty = 1;
	    addCompItem(sp, gbc, 0, 0, 1, 1);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW02");
	}
    }
  
    //******************************
    //Event listener code!!
    //******************************
  
    //ActionListener code.
    public void actionPerformed(ActionEvent evt)
    {
	try{
	    Object EventSrc = evt.getSource();
      
	    if(EventSrc instanceof JMenuItem)
		{
		    String arg = evt.getActionCommand();

		    if(arg.equals("Close This Window"))
			{
			    closeThisWindow();
			}
		    else if(arg.equals("Exit ParaProf!"))
			{
			    setVisible(false);
			    dispose();
			    System.exit(0);
			}
		    else if(arg.equals("Show Function Ledger"))
			{
			    //In order to be in this window, I must have loaded the data. So,
			    //just show the mapping ledger window.
			    (trial.getGlobalMapping()).displayMappingLedger(0);
			}
		    else if(arg.equals("Show Group Ledger"))
			{
			    //In order to be in this window, I must have loaded the data. So,
			    //just show the mapping ledger window.
			    (trial.getGlobalMapping()).displayMappingLedger(1);
			}
		    else if(arg.equals("Show User Event Ledger"))
			{
			    //In order to be in this window, I must have loaded the data. So,
			    //just show the mapping ledger window.
			    (trial.getGlobalMapping()).displayMappingLedger(2);
			}
		    else if(arg.equals("Close All Sub-Windows"))
			{
			    //Close the all subwindows.
			    trial.getSystemEvents().updateRegisteredObjects("subWindowCloseEvent");
			}
		    else if(arg.equals("About ParaProf"))
			{
			    JOptionPane.showMessageDialog(this, ParaProf.getInfoString());
			}
		    else if(arg.equals("Show Help Window"))
			{
			    //Show the racy help window.
			    ParaProf.helpWindow.clearText();
			    ParaProf.helpWindow.show();
			    //Since the data must have been loaded.  Tell them someting about
			    //where they are.
			    ParaProf.helpWindow.writeText("This is the total function statistics window.");
			    ParaProf.helpWindow.writeText("");
			    ParaProf.helpWindow.writeText("This window shows you the total statistics for all functions on this thread.");
			    ParaProf.helpWindow.writeText("");
			    ParaProf.helpWindow.writeText("Use the options menu to select different ways of displaying the data.");
			    ParaProf.helpWindow.writeText("");
			    ParaProf.helpWindow.writeText("Right click on any function within this window to bring up a popup");
			    ParaProf.helpWindow.writeText("menu. In this menu you can change or reset the default colour");
			    ParaProf.helpWindow.writeText("for the function, or to show more details about the function.");
			    ParaProf.helpWindow.writeText("You can also left click any function to hightlight it in the system.");
			}
		}
	}
	catch(Exception e)
	    {
		ParaProf.systemError(e, null, "CPTW03");
	    }
    }
  
    //******************************
    //MenuListener code.
    //******************************
    public void menuSelected(MenuEvent evt){
	try{
	    if(trial.groupNamesPresent())
		mappingGroupLedgerItem.setEnabled(true);
	    else
		mappingGroupLedgerItem.setEnabled(false);
	    
	    if(trial.userEventsPresent())
		userEventLedgerItem.setEnabled(true);
	    else{
		userEventLedgerItem.setEnabled(false);
	    }
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW03");
	}
	
    }
    public void menuDeselected(MenuEvent evt){}
    public void menuCanceled(MenuEvent evt){}
    //******************************
    //End - MenuListener code.
    //******************************
  
    //Observer functions.
    public void update(Observable o, Object arg){
	try{
	    String tmpString = (String) arg;
	    if(tmpString.equals("prefEvent")){
		panelRef.repaint();
	    }
	    if(tmpString.equals("colorEvent")){
		panelRef.repaint();
	    }
	    else if(tmpString.equals("dataEvent")){ 
		panelRef.repaint();
	    }
	    else if(tmpString.equals("subWindowCloseEvent")){ 
		closeThisWindow();
	    }
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW04");
	}
    } 
  
    //Helper functions.
    private void addCompItem(Component c, GridBagConstraints gbc, int x, int y, int w, int h){
	try{
	    gbc.gridx = x;
	    gbc.gridy = y;
	    gbc.gridwidth = w;
	    gbc.gridheight = h;
	    
	    getContentPane().add(c, gbc);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW05");
	}
    }
  
    //Respond correctly when this window is closed.
    void thisWindowClosing(java.awt.event.WindowEvent e){
	closeThisWindow();
    }
  
    void closeThisWindow(){ 
	try{
	    if(ParaProf.debugIsOn){
		System.out.println("------------------------");
		System.out.println("A total stat window for: \"" + "n,c,t, " + server + "," + context + "," + thread + "\" is closing");
		System.out.println("Clearing resourses for this window.");
	    }
      
	    setVisible(false);
	    trial.getSystemEvents().deleteObserver(this);
	    dispose();
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "CPTW06");
	}
    }
  
    //******************************
    //Instance data.
    //******************************
    private Trial trial = null;
    private CallPathTextWindowPanel panelRef;
    private StaticMainWindowData sMWData;
    private boolean global = false;
  
    private JMenuItem mappingGroupLedgerItem;
    private JMenuItem userEventLedgerItem;

    int server;
    int context;
    int thread;
    //******************************
    //End - Instance data.
    //******************************





}
