/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "mount.h"
#include "Options.h"

#include "indi/driverinfo.h"
#include "indi/indicommon.h"
#include "indi/clientmanager.h"
#include "indi/indifilter.h"

#include "ekosmanager.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"

#include <basedevice.h>

extern const char *libindi_strings_context;

#define UPDATE_DELAY    1000

namespace Ekos
{

Mount::Mount()
{
    setupUi(this);

    stopB->setIcon(QIcon::fromTheme("process-stop"));
    northB->setIcon(QIcon::fromTheme("go-up"));
    westB->setIcon(QIcon::fromTheme("go-previous"));
    eastB->setIcon(QIcon::fromTheme("go-next"));
    southB->setIcon(QIcon::fromTheme("go-down"));


    QFile tempFile;

    if (KSUtils::openDataFile( tempFile, "go-nw.png" ) )
    {
        northwestB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-ne.png" ) )
    {
        northeastB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-sw.png" ) )
    {
        southwestB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    if (KSUtils::openDataFile( tempFile, "go-se.png" ) )
    {
        southeastB->setIcon(QIcon(tempFile.fileName()));
        tempFile.close();
    }

    connect(northB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northB, SIGNAL(released()), this, SLOT(stop()));
    connect(westB, SIGNAL(pressed()), this, SLOT(move()));
    connect(westB, SIGNAL(released()), this, SLOT(stop()));
    connect(southB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southB, SIGNAL(released()), this, SLOT(stop()));
    connect(eastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(eastB, SIGNAL(released()), this, SLOT(stop()));
    connect(northeastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northeastB, SIGNAL(released()), this, SLOT(stop()));
    connect(northwestB, SIGNAL(pressed()), this, SLOT(move()));
    connect(northwestB, SIGNAL(released()), this, SLOT(stop()));
    connect(southeastB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southeastB, SIGNAL(released()), this, SLOT(stop()));
    connect(southwestB, SIGNAL(pressed()), this, SLOT(move()));
    connect(southwestB, SIGNAL(released()), this, SLOT(stop()));
    connect(stopB, SIGNAL(clicked()), this, SLOT(stop()));
    connect(saveB, SIGNAL(clicked()), this, SLOT(save()));

}

Mount::~Mount()
{

}

void Mount::setTelescope(ISD::GDInterface *newTelescope)
{
    currentTelescope = static_cast<ISD::Telescope*> (newTelescope);

    connect(currentTelescope, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SLOT(updateNumber(INumberVectorProperty*)), Qt::UniqueConnection);
    connect(currentTelescope, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SLOT(updateNumber(ISwitchVectorProperty*)), Qt::UniqueConnection);
    connect(currentTelescope, SIGNAL(messageUpdated(int)), this, SLOT(updateLog(int)), Qt::UniqueConnection);

    QTimer::singleShot(UPDATE_DELAY, this, SLOT(updateTelescopeCoords()));

    syncTelescopeInfo();
}

void Mount::syncTelescopeInfo()
{
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {

        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(xi18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = NULL;

        np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
        if (np && np->value > 0)
            primaryScopeApertureIN->setValue(np->value);

        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np && np->value > 0)
            primaryScopeFocalIN->setValue(np->value);

        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np && np->value > 0)
            guideScopeApertureIN->setValue(np->value);

        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np && np->value > 0)
            guideScopeFocalIN->setValue(np->value);

    }

    ISwitchVectorProperty *svp = currentTelescope->getBaseDevice()->getSwitch("TELESCOPE_SLEW_RATE");

    if (svp)
    {
        slewSpeedCombo->clear();
        slewSpeedCombo->setEnabled(true);

        for (int i=0; i < svp->nsp; i++)
            slewSpeedCombo->addItem(i18nc(libindi_strings_context, svp->sp[i].label));

        int index = IUFindOnSwitchIndex(svp);
        slewSpeedCombo->setCurrentIndex(index);
        connect(slewSpeedCombo, SIGNAL(activated(int)), currentTelescope, SLOT(setSlewRate(int)), Qt::UniqueConnection);
    }
    else
    {
        slewSpeedCombo->setEnabled(false);
        disconnect(slewSpeedCombo, SIGNAL(activated(int)), currentTelescope, SLOT(setSlewRate(int)));
    }

    if (currentTelescope->canPark())
    {
        parkB->setEnabled(true);
        unparkB->setEnabled(true);
        connect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()), Qt::UniqueConnection);
        connect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()), Qt::UniqueConnection);
    }
    else
    {
        parkB->setEnabled(false);
        unparkB->setEnabled(false);
        disconnect(parkB, SIGNAL(clicked()), currentTelescope, SLOT(Park()));
        disconnect(unparkB, SIGNAL(clicked()), currentTelescope, SLOT(UnPark()));
    }

}

