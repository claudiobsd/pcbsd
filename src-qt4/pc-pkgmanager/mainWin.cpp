/****************************************************************************
** ui.h extension file, included from the uic-generated form implementation.
**
** If you want to add, delete, or rename functions or slots, use
** Qt Designer to update this file, preserving your code.
**
** You should not define a constructor or destructor in this file.
** Instead, write your code in functions called init() and destroy().
** These will automatically be called by the form's constructor and
** destructor.
*****************************************************************************/
#include <fcntl.h>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QProgressDialog>
#include <QSocketNotifier>
#include <QString>
#include <QTextStream>
#include <pcbsd-utils.h>
#include <pcbsd-ui.h>
#include "mainWin.h"
#include "../config.h"

void mainWin::ProgramInit(QString ch)
{
  // Set any warden directories
  doingUpdate=false;
  lastError="";
  wDir = ch;

  //Grab the username
  //username = QString::fromLocal8Bit(getenv("LOGNAME"));
  connect(pushUpdatePkgs, SIGNAL(clicked()), this, SLOT(slotUpdatePkgsClicked()));
  connect(pushClose, SIGNAL(clicked()), this, SLOT(slotCloseClicked()));
  connect(buttonRescanPkgs, SIGNAL(clicked()), this, SLOT(slotRescanPkgsClicked()));
  connect(pushPkgApply, SIGNAL( clicked() ), this, SLOT( slotApplyClicked() ) );
  progressUpdate->setHidden(true);

  QTimer::singleShot(200, this, SLOT(slotRescanPkgsClicked() ) );
  initMetaWidget();
}

void mainWin::slotRescanPkgsClicked()
{
  // Check for pkg updates
  checkMPKGUpdates();
}

void mainWin::slotApplyClicked() {

}

void mainWin::checkMPKGUpdates() {
  if ( doingUpdate )
     return;

  QString line, tmp, name, pkgname, pkgover, pkgnver;
  QStringList up, listPkgs;
  bool haveUpdates = false;
  int totPkgs=0;
  buttonRescanPkgs->setEnabled(false);
  pushUpdatePkgs->setEnabled(false);
  listViewUpdatesPkgs->clear();
  groupUpdatesPkgs->setTitle(tr("Checking for updates"));

  QProcess p;
  if ( wDir.isEmpty() )
     p.start(QString("pc-updatemanager"), QStringList() << "pkgcheck");
  else
     p.start(QString("chroot"), QStringList() << wDir << "pc-updatemanager" << "pkgcheck");
  while(p.state() == QProcess::Starting || p.state() == QProcess::Running)
     QCoreApplication::processEvents();

  while (p.canReadLine()) {
    line = p.readLine().simplified();
    qDebug() << line;
    if ( line.indexOf("Upgrading") != 0) {
       continue;
    }
    tmp = line;
    pkgname = tmp.section(" ", 1, 1);
    pkgname.replace(":", "");
    pkgover = tmp.section(" ", 2, 2);
    pkgnver = tmp.section(" ", 4, 4);
    QTreeWidgetItem *myItem = new QTreeWidgetItem(QStringList() << pkgname << pkgover << pkgnver);
    listViewUpdatesPkgs->addTopLevelItem(myItem);
    haveUpdates = true;
    totPkgs++;
  }

  buttonRescanPkgs->setEnabled(true);
  pushUpdatePkgs->setEnabled(haveUpdates);
  if ( totPkgs > 0 ) {
    tabUpdates->setTabText(1, tr("Package Updates (%1)").arg(totPkgs));
    groupUpdatesPkgs->setTitle(tr("Available updates"));
  } else {
    tabUpdates->setTabText(1, tr("Package Updates"));
    groupUpdatesPkgs->setTitle(tr("No available updates"));
  }
 
}

void mainWin::slotSingleInstance() {
   this->hide();
   this->showNormal();
   this->activateWindow();
   this->raise();
}

void mainWin::slotCloseClicked() {
   close();
}

void mainWin::slotUpdatePkgsClicked() {
  dPackages = false;
  uPackages = false;
  doingUpdate=true;

  // Create our runlist of package commands
  pkgCmdList.clear();
  QStringList pCmds;

  if ( wDir.isEmpty() )
    pCmds << "pc-updatemanager" << "pkgupdate";
  else
    pCmds << "chroot" << wDir << "pc-updatemanager" << "pkgupdate";

  // Setup our runList
  pkgCmdList << pCmds;

  startPkgProcess();

  textStatus->setText(tr("Starting package updates..."));

}

