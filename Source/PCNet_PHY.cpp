/**
 * Copyright and Terms of Use/Distribution
 * 
 * The source code for "AMD-PCNET-II Network Driver for Darwin/x86 and Mac 
 * OS X/x86" and notes are written and released by Maxxuss.
 * 
 * http://maxxuss.hotbox.ru/pcnet.html
 * 
 * You can use this source code for any purpose as long as you provide this 
 * copyright note file with your source or binary distribution. Please also 
 * keep a reference to it in the header section of each provided source 
 * code file.
 * 
 * Please read the attached Copyright.txt for further information.
 * 
 */

#include "PCNet.h"

//---------------------------------------------------------------------------
// Function: phyAddMediumType
//
// Purpose:
//   Add an IONetworkMedium to a dictionary.
//   Also add the object to an array for quick lookup.

bool PCNET::phyAddMediumType( IOMediumType type,
        UInt32       bps,
        MediumIndex  index )
{   
    IONetworkMedium * medium;
    bool              ret = false;

    medium = IONetworkMedium::medium( type, bps, 0, index );
    if ( medium ) {
        ret = IONetworkMedium::addMedium( mediumDict, medium );
        if (ret) mediumTable[index] = medium;
        medium->release();
    }

    return ret;
}

//---------------------------------------------------------------------------
// Function: phyProbeMediaCapability
//
// Purpose:
//   Examine the PHY capabilities and advertise all supported media.

#define kMbScale 1000000 

void PCNET::phyProbeMediaCapability( void )
{
    phyAddMediumType( kIOMediumEthernet10BaseT |
            kIOMediumOptionHalfDuplex,
            10L * kMbScale, MEDIUM_INDEX_10_HD);

    phyAddMediumType( kIOMediumEthernet10BaseT |
            kIOMediumOptionFullDuplex,
            10L * kMbScale, MEDIUM_INDEX_10_FD );
}

//---------------------------------------------------------------------------

bool PCNET::phyReset( void )
{
    return true;
}

//---------------------------------------------------------------------------

bool PCNET::phySetMedium( MediumIndex mediumIndex )
{

    DEBUG_LOG("phySetMedium() ===>\n");
    if ( mediumIndex == currentMediumIndex )
    {
        return true;  // no change
    }

    bool success = lnc_set_medium(mediumIndex);

    DEBUG_LOG("phySetMedium() <===\n");

    if ( success ) currentMediumIndex = mediumIndex;

    return success;
}

//---------------------------------------------------------------------------

bool PCNET::phySetMedium( const IONetworkMedium * medium )
{
    return phySetMedium( medium->getIndex() );
}

//---------------------------------------------------------------------------
// Function: phyReportLinkStatus
//
// Purpose:
//   Called periodically to monitor for link changes. When a change
//   is detected, determine the negotiated link and report it to the
//   upper layers by calling IONetworkController::setLinkStatus().

void PCNET::phyReportLinkStatus( bool forceStatusReport )
{
    MediumIndex  activeMediumIndex;

    if (lnc_is_medium_available()) {
        lnc_watchdog();
        activeMediumIndex = lnc_get_medium();
        if (forceStatusReport || (activeMediumIndex != mediumIndexLastReported))
        {
            setLinkStatus( kIONetworkLinkValid | kIONetworkLinkActive,
                    phyGetMediumWithIndex( activeMediumIndex ) );
        }
        mediumIndexLastReported = activeMediumIndex;
    }
    else if (forceStatusReport || (mediumIndexLastReported != MEDIUM_INDEX_NONE)) {
        // Link down.
        setLinkStatus( kIONetworkLinkValid );
        mediumIndexLastReported = MEDIUM_INDEX_NONE;
    }

}

//---------------------------------------------------------------------------

const IONetworkMedium * PCNET::phyGetMediumWithIndex( MediumIndex index ) const
{
    if ( index < MEDIUM_INDEX_COUNT )
        return mediumTable[index];
    else
        return 0;
}

