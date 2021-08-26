#include <guichan.hpp>
#include <SDL/SDL_ttf.h>
#include <guichan/sdl.hpp>
#include "sdltruetypefont.hpp"
#include "SelectorEntry.hpp"
#include "UaeRadioButton.hpp"
#include "UaeDropDown.hpp"
#include "UaeCheckBox.hpp"

#include "sysconfig.h"
#include "sysdeps.h"
#include "config.h"
#include "options.h"
#include "memory.h"
#include "disk.h"
#include "uae.h"
#include "autoconf.h"
#include "filesys.h"
#include "blkdev.h"
#include "gui.h"
#include "gui_handling.h"


static gcn::Label *lblModel;
static gcn::UaeDropDown* cboModel;
static gcn::Label *lblConfig;
static gcn::UaeDropDown* cboConfig;
static gcn::UaeCheckBox* chkNTSC;

static gcn::UaeCheckBox* chkDFx[2];
static gcn::UaeCheckBox* chkDFxWriteProtect[2];
static gcn::Button* cmdDFxInfo[2];
static gcn::Button* cmdDFxEject[2];
static gcn::Button* cmdDFxSelect[2];
static gcn::UaeDropDown* cboDFxFile[2];

static gcn::UaeCheckBox* chkCD;
static gcn::Button* cmdCDEject;
static gcn::Button* cmdCDSelect;
static gcn::UaeDropDown* cboCDFile;

static gcn::UaeCheckBox* chkQuickstartMode;


struct amigamodels {
	int compalevels;
	char name[32];
	char configs[8][128];
};
static struct amigamodels amodels[] = {
	{ 4, "Amiga 500", { 
	     "1.3 ROM, OCS, 512 KB Chip + 512 KB Slow RAM (most common)", 
	     "1.3 ROM, ECS Agnus, 512 KB Chip RAM + 512 KB Slow RAM",
	     "1.3 ROM, ECS Agnus, 1 MB Chip RAM",
	     "1.3 ROM, OCS Agnus, 512 KB Chip RAM",
	     "1.2 ROM, OCS Agnus, 512 KB Chip RAM",
	     "1.2 ROM, OCS Agnus, 512 KB Chip RAM + 512 KB Slow RAM",
	     "\0" } },
	{ 4, "Amiga 500+", { 
	     "Basic non-expanded configuration",
	     "2 MB Chip RAM expanded configuration",
	     "MB Fast RAM expanded configuration\n",
	     "\0" } },
	{ 4, "Amiga 600", { 
	     "Basic non-expanded configuration",
	     "2 MB Chip RAM expanded configuration",
	     "n MB Fast RAM expanded configuration",
	     "\0" } },
	{ 4, "Amiga 1200", {
	      "Basic non-expanded configuration",
	      "4 MB Fast RAM expanded configuration",
	      "\0" } },
//	{ 2, "Amiga 3000", { 
//       "1.4 ROM, 2MB Chip + 8MB Fast",
//       "2.04 ROM, 2MB Chip + 8MB Fast",
//       "3.1 ROM, 2MB Chip + 8MB Fast",
//       "\0" } },
	{ 1, "Amiga 4000", {
       "68030, 3.1 ROM, 2MB Chip + 8MB Fast",
       "68040, 3.1 ROM, 2MB Chip + 8MB Fast",
       "\0" } },
	{ 3, "CD32", { 
	     "CD32", 
	     "CD32 with Full Motion Video cartridge",
	     "\0" } },
	{ 3, "CDTV", { 
	     "CDTV",
	     "\0" } },
	{ -1 }
};

static const int numModels = 7;
static int numModelConfigs = 0;
static bool bIgnoreListChange = true;


static const char *diskfile_filter[] = { ".adf", ".adz", ".fdi", ".ipf", ".zip", ".dms", ".gz", ".xz", "\0" };
static const char *cdfile_filter[] = { ".cue", ".ccd", ".iso", "\0" };

static void AdjustDropDownControls(void);