void mainWin::startPkgProcess() {

  if ( pkgCmdList.isEmpty() )
    return;
  if ( pkgCmdList.at(0).at(0).isEmpty() )
     return; 

  // Get the command name
  QString cmd;
  cmd = pkgCmdList.at(0).at(0);

  // Get any optional flags
  QStringList flags;
  for (int i = 0; i < pkgCmdList.at(0).size(); ++i) {
     if ( i == 0 )
       continue;
     flags << pkgCmdList.at(0).at(i);
  } 

  qDebug() << cmd + " " + flags.join(" ");
  
  // Setup the first process
  uProc = new QProcess();
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert("PCFETCHGUI", "YES");
  uProc->setProcessEnvironment(env);
  uProc->setProcessChannelMode(QProcess::MergedChannels);

  // Connect the slots
  connect( uProc, SIGNAL(readyReadStandardOutput()), this, SLOT(slotReadPkgOutput()) );
  connect( uProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotPkgDone()) );

  uProc->start(cmd, flags);

  stackedTop->setCurrentIndex(1);

  progressUpdate->setRange(0, 0 );
  progressUpdate->setValue(0);

}

void mainWin::slotReadPkgOutput() {
   QString line, tmp, cur, tot, fname;

   while (uProc->canReadLine()) {
     line = uProc->readLine().simplified();
     qDebug() << "Normal Line:" << line;
     tmp = line;
     tmp.truncate(50);
     if ( line.indexOf("to be downloaded") != -1 ) {
       textStatus->setText(tr("Downloading packages..."));
       curUpdate = 0;
       progressUpdate->setValue(0);
       continue;
     }
     if ( line.indexOf("Checking integrity") == 0 ) {
       textStatus->setText(line);
       uPackages = true;
       dPackages = false;
       curUpdate = 0;
       progressUpdate->setValue(0);
     }
     if ( line.indexOf("FETCH: ") == 0 ) { 
	progressUpdate->setValue(progressUpdate->value() + 1); 
	tmp = line; 
	tmp = tmp.remove(0, tmp.lastIndexOf("/") + 1); 
	textStatus->setText(tr("Downloading: %1").arg(tmp)); 
        continue;
     } 
     
     if ( line.indexOf("SIZE: ") == 0 ) {
          bool ok, ok2;
     	  line.replace("SIZE: ", "");
          line.replace("DOWNLOADED: ", "");
          line.replace("SPEED: ", "");
          line.section(" ", 0, 0).toInt(&ok);
          line.section(" ", 1, 1).toInt(&ok2);
   
          if ( ok && ok2 ) {
	    QString unit;
            int tot = line.section(" ", 0, 0).toInt(&ok);
            int cur = line.section(" ", 1, 1).toInt(&ok2);
            QString percent = QString::number( (float)(cur * 100)/tot );
            QString speed = line.section(" ", 2, 3);

  	    // Get the MB downloaded / total
	    if ( tot > 2048 ) {
	      unit="MB";
	      tot = tot / 1024;
       	      cur = cur / 1024;
	    } else {
	      unit="KB";
            }

            QString ProgressString=QString("%1" + unit + " of %2" + unit + " at %3").arg(cur).arg(tot).arg(speed);
            progressUpdate->setRange(0, tot);
            progressUpdate->setValue(cur);
         }
     }

     if ( uPackages ) {
       if ( line.indexOf("Upgrading") == 0 ) {
         textStatus->setText(line);
         curUpdate++;
         progressUpdate->setValue(curUpdate);
       }
       continue;
     }

   } // end of while
}

