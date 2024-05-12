/*
    The version of the AASun WWW pages package
    V 0.3   1st Release
    V 0.4   pchart.js : error for png button
            Add rounding for version display
    V 0.5   Add CT3 CT4 (Config, Diverter)
    V 0.6   Sequential message request
    V 0.7   Add diverter 2
    V 0.8   Add forcing: forcing.html forcing.js
            Better sendToServer() getServerData()
            31 days power and energy history
    V 0.9   Status word updated (WIFI)
    V 0.10  Bug in diverter On/Off button (Status word modified for WIFI)
            Add display selector in Config
            Add WIFI credential page
*/
let versionString = "0.10" ;

function getFooterString () { return "AASun - AdAstra-Soft" ; }

export { versionString, getFooterString };

// setConfig.cgi message ID
const midWrite              = 0 ;
const midDivOnOff           = 1 ;
const midRes1               = 2 ;     // Reserved
const midRes2               = 3 ;     // Reserved
const midVolt               = 4 ;
const midCurrent            = 5 ;
const midPower              = 6 ;
const midDiverter           = 7 ;
const midCoef               = 8 ;
const midCounter            = 9 ;
const midLan                = 10 ;
const midFp                 = 11 ;
const midEnergyNames        = 12 ;
const midDivertingRule      = 13 ;
const midForcingRules       = 14 ;
const midForcingManual      = 15 ;
const midResetEnergyTotal   = 16 ;
const midSetVar			    = 17 ;
const midSetAl			    = 18 ;
const midSetDisplay			= 19 ;

export
{
    midWrite,
    midDivOnOff,
    midRes1,
    midRes2,
    midVolt,
    midCurrent,
    midPower,
    midDiverter,
    midCoef,
    midCounter,
    midLan,
    midFp,
    midEnergyNames,
    midDivertingRule,
    midForcingRules,
    midForcingManual,
    midResetEnergyTotal,
    midSetVar,
    midSetAl,
    midSetDisplay,
} ;

//------------------------------------------------------------------
// To display the acknowledge dialog box

const dialog       = document.getElementById("ackDialog");

export function ackDialog (title, message)
{
    if (writeLed != null)
    {
        document.getElementById("ackTitle").innerHTML = title ;
        document.getElementById("ackMessage").innerHTML = message ;
        dialog.showModal () ;
    }
}

//------------------------------------------------------------------
//  Write red led management

let redLedBgOn      = "radial-gradient(circle at center, rgb(255, 80, 80) 0, rgb(255, 50, 50) 30%, rgb(180, 0, 0) 100%)" ;
let redLedBgOff     = "radial-gradient(circle at center, rgb(120, 0, 0) 0, rgb(20, 0, 0) 100%)" ;
let redLedShadowOn  = "rgba(255, 0, 0, 0.2) 0 -1px 7px 1px, inset #400 0 -1px 4px, #FFA0A0 0 2px 14px" ;
let redLedShadowOff = "none" ;
const writeLed = document.getElementById("writeLed") ;

export let bModified = false ;     // The configuration is not modified (no Send button click)

export function writeLedOn ()
{
    if (writeLed != null)
    {
        writeLed.style.background = redLedBgOn ;
        writeLed.style.boxShadow  = redLedShadowOn ;
    }
    bModified = true ;
}

export function writeLedOff ()
{
    if (writeLed != null)
    {
        writeLed.style.background = redLedBgOff ;
        writeLed.style.boxShadow  = redLedShadowOff ;
    }
    bModified = false ;
}
export function switchLed ()
{
    (bModified) ?  writeLedOff () : writeLedOn () ;
}

writeLedOff () ;

//------------------------------------------------------------------