static void CountModelConfigs(void)
{
  numModelConfigs = 0;
  if(quickstart_model >= 0 && quickstart_model < numModels) {
    for(int i=0; i<8; ++i) {
      if(amodels[quickstart_model].configs[i][0] == '\0')
        break;
      ++numModelConfigs;
    }
  }
}


static void SetControlState(int model)
{
  bool df1Visible = true;
  bool cdVisible = false;
  bool df0Editable = true;
  
  switch(model) {
    case 0: // A500
    case 1: // A500+
    case 2: // A600
    case 3: // A1200
    case 4: // A4000
      break;
    
    case 5: // CD32
    case 6: // CDTV
      // No floppy drive available, CD available
      df0Editable = false;
      df1Visible = false;
      cdVisible = true;
      break;
  }

  chkDFxWriteProtect[0]->setEnabled(df0Editable && !changed_prefs.floppy_read_only);
  cmdDFxInfo[0]->setEnabled(df0Editable);
  cmdDFxEject[0]->setEnabled(df0Editable);
  cmdDFxSelect[0]->setEnabled(df0Editable);
  cboDFxFile[0]->setEnabled(df0Editable);

  chkDFx[1]->setVisible(df1Visible);
  chkDFxWriteProtect[1]->setVisible(df1Visible);
  cmdDFxInfo[1]->setVisible(df1Visible);
  cmdDFxEject[1]->setVisible(df1Visible);
  cmdDFxSelect[1]->setVisible(df1Visible);
  cboDFxFile[1]->setVisible(df1Visible);

  chkCD->setVisible(cdVisible);
  cmdCDEject->setVisible(cdVisible);
  cmdCDSelect->setVisible(cdVisible);
  cboCDFile->setVisible(cdVisible);
}


static void AdjustPrefs(void)
{
  int old_cs = changed_prefs.cs_compatible;
  
  //if (quickstart_model > 0)
    built_in_prefs (&changed_prefs, quickstart_model, quickstart_conf, 0, 0);
  switch(quickstart_model) {
    case 0: // A500
    case 1: // A500+
    case 2: // A600
    case 3: // A1200
    case 4: // A4000
      // df0 always active
      changed_prefs.floppyslots[0].dfxtype = DRV_35_DD;
      
      // No CD available
      changed_prefs.cdslots[0].inuse = false;
      changed_prefs.cdslots[0].type = SCSI_UNIT_DISABLED;
      break;
    
    case 5: // CD32
    case 6: // CDTV
      // No floppy drive available, CD available
      changed_prefs.floppyslots[0].dfxtype = DRV_NONE;
      changed_prefs.floppyslots[1].dfxtype = DRV_NONE;
      changed_prefs.cdslots[0].inuse = true;
      changed_prefs.cdslots[0].type = SCSI_UNIT_IMAGE;
      break;
  }
  
  //if(emulating && old_cs != changed_prefs.cs_compatible)
  //  uae_restart(-1, NULL);
}


static void SetModelFromConfig(void)
{
  switch(changed_prefs.cs_compatible) {
    case CP_A500:
      quickstart_model = 0;
      if(changed_prefs.chipset_mask == 0)
        quickstart_conf = 0;
      else if(changed_prefs.chipmem_size == 0x100000)
        quickstart_conf = 2;
      else
        quickstart_conf = 1;
      break;
    
    case CP_A500P:
      quickstart_model = 1;
      if(changed_prefs.chipmem_size == 0x200000)
        quickstart_conf = 1;
      else if(changed_prefs.fastmem[0].size == 0x400000)
        quickstart_conf = 2;
      else
        quickstart_conf = 1;
      break;
      
    case CP_A600:
      quickstart_model = 2;
      if(changed_prefs.chipmem_size == 0x200000)
        quickstart_conf = 1;
      else if(changed_prefs.fastmem[0].size == 0x400000)
        quickstart_conf = 2;
      else
        quickstart_conf = 1;
      break;
      
    case CP_A1200:
      quickstart_model = 3;
      if(changed_prefs.fastmem[0].size == 0x400000)
        quickstart_conf = 1;
      else
        quickstart_conf = 0;
      break;
      
    case CP_A4000:
      quickstart_model = 4;
      if(changed_prefs.cpu_model == 68040)
        quickstart_conf = 1;
      else
        quickstart_conf = 0;
      break;
      
    case CP_CD32:
      quickstart_model = 5;
      if(changed_prefs.cs_cd32fmv)
        quickstart_conf = 1;
      else
        quickstart_conf = 0;
      break;
      
    case CP_CDTV:
      quickstart_model = 6;
      quickstart_conf = 0;
      break;

    default:
      if(changed_prefs.cpu_model == 68000)
        quickstart_model = 0;
      else if(changed_prefs.cpu_model == 68020)
        quickstart_model = 3;
      else
        quickstart_model = 4;
      quickstart_conf = 0;
  }
  cboModel->setSelected(quickstart_model);
}