// Function to read output of pipefile
void mainWin::slotReadEventPipe(int fd) {
  QString tmp, fname, cur, tot;
  bool ok, ok2;
  char buff[4028];
  int totread = read(fd, buff, 4020);
  buff[totread]='\0';
  QString line = buff;
  line = line.simplified();
  //qDebug() << "Found line:" << line;
  
  if ( line.indexOf("INFO_FETCH") != -1  && dPackages ) {
     tmp = line;
     fname = tmp.section(":", 4, 4);
     fname.remove(0, fname.lastIndexOf('/') + 1);
     fname  = fname.section('"', 0, 0);
     cur = tmp.section(":", 5, 5);
     cur = cur.remove(',');
     cur = cur.section(" ", 1, 1);
     cur = cur.simplified();
     tot = tmp.section(":", 6, 6);
     tot = tot.simplified();
     tot = tot.remove(',');
     tot = tot.section("}", 0, 0);

     textStatus->setText(tr("Downloading %1").arg(fname));
     tot.toInt(&ok);
     cur.toInt(&ok2);
     if ( ok && ok2 )
     { 
       progressUpdate->setRange(0, tot.toInt(&ok2));
       progressUpdate->setValue(cur.toInt(&ok2));
     }
     
     //qDebug() << "File:" << fname << "cur" << cur << "tot" << tot;
  }
}

void mainWin::slotPkgDone() {

  // Run the next command on the stack if necessary
  if (  pkgCmdList.size() > 1 ) {
	pkgCmdList.removeAt(0);	
        startPkgProcess();	
	return;
  }

  // Nothing left to run! Lets wrap up
  QFile sysTrig( SYSTRIGGER );
  if ( sysTrig.open( QIODevice::WriteOnly ) ) {
    QTextStream streamTrig( &sysTrig );
     streamTrig << "INSTALLFINISHED: ";
  }

  if ( uProc->exitCode() != 0 )
    QMessageBox::warning(this, tr("Failed pkgng command!"), tr("The package changes failed!"));

  stackedTop->setCurrentIndex(0);

}

/*****************************************
Code for meta-package (Basic Mode)
******************************************/

void mainWin::initMetaWidget()
{
  qDebug() << "Starting metaWidget...";

  populateMetaPkgs();

  // Connect our slots
  treeMetaPkgs->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(treeMetaPkgs, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(slotMetaRightClick()) );


}


// Display found meta-pkg data
void mainWin::populateMetaPkgs()
{
  pushPkgApply->setEnabled(false);
  treeMetaPkgs->clear();
  tmpMetaPkgList.clear();
  new QTreeWidgetItem(treeMetaPkgs, QStringList() << tr("Loading... Please wait...") );

  if ( ! metaPkgList.isEmpty() )
  	disconnect(treeMetaPkgs, SIGNAL(itemChanged(QTreeWidgetItem *, int)), 0, 0);
  metaPkgList.clear();

  // Start the process to get meta-pkg info
  getMetaProc = new QProcess();
  qDebug() << "Searching for meta-pkgs...";
  connect( getMetaProc, SIGNAL(readyReadStandardOutput()), this, SLOT(slotGetPackageDataOutput()) );
  connect( getMetaProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(slotFinishLoadingMetaPkgs()) );
  getMetaProc->setProcessChannelMode(QProcess::MergedChannels);
  getMetaProc->start(QString("pc-metapkgmanager"), QStringList() << chrootArg1 << chrootArg2 << "list");

}

// Display found meta-pkg data
void mainWin::slotFinishLoadingMetaPkgs()
{

  // Populate the metaPkgList
  parseTmpMetaList();

  treeMetaPkgs->clear();

  addTreeItems(QString()); 

  connect(treeMetaPkgs, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotDeskPkgsChanged(QTreeWidgetItem *, int)));
}

void mainWin::addTreeItems(QString parent)
{
  for (int z=0; z < metaPkgList.count(); ++z) {
    if ( metaPkgList.at(z).at(3) != parent )
      continue;

    // Hide the "base-system" package, since we are installing it anyway
    if (metaPkgList.at(z).at(0) == "base-system" )
      return;

    QTreeWidgetItem *deskItem = new QTreeWidgetItem(QStringList() << metaPkgList.at(z).at(0) );
    deskItem->setIcon(0, QIcon(metaPkgList.at(z).at(2)));
    deskItem->setToolTip(0, metaPkgList.at(z).at(1));
    deskItem->setCheckState(0, Qt::Unchecked);

    if ( metaPkgList.at(z).at(5) == "YES" )
      deskItem->setCheckState(0, Qt::Checked);

    if ( parent.isEmpty() ) {
      treeMetaPkgs->addTopLevelItem(deskItem);
    } else {
      // Locate the parent to attach to
      QTreeWidgetItemIterator it(treeMetaPkgs);
      while (*it) {
        if ((*it)->text(0) == parent ) {
	  (*it)->addChild(deskItem);
          if ( metaPkgList.at(z).at(5) == "YES" && (*it)->checkState(0) == Qt::Unchecked)
	    (*it)->setCheckState(0, Qt::PartiallyChecked);
          if ( metaPkgList.at(z).at(5) == "NO" && (*it)->checkState(0) == Qt::Checked)
	    (*it)->setCheckState(0, Qt::PartiallyChecked);
	  break;
	}
        it++;
      }
    }

    // Now look for any possible children
    addTreeItems(metaPkgList.at(z).at(0));    
  }
}

