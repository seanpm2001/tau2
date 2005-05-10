package edu.uoregon.tau.paraprof;

import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.*;

import javax.swing.*;
import javax.swing.event.MenuEvent;
import javax.swing.event.MenuListener;

import edu.uoregon.tau.dms.dss.Function;
import edu.uoregon.tau.dms.dss.Group;
import edu.uoregon.tau.dms.dss.UserEvent;
import edu.uoregon.tau.paraprof.interfaces.ParaProfWindow;

/**
 * LedgerWindow
 * This object represents the ledger window.
 *  
 * <P>CVS $Id: LedgerWindow.java,v 1.15 2005/05/10 01:48:38 amorris Exp $</P>
 * @author	Robert Bell, Alan Morris
 * @version	$Revision: 1.15 $
 * @see		LedgerDataElement
 * @see		LedgerWindowPanel
 */
public class LedgerWindow extends JFrame implements Observer, ParaProfWindow {

    public static final int FUNCTION_LEDGER = 0;
    public static final int GROUP_LEDGER = 1;
    public static final int USEREVENT_LEDGER = 2;
    private int windowType = -1; //0:function, 1:group, 2:userevent.

    private ParaProfTrial trial = null;
    private JScrollPane sp = null;
    private LedgerWindowPanel panel = null;
    private Vector list = new Vector();
    
    public void setupMenus() {
        JMenuBar mainMenu = new JMenuBar();

        mainMenu.add(ParaProfUtils.createFileMenu(this, panel, panel));
        //mainMenu.add(ParaProfUtils.createTrialMenu(trial, this));
        mainMenu.add(ParaProfUtils.createWindowsMenu(trial, this));
        mainMenu.add(ParaProfUtils.createHelpMenu(this, this));

        setJMenuBar(mainMenu);
    }

    public LedgerWindow(ParaProfTrial trial, int windowType) {
        this.trial = trial;
        this.windowType = windowType;

        setLocation(new java.awt.Point(300, 200));
        setSize(new java.awt.Dimension(350, 450));

        //Now set the title.
        switch (windowType) {
        case FUNCTION_LEDGER:
            this.setTitle("Function Ledger Window: " + trial.getTrialIdentifier(true));
            break;
        case GROUP_LEDGER:
            this.setTitle("Group Ledger Window: " + trial.getTrialIdentifier(true));
            break;
        case USEREVENT_LEDGER:
            this.setTitle("User Event Window: " + trial.getTrialIdentifier(true));
            break;
        default:
            throw new ParaProfException("Invalid Ledger Window Type: " + windowType);
        }

        //Add some window listener code
        addWindowListener(new java.awt.event.WindowAdapter() {
            public void windowClosing(java.awt.event.WindowEvent evt) {
                thisWindowClosing(evt);
            }
        });

        //Set the help window text if required.
        if (ParaProf.helpWindow.isVisible()) {
            this.help(false);
        }

       

        //Sort the local data.
        sortLocalData();
        
        getContentPane().setLayout(new GridBagLayout());

        GridBagConstraints gbc = new GridBagConstraints();
        gbc.insets = new Insets(5, 5, 5, 5);

        //Panel and ScrollPane definition.
        panel = new LedgerWindowPanel(trial, this, windowType);
        sp = new JScrollPane(panel);
        JScrollBar vScrollBar = sp.getVerticalScrollBar();
        vScrollBar.setUnitIncrement(35);

        setupMenus();
        
        gbc.fill = GridBagConstraints.BOTH;
        gbc.anchor = GridBagConstraints.CENTER;
        gbc.weightx = 1;
        gbc.weighty = 1;
        addCompItem(sp, gbc, 0, 0, 1, 1);

        
        trial.getSystemEvents().addObserver(this);
        ParaProf.incrementNumWindows();
    }


    public void update(Observable o, Object arg) {
            String tmpString = (String) arg;
            if (tmpString.equals("prefEvent")) {
                panel.repaint();
            } else if (tmpString.equals("colorEvent")) {
                panel.repaint();
            } else if (tmpString.equals("dataEvent")) {
                sortLocalData();
                panel.repaint();
            } else if (tmpString.equals("subWindowCloseEvent")) {
                closeThisWindow();
            }
    }

    public void help(boolean display) {
        ParaProf.helpWindow.clearText();
        if (display)
            ParaProf.helpWindow.show();
        if (windowType == 0) {
            ParaProf.helpWindow.writeText("This is the function ledger window.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("This window shows all the functionProfiles tracked in this profile.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("To see more information about any of the functionProfiles shown here,");
            ParaProf.helpWindow.writeText("right click on that function, and select from the popup menu.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("You can also left click any function to highlight it in the system.");
        } else if (windowType == 1) {
            ParaProf.helpWindow.writeText("This is the group ledger window.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("This window shows all the groups tracked in this profile.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("Left click any group to highlight it in the system.");
            ParaProf.helpWindow.writeText("Right click on any group, and select from the popup menu"
                    + " to display more options for masking or displaying functionProfiles in a particular group.");
        } else {
            ParaProf.helpWindow.writeText("This is the user event ledger window.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("This window shows all the user events tracked in this profile.");
            ParaProf.helpWindow.writeText("");
            ParaProf.helpWindow.writeText("Left click any user event to highlight it in the system.");
            ParaProf.helpWindow.writeText("Right click on any user event, and select from the popup menu.");
        }
    }

    //Updates this window's data copy.
    private void sortLocalData() {
        list = new Vector();
        if (this.windowType == FUNCTION_LEDGER) {
            for (Iterator it = trial.getDataSource().getFunctions(); it.hasNext();) {
                list.addElement(new LedgerDataElement((Function) it.next()));
            }
        } else if (this.windowType == GROUP_LEDGER) {
            for (Iterator it = trial.getDataSource().getGroups(); it.hasNext();) {
                list.addElement(new LedgerDataElement((Group) it.next()));
            }
        } else if (this.windowType == USEREVENT_LEDGER) {
            for (Iterator it = trial.getDataSource().getUserEvents(); it.hasNext();) {
                list.addElement(new LedgerDataElement((UserEvent) it.next()));
            }
        }
    }

    public Vector getData() {
        return list;
    }

    private void addCompItem(Component c, GridBagConstraints gbc, int x, int y, int w, int h) {
            gbc.gridx = x;
            gbc.gridy = y;
            gbc.gridwidth = w;
            gbc.gridheight = h;
            getContentPane().add(c, gbc);
    }

    public Rectangle getViewRect() {
        return sp.getViewport().getViewRect();
    }

    //Respond correctly when this window is closed.
    private void thisWindowClosing(java.awt.event.WindowEvent e) {
        closeThisWindow();
    }

    public void closeThisWindow() {
        try {
            setVisible(false);
            trial.getSystemEvents().deleteObserver(this);
            ParaProf.decrementNumWindows();
        } catch (Exception e) {
            // do nothing
        }
        dispose();
    }

   
}