void Mount::updateTelescopeCoords()
{
    double ra, dec;

    if (currentTelescope && currentTelescope->getEqCoords(&ra, &dec))
    {
        telescopeCoord.setRA(ra);
        telescopeCoord.setDec(dec);
        telescopeCoord.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

        raOUT->setText(telescopeCoord.ra().toHMSString());
        decOUT->setText(telescopeCoord.dec().toDMSString());
        azOUT->setText(telescopeCoord.az().toDMSString());
        altOUT->setText(telescopeCoord.alt().toDMSString());

        dms lst = KStarsData::Instance()->geo()->GSTtoLST( KStarsData::Instance()->clock()->utc().gst() );
        dms ha( lst.Degrees() - telescopeCoord.ra().Degrees() );
        QChar sgn('+');
        if ( ha.Hours() > 12.0 ) {
            ha.setH( 24.0 - ha.Hours() );
            sgn = '-';
        }
        haOUT->setText( QString("%1%2").arg(sgn).arg( ha.toHMSString() ) );
        lstOUT->setText(lst.toHMSString());

        if (currentTelescope->isConnected())
            QTimer::singleShot(UPDATE_DELAY, this, SLOT(updateTelescopeCoords()));
    }
}

void Mount::updateNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "TELESCOPE_INFO"))
    {
        if (nvp->s == IPS_ALERT)
        {
            QString newMessage;
            if (primaryScopeApertureIN->value() == 1 || primaryScopeFocalIN->value() == 1)
                newMessage = xi18n("Error syncing telescope info. Please fill telescope aperture and focal length.");
            else
                newMessage = xi18n("Error syncing telescope info. Check INDI control panel for more details.");
            if (newMessage != lastNotificationMessage)
            {
                appendLogText(newMessage);
                lastNotificationMessage = newMessage;
            }
        }
        else
        {
                syncTelescopeInfo();
                QString newMessage = xi18n("Telescope info updated successfully.");
                if (newMessage != lastNotificationMessage)
                {
                    appendLogText(newMessage);
                    lastNotificationMessage = newMessage;
                }

        }
    }
}

void Mount::updateSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "TELESCOPE_SLEW_RATE"))
    {
        int index = IUFindOnSwitchIndex(svp);
        slewSpeedCombo->setCurrentIndex(index);
    }
}

void Mount::appendLogText(const QString &text)
{
    logText.insert(0, xi18nc("log entry; %1 is the date, %2 is the text", "%1 %2", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"), text));

    emit newLog();
}

void Mount::updateLog(int messageID)
{
    INDI::BaseDevice *dv = currentTelescope->getBaseDevice();

    QString message = QString::fromStdString(dv->messageQueue(messageID));

    logText.insert(0, xi18nc("Message shown in Ekos Mount module", "%1", message));

    emit newLog();
}

void Mount::clearLog()
{
    logText.clear();
    emit newLog();
}

void Mount::move()
{
    QObject* obj = sender();

    if (obj == northB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH);
    }
    else if (obj == westB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST);
    }
    else if (obj == southB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH);
    }
    else if (obj == eastB)
    {
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST);
    }
    else if (obj == northwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST);
    }
    else if (obj == northeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_NORTH);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST);
    }
    else if (obj == southwestB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_WEST);
    }
    else if (obj == southeastB)
    {
        currentTelescope->MoveNS(ISD::Telescope::MOTION_SOUTH);
        currentTelescope->MoveWE(ISD::Telescope::MOTION_EAST);
    }
}

void Mount::stop()
{
    currentTelescope->Abort();
}

void Mount::save()
{
    INumberVectorProperty * nvp = currentTelescope->getBaseDevice()->getNumber("TELESCOPE_INFO");

    if (nvp)
    {

        primaryScopeGroup->setTitle(currentTelescope->getDeviceName());
        guideScopeGroup->setTitle(xi18n("%1 guide scope", currentTelescope->getDeviceName()));

        INumber *np = NULL;

        np = IUFindNumber(nvp, "TELESCOPE_APERTURE");
        if (np)
            np->value = primaryScopeApertureIN->value();
        np = IUFindNumber(nvp, "TELESCOPE_FOCAL_LENGTH");
        if (np)
            np->value = primaryScopeFocalIN->value();
        np = IUFindNumber(nvp, "GUIDER_APERTURE");
        if (np)
            np->value = guideScopeApertureIN->value() == 1 ? primaryScopeApertureIN->value() : guideScopeApertureIN->value();
        np = IUFindNumber(nvp, "GUIDER_FOCAL_LENGTH");
        if (np)
            np->value = guideScopeFocalIN->value() == 1 ? primaryScopeFocalIN->value() : guideScopeFocalIN->value();

        ClientManager *clientManager = currentTelescope->getDriverInfo()->getClientManager();

        clientManager->sendNewNumber(nvp);

        currentTelescope->setConfig(SAVE_CONFIG);

        appendLogText(xi18n("Saving telescope information..."));

    }
    else
        appendLogText(xi18n("Failed to save telescope information."));
}

}