// Check if a meta-pkg is installed
bool mainWin::isMetaPkgInstalled(QString mPkg)
{
  QString tmp;
  QProcess pcmp;
  pcmp.start(QString("pc-metapkgmanager"), QStringList() << chrootArg1 << chrootArg2 << "status" << mPkg);
  while ( pcmp.state() != QProcess::NotRunning ) {
     pcmp.waitForFinished(50);
     QCoreApplication::processEvents();
  }

  while (pcmp.canReadLine()) {
     tmp = pcmp.readLine().simplified();
     if ( tmp.indexOf("is installed") != -1 )
     return true;
  }

  return false;
}

// Function which checks for our GUI package schema data
void mainWin::slotGetPackageDataOutput()
{
  while (getMetaProc->canReadLine())
	tmpMetaPkgList << getMetaProc->readLine().simplified();
}

// Parse the pc-metapkg saved output
void mainWin::parseTmpMetaList()
{
  QString tmp, mName, mDesc, mIcon, mParent, mDesktop, mInstalled, mPkgFileList;
  QStringList package;

  for ( int i = 0 ; i < tmpMetaPkgList.size(); i++ )
  {
	QApplication::processEvents();

        tmp = tmpMetaPkgList.at(i);

	if ( tmp.indexOf("Meta Package: ") == 0) {
		mName = tmp.replace("Meta Package: ", "");
		continue;
	}
	if ( tmp.indexOf("Description: ") == 0) {
		mDesc = tmp.replace("Description: ", "");
		continue;
	}
	if ( tmp.indexOf("Icon: ") == 0) {
		mIcon = tmp.replace("Icon: ", "");
		mPkgFileList = mIcon;
		mPkgFileList.replace("pkg-icon.png", "pkg-list");
		continue;
	}
	if ( tmp.indexOf("Parent: ") == 0) {
		mParent = tmp.replace("Parent: ", "");
		continue;
	}
	if ( tmp.indexOf("Desktop: ") == 0) {
		mDesktop = tmp.replace("Desktop: ", "");
		continue;
	}

	// This is an empty category
	if ( tmp.indexOf("Category Entry") == 0) {
		// Now add this category to the string list
		package.clear();
		qDebug() << "Found Package" << mName << mDesc << mIcon << mParent << mDesktop;
		mInstalled = "CATEGORY";
		package << mName << mDesc << mIcon << mParent << mDesktop << mInstalled;
		metaPkgList.append(package);
		mName=""; mDesc=""; mIcon=""; mParent=""; mDesktop=""; mInstalled=""; mPkgFileList="";
	}

	// We have a Meta-Pkg
	if ( tmp.indexOf("Required Packages:") == 0) {
		// Now add this meta-pkg to the string list
		package.clear();
		qDebug() << "Found Package" << mName << mDesc << mIcon << mParent << mDesktop << mPkgFileList;

		if ( isMetaPkgInstalled(mName) )
			mInstalled = "YES";
		else
			mInstalled = "NO";

		package << mName << mDesc << mIcon << mParent << mDesktop << mInstalled << mPkgFileList;
		metaPkgList.append(package);
		mName=""; mDesc=""; mIcon=""; mParent=""; mDesktop=""; mInstalled=""; mPkgFileList="";
	}

    }

}

void mainWin::saveMetaPkgs()
{
	if ( ! haveMetaPkgChanges() )
		return;

	addPkgs = getAddPkgs();
	delPkgs = getDelPkgs();	

	startMetaChanges();

}

