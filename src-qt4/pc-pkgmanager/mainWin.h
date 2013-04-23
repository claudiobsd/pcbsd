#ifndef MAINWIN_H
#define MAINWIN_H

#include <QDialog>
#include <QFile>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSocketNotifier>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

#include <pcbsd-utils.h>
#include <pcbsd-ui.h>
#include "ui_mainWin.h"
#include "../config.h"

/* Change this to switch the default patch tmpdir */
const QString PATCHTMPDIR_DEFAULT( PREFIX + "/tmp" );
#define SYSTRIGGER "/tmp/.sysupdatetraytrigger"

class mainWin : public QDialog, private Ui::mainWin
{
        Q_OBJECT

public:
        mainWin() : QDialog()
        {
          setupUi(this);
        }

    QMap <QString, QString> codeMap;

public slots:
    void ProgramInit(QString);
    void slotSingleInstance();

private slots:
    void slotCloseClicked();
    
    // Generic pkgng handlers
    void slotReadPkgOutput();
    void slotPkgDone();
    void slotReadEventPipe(int fd);

    // Meta-Packages
    void slotFinishLoadingMetaPkgs();
    void slotGetPackageDataOutput();
    void slotApplyClicked();

    // Package updates
    void slotRescanPkgsClicked();
    void slotUpdatePkgsClicked();
    void slotApplyMetaChanges();
    void slotDeskPkgsChanged(QTreeWidgetItem *aItem, int __unused);
    void slotMetaRightClick();
    void slotMetaViewPkgs();

private:
    // Generic pkg process handlers
    void startPkgProcess();
    QList<QStringList> pkgCmdList;

    // Updates
    void doUpdates();
    bool sanityCheck();

    int curUpdate;
    int curUpdateIndex;
    int totUpdate;
    bool dPackages;
    bool uPackages;
    bool doingUpdate;
    QProcess *uProc;
    QList<QStringList> listUpdates;
    QString wDir, wIP;
    QString lastError;
    QSocketNotifier *eSock;


    // Meta-Packages
    void initMetaWidget();
    void populateMetaPkgs();
    void addTreeItems(QString parent);
    bool isMetaPkgInstalled(QString mPkg);
    void checkMPKGUpdates();
    void parseTmpMetaList();
    void saveMetaPkgs();
    void startMetaChanges();
    bool haveAMetaDesktop();
    bool haveMetaPkgChanges();
    QString getAddPkgs();
    QString getDelPkgs();
    bool allChildrenPkgsChecked(QString parent);
    bool allChildrenPkgsUnchecked(QString parent);
    void checkAllChildrenPkgs(QString parent);
    void uncheckAllChildrenPkgs(QString parent);
    QString getLineFromCommandOutput( QString cmd );

    QList<QStringList> metaPkgList;
    QStringList tmpMetaPkgList;
    QProcess *addMetaProc;
    QProcess *delMetaProc;
    QProcess *getMetaProc;
    QProgressDialog *delprogress;
    QProgressDialog *addprogress;
    QString addPkgs;
    QString delPkgs;
    QString pkgSource;
    QString Arch;
    QString chrootArg1;
    QString chrootArg2;
    bool canceled;
    QMenu *popup;
    dialogInfo *dIB;

signals:

} ;
#endif // MAINWIN_H

