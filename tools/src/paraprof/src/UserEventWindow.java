/* 
   UserEventWindow.java

   Title:      ParaProf
   Author:     Robert Bell
   Description:  The container for the UserEventWindowPanel.
*/

package paraprof;

import java.util.*;
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.border.*;
import javax.swing.event.*;
import java.awt.print.*;

public class UserEventWindow extends JFrame implements ActionListener, MenuListener, Observer, ChangeListener{
  
    public UserEventWindow(){
	try{
	    setLocation(new java.awt.Point(0, 0));
	    setSize(new java.awt.Dimension(100, 100));
            
	    //Set the title indicating that there was a problem.
	    this.setTitle("Wrong constructor used");
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "UEW01");
	}
    }
  
    public UserEventWindow(ParaProfTrial trial, int mappingID, StaticMainWindowData sMWData){
	try{
	    this.mappingID = mappingID;
	    this.trial = trial;
	    this.sMWData = sMWData;

	    int windowWidth = 650;
	    int windowHeight = 550;
	    //Grab the screen size.
	    Toolkit tk = Toolkit.getDefaultToolkit();
	    Dimension screenDimension = tk.getScreenSize();
	    int screenHeight = screenDimension.height;
	    int screenWidth = screenDimension.width;
	    if(windowWidth>screenWidth)
		windowWidth = screenWidth;
	    if(windowHeight>screenHeight)
		windowHeight = screenHeight;
	    //Set the window to come up in the center of the screen.
	    int xPosition = (screenWidth - windowWidth) / 2;
	    int yPosition = (screenHeight - windowHeight) / 2;
	    setSize(new java.awt.Dimension(windowWidth, windowHeight));
	    setLocation(xPosition, yPosition);

	    //Grab the appropriate global mapping element.
	    GlobalMapping tmpGM = trial.getGlobalMapping();
	    GlobalMappingElement tmpGME = tmpGM.getGlobalMappingElement(mappingID, 2);
      
	    mappingName = tmpGME.getMappingName();
      
	    //Now set the title.
	    this.setTitle("User Event Window: " + trial.getProfilePathName());
      
	    //Add some window listener code
	    addWindowListener(new java.awt.event.WindowAdapter() {
		    public void windowClosing(java.awt.event.WindowEvent evt) {
			thisWindowClosing(evt);
		    }
		});

	    //Set the help window text if required.
	    if(ParaProf.helpWindow.isVisible()){
		this.help(false);
	    }

	    //####################################
	    //Code to generate the menus.
	    //####################################
	    JMenuBar mainMenu = new JMenuBar();

	    //File menu.
	    JMenu fileMenu = new JMenu("File");
	    UtilFncs.fileMenuItems(fileMenu, this);
      
	    //Options menu.
	    JMenu optionsMenu = new JMenu("Options");
	    optionsMenu.addMenuListener(this);
	    UtilFncs.usereventOptionMenuItems(optionsMenu,this);
 
	    //Windows menu
	    JMenu windowsMenu = new JMenu("Windows");
	    windowsMenu.addMenuListener(this);
	    UtilFncs.windowMenuItems(windowsMenu,this);

	    //Help menu.
	    JMenu helpMenu = new JMenu("Help");
	    UtilFncs.helpMenuItems(helpMenu, this);
	    
	    //Now, add all the menus to the main menu.
	    mainMenu.add(fileMenu);
	    mainMenu.add(optionsMenu);
	    mainMenu.add(windowsMenu);
	    mainMenu.add(helpMenu);
	    
	    setJMenuBar(mainMenu);
	    //####################################
	    //End - Code to generate the menus.
	    //####################################

	    //####################################
	    //Create and add the components.
	    //####################################
	    //Setting up the layout system for the main window.
	    contentPane = getContentPane();
	    gbl = new GridBagLayout();
	    contentPane.setLayout(gbl);
	    gbc = new GridBagConstraints();
	    gbc.insets = new Insets(5, 5, 5, 5);

	    //######
	    //Panel and ScrollPane definition.
	    //######
	    panel = new UserEventWindowPanel(trial, mappingID, this);
	    //The scroll panes into which the list shall be placed.
	    sp = new JScrollPane(panel);
	    //######
	    //Panel and ScrollPane definition.
	    //######

	    //######
	    //Slider setup.
	    //Do the slider stuff, but don't add.  By default, sliders are off.
	    //######
	    String sliderMultipleStrings[] = {"1.00", "0.75", "0.50", "0.25", "0.10"};
	    sliderMultiple = new JComboBox(sliderMultipleStrings);
	    sliderMultiple.addActionListener(this);
      
	    barLengthSlider.setPaintTicks(true);
	    barLengthSlider.setMajorTickSpacing(5);
	    barLengthSlider.setMinorTickSpacing(1);
	    barLengthSlider.setPaintLabels(true);
	    barLengthSlider.setSnapToTicks(true);
	    barLengthSlider.addChangeListener(this);
	    //######
	    //End - Slider setup.
	    //Do the slider stuff, but don't add.  By default, sliders are off.
	    //######
	    gbc.fill = GridBagConstraints.BOTH;
	    gbc.anchor = GridBagConstraints.CENTER;
	    gbc.weightx = 0.95;
	    gbc.weighty = 0.98;
	    addCompItem(sp, gbc, 0, 0, 1, 1);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "UEW02");
	}
    
    
    }
  
    //####################################
    //Interface code.
    //####################################

    //######
    //ActionListener.
    //######
    public void actionPerformed(ActionEvent evt){
	try{
	    Object EventSrc = evt.getSource();
      
	    if(EventSrc instanceof JMenuItem){
		String arg = evt.getActionCommand();
		
		if(arg.equals("Print")){
		    PrinterJob job = PrinterJob.getPrinterJob();
		    PageFormat defaultFormat = job.defaultPage();
		    PageFormat selectedFormat = job.pageDialog(defaultFormat);
		    job.setPrintable(panel, selectedFormat);
		    if(job.printDialog()){
			job.print();
		    }
		}
		else if(arg.equals("Edit ParaProf Preferences!")){
		    trial.getPreferences().showPreferencesWindow();
		}
		if(arg.equals("Close This Window")){
		    closeThisWindow();}
		else if(arg.equals("Exit Racy!")){
		    setVisible(false);
		    dispose();
		    System.exit(0);
		}
		else if(arg.equals("Number of Userevents")){
		    metric = 12;
		    panel.repaint();
		}
		else if(arg.equals("Min. Value")){
		    metric = 14;
		    panel.repaint();
		}
		else if(arg.equals("Max. Value")){
		    metric = 16;
		    panel.repaint();
		}
		else if(arg.equals("Mean Value")){
		    metric = 18;
		    panel.repaint();
		}
		else if(arg.equals("Decending Order")){
		    if(((JCheckBoxMenuItem)optionsMenu.getItem(0)).isSelected())
			order = 0;
		    else
			order = 1;
		    sortLocalData();
		    panel.repaint();
		}
		else if(arg.equals("Display Sliders")){
		    if(((JCheckBoxMenuItem)optionsMenu.getItem(2)).isSelected())
			displaySiders(true);
		    else
			displaySiders(false);
		}
		else if(arg.equals("Show Function Ledger")){
		    (new MappingLedgerWindow(trial, 0)).show();
		}
		else if(arg.equals("Show Group Ledger")){
		    (new MappingLedgerWindow(trial, 1)).show();
		}
		else if(arg.equals("Show User Event Ledger")){
		    (new MappingLedgerWindow(trial, 2)).show();
		}
		else if(arg.equals("Close All Sub-Windows")){
		    trial.getSystemEvents().updateRegisteredObjects("subWindowCloseEvent");
		}
		else if(arg.equals("About ParaProf")){
		    JOptionPane.showMessageDialog(this, ParaProf.getInfoString());
		}
		else if(arg.equals("Show Help Window")){
		    this.help(true);
		}
		    }
	    else if(EventSrc == sliderMultiple){
		panel.changeInMultiples();
	    }
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "UEW03");
	}
    }
    //######
    //End - ActionListener
    //######
    
    //######
    //ChangeListener.
    //######
    public void stateChanged(ChangeEvent event){
	panel.changeInMultiples();}
    //######
    //End - ChangeListener.
    //######

    //######
    //MenuListener.
    //######
    public void menuSelected(MenuEvent evt){
	try{
	    if(trial.groupNamesPresent())
		((JMenuItem)windowsMenu.getItem(1)).setEnabled(true);
	    else
		((JMenuItem)windowsMenu.getItem(1)).setEnabled(false);
	    
	    if(trial.userEventsPresent())
		((JMenuItem)windowsMenu.getItem(2)).setEnabled(true);
	    else
		((JMenuItem)windowsMenu.getItem(2)).setEnabled(false);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "UEW04");
	}
    }
    
    public void menuDeselected(MenuEvent evt){}
    public void menuCanceled(MenuEvent evt){}
    //######
    //End - MenuListener.
    //######

    //######
    //Observer.
    //######
    public void update(Observable o, Object arg){
	try{
	    String tmpString = (String) arg;
	    if(tmpString.equals("prefEvent")){
		panel.repaint();
	    }
	    else if(tmpString.equals("colorEvent")){
		panel.repaint();
	    }
	    else if(tmpString.equals("dataSetChangeEvent")){
		sortLocalData();
		panel.repaint();
	    }
	    else if(tmpString.equals("subWindowCloseEvent")){
		closeThisWindow();
	    }
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "UEW05");
	}
    }
    //######
    //End - Observer.
    //######

    //####################################
    //End - Interface code.
    //####################################

    private void help(boolean display){
	//Show the ParaProf help window.
	ParaProf.helpWindow.clearText();
	if(display)
	    ParaProf.helpWindow.show();
	ParaProf.helpWindow.writeText("This is the userevent data window for:");
        ParaProf.helpWindow.writeText(mappingName);
        ParaProf.helpWindow.writeText("");
        ParaProf.helpWindow.writeText("This window shows you this userevent's statistics across all the threads.");
        ParaProf.helpWindow.writeText("");
        ParaProf.helpWindow.writeText("Use the options menu to select different ways of displaying the data.");
        ParaProf.helpWindow.writeText("");
        ParaProf.helpWindow.writeText("Right click anywhere within this window to bring up a popup");
        ParaProf.helpWindow.writeText("menu. In this menu you can change or reset the default colour");
        ParaProf.helpWindow.writeText("for this userevent.");
    }

    public void sortLocalData(){
	try{
	    list = sMWData.getMappingData(mappingID, 1, metric+order);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "MDW06");
	}
    }
    
    public Vector getData(){
	return list;}
    
    public int getMetric(){
	return metric;}

    public int getSliderValue(){
	int tmpInt = -1;
    	try{
	    tmpInt = barLengthSlider.getValue();
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "MDW07");
	}
	return tmpInt;
    }
  
    public double getSliderMultiple(){
	String tmpString = null;
	try{
	    tmpString = (String) sliderMultiple.getSelectedItem();
	    return Double.parseDouble(tmpString);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "MDW08");
	}
	return 0;
    }
        
    private void displaySiders(boolean displaySliders){
	if(displaySliders){
	    //Since the menu option is a toggle, the only component that needs to be
	    //removed is that scrollPane.  We then add back in with new parameters.
	    //This might not be required as it seems to adjust well if left in, but just
	    //to be sure.
	    contentPane.remove(sp);
	    
	    gbc.fill = GridBagConstraints.NONE;
	    gbc.anchor = GridBagConstraints.EAST;
	    gbc.weightx = 0.10;
	    gbc.weighty = 0.01;
	    addCompItem(sliderMultipleLabel, gbc, 0, 0, 1, 1);
	    
	    gbc.fill = GridBagConstraints.NONE;
	    gbc.anchor = GridBagConstraints.WEST;
	    gbc.weightx = 0.10;
	    gbc.weighty = 0.01;
	    addCompItem(sliderMultiple, gbc, 1, 0, 1, 1);
	    
	    gbc.fill = GridBagConstraints.NONE;
	    gbc.anchor = GridBagConstraints.EAST;
	    gbc.weightx = 0.10;
	    gbc.weighty = 0.01;
	    addCompItem(barLengthLabel, gbc, 2, 0, 1, 1);
	    
	    gbc.fill = GridBagConstraints.HORIZONTAL;
	    gbc.anchor = GridBagConstraints.WEST;
	    gbc.weightx = 0.70;
	    gbc.weighty = 0.01;
	    addCompItem(barLengthSlider, gbc, 3, 0, 1, 1);
	    
	    gbc.fill = GridBagConstraints.BOTH;
	    gbc.anchor = GridBagConstraints.CENTER;
	    gbc.weightx = 0.95;
	    gbc.weighty = 0.98;
	    addCompItem(sp, gbc, 0, 1, 4, 1);
	}
	else{
	    contentPane.remove(sliderMultipleLabel);
	    contentPane.remove(sliderMultiple);
	    contentPane.remove(barLengthLabel);
	    contentPane.remove(barLengthSlider);
	    contentPane.remove(sp);
	    
	    gbc.fill = GridBagConstraints.BOTH;
	    gbc.anchor = GridBagConstraints.CENTER;
	    gbc.weightx = 0.95;
	    gbc.weighty = 0.98;
	    addCompItem(sp, gbc, 0, 0, 1, 1);
	}
	
	//Now call validate so that these componant changes are displayed.
	validate();
    }

    private void addCompItem(Component c, GridBagConstraints gbc, int x, int y, int w, int h){
	try{
	    gbc.gridx = x;
	    gbc.gridy = y;
	    gbc.gridwidth = w;
	    gbc.gridheight = h;
	    
	    getContentPane().add(c, gbc);
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "MDW09");
	}
    }
  
    //Respond correctly when this window is closed.
    void thisWindowClosing(java.awt.event.WindowEvent e){
	closeThisWindow();}
  
    void closeThisWindow(){
	try{
	    if(ParaProf.debugIsOn){
		System.out.println("------------------------");
		System.out.println("A mapping window for: \"" + mappingName + "\" is closing");
		System.out.println("Clearing resourses for that window.");
	    }
	    setVisible(false);
	    trial.getSystemEvents().deleteObserver(this);
	    dispose();
	}
	catch(Exception e){
	    ParaProf.systemError(e, null, "MDW10");
	}
    }
  
    //####################################
    //Instance data.
    //####################################
    private ParaProfTrial trial = null;
    private StaticMainWindowData sMWData = null;
    Vector sMWGeneralData = null;
    private int mappingID = -1;
    private String mappingName = null;

    private JMenu optionsMenu = null;
    private JMenu windowsMenu = null;

    private JLabel sliderMultipleLabel = new JLabel("Slider Mulitiple");
    private JComboBox sliderMultiple;
    private JLabel barLengthLabel = new JLabel("Bar Mulitiple");
    private JSlider barLengthSlider = new JSlider(0, 40, 1);
  
    private Container contentPane = null;
    private GridBagLayout gbl = null;
    private GridBagConstraints gbc = null;
  
    UserEventWindowPanel panel = null;
    JScrollPane sp = null;
    JLabel label = null;

    private Vector list = null;
    
    private int order = 0; //0: descending order,1: ascending order.
    private int metric = 10; //12-number of userevents,14-min,16-max,18-mean.
    //####################################
    //End - Instance data.
    //####################################
}