void mainWin::startMetaChanges()
{
 // Create our runlist of package commands
  pkgCmdList.clear();
  QStringList pCmds;

  if ( ! delPkgs.isEmpty() ) {
    if ( wDir.isEmpty() )
      pCmds << "pc-metapkgmanager" << "del" << delPkgs;
    else  
      pCmds << "chroot" << wDir << "pc-metapkgmanager" << "del" << delPkgs;
    pkgCmdList << pCmds;
  }
  
  pCmds.clear();

  if ( ! addPkgs.isEmpty() ) {
    if ( wDir.isEmpty() )
      pCmds << "pc-metapkgmanager" << "add" << addPkgs;
    else  
      pCmds << "chroot" << wDir << "pc-metapkgmanager" << "add" << addPkgs;
    pkgCmdList << pCmds;
  }

  // Lets kick it off now
  startPkgProcess();
}

bool mainWin::haveAMetaDesktop()
{
	// If running in a chroot we can skip this check
	if ( ! chrootArg1.isEmpty() )
	  return true;
        
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
         if ( ((*it)->checkState(0) == Qt::Checked) || ((*it)->checkState(0) == Qt::PartiallyChecked) )
	   for (int z=0; z < metaPkgList.count(); ++z)
	     if ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(4) == "YES" )
		return true;
         ++it;
        }

        QMessageBox::warning(this, tr("No Desktop"),
          tr("No desktops have been selected! Please choose at least one desktop before saving."),
          QMessageBox::Ok,
          QMessageBox::Ok);

	return false;
}

bool mainWin::haveMetaPkgChanges()
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
	  for (int z=0; z < metaPkgList.count(); ++z)
	    // See if any packages status have changed
	    if ( ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "YES" && (*it)->checkState(0) == Qt::Unchecked ) \
	      || ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "YES" && (*it)->checkState(0) == Qt::PartiallyChecked ) \
	      || ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "NO" && (*it)->checkState(0) == Qt::Checked ) )
		return true;
         ++it;
        }

	return false;
}

QString mainWin::getAddPkgs()
{
	QString tmp;
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
	  for (int z=0; z < metaPkgList.count(); ++z)
	    // See if any packages status have changed
	    if ( ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "NO" && (*it)->checkState(0) == Qt::Checked ) || \
	         ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "NO" && (*it)->checkState(0) == Qt::PartiallyChecked ) )
		if ( tmp.isEmpty() )
			tmp = (*it)->text(0);
		else
			tmp = tmp + "," + (*it)->text(0);
         ++it;
        }

	return tmp;
}

QString mainWin::getDelPkgs()
{
	QString tmp;
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
	  for (int z=0; z < metaPkgList.count(); ++z)
	    // See if any packages status have changed
	    if ( (*it)->text(0) == metaPkgList.at(z).at(0) && metaPkgList.at(z).at(5) == "YES" && (*it)->checkState(0) == Qt::Unchecked )
		if ( tmp.isEmpty() )
			tmp = (*it)->text(0);
		else
			tmp = tmp + "," + (*it)->text(0);
         ++it;
        }

	return tmp;
}


// Time to save meta-pkgs
void mainWin::slotApplyMetaChanges() {
    saveMetaPkgs();
}



// The User changed the tree widget checked / unchecked stuff sanity check
void mainWin::slotDeskPkgsChanged(QTreeWidgetItem *aItem, int __unused)
{
	if (!aItem)
  	  return;

        disconnect(treeMetaPkgs, SIGNAL(itemChanged(QTreeWidgetItem *, int)), 0, 0);

	if (aItem->childCount() == 0) {
		if (aItem->checkState(0) == Qt::Checked && aItem->parent() )
			if ( allChildrenPkgsChecked(aItem->parent()->text(0)))
				aItem->parent()->setCheckState(0, Qt::Checked);	
			else
				aItem->parent()->setCheckState(0, Qt::PartiallyChecked);	
		if (aItem->checkState(0) == Qt::Unchecked && aItem->parent() )
			if ( ! allChildrenPkgsUnchecked(aItem->parent()->text(0)))
				aItem->parent()->setCheckState(0, Qt::PartiallyChecked);	


	} else {
		if (aItem->checkState(0) == Qt::Checked )
			checkAllChildrenPkgs(aItem->text(0));
		else
			uncheckAllChildrenPkgs(aItem->text(0));
	}
	

    connect(treeMetaPkgs, SIGNAL(itemChanged(QTreeWidgetItem *, int)), this, SLOT(slotDeskPkgsChanged(QTreeWidgetItem *, int)));

	if ( haveMetaPkgChanges() )
		pushPkgApply->setEnabled(true);
	else
		pushPkgApply->setEnabled(false);
}