class AmigaModelListModel : public gcn::ListModel
{
  public:
    AmigaModelListModel()
    {
    }
    
    int getNumberOfElements()
    {
      return numModels;
    }

    std::string getElementAt(int i)
    {
      if(i < 0 || i >= numModels)
        return "---";
      return amodels[i].name;
    }
};
static AmigaModelListModel amigaModelList;


class AmigaConfigListModel : public gcn::ListModel
{
  public:
    AmigaConfigListModel()
    {
    }
    
    int getNumberOfElements()
    {
      return numModelConfigs;
    }

    std::string getElementAt(int i)
    {
      if(quickstart_model < 0 || i < 0 || i >= numModelConfigs)
        return "---";
      return amodels[quickstart_model].configs[i];
    }
};
static AmigaConfigListModel amigaConfigList;


class QSDiskfileListModel : public gcn::ListModel
{
  public:
    QSDiskfileListModel()
    {
    }
    
    int getNumberOfElements()
    {
      return lstMRUDiskList.size();
    }

    std::string getElementAt(int i)
    {
      if(i < 0 || i >= lstMRUDiskList.size())
        return "---";
      return lstMRUDiskList[i];
    }
};
static QSDiskfileListModel diskfileList;


class QSCDfileListModel : public gcn::ListModel
{
  public:
    QSCDfileListModel()
    {
    }
    
    int getNumberOfElements()
    {
      return lstMRUCDList.size();
    }

    std::string getElementAt(int i)
    {
      if(i < 0 || i >= lstMRUCDList.size())
        return "---";
      return lstMRUCDList[i];
    }
};
static QSCDfileListModel cdfileList;


class QSCDButtonActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (actionEvent.getSource() == cmdCDEject) {
  	    //---------------------------------------
        // Eject CD from drive
  	    //---------------------------------------
        strncpy(changed_prefs.cdslots[0].name, "", MAX_DPATH);
        AdjustDropDownControls();
      } else if(actionEvent.getSource() == cmdCDSelect) {
  	    char tmp[MAX_DPATH];

  	    if(strlen(changed_prefs.cdslots[0].name) > 0)
  	      strncpy(tmp, changed_prefs.cdslots[0].name, MAX_DPATH);
  	    else
  	      strncpy(tmp, currentDir, MAX_DPATH);

  	    if(SelectFile("Select CD image file", tmp, cdfile_filter))
	      {
    	    if(strncmp(changed_prefs.cdslots[0].name, tmp, MAX_DPATH))
    	    {
      	    strncpy(changed_prefs.cdslots[0].name, tmp, sizeof(changed_prefs.cdslots[0].name));
      	    changed_prefs.cdslots[0].inuse = true;
      	    changed_prefs.cdslots[0].type = SCSI_UNIT_IMAGE;
      	    AddFileToCDList(tmp, 1);
      	    extractPath(tmp, currentDir);

            AdjustDropDownControls();
    	    }
	      }
	      cmdCDSelect->requestFocus();
      }
      RefreshPanelHD();
      RefreshPanelQuickstart();
    }
};
static QSCDButtonActionListener* cdButtonActionListener;


class QSCDFileActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    //---------------------------------------
      // CD image from list selected
	    //---------------------------------------
	    if(!bIgnoreListChange) {
  	    int idx = cboCDFile->getSelected();

  	    if(idx < 0) {
          strncpy(changed_prefs.cdslots[0].name, "", MAX_DPATH);
          AdjustDropDownControls();
	      } else {
    	    if(cdfileList.getElementAt(idx).compare(changed_prefs.cdslots[0].name))
	        {
      	    strncpy(changed_prefs.cdslots[0].name, cdfileList.getElementAt(idx).c_str(), sizeof(changed_prefs.cdslots[0].name));
      	    changed_prefs.cdslots[0].inuse = true;
      	    changed_prefs.cdslots[0].type = SCSI_UNIT_IMAGE;
      	    lstMRUCDList.erase(lstMRUCDList.begin() + idx);
      	    lstMRUCDList.insert(lstMRUCDList.begin(), changed_prefs.cdslots[0].name);
            bIgnoreListChange = true;
            cboCDFile->setSelected(0);
            bIgnoreListChange = false;
          }
  	    }
      }
      RefreshPanelHD();
      RefreshPanelQuickstart();
    }
};
static QSCDFileActionListener* cdFileActionListener;


class AmigaModelActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
      if (!bIgnoreListChange) {
        if (actionEvent.getSource() == cboModel) {
    	    //---------------------------------------
          // Amiga model selected
    	    //---------------------------------------
    	    if(quickstart_model != cboModel->getSelected()) {
      	    quickstart_model = cboModel->getSelected();
      	    CountModelConfigs();
      	    cboConfig->setSelected(0);
      	    SetControlState(quickstart_model);
      	    AdjustPrefs();
      	    DisableResume();
      	  }
        } else if (actionEvent.getSource() == cboConfig) {
    	    //---------------------------------------
          // Model configuration selected
    	    //---------------------------------------
    	    if(quickstart_conf != cboConfig->getSelected()) {
      	    quickstart_conf = cboConfig->getSelected();
      	    AdjustPrefs();
      	  }
        }
        RefreshAllPanels();
      }
    }
};
static AmigaModelActionListener* amigaModelActionListener;


class QSNTSCButtonActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    if (chkNTSC->isSelected()) {
	      changed_prefs.ntscmode = true;
	      changed_prefs.chipset_refreshrate = 60;
      } else {
	      changed_prefs.ntscmode = false;
	      changed_prefs.chipset_refreshrate = 50;
      }
      RefreshPanelChipset();
    }
};
static QSNTSCButtonActionListener* ntscButtonActionListener;


class QSDFxCheckActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    for(int i=0; i<2; ++i) {
	      if (actionEvent.getSource() == chkDFx[i]) {
    	    //---------------------------------------
          // Drive enabled/disabled
    	    //---------------------------------------
          if(chkDFx[i]->isSelected())
            changed_prefs.floppyslots[i].dfxtype = DRV_35_DD;
          else
            changed_prefs.floppyslots[i].dfxtype = DRV_NONE;
        } else if(actionEvent.getSource() == chkDFxWriteProtect[i]) {
    	    //---------------------------------------
          // Write-protect changed
    	    //---------------------------------------
          disk_setwriteprotect (&changed_prefs, i, changed_prefs.floppyslots[i].df, chkDFxWriteProtect[i]->isSelected());
          if(disk_getwriteprotect(&changed_prefs, changed_prefs.floppyslots[i].df) != chkDFxWriteProtect[i]->isSelected()) {
            // Failed to change write protection -> maybe filesystem doesn't support this
            ShowMessage("Set/Clear write protect", "Failed to change write permission.", "Maybe underlying filesystem doesn't support this.", "Ok", "");
          }
          DISK_reinsert(i);
        }
      }

      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static QSDFxCheckActionListener* dfxCheckActionListener;


class QSDFxButtonActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    for(int i=0; i<2; ++i) {
	      if (actionEvent.getSource() == cmdDFxInfo[i]) {
    	    //---------------------------------------
          // Show info about current disk-image
    	    //---------------------------------------
          if(changed_prefs.floppyslots[i].dfxtype != DRV_NONE && strlen(changed_prefs.floppyslots[i].df) > 0)
            ; // ToDo: Show info dialog
        } else if (actionEvent.getSource() == cmdDFxEject[i]) {
    	    //---------------------------------------
          // Eject disk from drive
    	    //---------------------------------------
          disk_eject(i);
          strncpy(changed_prefs.floppyslots[i].df, "", MAX_DPATH);
          AdjustDropDownControls();
        } else if (actionEvent.getSource() == cmdDFxSelect[i]) {
    	    //---------------------------------------
          // Select disk for drive
    	    //---------------------------------------
    	    char tmp[MAX_PATH];

    	    if(strlen(changed_prefs.floppyslots[i].df) > 0)
    	      strncpy(tmp, changed_prefs.floppyslots[i].df, MAX_PATH);
    	    else
    	      strncpy(tmp, currentDir, MAX_PATH);
    	    if(SelectFile("Select disk image file", tmp, diskfile_filter))
  	      {
      	    if(strncmp(changed_prefs.floppyslots[i].df, tmp, MAX_PATH))
      	    {
        	    strncpy(changed_prefs.floppyslots[i].df, tmp, sizeof(changed_prefs.floppyslots[i].df));
        	    disk_insert(i, tmp);
        	    AddFileToDiskList(tmp, 1);
        	    extractPath(tmp, currentDir);

              AdjustDropDownControls();
      	    }
  	      }
  	      cmdDFxSelect[i]->requestFocus();
        }
      }

      RefreshPanelFloppy();
      RefreshPanelQuickstart();
    }
};
static QSDFxButtonActionListener* dfxButtonActionListener;


class QSDiskFileActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    for(int i=0; i<2; ++i) {
	      if (actionEvent.getSource() == cboDFxFile[i]) {
    	    //---------------------------------------
          // Diskimage from list selected
    	    //---------------------------------------
    	    if(!bIgnoreListChange) {
      	    int idx = cboDFxFile[i]->getSelected();

      	    if(idx < 0) {
              disk_eject(i);
              strncpy(changed_prefs.floppyslots[i].df, "", MAX_DPATH);
              AdjustDropDownControls();
    	      } else {
        	    if(diskfileList.getElementAt(idx).compare(changed_prefs.floppyslots[i].df)) {
          	    strncpy(changed_prefs.floppyslots[i].df, diskfileList.getElementAt(idx).c_str(), sizeof(changed_prefs.floppyslots[i].df));
          	    disk_insert(i, changed_prefs.floppyslots[i].df);
          	    lstMRUDiskList.erase(lstMRUDiskList.begin() + idx);
          	    lstMRUDiskList.insert(lstMRUDiskList.begin(), changed_prefs.floppyslots[i].df);
                bIgnoreListChange = true;
                cboDFxFile[i]->setSelected(0);
                bIgnoreListChange = false;
              }
      	    }
            RefreshPanelFloppy();
            RefreshPanelQuickstart();
      	  }
        }
      }
    }
};
static QSDiskFileActionListener* diskFileActionListener;


class QuickstartModeActionListener : public gcn::ActionListener
{
  public:
    void action(const gcn::ActionEvent& actionEvent)
    {
	    quickstart_start = chkQuickstartMode->isSelected();
    }
};
static QuickstartModeActionListener* quickstartModeActionListener;