// Check the "parent" app to see if all its children are checked or not
bool mainWin::allChildrenPkgsChecked(QString parent)
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
         if ((*it)->text(0) == parent ) {
	   if ( (*it)->childCount() <= 0)
	     return true;

	   for ( int i = 0; i < (*it)->childCount() ; ++i) {
	     if ( ! allChildrenPkgsChecked((*it)->child(i)->text(0)))
	       return false;

             if ((*it)->child(i)->checkState(0) != Qt::Checked ) 
	       return false;
	   }
         }
         ++it;
        }
	return true;
}

// Check the "parent" app to see if all its children are unchecked or not
bool mainWin::allChildrenPkgsUnchecked(QString parent)
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
         if ((*it)->text(0) == parent ) {
	   if ( (*it)->childCount() <= 0)
	     return true;

	   for ( int i = 0; i < (*it)->childCount() ; ++i) {
	     if ( ! allChildrenPkgsUnchecked((*it)->child(i)->text(0)))
	       return false;

             if ((*it)->child(i)->checkState(0) != Qt::Unchecked ) 
	       return false;
	   }
         }
         ++it;
        }
	return true;
}

// Check all children of parent
void mainWin::checkAllChildrenPkgs(QString parent)
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
         if (! (*it)->parent()) {
           ++it;
	   continue;
	 } 

         // Lets walk the tree see what pops up
	 bool pFound=false;
         QTreeWidgetItem *itP = (*it)->parent();
	 do {
	   pFound=false;
	   if (itP->text(0) == parent) {
	     (*it)->setCheckState(0, Qt::Checked);
	     break;
	   }
	   if ( itP->parent() ) {
	     itP = itP->parent();
             pFound=true;
           }
         } while (pFound);

         ++it;
       }
}

// UnCheck all children of parent
void mainWin::uncheckAllChildrenPkgs(QString parent)
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
         if (! (*it)->parent()) {
           ++it;
	   continue;
	 } 

         // Lets walk the tree see what pops up
	 bool pFound=false;
         QTreeWidgetItem *itP = (*it)->parent();
	 do {
	   pFound=false;
	   if (itP->text(0) == parent) {
	     (*it)->setCheckState(0, Qt::Unchecked);
	     break;
	   }
	   if ( itP->parent() ) {
	     itP = itP->parent();
             pFound=true;
           }
         } while (pFound);

         ++it;
       }
}

void mainWin::slotMetaRightClick()
{
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
          for (int z=0; z < metaPkgList.count(); ++z) {
            if ( (*it)->isSelected() && (*it)->text(0) == metaPkgList.at(z).at(0) ) {
	      if (metaPkgList.at(z).at(5) == "CATEGORY")
	        return;
	      popup = new QMenu;
	      popup->setTitle((*it)->text(0));
	      popup->addAction(tr("View Packages"), this, SLOT(slotMetaViewPkgs()));
	      popup->exec( QCursor::pos() );
	    }
	  }
         ++it;
        }
}

void mainWin::slotMetaViewPkgs()
{
	QStringList packageList;
        QTreeWidgetItemIterator it(treeMetaPkgs);
        while (*it) {
          for (int z=0; z < metaPkgList.count(); ++z) {
            if ( (*it)->isSelected() && (*it)->text(0) == metaPkgList.at(z).at(0) ) {
 
		QFile pList(metaPkgList.at(z).at(6));
		if ( ! pList.exists() )
		  return;
		
 	        if ( ! pList.open(QIODevice::ReadOnly | QIODevice::Text))
                  return;

                while ( !pList.atEnd() )
     		  packageList << pList.readLine().simplified();

		pList.close();
	        packageList.sort();
			
		dIB = new dialogInfo();
    		dIB->programInit(tr("Package Listing for:") + " " + (*it)->text(0));
		dIB->setInfoText(packageList.join("\n"));
                dIB->show();
	    }
	  }
         ++it;
        }
}

QString mainWin::getLineFromCommandOutput( QString cmd )
{
        FILE *file = popen(cmd.toLatin1(),"r");
 
        char buffer[100];
 
        QString line = "";
        char firstChar;

        if ((firstChar = fgetc(file)) != -1){
                line += firstChar;
                line += fgets(buffer,100,file);
        }
        pclose(file);
        return line.simplified();
}