void InitPanelQuickstart(const struct _ConfigCategory& category)
{
	int posX;
	int posY = DISTANCE_BORDER;
	int i;
	
	amigaModelActionListener = new AmigaModelActionListener();
  ntscButtonActionListener = new QSNTSCButtonActionListener();
	dfxCheckActionListener = new QSDFxCheckActionListener();
	dfxButtonActionListener = new QSDFxButtonActionListener();
	diskFileActionListener = new QSDiskFileActionListener();
  cdButtonActionListener = new QSCDButtonActionListener();
  cdFileActionListener = new QSCDFileActionListener();
	quickstartModeActionListener = new QuickstartModeActionListener();
	
  lblModel = new gcn::Label("Amiga model:");
  lblModel->setSize(100, LABEL_HEIGHT);
  lblModel->setAlignment(gcn::Graphics::RIGHT);
	cboModel = new gcn::UaeDropDown(&amigaModelList);
  cboModel->setSize(160, DROPDOWN_HEIGHT);
  cboModel->setBaseColor(gui_baseCol);
  cboModel->setId("qscboAModel");
  cboModel->addActionListener(amigaModelActionListener);

  lblConfig = new gcn::Label("Configuration:");
  lblConfig->setSize(100, LABEL_HEIGHT);
  lblConfig->setAlignment(gcn::Graphics::RIGHT);
	cboConfig = new gcn::UaeDropDown(&amigaConfigList);
  cboConfig->setSize(category.panel->getWidth() - lblConfig->getWidth() - 8 - 2 * DISTANCE_BORDER, DROPDOWN_HEIGHT);
  cboConfig->setBaseColor(gui_baseCol);
  cboConfig->setId("qscboAConfig");
  cboConfig->addActionListener(amigaModelActionListener);

	chkNTSC = new gcn::UaeCheckBox("NTSC");
  chkNTSC->setId("qsNTSC");
  chkNTSC->addActionListener(ntscButtonActionListener);

	for(i=0; i<2; ++i)
	{
	  char tmp[20];
	  snprintf(tmp, 20, "DF%d:", i); 
	  chkDFx[i] = new gcn::UaeCheckBox(tmp);
	  chkDFx[i]->addActionListener(dfxCheckActionListener);
	  snprintf(tmp, 20, "qsDF%d", i);
	  chkDFx[i]->setId(tmp);

	  chkDFxWriteProtect[i] = new gcn::UaeCheckBox("Write-protected");
	  chkDFxWriteProtect[i]->addActionListener(dfxCheckActionListener);
	  snprintf(tmp, 20, "qsWP%d", i);
	  chkDFxWriteProtect[i]->setId(tmp);
	  
    cmdDFxInfo[i] = new gcn::Button("?");
    cmdDFxInfo[i]->setSize(SMALL_BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxInfo[i]->setBaseColor(gui_baseCol);
    cmdDFxInfo[i]->addActionListener(dfxButtonActionListener);
	  snprintf(tmp, 20, "qsInfo%d", i);
	  cmdDFxInfo[i]->setId(tmp);

    cmdDFxEject[i] = new gcn::Button("Eject");
    cmdDFxEject[i]->setSize(SMALL_BUTTON_WIDTH * 2, SMALL_BUTTON_HEIGHT);
    cmdDFxEject[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "qscmdEject%d", i);
	  cmdDFxEject[i]->setId(tmp);
    cmdDFxEject[i]->addActionListener(dfxButtonActionListener);

    cmdDFxSelect[i] = new gcn::Button("Select file");
    cmdDFxSelect[i]->setSize(BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
    cmdDFxSelect[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "qscmdSel%d", i);
	  cmdDFxSelect[i]->setId(tmp);
    cmdDFxSelect[i]->addActionListener(dfxButtonActionListener);

	  cboDFxFile[i] = new gcn::UaeDropDown(&diskfileList);
    cboDFxFile[i]->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER, DROPDOWN_HEIGHT);
    cboDFxFile[i]->setBaseColor(gui_baseCol);
	  snprintf(tmp, 20, "qscboDisk%d", i);
	  cboDFxFile[i]->setId(tmp);
    cboDFxFile[i]->addActionListener(diskFileActionListener);
	}

  chkCD = new gcn::UaeCheckBox("CD drive");
  chkCD->setId("qsCD drive");
  chkCD->setEnabled(false);
  
  cmdCDEject = new gcn::Button("Eject");
  cmdCDEject->setSize(SMALL_BUTTON_WIDTH * 2, SMALL_BUTTON_HEIGHT);
  cmdCDEject->setBaseColor(gui_baseCol);
  cmdCDEject->setId("qscdEject");
  cmdCDEject->addActionListener(cdButtonActionListener);

  cmdCDSelect = new gcn::Button("Select image");
  cmdCDSelect->setSize(BUTTON_WIDTH, SMALL_BUTTON_HEIGHT);
  cmdCDSelect->setBaseColor(gui_baseCol);
  cmdCDSelect->setId("qsCDSelect");
  cmdCDSelect->addActionListener(cdButtonActionListener);

  cboCDFile = new gcn::UaeDropDown(&cdfileList);
  cboCDFile->setSize(category.panel->getWidth() - 2 * DISTANCE_BORDER, DROPDOWN_HEIGHT);
  cboCDFile->setBaseColor(gui_baseCol);
  cboCDFile->setId("qscboCD");
  cboCDFile->addActionListener(cdFileActionListener);

	chkQuickstartMode = new gcn::UaeCheckBox("Start in Quickstart mode");
  chkQuickstartMode->setId("qsMode");
  chkQuickstartMode->addActionListener(quickstartModeActionListener);

  category.panel->add(lblModel, DISTANCE_BORDER, posY);
  category.panel->add(cboModel, DISTANCE_BORDER + lblModel->getWidth() + 8, posY);
  category.panel->add(chkNTSC, cboModel->getX() + cboModel->getWidth() + 8, posY);
  posY += cboModel->getHeight() + DISTANCE_NEXT_Y;
  category.panel->add(lblConfig, DISTANCE_BORDER, posY);
  category.panel->add(cboConfig, DISTANCE_BORDER + lblConfig->getWidth() + 8, posY);
  posY += cboConfig->getHeight() + DISTANCE_NEXT_Y;

	for(i=0; i<2; ++i)
	{
	  posX = DISTANCE_BORDER;
	  category.panel->add(chkDFx[i], posX, posY);
	  posX += 180;
	  category.panel->add(chkDFxWriteProtect[i], posX, posY);
	  posX += chkDFxWriteProtect[i]->getWidth() + 4 * DISTANCE_NEXT_X;
//	  category.panel->add(cmdDFxInfo[i], posX, posY);
	  posX += cmdDFxInfo[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxEject[i], posX, posY);
	  posX += cmdDFxEject[i]->getWidth() + DISTANCE_NEXT_X;
	  category.panel->add(cmdDFxSelect[i], posX, posY);
	  posY += chkDFx[i]->getHeight() + 8;

	  category.panel->add(cboDFxFile[i], DISTANCE_BORDER, posY);
	  posY += cboDFxFile[i]->getHeight() + DISTANCE_NEXT_Y + 4;
  }
  
  category.panel->add(chkCD, chkDFx[1]->getX(), chkDFx[1]->getY());
  category.panel->add(cmdCDEject, cmdDFxEject[1]->getX(), cmdDFxEject[1]->getY());
  category.panel->add(cmdCDSelect, cmdDFxSelect[1]->getX(), cmdDFxSelect[1]->getY());
  category.panel->add(cboCDFile, cboDFxFile[1]->getX(), cboDFxFile[1]->getY());

  category.panel->add(chkQuickstartMode, category.panel->getWidth() - chkQuickstartMode->getWidth() - DISTANCE_BORDER, posY);
  posY += chkQuickstartMode->getHeight() + DISTANCE_NEXT_Y;
  
  chkCD->setVisible(false);
  cmdCDEject->setVisible(false);
  cmdCDSelect->setVisible(false);
  cboCDFile->setVisible(false);

  bIgnoreListChange = false;
    
  cboModel->setSelected(quickstart_model);
  CountModelConfigs();
  cboConfig->setSelected(quickstart_conf);
  SetControlState(quickstart_model);
  
  //SetModelFromConfig();
  // Only change the current prefs if we're not already emulating
  if (!emulating)
    AdjustPrefs();
  
  RefreshPanelQuickstart();
}


void ExitPanelQuickstart(void)
{
	delete lblModel;
	delete cboModel;
	delete lblConfig;
	delete cboConfig;
	delete chkNTSC;
	for(int i=0; i<2; ++i)
	{
	  delete chkDFx[i];
	  delete chkDFxWriteProtect[i];
	  delete cmdDFxInfo[i];
	  delete cmdDFxEject[i];
	  delete cmdDFxSelect[i];
	  delete cboDFxFile[i];
	}
  delete chkCD;
  delete cmdCDEject;
  delete cmdCDSelect;
  delete cboCDFile;
  delete chkQuickstartMode;
  
  delete amigaModelActionListener;
  delete ntscButtonActionListener;
  delete dfxCheckActionListener;
  delete dfxButtonActionListener;
  delete diskFileActionListener;
  delete cdButtonActionListener;
  delete cdFileActionListener;
  delete quickstartModeActionListener;
}


static void AdjustDropDownControls(void)
{
  int i, j;
  
  bIgnoreListChange = true;
  
  for(i=0; i<2; ++i)
  {
    bool found = FALSE;
    cboDFxFile[i]->clearSelected();

    if((changed_prefs.floppyslots[i].dfxtype != DRV_NONE) && strlen(changed_prefs.floppyslots[i].df) > 0)
    {
      for(j=0; j<lstMRUDiskList.size(); ++j)
      {
        if(!lstMRUDiskList[j].compare(changed_prefs.floppyslots[i].df))
        {
          cboDFxFile[i]->setSelected(j);
          found = TRUE;
          break;
        }
      }
      if (!found)
      {
        AddFileToDiskList(changed_prefs.floppyslots[i].df, 1);
        cboDFxFile[i]->setSelected(0);
      } 
    }
  }

  cboCDFile->clearSelected();
  if((changed_prefs.cdslots[0].inuse) && strlen(changed_prefs.cdslots[0].name) > 0)
  {
    bool found = FALSE;
    for(i = 0; i < lstMRUCDList.size(); ++i)
    {
      if(!lstMRUCDList[i].compare(changed_prefs.cdslots[0].name))
      {
        cboCDFile->setSelected(i);
        found = TRUE;
        break;
      }
      if (!found)
      {
        AddFileToCDList(changed_prefs.cdslots[0].name, 1);
        cboCDFile->setSelected(0);
      } 
    }
  }
       
  bIgnoreListChange = false;
}


void RefreshPanelQuickstart(void)
{
  int i;
  bool prevAvailable = true;
  
  chkNTSC->setSelected(changed_prefs.ntscmode);

  AdjustDropDownControls();

  changed_prefs.nr_floppies = 0;
  for(i=0; i<4; ++i)
  {
    bool driveEnabled = changed_prefs.floppyslots[i].dfxtype != DRV_NONE;
    if(i < 2) {
      chkDFx[i]->setSelected(driveEnabled);
      chkDFxWriteProtect[i]->setSelected(disk_getwriteprotect(&changed_prefs, changed_prefs.floppyslots[i].df));
      if(i == 0)
        chkDFx[i]->setEnabled(false);
      else
        chkDFx[i]->setEnabled(prevAvailable);
      
      cmdDFxInfo[i]->setEnabled(driveEnabled);
      chkDFxWriteProtect[i]->setEnabled(driveEnabled && !changed_prefs.floppy_read_only);
      cmdDFxEject[i]->setEnabled(driveEnabled);
      cmdDFxSelect[i]->setEnabled(driveEnabled);
      cboDFxFile[i]->setEnabled(driveEnabled);
    }    
    prevAvailable = driveEnabled;
    if(driveEnabled)
      changed_prefs.nr_floppies = i + 1;
  }

  chkCD->setSelected(changed_prefs.cdslots[0].inuse);
  cmdCDEject->setEnabled(changed_prefs.cdslots[0].inuse);
  cmdCDSelect->setEnabled(changed_prefs.cdslots[0].inuse);
  cboCDFile->setEnabled(changed_prefs.cdslots[0].inuse);

  chkQuickstartMode->setSelected(quickstart_start);
}


bool HelpPanelQuickstart(std::vector<std::string> &helptext)
{
  helptext.clear();
  helptext.push_back("Simplified start of emulation by just selecting the Amiga model and the disk/CD you want to use.");
  helptext.push_back("");
  helptext.push_back("After selecting the Amiga model, you can choose from a small list of standard configurations for this model to");
  helptext.push_back("start with.");
  helptext.push_back("");
  helptext.push_back("When you activate \"Start in Quickstart mode\", the next time you run UAE4ARM, it  will start with the quickstart");
  helptext.push_back("panel. Otherwise you start in configuraions panel.");
  return true;
}
